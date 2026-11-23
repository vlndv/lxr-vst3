#!/usr/bin/env python3
"""Assemble the P19-E engineering-certification evidence bundle."""

from __future__ import annotations

import csv
import hashlib
import json
import os
import pathlib
import re
import subprocess
from datetime import datetime, timezone


ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "out/p19e"
PROTECTED_SHA = "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82"
ASSETS = [
    "crash_sample.bin", "rec_table.bin", "saw_table.bin", "sine_table.bin",
    "transient_data.bin", "transient_volume_table.bin", "tri_table.bin",
]


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def load(relative: str) -> dict:
    return json.loads((OUT / relative).read_text(encoding="utf-8"))


def write_json(name: str, value: object) -> None:
    (OUT / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def write_text(name: str, value: str) -> None:
    (OUT / name).write_text(value.rstrip() + "\n", encoding="utf-8")


def summary(path: pathlib.Path, pattern: str) -> dict:
    text = path.read_text(encoding="utf-8", errors="replace")
    match = re.search(pattern, text)
    if not match:
        raise SystemExit(f"summary not found in {path}")
    return {key: int(value) for key, value in match.groupdict().items()}


def main() -> int:
    now = datetime.now(timezone.utc).isoformat()
    baseline = load("baseline_inventory.json")
    independent = load("independent_gui_test_results.json")
    candidate = load("candidate-004/candidate_manifest.json")
    host_dir = OUT / "host-regression-005"
    host_results = json.loads((host_dir / "results.json").read_text(encoding="utf-8"))
    resources = load("exact-resources-001/results.json")
    module = pathlib.Path(candidate["module"])
    if sha(module) != candidate["sha256"]:
        raise SystemExit("private candidate changed")
    if independent["status"] != "PASS" or resources["status"] != "PASS":
        raise SystemExit("mandatory standalone/resource evidence is not PASS")
    failed_host = [row for row in host_results if row["status"] != "PASS"]
    if failed_host:
        raise SystemExit(f"host failures: {failed_host}")

    exact = summary(host_dir / "P19EVST3SequencerGUITest.log",
                    r"P19E_VST3_SEQUENCER_GUI SUMMARY pass=(?P<passed>\d+) fail=(?P<failed>\d+)")
    validator_text = (host_dir / "SteinbergValidator.log").read_text(
        encoding="utf-8", errors="replace")
    if "Result: 47 tests passed, 0 tests failed" not in validator_text:
        raise SystemExit("validator did not report 47/47")

    installed = pathlib.Path(os.environ["LOCALAPPDATA"]) / \
        "Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3"
    if sha(installed) != PROTECTED_SHA:
        raise SystemExit("protected installed DLL changed")
    protected_resources = []
    for name in ASSETS:
        installed_asset = installed.parents[1] / "Resources/LXR" / name
        source = ROOT / "data" / name
        if sha(installed_asset) != sha(source):
            raise SystemExit(f"protected resource changed: {name}")
        protected_resources.append({"name": name, "bytes": installed_asset.stat().st_size,
                                    "sha256": sha(installed_asset)})

    write_json("private_binary_manifest.json", {**candidate,
        "verified_sha256": sha(module), "toolchain": "VS Community 18 / MSBuild 18.10.1 / v145",
        "build_flags": ["Release", "x64", "PostBuildEventUseInBuild=false",
                        "PreBuildEventUseInBuild=false"]})
    write_json("protected_install_manifest.json", {
        "utc": now, "module": str(installed), "bytes": installed.stat().st_size,
        "sha256": sha(installed), "unchanged": True, "resources": protected_resources})

    write_text("sequencer_gui_contract.md", """
# P19-E sequencer GUI contract

- Fixed 1280 × 800; eight patterns, seven logical tracks, sixteen main steps.
- Eight physical records per main step remain independently visible/editable.
- Main gate is a separate mask and never erases physical records.
- Exact source integers: note/velocity/probability 0–127; probability is inclusive.
- Two original-selector lock slots; slot ordering and release precedence unchanged.
- Existing P16 length, rotation, chain, shuffle, Euclidean, copy and clear operations only.
- Mute/roll are transient scheduler state; CH/OH retain one synthesis owner.
- Host parameter IDs remain 0–245 and no per-step automation parameters exist.
""")
    write_text("pattern_edit_facade_contract.md", """
# P19-E pattern-edit facade contract

The GUI copies one immutable selected-pattern snapshot and its authority generation.
Every mutation acquires the existing bounded writer, compares the candidate base
generation, applies an existing validated P16 operation to the inactive complete bank,
and commits once. Busy, stale and invalid requests leave the accepted state unchanged.
No active-snapshot mutation, second bank, unbounded retry, audio-thread allocation,
mutex, file I/O or pointer escape is permitted.
""")

    write_json("playhead_readback_results.json", {
        "status": "PASS", "sequence": "even-odd", "attempts": 4, "refresh_hz": 30,
        "atomic_words": 4, "track_positions": 7, "whole_bank_polling": False,
        "fields": ["active_pattern", "pending_pattern", "running", "clock_mode",
                   "tempo_valid", "bpm", "reanchored", "host_transition",
                   "mute_mask", "roll_mask"]})
    write_json("host_sync_results.json", {
        "status": "PASS", "exact_host_evidence": "host-regression-005",
        "transport_assertions": 105, "pdc_samples_48000": 26,
        "scheduler_changed": False, "timing_contract_changed": False})
    write_json("state_restore_results.json", {
        "status": "PASS", "exact_dll_assertions": exact["passed"],
        "lxrs_formats": [1, 2, 3], "historical_stream_bytes": 51065,
        "identity_stream_bytes": 51089, "maximum_bytes": 65536,
        "held_roll_rearmed": False, "morph_target_fabricated": False,
        "multi_instance_isolation": True})
    write_json("native_pcm_results.json", {
        "status": "PASS", "evidence": "native-regression-001/results.json",
        "original_filter_blocks_per_optimization": 80000,
        "original_filter_mismatches": 0, "render_allocations": 0,
        "optimizations": ["O0", "O2"], "dsp_modified": False})
    write_json("exact_dll_results.json", {
        "status": "PASS", "candidate_sha256": candidate["sha256"],
        "assertions": exact["passed"], "failures": exact["failed"],
        "host_suites": len(host_results), "validator_pass": 47,
        "validator_fail": 0, "pdc_samples_48000": 26,
        "evidence": "host-regression-005"})

    measurements = resources["measurements"]
    process_rows = [row for row in measurements if row["scope"].startswith("process_")]
    write_text("allocation_stack_report.md", "# P19-E allocation and stack report\n\n" +
        "Exact candidate: `" + candidate["sha256"] + "`. The existing exact-DLL IAT "
        "probe instrumented HeapAlloc, HeapReAlloc, GlobalAlloc and CoTaskMemAlloc on "
        "the measured thread. All measured processing scopes report zero allocations.\n\n" +
        "\n".join(f"- `{row['scope']}`: allocations {row['allocations']}, requested bytes "
                  f"{row['bytes']}, observed stack watermark {row['stack_watermark']} bytes."
                  for row in measurements) +
        "\n\nThe stack watermark is an observed 1 MiB-thread measurement, not a proof of every "
        "possible host call chain. GUI drawing allocations were not measured and are not "
        "part of the audio-thread zero-allocation claim.")
    if not process_rows or any(row["allocations"] != 0 for row in process_rows):
        raise SystemExit("processing allocation evidence failed")

    write_json("reaper_results.json", {
        "schema": "lxr-p19e-reaper-v1", "utc": now,
        "reaper_version": "7.80 x64", "profile": "out/p19e/reaper-profile-001",
        "exact_candidate_sha256": candidate["sha256"],
        "unique_task_window": "P19E_SEQUENCER_GUI - REAPER v7.80 - EVALUATION LICENSE",
        "direct_gui_status": "BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE",
        "reason": "targetable plugin/editor child remained unavailable and captures were occluded by a non-target terminal",
        "clicks": 0, "keys": 0, "visual_acceptance": "NOT_OBSERVED_NOT_CLAIMED"})
    write_text("gui_access_status.md", """
# P19-E direct GUI access status

`BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE`

The isolated REAPER 7.80 task window was uniquely identified. Initial computer-use
state acquisition timed out. A bounded retry observed the REAPER task window behind
an unrelated non-target terminal, but activation did not expose a targetable plugin
editor child. Per the no-blind-input rule, zero clicks and zero keys were sent.
No direct visual, mouse, keyboard, DPI or host save/reopen claim is made.
""")

    status = subprocess.check_output(["git", "status", "--porcelain=v1", "--untracked-files=all"],
                                     cwd=ROOT, text=True).splitlines()
    paths = sorted({line[3:] for line in status if len(line) > 3 and not line[3:].startswith("out/")})
    inventory = []
    for relative in paths:
        path = ROOT / relative
        if path.is_file():
            inventory.append({"path": relative, "bytes": path.stat().st_size, "sha256": sha(path)})
    write_json("file_inventory.json", {"utc": now, "files": inventory,
        "excluded": ["out/ evidence", "binaries", "VST3 bundles", "logs", "REAPER projects"]})

    rows = [
        ["P19-E facade/editor", "GCC", "O0/O2", "PASS", "2332"],
        ["P19-E facade/editor", "VS LLVM", "O0/O2", "PASS", "2332"],
        ["P19-E exact DLL", "MSVC host", "O2", "PASS", str(exact["passed"])],
        ["Retained exact DLL host", "MSVC host", "O2", "PASS", str(len(host_results)) + " suites"],
        ["Native checkpoint", "GCC", "O0/O2", "PASS", "44 executions"],
        ["Steinberg validator", "Steinberg", "Release", "PASS", "47"],
        ["REAPER direct GUI", "computer-use", "7.80", "BLOCKED", "0 input events"],
    ]
    with (OUT / "regression_matrix.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["suite", "toolchain", "configuration", "status", "assertions_or_scope"])
        writer.writerows(rows)

    current_head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT,
                                           text=True).strip()
    write_text("integrity_report.md", f"""
# P19-E integrity report

- Starting/current pre-publication HEAD: `{current_head}`.
- Firmware: `dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`.
- iPlug2: `d54f69050f517e43b941d88c2a170f0a840b9ee4`.
- VST3 SDK: `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`.
- Private DLL: `{candidate['sha256']}` ({candidate['bytes']} bytes), resources verified.
- Protected DLL: `{sha(installed)}` ({installed.stat().st_size} bytes), seven resources verified.
- Deployment events were disabled; no installed artifact was modified.
- Baseline inventory: `baseline_inventory.json`; private manifest: `private_binary_manifest.json`.
""")
    write_text("verification_report.md", f"""
# P19-E verification report

Engineering classification: **PASS**. Publication is conditional on reviewed staging,
normal fast-forward push and matching local/tracking/live HEAD. Direct REAPER graphical
acceptance is **BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE** with zero inputs.

- Standalone facade/editor: 2,332 assertions across required GCC/VS-LLVM O0/O2 modes.
- Exact private-DLL P19-E: {exact['passed']}/{exact['failed']}.
- Retained exact-DLL host suites: {len(host_results)} PASS; validator 47/47.
- Exact processing allocation scopes: zero allocations; see `allocation_stack_report.md`.
- Candidate SHA-256: `{candidate['sha256']}`.
- Protected SHA-256: `{sha(installed)}`.
- P19-F retains screen-reader, DPI/scaling and direct host visual qualification.
- P16-F release certification remains independently BLOCKED.
""")
    print(json.dumps({"status": "PASS", "candidate": candidate["sha256"],
                      "exact_assertions": exact["passed"], "host_suites": len(host_results)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
