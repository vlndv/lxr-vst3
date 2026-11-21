#!/usr/bin/env python3
"""Build and run the independent P19-B GUI-foundation contract."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tests" / "P19BGUIFoundationTest.cpp"
OUT = ROOT / "out" / "p19b"


def run(command: list[str], cwd: pathlib.Path = ROOT) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)
    return result


def inventory_expectations() -> dict[int, tuple[int, int, int]]:
    path = ROOT / "out" / "p19a" / "parameter_control_inventory.csv"
    result: dict[int, tuple[int, int, int]] = {}
    with path.open(newline="", encoding="utf-8-sig") as handle:
        for row in csv.DictReader(handle):
            param = int(row["parameter_id"])
            if param in {1, 8, 37, 88, 114, 137, 228, 240, 241, 242, 245}:
                result[param] = (int(row["minimum"]), int(row["maximum"]), int(row["default"]))
    expected = {
        1: (0, 127, 1), 8: (0, 127, 63), 37: (0, 127, 64),
        88: (0, 127, 100), 114: (0, 127, 127), 137: (0, 127, 1),
        228: (0, 65535, 0), 240: (0, 1, 0), 241: (0, 1, 0),
        242: (1, 255, 120), 245: (0, 255, 0),
    }
    if result != expected:
        raise SystemExit(f"P19-A independent expectations changed: {result!r}")
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    inventory_expectations()

    compilers: list[tuple[str, str, list[str]]] = []
    gcc = shutil.which("g++")
    if gcc:
        compilers += [("gcc-o0", gcc, ["-O0"]), ("gcc-o2", gcc, ["-O2"])]

    clang = shutil.which("clang-cl")
    if not clang:
        candidate = pathlib.Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe")
        clang = str(candidate) if candidate.exists() else None
    if clang:
        compilers += [("vs-llvm-o0", clang, ["/Od"]), ("vs-llvm-o2", clang, ["/O2"])]
    if len(compilers) != 4:
        raise SystemExit(f"required GCC and VS LLVM compiler matrix unavailable: {compilers!r}")

    results: list[dict[str, object]] = []
    build_dir = OUT / "contract-build"
    build_dir.mkdir(exist_ok=True)
    for name, compiler, optimization in compilers:
        executable = build_dir / f"p19b-{name}.exe"
        if "clang-cl" in pathlib.Path(compiler).name.lower():
            command = [compiler, "/nologo", "/std:c++17", "/EHs-c-", "/GR-", *optimization,
                       str(SOURCE), "/Fe:" + str(executable)]
        else:
            command = [compiler, "-std=c++17", "-fno-exceptions", "-fno-rtti", "-pthread",
                       *optimization, str(SOURCE), "-o", str(executable)]
        if not args.verify_only or not executable.exists():
            run(command)
        execution = run([str(executable)])
        payload = json.loads(execution.stdout.strip().splitlines()[-1])
        payload.update({"configuration": name, "compiler": compiler})
        results.append(payload)

    report = {
        "schema": "lxr-p19b-gui-foundation-tests-v1",
        "status": "PASS",
        "source": str(SOURCE.relative_to(ROOT)).replace("\\", "/"),
        "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest().upper(),
        "independent_p19a_expectations": True,
        "configurations": results,
    }
    (OUT / "gui_foundation_test_results.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
