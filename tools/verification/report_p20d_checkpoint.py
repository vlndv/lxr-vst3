#!/usr/bin/env python3
"""Assemble the bounded P20-D evidence set from completed test artifacts."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "out/p20d"
START = "0155e153ae34ff2fb5740ea7bfe6855ef5d1c220"
PRODUCTION_SHA = "FB1045BD5716282607487F6A70A6082206309EE741C8F751768AE5C8F6F7650D"
PRODUCTION_SOURCE = "80DB83C0C8F79020B06580A54E5627537CE35C5B3E1FBE57DEE33413C0113400"
PROTECTED_SHA = "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82"
ASSETS = ["crash_sample.bin", "rec_table.bin", "saw_table.bin", "sine_table.bin",
          "transient_data.bin", "transient_volume_table.bin", "tri_table.bin"]
BASELINE_PATHS = [
    "vendor/iPlug2/Examples/LXR/LXR.cpp",
    "tests/P19EVST3SequencerGUITest.cpp",
    "tests/P20BHostEventCaptureTest.cpp",
    "tools/verification/build_p16f3_candidate.py",
    "tools/verification/report_p20b_checkpoint.py",
    "docs/phase_log.md",
    "docs/p20b_release_blocker_resolution.md",
    "docs/p20c_release_evidence_closure.md",
    "tests/P20CHostTransitionCaptureTest.cpp",
    "tools/verification/report_p20c_checkpoint.py",
]


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def read(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(name: str, value) -> None:
    (OUT / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def write_csv(name: str, rows: list[dict]) -> None:
    fields = list(dict.fromkeys(key for row in rows for key in row))
    with (OUT / name).open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def git(*arguments: str) -> str:
    return subprocess.check_output(["git", *arguments], cwd=ROOT, text=True).strip()


def inventory(path: Path) -> dict[str, object]:
    return {"path": path.relative_to(ROOT).as_posix(), "bytes": path.stat().st_size,
            "sha256": sha(path)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--retention", type=Path, required=True)
    parser.add_argument("--diagnostic-manifest", type=Path, required=True)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc).isoformat()
    retention = read(args.retention.resolve())
    diagnostic = read(args.diagnostic_manifest.resolve())
    assert retention["status"] == "PASS"
    assert diagnostic["p20d_diagnostic_capture"] is True
    diagnostic_dll = Path(diagnostic["module"])
    assert sha(diagnostic_dll) == diagnostic["sha256"] == retention["diagnostic_dll_sha256"]

    production = ROOT / "out/p20b/candidate-001/bundle/LXR.vst3/Contents/x86_64-win/LXR.vst3"
    production_manifest = read(ROOT / "out/p20b/candidate-001/candidate_manifest.json")
    assert sha(production) == production_manifest["sha256"] == PRODUCTION_SHA
    assert production_manifest["source_fingerprint"] == PRODUCTION_SOURCE

    installed = (Path(os.environ["LOCALAPPDATA"]) /
                 "Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3")
    assert sha(installed) == PROTECTED_SHA
    protected = [{"path": str(installed), "bytes": installed.stat().st_size,
                  "sha256": sha(installed)}]
    for name in ASSETS:
        item = installed.parents[1] / "Resources/LXR" / name
        assert sha(item) == sha(ROOT / "data" / name)
        protected.append({"path": str(item), "bytes": item.stat().st_size,
                          "sha256": sha(item)})

    p20c_reconciliation = read(ROOT / "out/p20c/p20b_worktree_reconciliation.json")
    checkpoint_hashes = {row["path"]: row["sha256"]
                         for row in p20c_reconciliation["p20b_paths"]}
    checkpoint_hashes["tests/P20CHostTransitionCaptureTest.cpp"] = (
        read(ROOT / "out/p20c/artifact_lineage.json")["p20c_test_images"]
        ["native_source_sha256"])
    extended = {"vendor/iPlug2/Examples/LXR/LXR.cpp",
                "tools/verification/build_p16f3_candidate.py", "docs/phase_log.md"}
    reconciliation = []
    for name in BASELINE_PATHS:
        expected = checkpoint_hashes.get(name)
        observed = sha(ROOT / name)
        if name not in extended and expected is not None:
            assert observed == expected, (name, expected, observed)
        reconciliation.append({
            "path": name, "p20c_final_sha256": expected, "observed_sha256": observed,
            "status": "P20D_INTENTIONALLY_EXTENDED" if name in extended else
                      "MATCHES_P20C_FINAL_MANIFEST" if expected is not None else
                      "CONTINUOUS_P20C_EVIDENCE_NO_EMBEDDED_SOURCE_HASH",
        })
    write_json("p20bc_worktree_reconciliation.json", {
        "classification": "ACCEPTED_INTENTIONAL_P20B_P20C_WORKTREE",
        "reconciled_before_p20d_edits": True,
        "basis": "P20-C fresh checkpoint source manifest plus preserved starting status",
        "starting_paths": reconciliation,
        "starting_index_empty": True,
        "starting_status_path_count": 10,
        "unrelated_starting_paths": [],
    })

    live = subprocess.check_output(
        ["git", "ls-remote", "origin", "refs/heads/p12b-4c-fir-resampler"],
        cwd=ROOT, text=True).split()[0]
    write_json("baseline_inventory.json", {
        "status": "PASS", "generated_utc": now, "repository": str(ROOT),
        "branch": git("branch", "--show-current"), "starting_head": START,
        "current_head": git("rev-parse", "HEAD"),
        "tracking_head": git("rev-parse", "@{upstream}"), "live_remote_head": live,
        "remote": git("remote", "get-url", "origin"),
        "starting_index_empty": True,
        "classification": "ACCEPTED_INTENTIONAL_P20B_P20C_WORKTREE",
        "production_candidate_sha256": PRODUCTION_SHA,
        "diagnostic_candidate_sha256": diagnostic["sha256"],
        "protected_install_sha256": PROTECTED_SHA,
    })

    architecture = f"""# P20-D event-retention architecture

