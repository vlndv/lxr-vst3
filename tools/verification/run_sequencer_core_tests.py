#!/usr/bin/env python3
"""Build and run the isolated P16-B sequencer core tests at O0 and O2."""

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


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def command(args: list[str], cwd: pathlib.Path) -> dict:
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    return {
        "command": args,
        "exit_code": result.returncode,
        "stdout": result.stdout,
        "stderr": result.stderr,
        "status": "PASS" if result.returncode == 0 else "FAIL",
    }


def write_json(path: pathlib.Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path, default=ROOT / "out/p16b")
    parser.add_argument("--cc", default="C:/msys64/ucrt64/bin/gcc.exe")
    parser.add_argument("--cxx", default="C:/msys64/ucrt64/bin/g++.exe")
    args = parser.parse_args()

    cc = pathlib.Path(args.cc)
    cxx = pathlib.Path(args.cxx)
    if not cc.exists() or not cxx.exists():
        print("Missing GCC/G++; use the documented MSYS2 UCRT64 toolchain.", file=sys.stderr)
        return 2

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    build = output / "core-build"
    if build.exists():
        shutil.rmtree(build)
    build.mkdir()

    adapter = ROOT / "tests/reference/sequencer/original_sequencer_core_adapter.c"
    test = ROOT / "tests/P16BSequencerCoreTest.cpp"
    production = [
        ROOT / "dsp/PatternBank.cpp",
        ROOT / "dsp/PatternStateAuthority.cpp",
        ROOT / "dsp/SequencerClock.cpp",
    ]
    runs: list[dict] = []
    metrics: dict[str, dict] = {}

    for optimization in ("O0", "O2"):
        variant = build / optimization
        variant.mkdir()
        original_object = variant / "original-core.o"
        executable = variant / "P16BSequencerCoreTest.exe"
        common = ["-Wall", "-Wextra", "-Werror", "-ffp-contract=off", f"-{optimization}"]
        compile_c = command([
            str(cc), "-std=gnu11", *common,
            "-I" + str(ROOT / "tests/reference/sequencer/compat"),
            "-I" + str(ROOT / "tests/reference/sequencer"),
            "-I" + str(FIRMWARE), "-c", str(adapter), "-o", str(original_object),
        ], variant)
        compile_cpp = command([
            str(cxx), "-std=c++17", "-fno-exceptions", "-fno-rtti", "-pthread", *common,
            "-I" + str(ROOT / "dsp"), "-I" + str(ROOT / "tests"),
            str(test), *[str(path) for path in production], str(original_object),
            "-o", str(executable),
        ], variant) if compile_c["status"] == "PASS" else {"status": "NOT_RUN"}
        execute = command([str(executable)], variant) if compile_cpp["status"] == "PASS" else {"status": "NOT_RUN"}
        if execute.get("status") == "PASS":
            try:
                metrics[optimization] = json.loads(execute["stdout"].strip())
            except json.JSONDecodeError:
                execute["status"] = "FAIL"
                execute["stderr"] += "\nTest stdout was not valid JSON."
        runs.append({
            "optimization": optimization,
            "compile_original_c": compile_c,
            "compile_cpp": compile_cpp,
            "execute": execute,
        })

    sources = [
        ROOT / "interfaces/SequencerCore.h",
        ROOT / "dsp/SequencerTypes.h",
        ROOT / "dsp/SequencerRng.h",
        ROOT / "dsp/PatternBank.h",
        ROOT / "dsp/PatternBank.cpp",
        ROOT / "dsp/SequencerClock.h",
        ROOT / "dsp/SequencerClock.cpp",
        ROOT / "dsp/PatternStateAuthority.h",
        ROOT / "dsp/PatternStateAuthority.cpp",
        ROOT / "tests/P16BSequencerCoreTest.cpp",
        adapter,
        ROOT / "tests/reference/sequencer/original_sequencer_core_adapter.h",
        FIRMWARE / "sequencer.h",
        FIRMWARE / "sequencer.c",
    ]
    passed = len(metrics) == 2 and all(
        stage.get("status") == "PASS"
        for run in runs
        for stage in (run["compile_original_c"], run["compile_cpp"], run["execute"])
    )
    generated = datetime.now(timezone.utc).isoformat()
    common = {
        "generated_utc": generated,
        "firmware_revision": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
        "classification": "PASS" if passed else "FAIL",
        "variants": metrics,
    }

    write_json(output / "core_test_results.json", {
        "schema": "P16B-sequencer-core-results-v1",
        **common,
        "scope": "Isolated sequencer core; no Engine, VST3, persistence or host integration.",
        "inputs": {str(path.relative_to(ROOT)).replace("\\", "/"): digest(path) for path in sources},
        "runs": runs,
    })
    write_json(output / "data_model_verification.json", {
        "schema": "P16B-data-model-v1", **common,
        "counts": {"patterns": 8, "tracks": 7, "positions_per_track": 128,
                   "main_steps": 16, "substeps_per_main": 8, "stored_steps": 7168},
        "step_bytes": 7,
        "field_offsets": [0, 1, 2, 3, 4, 5, 6],
        "comparison": "Compiled pinned sequencer.h ABI and source-extracted C defaults match production types."
    })
    write_json(output / "default_bank_comparison.json", {
        "schema": "P16B-default-bank-v1", **common,
        "defaults": {"velocity": 100, "boundary_substep_active": True,
                     "other_substeps_active": False, "probability": 127,
                     "note": 63, "automation_sentinel": 255,
                     "main_gates": 0, "effective_track_length": 16,
                     "rotation": 0, "repeat": 0, "next_pattern": "self",
                     "shuffle": 0, "active_pattern": 0, "tempo": 120},
        "factory_preset_dependency": "none; firmware initialization defaults only"
    })
    write_json(output / "rng_results.json", {
        "schema": "P16B-rng-v1", **common,
        "algorithm": "xorshift32 fixed-width deterministic adaptation",
        "hardware_bit_exact": False,
        "checks": ["same seed replay", "independent instances", "reset replay",
                   "inclusive probability zero", "probability 127",
                   "per-track cluster latch", "inactive-boundary reuse"]
    })
    write_json(output / "clock_results.json", {
        "schema": "P16B-clock-v1", **common,
        "rate_rational": {"numerator": 5632353, "denominator": 128},
        "position_step": "rate * 60 / (BPM * 32)",
        "tempo_range": [1, 255],
        "native_block_frames": 32,
        "checks": ["120 BPM", "tempo bounds", "fractional remainder",
                   "one million positions", "chunk invariance", "start/stop/resume",
                   "tempo change at boundary", "monotonic input", "block quantization"]
    })
    write_json(output / "authority_results.json", {
        "schema": "P16B-authority-v1", **common,
        "design": "three immutable fixed-size slots with reader pins and atomic writer claim",
        "slot_count": 3,
        "reader_attempt_bound": 8,
        "checks": ["coherent publication", "invalid edit rollback", "snapshot lifetime",
                   "pinned-slot backpressure", "concurrent readers/writer",
                   "no torn bank", "multi-instance isolation", "zero operation allocations"]
    })

    allocation_lines = [
        "# P16-B allocation and timing report", "",
        "Measurements are standalone wall-clock diagnostics, not hard real-time guarantees.", "",
        "| Variant | Pattern bank | Authority | Slot payload | Allocations | Clock ns/event | Acquire ns | Edit/publish ns | Busy observations | Reader snapshots |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for variant in ("O0", "O2"):
        value = metrics.get(variant, {})
        allocation_lines.append(
            f"| {variant} | {value.get('pattern_bank_bytes', 'n/a')} | "
            f"{value.get('authority_bytes', 'n/a')} | {value.get('slot_payload_bytes', 'n/a')} | "
            f"{value.get('allocation_delta', 'n/a')} | {value.get('clock_ns_per_event', 'n/a')} | "
            f"{value.get('acquire_ns', 'n/a')} | {value.get('edit_ns', 'n/a')} | "
            f"{value.get('publication_busy', 'n/a')} | {value.get('reader_snapshots', 'n/a')} |"
        )
    allocation_lines += [
        "", "The audio-facing reader performs a bounded pin/check operation and returns Busy after eight attempts.",
        "Writers scan two inactive slots once. Full-bank copying and validation occur only on the editing writer, never per native block.",
        "The core contains no mutex, heap allocation, file I/O, logging or unbounded production loop."
    ]
    (output / "allocation_report.md").write_text("\n".join(allocation_lines) + "\n", encoding="utf-8")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
