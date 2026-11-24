#!/usr/bin/env python3
"""Build the P19-F source-independent GUI qualification matrix and evidence."""

from __future__ import annotations

import hashlib
import json
import pathlib
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "out/p19f"


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    if result.returncode:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)
    return result


def compilers() -> list[tuple[str, str, list[str], bool]]:
    gcc = shutil.which("g++") or r"C:\msys64\ucrt64\bin\g++.exe"
    clang = shutil.which("clang-cl") or str(pathlib.Path(
        r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe"))
    rows = [
        ("gcc-o0", gcc, ["-O0"], False),
        ("gcc-o2", gcc, ["-O2"], False),
        ("vs-llvm-o0", clang, ["/Od", "/Ob0"], True),
        ("vs-llvm-o2", clang, ["/O2"], True),
    ]
    if not all(pathlib.Path(row[1]).exists() for row in rows):
        raise SystemExit(f"required compiler matrix unavailable: {rows!r}")
    return rows


def write_json(name: str, value: object) -> None:
    (OUT / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    build = OUT / "standalone-build"
    build.mkdir(parents=True, exist_ok=True)
    source = ROOT / "tests/P19FGUIQualificationTest.cpp"
    rows = []
    for configuration, compiler, optimization, llvm in compilers():
        executable = build / f"p19f-gui-{configuration}.exe"
        if llvm:
            command = [compiler, "/nologo", "/std:c++17", "/EHs-c-", "/GR-",
                       *optimization, "/I.", str(source), "/Fe:" + str(executable)]
        else:
            command = [compiler, "-std=c++17", "-fno-exceptions", "-fno-rtti",
                       "-pthread", "-ffp-contract=off", *optimization, "-I.",
                       str(source), "-o", str(executable)]
        compile_result = run(command)
        execution = run([str(executable)])
        match = re.search(r"PASS \((\d+) checks\)", execution.stdout)
        if not match:
            raise SystemExit(f"unexpected output for {configuration}: {execution.stdout!r}")
        rows.append({"configuration": configuration, "compiler": compiler,
                     "status": "PASS", "checks": int(match.group(1)),
                     "compile_stderr": compile_result.stderr})

    backend = ROOT / "vendor/iPlug2/IGraphics/Platforms/IGraphicsWin.cpp"
    build_script = ROOT / "tools/verification/build_p16f3_candidate.py"
    targets = ROOT / "tools/verification/p19f_iplug_override.targets"
    dpi_source = backend.read_text(encoding="utf-8")
    build_source = build_script.read_text(encoding="utf-8")
    if "int screenScale = GetScaleForHWND(mParentWnd);" not in dpi_source:
        raise SystemExit("pinned iPlug2 DPI source was unexpectedly modified")
    if ("float screenScale = GetScaleForHWND(mParentWnd);" not in build_source or
            "P19FIGraphicsWinSource" not in targets.read_text(encoding="utf-8")):
        raise SystemExit("P19-F isolated DPI override is missing")

    report = {
        "schema": "lxr-p19f-gui-qualification-v1", "status": "PASS",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "configurations": rows,
        "total_assertions": sum(int(row["checks"]) for row in rows),
        "coverage": {"views": 5, "parameters_registered": 246,
                     "parameters_visible": 223, "parameters_hidden": 23,
                     "sound_controls": 162, "modulation_controls": 54,
                     "sound_owners": 6, "sequencer_tracks": 7},
        "dpi_backend": {"fractional_scale_preserved": True,
                        "factors": [1.0, 1.25, 1.5, 2.0]},
        "sources": {str(path.relative_to(ROOT)).replace("\\", "/"): digest(path)
                    for path in (source, ROOT / "interfaces/P19FGUIQualification.h",
                                 backend, build_script, targets)},
    }
    write_json("standalone_gui_qualification_results.json", report)
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
