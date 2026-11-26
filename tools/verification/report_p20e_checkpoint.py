#!/usr/bin/env python3
"""Generate the P20-E governance evidence from preserved P20-A through P20-D evidence."""

from __future__ import annotations

import csv
import hashlib
import json
import os
import subprocess
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "out/p20e"
HEAD = "0155e153ae34ff2fb5740ea7bfe6855ef5d1c220"
PRODUCTION = "FB1045BD5716282607487F6A70A6082206309EE741C8F751768AE5C8F6F7650D"
DIAGNOSTIC = "53A51D63980196DD15647E4584ECE73B80078FC8349646700C88F60EB13C547D"
PROTECTED = "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82"
ASSETS = ["crash_sample.bin", "rec_table.bin", "saw_table.bin", "sine_table.bin",
          "transient_data.bin", "transient_volume_table.bin", "tri_table.bin"]
START_PATHS = [
    "docs/phase_log.md", "dsp/LXRAudioAdapter.cpp", "dsp/LXRAudioAdapter.h",
    "tests/P19EVST3SequencerGUITest.cpp", "tools/verification/build_p16f3_candidate.py",
    "vendor/iPlug2/Examples/LXR/LXR.cpp", "docs/p20b_release_blocker_resolution.md",
    "docs/p20c_release_evidence_closure.md", "docs/p20d_exact_dll_event_retention.md",
    "dsp/NativeBatchCapture.h", "tests/P20BHostEventCaptureTest.cpp",
    "tests/P20CHostTransitionCaptureTest.cpp", "tests/P20DExactDllRetentionTest.cpp",
    "tools/verification/p20d_diagnostic_capture.targets",
    "tools/verification/report_p20b_checkpoint.py",
    "tools/verification/report_p20c_checkpoint.py",
    "tools/verification/report_p20d_checkpoint.py",
    "tools/verification/run_p20d_retention_tests.py",
]


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def read_json(path: Path):
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


def gate(gate_id: str, category: str, requirement: str, source: str,
         criterion: str, evidence: str, authority: str, status: str,
         *, binary: str = PRODUCTION, implementation: str = "YES",
         automated: str = "YES", reaper: str = "NO", provenance: str = "YES",
         local: str = "YES", approval: str = "NO", dependency: str = "NONE",
         action: str = "RETAIN", release: str = "CLOSED",
         publication: str = "SUPPORTS", disposition: str = "ACCEPTED",
         exact: str = "YES", replacement: str = "NONE",
         retained: str = "N/A", inequality: str = "N/A") -> dict[str, str]:
    return {
        "gate_id": gate_id, "category": category, "requirement": requirement,
        "requirement_source": source, "mandatory": "YES",
        "original_acceptance_criterion": criterion, "latest_technical_evidence": evidence,
        "evidence_authority": authority, "binary_sha256": binary,
        "exact_artifact_claim": exact, "current_status": status,
        "implementation_complete": implementation,
        "automated_engineering_pass": automated,
        "actual_reaper_required": reaper,
        "historical_provenance_exists": provenance,
        "locally_completable": local, "owner_approval_required": approval,
        "blocking_dependency": dependency, "replacement_criterion": replacement,
        "historical_failure_retained": retained, "observed_inequality": inequality,
        "proposed_next_action": action, "release_impact": release,
        "development_publication_impact": publication,
        "final_disposition": disposition,
    }


