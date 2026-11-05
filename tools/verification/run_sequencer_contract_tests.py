#!/usr/bin/env python3
"""Compile and run the P16-A source-contract tests at O0 and O2."""

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
FIRMWARE = ROOT / "ref/LXR-0.37/mainboard/LxrStm32/src/Sequencer"
ADAPTER = ROOT / "tests/reference/sequencer/original_sequencer_adapter.c"
TEST = ROOT / "tests/P16ASequencerContractTest.cpp"


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def run(command: list[str], cwd: pathlib.Path) -> dict:
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    return {
        "command": [str(item) for item in command],
        "exit_code": result.returncode,
        "stdout": result.stdout,
        "stderr": result.stderr,
        "status": "PASS" if result.returncode == 0 else "FAIL",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path, default=ROOT / "out/p16a")
    parser.add_argument("--cc", default=str(ROOT / "C:/msys64/ucrt64/bin/gcc.exe"))
    parser.add_argument("--cxx", default=str(ROOT / "C:/msys64/ucrt64/bin/g++.exe"))
    args = parser.parse_args()

    # pathlib treats a drive-qualified string joined to ROOT oddly on Windows.
    cc = pathlib.Path("C:/msys64/ucrt64/bin/gcc.exe") if "C:/" in args.cc.replace("\\", "/") else pathlib.Path(args.cc)
    cxx = pathlib.Path("C:/msys64/ucrt64/bin/g++.exe") if "C:/" in args.cxx.replace("\\", "/") else pathlib.Path(args.cxx)
    if not cc.exists() or not cxx.exists():
        print("Missing GCC/G++; use the documented MSYS2 UCRT64 toolchain.", file=sys.stderr)
        return 2

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    build = output / "contract-build"
    if build.exists():
        shutil.rmtree(build)
    build.mkdir()

    records = []
    for opt in ("O0", "O2"):
        opt_dir = build / opt
        opt_dir.mkdir()
        adapter_obj = opt_dir / "adapter.o"
        exe = opt_dir / "P16ASequencerContractTest.exe"
        common = ["-Wall", "-Wextra", "-Werror", "-ffp-contract=off", f"-{opt}"]
        compile_c = run([
            str(cc), "-std=gnu11", *common,
            "-I" + str(ROOT / "tests/reference/sequencer/compat"),
            "-I" + str(ROOT / "tests/reference/sequencer"),
            "-I" + str(FIRMWARE), "-c", str(ADAPTER), "-o", str(adapter_obj),
        ], opt_dir)
        compile_cpp = run([
            str(cxx), "-std=c++17", "-fno-exceptions", "-fno-rtti", *common,
            "-I" + str(ROOT / "tests"), str(TEST), str(adapter_obj), "-o", str(exe),
        ], opt_dir) if compile_c["status"] == "PASS" else {"status": "NOT_RUN"}
        execute = run([str(exe)], opt_dir) if compile_cpp["status"] == "PASS" else {"status": "NOT_RUN"}
        records.append({"optimization": opt, "compile_c": compile_c,
                        "compile_cpp": compile_cpp, "execute": execute})

    inputs = [
        ADAPTER,
        ROOT / "tests/reference/sequencer/original_sequencer_adapter.h",
        TEST,
        FIRMWARE / "sequencer.h",
        FIRMWARE / "sequencer.c",
        FIRMWARE / "clockSync.c",
        ROOT / "ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio/automationNode.c",
    ]
    passed = all(stage.get("status") == "PASS" for record in records
                 for stage in (record["compile_c"], record["compile_cpp"], record["execute"]))
    report = {
        "schema": "P16A-sequencer-contract-results-v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "classification": "PASS" if passed else "FAIL",
        "scope": "Source-header ABI plus standalone design-contract simulation; not a full original-C runtime differential.",
        "firmware_revision": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
        "inputs": {str(path.relative_to(ROOT)).replace("\\", "/"): sha(path) for path in inputs},
        "runs": records,
    }
    (output / "contract_test_results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
