#!/usr/bin/env python3
"""Build and run the P16-F1 host transport suite at O0 and O2."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
from datetime import datetime, timezone


ROOT = pathlib.Path(__file__).resolve().parents[2]
PRODUCTION = [
    "Cymbal", "Decimator", "Distortion", "DrumVoice", "Engine",
    "Envelopes", "HiHat", "HostTransport", "Lfo", "LXRAudioAdapter",
    "Mixer", "Modulation", "Oscillator", "OscTables", "ParamMapEnv",
    "ParamMapMisc", "PatternBank", "ParameterArray", "PatternStateAuthority",
    "ResonantFilter", "SequencerClock", "Snare", "TransientGen",
    "TransientTables", "TriggerScheduler",
]


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def run(command: list[str]) -> dict:
    try:
        result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    except OSError as error:
        blocked = getattr(error, "winerror", None) == 4551
        return {
            "command": command,
            "exit_code": None,
            "stdout": "",
            "stderr": str(error),
            "status": "BLOCKED" if blocked else "ERROR",
        }
    return {
        "command": command,
        "exit_code": result.returncode,
        "stdout": result.stdout,
        "stderr": result.stderr,
        "status": "PASS" if result.returncode == 0 else "FAIL",
    }


def write_json(path: pathlib.Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path, default=ROOT / "out/p16f1")
    parser.add_argument("--cxx", default="C:/msys64/ucrt64/bin/g++.exe")
    args = parser.parse_args()
    cxx = pathlib.Path(args.cxx)
    if not cxx.exists():
        print("Missing documented MSYS2 UCRT64 G++.", file=sys.stderr)
        return 2

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    build = output / "host-transport-build"
    if build.exists():
        shutil.rmtree(build)
    build.mkdir()

    test = ROOT / "tests/P16F1HostTransportTest.cpp"
    sources = [ROOT / f"dsp/{name}.cpp" for name in PRODUCTION]
    runs: list[dict] = []
    metrics: dict[str, dict] = {}
    for optimization in ("O0", "O2"):
        executable = build / f"P16F1HostTransportTest-{optimization}.exe"
        command = [
            str(cxx), "-std=c++17", "-fno-exceptions", "-fno-rtti",
            "-pthread", "-Wall", "-Wextra", "-ffp-contract=off",
            f"-{optimization}", "-I" + str(ROOT / "dsp"),
            "-I" + str(ROOT / "tests"), str(test),
            *[str(source) for source in sources], "-o", str(executable),
        ]
        compile_result = run(command)
        execute_result = (run([str(executable)])
                          if compile_result["status"] == "PASS"
                          else {"status": "NOT_RUN"})
        if execute_result.get("status") == "PASS":
            try:
                metrics[optimization] = json.loads(
                    execute_result["stdout"].strip())
            except json.JSONDecodeError:
                execute_result["status"] = "FAIL"
                execute_result["stderr"] += "\nTest stdout is not valid JSON."
        runs.append({
            "optimization": optimization,
            "compile": compile_result,
            "execute": execute_result,
        })

    passed = len(metrics) == 2 and all(
        stage.get("status") == "PASS"
        for item in runs
        for stage in (item["compile"], item["execute"])
    ) and all(value.get("fail") == 0 and
              value.get("processing_allocations") == 0
              for value in metrics.values())
    inputs = [test, ROOT / "interfaces/HostTransport.h",
              ROOT / "tools/verification/run_host_transport_tests.py",
              *sources]
    result = {
        "phase": "P16-F1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "classification": "PASS" if passed else "FAIL",
        "variants": metrics,
        "runs": runs,
        "input_sha256": {
            str(path.relative_to(ROOT)).replace("\\", "/"): digest(path)
            for path in inputs
        },
    }
    write_json(output / "transport_test_results.json", result)
    print(json.dumps({
        "classification": result["classification"],
        "variants": metrics,
    }))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
