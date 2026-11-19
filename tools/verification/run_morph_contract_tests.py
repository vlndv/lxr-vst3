#!/usr/bin/env python3
"""P18-A original-firmware morph contract runner; no plugin build or Git writes."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import pathlib
import subprocess
import sys
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[2]
FW = ROOT / "ref/LXR-0.37"
AVR = FW / "front/LxrAvr"
P17_REF = ROOT / "tests/reference/presets"
P18_REF = ROOT / "tests/reference/morph"
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import run_preset_kit_contract_tests as p17  # noqa: E402


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def emit_parameter_matrix(out: pathlib.Path) -> None:
    source = ROOT / "out/p17b/parameter_mapping_matrix.csv"
    route_ids = set(range(149, 155)) | set(range(161, 173))
    rows = []
    with source.open(newline="") as handle:
        for row in csv.DictReader(handle):
            parameter_id = int(row["id"])
            dtype = row["source_dtype"]
            if row["kind"] == "Reserved":
                value_class = "RESERVED_OPAQUE"
            elif row["kind"] == "Selector":
                value_class = "ENUMERATED_ROUTE_SELECTOR"
            elif row["kind"] == "VoiceHint":
                value_class = "ENUMERATED_ROUTE_VOICE_HINT"
            elif ("DTYPE_MENU" in dtype or dtype in {"DTYPE_NOTE_NAME", "DTYPE_ON_OFF",
                                                     "DTYPE_MIX_FM"}):
                value_class = "ENUMERATED_OR_DISCRETE_BYTE"
            else:
                value_class = "CONTINUOUS_BYTE_SCALAR"
            route_family = ("VELOCITY_DESTINATION" if 149 <= parameter_id <= 154 else
                            "LFO_VOICE_HINT" if 161 <= parameter_id <= 166 else
                            "LFO_DESTINATION" if 167 <= parameter_id <= 172 else "NONE")
            rows.append({
                "id": parameter_id,
                "identity": row["identity"],
                "owner": row["owner"],
                "source_dtype": dtype,
                "value_class": value_class,
                "original_numeric_sweep": "YES_ALL_228",
                "effective_morph_policy": ("SOURCE_ROUTE_ANCHOR_EXCLUDED" if parameter_id in route_ids
                                           else "NUMERIC_INTERPOLATION_SENT"),
                "route_family": route_family,
                "trigger_only": "NO",
                "global_parameter": "YES" if row["owner"] == "Global" else "NO",
                "shared_hihat_parameter": "YES" if row["owner"] == "HiHat" else "NO",
                "reserved": "YES" if row["kind"] == "Reserved" else "NO",
                "runtime_unimplemented": "YES" if row["native_implementation"] == "Opaque" else "NO",
                "unknown": "NO",
                "native_policy": row["native_policy"],
                "evidence": ("presetManager.c:57-91,843-887; Parameters.h; "
                             "out/p17b/parameter_mapping_matrix.csv"),
            })
    assert len(rows) == 228
    destination = out / "parameter_participation_matrix.csv"
    with destination.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--cc", default="C:/msys64/ucrt64/bin/gcc.exe")
    parser.add_argument("--cxx", default="C:/msys64/ucrt64/bin/g++.exe")
    parser.add_argument("--compiler", choices=["gcc", "vs-llvm"], default="gcc")
    parser.add_argument("--run-id", help="Evidence directory suffix; defaults to compiler")
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    emit_parameter_matrix(out)
    run_dir = out / ("reference-runs-" + (args.run_id or args.compiler))
    run_dir.mkdir(exist_ok=False)

    values = p17.enums(AVR / "Parameters.h")
    assert values["END_OF_SOUND_PARAMETERS"] == 228
    first: dict[int, str] = {}
    for name, value in values.items():
        if value < 228:
            first.setdefault(value, name)
    assert set(first) == set(range(228))
    enum_list = run_dir / "original_enum_list.inc"
    enum_list.write_text(",\n".join(first[i] for i in range(228)) + "\n")

    fragments, receipts = [], []
    for name in ["menu_init", "numtostrpu"]:
        text, receipt = p17.function_slice(AVR / "Menu/menu.c", name)
        fragments.append(text)
        receipts.append(receipt)
    fragment = run_dir / "original_menu_functions.inc"
    fragment.write_text("\n\n".join(fragments) + "\n")

    sources = [
        P18_REF / "original_morph_adapter.c",
        P18_REF / "original_morph_adapter.h",
        P17_REF / "original_preset_adapter.c",
        P17_REF / "original_preset_adapter.h",
        ROOT / "tests/P18AMorphContractTest.cpp",
        AVR / "Preset/presetManager.c",
        AVR / "Preset/PresetManager.h",
        AVR / "Menu/menu.c",
        AVR / "Menu/menu.h",
        AVR / "Menu/Cc2Text.c",
        AVR / "Menu/CcNr2Text.h",
        AVR / "Parameters.h",
        pathlib.Path(__file__).resolve(),
    ] + list((P17_REF / "compat").rglob("*.h"))
    manifest = [{"path": p.relative_to(ROOT).as_posix(), "sha256": sha(p)} for p in sources]
    (run_dir / "source_manifest.json").write_text(json.dumps({
        "firmware": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
        "files": manifest,
        "unchanged_original_slices": receipts,
        "scope": "Unchanged presetManager.c through bounded P17/P18 host shims; no AVR timing or production plugin claim",
    }, indent=2))

    environment = os.environ.copy()
    environment["PATH"] = str(pathlib.Path(args.cc).parent) + ";" + environment["PATH"]
    clang = None
    if args.compiler == "vs-llvm":
        from run_checkpoint_tests import msvc_environment, visual_studio
        vs = visual_studio()
        environment = msvc_environment(vs, "14.51")
        clang = vs / "VC/Tools/Llvm/x64/bin/clang-cl.exe"

    commands: list[dict] = []
    runs: list[dict] = []

    def command(label: str, argv: list) -> dict:
        row = {"label": label, "command": list(map(str, argv))}
        try:
            process = subprocess.run(row["command"], cwd=ROOT, env=environment,
                                     capture_output=True, text=True, timeout=180)
            row.update(exit_code=process.returncode,
                       status="PASS" if process.returncode == 0 else "FAIL",
                       stdout=process.stdout, stderr=process.stderr)
        except OSError as error:
            row.update(status="BLOCKED" if getattr(error, "winerror", None) == 4551 else "ERROR",
                       error=str(error))
        except subprocess.TimeoutExpired as error:
            row.update(status="BLOCKED", error=str(error))
        commands.append(row)
        (run_dir / "commands.json").write_text(json.dumps(commands, indent=2))
        print(label, row["status"], flush=True)
        return row

    defines = {
        "P17_ORIGINAL_MENU_HEADER": AVR / "Menu/menu.h",
        "P17_ORIGINAL_FRONT_HEADER": AVR / "frontPanelParser.h",
        "P17_ORIGINAL_PRESET_SOURCE": AVR / "Preset/presetManager.c",
        "P17_ORIGINAL_MENU_FRAGMENT": fragment,
        "P17_ORIGINAL_ENUM_LIST": enum_list,
    }
    for optimization in ["O0", "O2"]:
        build = run_dir / optimization
        build.mkdir()
        stages = []
        obj_suffix = ".obj" if clang else ".o"
        adapter_obj = build / ("morph" + obj_suffix)
        targets_obj = build / ("targets" + obj_suffix)
        flags = [f'-D{name}="{path.as_posix()}"' for name, path in defines.items()]
        if clang:
            common = ["/nologo", "/Od" if optimization == "O0" else "/O2", "/MD",
                      "/I" + str(P17_REF / "compat")]
            flags = [f'/D{name}="{path.as_posix()}"' for name, path in defines.items()]
            stages.append(command(optimization + "-compile-original-morph", [
                clang, *common, "/TC", "/clang:-std=gnu11", "/clang:-fgnu89-inline",
                *flags, "/c", P18_REF / "original_morph_adapter.c", "/Fo" + str(adapter_obj)]))
            stages.append(command(optimization + "-compile-original-targets", [
                clang, *common, "/TC", "/clang:-std=gnu11", "/c", AVR / "Menu/Cc2Text.c",
                "/Fo" + str(targets_obj)]))
            test_obj = build / "test.obj"
            if all(row["status"] == "PASS" for row in stages):
                stages.append(command(optimization + "-compile-test", [
                    clang, *common, "/std:c++17", "/GR-", "/EHs-c-", "/c",
                    ROOT / "tests/P18AMorphContractTest.cpp", "/Fo" + str(test_obj)]))
            executable = build / "P18AMorphContractTest.exe"
            if all(row["status"] == "PASS" for row in stages):
                stages.append(command(optimization + "-link-test", [
                    clang, test_obj, adapter_obj, targets_obj, "/Fe" + str(executable)]))
        else:
            common = ["-" + optimization, "-Wall", "-Wextra", "-I" + str(P17_REF / "compat")]
            stages.append(command(optimization + "-compile-original-morph", [
                args.cc, "-std=gnu11", *common, *flags, "-c",
                P18_REF / "original_morph_adapter.c", "-o", adapter_obj]))
            stages.append(command(optimization + "-compile-original-targets", [
                args.cc, "-std=gnu11", *common, "-c", AVR / "Menu/Cc2Text.c", "-o", targets_obj]))
            executable = build / "P18AMorphContractTest.exe"
            if all(row["status"] == "PASS" for row in stages):
                stages.append(command(optimization + "-compile-link-test", [
                    args.cxx, "-std=c++17", "-fno-exceptions", "-fno-rtti", *common,
                    ROOT / "tests/P18AMorphContractTest.cpp", adapter_obj, targets_obj,
                    "-o", executable]))
        if all(row["status"] == "PASS" for row in stages):
            run = command(optimization + "-execute", [executable])
            run["executable_sha256"] = sha(executable)
            if run["status"] == "PASS":
                try:
                    run["metrics"] = json.loads(run["stdout"].strip())
                except json.JSONDecodeError:
                    run["status"] = "FAIL"
            runs.append({"optimization": optimization, **run})
        else:
            runs.append({"optimization": optimization, "status": "BLOCKED",
                         "reason": "Build did not complete; see commands.json"})

    assert all(sha(ROOT / row["path"]) == row["sha256"] for row in manifest)
    status = ("PASS" if len(runs) == 2 and all(r["status"] == "PASS" for r in runs)
              else "FAIL" if any(r["status"] == "FAIL" for r in runs) else "BLOCKED")
    result = {
        "utc": datetime.now(timezone.utc).isoformat(),
        "compiler": args.compiler,
        "firmware": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
        "status": status,
        "runs": runs,
        "source_manifest": str((run_dir / "source_manifest.json").relative_to(out)).replace("\\", "/"),
        "limits": [
            "Host shims capture messages and FatFs bytes; AVR UART timing is source-audited, not cycle-emulated",
            "The unchanged original interpolation and morph/save functions are the reference oracle",
            "No production plugin or VST3 binary was built or executed",
        ],
    }
    (run_dir / "result.json").write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