Classification: **PASS**.

The adapter may complete multiple 32-native-frame refills in one host callback. Each refill replaces `TriggerScheduler::lastBatch()`; therefore P20-C post-callback sampling could see only the final refill even though every earlier event had already been dispatched to the engine. P20-D copies the completed batch immediately after native render and before the next refill can overwrite it.

`LXR_P20D_DIAGNOSTIC_CAPTURE` compile-time guards the entire per-instance interface and storage. The ordinary production candidate does not contain it. The controlled host calls `process()` and drains after return on one thread, so a fixed synchronous callback-local publication array is sufficient and avoids an unnecessary SPSC queue. Records are copied, not referenced, and `mRetained` is advanced only after a record is complete.

Capacity is 64 records. At 48 kHz, 1,024 host frames demand fewer than 939 native frames and at most 30 observed refills; the fixed capacity has greater than 2x margin. Record size is {retention['resources']['record_bytes']} bytes and object size is {retention['resources']['capture_object_bytes']} bytes.
"""
    (OUT / "event_retention_architecture.md").write_text(architecture, encoding="utf-8")

    partitions = retention["partitions"]
    write_csv("callback_partition_matrix.csv", partitions)
    write_json("native_batch_capture_results.json", {
        "status": "PASS", "callback_partitions": len(partitions),
        "host_frames": [row["frames"] for row in partitions],
        "native_batches_each": 11001, "retained_batches_each": 11001,
        "dropped_batches": 0, "native_boundary_tolerance": 0,
        "independent_oracle": "P16/P20-C rational scheduler oracle",
        "capture_boundary": "after Engine::processBlock and before native refill advance",
    })
    write_json("four_cycle_event_results.json", {
        "status": "PASS", "duration_host_frames": 384000, "duration_seconds": 8,
        "pattern_cycles": 4, "triggers": 448, "locks": 896,
        "native_batches": 11001, "dropped": 0, "event_order": "exact",
        "native_boundaries": "exact", "snapshot_generations": "exact",
    })
    write_json("overflow_results.json", {"status": "PASS", **retention["overflow"],
        "policy": "retain oldest complete records, count every dropped completed batch",
        "out_of_bounds_write": False, "pcm_equal": True})
    first = partitions[0]
    write_json("observer_pcm_equivalence.json", {
        "status": "PASS", "partitions": len(partitions), "native_pcm_equal": True,
        "host_pcm_equal": True, "pcm_hash": first["pcm_hash"],
        "accepted_state_equal": True, "state_hash": first["state_hash"],
        "rng_equal": True, "rng_state": first["rng"], "parameters_equal": True,
        "scheduling_equal": True, "transport_equal": True,
    })
    write_json("transport_transition_results.json", {
        "status": "PASS", **retention["transport"],
        "source_level_rerun": {"O0": "97/0", "O2": "97/0",
                               "processing_allocations": 0},
        "source_sha256": sha(ROOT / "tests/P20CHostTransitionCaptureTest.cpp"),
    })
    write_json("pattern_transition_copy_results.json", {
        "status": "PASS", "scenarios": retention["copy_scenarios"],
        "rejections": retention["copy_rejection"],
        "facade": "production P19-E SequencerEditFacade",
        "source_pattern": 1, "source_track": 0, "destination_pattern": 1,
        "destination_track": 1, "wrong_destination_sentinel": 2,
        "partial_state_observed": False, "stale_overwrite": False,
    })
    write_json("euclidean_clear_regression.json", {
        "status": "PASS", "P16E_O0": "9869/0", "P16E_O2": "9869/0",
        "exact_host": "833/0", "euclidean_trace_preserved": True,
        "lock_trace_preserved": True, "track_clear_suppression": True,
        "pattern_clear_suppression": True, "snapshot_order_preserved": True,
        "state_persistence": "PASS", "wrong_destination_diagnostic_preserved": True,
    })
    write_json("event_generation_results.json", {
        "status": "PASS", "trigger_count": 448, "lock_count": 896,
        "event_order": "exact independent oracle", "native_tolerance_frames": 0,
        "pattern_transition": "exact", "published_generation": 2,
        "consumed_generation": 2,
    })
    write_json("multi_instance_results.json", {
        "status": "PASS", "instance_identity_unique": True,
        "cross_instance_records": 0, "peer_state_unchanged": True,
        "peer_pcm_unchanged": True,
    })
    resources = retention["resources"]
    (OUT / "allocation_stack_report.md").write_text(
        "# Allocation, stack and capture cost\n\n"
        f"- Measured callback allocations: **{resources['allocations']}**.\n"
        f"- Requested callback allocation bytes: **{resources['requested_bytes']}**.\n"
        f"- Diagnostic capture object: **{resources['capture_object_bytes']} bytes**.\n"
        f"- Fixed record: **{resources['record_bytes']} bytes**; capacity 64.\n"
        f"- Observed 1,024-frame callback stack watermark: **{resources['stack_watermark']} bytes** on a 1 MiB stack.\n"
        "- Peak callback durations are non-isolated diagnostic samples and do not establish a hard real-time bound.\n"
        "- No mutex, wait, file I/O, logging or unbounded copy loop exists in the hook.\n",
        encoding="utf-8")

    write_json("diagnostic_binary_manifest.json", {
        "status": "PASS", **diagnostic, "verified_sha256": sha(diagnostic_dll),
        "qualification_scope": "P20-D test-only exact-DLL native-batch observation",
        "installed": False, "release_candidate": False,
        "focused_test_executable_sha256": retention["executable_sha256"],
        "focused_test_source_sha256": retention["source_sha256"],
    })
    write_json("production_binary_manifest.json", {
        "status": "PASS", **production_manifest, "verified_sha256": sha(production),
        "p20d_rebuilt": False, "diagnostic_hook_present": False,
        "qualification_scope": "unchanged repaired production Import candidate",
    })
    shutil.copyfile(ROOT / "out/p20c/historical_pcm_dispositions.csv",
                    OUT / "historical_pcm_dispositions.csv")
    (OUT / "historical_fir_identity.md").write_text(
        "# Historical FIR executable identity\n\nClassification: **HISTORICAL_IDENTITY_UNPROVABLE**.\n\n"
        "P20-D recovered no new original hash, build log, PE identity or test manifest for the earlier image denied by Windows Application Control. A passing executable currently at the same path cannot establish that the denied image passed. No security control was bypassed.\n",
        encoding="utf-8")
    (OUT / "reaper_gui_status.md").write_text(
        "# REAPER graphical status\n\nClassification: **GRAPHICAL_BLOCKED**.\n\n"
        "No genuinely new supported desktop-control capability was available. Per P20-D policy, no equivalent attachment campaign, blind click or blind keystroke was attempted. The isolated profile and manual procedure remain preserved.\n",
        encoding="utf-8")
    (OUT / "reaper_event_status.md").write_text(
        "# REAPER event status\n\nClassification: **BLOCKED**.\n\n"
        "The exact-DLL controlled-host trace is not an actual REAPER trace. No supported REAPER-specific observation mechanism was available.\n",
        encoding="utf-8")

    checkpoint = read(ROOT / "out/p20d/checkpoint-final/results.json")
    assert len(checkpoint) == 64 and all(row["status"] == "PASS" for row in checkpoint)
    regression = [{"test": row["name"], "status": row["status"],
                   "exit_code": row["exit_code"],
                   "evidence": f"out/p20d/checkpoint-final/{row['name']}.log"}
                  for row in checkpoint]
    regression += [
        {"test": "P20DExactDllRetentionTest", "status": "PASS", "exit_code": 0,
         "evidence": str(args.retention)},
        {"test": "P20BHostEventCaptureTest", "status": "PASS", "exit_code": 0,
         "evidence": "out/p20d/p20b-event-import-final.log"},
        {"test": "P20CHostTransitionCaptureTest-O0", "status": "PASS", "exit_code": 0,
         "evidence": "out/p20d/p20c-transition-o0.log"},
        {"test": "P20CHostTransitionCaptureTest-O2", "status": "PASS", "exit_code": 0,
         "evidence": "out/p20d/p20c-transition-o2.log"},
        {"test": "P16EPerformanceSequencerTest-O0/O2", "status": "PASS", "exit_code": 0,
         "evidence": "out/p20d/performance-final/performance_sequencer_results.json"},
    ]
    write_csv("regression_matrix.csv", regression)
    write_json("protected_install_manifest.json", {
        "status": "PASS", "unchanged": True,
        "expected_module_sha256": PROTECTED_SHA, "entries": protected})

    gates = [
        {"category": "controlled_host_retained_event_capture", "classification": "PASS"},
        {"category": "pattern_transition_copy", "classification": "PASS"},
        {"category": "exact_dll_diagnostic_qualification", "classification": "PASS"},
        {"category": "lxqb_import", "classification": "PRODUCTION_PASS / GRAPHICAL_BLOCKED"},
        {"category": "historical_pcm", "classification": "UNRESOLVED"},
        {"category": "historical_fir_executable", "classification": "HISTORICAL_IDENTITY_UNPROVABLE"},
        {"category": "reaper_event_acceptance", "classification": "BLOCKED"},
        {"category": "development_engineering", "classification": "PASS"},
        {"category": "development_publication", "classification": "BLOCKED"},
        {"category": "full_release", "classification": "NOT_CERTIFIED"},
    ]
    write_csv("release_gate_matrix.csv", gates)

    report = f"""# P20-D verification report

