#!/usr/bin/env python3
"""Build and run the independent P19-C Sound-editor contract matrix."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import pathlib
import re
import shutil
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tests" / "P19CSoundEditorTest.cpp"
INVENTORY = ROOT / "design" / "p19a" / "parameter_control_inventory.csv"
OUT = ROOT / "out" / "p19c"


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    if result.returncode:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)
    return result


def validate_inventory() -> dict[str, object]:
    sound: list[int] = []
    modulation: list[int] = []
    with INVENTORY.open(newline="", encoding="utf-8-sig") as handle:
        rows = list(csv.DictReader(handle))
    if len(rows) != 246:
        raise SystemExit(f"expected 246 registered host rows, found {len(rows)}")
    for row in rows:
        parameter = int(row["parameter_id"])
        if row["view"] == "Sound":
            sound.append(parameter)
        elif row["view"] == "Modulation":
            modulation.append(parameter)
    required_reclassification = set(range(128, 143))
    if len(sound) != 162 or not required_reclassification.issubset(sound):
        raise SystemExit(
            f"P19-C source-grounded Sound inventory mismatch: {len(sound)} rows"
        )
    if len(modulation) != 54 or required_reclassification.intersection(modulation):
        raise SystemExit(
            f"P19-D modulation boundary mismatch: {len(modulation)} rows"
        )
    return {
        "registered_host_parameters": len(rows),
        "sound_parameters": len(sound),
        "deferred_modulation_parameters": len(modulation),
        "sound_parameter_ids": sound,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    inventory = validate_inventory()

    compilers: list[tuple[str, str, list[str]]] = []
    gcc = shutil.which("g++")
    if gcc:
        compilers += [("gcc-o0", gcc, ["-O0"]), ("gcc-o2", gcc, ["-O2"])]

    clang = shutil.which("clang-cl")
    if not clang:
        candidate = pathlib.Path(
            r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe"
        )
        clang = str(candidate) if candidate.exists() else None
    if clang:
        compilers += [
            ("vs-llvm-o0", clang, ["/Od"]),
            ("vs-llvm-o2", clang, ["/O2"]),
        ]
    if len(compilers) != 4:
        raise SystemExit(
            f"required GCC and VS LLVM compiler matrix unavailable: {compilers!r}"
        )

    build_dir = OUT / "sound-editor-contract-build"
    build_dir.mkdir(exist_ok=True)
    results: list[dict[str, object]] = []
    for name, compiler, optimization in compilers:
        executable = build_dir / f"p19c-{name}.exe"
        if "clang-cl" in pathlib.Path(compiler).name.lower():
            command = [
                compiler, "/nologo", "/std:c++17", "/EHs-c-", "/GR-",
                *optimization, "/I.", str(SOURCE), "/Fe:" + str(executable),
            ]
        else:
            command = [
                compiler, "-std=c++17", "-fno-exceptions", "-fno-rtti",
                *optimization, "-I.", str(SOURCE), "-o", str(executable),
            ]
        if not args.verify_only or not executable.exists():
            run(command)
        execution = run([str(executable)])
        match = re.search(r"PASS \((\d+) checks\)", execution.stdout)
        if not match:
            raise SystemExit(f"unexpected {name} output: {execution.stdout!r}")
        results.append({
            "configuration": name,
            "compiler": compiler,
            "checks": int(match.group(1)),
            "status": "PASS",
        })

    report = {
        "schema": "lxr-p19c-sound-editor-tests-v1",
        "status": "PASS",
        "source": str(SOURCE.relative_to(ROOT)).replace("\\", "/"),
        "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest().upper(),
        "independent_source_expectations": True,
        "inventory": inventory,
        "configurations": results,
        "total_checks": sum(int(item["checks"]) for item in results),
    }
    path = OUT / "sound_editor_test_results.json"
    path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
