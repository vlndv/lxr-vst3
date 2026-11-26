#!/usr/bin/env python3
"""Build and run the focused P20-D exact-DLL diagnostic qualification."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def parse_fields(line: str) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in re.findall(r"([a-z_]+)=([^\s]+)", line):
        try:
            result[key] = int(value)
        except ValueError:
            result[key] = value
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dll", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    dll = args.dll.resolve()
    manifest_path = args.manifest.resolve()
    output = args.output.resolve()
    if output.exists():
        raise SystemExit(f"refusing to overwrite existing output: {output}")
    output.mkdir(parents=True)

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not manifest.get("p20d_diagnostic_capture"):
        raise SystemExit("candidate manifest does not identify a P20-D diagnostic build")
    actual_dll = sha256(dll)
    if actual_dll != str(manifest["sha256"]).upper():
        raise SystemExit("diagnostic DLL does not match its manifest")

    baseline = json.loads((ROOT / "out/p20b/exact_candidate_results.json").read_text(
        encoding="utf-8"))
    command = list(baseline["focused_probe"]["compile_commands"][0])
    source = ROOT / "tests/P20DExactDllRetentionTest.cpp"
    executable = output / "P20DExactDllRetentionTest.exe"
    object_file = output / "P20DExactDllRetentionTest.obj"
    for index, argument in enumerate(command):
        if argument.endswith("tests\\P20BHostEventCaptureTest.cpp"):
            command[index] = str(source)
        elif argument.startswith("/Fo"):
            command[index] = "/Fo" + str(object_file)
        elif argument.startswith("/Fe"):
            command[index] = "/Fe" + str(executable)

    vs_shell = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1")
    quoted = " ".join("'" + item.replace("'", "''") + "'" for item in command)
    compile_ps = (
        f"& '{vs_shell}' -Arch amd64 -HostArch amd64 | Out-Null; "
        f"& {quoted}; exit $LASTEXITCODE"
    )
    compiled = subprocess.run(
        ["powershell", "-NoProfile", "-Command", compile_ps], cwd=ROOT,
        text=True, capture_output=True, errors="replace")
    (output / "compile.log").write_text(
        compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        print(compiled.stdout)
        print(compiled.stderr, file=sys.stderr)
        return compiled.returncode

    run = subprocess.run([str(executable), str(dll)], cwd=ROOT, text=True,
                         capture_output=True, errors="replace")
    log = run.stdout + run.stderr
    (output / "run.log").write_text(log, encoding="utf-8")
    partitions = [parse_fields(line) for line in log.splitlines()
                  if line.startswith("P20D_PARTITION ")]
    overflow = next((parse_fields(line) for line in log.splitlines()
                     if line.startswith("P20D_OVERFLOW ")), {})
    resources = next((parse_fields(line) for line in log.splitlines()
                      if line.startswith("P20D_RESOURCES ")), {})
    transport = next((parse_fields(line) for line in log.splitlines()
                      if line.startswith("P20D_TRANSPORT ")), {})
    copies = [parse_fields(line) for line in log.splitlines()
              if line.startswith("P20D_COPY scenario=")]
    rejection = next((parse_fields(line) for line in log.splitlines()
                      if line.startswith("P20D_COPY_REJECTION ")), {})
    summary_match = re.search(r"SUMMARY pass=(\d+) fail=(\d+)", log)
    expected_partitions = [1, 16, 31, 32, 33, 64, 127, 128, 256, 512, 1024]
    status = "PASS" if (
        run.returncode == 0 and summary_match and int(summary_match.group(2)) == 0 and
        [row.get("frames") for row in partitions] == expected_partitions and
        all(row.get("dropped_batches") == 0 and row.get("triggers") == 448 and
            row.get("locks") == 896 for row in partitions) and
        overflow.get("generated") == 30 and overflow.get("retained") == 2 and
        overflow.get("dropped") == 28 and len(copies) == 6 and
        rejection.get("stale") == 1 and rejection.get("busy") == 1
    ) else "FAIL"
    results = {
        "status": status,
        "diagnostic_dll": str(dll),
        "diagnostic_dll_sha256": actual_dll,
        "diagnostic_manifest": str(manifest_path),
        "source": str(source),
        "source_sha256": sha256(source),
        "executable": str(executable),
        "executable_sha256": sha256(executable),
        "compile_command": command,
        "compile_exit_code": compiled.returncode,
        "run_exit_code": run.returncode,
        "summary": ({"pass": int(summary_match.group(1)),
                     "fail": int(summary_match.group(2))} if summary_match else None),
        "partitions": partitions,
        "overflow": overflow,
        "resources": resources,
        "transport": transport,
        "copy_scenarios": copies,
        "copy_rejection": rejection,
    }
    (output / "results.json").write_text(
        json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": status, "summary": results["summary"],
                      "partitions": len(partitions), "copy_scenarios": len(copies)},
                     indent=2))
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