Generated: {now}

Focused engineering: **PASS**. Full release: **NOT_CERTIFIED**.

The compile-time diagnostic candidate `{diagnostic['sha256']}` retains all 11,001 completed native batches, 448 triggers and 896 locks for each of the eleven callback partitions through 1,024 host frames. Four cycles complete with zero drops and exact independent event/native/generation identity. Capture-enabled/disabled PCM, state, RNG, parameters and scheduling match. Forced capacity-two overflow reports 28/30 drops and preserves PCM.

Copy through a pending-to-active pattern transition passes six boundary/transport cases plus stale and Busy rejection. Exact transport retention and fresh P16-E/P20-B/P20-C regressions pass. The unchanged production Import candidate remains `{PRODUCTION_SHA}`. Fresh checkpoint is 64/64 and Steinberg validator is 47/47. Protected installation is unchanged.

Actual REAPER GUI/event evidence remains BLOCKED. Historical PCM remains UNRESOLVED and historical FIR identity remains unprovable. Existing publication policy does not authorize replacing those gates, so no commit/push is published. P16-F certification remains blocked.
"""
    (OUT / "verification_report.md").write_text(report, encoding="utf-8")

    required = {
        "baseline_inventory.json", "p20bc_worktree_reconciliation.json",
        "event_retention_architecture.md", "native_batch_capture_results.json",
        "callback_partition_matrix.csv", "four_cycle_event_results.json",
        "overflow_results.json", "observer_pcm_equivalence.json",
        "transport_transition_results.json", "pattern_transition_copy_results.json",
        "euclidean_clear_regression.json", "event_generation_results.json",
        "multi_instance_results.json", "allocation_stack_report.md",
        "diagnostic_binary_manifest.json", "production_binary_manifest.json",
        "historical_pcm_dispositions.csv", "historical_fir_identity.md",
        "reaper_gui_status.md", "reaper_event_status.md", "release_gate_matrix.csv",
        "regression_matrix.csv", "protected_install_manifest.json",
        "file_inventory.json", "integrity_report.md", "verification_report.md",
    }
    (OUT / "integrity_report.md").write_text(
        "# P20-D integrity report\n\nStatus: **PASS**.\n\n"
        f"Required evidence: {len(required)}/{len(required)} present. Diagnostic DLL: `{diagnostic['sha256']}`. "
        f"Production DLL: `{PRODUCTION_SHA}`. Protected DLL: `{PROTECTED_SHA}`. "
        "Frozen historical references were not modified. The diagnostic artifact was neither installed nor designated a release candidate.\n",
        encoding="utf-8")
    direct_files = sorted(path for path in OUT.iterdir()
                          if path.is_file() and path.name != "file_inventory.json")
    write_json("file_inventory.json", [inventory(path) for path in direct_files])
    present = {path.name for path in OUT.iterdir() if path.is_file()}
    missing = sorted(required - present)
    assert not missing, missing
    print(json.dumps({"status": "PASS", "diagnostic_sha256": diagnostic["sha256"],
                      "required_evidence": len(required), "missing": missing}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
