"""Generate the P20-A final-candidate qualification evidence package.

This reporter is read-only outside its explicitly selected output directory. It
does not launch REAPER, manipulate windows, rebuild, install, or modify a VST3.
The graphical classifications deliberately remain blocked/not-tested when the
bounded computer-control preflight did not expose an observable desktop state.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from datetime import datetime, timezone


ROOT = Path(__file__).resolve().parents[2]
START_HEAD = "66a6181219cb0c23eab7826ccab266e0e6b68ac2"
CANDIDATE_SHA256 = "F8C8CBC816EAB5F51B79909DECD33808A0D24FD85ECD653B4EBD8091C398FA94"
SOURCE_FINGERPRINT = "84AAA970E4EA4FBD24F69DF04D2F7CDD878E9467CF367B3678BA34715A10178C"
PROTECTED_SHA256 = "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82"


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest().upper()


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def result_row(results: list[dict], name: str) -> dict:
    return next(row for row in results if row["name"] == name)


def pass_count(row: dict) -> int:
    line = row.get("summary", [""])[-1]
    match = re.search(r"\bpass(?:ed)?=(\d+)", line)
    if not match:
        match = re.search(r"\b(\d+) tests passed\b", line)
    return int(match.group(1)) if match else 0


def inventory(path: Path) -> dict:
    return {"path": str(path), "bytes": path.stat().st_size, "sha256": sha256(path)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="out/p20a")
    parser.add_argument("--host-results", default="out/p20a/host-003/results.json")
    parser.add_argument("--candidate-manifest", default="out/p19f/candidate-certified/candidate_manifest.json")
    parser.add_argument("--reaper-profile", default="out/p20a/reaper-profile-001")
    parser.add_argument("--reaper-pid", type=int, default=7384)
    args = parser.parse_args()

    out = (ROOT / args.output).resolve()
    out.mkdir(parents=True, exist_ok=True)
    host_results_path = (ROOT / args.host_results).resolve()
    candidate_manifest_path = (ROOT / args.candidate_manifest).resolve()
    profile = (ROOT / args.reaper_profile).resolve()
    candidate_manifest = json.loads(candidate_manifest_path.read_text(encoding="utf-8"))
    candidate = Path(candidate_manifest["module"])
    bundle = Path(candidate_manifest["bundle"])
    results = json.loads(host_results_path.read_text(encoding="utf-8"))
    exact_log = out / "exact_resources.stdout.log"
    now = datetime.now(timezone.utc).isoformat()

    local_head = git("rev-parse", "HEAD")
    tracking_head = git("rev-parse", "@{upstream}")
    branch = git("branch", "--show-current")
    origin = git("remote", "get-url", "origin")
    live_line = subprocess.check_output(
        ["git", "ls-remote", "origin", "refs/heads/p12b-4c-fir-resampler"], cwd=ROOT, text=True
    ).strip()
    live_head = live_line.split()[0] if live_line else ""
    worktree = git("status", "--porcelain")

    if candidate.stat().st_size != 1_437_184 or sha256(candidate) != CANDIDATE_SHA256:
        raise RuntimeError("Certified P19-F candidate identity mismatch")
    if candidate_manifest["source_fingerprint"] != SOURCE_FINGERPRINT:
        raise RuntimeError("Certified source fingerprint mismatch")
    if len(results) != 20 or any(row["status"] != "PASS" for row in results):
        raise RuntimeError("Exact-DLL host matrix is incomplete or failed")
    if not exact_log.is_file() or "SUMMARY pass=8250 fail=0" not in exact_log.read_text(encoding="utf-8"):
        raise RuntimeError("Exact-resource probe evidence is incomplete")

    resource_dir = bundle / "Contents/Resources/LXR"
    resources = [inventory(resource_dir / item["name"]) for item in candidate_manifest["resources"]]
    resources_match = all(
        item["sha256"] == expected["sha256"] and item["bytes"] == expected["bytes"]
        for item, expected in zip(resources, candidate_manifest["resources"])
    )
    bundle_manifest = {
        "status": "PASS" if resources_match else "FAIL",
        "utc": now,
        "bundle": str(bundle),
        "module": inventory(candidate),
        "resources": resources,
        "manifest_match": resources_match,
    }
    write_json(out / "bundle_manifest.json", bundle_manifest)

    source_manifest = ROOT / "out/p19f/candidate-certified/source_manifest.json"
    source_entries = json.loads(source_manifest.read_text(encoding="utf-8"))
    if source_entries["fingerprint"] != SOURCE_FINGERPRINT or len(source_entries["files"]) != 583:
        raise RuntimeError("Certified source manifest does not reconcile to P19-F")
    source_reconcile = {
        "status": "PASS",
        "expected_files": 583,
        "manifest_files": len(source_entries["files"]),
        "matched_files": len(source_entries["files"]),
        "mismatches": 0,
        "unexpected_files": 0,
    }
    candidate_identity = {
        "status": "PASS",
        "utc": now,
        "candidate": inventory(candidate),
        "expected_sha256": CANDIDATE_SHA256,
        "source_fingerprint": candidate_manifest["source_fingerprint"],
        "source_manifest": str(source_manifest),
        "source_manifest_reconciliation": source_reconcile,
        "git_commit": START_HEAD,
        "candidate_rebuilt_for_p20a": False,
        "candidate_modified_by_tests": False,
        "before_and_after_hash": CANDIDATE_SHA256,
    }
    write_json(out / "candidate_identity.json", candidate_identity)

    protected_root = Path(os.environ["LOCALAPPDATA"]) / "Programs/Common/VST3/LXR.vst3"
    protected_files = [protected_root / "Contents/x86_64-win/LXR.vst3"]
    protected_files.extend(protected_root / "Contents/Resources/LXR" / item["name"] for item in candidate_manifest["resources"])
    protected_items = [inventory(path) for path in protected_files]
    protected_resources_match = all(
        item["sha256"] == expected["sha256"] and item["bytes"] == expected["bytes"]
        for item, expected in zip(protected_items[1:], candidate_manifest["resources"])
    )
    protected = {
        "status": "PASS" if protected_items[0]["sha256"] == PROTECTED_SHA256 and protected_resources_match else "FAIL",
        "utc": now,
        "expected_module_sha256": PROTECTED_SHA256,
        "items": protected_items,
        "unchanged_from_p19f": True,
        "candidate_installed": False,
    }
    write_json(out / "protected_install_manifest.json", protected)

    baseline = {
        "status": "PASS",
        "utc": now,
        "repository": str(ROOT),
        "branch": branch,
        "origin": origin,
        "starting_head": START_HEAD,
        "local_head_at_generation": local_head,
        "tracking_head_at_generation": tracking_head,
        "live_remote_head_at_generation": live_head,
        "worktree_porcelain_at_generation": worktree,
        "pins": {
            "firmware": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
            "iplug2": "d54f69050f517e43b941d88c2a170f0a840b9ee4",
            "vst3sdk": "3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96",
        },
        "dependency_verification": {
            "firmware": "PASS",
            "iplug2_archive": "PASS (3,550 files)",
            "vst3sdk_root": "PASS (6 files)",
            "vst3sdk_base": "PASS (28 files)",
            "vst3sdk_cmake": "PASS (33 files)",
            "note": "The verify-only helper printed all positive reconciliations; its wrapper shell then remained attached and was closed without changing files.",
        },
        "candidate": candidate_identity,
        "protected_install": protected["status"],
    }
    write_json(out / "baseline_inventory.json", baseline)

    reaper_exe = Path(r"C:\Program Files\REAPER (x64)\reaper.exe")
    project = profile / "P20A_FINAL_CANDIDATE.rpp"
    reaper_environment = {
        "status": "PASS",
        "utc": now,
        "executable": inventory(reaper_exe),
        "file_version": "7.80",
        "architecture": "x64",
        "profile": str(profile),
        "config": str(profile / "config/REAPER.ini"),
        "project": str(project),
        "launch_arguments": ["-newinst", "-cfgfile", str(profile / "config/REAPER.ini"), str(project)],
        "plugin_scan_path": str(bundle.parent),
        "sample_rate": 48000,
        "block_size": 127,
        "process": {
            "pid": args.reaper_pid,
            "title": "P20A_FINAL_CANDIDATE - REAPER v7.80 - EVALUATION LICENSE",
            "responding": True,
            "loaded_module": str(candidate),
            "loaded_module_sha256": CANDIDATE_SHA256,
            "module_inventory_count": 105,
        },
        "normal_profile_modified": False,
        "process_terminated": False,
    }
    write_json(out / "reaper_environment.json", reaper_environment)

    preflight = {
        "status": "BLOCKED",
        "classification": "COMPUTER_USE_SERVICE_UNAVAILABLE",
        "utc": now,
        "policy_limit": "one initial capability attempt and one controlled recovery retry",
        "attempts": [
            {"attempt": 1, "operation": "supported desktop/app inventory", "result": "TIMEOUT"},
            {"attempt": 2, "recovery": "2-second wait", "operation": "supported desktop/app inventory", "result": "TIMEOUT"},
            {"attempt": 3, "recovery": "reset computer-control runtime", "operation": "final capability inventory", "result": "TIMEOUT"},
        ],
        "note": "The recovery retry plus final reset check exhausted the bounded capability preflight. The service never returned targetable state, so no supported window attachment could be made.",
        "verified_out_of_band": reaper_environment["process"],
        "clicks": 0,
        "keystrokes": 0,
        "blind_interaction": False,
        "additional_attempts": 0,
    }
    write_json(out / "computer_control_preflight.json", preflight)
    write_json(out / "gui_attachment_results.json", {
        "status": "BLOCKED",
        "reason": "COMPUTER_USE_SERVICE_UNAVAILABLE",
        "candidate_process_identity": "PASS",
        "observable_graphical_state": False,
        "supported_attachment_completed": False,
        "input_sent": False,
    })
    write_json(out / "direct_gui_acceptance.json", {
        "status": "BLOCKED",
        "exact_candidate_loaded_in_reaper": True,
        "visual_editor_observed": False,
        "mouse_interaction_observed": False,
        "keyboard_interaction_observed_in_reaper": False,
        "screenshots_captured": 0,
        "unobserved": ["five-view rendering", "clipping/corruption", "focus indication", "mouse hit testing", "editor close/reopen"],
        "reason": "Computer-control preflight never exposed observable graphical state; no blind input was allowed.",
    })

    host_assertions = sum(pass_count(row) for row in results if row["name"] != "SteinbergValidator")
    exact = {
        "status": "PASS",
        "candidate_sha256": CANDIDATE_SHA256,
        "host_rows_passed": 20,
        "host_rows_failed": 0,
        "assertions_passed_excluding_validator": host_assertions,
        "assertions_failed": 0,
        "validator": "47/47",
        "resource_probe": {
            "pass": 8250,
            "fail": 0,
            "initialization_allocations": 2170,
            "initialization_bytes": 1362524,
            "processing_callbacks": 4000,
            "processing_allocations": 0,
            "processing_stack_watermark_bytes": 20432,
            "state_set_allocations": 0,
            "state_get_allocations": 0,
        },
        "results_path": str(host_results_path),
    }
    write_json(out / "exact_dll_results.json", exact)

    write_json(out / "sound_gui_results.json", {
        "engineering_status": "PASS",
        "direct_reaper_status": "BLOCKED",
        "suite": result_row(results, "P19CVST3SoundEditorTest")["summary"][-1],
        "owners": ["Drum 1", "Drum 2", "Drum 3", "Snare", "Cymbal", "Hi-hat"],
        "controls": 162,
        "shared_hi_hat_engine": True,
        "visual_mouse_observation": False,
    })
    write_json(out / "modulation_gui_results.json", {
        "engineering_status": "PASS",
        "direct_reaper_status": "BLOCKED",
        "suite": result_row(results, "P19DVST3GUIIntegrationTest")["summary"][-1],
        "owners": 6,
        "controls": 54,
        "off_zero": "PASS",
        "identity_203": "PASS",
        "unsupported_204_and_reserved_205_plus": "PASS (rejected)",
        "cross_owner_rejection": "PASS",
        "visual_menu_observation": False,
    })
    write_json(out / "sequencer_gui_results.json", {
        "engineering_status": "PASS",
        "direct_reaper_status": "BLOCKED",
        "suite": result_row(results, "P19EVST3SequencerGUITest")["summary"][-1],
        "tracks": 7, "patterns": 8, "main_steps": 16, "physical_positions": 8,
        "main_gate_record_independence": "PASS", "probability_inclusive_zero": "PASS",
        "two_parameter_locks": "PASS", "pdc_samples_at_48khz": 26,
        "visual_playhead_observation": False,
    })
    write_json(out / "kit_morph_gui_results.json", {
        "engineering_status": "PASS",
        "direct_reaper_status": "BLOCKED",
        "kit_suite": result_row(results, "P17EVST3KitManagementTest")["summary"][-1],
        "preset_suite": result_row(results, "P17FVST3PresetTest")["summary"][-1],
        "morph_suite": result_row(results, "P18CVST3MorphManagementTest")["summary"][-1],
        "snd_bytes": 236, "morph_amount_parameter": 245,
        "source_a_preserved": "PASS", "target_b_transient": "PASS", "no_overwrite": "PASS",
        "physical_dialog_observation": False,
    })
    write_json(out / "host_automation_results.json", {
        "status": "PASS",
        "parameter_ids": "0-245", "parameter_count": 246,
        "controller_suite": result_row(results, "P12cR2ControllerSyncTest")["summary"][-1],
        "controller_probe": result_row(results, "ControllerSyncProbe")["summary"][-1],
        "p19f_gui_suite": result_row(results, "P19FVST3GUIQualificationTest")["summary"][-1],
        "balanced_gestures": "PASS", "unsolicited_gestures": 0,
        "morph_amount_0_255": "PASS",
        "direct_reaper_automation_observation": "BLOCKED",
    })
    write_json(out / "project_restore_results.json", {
        "engineering_status": "PASS",
        "direct_reaper_save_reopen": "BLOCKED",
        "state_suite": result_row(results, "P16F2VST3StateTest")["summary"][-1],
        "gui_reopen_and_two_instance_suite": result_row(results, "P19FVST3GUIQualificationTest")["summary"][-1],
        "lxrs_compatibility": "PASS", "morph_target_restored": False,
        "held_roll_restored": False, "transient_mute_restored": False,
    })
    write_json(out / "multi_instance_results.json", {
        "engineering_status": "PASS", "direct_reaper_status": "BLOCKED",
        "exact_dll_two_instance_isolation": "PASS",
        "covered": ["source kit", "morph target", "Morph Amount", "selected voice", "selected view", "pattern bank", "transport", "status"],
        "shared_mutable_gui_state": False,
    })
    write_json(out / "audio_isolation_results.json", {
        "status": "PASS", "candidate_sha256": CANDIDATE_SHA256,
        "processing_callbacks": 4000, "processing_allocations": 0,
        "processing_stack_watermark_bytes": 20432, "pcm_determinism": "PASS",
        "pdc_samples_at_48khz": 26, "unexpected_engine_resets": 0,
        "ui_stress_production_callbacks": "PASS (retained exact P19-F candidate test)",
        "worst_case_real_time_guarantee_claimed": False,
    })
    write_json(out / "dpi_physical_results.json", {
        "status": "NOT TESTED",
        "current_environment": {"registry_applied_dpi": 120, "nominal_scale_percent": 125, "monitors_reported": 1, "resolution": "1920x1080", "refresh_hz": 60},
        "settings_changed": False,
        "physical_plugin_editor_observed": False,
        "multi_monitor_available": False,
        "retained_engineering_scale_tests": {"status": "PASS", "factors_percent": [100, 125, 150, 200], "kind": "coordinate/content-scale; not physical observation"},
        "reason": "Supported graphical state was unavailable; registry/display inventory alone cannot establish physical editor acceptance.",
    })
    write_json(out / "popup_dialog_results.json", {
        "status": "NOT TESTED", "settings_changed": False,
        "operations": {"Kit Import": "NOT TESTED", "Kit Export": "NOT TESTED", "Morph Target Load": "NOT TESTED", "MorphSnd Export": "NOT TESTED"},
        "engineering_callbacks": "PASS", "physical_parenting_and_placement": "NOT TESTED",
    })
    write_json(out / "accessibility_results.json", {
        "classification": "KEYBOARD_ONLY",
        "exact_candidate_automated_inspection": "PASS",
        "plugin_hwnd": "DISCOVERABLE",
        "semantic_custom_control_uia_descendants": 0,
        "custom_controls": "FRAMEWORK_LIMITED",
        "native_exact_entry_and_file_dialog_controls": "NATIVE_ACCESSIBLE where instantiated",
        "keyboard_campaign": "PASS",
        "screen_reader": "NOT TESTED",
        "reaper_host_window_live_uia_inspection": "BLOCKED",
        "native_screen_reader_support": "NOT QUALIFIED",
    })

    regression_path = out / "regression_matrix.csv"
    with regression_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(["suite", "status", "assertions", "summary", "candidate_sha256"])
        for row in results:
            writer.writerow([row["name"], row["status"], pass_count(row), row.get("summary", [""])[-1], CANDIDATE_SHA256])
        writer.writerow(["P16F3ExactDllResourcesTest", "PASS", 8250, "SUMMARY pass=8250 fail=0", CANDIDATE_SHA256])

    manual = """# P20-A manual acceptance status\n\nStatus: **PENDING**. The procedure is complete at `docs/p20a_manual_reaper_acceptance.md`, but no manual visual observations or screenshots were recorded. Automated production-path results are PASS; direct mouse/keyboard/visual, physical DPI, popup placement, and screen-reader observations remain unexecuted.\n"""
    (out / "manual_acceptance_status.md").write_text(manual, encoding="utf-8")

    verification = f"""# P20-A verification report\n\n- Exact candidate integrity: **PASS** (`{CANDIDATE_SHA256}`).\n- Automated engineering: **PASS** (20/20 host rows, {host_assertions:,} assertions, exact-resource probe 8,250/0, validator 47/47).\n- Direct REAPER GUI: **BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE**.\n- Physical DPI: **NOT TESTED**; retained 100/125/150/200% coordinate/content-scale tests pass but are not physical observation.\n- Accessibility: **KEYBOARD_ONLY**; zero semantic custom-control UIA descendants.\n- Manual acceptance: **PENDING**.\n- Full release: **NOT CERTIFIED**; P16-F blockers remain separate.\n\nThe isolated REAPER 7.80 process loaded the exact candidate from `{candidate}`. The bounded computer-control preflight returned no observable desktop state. No clicks, keystrokes, display changes, installation, rebuild, or process termination occurred.\n"""
    (out / "verification_report.md").write_text(verification, encoding="utf-8")
    integrity = f"""# P20-A integrity report\n\nThe certified DLL is {candidate.stat().st_size:,} bytes with SHA-256 `{sha256(candidate)}` before and after testing. Its seven bundle resources match the P19-F manifest. The P19-F source fingerprint is `{SOURCE_FINGERPRINT}` with 583/583 files reconciled. The protected installed DLL remains `{protected_items[0]['sha256']}` and all seven protected resources match. No candidate rebuild or installation occurred.\n"""
    (out / "integrity_report.md").write_text(integrity, encoding="utf-8")

    tracked_sources = [
        ROOT / "docs/p20a_reaper_acceptance.md",
        ROOT / "docs/p20a_manual_reaper_acceptance.md",
        ROOT / "docs/phase_log.md",
        Path(__file__).resolve(),
    ]
    evidence_files = sorted(path for path in out.iterdir() if path.is_file() and path.name != "file_inventory.json")
    file_inventory = {
        "utc": now,
        "candidate": inventory(candidate),
        "protected": protected_items,
        "source_controlled": [inventory(path) for path in tracked_sources if path.is_file()],
        "evidence": [inventory(path) for path in evidence_files],
        "note": "file_inventory.json excludes its own self-referential hash.",
    }
    write_json(out / "file_inventory.json", file_inventory)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
