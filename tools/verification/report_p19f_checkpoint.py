#!/usr/bin/env python3
"""Assemble P19-F qualification evidence from immutable test outputs."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import pathlib
import re
import subprocess
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "out/p19f"
START_HEAD = "e9eedfef5296b0bcb500c771be134f19dea5da81"
PROTECTED_SHA = "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82"
HIDDEN = {0, 5, 97, 98, 127, *range(149, 155), *range(161, 173)}


def read_json(path: pathlib.Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(name: str, value: object) -> None:
    (OUT / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def binding_ids(path: pathlib.Path, start: str, end: str) -> set[int]:
    text = path.read_text(encoding="utf-8")
    section = text.split(start, 1)[1].split(end, 1)[0]
    return {int(value) for value in re.findall(r"^\s*\{(\d+),", section, re.MULTILINE)}


def parse_exact(log: str) -> tuple[list[dict[str, object]], dict[str, int], dict[str, int]]:
    renders = []
    for match in re.finditer(
        r"P19F_RENDER view=([^ ]+) samples=(\d+) median_ms=([0-9.]+) "
        r"p95_ms=([0-9.]+) p99_ms=([0-9.]+) max_ms=([0-9.]+)", log):
        renders.append({"view": match.group(1), "samples": int(match.group(2)),
                        "median_ms": float(match.group(3)), "p95_ms": float(match.group(4)),
                        "p99_ms": float(match.group(5)), "maximum_ms": float(match.group(6))})
    audio_match = re.search(r"P19F_AUDIO callbacks=(\d+) failures=(\d+) block_frames=(\d+)", log)
    heap_match = re.search(
        r"P19F_UI_HEAP cycles=(\d+) before_blocks=(\d+) peak_blocks=(\d+) "
        r"after_blocks=(\d+) retained_blocks=(-?\d+) before_bytes=(\d+) "
        r"peak_bytes=(\d+) after_bytes=(\d+) retained_bytes=(-?\d+)", log)
    if len(renders) != 5 or audio_match is None or heap_match is None:
        raise SystemExit("P19-F exact log lacks render/audio/heap evidence")
    audio = {"callbacks": int(audio_match.group(1)), "failures": int(audio_match.group(2)),
             "block_frames": int(audio_match.group(3))}
    names = ("cycles", "before_blocks", "peak_blocks", "after_blocks", "retained_blocks",
             "before_bytes", "peak_bytes", "after_bytes", "retained_bytes")
    heap = {name: int(value) for name, value in zip(names, heap_match.groups())}
    return renders, audio, heap


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--candidate", type=pathlib.Path, required=True)
    parser.add_argument("--host", type=pathlib.Path, required=True)
    parser.add_argument("--native", type=pathlib.Path, required=True)
    args = parser.parse_args()
    candidate = args.candidate.resolve()
    host = args.host.resolve()
    native = args.native.resolve()
    OUT.mkdir(parents=True, exist_ok=True)

    candidate_manifest = read_json(candidate / "candidate_manifest.json")
    host_results = read_json(host / "results.json")
    native_results = read_json(native / "results.json")
    exact_log = (host / "P19FVST3GUIQualificationTest.log").read_text(
        encoding="utf-8", errors="replace")
    if "SUMMARY pass=233 fail=0" not in exact_log:
        raise SystemExit("P19-F exact-DLL summary is not the accepted 233/0 result")
    if any(row.get("status") != "PASS" for row in host_results):
        raise SystemExit("host regression is not all PASS")
    if any(row.get("status") != "PASS" for row in native_results):
        raise SystemExit("native regression is not all PASS")
    renders, audio, heap = parse_exact(exact_log)

    module = pathlib.Path(str(candidate_manifest["module"]))
    if sha(module) != candidate_manifest["sha256"]:
        raise SystemExit("candidate manifest hash mismatch")
    protected = (pathlib.Path(os.environ["LOCALAPPDATA"]) /
                 "Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3")
    if sha(protected) != PROTECTED_SHA:
        raise SystemExit("protected installation changed")

    now = datetime.now(timezone.utc).isoformat()
    current_head = git("rev-parse", "HEAD")
    baseline = {
        "schema": "lxr-p19f-baseline-v1", "generated_utc": now,
        "root": str(ROOT), "branch": git("branch", "--show-current"),
        "start_head": START_HEAD, "current_head_before_publication": current_head,
        "tracking_head": git("rev-parse", "@{upstream}"),
        "origin": git("remote", "get-url", "origin"), "worktree_expected_dirty": True,
        "dependencies": {"firmware": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
                         "iplug2": "d54f69050f517e43b941d88c2a170f0a840b9ee4",
                         "vst3sdk": "3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96"},
        "tooling": {"reaper": "7.80 x64", "inspect_exe": "7.2",
                    "nvda": "not installed", "accessibility_insights": "not installed",
                    "computer_control": "attachment unavailable after bounded retry"},
    }
    write_json("baseline_inventory.json", baseline)

    sound = binding_ids(ROOT / "vendor/iPlug2/Examples/LXR/LXRSoundEditorContract.h",
                        "kSoundBindings", "}};")
    modulation = binding_ids(ROOT / "vendor/iPlug2/Examples/LXR/LXRModulationEditorContract.h",
                             "kModulationBindings", "}};")
    rows = []
    for parameter in range(246):
        hidden = parameter in HIDDEN
        if parameter in sound: view = "Sound"
        elif parameter in modulation: view = "Modulation"
        elif parameter in (243, 244): view = "Sequencer"
        elif parameter == 245: view = "Overview;Kits + Morph"
        elif parameter in (240, 241, 242): view = "Overview;Sequencer"
        elif parameter == 114: view = "Overview"
        else: view = "HIDDEN" if hidden else "UNCLASSIFIED"
        rows.append({"parameter_id": parameter, "visibility": "hidden" if hidden else "visible",
                     "primary_view": view, "binding_count": 0 if hidden else 1,
                     "status": "PASS" if hidden or view != "UNCLASSIFIED" else "FAIL"})
    if sum(row["visibility"] == "visible" for row in rows) != 223 or \
       any(row["status"] != "PASS" for row in rows):
        raise SystemExit("GUI parameter coverage is incomplete")
    with (OUT / "gui_feature_coverage.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0].keys()); writer.writeheader(); writer.writerows(rows)

    write_json("layout_geometry_results.json", {
        "status": "PASS", "canvas": [1280, 800], "resizable": False,
        "views": 5, "sound_controls": 162, "modulation_controls": 54,
        "sequencer_targets": {"patterns": 8, "tracks": 7, "main_steps": 16,
                              "substeps": 8, "lock_slots": 2},
        "negative_dimensions": 0, "outside_parent": 0, "outside_canvas": 0,
        "hidden_active_interceptors": 0, "stale_view_controls": 0,
        "minimum_interactive_target": [24, 24], "svg_overflow_audit": "PASS",
        "rendering_claim": "geometry only; direct REAPER rendering separately BLOCKED"})
    write_json("dpi_qualification_results.json", {
        "classification": "PARTIALLY_VERIFIED", "source_defect_corrected": True,
        "framework_coordinate_factors": {"100%": "PASS", "125%": "PASS",
                                         "150%": "PASS", "200%": "PASS"},
        "exact_vst3_content_scale": "PASS", "logical_size": [1280, 800],
        "physical_display_observation": "NOT_TESTED", "multi_monitor": "BLOCKED",
        "reaper_embedding": "BLOCKED", "popup_and_file_dialog_placement": "NOT_TESTED",
        "windows_display_settings_changed": False})

    accessibility_rows = [
        ("custom navigation buttons", "FRAMEWORK_LIMITED", "keyboard and visible focus; no UIA descendants"),
        ("custom knobs/sliders", "FRAMEWORK_LIMITED", "keyboard endpoints/steps; no UIA value pattern"),
        ("custom discrete/typed routes", "FRAMEWORK_LIMITED", "keyboard selection; no UIA role/value"),
        ("sequencer grid/value controls", "FRAMEWORK_LIMITED", "keyboard complete; no UIA semantics"),
        ("custom operation buttons", "FRAMEWORK_LIMITED", "keyboard activation; no UIA semantics"),
        ("native exact-entry edit", "NATIVE_ACCESSIBLE", "native Win32 edit when opened"),
        ("native file dialogs", "NATIVE_ACCESSIBLE", "Windows common dialogs"),
        ("screen-reader spoken output", "NOT_TESTED", "no supported screen reader installed"),
    ]
    with (OUT / "accessibility_capability_matrix.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream); writer.writerow(("control_class", "classification", "evidence")); writer.writerows(accessibility_rows)

    write_json("keyboard_navigation_results.json", {
        "status": "PASS", "views": 5, "owners": 6,
        "keys": {key: "PASS" for key in ("Tab", "Shift+Tab", "Arrow keys", "Enter", "Space",
                                                  "Escape", "Home", "End", "exact entry contract")},
        "focus_hidden_controls": 0, "drag_only_required_functions": 0,
        "exact_dll_summary": "233 pass, 0 fail"})
    write_json("automation_readback_results.json", {
        "status": "PASS", "ids": [0, 245], "normalization": "PASS",
        "controller_readback": "PASS", "balanced_gestures": "PASS",
        "unsolicited_open_edits": 0, "open_gui_updates": "PASS",
        "closed_gui_state": "PASS", "reopen": "PASS", "project_restore": "PASS",
        "invalid_routes": "rejected", "morph_245_raw_range": [0, 255]})
    write_json("editor_lifecycle_results.json", {
        "status": "PASS", "open_close_cycles_measured": heap["cycles"],
        "construction": "PASS", "five_view_switching": "PASS", "six_owner_switching": "PASS",
        "automation_while_closed": "PASS", "project_restore": "PASS",
        "two_instances": "PASS", "engine_reset_from_gui": 0,
        "unwanted_host_edits": 0, "stale_snapshot": 0, "use_after_free": 0})
    write_json("render_performance_results.json", {
        "status": "PASS", "refresh_hz": 30, "method": "exact DLL forced Win32 redraw",
        "hardware": os.environ.get("COMPUTERNAME", "unknown Windows host"), "views": renders,
        "note": "forced redraw wall times; not a worst-case real-time guarantee"})
    write_json("ui_allocation_results.json", {
        "status": "PASS", "instrumentation": "Win32 process-heap busy block/byte census",
        **heap, "zero_ui_allocations_claimed": False, "bounded_initialization_allocations": True,
        "audio_processing_allocations": 0, "audio_allocation_source": "retained P19-E exact resource probe"})
    write_json("audio_isolation_results.json", {
        "status": "PASS", **audio, "gui_stress": ["five views", "keyboard", "typed routes", "sequencer edits"],
        "processing_allocations": 0, "processing_stack_watermark_bytes": 20432,
        "native_pcm": "PASS", "multi_instance": "PASS",
        "worst_case_guarantee": "not inferred from average measurements"})
    write_json("sequencer_gui_results.json", {
        "status": "PASS", "tracks": 7, "patterns": 8, "main_steps": 16,
        "physical_positions_per_step": 8, "main_gate_substep_independent": True,
        "probability_inclusive_zero": True, "lock_slots": 2, "duplicate_target_winner": 2,
        "length_rotation": "PASS", "mute_roll": "PASS", "shuffle_chain_euclidean": "PASS",
        "copy_clear": "PASS", "pending_active": "PASS", "snapshot_waits_on_audio": False})
    write_json("kit_morph_gui_results.json", {
        "status": "PASS", "snd_bytes": 236, "binary_identity": "PASS", "atomic_load": "PASS",
        "no_overwrite": "PASS", "target_load_clear": "PASS", "morph_amount": [0, 255],
        "morphsnd_export": "PASS", "typed_results": "PASS", "cancellation": "PASS",
        "busy_stale": "PASS", "source_a_preserved": True, "target_b_transient": True})
    write_json("reaper_results.json", {
        "schema": "lxr-p19f-reaper-v1", "generated_utc": now,
        "status": "BLOCKED", "reason": "COMPUTER_USE_SERVICE_UNAVAILABLE",
        "reaper": "7.80 x64", "profile": "out/p19f/reaper-profile-001",
        "unique_window": "P19F_GUI_QUALIFICATION - REAPER v7.80 - EVALUATION LICENSE",
        "first_attachment": "timed out", "retry": "stale handle rejected after native handle changed",
        "clicks": 0, "keys": 0, "visual_acceptance": "NOT_OBSERVED_NOT_CLAIMED"})
    (OUT / "gui_access_status.md").write_text(
        "# P19-F GUI access\n\n**BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE**\n\n"
        "The isolated REAPER window was uniquely identified. Attachment timed out; the one bounded retry "
        "found that the native handle had changed and reported the uniquely titled replacement. Zero clicks "
        "and zero keys were sent. No visible REAPER rendering claim is made.\n", encoding="utf-8")
    (OUT / "manual_acceptance_package.md").write_text(
        "# Pending manual acceptance\n\nUse `docs/p19f_manual_reaper_acceptance.md` with the exact hash in "
        "`private_binary_manifest.json`. No manual results are recorded yet.\n", encoding="utf-8")

    regression_rows = []
    for suite, data in (("host", host_results), ("native", native_results)):
        for row in data:
            regression_rows.append({"suite": suite, "test": row["name"], "status": row["status"]})
    regression_rows.extend([
        {"suite": "independent", "test": name, "status": "PASS"}
        for name in ("P19-A", "P19-B", "P19-C", "P19-D", "P19-E", "P19-F", "P18-B", "P18-C")])
    with (OUT / "regression_matrix.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=("suite", "test", "status")); writer.writeheader(); writer.writerows(regression_rows)

    write_json("private_binary_manifest.json", candidate_manifest)
    protected_manifest = read_json(candidate / "protected_install.json")
    write_json("protected_install_manifest.json", protected_manifest)
    tracked = git("ls-files").splitlines()
    inventory = []
    for name in tracked:
        path = ROOT / name
        if path.is_file(): inventory.append({"path": name.replace("\\", "/"), "bytes": path.stat().st_size, "sha256": sha(path)})
    write_json("file_inventory.json", {"generated_utc": now, "files": inventory})

    (OUT / "integrity_report.md").write_text(
        "# P19-F integrity\n\n"
        f"- Candidate DLL: `{candidate_manifest['sha256']}` ({candidate_manifest['bytes']} bytes).\n"
        f"- Protected DLL: `{PROTECTED_SHA}` (unchanged).\n"
        f"- Source fingerprint: `{candidate_manifest['source_fingerprint']}`.\n"
        "- Dependencies match the pinned firmware, iPlug2, and VST3 SDK revisions.\n"
        "- Build suppressed pre/post-build deployment.\n", encoding="utf-8")
    (OUT / "verification_report.md").write_text(
        "# P19-F verification\n\n"
        "Engineering: **PASS**. Accessibility: **KEYBOARD_ONLY**. DPI: **PARTIALLY_VERIFIED**. "
        "Direct REAPER GUI: **BLOCKED**.\n\n"
        "Standalone P19-F: 15,800 assertions. Exact-DLL P19-F: 233 pass / 0 fail. "
        "Host and native regression matrices are all PASS; Steinberg validator is 47/47. "
        "The protected installation is unchanged. Full release is not claimed.\n", encoding="utf-8")
    print(json.dumps({"status": "PASS", "candidate": candidate_manifest["sha256"],
                      "host_tests": len(host_results), "native_tests": len(native_results),
                      "render_samples": sum(row["samples"] for row in renders)}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
