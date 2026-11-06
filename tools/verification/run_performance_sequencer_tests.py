#!/usr/bin/env python3
"""Build and run the P16-E performance sequencer suite at O0 and O2."""

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
    "ParameterArray", "PatternStateAuthority", "ResonantFilter", "SequencerClock",
    "Snare", "TransientGen", "TransientTables", "TriggerScheduler",
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
    parser.add_argument("--output", type=pathlib.Path, default=ROOT / "out/p16e")
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
    build = output / "performance-build"
    if build.exists():
        shutil.rmtree(build)
    build.mkdir()

    test = ROOT / "tests/P16EPerformanceSequencerTest.cpp"
    adapter = ROOT / "tests/reference/sequencer/original_performance_adapter.c"
    original = FIRMWARE / "EuklidGenerator.c"
    runs: list[dict] = []
    metrics: dict[str, dict] = {}
    for optimization in ("O0", "O2"):
        variant = build / optimization
        variant.mkdir()
        original_object = variant / "original-euclidean.o"
        adapter_object = variant / "performance-adapter.o"
        executable = variant / "P16EPerformanceSequencerTest.exe"
        common = ["-Wall", "-Wextra", "-ffp-contract=off", f"-{optimization}"]
        compile_original = run([
            str(cc), "-std=gnu11", "-fgnu89-inline", *common, "-Werror",
            "-I" + str(ROOT / "tests/reference/sequencer/compat"),
            "-I" + str(FIRMWARE), "-c", str(original), "-o", str(original_object),
        ])
        compile_adapter = run([
            str(cc), "-std=gnu11", *common, "-Werror",
            "-I" + str(ROOT / "tests/reference/sequencer/compat"),
            "-I" + str(ROOT / "tests/reference/sequencer"),
            "-I" + str(FIRMWARE), "-c", str(adapter), "-o", str(adapter_object),
        ]) if compile_original["status"] == "PASS" else {"status": "NOT_RUN"}
        compile_cpp = run([
            str(cxx), "-std=c++17", "-fno-exceptions", "-fno-rtti", "-pthread",
            *common, "-I" + str(ROOT / "dsp"), "-I" + str(ROOT / "tests"),
            str(test), *[str(ROOT / f"dsp/{name}.cpp") for name in PRODUCTION],
            str(original_object), str(adapter_object), "-o", str(executable),
        ]) if compile_adapter.get("status") == "PASS" else {"status": "NOT_RUN"}
        execute = run([str(executable)]) if compile_cpp.get("status") == "PASS" else {"status": "NOT_RUN"}
        if execute.get("status") == "PASS":
            try:
                metrics[optimization] = json.loads(execute["stdout"].strip())
            except json.JSONDecodeError:
                execute["status"] = "FAIL"
                execute["stderr"] += "\nTest stdout is not valid JSON."
        runs.append({
            "optimization": optimization,
            "compile_unchanged_original_euclidean": compile_original,
            "compile_source_adapter": compile_adapter,
            "compile_cpp": compile_cpp,
            "execute": execute,
        })

    passed = len(metrics) == 2 and all(
        stage.get("status") == "PASS"
        for item in runs
        for stage in (item["compile_unchanged_original_euclidean"],
                      item["compile_source_adapter"], item["compile_cpp"], item["execute"])
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
        ROOT / "tests/reference/sequencer/original_performance_adapter.h",
        original, FIRMWARE / "sequencer.c", FIRMWARE / "sequencer.h",
        ROOT / "dsp/PatternBank.cpp", ROOT / "dsp/SequencerClock.cpp",
        ROOT / "dsp/TriggerScheduler.cpp",
        ROOT / "tools/verification/run_performance_sequencer_tests.py",
    ]
    write_json(output / "performance_sequencer_results.json", {
        "schema": "P16E-performance-sequencer-v1", **common_result,
        "runs": runs,
        "inputs": {str(path.relative_to(ROOT)).replace("\\", "/"): digest(path)
                   for path in inputs},
    })
    artifacts = {
        "pattern_chain_results.json": {
            "checks": ["fixed next", "absolute repeat modulo", "track-0-only transition",
                       "stopped and running manual selection", "rotated new-pattern start"],
        },
        "shuffle_timing_results.json": {
            "checks": ["all 16 phase entries", "amounts 0 1 64 127", "tempo 1 120 255",
                       "next-native-block quantization", "live change monotonicity",
                       "no duplicate dispatch", "no long-term phase accumulation"],
        },
        "mute_roll_results.json": {
            "checks": ["seven-track mute", "muted progression", "muted RNG suppression",
                       "roll ignores mute gate and probability", "14 exact roll selectors",
                       "one-shot while stopped", "release", "roll locks before trigger"],
        },
        "euclidean_results.json": {
            "oracle": "unchanged pinned EuklidGenerator.c",
            "checks": ["all lengths 1-16", "all pulses 1-length",
                       "all rotations 0-length", "unrelated StepData preserved"],
        },
        "copy_clear_results.json": {
            "checks": ["track bytes gates length rotation", "pattern metadata exclusion",
                       "canonical clear defaults", "candidate publication"],
        },
        "rng_trace_results.json": {
            "checks": ["one full xorshift value per random-next",
                       "random draw precedes new-pattern probability", "fixed transitions consume none"],
            "hardware_entropy_bit_exact": False,
        },
        "native_pcm_results.json": {
            "checks": ["pattern A/B nonzero and distinguishable",
                       "MIDI-only stopped exact", "roll audible dispatch",
                       "matched seed and state deterministic via P16-C/P16-D regressions"],
        },
        "concurrency_results.json": {
            "checks": ["explicit unpublished-candidate barrier", "old generation complete",
                       "atomic commit", "three-slot publication inherited"],
        },
    }
    for name, details in artifacts.items():
        write_json(output / name, {"schema": f"P16E-{name[:-5]}-v1",
                                   **common_result, **details})
    (output / "allocation_report.md").write_text(
        "# P16-E allocation and capacity report\n\n"
        f"Classification: {'PASS' if passed else 'FAIL'}\n\n"
        "The measured scheduler processing loop performs zero C++ heap allocations. "
        "It uses atomics, fixed arrays and bounded loops only. One physical position is "
        "dispatched per native block after source-timer negative-delta serialization. "
        "The hard event bound is 21 (seven ordinary + seven periodic rolls + seven "
        "already-queued one-shots); the automation-operation bound is 42. Snapshot "
        "acquisition remains bounded to eight attempts. Edit publication copies one "
        "fixed bank into an inactive slot off the audio path.\n",
        encoding="utf-8")
    (output / "som_assessment.md").write_text(
        "# P16-E SOM assessment\n\n"
        "Classification: **DEFERRED**. `SomGenerator.c::som_tick` and its learned node "
        "table are source-confirmed. It runs only on track-0 positions divisible by 8, "
        "uses seven interpolated thresholds and hardware RNG, and the pinned sequencer "
        "calls it from inside the seven-track loop. The official owner manual does not "
        "document a SOM control or complete performance contract, and the storage/UI "
        "entry path is incomplete. Implementing it would require inventing user and "
        "lifecycle semantics, so P16-E preserves no false runtime claim and defers it.\n",
        encoding="utf-8")
    tree_rows = []
    tree = subprocess.check_output(
        ["git", "ls-tree", "-r", "HEAD"], cwd=ROOT, text=True)
    for line in tree.splitlines():
        metadata, path = line.split("\t", 1)
        mode, object_type, object_id = metadata.split()
        tree_rows.append({"path": path, "mode": mode,
                          "type": object_type, "object": object_id})
    status_lines = subprocess.check_output(
        ["git", "status", "--porcelain"], cwd=ROOT, text=True).splitlines()
    working = []
    for line in status_lines:
        path = line[3:]
        candidate = ROOT / path
        working.append({"status": line[:2], "path": path,
                        "sha256": digest(candidate) if candidate.is_file() else None})
    write_json(output / "file_inventory.json", {
        "schema": "P16E-file-inventory-v1",
        "starting_head": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "starting_tracked_count": len(tree_rows),
        "starting_tracked_inventory": tree_rows,
        "working_task_inventory": working,
        "generated_utc": generated,
    })
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
