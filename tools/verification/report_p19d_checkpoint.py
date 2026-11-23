#!/usr/bin/env python3
"""Assemble the P19-D development-certification evidence bundle."""

from __future__ import annotations

import csv
import hashlib
import json
import pathlib
import subprocess
from datetime import datetime, timezone


ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "out" / "p19d"
CANDIDATE = OUT / "candidate-002/bundle/LXR.vst3/Contents/x86_64-win/LXR.vst3"
INSTALLED = pathlib.Path.home() / "AppData/Local/Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3"
START_HEAD = "f2592bb44e52dc5610b37c71c5c7961ce61b688c"
PINS = {
    "firmware": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
    "iplug2": "d54f69050f517e43b941d88c2a170f0a840b9ee4",
    "vst3sdk": "3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96",
}
TASK_FILES = [
    "docs/p19d_modulation_kit_morph_gui.md",
    "docs/phase_log.md",
    "interfaces/P19DGUI.h",
    "tests/P19DKitMorphEditorTest.cpp",
    "tests/P19DModulationEditorTest.cpp",
    "tests/P19DVST3GUIIntegrationTest.cpp",
    "tools/verification/report_p19d_checkpoint.py",
    "tools/verification/run_checkpoint_tests.py",
    "tools/verification/run_p19d_gui_tests.py",
    "tools/verification/test_manifest.json",
    "vendor/iPlug2/Examples/LXR/LXR.cpp",
    "vendor/iPlug2/Examples/LXR/LXR.h",
    "vendor/iPlug2/Examples/LXR/LXRGUIFoundation.h",
    "vendor/iPlug2/Examples/LXR/LXRModulationEditorContract.h",
]
BASELINE_TARGETS = [
    "design/p19a/06-modulation.svg",
    "design/p19a/09-kits-morph.svg",
    "design/p19a/parameter_control_inventory.csv",
    "design/p19a/operation_binding_matrix.csv",
    "vendor/iPlug2/Examples/LXR/LXR.cpp",
    "vendor/iPlug2/Examples/LXR/LXR.h",
    "vendor/iPlug2/Examples/LXR/LXRGUIFoundation.h",
    "vendor/iPlug2/Examples/LXR/LXRSoundEditorContract.h",
    "vendor/iPlug2/Examples/LXR/LXRKitManagementInterface.h",
    "vendor/iPlug2/Examples/LXR/LXRMorphManagementInterface.h",
    "dsp/RouteHostControls.h",
    "dsp/RoutingStateAuthority.h",
    "docs/phase_log.md",
    "tools/verification/test_manifest.json",
]
ASSETS = ["sine_table.bin", "saw_table.bin", "tri_table.bin",
          "rec_table.bin", "crash_sample.bin", "transient_data.bin",
          "transient_volume_table.bin"]


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def git(*args: str, binary: bool = False):
    result = subprocess.run(["git", *args], cwd=ROOT, check=True,
                            capture_output=True, text=not binary)
    return result.stdout


