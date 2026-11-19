#!/usr/bin/env python3
"""Build and run P18-B original-C/runtime morph tests at O0 and O2."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import subprocess
import sys
import time
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[2]
FW = ROOT / "ref/LXR-0.37"
AVR = FW / "front/LxrAvr"
P17_REF = ROOT / "tests/reference/presets"
P18_REF = ROOT / "tests/reference/morph"
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import run_preset_kit_contract_tests as p17  # noqa: E402

PRODUCTION = [
    "Cymbal", "Decimator", "Distortion", "DrumVoice", "Engine",
    "Envelopes", "HiHat", "HostTransport", "Lfo", "LXRAudioAdapter",
    "MidiParser", "Mixer", "Modulation", "Oscillator", "OscTables",
    "ParameterArray", "ParamMapEnv", "ParamMapMisc", "PatternBank",
    "PatternStateAuthority", "ResonantFilter", "SequencerClock", "Snare",
    "TransientGen", "TransientTables", "TriggerScheduler",
]


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def execute(command: list[str], env: dict[str, str], timeout: int = 900) -> dict:
    started = time.monotonic()
    try:
        run = subprocess.run(command, cwd=ROOT, env=env, text=True,
                             capture_output=True, timeout=timeout)
        result = {"status": "PASS" if run.returncode == 0 else "FAIL",
                  "exit_code": run.returncode, "stdout": run.stdout,
                  "stderr": run.stderr}
    except OSError as error:
        result = {"status": "BLOCKED" if getattr(error, "winerror", None) == 4551 else "ERROR",
                  "error": str(error)}
    except subprocess.TimeoutExpired as error:
        result = {"status": "BLOCKED", "error": str(error)}
    result["command"] = list(map(str, command))
    result["elapsed_seconds"] = time.monotonic() - started
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--compiler", choices=["gcc", "vs-llvm"], default="gcc")
    parser.add_argument("--cc", default="C:/msys64/ucrt64/bin/gcc.exe")
    parser.add_argument("--cxx", default="C:/msys64/ucrt64/bin/g++.exe")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)

    values = p17.enums(AVR / "Parameters.h")
    assert values["END_OF_SOUND_PARAMETERS"] == 228
    first: dict[int, str] = {}
    for name, value in values.items():
        if value < 228:
            first.setdefault(value, name)
    assert set(first) == set(range(228))
    enum_list = output / "original_enum_list.inc"
    enum_list.write_text(",\n".join(first[i] for i in range(228)) + "\n")
    fragments, receipts = [], []
    for name in ("menu_init", "numtostrpu"):
        text, receipt = p17.function_slice(AVR / "Menu/menu.c", name)
        fragments.append(text)
        receipts.append(receipt)
    fragment = output / "original_menu_functions.inc"
    fragment.write_text("\n\n".join(fragments) + "\n")

    test = ROOT / "tests/P18BMorphRuntimeTest.cpp"
    production = [ROOT / "dsp/KitContract.cpp"] + [
        ROOT / f"dsp/{name}.cpp" for name in PRODUCTION
    ]
    inputs = [test, P18_REF / "original_morph_adapter.c",
              P18_REF / "original_morph_adapter.h",
              P17_REF / "original_preset_adapter.h",
              ROOT / "dsp/Morphing.h", ROOT / "interfaces/Morphing.h",
              ROOT / "dsp/KitContract.h", ROOT / "dsp/RoutingStateAuthority.h",
              AVR / "Preset/presetManager.c", AVR / "Menu/menu.c",
              AVR / "Menu/Cc2Text.c", AVR / "Parameters.h",
              pathlib.Path(__file__).resolve(), *production,
              *list((P17_REF / "compat").rglob("*.h"))]
    manifest = {str(path.relative_to(ROOT)).replace("\\", "/"): sha(path)
                for path in sorted(set(inputs))}
    (output / "source_manifest.json").write_text(json.dumps({
        "utc": datetime.now(timezone.utc).isoformat(),
        "firmware": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
        "files": manifest, "unchanged_original_slices": receipts,
        "oracle": "unchanged presetManager.c interpolation via the P18-A bounded adapter",
    }, indent=2) + "\n")

    env = os.environ.copy()
    cc = pathlib.Path(args.cc)
    cxx = pathlib.Path(args.cxx)
    clang = None
    if args.compiler == "vs-llvm":
        from run_checkpoint_tests import msvc_environment, visual_studio
        vs = visual_studio()
        env = msvc_environment(vs, "14.51")
        clang = vs / "VC/Tools/Llvm/x64/bin/clang-cl.exe"
    else:
        env["PATH"] = str(cxx.parent) + os.pathsep + env.get("PATH", "")

    defines = {
        "P17_ORIGINAL_MENU_HEADER": AVR / "Menu/menu.h",
        "P17_ORIGINAL_FRONT_HEADER": AVR / "frontPanelParser.h",
        "P17_ORIGINAL_PRESET_SOURCE": AVR / "Preset/presetManager.c",
        "P17_ORIGINAL_MENU_FRAGMENT": fragment,
        "P17_ORIGINAL_ENUM_LIST": enum_list,
    }
    runs: list[dict] = []
    for optimization in ("O0", "O2"):
        build = output / optimization
        build.mkdir()
        obj_suffix = ".obj" if clang else ".o"
        morph_obj = build / ("original_morph" + obj_suffix)
        targets_obj = build / ("original_targets" + obj_suffix)
        executable = build / "P18BMorphRuntimeTest.exe"
        if clang:
            common = [str(clang), "/nologo", "/Od" if optimization == "O0" else "/O2",
                      "/MD", "/I" + str(P17_REF / "compat")]
            define_flags = [f'/D{name}="{path.as_posix()}"' for name, path in defines.items()]
            adapter = execute([*common, "/TC", "/clang:-std=gnu11", "/clang:-fgnu89-inline",
                               *define_flags, "/c", str(P18_REF / "original_morph_adapter.c"),
                               "/Fo" + str(morph_obj)], env)
            targets = execute([*common, "/TC", "/clang:-std=gnu11", "/c",
                               str(AVR / "Menu/Cc2Text.c"), "/Fo" + str(targets_obj)], env)
            build_result = {"status": "BLOCKED"}
            if adapter["status"] == targets["status"] == "PASS":
                build_result = execute([str(clang), "/nologo", "/std:c++17", "/GR-", "/EHs-c-",
                    "/fp:strict", "/MD", "/Od" if optimization == "O0" else "/O2", "/W4",
                    "/I" + str(ROOT / "dsp"), "/I" + str(ROOT / "tests"), str(test),
                    *map(str, production), str(morph_obj), str(targets_obj),
                    "/Fe" + str(executable)], env)
        else:
            common_c = [f"-{optimization}", "-Wall", "-Wextra", "-I" + str(P17_REF / "compat")]
            define_flags = [f'-D{name}="{path.as_posix()}"' for name, path in defines.items()]
            adapter = execute([str(cc), "-std=gnu11", *common_c, *define_flags,
                               "-c", str(P18_REF / "original_morph_adapter.c"),
                               "-o", str(morph_obj)], env)
            targets = execute([str(cc), "-std=gnu11", *common_c, "-c",
                               str(AVR / "Menu/Cc2Text.c"), "-o", str(targets_obj)], env)
            build_result = {"status": "BLOCKED"}
            if adapter["status"] == targets["status"] == "PASS":
                build_result = execute([str(cxx), "-std=c++17", "-fno-exceptions", "-fno-rtti",
                    "-ffp-contract=off", "-pthread", "-Wall", "-Wextra", f"-{optimization}",
                    "-I" + str(ROOT / "dsp"), "-I" + str(ROOT / "tests"), str(test),
                    *map(str, production), str(morph_obj), str(targets_obj),
                    "-o", str(executable)], env)
        row = {"optimization": optimization, "adapter_build": adapter,
               "target_build": targets, "runtime_build": build_result}
        if build_result["status"] == "PASS":
            tested = execute([str(executable)], env)
            tested["executable_sha256"] = sha(executable)
            if tested["status"] == "PASS":
                try:
                    metrics = json.loads(tested["stdout"].strip())
                    tested["metrics"] = metrics
                    if (metrics["fail"] != 0 or
                        metrics["exhaustive_inputs"] != 16777216 or
                        metrics["projection_allocations"] != 0):
                        tested["status"] = "FAIL"
                except (KeyError, json.JSONDecodeError):
                    tested["status"] = "FAIL"
            row["execute"] = tested
        runs.append(row)
        (output / "commands.json").write_text(json.dumps(runs, indent=2) + "\n")
        print(optimization, row.get("execute", build_result)["status"], flush=True)

    unchanged = all(sha(ROOT / path) == digest for path, digest in manifest.items())
    statuses = [row.get("execute", row["runtime_build"])["status"] for row in runs]
    classification = ("PASS" if unchanged and statuses == ["PASS", "PASS"] else
                      "BLOCKED" if all(status in ("PASS", "BLOCKED") for status in statuses) else
                      "FAIL")
    result = {"schema": "P18B-morph-runtime-v1", "classification": classification,
              "compiler": args.compiler, "source_unchanged_during_run": unchanged,
              "runs": runs}
    (output / "morph_runtime_results.json").write_text(json.dumps(result, indent=2) + "\n")
    return 0 if classification == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
