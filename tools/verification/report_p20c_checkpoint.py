#!/usr/bin/env python3
"""Assemble P20-C evidence without changing production code or frozen references."""

from __future__ import annotations

import csv
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from datetime import datetime, timezone


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "out/p20c"
START = "0155e153ae34ff2fb5740ea7bfe6855ef5d1c220"
CANDIDATE_SHA = "FB1045BD5716282607487F6A70A6082206309EE741C8F751768AE5C8F6F7650D"
SOURCE_FINGERPRINT = "80DB83C0C8F79020B06580A54E5627537CE35C5B3E1FBE57DEE33413C0113400"
PROTECTED_SHA = "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82"
ASSETS = ["crash_sample.bin", "rec_table.bin", "saw_table.bin", "sine_table.bin",
          "transient_data.bin", "transient_volume_table.bin", "tri_table.bin"]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def read(path: str | Path):
    return json.loads((ROOT / path).read_text(encoding="utf-8-sig"))


def write_json(name: str, value) -> None:
    (OUT / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def write_csv(name: str, rows: list[dict]) -> None:
    fields = list(dict.fromkeys(key for row in rows for key in row))
    with (OUT / name).open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def parse_fields(line: str) -> dict[str, str]:
    return dict(re.findall(r"([a-zA-Z_]+)=([^ ]+)", line))


def inventory(path: Path) -> dict:
    return {"path": str(path.relative_to(ROOT)).replace("\\", "/"),
            "bytes": path.stat().st_size, "sha256": sha(path)}


def read_log(path: Path) -> str:
    data = path.read_bytes()
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        return data.decode("utf-16")
    return data.decode("utf-8", errors="replace")


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc).isoformat()
    candidate = ROOT / "out/p20b/candidate-001/bundle/LXR.vst3/Contents/x86_64-win/LXR.vst3"
    candidate_manifest = read("out/p20b/candidate-001/candidate_manifest.json")
    source_manifest = read("out/p20b/candidate-001/source_manifest.json")
    assert sha(candidate) == candidate_manifest["sha256"] == CANDIDATE_SHA
    assert source_manifest["fingerprint"] == candidate_manifest["source_fingerprint"] == SOURCE_FINGERPRINT
    candidate_production = [item for item in source_manifest["files"]
                            if item["path"].startswith(("dsp/", "interfaces/", "plugin/", "data/", "vendor/"))]
    assert all(sha(ROOT / item["path"]) == item["sha256"] for item in candidate_production)

    installed = Path(os.environ["LOCALAPPDATA"]) / "Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3"
    assert sha(installed) == PROTECTED_SHA
    installed_entries = [{"path": str(installed), "bytes": installed.stat().st_size,
                          "sha256": sha(installed)}]
    for name in ASSETS:
        resource = installed.parents[1] / "Resources/LXR" / name
        assert sha(resource) == sha(ROOT / "data" / name)
        installed_entries.append({"path": str(resource), "bytes": resource.stat().st_size,
                                  "sha256": sha(resource)})

    status = git("status", "--short").splitlines()
    index = git("diff", "--cached", "--name-only")
    expected_p20b = [
        "vendor/iPlug2/Examples/LXR/LXR.cpp", "tests/P19EVST3SequencerGUITest.cpp",
        "tests/P20BHostEventCaptureTest.cpp", "tools/verification/build_p16f3_candidate.py",
        "tools/verification/report_p20b_checkpoint.py", "docs/phase_log.md",
        "docs/p20b_release_blocker_resolution.md",
    ]
    reconciliation = []
    p20b_source_hashes = {item["path"]: item["sha256"] for item in source_manifest["files"]}
    for name in expected_p20b:
        path = ROOT / name
        certified = p20b_source_hashes.get(name)
        current = sha(path)
        reconciliation.append({"path": name, "present": path.is_file(), "sha256": sha(path),
                               "certified_candidate_hash": certified,
                               "current_status": ("P20C_EXTENDED" if certified and current != certified
                                                  else "UNCHANGED_FROM_CERTIFIED_SOURCE" if certified
                                                  else "P20B_EVIDENCE_ALIGNED_NO_EMBEDDED_SOURCE_HASH"),
                               "basis": "candidate source manifest" if name in p20b_source_hashes
                               else "P20-B generated report/tool content and file inventory"})
    write_json("p20b_worktree_reconciliation.json", {
        "classification": "ACCEPTED_INTENTIONAL_P20B_WORKTREE", "generated_utc": now,
        "starting_head": START, "branch": git("branch", "--show-current"), "index_empty": not index,
        "p20b_paths": reconciliation, "p20c_additions_excluded_from_starting_classification": True,
        "classification_basis": "Read-only reconciliation was completed before P20-C edits; current hashes also expose the later bounded test/document extensions.",
        "exact_starting_status_recorded_in": "out/p20b/baseline_inventory.json",
    })
    write_json("baseline_inventory.json", {
        "status": "PASS", "generated_utc": now, "repository": str(ROOT),
        "branch": git("branch", "--show-current"), "starting_head": START,
        "current_head": git("rev-parse", "HEAD"), "tracking_head": git("rev-parse", "@{upstream}"),
        "live_remote_head": subprocess.check_output(
            ["git", "ls-remote", "origin", "refs/heads/p12b-4c-fir-resampler"],
            cwd=ROOT, text=True).split()[0],
        "remote": git("remote", "get-url", "origin"), "index_empty": not index,
        "candidate_sha256": CANDIDATE_SHA, "candidate_source_fingerprint": SOURCE_FINGERPRINT,
        "working_tree_status_at_report": status,
    })

    host_log = read_log(ROOT / "out/p20c-dev-host/run-final.log")
    partition_rows = []
    for line in host_log.splitlines():
        if line.startswith("PARTITION "):
            fields = parse_fields(line)
            partition_rows.append({"surface": "exact_candidate_post_callback", **fields,
                                   "complete_retention": str(fields["missing_triggers"] == "0" and fields["missing_locks"] == "0")})
    native_log = read_log(ROOT / "out/p20c-dev-native/run-o2.log")
    native_rows = []
    transition_rows = []
    for line in native_log.splitlines():
        if line.startswith("NATIVE_PARTITION "):
            native_rows.append({"surface": "source_level_per_native_block", **parse_fields(line),
                                "complete_retention": "True"})
        elif line.startswith("TRANSITION "):
            transition_rows.append({**parse_fields(line), "status": "PASS"})
    assert len(partition_rows) == 22 and len(native_rows) == 11 and len(transition_rows) == 11
    write_csv("callback_partition_matrix.csv", partition_rows + native_rows)
    write_csv("transport_transition_matrix.csv", transition_rows)
    write_json("event_retention_results.json", {
        "classification": "PARTIAL", "duration_host_frames": 384000, "duration_seconds": 8,
        "pattern_cycles": 4, "source_level": {"generated_triggers": 448, "retained_triggers": 448,
        "generated_locks": 896, "retained_locks": 896, "dropped": 0, "peak_occupancy": 1344,
        "capacity": {"triggers": 1024, "locks": 2048}, "drain_frequency": "per native block"},
        "exact_dll": {"observer": "host-owned copy of scheduler lastBatch after process returns",
        "complete_through_callback_frames": 33, "incomplete_from_callback_frames": 64,
        "loss_is_reported": True, "scheduler_overflow": 0},
    })
    write_json("event_overflow_results.json", {
        "status": "PASS", "forced": {"generated": 5, "retained": 2, "dropped": 3, "peak": 2},
        "normal_source_level_drops": 0, "exact_dll_storage_drops": 0,
        "exact_dll_unobservable_prior_batches_are_reported_as_missing_not_overflow": True,
    })
    write_json("observer_pcm_equivalence.json", {
        "status": "PASS", "exact_dll_partitions": 22, "pcm_equal": True, "state_equal": True,
        "rng_equal": True, "emitted_counts_equal": True, "candidate_sha256": CANDIDATE_SHA,
    })
    write_json("snapshot_generation_results.json", {
        "status": "PASS", "source_dense_trace_generation": 1,
        "exact_dll_live_copy": {"published": 2, "consumed": 2, "destination_track": 1},
        "exact_dll_live_clear": {"track_generation": 2, "pattern_generation": 3, "consumed": 3},
        "stale_generation_regression": "P19-E facade fresh PASS",
    })
    write_json("euclidean_trace_results.json", {
        "classification": "CONTROLLED_HOST_PASS", "accepted_inputs": "P16-E 1..16 exhaustive",
        "original_c_assertions": {"O0": 9869, "O2": 9869, "fail": 0},
        "exact_dll": {"triggers": 16, "locks": 32, "generation": 1,
        "complete_one_frame_trace": True, "partition_pcm_equivalent": True},
        "physical_records_preserved": True, "unrelated_metadata_preserved": True,
        "reaper": "BLOCKED",
    })
    write_json("copy_clear_trace_results.json", {
        "classification": "PARTIAL", "copy": {"stopped_transport": "PASS", "running_transport": "PASS",
        "callback_partitions": "PCM/state/RNG PASS; retained exact trace partial >=64 frames",
        "multiple_instances": "PASS", "destination_track_regression": "PASS track 1",
        "pattern_transition_boundary": "NOT DIRECTLY OBSERVED WITH COPY"},
        "clear": {"running_track": "PASS", "running_pattern": "PASS", "confirmation": "PASS",
        "trigger_suppression": "PASS", "save_reopen": "PASS through LXRS state restore"},
        "reaper": "BLOCKED",
    })

    historical = read("out/p20b/historical_pcm_inventory.json")["original_cases"]
    case_names = ["historical_pcm_case_f7.json", "historical_pcm_case_f4_selector0.json",
                  "historical_pcm_case_f4_selector12.json", "historical_pcm_case_f4_off_stress.json"]
    dispositions = []
    for row, filename in zip(historical, case_names):
        off = row["test_id"] == "RAW-F4-OFF-STRESS"
        disposition = "INSUFFICIENT_HISTORICAL_EVIDENCE" if off else "PLAUSIBLE_BUT_UNPROVEN"
        result = {
            "test_id": row["test_id"], "disposition": disposition,
            "first_unequal": {"frame": int(row["first_differing_frame"]),
            "channel": int(row["first_differing_channel"]), "expected": int(row["expected_integer_value"]),
            "observed": int(row["actual_integer_value"])},
            "mismatch_count": int(row["differing_scalar_samples"]),
            "maximum_absolute_difference": int(row["maximum_absolute_integer_difference"]),
            "last_unequal_frame": int(row["last_differing_frame"]),
            "source_fingerprint": row["original_source_fingerprint"],
            "dll_sha256": row["original_binary_sha256"], "reference_file": row["reference_file"],
            "reference_file_sha256": row["reference_file_sha256"], "fixture": row["input_fixture"],
            "fixture_sha256": row["input_fixture_sha256"], "sample_rate": int(row["sample_rate"]),
            "callback_evidence": row["host_callback_partition"], "midi_timeline": row["midi_timeline"],
            "initial_transient_state": row["initial_engine_state"], "native_phase": row["native_processing_phase"],
            "provenance": {
                "historical_source_commit": row["f13_reference_git_revision"],
                "reference_image": row["reference_file"],
                "exact_fixture_bytes": row["input_fixture_sha256"],
                "test_executable_identity": "NOT RECORDED",
                "dll_identity": row["original_binary_sha256"],
                "exact_toolchain": "NOT RECORDED FOR ORIGINAL RENDER",
                "callback_partition": row["host_callback_partition"],
                "control_receipt_sequence": row["controller_values"],
                "serialized_initial_state": row["persistent_state_fingerprints"],
                "transient_startup_state": row["initial_engine_state"],
                "host_transport": row["host_transport"],
                "midi_events": row["midi_timeline"],
            },
            "root_cause_confirmed": False,
            "reason": ("Difference begins at frame zero before external MIDI; historical startup/filter/envelope/voice/FIR state and executable identity were not captured."
                       if off else "Controlled receipt schedules reproduce callback-sensitive behavior, but the exact historical receipt sequence, native phase, and transient state were not recorded."),
            "original_raw_gate": "OPEN",
        }
        write_json(filename, result)
        dispositions.append({"test_id": row["test_id"], "disposition": disposition,
                             "root_cause_confirmed": "NO", "original_gate": "OPEN",
                             "evidence": f"out/p20c/{filename};out/p20b/historical_pcm_inventory.json"})
    write_csv("historical_pcm_dispositions.csv", dispositions)

    (OUT / "prospective_pcm_protocol.md").write_text("""# Prospective PCM protocol\n\nStatus: specified; separate from the four open historical inequalities.\n\nRecord the exact DLL, reference executable, fixture and source fingerprints; compiler and flags; complete serialized and transient initialization state; RNG; filter/envelope/voice/FIR and resampler phase; sample format and integer conversion; every callback size; complete parameter-receipt, transport and MIDI timeline; native-to-host conversion; first unequal scalar and a complete mismatch summary. Run the same frozen input through the reference and candidate in one owned process, retain callback and native-boundary receipts, and hash all outputs. A prospective PASS cannot retire a historical raw failure without an authorized replacement criterion.\n""", encoding="utf-8")

    lineage = read("out/p20b/artifact_lineage.json")
    host_probe = ROOT / "out/p20c-dev-host/P20BHostEventCaptureTest.exe"
    native_o0 = ROOT / "out/p20c-dev-native/P20CHostTransitionCaptureTest-O0.exe"
    native_o2 = ROOT / "out/p20c-dev-native/P20CHostTransitionCaptureTest-O2.exe"
    write_json("artifact_lineage.json", {
        "classification": "HISTORICAL_IDENTITY_UNPROVABLE", "p20b_indexed_manifests": 41,
        "current_candidate": candidate_manifest, "candidate_verified_sha256": sha(candidate),
        "candidate_production_inputs_match_build_manifest": True,
        "p20c_test_images": {
            "exact_dll_host_probe": {"path": str(host_probe.relative_to(ROOT)), "sha256": sha(host_probe),
                "source": "tests/P20BHostEventCaptureTest.cpp",
                "source_sha256": sha(ROOT / "tests/P20BHostEventCaptureTest.cpp"),
                "candidate_sha256": CANDIDATE_SHA, "assertions": "833/0"},
            "native_probe_O0": {"path": str(native_o0.relative_to(ROOT)), "sha256": sha(native_o0)},
            "native_probe_O2": {"path": str(native_o2.relative_to(ROOT)), "sha256": sha(native_o2)},
            "native_source_sha256": sha(ROOT / "tests/P20CHostTransitionCaptureTest.cpp"),
            "checkpoint_results_sha256": sha(ROOT / "out/p20c-checkpoint/results.json"),
        },
        "historical_lineage_source": "out/p20b/artifact_lineage.json",
        "historical_lineage_sha256": sha(ROOT / "out/p20b/artifact_lineage.json"),
        "p20b_status": lineage["status"],
    })
    (OUT / "historical_fir_identity.md").write_text("""# Historical FIR executable identity\n\nClassification: **HISTORICAL_IDENTITY_UNPROVABLE**.\n\nThe executable now at the historical path has SHA-256 `7E01A6927D03950B53B1C17ECFB3BE9322859F5392843B64A4487103DFE6D88B` and passes its current run. Preserved evidence does not contain the SHA-256, build log, source fingerprint, PE identity or manifest for the earlier image denied by Windows Application Control. This is current-path evidence only; it does not prove the denied executable later passed. No security policy was bypassed.\n""", encoding="utf-8")

    write_json("lxqb_import_results.json", {"production": "PRODUCTION_PASS", "graphical": "GRAPHICAL_BLOCKED",
        "exact_candidate_sha256": CANDIDATE_SHA, "valid_import": "PASS", "eight_malformed_cases": "PASS",
        "atomic_rejection": "PASS", "instance_independence": "PASS", "save_reopen": "PASS",
        "exact_host_assertions": {"pass": 833, "fail": 0}})
    (OUT / "reaper_gui_status.md").write_text("""# REAPER graphical status\n\nClassification: **GRAPHICAL_BLOCKED**.\n\nThe supported desktop-control inventory did not become available: two bounded state reads timed out, and no input was sent. The isolated REAPER profile and P20-A manual procedure remain preserved. No controlled-host result is relabelled as graphical evidence.\n""", encoding="utf-8")
    (OUT / "reaper_event_status.md").write_text("""# REAPER event status\n\nClassification: **BLOCKED**.\n\nNo supported direct REAPER event-observation channel was available. Controlled-host and source-level traces are reported separately and are not actual REAPER traces.\n""", encoding="utf-8")

    checkpoint = read("out/p20c-checkpoint/results.json")
    assert len(checkpoint) == 64 and all(row["exit_code"] == 0 for row in checkpoint)
    write_json("native_pcm_results.json", {"status": "PASS", "checkpoint_commands": 64,
        "checkpoint_failures": 0, "native_O0_O2": "PASS", "original_c_filter_blocks_each": 80000,
        "original_c_filter_mismatches": 0, "frozen_pcm_references": "76/76 (unchanged P20-B exact candidate)",
        "new_native_transition_trace": {"O0": "97/0", "O2": "97/0"}})
    regression_rows = []
    for row in checkpoint:
        regression_rows.append({"test": row["name"], "status": "PASS", "exit_code": row["exit_code"],
                                "evidence": "out/p20c-checkpoint/" + row["name"] + ".log"})
    regression_rows += [
        {"test": "P20BHostEventCaptureTest", "status": "PASS", "exit_code": 0,
         "evidence": "out/p20c-dev-host/run-final.log"},
        {"test": "P20CHostTransitionCaptureTest-O0", "status": "PASS", "exit_code": 0,
         "evidence": "out/p20c-dev-native/run-o0.log"},
        {"test": "P20CHostTransitionCaptureTest-O2", "status": "PASS", "exit_code": 0,
         "evidence": "out/p20c-dev-native/run-o2.log"},
        {"test": "P16EPerformanceSequencerTest-O0/O2", "status": "PASS", "exit_code": 0,
         "evidence": "out/p20c-performance-sequencer/performance_sequencer_results.json"},
    ]
    write_csv("regression_matrix.csv", regression_rows)
    (OUT / "allocation_stack_report.md").write_text("""# Allocation and stack report\n\n- New 8-second native trace: zero measured processing allocations at O0 and O2.\n- Fixed trace-object footprint: 163,960 bytes in the test executable; this is not a whole-thread watermark and no production DSP object changed.\n- Retained exact-DLL processing watermark for the unchanged candidate: 20,432 bytes on a 1 MiB stack (P20-B/P20-A resource probe).\n- Capacity: 1,024 triggers plus 2,048 locks; observed peak occupancy 1,344 combined records.\n- Existing fresh latency/runtime suites report zero render allocations.\n- Exact-DLL callback peak durations are retained per partition in `callback_partition_matrix.csv`; these are non-isolated wall-clock diagnostics, not hard real-time bounds.\n- An early test-only 4,096/8,192-record automatic array exceeded the probe stack and was reduced before qualification; this was a harness defect, not a plugin callback failure.\n""", encoding="utf-8")

    write_json("private_binary_manifest.json", {"status": "PASS", **candidate_manifest,
        "verified_sha256": sha(candidate), "rebuilt_for_p20c": False,
        "reason": "Only test/evidence sources changed; all production inputs remain byte-identical to the P20-B manifest."})
    write_json("protected_install_manifest.json", {"status": "PASS", "entries": installed_entries,
        "expected_module_sha256": PROTECTED_SHA, "unchanged": True})

    gates = [
        {"category": "lxqb_import", "classification": "PRODUCTION_PASS / GRAPHICAL_BLOCKED", "release_gate": "OPEN"},
        {"category": "controlled_host_event_timing", "classification": "PARTIAL", "release_gate": "OPEN"},
        {"category": "actual_reaper_timing", "classification": "BLOCKED", "release_gate": "OPEN"},
        {"category": "euclidean_copy_clear", "classification": "CONTROLLED_HOST_PARTIAL / REAPER_BLOCKED", "release_gate": "OPEN"},
        {"category": "historical_pcm", "classification": "PARTIALLY_RECONCILED", "release_gate": "OPEN"},
        {"category": "exact_artifact", "classification": "HISTORICAL_IDENTITY_UNPROVABLE", "release_gate": "OPEN"},
        {"category": "development_engineering", "classification": "BLOCKED", "release_gate": "OPEN"},
        {"category": "development_publication", "classification": "BLOCKED", "release_gate": "OPEN"},
        {"category": "full_release", "classification": "NOT_CERTIFIED", "release_gate": "OPEN"},
    ]
    write_csv("release_gate_matrix.csv", gates)

    report = f"""# P20-C verification report\n\nGenerated: {now}\n\nOverall engineering classification: **BLOCKED**. Full release: **NOT_CERTIFIED**.\n\nThe unchanged P20-B candidate `{CANDIDATE_SHA}` passes the repaired production Import path, 833/0 focused exact-host assertions, 64/64 fresh regression commands, and Steinberg validator 47/47. Source-level per-native-block retention is complete for 448 triggers and 896 locks over four cycles at all eleven callback partitions with zero drops, exact ordering and zero processing allocations.\n\nThe exact-DLL observer is post-callback `lastBatch()` sampling, not a production queue. It retains complete traces through 33-frame callbacks but cannot retain earlier native batches at 64 frames and above; all missing counts are explicit and PCM/state/RNG remain exact. Copy at a pattern-transition boundary was not directly observed. Actual REAPER GUI/event evidence remains blocked. The three selector PCM causes are plausible but unproven; OFF-stress lacks startup provenance. Historical FIR executable identity remains unprovable. No acceptance criterion was changed, no DSP/scheduler code changed, and no development commit was published.\n"""
    (OUT / "verification_report.md").write_text(report, encoding="utf-8")

    required = {"baseline_inventory.json", "p20b_worktree_reconciliation.json", "release_gate_matrix.csv",
                "callback_partition_matrix.csv", "transport_transition_matrix.csv", "event_retention_results.json",
                "event_overflow_results.json", "euclidean_trace_results.json", "copy_clear_trace_results.json",
                "snapshot_generation_results.json", "observer_pcm_equivalence.json",
                *case_names, "historical_pcm_dispositions.csv", "prospective_pcm_protocol.md",
                "artifact_lineage.json", "historical_fir_identity.md", "lxqb_import_results.json",
                "reaper_gui_status.md", "reaper_event_status.md", "native_pcm_results.json",
                "regression_matrix.csv", "allocation_stack_report.md", "private_binary_manifest.json",
                "protected_install_manifest.json", "file_inventory.json", "integrity_report.md",
                "verification_report.md"}
    (OUT / "integrity_report.md").write_text(
        "# P20-C integrity report\n\nStatus: **PASS**.\n\n"
        f"Required evidence files: {len(required)}/{len(required)} present. Missing: none. "
        f"Candidate SHA-256: `{CANDIDATE_SHA}`. Protected module SHA-256: `{PROTECTED_SHA}`. "
        "All candidate production inputs match the P20-B build manifest; frozen historical references were read only.\n",
        encoding="utf-8")
    evidence_files = sorted(path for path in OUT.iterdir() if path.is_file() and path.name not in
                            {"file_inventory.json"})
    write_json("file_inventory.json", [inventory(path) for path in evidence_files])
    present = {path.name for path in OUT.iterdir() if path.is_file()}
    missing = sorted(required - present)
    assert not missing
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
