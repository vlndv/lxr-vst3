#!/usr/bin/env python3
"""Build and run P17-E kit-management tests at O0 and O2."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import pathlib
import subprocess
import time
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[2]
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


def execute(command: list[str], env: dict[str, str], timeout: int = 180) -> dict:
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
    parser.add_argument("--cxx", default="C:/msys64/ucrt64/bin/g++.exe")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    env = os.environ.copy()
    compiler = pathlib.Path(args.cxx)
    if args.compiler == "vs-llvm":
        from run_checkpoint_tests import msvc_environment, visual_studio
        vs = visual_studio()
        env = msvc_environment(vs, "14.51")
        compiler = vs / "VC/Tools/Llvm/x64/bin/clang-cl.exe"
    else:
        env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")

    sources = [
        ROOT / "tests/P17EKitManagementTest.cpp",
        ROOT / "dsp/KitContract.cpp", ROOT / "dsp/KitFileCodec.cpp",
        ROOT / "dsp/KitFileIO.cpp", ROOT / "dsp/KitFileIOWin32.cpp",
    ] + [ROOT / f"dsp/{name}.cpp" for name in PRODUCTION]
    inputs = sources + [
        ROOT / "dsp/KitManagement.h", ROOT / "interfaces/KitManagement.h",
        ROOT / "dsp/RuntimeKitLoad.h", ROOT / "dsp/KitFileIO.h",
        ROOT / "dsp/KitFileCodec.h", ROOT / "dsp/KitContract.h",
        ROOT / "dsp/RoutingStateAuthority.h",
        ROOT / "dsp/ParameterStateAuthority.h", pathlib.Path(__file__).resolve(),
    ]
    manifest = {str(path.relative_to(ROOT)).replace("\\", "/"): sha(path)
                for path in sorted(set(inputs))}
    (output / "source_manifest.json").write_text(json.dumps({
        "utc": datetime.now(timezone.utc).isoformat(),
        "firmware": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
        "files": manifest,
    }, indent=2) + "\n", encoding="utf-8")

    runs: list[dict] = []
    for optimization in ("O0", "O2"):
        build = output / optimization
        build.mkdir()
        filesystem = build / "filesystem"
        filesystem.mkdir()
        executable = build / "P17EKitManagementTest.exe"
        if args.compiler == "vs-llvm":
            command = [str(compiler), "/nologo", "/std:c++17", "/GR-",
                       "/EHs-c-", "/fp:strict", "/MD",
                       "/Od" if optimization == "O0" else "/O2", "/W4",
                       "/D_WIN32_WINNT=0x0601", "/I" + str(ROOT / "dsp"),
                       "/I" + str(ROOT / "tests"), *map(str, sources),
                       "/Fe" + str(executable)]
        else:
            command = [str(compiler), "-std=c++17", "-fno-exceptions",
                       "-fno-rtti", "-ffp-contract=off", "-pthread", "-Wall",
                       "-Wextra", f"-{optimization}", "-D_WIN32_WINNT=0x0601",
                       "-I" + str(ROOT / "dsp"), "-I" + str(ROOT / "tests"),
                       *map(str, sources), "-o", str(executable)]
        built = execute(command, env)
        run = {"optimization": optimization, "build": built}
        if built["status"] == "PASS":
            tested = execute([str(executable), str(filesystem)], env)
            tested["executable_sha256"] = sha(executable)
            if tested["status"] == "PASS":
                try:
                    tested["metrics"] = json.loads(tested["stdout"].strip())
                    if (tested["metrics"]["fail"] != 0 or
                            tested["metrics"]["publication_allocations"] != 0):
                        tested["status"] = "FAIL"
                except (KeyError, json.JSONDecodeError):
                    tested["status"] = "FAIL"
            run["execute"] = tested
        runs.append(run)
        (output / "commands.json").write_text(json.dumps(runs, indent=2) + "\n",
                                                encoding="utf-8")
        print(optimization, run.get("execute", built)["status"], flush=True)

    unchanged = all(sha(ROOT / path) == digest for path, digest in manifest.items())
    statuses = [run.get("execute", run["build"])["status"] for run in runs]
    classification = ("PASS" if unchanged and statuses == ["PASS", "PASS"] else
                      "BLOCKED" if all(s in ("PASS", "BLOCKED") for s in statuses)
                      else "FAIL")
    result = {"schema": "P17E-kit-management-v1",
              "classification": classification, "compiler": args.compiler,
              "source_unchanged_during_run": unchanged, "runs": runs}
    (output / "kit_management_results.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8")
    with (output / "regression_matrix.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(["suite", "compiler", "optimization", "status", "assertions"])
        for run in runs:
            tested = run.get("execute", run["build"])
            writer.writerow(["P17EKitManagementTest", args.compiler,
                             run["optimization"], tested["status"],
                             tested.get("metrics", {}).get("pass", 0)])
    return 0 if classification == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
