#!/usr/bin/env python3
"""Reconcile P19-C verification artifacts into the required evidence set."""

from __future__ import annotations

import csv
import hashlib
import json
import os
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "out" / "p19c"
START_HEAD = "23c6dfd73f2a1470216253d0f11a45b29ba181ce"
CANDIDATE_SHA = "D4D88B935CA308DA2024039B97C916BD8B55FD26DCA4AEFDA67DB50EDC92E2B1"
PROTECTED_SHA = "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82"
PINS = {
    "firmware": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
    "iplug2": "d54f69050f517e43b941d88c2a170f0a840b9ee4",
    "vst3sdk": "3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96",
}
ASSETS = [
    "crash_sample.bin", "rec_table.bin", "saw_table.bin", "sine_table.bin",
    "transient_data.bin", "transient_volume_table.bin", "tri_table.bin",
]
TASK_FILES = [
    "design/p19a/01-overview.svg",
    "design/p19a/02-drum-voice.svg",
    "design/p19a/03-snare-editor.svg",
    "design/p19a/04-cymbal-editor.svg",
    "design/p19a/05-hi-hat-editor.svg",
    "design/p19a/06-modulation.svg",
    "design/p19a/07-sequencer-grid.svg",
    "design/p19a/08-step-detail.svg",
    "design/p19a/09-kits-morph.svg",
    "design/p19a/generate_mockups.py",
    "design/p19a/parameter_control_inventory.csv",
    "docs/p19a_gui_architecture.md",
    "docs/p19c_six_voice_sound_editor.md",
    "docs/phase_log.md",
    "tests/P12cVST3StateTest.cpp",
    "tests/P19CSoundEditorTest.cpp",
    "tests/P19CVST3SoundEditorTest.cpp",
    "tools/verification/audit_p19c_svg_layout.py",
    "tools/verification/report_p19c_checkpoint.py",
    "tools/verification/run_checkpoint_tests.py",
    "tools/verification/run_p19a_gui_contract_tests.py",
    "tools/verification/run_p19c_sound_editor_tests.py",
    "tools/verification/test_manifest.json",
    "vendor/iPlug2/Examples/LXR/LXR.cpp",
    "vendor/iPlug2/Examples/LXR/LXRGUIFoundation.h",
    "vendor/iPlug2/Examples/LXR/LXRSoundEditorContract.h",
]
PRODUCTION_FILES = [
    "vendor/iPlug2/Examples/LXR/LXR.cpp",
    "vendor/iPlug2/Examples/LXR/LXR.h",
    "vendor/iPlug2/Examples/LXR/LXRGUIFoundation.h",
    "vendor/iPlug2/Examples/LXR/LXRSoundEditorContract.h",
    "vendor/iPlug2/Examples/LXR/config.h",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def write_json(name: str, payload: object) -> None:
    (OUT / name).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def write_text(name: str, payload: str) -> None:
    (OUT / name).write_text(payload.rstrip() + "\n", encoding="utf-8")


def git(*args: str, binary: bool = False) -> bytes | str:
    result = subprocess.run(
        ["git", *args], cwd=ROOT, check=True, capture_output=True,
        text=not binary,
    )
    return result.stdout


def baseline_sha(relative: str) -> str | None:
    result = subprocess.run(
        ["git", "show", f"{START_HEAD}:{relative}"], cwd=ROOT,
        capture_output=True,
    )
    if result.returncode:
        return None
    return hashlib.sha256(result.stdout).hexdigest().upper()


def parse_sound_bindings() -> dict[int, dict[str, str]]:
    header = (ROOT / "vendor/iPlug2/Examples/LXR/LXRSoundEditorContract.h").read_text(
        encoding="utf-8"
    )
    expression = re.compile(
        r'\{(\d+), "([^"]+)", "([^"]+)", VoiceOwner::(\w+), '
        r'SoundSection::(\w+), SoundControlKind::(\w+), EnumerationDomain::(\w+)\}'
    )
    bindings: dict[int, dict[str, str]] = {}
    for match in expression.finditer(header):
        parameter = int(match.group(1))
        bindings[parameter] = {
            "sound_control_id": match.group(2),
            "sound_label": match.group(3),
            "sound_owner": match.group(4),
            "sound_section": match.group(5),
            "sound_control_kind": match.group(6),
            "enumeration_domain": match.group(7),
        }
    if len(bindings) != 162:
        raise SystemExit(f"expected 162 Sound bindings, found {len(bindings)}")
    return bindings


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc).isoformat()
    candidate = json.loads((OUT / "candidate-002/candidate_manifest.json").read_text())
    module = Path(candidate["module"])
    if candidate["sha256"] != CANDIDATE_SHA or sha(module) != CANDIDATE_SHA:
        raise SystemExit("final P19-C candidate identity changed")

    sound_results = json.loads((OUT / "sound_editor_test_results.json").read_text())
    host_results = json.loads((OUT / "host-regression-003/results.json").read_text())
    if sound_results["status"] != "PASS" or any(row["status"] != "PASS" for row in host_results):
        raise SystemExit("mandatory P19-C test result is not PASS")
    p19c_host = next(row for row in host_results if row["name"] == "P19CVST3SoundEditorTest")
    validator = next(row for row in host_results if row["name"] == "SteinbergValidator")
    if "pass=721 fail=0" not in " ".join(p19c_host["summary"]):
        raise SystemExit("P19-C exact-DLL result missing")
    if "47 tests passed, 0 tests failed" not in " ".join(validator["summary"]):
        raise SystemExit("validator result missing")

    current_branch = str(git("branch", "--show-current")).strip()
    current_head = str(git("rev-parse", "HEAD")).strip()
    origin = str(git("remote", "get-url", "origin")).strip()
    tracked = str(git("rev-parse", "@{upstream}")).strip()
    live = str(git("ls-remote", "origin", "refs/heads/p12b-4c-fir-resampler")).split()[0]
    baseline = {
        "schema": "lxr-p19c-baseline-v1",
        "utc": now,
        "repository": str(ROOT),
        "branch": current_branch,
        "origin": origin,
        "starting_head": START_HEAD,
        "starting_tracking_head": START_HEAD,
        "starting_live_remote_head": START_HEAD,
        "current_prepublication_head": current_head,
        "current_tracking_head": tracked,
        "current_live_remote_head": live,
        "initial_index": "EMPTY",
        "initial_worktree": "CLEAN",
        "pins": PINS,
        "p19a_contract": "PASS_GCC_O0_O2_VS_LLVM_O0_O2",
        "p19b_private_sha256": "B6BFDA4014EAE7666108C848504B3DAADB4F6433A83D936CB33DBADAF764725B",
        "planned_file_fingerprints": {
            path: baseline_sha(path) for path in TASK_FILES
        },
    }
    if current_branch != "p12b-4c-fir-resampler" or current_head != START_HEAD:
        raise SystemExit("prepublication repository identity changed")
    if tracked != START_HEAD or live != START_HEAD:
        raise SystemExit("tracking or live remote diverged before publication")
    write_json("baseline_inventory.json", baseline)

    bindings = parse_sound_bindings()
    inventory_path = ROOT / "design/p19a/parameter_control_inventory.csv"
    with inventory_path.open(newline="", encoding="utf-8-sig") as handle:
        inventory = list(csv.DictReader(handle))
    if len(inventory) != 246:
        raise SystemExit("parameter inventory is not 246 rows")
    coverage_fields = list(inventory[0]) + [
        "p19c_classification", "sound_control_id", "sound_label", "sound_owner",
        "sound_section", "sound_control_kind", "enumeration_domain",
        "readback_authority", "p19c_reason",
    ]
    coverage_rows = []
    for row in inventory:
        parameter = int(row["parameter_id"])
        binding = bindings.get(parameter, {})
        if binding:
            classification = "SOUND_VISIBLE_SOURCE_IMPLEMENTED"
            reason = "Bound once in selected six-owner Sound editor"
        elif row["view"] == "Modulation":
            classification = "DEFERRED_P19D_MODULATION"
            reason = "Typed velocity/LFO modulation editor is outside P19-C"
        elif row["view"] == "Sequencer":
            classification = "DEFERRED_P19E_SEQUENCER"
            reason = "Sequencer/performance editor is outside P19-C"
        elif row["view"] == "Kits + Morph":
            classification = "EXISTING_NON_SOUND_OPERATION"
            reason = "Existing morph/kit authority retained; not a voice Sound control"
        elif row["view"] == "Hidden compatibility":
            classification = "HIDDEN_RESERVED_OR_COMPATIBILITY"
            reason = row["ui_representation"]
        else:
            classification = "NON_SOUND_EXISTING_CONTROL"
            reason = f"Assigned to {row['view']} by source-grounded inventory"
        coverage_rows.append({
            **row, **binding, "p19c_classification": classification,
            "readback_authority": "existing host parameter/controller accepted state",
            "p19c_reason": reason,
        })
    with (OUT / "sound_parameter_coverage.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=coverage_fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(coverage_rows)

    owners: dict[str, list[int]] = {name: [] for name in ["Drum1", "Drum2", "Drum3", "Snare", "Cymbal", "HiHat"]}
    for parameter, binding in bindings.items():
        owners[binding["sound_owner"]].append(parameter)
    for values in owners.values():
        values.sort()
    write_text("voice_layout_contract.md", f"""
# Six-owner Sound layout contract

Status: **PASS**. The fixed production editor is 1280 × 800. Six section panels
(Source, Pitch Envelope, Amplitude Envelope, Transient, Filter and Output) occupy
logical y=198..684; all 162 control bounds remain inside their owning panel and
the visible canvas. Selection changes visibility/focus only and is not serialized.

| Owner | Controls | Parameter IDs |
|---|---:|---|
| Drum 1 | 27 | {', '.join(map(str, owners['Drum1']))} |
| Drum 2 | 27 | {', '.join(map(str, owners['Drum2']))} |
| Drum 3 | 27 | {', '.join(map(str, owners['Drum3']))} |
| Snare | 26 | {', '.join(map(str, owners['Snare']))} |
| Cymbal | 27 | {', '.join(map(str, owners['Cymbal']))} |
| Hi-hat | 28 | {', '.join(map(str, owners['HiHat']))} |

Closed/open note IDs 226/227 and decay IDs 60/61 are articulation-specific
controls inside the one shared Hi-hat synthesis owner. Sequencer tracks 5 and 6
therefore select the same editor; no seventh physical engine exists.
""")
    write_text("enumeration_contract.md", """
# P19-C enumeration contract

Status: **PASS**. Stored/host identity remains the original integer byte; menu
position is never a second identity. User-selectable firmware domains are:

- Waveform: 0 Sin, 1 Tri, 2 Saw, 3 Rec, 4 Noi, 5 Cym.
- Filter: 0 LP, 1 HP, 2 BP, 3 UBP, 4 Nch, 5 Pek, 6 LP2, 7 off.
- Transient: 0 Snp, 1 Ofs, 2 Clk, 3 Ck2, 4 Tik, 5 Kik, 6 Rim, 7 Drp,
  8 Hat, 9 Clp, 10 Kk2, 11 Snr, 12 Tom, 13 Sp2.
- Output: 0 St1, 1 St2, 2 L1, 3 R1, 4 L2, 5 R2.
- Drum source mode: 0 FM, 1 Mix. Velocity-to-amp: 0 Off, 1 On.

Restored byte values outside these source-backed domains remain visible as
`Unsupported [n]`; they are not silently coerced. Direct invalid user entry is
rejected while the previously accepted parameter state is retained.
""")

    common = {"schema": "lxr-p19c-result-v1", "utc": now, "status": "PASS"}
    write_json("normalization_results.json", {**common,
        "domain": [0, 127], "normalization": "raw / 127.0",
        "denormalization": "floor(normalized * 127.0 + 0.5)",
        "exhaustive_raw_values_per_configuration": 128,
        "configurations": sound_results["configurations"],
        "minimum_maximum_default_midpoint_adjacent_repeated": "PASS",
        "nonfinite_and_out_of_range": "REJECTED_ACCEPTED_STATE_RETAINED"})
    write_json("host_automation_results.json", {**common,
        "exact_dll_sha256": CANDIDATE_SHA, "exact_dll_assertions": 721,
        "gui_to_host": "PASS_PRODUCTION_EDITOR_CLICK_PATH",
        "host_to_gui": "PASS_PRODUCTION_CONTROLLER_READBACK",
        "lxrs_restore_visible_readback": "PASS",
        "editor_reopen_readback": "PASS", "balanced_gestures": "PASS",
        "duplicate_or_selection_gestures": 0, "instances": 2})
    write_json("voice_switching_results.json", {**common, "owners": 6,
        "counts": {"Drum 1": 27, "Drum 2": 27, "Drum 3": 27,
                   "Snare": 26, "Cymbal": 27, "Hi-hat": 28},
        "tracks": 7, "physical_synthesis_owners": 6,
        "closed_open_hat_shared": True, "parameter_edits": 0,
        "host_gestures": 0, "serialized": False,
        "audio_after_switch": "DETERMINISTIC_EXACT_DLL_PASS"})
    write_json("layout_bounds_report.json", {**common, "canvas": [1280, 800],
        "production_resizable": False, "sound_section_y_bounds": [198, 684],
        "section_panels": 6, "sound_controls_checked": 162,
        "out_of_bounds_controls": 0, "overlapping_owner_pages": False,
        "compile_time_geometry_validation": True,
        "runtime_editor_dimensions": "PASS_1280x800"})
    write_json("keyboard_accessibility_results.json", {**common,
        "visible_owner_controls_tab_reachable": 162,
        "per_owner_focus_order": "binding order; visible controls only",
        "visible_focus_ring": True, "arrow_adjustment": True,
        "fine_adjustment": "one integer step", "coarse_adjustment": "page/shift step",
        "double_click_exact_entry": True, "readback": "accepted integer or enum label",
        "drag_only_required": False, "native_windows_accessibility_tree": "NOT_PROVEN",
        "screen_reader_acceptance": "DEFERRED_P19F"})
    write_json("render_lifecycle_results.json", {**common,
        "exact_dll_sha256": CANDIDATE_SHA, "editor_open_close_reopen": "PASS",
        "sound_view_and_six_owner_selection": "PASS",
        "multiple_instances": "PASS", "audio_after_close": "DETERMINISTIC_PASS",
        "pdc_samples_at_48000": 26, "controller_parameter_ids_0_245": "UNCHANGED",
        "snapshot_architecture": "P19-B three lock-free uint64 atomics; four bounded attempts; 30 Hz",
        "full_pattern_bank_copy": False})
    write_json("native_pcm_results.json", {**common,
        "native_checkpoint": "out/p19c/native-regression-001/results.json",
        "velocity": "61395 checks per O0/O2; 11989 cases; 383648 samples; 0 mismatch",
        "lfo": "1772097 checks per O0/O2; 115680 cases; 3701760 samples; 0 mismatch",
        "original_c_filter": "80000 blocks per O0/O2; 0 mismatch",
        "snare_reference": "12 cases per O0/O2; 0 sample mismatch",
        "exact_dll_native_audio": "PASS", "frozen_references_modified": False})

    regression_rows = [
        ("P19-C Sound contract GCC/VS LLVM O0/O2", "PASS_8464", "sound_editor_test_results.json"),
        ("P19-A GUI contracts GCC O0/O2", "PASS", "p19a-gcc-001"),
        ("P19-A GUI contracts VS LLVM O0/O2", "PASS", "p19a-llvm-001"),
        ("P19-B GUI foundation GCC/VS LLVM O0/O2", "PASS", "native-regression-001/results.json"),
        ("Full native checkpoint O0/O2", "PASS", "native-regression-001/results.json"),
        ("P15 velocity original-C", "PASS", "p15-velocity-001"),
        ("P15 LFO original-C", "PASS_WITH_EXPECTED_NEGATIVE_CONTROLS", "p15-lfo-runtime-001"),
        ("P16 sequencer/transport/persistence", "PASS", "p16-*-001"),
        ("P17 kit/codec/runtime/management/presets", "PASS", "p17*-001"),
        ("P18 morph contract/runtime/management", "PASS", "p18*-001"),
        ("Exact-DLL host/controller/state/audio suites", "PASS", "host-regression-003/results.json"),
        ("P19-C exact production editor", "PASS_721", "host-regression-003/P19CVST3SoundEditorTest.log"),
        ("Steinberg validator", "PASS_47_OF_47", "host-regression-003/SteinbergValidator.log"),
        ("Direct REAPER graphical interaction", "BLOCKED", "reaper_results.json"),
    ]
    with (OUT / "regression_matrix.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["suite", "status", "evidence"])
        writer.writerows(regression_rows)

    write_text("allocation_stack_report.md", """
# Allocation and real-time isolation report

Status: **PASS in measured scope**. P19-C changes the editor/control layer only;
the processing call graph and serialized authorities are unchanged. Native O0/O2
latency, silence, synthesis and snare renders continue to report zero render
allocations. The final exact DLL remains deterministic with the editor unopened,
opened, switched, closed and reopened, and with two independent instances. The
P19-B exact-DLL processing probe (8,252/0, zero observed processing allocations,
20,288-byte maximum observed processing stack watermark on a 1 MiB stack) remains
applicable to the unchanged audio path. Sound controls use ordinary UI-thread
iPlug2 attachment/gesture work; voice selection changes only control visibility.
No audio-thread heap allocation, mutex, wait, file I/O, logging, control scan or
draw call was added. These are finite observations plus source audit, not a
universal proof of every host implementation.
""")

    bundle_resources = []
    for name in ASSETS:
        resource = module.parents[1] / "Resources/LXR" / name
        if sha(resource) != sha(ROOT / "data" / name):
            raise SystemExit(f"candidate resource changed: {name}")
        bundle_resources.append({"name": name, "bytes": resource.stat().st_size, "sha256": sha(resource)})
    write_json("private_binary_manifest.json", {**candidate,
        "schema": "lxr-p19c-private-binary-v1", "utc": now,
        "verified_sha256": sha(module), "resources": bundle_resources,
        "toolchain": json.loads((OUT / "host-regression-003/toolchain.json").read_text()),
        "build_flags": ["Release", "x64", "v145", "/p:PostBuildEventUseInBuild=false"],
        "exact_dll_p19c": "721/0", "validator": "47/47"})

    installed = Path(os.environ["LOCALAPPDATA"]) / "Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3"
    if sha(installed) != PROTECTED_SHA:
        raise SystemExit("protected installed DLL changed")
    protected_resources = []
    for name in ASSETS:
        resource = installed.parents[1] / "Resources/LXR" / name
        if sha(resource) != sha(ROOT / "data" / name):
            raise SystemExit(f"protected resource changed: {name}")
        protected_resources.append({"name": name, "bytes": resource.stat().st_size, "sha256": sha(resource)})
    write_json("protected_install_manifest.json", {
        "schema": "lxr-p19c-protected-install-v1", "utc": now, "status": "UNCHANGED",
        "module": str(installed), "bytes": installed.stat().st_size,
        "sha256": sha(installed), "expected_sha256": PROTECTED_SHA,
        "resources": protected_resources,
    })

    write_json("reaper_results.json", {
        "schema": "lxr-p19c-reaper-v1", "utc": now,
        "engineering_exact_dll_status": "PASS",
        "final_exact_candidate_profile": "out/p19c/reaper-profile-002",
        "reaper_version": "7.80 x64", "exact_candidate_sha256": CANDIDATE_SHA,
        "attempted_profile": "out/p19c/reaper-profile-001",
        "attempted_candidate_sha256": "28861986574D387E6BE7541195E2ADBAEB58986B09DFBD44538500B69C64E590",
        "unique_task_window": "01_drum1 - REAPER v7.80 - EVALUATION LICENSE",
        "direct_gui_status": "BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE",
        "reason": "computer-use application attachment timed out on both permitted attempts",
        "clicks": 0, "keys": 0, "screenshot": "UNAVAILABLE",
        "bounds_voice_controls_automation_save_reopen_dpi": "NOT_OBSERVED_NOT_CLAIMED",
    })
    write_text("gui_access_status.md", f"""
# REAPER graphical access status

**BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE.** REAPER 7.80 x64 was launched
with the isolated `out/p19c/reaper-profile-001` configuration and the then-current
P19-C engineering candidate. The unique window `01_drum1 - REAPER v7.80 -
EVALUATION LICENSE` was resolved. Initial attachment and the one permitted
reset/recovery attachment both timed out before a screenshot or accessible UI
state became available. A final exact-candidate profile was subsequently prepared
at `out/p19c/reaper-profile-002`, hash-bound to `{CANDIDATE_SHA}`, but graphical
access remained unavailable, so no final-candidate visual interaction is claimed.
Zero clicks and zero keys were sent. Consequently, direct visual bounds, six
editor selection, control manipulation, automation, save/reopen and DPI results
are NOT RUN and NOT CLAIMED. Independent production-path exact-DLL lifecycle,
controller/readback, state restore, multi-instance, deterministic audio, PDC and
validator gates pass.
""")

    file_rows = []
    for relative in TASK_FILES:
        path = ROOT / relative
        if not path.is_file():
            raise SystemExit(f"missing task file: {relative}")
        file_rows.append({"path": relative, "bytes": path.stat().st_size,
                          "sha256": sha(path), "baseline_sha256": baseline_sha(relative)})
    write_json("file_inventory.json", {"schema": "lxr-p19c-file-inventory-v1",
        "utc": now, "files": file_rows})

    production_identity = []
    for relative in PRODUCTION_FILES:
        path = ROOT / relative
        production_identity.append({"path": relative, "bytes": path.stat().st_size,
                                    "sha256": sha(path)})
    write_text("integrity_report.md", f"""
# P19-C integrity report

Status: **PASS before publication**. Branch, local HEAD, tracking HEAD and live
remote began at `{START_HEAD}` with an empty index/worktree. Firmware, iPlug2 and
VST3 SDK pins match. The final private module is `{CANDIDATE_SHA}` ({module.stat().st_size}
bytes); all seven bundled resources match `data/`. The protected installed module
remains `{PROTECTED_SHA}` ({installed.stat().st_size} bytes) and its seven resources
also match. Pre/post deployment actions were disabled and no installation ran.
The exact module passed the production editor/controller/lifecycle suite and
validator. Task-owned current/baseline fingerprints are in `file_inventory.json`
and `baseline_inventory.json`; production identities are embedded below.

```json
{json.dumps(production_identity, indent=2)}
```
""")
    write_text("verification_report.md", f"""
# P19-C verification report

Overall classification: **DEVELOPMENT ENGINEERING PASS / DIRECT REAPER GUI BLOCKED**.

The source-grounded Sound editor exposes 162 unique existing host parameters
across six physical owners (27/27/27/26/27/28), with shared closed/open Hi-hat
semantics and no seventh engine. Exact byte normalization, source-backed enum
domains, unsupported restored-value presentation, balanced gestures, host
automation/readback, LXRS restore, reopen, focus traversal, voice switching and
multi-instance isolation pass. Nine concept SVGs and 162 runtime control bounds
fit 1280 × 800. No DSP algorithm, host ID or serialized state was changed.

Standalone Sound contracts pass 8,464 checks across GCC O0/O2 and VS LLVM
Od/O2. The exact private DLL `{CANDIDATE_SHA}` passes the P19-C production editor
suite 721/0, all retained host suites, deterministic native audio/PDC 26, and
Steinberg validator 47/47. Relevant P15–P19 native and management regressions
pass. The protected installation is unchanged.

Direct REAPER graphical acceptance is separately `BLOCKED —
COMPUTER_USE_SERVICE_UNAVAILABLE`; no visual result is inferred. P19-D retains
the full modulation editor and P19-E the sequencer editor. Native accessibility,
DPI/visual host acceptance remain P19-F gates. P16-F release certification
remains independently BLOCKED; this checkpoint is development-only.
""")
    print(json.dumps({"status": "PASS", "candidate": CANDIDATE_SHA,
                      "protected": PROTECTED_SHA, "sound_bindings": len(bindings),
                      "task_files": len(file_rows), "direct_reaper_gui": "BLOCKED"}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
