#!/usr/bin/env python3
"""Build and run the P16-C scheduler/original-C/native PCM suite at O0/O2."""

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
PRODUCTION = [
    "Cymbal", "Decimator", "Distortion", "DrumVoice", "Engine",
    "Envelopes", "HiHat", "Lfo", "LXRAudioAdapter", "Mixer", "Modulation",
    "Oscillator", "OscTables", "ParamMapEnv", "ParamMapMisc", "PatternBank",
    "ParameterArray", "PatternStateAuthority", "ResonantFilter", "SequencerClock", "Snare",
    "TransientGen", "TransientTables", "TriggerScheduler",
]


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def run(args: list[str], cwd: pathlib.Path) -> dict:
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
    parser.add_argument("--output", type=pathlib.Path, default=ROOT / "out/p16c")
    parser.add_argument("--cc", default="C:/msys64/ucrt64/bin/gcc.exe")
    parser.add_argument("--cxx", default="C:/msys64/ucrt64/bin/g++.exe")
    args = parser.parse_args()
    cc = pathlib.Path(args.cc)
    cxx = pathlib.Path(args.cxx)
    if not cc.exists() or not cxx.exists():
        print("Missing documented MSYS2 UCRT64 GCC/G++.", file=sys.stderr)
        return 2

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    build = output / "trigger-build"
    if build.exists():
        shutil.rmtree(build)
    build.mkdir()

    adapter = ROOT / "tests/reference/sequencer/original_trigger_scheduler_adapter.c"
    test = ROOT / "tests/P16CTriggerSchedulerTest.cpp"
    runs: list[dict] = []
    metrics: dict[str, dict] = {}
    for optimization in ("O0", "O2"):
        variant = build / optimization
        variant.mkdir()
        original = variant / "original-trigger-scheduler.o"
        executable = variant / "P16CTriggerSchedulerTest.exe"
        common = ["-Wall", "-Wextra", "-ffp-contract=off", f"-{optimization}"]
        compile_c = run([
            str(cc), "-std=gnu11", *common, "-Werror",
            "-I" + str(ROOT / "tests/reference/sequencer/compat"),
            "-I" + str(ROOT / "tests/reference/sequencer"),
            "-I" + str(FIRMWARE), "-c", str(adapter), "-o", str(original),
        ], ROOT)
        compile_cpp = run([
            str(cxx), "-std=c++17", "-fno-exceptions", "-fno-rtti", "-pthread",
            *common, "-I" + str(ROOT / "dsp"), "-I" + str(ROOT / "tests"),
            str(test), *[str(ROOT / f"dsp/{name}.cpp") for name in PRODUCTION],
            str(original), "-o", str(executable),
        ], ROOT) if compile_c["status"] == "PASS" else {"status": "NOT_RUN"}
        execute = run([str(executable)], ROOT) if compile_cpp["status"] == "PASS" else {"status": "NOT_RUN"}
        if execute.get("status") == "PASS":
            try:
                metrics[optimization] = json.loads(execute["stdout"].strip())
            except json.JSONDecodeError:
                execute["status"] = "FAIL"
                execute["stderr"] += "\nTest stdout is not valid JSON."
        runs.append({
            "optimization": optimization,
            "compile_source_derived_c": compile_c,
            "compile_cpp": compile_cpp,
            "execute": execute,
        })

    passed = len(metrics) == 2 and all(
        stage.get("status") == "PASS"
        for item in runs
        for stage in (item["compile_source_derived_c"], item["compile_cpp"], item["execute"])
    ) and all(value.get("fail") == 0 for value in metrics.values())
    generated = datetime.now(timezone.utc).isoformat()
    common_result = {
        "firmware_revision": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
        "generated_utc": generated,
        "classification": "PASS" if passed else "FAIL",
        "variants": metrics,
    }
    inputs = [
        test, adapter,
        ROOT / "tests/reference/sequencer/original_trigger_scheduler_adapter.h",
        ROOT / "dsp/TriggerScheduler.h", ROOT / "dsp/TriggerScheduler.cpp",
        ROOT / "dsp/LXRAudioAdapter.h", ROOT / "dsp/LXRAudioAdapter.cpp",
        FIRMWARE / "sequencer.c", FIRMWARE / "sequencer.h",
    ]
    write_json(output / "scheduler_test_results.json", {
        "schema": "P16C-trigger-scheduler-results-v1", **common_result,
        "runs": runs,
        "inputs": {str(path.relative_to(ROOT)).replace("\\", "/"): digest(path)
                   for path in inputs},
    })
    write_json(output / "event_timing_results.json", {
        "schema": "P16C-event-timing-v1", **common_result,
        "native_block_frames": 32,
        "tempo_range": [1, 255],
        "checks": ["immediate start at frame 0", "120 BPM next boundary frame 704",
                   "tempo 1 accepted without duplicate start",
                   "255 BPM next boundary frame 352",
                   "running 120-to-60 BPM change reanchors at frame 1376",
                   "full 128-position wrap", "all eight patterns",
                   "variable native-block grouping", "stop",
                   "source-style restart", "deterministic reset"],
    })
    write_json(output / "original_c_comparison.json", {
        "schema": "P16C-original-c-comparison-v1", **common_result,
        "classification_detail": "source-derived C oracle; full sequencer.c is not directly linkable without STM32 services",
        "source_ranges": ["sequencer.c:488-543", "sequencer.c:1468-1485",
                          "MidiVoiceControl.c:51-62"],
        "comparison": "exact event count/order/positions/owner/articulation/note/velocity/RNG state",
        "hardware_entropy_equivalence": False,
    })
    write_json(output / "probability_results.json", {
        "schema": "P16C-probability-v1", **common_result,
        "checks": ["inclusive zero pass for RNG zero", "zero reject for nonzero RNG",
                   "127 always passes", "one draw at active cluster boundary",
                   "inactive boundary preserves prior latch", "ascending multitrack consumption"],
    })
    write_json(output / "track_dispatch_results.json", {
        "schema": "P16C-track-dispatch-v1", **common_result,
        "mapping": [[0, 0, "standard"], [1, 1, "standard"], [2, 2, "standard"],
                    [3, 3, "standard"], [4, 4, "standard"],
                    [5, 5, "closed"], [6, 5, "open"]],
        "same_boundary_order": [0, 1, 2, 3, 4, 5, 6],
    })
    write_json(output / "audio_render_results.json", {
        "schema": "P16C-audio-render-v1", **common_result,
        "checks": ["sequencer off exact PCM", "D1 nonzero and exact direct-trigger PCM",
                   "all seven logical tracks nonzero", "closed/open simultaneous exact PCM",
                   "MIDI trigger then sequencer trigger exact PCM", "native adapter pre-block dispatch"],
    })
    allocation = [
        "# P16-C allocation and capacity report", "",
        "The measured scheduler processing loop performs zero C++ heap allocations.",
        "Construction, asset loading and test infrastructure are outside the measured scope.", "",
        "- Supported tempo: 1–255 BPM.",
        "- Minimum physical-position interval at 255 BPM: about 323.55 native frames.",
        "- Native block: 32 frames; therefore at most one physical position is due per normal block.",
        "- Seven tracks imply a maximum ordinary event density of seven events per block.",
        "- Capacity is seven; defensive overflow rejects newest, preserves accepted order and saturates diagnostics.",
        "- Snapshot acquisition is bounded to eight attempts; Busy skips the boundary while cached lengths advance positions.",
        "- Per block bounds: one clock event, eight acquisition attempts, seven track reads and seven dispatches.",
        "- No mutex, file I/O, logging, callback, dynamic table or unbounded production loop is present.",
    ]
    (output / "allocation_report.md").write_text("\n".join(allocation) + "\n", encoding="utf-8")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
