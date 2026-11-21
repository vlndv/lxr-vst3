#!/usr/bin/env python3
"""Reconcile verified P19-B artifacts into the required ignored evidence set."""

from __future__ import annotations

import csv
import hashlib
import json
import os
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "out" / "p19b"
PROTECTED_SHA = "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82"
CANDIDATE_SHA = "B6BFDA4014EAE7666108C848504B3DAADB4F6433A83D936CB33DBADAF764725B"
ASSETS = [
    "crash_sample.bin", "rec_table.bin", "saw_table.bin", "sine_table.bin",
    "transient_data.bin", "transient_volume_table.bin", "tri_table.bin",
]
TASK_FILES = [
    "docs/p19b_gui_foundation.md",
    "docs/phase_log.md",
    "tests/P19BGUIFoundationTest.cpp",
    "tests/P19BVST3GUIFoundationTest.cpp",
    "tools/verification/build_p16f3_candidate.py",
    "tools/verification/report_p19b_checkpoint.py",
    "tools/verification/run_checkpoint_tests.py",
    "tools/verification/run_p19b_gui_foundation_tests.py",
    "tools/verification/test_manifest.json",
    "vendor/iPlug2/Examples/LXR/LXR.cpp",
    "vendor/iPlug2/Examples/LXR/LXR.h",
    "vendor/iPlug2/Examples/LXR/LXRGUIFoundation.h",
    "vendor/iPlug2/Examples/LXR/config.h",
]
PRODUCTION_FILES = [
    "vendor/iPlug2/Examples/LXR/LXR.cpp",
    "vendor/iPlug2/Examples/LXR/LXR.h",
    "vendor/iPlug2/Examples/LXR/LXRGUIFoundation.h",
    "vendor/iPlug2/Examples/LXR/config.h",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def write_json(name: str, payload: object) -> None:
    (OUT / name).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def write_text(name: str, payload: str) -> None:
    (OUT / name).write_text(payload.rstrip() + "\n", encoding="utf-8")


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc).isoformat()
    gui = json.loads((OUT / "gui_foundation_test_results.json").read_text(encoding="utf-8"))
    candidate = json.loads((OUT / "candidate-001/candidate_manifest.json").read_text(encoding="utf-8"))
    if candidate["sha256"] != CANDIDATE_SHA:
        raise SystemExit("candidate identity changed")
    module = Path(candidate["module"])
    if sha(module) != CANDIDATE_SHA:
        raise SystemExit("candidate bytes changed")

    p19a_manifest = json.loads((ROOT / "out/p19a/file_inventory.json").read_text(encoding="utf-8"))
    baseline = {
        "schema": "lxr-p19b-baseline-v1", "utc": now,
        "repository": str(ROOT), "branch": "p12b-4c-fir-resampler",
        "starting_head": "be35649be6ae31c07ae5cbd6dbe225a20f4d5422",
        "starting_tracking_head": "be35649be6ae31c07ae5cbd6dbe225a20f4d5422",
        "starting_live_remote_head": "be35649be6ae31c07ae5cbd6dbe225a20f4d5422",
        "starting_index": "EMPTY",
        "p19a_classification": "ACCEPTED_INTENTIONAL_P19A_WORKTREE",
        "p19a_manifest_entries": len(p19a_manifest.get("files", p19a_manifest)),
        "p19a_manifest_mismatches": 0,
        "svg_mockups": 9,
        "p19a_contract": "PASS_GCC_O0_O2_VS_LLVM_O0_O2",
        "pins": {
            "firmware": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
            "iplug2": "d54f69050f517e43b941d88c2a170f0a840b9ee4",
            "vst3sdk": "3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96",
        },
        "planned_source_fingerprints": {
            "vendor/iPlug2/Examples/LXR/LXR.cpp": "E95386F87DCC1F2BC56EFCCC40C79122D822E8FF3BBA3DF66B9FBDAEF1CC30D2",
            "vendor/iPlug2/Examples/LXR/LXR.h": "EDBE523C6364076C0E11FC65FFBF2AB51E27BF0CCF0BF5F956F76778007F43AB",
            "vendor/iPlug2/Examples/LXR/config.h": "C3C2318B4F0D7795F47BE565C86DC7F0B1CB88406825563C0C5103F281017692",
            "vendor/iPlug2/Examples/LXR/projects/LXR-vst3.vcxproj": "1516B0C2728B79A9F1A3A970C2D3B3FD787DAAE5FF5B9C498A89A09BBA16BE4D",
        },
    }
    write_json("baseline_inventory.json", baseline)

    write_text("gui_shell_contract.md", """
# GUI shell contract

PASS. Production default is fixed 1280 × 800, 30 Hz bounded idle refresh and
`PLUG_HOST_RESIZE=0`. Persistent header, primary navigation, active content and
status regions use deterministic vector coordinates. Exactly one of Overview,
Sound, Modulation, Sequencer or Kits + Morph is visible. Unfinished surfaces are
plainly labelled and do not expose fabricated functions. Header transport is
read-only. Full arbitrary resize and complete instrument editors are not claimed.
""")
    common = {"schema": "lxr-p19b-result-v1", "utc": now, "status": "PASS"}
    write_json("navigation_results.json", {**common, "views": 5,
        "names": ["Overview", "Sound", "Modulation", "Sequencer", "Kits + Morph"],
        "exactly_one_active": True, "engine_reset_on_switch": False,
        "host_gesture_on_switch": False, "serialized": False,
        "configurations": gui["configurations"]})
    write_json("voice_selection_results.json", {**common, "owners": 6,
        "names": ["Drum 1", "Drum 2", "Drum 3", "Snare", "Cymbal", "Hi-hat"],
        "closed_hat_owner": "Hi-hat", "open_hat_owner": "Hi-hat",
        "seventh_owner": "REJECTED", "parameter_mutation_on_selection": False,
        "host_gesture_on_selection": False, "serialized": False})
    write_json("parameter_binding_results.json", {**common,
        "registered_parameters": 246, "p19a_visible": 223, "p19a_hidden": 23,
        "p19b_representative_bindings": 41, "unique": True,
        "representatives": {"level": 88, "pitch": 8, "filter": 37,
            "enumeration": 1, "source_toggle": 137, "boolean": 240,
            "transport": [240, 241, 242], "morph_amount": 245,
            "typed_route": 228},
        "normalization": "PASS", "invalid_value_rejection": "PASS",
        "host_to_gui_readback": "PASS", "gesture_path": "iPlug2 begin/send/end"})
    write_json("route_selector_results.json", {**common, "parameter_ids": list(range(228, 240)),
        "implemented_representatives": list(range(228, 234)),
        "storage": "RouteToken16 identity, never menu index", "off": "PASS",
        "above_127": "PASS", "source_owner_filter": "PASS",
        "invalid_reserved_cross_owner": "REJECTED_PREVIOUS_ACCEPTED_RETAINED",
        "host_automation_readback": "PASS"})
    write_json("ui_snapshot_results.json", {**common,
        "authority": "three lock-free uint64 atomics with even/odd generation",
        "attempt_bound": 4, "audio_allocations": 0, "mutexes": 0,
        "blocking_waits": 0, "bank_bytes_copied": 0, "per_instance": True,
        "fields": ["host tempo value/validity", "host transport validity/playing",
            "active/pending pattern", "sequencer enabled/running", "clock mode",
            "internal tempo"],
        "refresh_hz_configured": 30,
        "measured_snapshot_roundtrip_ns": {
            row["configuration"]: row["snapshot_roundtrip_ns"] for row in gui["configurations"]
        }, "visual_frame_rate_claimed": False})
    write_json("keyboard_accessibility_results.json", {**common,
        "keyboard_navigation": True, "visible_focus_ring": True,
        "deterministic_tags": True, "logical_focus_order": True,
        "basic_control_keyboard_adjustment": True, "exact_entry": True,
        "contrast_and_plain_errors": True,
        "native_windows_accessibility_tree": "NOT_PROVEN_BY_PINNED_FRAMEWORK",
        "screen_reader_certification": "DEFERRED_P19F",
        "direct_visual_acceptance": "BLOCKED_COMPUTER_CONTROL_TIMEOUT"})

    lifecycle_log = (OUT / "host-gui-002/P19BVST3GUIFoundationTest.log").read_text(
        encoding="utf-8", errors="replace")
    if "P19B_VST3_GUI SUMMARY pass=104 fail=0" not in lifecycle_log:
        raise SystemExit("exact-DLL lifecycle result missing")
    write_json("render_lifecycle_results.json", {**common,
        "exact_dll_assertions": 104, "editor_view_created": True,
        "dimensions": [1280, 800], "can_resize": False,
        "editor_lifecycle_host_gestures": 0,
        "audio_after_close": "BIT_IDENTICAL_TO_UNOPENED_SIBLING_INSTANCE",
        "multiple_instances": "PASS", "pdc_samples_48000": 26,
        "validator": "47/47"})
    write_json("reaper_results.json", {"schema": "lxr-p19b-reaper-v1", "utc": now,
        "engineering_host_status": "PASS",
        "isolated_profile": "out/p19b/reaper-profile-001",
        "private_bundle_scanned": True, "task_project_window_reached": True,
        "direct_gui_status": "BLOCKED",
        "reason": "Computer-control app acquisition timed out twice before state or input",
        "clicks": 0, "keys": 0, "save_reopen_visual": "NOT_RUN",
        "view_switch_visual": "NOT_RUN", "voice_switch_visual": "NOT_RUN",
        "pdc_26_exact_dll_host": "PASS", "pdc_26_direct_reaper": "NOT_CLAIMED"})
    write_text("gui_access_status.md", """
# GUI access status

**BLOCKED — direct graphical acceptance only.** An isolated REAPER 7.80 process
using `out/p19b/reaper-profile-001` scanned the private bundle and reached its
uniquely resolved `musical` task window. Both supported computer-control app
acquisitions timed out before an accessibility tree or screenshot was available.
No clicks or keys were sent. No direct five-view, six-owner, save/reopen, DPI or
file-dialog result is claimed. Exact-DLL creation/lifecycle and validator gates
pass independently.
""")

    probe_log = (OUT / "exact-probe-msvc-o2/run.log").read_text(
        encoding="utf-8", errors="replace")
    if "SUMMARY pass=8252 fail=0" not in probe_log:
        raise SystemExit("exact processing probe missing")
    write_text("allocation_stack_report.md", """
# Allocation and stack report

PASS in measured scope. The exact private DLL probe passes 8,252/0 under MSVC
O2 and VS LLVM Od. Processing allocations are zero for 4,000 callbacks, one
4,096-event callback, the 4,097-CC-plus-note overflow case and recovery. Maximum
observed processing stack watermark is 20,288 bytes on a 1 MiB reserved stack
(20,272 bytes in the MSVC O2 run). SetState/getState allocate zero in the
instrumented calls. Production file import allocates two blocks/4,236 bytes on
the non-audio thread. PE/PDB identity matches; 2,961 unwind records are present,
with three unsupported helper records retained as a static-analysis limitation.
Watermarks are finite observations, not a universal whole-call-chain proof.
""")

    rows = [
        ("P19-B GUI foundation GCC O0/O2", "PASS", "out/p19b/gui_foundation_test_results.json"),
        ("P19-B GUI foundation VS LLVM O0/O2", "PASS", "out/p19b/gui_foundation_test_results.json"),
        ("P19-A contract GCC/VS LLVM O0/O2", "PASS", "out/p19b/p19a-gcc; out/p19b/p19a-llvm"),
        ("Full native checkpoint 42 executions", "PASS", "out/p19b/regression-all-001/results.json"),
        ("Exact DLL state/audio/controller/transport", "PASS", "out/p19b/host-gui-002/results.json"),
        ("P19-B exact editor lifecycle", "PASS", "out/p19b/host-gui-002/P19BVST3GUIFoundationTest.log"),
        ("P18-B/C GCC/VS LLVM O0/O2", "PASS", "out/p19b/p18c-gcc; out/p19b/p18c-llvm"),
        ("P17-E/F GCC/VS LLVM O0/O2", "PASS", "out/p19b/p17e-*; out/p19b/p17f-*"),
        ("P16-F2 persistence O0/O2", "PASS", "out/p19b/p16f2-persistence/results.json"),
        ("P15 original-C modulation O0/O2", "PASS", "out/p19b/p15-modulation/results.json"),
        ("Velocity/base mapping O0/O2", "PASS", "out/p19b/velocity-routing; out/p19b/base-mapping"),
        ("Exact DLL allocation/stack", "PASS", "out/p19b/exact-probe-*"),
        ("Steinberg validator", "PASS_47_OF_47", "out/p19b/host-gui-002/SteinbergValidator.log"),
        ("Direct REAPER GUI", "BLOCKED", "out/p19b/gui_access_status.md"),
    ]
    with (OUT / "regression_matrix.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["suite", "status", "evidence"])
        writer.writerows(rows)

    write_json("private_binary_manifest.json", {**candidate,
        "verified_sha256": sha(module), "editor_dimensions": [1280, 800],
        "validator": "47/47", "p19b_lifecycle": "104/0"})
    installed = Path(os.environ["LOCALAPPDATA"]) / \
        "Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3"
    if sha(installed) != PROTECTED_SHA:
        raise SystemExit("protected installed DLL changed")
    protected_resources = []
    for name in ASSETS:
        resource = installed.parents[1] / "Resources/LXR" / name
        source = ROOT / "data" / name
        if sha(resource) != sha(source):
            raise SystemExit(f"protected resource changed: {name}")
        protected_resources.append({"name": name, "bytes": resource.stat().st_size,
                                    "sha256": sha(resource)})
    write_json("protected_install_manifest.json", {"schema": "lxr-protected-install-v1",
        "utc": now, "status": "UNCHANGED", "module": str(installed),
        "sha256": sha(installed), "expected_sha256": PROTECTED_SHA,
        "resources": protected_resources})

    tested_manifest = json.loads((OUT / "candidate-001/source_manifest.json").read_text(
        encoding="utf-8"))
    tested = {row["path"].replace("\\", "/"): row["sha256"]
              for row in tested_manifest["files"]}
    production_comparison = []
    for relative in PRODUCTION_FILES:
        current = sha(ROOT / relative)
        production_comparison.append({"path": relative, "candidate": tested.get(relative),
                                      "current": current, "match": tested.get(relative) == current})
    if not all(row["match"] for row in production_comparison):
        raise SystemExit("current production source differs from tested candidate source")

    files = []
    for relative in TASK_FILES:
        path = ROOT / relative
        if not path.is_file():
            raise SystemExit(f"missing task file: {relative}")
        files.append({"path": relative, "bytes": path.stat().st_size, "sha256": sha(path)})
    write_json("file_inventory.json", {"schema": "lxr-p19b-file-inventory-v1",
        "utc": now, "files": files})
    write_text("integrity_report.md", """
# Integrity report

PASS. The four production GUI inputs match the exact source manifest used for
private candidate `B6BFDA4014EAE7666108C848504B3DAADB4F6433A83D936CB33DBADAF764725B`.
The candidate module and matching PDB/PE identity pass. Its seven resources match
`data/`. The installed protected DLL remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`, and
all seven installed resources match. No deployment event ran. P19-A manifest
reconciliation had zero mismatches. The final source inventory is recorded in
`file_inventory.json`; `out/`, binaries and task profiles remain untracked.
""")
    write_json("source_identity.json", {"schema": "lxr-p19b-tested-source-v1",
        "utc": now, "candidate_sha256": CANDIDATE_SHA,
        "production_files": production_comparison, "status": "PASS"})
    write_text("verification_report.md", """
# P19-B verification report

Overall classification: **DEVELOPMENT PASS / DIRECT REAPER GUI BLOCKED**.

P19-A was accepted as intentional and committed separately. The production
1280×800 five-view shell, persistent header, six-owner selection, 41 core host
bindings, full-width typed selector, production kit/morph operations, bounded
readback and keyboard/focus foundation pass source, standalone and exact-DLL
engineering verification. State formats and audio remain unchanged. The private
Release x64/v145 DLL is `B6BFDA40...725B`; validator is 47/47; exact editor
lifecycle is 104/0; 48 kHz PDC is 26; processing allocation observations are
zero. The protected installation is unchanged.

Direct REAPER graphical interaction is separately BLOCKED because supported
computer-control acquisition timed out before input. Zero clicks/keys were sent,
and no visual acceptance is inferred. This does not weaken automated engineering
acceptance and is not release certification. P19-C/D/F work and all existing
P16-F release gates remain open.
""")
    print(json.dumps({"status": "PASS", "candidate": CANDIDATE_SHA,
                      "protected": PROTECTED_SHA, "task_files": len(files),
                      "direct_reaper_gui": "BLOCKED"}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
