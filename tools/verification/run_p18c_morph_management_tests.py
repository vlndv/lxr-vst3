#!/usr/bin/env python3
"""Run P18-B parity plus P18-C morph-management contracts at O0/O2."""

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
HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import run_p18b_morph_runtime_tests as p18b  # noqa: E402


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

    baseline = output / "p18b-baseline"
    baseline_command = [sys.executable, str(HERE / "run_p18b_morph_runtime_tests.py"),
                        "--output", str(baseline), "--compiler", args.compiler,
                        "--cc", args.cc, "--cxx", args.cxx]
    baseline_run = execute(baseline_command, os.environ.copy(), 1800)
    baseline_result_path = baseline / "morph_runtime_results.json"
    baseline_classification = "FAIL"
    if baseline_result_path.is_file():
        baseline_classification = json.loads(
            baseline_result_path.read_text(encoding="utf-8"))["classification"]

    test = ROOT / "tests/P18CMorphManagementTest.cpp"
    production = [ROOT / "dsp/KitContract.cpp", ROOT / "dsp/KitFileCodec.cpp"]
    inputs = [test, ROOT / "dsp/MorphManagement.h", ROOT / "interfaces/MorphManagement.h",
              ROOT / "dsp/Morphing.h", ROOT / "dsp/KitFileCodec.h",
              ROOT / "tests/reference/morph/original_morph_adapter.c",
              ROOT / "tests/reference/morph/original_morph_adapter.h",
              pathlib.Path(__file__).resolve(), *production]
    (output / "source_manifest.json").write_text(json.dumps({
        "utc": datetime.now(timezone.utc).isoformat(),
        "firmware": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
        "files": {str(path.relative_to(ROOT)).replace("\\", "/"): sha(path)
                  for path in sorted(set(inputs))},
        "p18b_baseline": baseline_classification,
    }, indent=2) + "\n", encoding="utf-8")

    env = os.environ.copy()
    clang = None
    if args.compiler == "vs-llvm":
        from run_checkpoint_tests import msvc_environment, visual_studio
        vs = visual_studio()
        env = msvc_environment(vs, "14.51")
        clang = vs / "VC/Tools/Llvm/x64/bin/clang-cl.exe"
    else:
        env["PATH"] = str(pathlib.Path(args.cxx).parent) + os.pathsep + env.get("PATH", "")

    rows = []
    for optimization in ("O0", "O2"):
        build = output / optimization
        build.mkdir()
        suffix = ".obj" if clang else ".o"
        morph_object = baseline / optimization / ("original_morph" + suffix)
        target_object = baseline / optimization / ("original_targets" + suffix)
        executable = build / "P18CMorphManagementTest.exe"
        if clang:
            command = [str(clang), "/nologo", "/std:c++17", "/GR-", "/EHs-c-",
                       "/fp:strict", "/MD", "/Od" if optimization == "O0" else "/O2",
                       "/W4", "/I" + str(ROOT / "dsp"), "/I" + str(ROOT / "tests"),
                       str(test), *map(str, production), str(morph_object),
                       str(target_object), "/Fe" + str(executable)]
        else:
            command = [args.cxx, "-std=c++17", "-fno-exceptions", "-fno-rtti",
                       "-ffp-contract=off", "-pthread", "-Wall", "-Wextra",
                       f"-{optimization}", "-I" + str(ROOT / "dsp"),
                       "-I" + str(ROOT / "tests"), str(test), *map(str, production),
                       str(morph_object), str(target_object), "-o", str(executable)]
        built = execute(command, env)
        row = {"optimization": optimization, "build": built}
        if built["status"] == "PASS":
            run = execute([str(executable)], env)
            run["executable_sha256"] = sha(executable)
            if run["status"] == "PASS":
                try:
                    metrics = json.loads(run["stdout"].strip())
                    run["metrics"] = metrics
                    if metrics["fail"] != 0 or metrics["allocations"] != 0:
                        run["status"] = "FAIL"
                except (KeyError, json.JSONDecodeError):
                    run["status"] = "FAIL"
            row["execute"] = run
        rows.append(row)
        print(optimization, row.get("execute", built)["status"], flush=True)

    unchanged = all(sha(ROOT / path) == digest for path, digest in
                    json.loads((output / "source_manifest.json").read_text())["files"].items())
    statuses = [row.get("execute", row["build"])["status"] for row in rows]
    classification = ("PASS" if baseline_classification == "PASS" and
                      statuses == ["PASS", "PASS"] and unchanged else "FAIL")
    result = {"schema": "P18C-morph-management-v1",
              "classification": classification, "compiler": args.compiler,
              "p18b_baseline": baseline_classification,
              "source_unchanged_during_run": unchanged, "runs": rows,
              "baseline_command": baseline_run}
    (output / "morph_management_results.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return 0 if classification == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