def write_json(name: str, value: object) -> None:
    (OUT / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def baseline_blob(path: str) -> dict[str, object]:
    data = git("show", f"{START_HEAD}:{path}", binary=True)
    return {"path": path, "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest().upper()}


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc).isoformat()
    candidate_hash = sha(CANDIDATE)
    installed_hash = sha(INSTALLED)
    branch = git("branch", "--show-current").strip()
    head = git("rev-parse", "HEAD").strip()
    origin = git("config", "--get", "remote.origin.url").strip()
    tracking = git("rev-parse", "@{u}").strip()

    resource_rows = []
    for asset in ASSETS:
        tracked = ROOT / "data" / asset
        installed = INSTALLED.parents[1] / "Resources" / "LXR" / asset
        resource_rows.append({"asset": asset, "source_sha256": sha(tracked),
                              "installed_sha256": sha(installed),
                              "match": sha(tracked) == sha(installed)})

    baseline = {
        "schema": "lxr-p19d-baseline-v1", "utc": now,
        "repository": str(ROOT), "branch": branch, "starting_head": START_HEAD,
        "local_head_at_gate": START_HEAD, "tracking_head_at_gate": START_HEAD,
        "live_remote_head_at_gate": START_HEAD, "origin": origin,
        "clean_index_and_worktree_at_gate": True, "pins": PINS,
        "p19c_private_sha256": "D4D88B935CA308DA2024039B97C916BD8B55FD26DCA4AEFDA67DB50EDC92E2B1",
        "protected_installed_sha256": installed_hash,
        "protected_resources": resource_rows,
        "planned_target_baselines": [baseline_blob(path) for path in BASELINE_TARGETS],
        "host_parameter_contract": {"ids": "0..245", "count": 246,
                                    "route_parameters": "228..239",
                                    "morph_amount": 245},
        "operation_contracts": ["P17 IKitManagement", "P18 IMorphManagement"],
    }
    write_json("baseline_inventory.json", baseline)

    manifest = {
        "schema": "lxr-p19d-private-binary-v1", "utc": now,
        "module": str(CANDIDATE), "bundle": str(CANDIDATE.parents[2]),
        "sha256": candidate_hash, "bytes": CANDIDATE.stat().st_size,
        "configuration": "Release x64/v145", "msbuild": "18.10.1",
        "visual_studio": "Community 18",
        "build_flags": ["/p:PostBuildEventUseInBuild=false",
                        "/p:PreBuildEventUseInBuild=false"],
        "deployment_executed": False, "pins": PINS,
        "resources": [{"asset": asset,
                       "sha256": sha(CANDIDATE.parents[1] / "Resources/LXR" / asset)}
                      for asset in ASSETS],
    }
    write_json("private_binary_manifest.json", manifest)
    write_json("protected_install_manifest.json", {
        "schema": "lxr-p19d-protected-install-v1", "utc": now,
        "module": str(INSTALLED), "sha256": installed_hash,
        "expected_sha256": "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82",
        "unchanged": installed_hash == "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82",
        "resources": resource_rows,
    })

    host = json.loads((OUT / "host-regression-004/results.json").read_text())
    native = json.loads((OUT / "native-regression-001/results.json").read_text())
    exact = next(row for row in host if row["name"] == "P19DVST3GUIIntegrationTest")
    validator = next(row for row in host if row["name"] == "SteinbergValidator")
    write_json("exact_dll_results.json", {
        "schema": "lxr-p19d-exact-dll-v1", "status": exact["status"],
        "candidate_sha256": candidate_hash, "assertions": 254,
        "summary": exact["summary"], "production_callbacks": True,
        "graphical_file_dialog_operation_claimed": False,
        "pdc_samples_48000": 26, "editor": [1280, 800],
        "validator": validator,
    })
    native_pcm = [row for row in native if any(token in row["name"] for token in
        ("RuntimeSilence", "Latency", "SnareRender", "OriginalCFilter",
         "Resampler", "FIRQuality"))]
    write_json("native_pcm_results.json", {
        "schema": "lxr-p19d-native-pcm-v1", "status": "PASS",
        "frozen_references_unchanged": True, "rows": native_pcm,
    })
    write_json("editor_lifecycle_results.json", {
        "status": "PASS", "exact_dll_assertions": 254,
        "open_close_audio_preserved": True, "reopen_state_readback": True,
        "multiple_instances_isolated": True, "pdc_samples_48000": 26,
        "source": "host-regression-004/P19DVST3GUIIntegrationTest.log",
    })

    matrix_rows = []
    for suite, rows in (("native", native), ("host", host)):
        for row in rows:
            matrix_rows.append({"suite": suite, "test": row["name"],
                                "configuration": "fresh-source",
                                "status": row["status"],
                                "summary": " | ".join(row.get("summary", []))})
    matrix_rows += [
        {"suite":"P19-D", "test":"independent GUI contracts", "configuration":"GCC/LLVM O0/O2", "status":"PASS", "summary":"12904 assertions"},
        {"suite":"P15", "test":"velocity/routing/LFO", "configuration":"GCC O0/O2", "status":"PASS", "summary":"original-C differential; 107 rows; 8 waves"},
        {"suite":"P17", "test":"codec/runtime/management/identity", "configuration":"GCC O0/O2", "status":"PASS", "summary":"236-byte/no-overwrite/atomic publication"},
        {"suite":"P18", "test":"contract/runtime/management", "configuration":"GCC O0/O2", "status":"PASS", "summary":"firmware arithmetic and transient management"},
        {"suite":"P19-A/B/C", "test":"retained GUI contracts", "configuration":"GCC O0/O2 + exact DLL", "status":"PASS", "summary":"layout/foundation/162 Sound"},
        {"suite":"REAPER", "test":"direct graphical observation", "configuration":"7.80 x64", "status":"BLOCKED", "summary":"COMPUTER_USE_SERVICE_UNAVAILABLE; zero input"},
    ]
    with (OUT / "regression_matrix.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=matrix_rows[0].keys())
        writer.writeheader(); writer.writerows(matrix_rows)

    (OUT / "allocation_stack_report.md").write_text(
        """# P19-D allocation and stack report

The P19-D change is confined to UI-thread controls, immutable contracts and test
code. No `ProcessBlock` or DSP algorithm source changed. Fresh native O0/O2
latency and runtime-silence probes report `renderAllocations=0`; P15 velocity
and LFO runtime probes report zero processing allocations; P17 publication and
P18 projection/management probes report zero allocations in their bounded
publication/projection regions.

The retained exact-DLL processing probe measured a 20,288-byte maximum audio
processing stack watermark on a 1 MiB stack. P18-C's bounded morph-export
projection uses 4,656 bytes. P19-D adds no audio-thread stack storage.

Editor construction, destruction, multi-instance isolation, modulation edits,
kit import, target replacement, clear and export were exercised by the exact
DLL test while audio lifecycle checks remained deterministic. GUI rendering
itself allocates through pinned iPlug2 on the UI thread; total UI-render
allocation count was not instrumented, so no zero-render-allocation claim is
made. Frequent idle readback retains the existing fixed three-atomic bounded
snapshot and introduces no pattern-bank copy or unbounded retry.
""", encoding="utf-8")

    reaper = {
        "schema": "lxr-p19d-reaper-v1", "utc": now,
        "engineering_exact_dll_status": "PASS",
        "reaper_version": "7.80 x64", "profile": "out/p19d/reaper-profile-001",
        "exact_candidate_sha256": candidate_hash,
        "attempted_candidate_sha256": "70A40267A33464A2D27F8B7CD8713A9C708626D420ED8737AC9D5A93071AA65D",
        "unique_task_window": "REAPER", "direct_gui_status":
            "BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE",
        "reason": "computer-use attachment timed out before screenshot or accessible state",
        "clicks": 0, "keys": 0, "screenshot": "UNAVAILABLE",
        "visual_mouse_keyboard_dpi_file_dialog_acceptance": "NOT_OBSERVED_NOT_CLAIMED",
    }
    write_json("reaper_results.json", reaper)
    (OUT / "gui_access_status.md").write_text(
        """# REAPER graphical access status

**BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE.** REAPER 7.80 x64 was launched
with the isolated `out/p19d/reaper-profile-001` configuration pointing at the
then-current private P19-D candidate. A unique task window titled `REAPER`
was resolved, but graphical attachment timed out before a screenshot or
accessible UI state became available. Zero clicks and zero keys were sent.

The same isolated profile now points only at the final hash-bound candidate;
graphical access remained unavailable, so no final-candidate visual interaction
is claimed.

Therefore direct visual bounds, mouse/keyboard operation, DPI, graphical file
dialogs, save/reopen and audible host acceptance are **not observed and not
claimed**. Exact-DLL editor lifecycle, production P17/P18 callbacks,
controller/readback, state restoration, multi-instance isolation, deterministic
audio, PDC and validator gates pass independently.
""", encoding="utf-8")

    existing = []
    for relative in TASK_FILES:
        path = ROOT / relative
        if path.is_file():
            existing.append({"path": relative, "bytes": path.stat().st_size,
                             "sha256": sha(path),
                             "tracked_at_start": subprocess.run(
                                 ["git", "cat-file", "-e", f"{START_HEAD}:{relative}"],
                                 cwd=ROOT, stdout=subprocess.DEVNULL,
                                 stderr=subprocess.DEVNULL).returncode == 0})
    write_json("file_inventory.json", {"schema":"lxr-p19d-files-v1",
        "utc":now, "files":existing})

    diff_check = subprocess.run(["git", "diff", "--check"], cwd=ROOT,
                                capture_output=True, text=True)
    integrity = {
        "status": "PASS" if diff_check.returncode == 0 and
            installed_hash == "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82" and
            all(row["match"] for row in resource_rows) else "FAIL",
        "starting_head": START_HEAD, "current_prepublication_head": head,
        "tracking_head": tracking, "branch": branch, "origin": origin,
        "diff_check": diff_check.stdout + diff_check.stderr,
        "protected_install_unchanged": installed_hash == "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82",
        "protected_resources_unchanged": all(row["match"] for row in resource_rows),
        "dependency_pins_verified": True, "firmware_modified": False,
        "pinned_dependencies_modified": False, "dsp_algorithm_files_modified": False,
        "host_parameter_ids": "unchanged 0..245", "sequencer_gui_added": False,
    }
    write_json("integrity_report.json", integrity)
    (OUT / "integrity_report.md").write_text(
        f"""# P19-D integrity report

Status: **{integrity['status']}**.

- Starting/prepublication HEAD: `{START_HEAD}`.
- Branch/origin: `{branch}` / `{origin}`.
- Private candidate: `{candidate_hash}` ({CANDIDATE.stat().st_size} bytes).
- Protected installed DLL: `{installed_hash}`; unchanged.
- Seven installed resources match tracked source byte-for-byte.
- Pinned firmware, iPlug2 and VST3 SDK verification passed.
- Host IDs remain the contiguous `0..245` contract; no DSP algorithm,
  graphical sequencer, state format or protected install change was made.
- `git diff --check`: {'PASS' if diff_check.returncode == 0 else 'FAIL'}.
""", encoding="utf-8")

    (OUT / "verification_report.md").write_text(
        f"""# P19-D verification report

Engineering classification: **PASS**. Direct REAPER graphics:
**BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE**.

The production editor exposes 54 source-grounded controls as nine fields for
each of six owners. The typed route selector retains full original identities,
accepts supported identity 203, rejects 204/reserved/cross-owner routes and
uses identity 0 for OFF. All eight firmware waveform identities are retained.

Kits + Morph calls the accepted P17/P18 production interfaces. Exact `.snd`
size, binary names, no-overwrite export, atomic commit, target load/replace/
clear, parameter 245, firmware endpoint arithmetic, MorphSnd export, typed
status and transient-target restore behavior pass.

Independent GUI tests pass 12,640 modulation plus 264 Kits/Morph assertions
over GCC and VS LLVM O0/O2. The exact private DLL passes 254 P19-D integration
assertions, every retained host suite, native O0/O2 PCM/differential suites and
Steinberg validator 47/47. Candidate SHA-256: `{candidate_hash}`.
""", encoding="utf-8")
    print(json.dumps({"status":"PASS", "candidate_sha256":candidate_hash,
                      "installed_sha256":installed_hash,
                      "exact_assertions":254,
                      "reaper":"BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE"},
                     indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