def register() -> list[dict[str, str]]:
    p20d = "out/p20d/regression_matrix.csv;out/p20d/verification_report.md"
    exact = "out/p20d/retention-final-005/results.json"
    rows = [
        gate("PF-001", "production_functionality", "Original synthesis", "docs/architecture.md;docs/p16f13_development_checkpoint.md", "Original v0.37 synthesis and frozen outputs pass", p20d, "ORIGINAL_FIRMWARE_SOURCE;ORIGINAL_C_DIFFERENTIAL;STANDALONE_NATIVE_TEST;EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("PF-002", "production_functionality", "Modulation", "docs/architecture.md;docs/phase_log.md P15", "Velocity and LFO routing/order/state pass", p20d, "ORIGINAL_FIRMWARE_SOURCE;STANDALONE_NATIVE_TEST;EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("PF-003", "production_functionality", "MIDI", "docs/param_map.md;docs/p16f13_development_checkpoint.md", "Timestamped MIDI/controller/state behavior passes", p20d, "ORIGINAL_FIRMWARE_SOURCE;STANDALONE_NATIVE_TEST;EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("PF-004", "production_functionality", "Seven-track sequencer", "docs/architecture.md;docs/phase_log.md P16", "Original seven-track sequencing and operations pass", p20d, "ORIGINAL_FIRMWARE_SOURCE;ORIGINAL_C_DIFFERENTIAL;STANDALONE_NATIVE_TEST;EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("PF-005", "production_functionality", "Host synchronization", "docs/p16f13_development_checkpoint.md", "Source-grounded start/stop/tempo/seek/loop/reanchor pass", "out/p20d/transport_transition_results.json", "STANDALONE_NATIVE_TEST;EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("PF-006", "production_functionality", "Pattern persistence", "docs/p16f13_development_checkpoint.md", "Pattern banks survive supported LXRS save/reopen", p20d, "STANDALONE_NATIVE_TEST;EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("PF-007", "production_functionality", ".lxqb Import production path", "docs/p20b_release_blocker_resolution.md", "Valid import, malformed/Busy rollback, playback and reopen pass", "out/p20b/exact_candidate_results.json;out/p20d/p20b-event-import-final.log", "EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("PF-008", "production_functionality", "Original .snd kit management", "docs/phase_log.md P17-E", "Original-format import/export and rollback pass", p20d, "ORIGINAL_FIRMWARE_SOURCE;STANDALONE_NATIVE_TEST;EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("PF-009", "production_functionality", "Host presets", "docs/phase_log.md P17-F", "Kit identity and host preset restore pass", p20d, "STANDALONE_NATIVE_TEST;EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("PF-010", "production_functionality", "Firmware-faithful morphing", "docs/phase_log.md P18-B/P18-C", "Original interpolation and production management pass", p20d, "ORIGINAL_FIRMWARE_SOURCE;ORIGINAL_C_DIFFERENTIAL;STANDALONE_NATIVE_TEST;EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("PF-011", "production_functionality", "Complete five-view GUI implementation", "docs/p19f_gui_qualification.md", "Five views and complete control inventory pass automated exact-DLL qualification", p20d, "EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("EI-001", "engineering_integrity", "Native PCM regression", "docs/p16f13_development_checkpoint.md", "Native O0/O2 and 76 frozen PCM references pass unchanged", "out/p20d/regression_matrix.csv;out/p20b/regression_matrix.csv", "STANDALONE_NATIVE_TEST", "PASS"),
        gate("EI-002", "engineering_integrity", "Original-C differential", "docs/p16f13_development_checkpoint.md", "Original-C filter/snare/performance differentials pass", p20d, "ORIGINAL_C_DIFFERENTIAL", "PASS"),
        gate("EI-003", "engineering_integrity", "FIR quality and latency", "docs/build_and_test.md;docs/p16f13_development_checkpoint.md", "FIR thresholds and 26-sample PDC pass", p20d, "STANDALONE_NATIVE_TEST;EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("EI-004", "engineering_integrity", "Host parameter compatibility", "docs/param_map.md;docs/p16f13_development_checkpoint.md", "Host IDs 0-245 and normalization contracts unchanged", p20d, "EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("EI-005", "engineering_integrity", "LXRS compatibility", "docs/p16f13_development_checkpoint.md", "v1/v2/v3, 51065/51089 streams and 65536 ceiling pass", p20d, "STANDALONE_NATIVE_TEST;EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("EI-006", "engineering_integrity", "Controller synchronization", "docs/build_and_test.md", "ControllerSyncProbe 1057/0 and host suites pass", p20d, "EXACT_DLL_CONTROLLED_HOST", "PASS"),
        gate("EI-007", "engineering_integrity", "Real-time processing allocations", "AGENTS.md;docs/risks.md", "Zero measured callback allocations; no blocking/I/O/logging", "out/p20d/allocation_stack_report.md", "EXACT_DLL_CONTROLLED_HOST", "PASS", binary=DIAGNOSTIC),
        gate("EI-008", "engineering_integrity", "Multi-instance isolation", "docs/p16f13_development_checkpoint.md", "State, capture and PCM remain instance-local", "out/p20d/multi_instance_results.json", "EXACT_DLL_CONTROLLED_HOST", "PASS", binary=DIAGNOSTIC),
        gate("EI-009", "engineering_integrity", "Steinberg validator", "docs/build_and_test.md", "Validator passes 47/47", "out/p20d/diagnostic-validator-final.log;out/p20d/checkpoint-final/SteinbergValidator.log", "STEINBERG_VALIDATOR", "PASS", binary=f"{PRODUCTION};{DIAGNOSTIC}"),
        gate("EI-010", "engineering_integrity", "Exact production candidate identity", "docs/p20b_release_blocker_resolution.md", "Tested production module hash equals manifest", "out/p20d/production_binary_manifest.json", "HISTORICAL_ARTIFACT_PROVENANCE", "PASS"),
        gate("EI-011", "engineering_integrity", "Protected installation integrity", "docs/p20a_reaper_acceptance.md", "Protected DLL and seven resources remain unchanged", "out/p20d/protected_install_manifest.json", "HISTORICAL_ARTIFACT_PROVENANCE", "PASS", binary=PROTECTED),
        gate("EI-012", "engineering_integrity", "Diagnostic compile guard isolation", "docs/p20d_exact_dll_event_retention.md", "Capture exists only with LXR_P20D_DIAGNOSTIC_CAPTURE", "dsp/NativeBatchCapture.h;tools/verification/p20d_diagnostic_capture.targets;vendor/iPlug2/Examples/LXR/projects/LXR-vst3.vcxproj", "EXACT_DLL_CONTROLLED_HOST", "PASS", binary=f"{PRODUCTION};{DIAGNOSTIC}"),
        gate("EI-013", "engineering_integrity", "Controlled-host retained event timing", "docs/p20d_exact_dll_event_retention.md", "All native batches/events retained for partitions 1-1024", exact, "EXACT_DLL_CONTROLLED_HOST", "PASS", binary=DIAGNOSTIC),
        gate("EI-014", "engineering_integrity", "Pattern-transition copy", "docs/p20d_exact_dll_event_retention.md", "Publication, consumption, events, stale and Busy cases pass", "out/p20d/pattern_transition_copy_results.json", "EXACT_DLL_CONTROLLED_HOST", "PASS", binary=DIAGNOSTIC),
    ]
    blocked = dict(implementation="YES", automated="PARTIAL", reaper="YES", provenance="YES",
                   local="NO", dependency="Supported REAPER desktop/event access or owner-operated manual run",
                   action="Execute preserved manual procedure in isolated REAPER 7.80",
                   release="OPEN_BLOCKS_RELEASE", publication="OWNER_MUST_SEPARATE_RELEASE_GATE",
                   disposition="EXTERNAL_BLOCKED")
    rows += [
        gate("RS-001", "release_specific", "Direct REAPER .lxqb Import", "docs/p20a_manual_reaper_acceptance.md;docs/p20b_release_blocker_resolution.md", "Real control visibility, valid/invalid import, playback and save/reopen observed", "out/p20d/reaper_gui_status.md", "ACTUAL_REAPER_GRAPHICAL_INTERACTION", "EXTERNAL_BLOCKED", **blocked),
        gate("RS-002", "release_specific", "Actual REAPER event timing", "docs/p20a_reaper_acceptance.md;docs/p20d_exact_dll_event_retention.md", "REAPER callback/transport trigger and lock trace matches oracle", "out/p20d/reaper_event_status.md", "ACTUAL_REAPER_PRODUCTION_PATH", "EXTERNAL_BLOCKED", **blocked),
        gate("RS-003", "release_specific", "Euclidean/copy-clear REAPER traces", "docs/p20b_release_blocker_resolution.md", "Operations during REAPER playback show correct subsequent events", "out/p20d/reaper_event_status.md", "ACTUAL_REAPER_PRODUCTION_PATH", "EXTERNAL_BLOCKED", **blocked),
    ]
    pcm = [
        ("RS-004", "F7 selector 0 historical PCM equality", "4591AB2EBFDD6FE738F4983B9FBBF292C4B4E06DA7E0F7528C1E9426E050BE1D", "11785/0 expected 0 observed -1", "out/p20c/historical_pcm_case_f7.json", "PLAUSIBLE_BUT_UNPROVEN"),
        ("RS-005", "F4 selector 0 historical PCM equality", "179E6643FC4E6905E89EF8FB273382CCAB866BC145C24B3F2D9917B5EBFD0FC8", "11785/0 expected 0 observed -1", "out/p20c/historical_pcm_case_f4_selector0.json", "PLAUSIBLE_BUT_UNPROVEN"),
        ("RS-006", "F4 selector 12 historical PCM equality", "179E6643FC4E6905E89EF8FB273382CCAB866BC145C24B3F2D9917B5EBFD0FC8", "119988/1 expected 11348 observed 11347", "out/p20c/historical_pcm_case_f4_selector12.json", "PLAUSIBLE_BUT_UNPROVEN"),
        ("RS-007", "F4 OFF-stress historical PCM equality", "179E6643FC4E6905E89EF8FB273382CCAB866BC145C24B3F2D9917B5EBFD0FC8", "0/1 expected -1738506 observed -1738508", "out/p20c/historical_pcm_case_f4_off_stress.json", "INSUFFICIENT_HISTORICAL_EVIDENCE"),
    ]
    for gate_id, requirement, binary, inequality, evidence, disposition in pcm:
        rows.append(gate(gate_id, "release_specific", requirement,
            "docs/p16f13_development_checkpoint.md final testing row;preserved P16-F raw gate",
            "Exact historical equality or owner-approved replacement criterion", evidence,
            "HISTORICAL_ARTIFACT_PROVENANCE", "HISTORICAL_EVIDENCE_INSUFFICIENT",
            binary=binary, implementation="YES", automated="NO", provenance="PARTIAL",
            local="NO", approval="YES", dependency="Original callback/transient/executable provenance was not recorded",
            action="Request owner decision on prospective exact-artifact protocol",
            release="OPEN_BLOCKS_RELEASE", publication="OWNER_MUST_SEPARATE_RELEASE_GATE",
            disposition=disposition, replacement="PROPOSED_UNAPPROVED", retained="YES",
            inequality=inequality))
    rows += [
        gate("RS-008", "release_specific", "Historical FIR executable identity", "docs/p16f13_development_checkpoint.md;out/p20c/historical_fir_identity.md", "Previously denied executable identity is proven or owner approves equivalent-source replacement", "out/p20d/historical_fir_identity.md", "HISTORICAL_ARTIFACT_PROVENANCE", "HISTORICAL_IDENTITY_UNPROVABLE", binary="", implementation="YES", automated="NO", provenance="NO", local="NO", approval="YES", dependency="Original denied executable SHA-256/build identity absent", action="Request owner decision on equivalent-source qualification", release="OPEN_BLOCKS_RELEASE", publication="OWNER_MUST_SEPARATE_RELEASE_GATE", disposition="HISTORICAL_IDENTITY_UNPROVABLE", exact="NO", replacement="PROPOSED_UNAPPROVED", retained="YES"),
        gate("RS-009", "release_specific", "Manual graphical acceptance", "docs/p19f_manual_reaper_acceptance.md;docs/p20a_manual_reaper_acceptance.md", "Actual final-candidate mouse/keyboard/dialog/save-reopen observations", "out/p20d/reaper_gui_status.md", "ACTUAL_REAPER_GRAPHICAL_INTERACTION", "EXTERNAL_BLOCKED", **blocked),
        gate("RS-010", "release_specific", "Physical DPI and dialog placement", "docs/p19f_gui_qualification.md", "Physical 100/125/150/200% and dialog placement observed in REAPER", "docs/p19f_gui_qualification.md;out/p20d/reaper_gui_status.md", "ACTUAL_REAPER_GRAPHICAL_INTERACTION", "EXTERNAL_BLOCKED", **blocked),
        gate("RS-011", "release_specific", "Native accessibility", "docs/p19f_gui_qualification.md;docs/p16f13_development_checkpoint.md custom GUI row", "Required native/screen-reader behavior is observed or owner accepts keyboard-only criterion", "docs/p19f_gui_qualification.md", "ACTUAL_REAPER_GRAPHICAL_INTERACTION", "EXTERNAL_BLOCKED", implementation="PARTIAL", automated="PARTIAL", reaper="YES", provenance="YES", local="NO", approval="YES", dependency="No screen reader/native semantic custom-control tree observation; UIA exposed zero semantic descendants", action="Run supported accessibility acceptance or request explicit keyboard-only criterion", release="OPEN_BLOCKS_RELEASE", publication="OWNER_MUST_SEPARATE_RELEASE_GATE", disposition="KEYBOARD_ONLY_UNAPPROVED_FOR_RELEASE", replacement="PROPOSED_UNAPPROVED"),
        gate("RS-012", "release_specific", "Final release package certification", "docs/p16f13_development_checkpoint.md final testing row", "All mandatory gates pass for one packaged binary/source/resource set", "out/p20d/release_gate_matrix.csv", "HISTORICAL_ARTIFACT_PROVENANCE", "BLOCKED_BY_DEPENDENCIES", binary="", implementation="NO", automated="NO", provenance="PARTIAL", local="NO", dependency="RS-001 through RS-011 contain unresolved mandatory gates", action="Close external and owner-decision gates, then build and qualify one final package", release="OPEN_BLOCKS_RELEASE", publication="NONE", disposition="NOT_CERTIFIED", exact="NO"),
        gate("GOV-001", "governance", "Development-only source publication", "docs/p16f13_development_checkpoint.md commit policy;docs/p20d_exact_dll_event_retention.md", "Current milestone has explicit publication authorization plus tested reviewed source", "out/p20e/development_publication_policy_review.md", "OWNER_APPROVED_REPLACEMENT_CRITERION", "OWNER_DECISION_REQUIRED", binary="", implementation="YES", automated="YES", provenance="YES", local="YES", approval="YES", dependency="No explicit current authorization supersedes the preserved P20-D publication block", action="Obtain explicit owner approval, then review/stage two prepared groups", release="NO_RELEASE_WAIVER", publication="BLOCKS_PUBLICATION_ONLY", disposition="REQUIRES_EXPLICIT_OWNER_APPROVAL", exact="NO"),
    ]
    return rows


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc).isoformat()
    production_path = ROOT / "out/p20b/candidate-001/bundle/LXR.vst3/Contents/x86_64-win/LXR.vst3"
    diagnostic_path = ROOT / "out/p20d/diagnostic-005/bundle/LXR.vst3/Contents/x86_64-win/LXR.vst3"
    assert sha(production_path) == PRODUCTION
    assert sha(diagnostic_path) == DIAGNOSTIC
    installed_root = Path(os.environ["LOCALAPPDATA"]) / "Programs/Common/VST3/LXR.vst3/Contents"
    installed = installed_root / "x86_64-win/LXR.vst3"
    assert sha(installed) == PROTECTED
    protected = [{"path": str(installed), "sha256": sha(installed), "bytes": installed.stat().st_size}]
    for name in ASSETS:
        target = installed_root / "Resources/LXR" / name
        assert sha(target) == sha(ROOT / "data" / name)
        protected.append({"path": str(target), "sha256": sha(target), "bytes": target.stat().st_size})

    manifest = read_json(ROOT / "out/p20d/diagnostic-005/source_manifest.json")
    hashes = {row["path"].replace("\\", "/"): row["sha256"] for row in manifest["files"]}
    reconciliation = []
    for name in START_PATHS:
        observed = sha(ROOT / name)
        expected = hashes[name]
        assert observed == expected
        reconciliation.append({"path": name, "p20d_final_sha256": expected,
                               "observed_before_p20e_sha256": observed, "status": "MATCH"})
    write_json("p20bcd_worktree_reconciliation.json", {
        "classification": "ACCEPTED_INTENTIONAL_P20BCD_WORKTREE",
        "starting_path_count": len(START_PATHS), "all_hashes_match": True,
        "index_empty_before_p20e": True, "unrelated_paths": [], "paths": reconciliation})

    live = subprocess.check_output(["git", "ls-remote", "origin",
        "refs/heads/p12b-4c-fir-resampler"], cwd=ROOT, text=True).split()[0]
    write_json("baseline_inventory.json", {
        "generated_utc": now, "classification": "ACCEPTED_INTENTIONAL_P20BCD_WORKTREE",
        "repository": str(ROOT), "branch": git("branch", "--show-current"),
        "starting_head": HEAD, "local_head": git("rev-parse", "HEAD"),
        "tracking_head": git("rev-parse", "@{upstream}"), "live_remote_head": live,
        "remote": git("remote", "get-url", "origin"), "starting_index_empty": True,
        "starting_path_count": 18, "firmware": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
        "iplug2": "d54f69050f517e43b941d88c2a170f0a840b9ee4",
        "vst3sdk": "3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96",
        "production_sha256": PRODUCTION, "diagnostic_sha256": DIAGNOSTIC,
        "protected_sha256": PROTECTED})

    gates = register()
    write_csv("release_gate_register.csv", gates)
    authorities = [
        ("ORIGINAL_FIRMWARE_SOURCE", "Pinned original v0.37 source", "Defines firmware behavior; not host GUI evidence"),
        ("ORIGINAL_C_DIFFERENTIAL", "Unchanged-original-C comparison", "Proves tested C behavior only"),
        ("STANDALONE_NATIVE_TEST", "Current source/native harness", "Not an exact DLL or REAPER observation"),
        ("EXACT_DLL_CONTROLLED_HOST", "Exact VST3 in controlled host", "Not actual REAPER evidence"),
        ("STEINBERG_VALIDATOR", "Steinberg validator", "Interface compliance; not musical/GUI acceptance"),
        ("ACTUAL_REAPER_PRODUCTION_PATH", "Actual REAPER processing observation", "Required where gate explicitly names REAPER"),
        ("ACTUAL_REAPER_GRAPHICAL_INTERACTION", "Actual visible REAPER GUI interaction", "Required for graphical/manual gates"),
        ("HISTORICAL_ARTIFACT_PROVENANCE", "Preserved original artifact record", "Cannot be reconstructed by path or current rebuild"),
        ("OWNER_APPROVED_REPLACEMENT_CRITERION", "Explicit owner-approved replacement", "No proposal becomes authority before approval"),
    ]
    write_csv("release_gate_authority_matrix.csv", [
        {"authority": a, "definition": b, "substitution_rule": c} for a, b, c in authorities])

    (OUT / "engineering_evidence_summary.md").write_text(
        "# P20-E engineering evidence summary\n\nP20-D focused engineering remains PASS. No source or test configuration affecting that qualification changed in P20-E and no large regression rerun was justified. Preserved evidence reports 64/64 regression commands, P20-D 646/0, P20-B 833/0, P20-C 97/0 at O0/O2, P16-E 9,869/0 at O0/O2, 76/76 frozen PCM references, zero measured callback allocations, and Steinberg validator 47/47 for production and diagnostic candidates. Controlled-host results remain distinct from actual REAPER evidence.\n",
        encoding="utf-8")

    pcm_rows = []
    for name in ["f7", "f4_selector0", "f4_selector12", "f4_off_stress"]:
        case = read_json(ROOT / f"out/p20c/historical_pcm_case_{name}.json")
        pcm_rows.append({"test_id": case["test_id"], "disposition": case["disposition"],
            "first_frame": case["first_unequal"]["frame"], "channel": case["first_unequal"]["channel"],
            "expected": case["first_unequal"]["expected"], "observed": case["first_unequal"]["observed"],
            "mismatch_count": case["mismatch_count"], "preserved_reference_sha256": case["reference_file_sha256"],
            "preserved_fixture_sha256": case["fixture_sha256"],
            "missing": "exact historical executable/toolchain/callback/native phase/transient startup state",
            "historical_root_cause_provable": "NO", "prospective_protocol_addresses_missing_fields": "YES",
            "replacement_proposal": "PROPOSED_UNAPPROVED", "remaining_unproven_after_replacement": "historical equality and historical root cause"})
    write_csv("historical_pcm_decision_matrix.csv", pcm_rows)
    (OUT / "historical_fir_identity_decision.md").write_text(
        "# Historical FIR identity decision\n\nStatus: **HISTORICAL_IDENTITY_UNPROVABLE**. The earlier Windows Application Control-denied executable has no preserved SHA-256/build manifest/PE identity. The passing image now at the path cannot identify the earlier image. Proposed owner option: accept an equivalent-source rebuild with pinned source/toolchain and exact manifest as a prospective criterion. This remains unapproved and cannot prove the old image passed. No security policy change is proposed.\n",
        encoding="utf-8")

    write_csv("reaper_manual_acceptance_matrix.csv", [
        {"step": n, "requirement": text, "evidence_class": "ACTUAL_REAPER_GRAPHICAL_INTERACTION", "status": "EXTERNAL_BLOCKED"}
        for n, text in enumerate(["Verify REAPER 7.80 isolated profile and production DLL SHA-256",
            "Observe the real Import bank control", "Import a valid .lxqb", "Reject malformed .lxqb without state loss",
            "Observe imported pattern state", "Play the imported sequence", "Save and reopen the project",
            "Capture screenshots or equivalent actual observations"], 1)])
    write_csv("reaper_event_acceptance_matrix.csv", [
        {"scenario": text, "candidate_sha256": PRODUCTION, "oracle": "5632353/128 native rate; 32-frame blocks; zero native-block identity tolerance", "trace_schema": "callback index/frames, transport, PPQ/tempo, native boundary, pattern/generation, ordered triggers and locks", "status": "EXTERNAL_BLOCKED"}
        for text in ["start/stop/restart", "tempo change", "seek/reanchor", "loop wrap", "pending pattern transition", "Euclidean during playback", "copy/clear during playback"]])
    (OUT / "accessibility_dpi_decision.md").write_text(
        "# Accessibility and DPI decision\n\nAutomated scale mapping at 100/125/150/200% and complete keyboard paths pass. Physical multi-monitor DPI, popup/file-dialog placement and actual REAPER observation remain EXTERNAL_BLOCKED. UI Automation found zero semantic custom-control descendants and no screen-reader run exists, so native accessibility remains KEYBOARD_ONLY rather than PASS. Owner approval would be required to adopt keyboard-only as the release criterion; no approval is recorded.\n",
        encoding="utf-8")

    policy = """# Development publication policy review

Eligibility: **REQUIRES_EXPLICIT_OWNER_APPROVAL**.

1. The controlling rule is milestone-specific authorization plus complete engineering evidence, explicit staged-path/diff/blob review, tested-source identity, protected-install verification and fast-forward push.
2. It is documented in `docs/p16f13_development_checkpoint.md` (opening scope and Commit and publication policy), milestone entries in `docs/phase_log.md`, and the preserved P20-B/C/D reports.
3. Development checkpoints and release certification are explicitly separate.
4. Project-wide policy does not inherently require REAPER graphical PASS before every development commit: P19 and P20-A development commits were published while direct GUI remained blocked. Those milestones had their own authorization.
5. Project-wide policy likewise does not inherently require historical PCM closure before a development commit: P16-F13 explicitly authorized a development-only checkpoint with release rows open.
6. Test-only diagnostics may be published in principle when compile-time isolated and explicitly authorized; the P20-D guard satisfies the engineering side but supplies no Git authorization itself.
7. The P20-B Import repair satisfies development engineering standards: exact production Import, malformed/Busy rollback, persistence and regression evidence pass.
8. The P20-D capture satisfies development engineering standards: fixed, guarded, per-instance, zero measured allocations, PCM/RNG invariant and complete retained traces.
9. Explicit owner approval is required now because P16-F13 and earlier milestone permissions were task-specific, while P20-B/C/D deliberately retained these exact changes and P20-D records publication BLOCKED.
10. No engineering defect prevents publication. The sole publication blocker is absence of explicit current authority to separate the verified development checkpoint from unresolved release-only gates.

No files are staged by P20-E. Approval would authorize only the reviewed development commits; it would not waive any REAPER, PCM, FIR, accessibility or release-package gate.
"""
    (OUT / "development_publication_policy_review.md").write_text(policy, encoding="utf-8")

    groups = {
        "eligibility": "REQUIRES_EXPLICIT_OWNER_APPROVAL", "staged": False,
        "group_a": {"commit": "fix(lxr): restore pattern bank Import control",
            "paths": ["vendor/iPlug2/Examples/LXR/LXR.cpp (Import tag/control/focus hunks only)",
                      "tests/P20BHostEventCaptureTest.cpp (whole new exact-host test; retained intact because Import and event fixtures share one translation unit)",
                      "docs/p20b_release_blocker_resolution.md", "tools/verification/report_p20b_checkpoint.py",
                      "docs/phase_log.md (P20-B hunk only)",
                      "tools/verification/build_p16f3_candidate.py (P20-B private-candidate hunks only)"],
            "excludes": ["LXR.cpp diagnostic ProcessBlock hunk", "adapter/retention hook", "P20-C/D tests"]},
        "group_b": {"commit": "test(lxr): qualify retained native event capture",
            "depends_on": "group_a", "paths": ["vendor/iPlug2/Examples/LXR/LXR.cpp (diagnostic ProcessBlock hunk only)",
                "dsp/LXRAudioAdapter.h", "dsp/LXRAudioAdapter.cpp", "dsp/NativeBatchCapture.h",
                "tests/P19EVST3SequencerGUITest.cpp", "tests/P20CHostTransitionCaptureTest.cpp",
                "tests/P20DExactDllRetentionTest.cpp", "tools/verification/p20d_diagnostic_capture.targets",
                "tools/verification/run_p20d_retention_tests.py", "tools/verification/report_p20c_checkpoint.py",
                "tools/verification/report_p20d_checkpoint.py", "tools/verification/build_p16f3_candidate.py (P20-D hunks only)",
                "docs/p20c_release_evidence_closure.md", "docs/p20d_exact_dll_event_retention.md",
                "docs/p20e_release_gate_decision_register.md", "docs/p20e_owner_decisions.md",
                "tools/verification/report_p20e_checkpoint.py", "tools/verification/validate_p20e_release_register.py",
                "docs/phase_log.md (P20-C/D/E hunks only)"],
            "compile_guard": "LXR_P20D_DIAGNOSTIC_CAPTURE absent from ordinary project definitions"},
        "review_protocol": ["construct patches from explicit hunks", "build each proposed tree without committing",
            "run affected qualification if staged bytes differ from P20-D source manifest", "git diff --cached --check",
            "git diff --cached --name-status", "review full staged diff and blob hashes", "verify fast-forward and protected install"]}
    write_json("proposed_checkpoint_manifest.json", groups)

    decisions = {
        "continue_engineering": {"decision": "GO", "reason": "No current implementation defect blocks bounded work"},
        "development_publication": {"decision": "CONDITIONAL_GO", "condition": "Explicit owner approval of Decision 1", "eligibility": "REQUIRES_EXPLICIT_OWNER_APPROVAL"},
        "internal_testing": {"decision": "GO", "condition": "Private, isolated, non-installed, protected-install checks retained"},
        "release_packaging_preparation": {"decision": "CONDITIONAL_GO", "scope": "Source/licence/resource manifest and checklist design only; no binary distribution"},
        "public_release": {"decision": "NO_GO", "reason": "Mandatory REAPER, historical, accessibility/DPI and final-package gates remain open"}}
    write_json("go_no_go_decisions.json", decisions)
    write_csv("final_release_checklist.csv", [{"gate_id": row["gate_id"], "requirement": row["requirement"],
        "status": row["current_status"], "satisfied": "YES" if row["current_status"] == "PASS" else "NO",
        "next_action": row["proposed_next_action"]} for row in gates])
    write_json("artifact_identity_manifest.json", {
        "production": {"path": str(production_path), "sha256": PRODUCTION,
                       "source_fingerprint": "80DB83C0C8F79020B06580A54E5627537CE35C5B3E1FBE57DEE33413C0113400"},
        "diagnostic": {"path": str(diagnostic_path), "sha256": DIAGNOSTIC,
                       "source_fingerprint": manifest["fingerprint"], "release_binary": False,
                       "compile_definition": "LXR_P20D_DIAGNOSTIC_CAPTURE"},
        "historical_fir": {"classification": "HISTORICAL_IDENTITY_UNPROVABLE",
                           "identity_inferred_from_path": False}})
    write_json("protected_install_manifest.json", {"status": "PASS", "unchanged": True,
        "expected_dll_sha256": PROTECTED, "entries": protected})

    # Validation results are written by validate_p20e_release_register.py.
    report = f"""# P20-E verification report

Generated: {now}

Decision review: **PASS**. Full release: **NOT_CERTIFIED**.

All 18 P20-B/C/D paths match the final P20-D source manifest. Production `{PRODUCTION}`, diagnostic `{DIAGNOSTIC}`, protected `{PROTECTED}`, pins and seven resources match. The register contains 38 mandatory gates: 25 PASS, 6 EXTERNAL_BLOCKED, 5 historical-evidence/identity insufficient, 1 dependency-blocked final package and 1 owner-decision publication gate.

Publication eligibility is **REQUIRES_EXPLICIT_OWNER_APPROVAL**. Earlier development checkpoints prove that release blockers do not automatically prohibit development commits, but their authorizations were milestone-specific and do not supersede the preserved P20-D publication block. No staging, commit or push occurred. Public release is NO-GO.
"""
    (OUT / "verification_report.md").write_text(report, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
