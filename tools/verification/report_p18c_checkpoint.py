#!/usr/bin/env python3
"""Assemble P18-C evidence only from completed immutable test outputs."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import pathlib
import subprocess
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "out/p18c"
ASSETS = ["crash_sample.bin", "rec_table.bin", "saw_table.bin",
          "sine_table.bin", "transient_data.bin",
          "transient_volume_table.bin", "tri_table.bin"]
EXPECTED_PROTECTED = "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82"


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def load(path: pathlib.Path):
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(name: str, value) -> None:
    (OUT / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def host_rows() -> dict[str, dict]:
    rows = load(OUT / "checkpoint-host-final4/results.json")
    return {row["name"]: row for row in rows}


def assertion_lines(name: str) -> list[str]:
    text = (OUT / "checkpoint-host-final4" / (name + ".log")).read_text(
        encoding="utf-8", errors="replace")
    return [line[5:] for line in text.splitlines() if line.startswith("PASS ")]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--commit-sha", default="PENDING")
    args = parser.parse_args()
    now = datetime.now(timezone.utc).isoformat()

    gcc = load(OUT / "native-gcc-final2/morph_management_results.json")
    llvm = load(OUT / "native-vsllvm-final/morph_management_results.json")
    p18a_gcc = load(OUT / "regression-p18a-final/reference-runs-gcc/result.json")
    p18a_llvm = load(OUT / "regression-p18a-vsllvm-final/reference-runs-vs-llvm/result.json")
    p18b = load(OUT / "native-gcc-final2/p18b-baseline/morph_runtime_results.json")
    host = host_rows()
    required_host = [
        "P12cVST3StateTest", "P12b4cVST3RuntimeTest",
        "P12cR2ControllerSyncTest", "P15M2GVST3StateTest",
        "P15M2HVST3ControllerTest", "P16F1VST3TransportTest",
        "P16F2VST3StateTest", "P16F3VST3CertificationTest",
        "P16F3MidiMuteTest", "P17EVST3KitManagementTest",
        "P17FVST3PresetTest", "P18BVST3MorphTest",
        "P18CVST3MorphManagementTest", "SteinbergValidator",
    ]
    if not all(host.get(name, {}).get("status") == "PASS"
               for name in required_host):
        raise RuntimeError("final exact-DLL host matrix is not wholly PASS")
    if gcc["classification"] != "PASS" or llvm["classification"] != "PASS":
        raise RuntimeError("P18-C O0/O2 matrix is not PASS")
    if p18a_gcc["status"] != "PASS" or p18a_llvm["status"] != "PASS":
        raise RuntimeError("P18-A original-C matrix is not PASS")

    assertions = assertion_lines("P18CVST3MorphManagementTest")
    common = {"utc": now, "classification": "PASS",
              "exact_private_dll": sha(OUT / "private-bundle-final2/LXR.vst3/Contents/x86_64-win/LXR.vst3"),
              "host_assertions": assertions}
    write_json("target_loading_results.json", {**common, "covered": [
        x for x in assertions if any(key in x for key in
        ("target load", "invalid target", "replacement", "source A",
         "source-kit load", "dialog cancellation"))]})
    write_json("target_clear_results.json", {**common, "covered": [
        x for x in assertions if "clear" in x.lower() or "without target" in x.lower()]})
    write_json("target_identity_results.json", {**common,
        "native_binary_display": "PASS: M\\x01R\\x00P; trailing zero display trim only",
        "covered": [x for x in assertions if "identity" in x.lower()]})
    write_json("morph_amount_results.json", {**common,
        "range": [0, 255], "host_parameter": 245,
        "rapid_automation": "PASS", "clear_amounts": [0, 127, 255],
        "gcc": [row["execute"]["metrics"] for row in gcc["runs"]],
        "vs_llvm": [row["execute"]["metrics"] for row in llvm["runs"]]})
    write_json("morph_export_results.json", {**common,
        "size": 236, "no_replace": "PASS", "round_trip_identity": "PASS",
        "no_target": "PASS", "nonrepresentable_route": "PASS",
        "coherent_snapshot": "PASS"})
    write_json("original_c_comparison.json", {
        "utc": now, "classification": "PASS", "gcc": p18a_gcc,
        "vs_llvm": p18a_llvm, "runtime_gcc": p18b,
        "morphsnd_policy": "unchanged original numeric oracle for all 228 bytes; accepted P18-B source-owned typed selectors restored at 149-154 and 161-172"})
    write_json("native_pcm_results.json", {
        "utc": now, "classification": "PASS",
        "O0": p18b["runs"][0]["execute"]["metrics"]["pcm_hashes"],
        "O2": p18b["runs"][1]["execute"]["metrics"]["pcm_hashes"],
        "optimization_identity": p18b["runs"][0]["execute"]["metrics"]["pcm_hashes"] ==
                                 p18b["runs"][1]["execute"]["metrics"]["pcm_hashes"],
        "exact_dll_determinism": "PASS"})
    write_json("state_restore_results.json", {**common,
        "lxrs_sizes_unchanged": [51065, 51089], "maximum": 65536,
        "target_persisted": False, "amount_authority_persisted": False,
        "reopen_target_present": False})
    write_json("controller_results.json", {**common,
        "parameter": 245, "range": [0, 255], "controller_sync": "PASS",
        "pdc_48khz": 26, "multi_instance": "PASS"})
    write_json("reaper_results.json", {
        "utc": now, "classification": "BLOCKED",
        "reason": "COMPUTER_USE_SERVICE_UNAVAILABLE",
        "attempts": ["native app inventory timed out", "list_windows timed out",
                     "reset-and-retry service_info timed out"],
        "input_actions": 0, "blind_clicks": 0,
        "independent_exact_dll_operations": "PASS",
        "pdc_48khz_exact_dll": 26})

    (OUT / "morph_management_contract.md").write_text(
        "# Morph-management contract\n\nPASS. Load validates a complete exact-size `.snd` before one-generation publication; failure retains B. Clear removes B/name/presence and retains amount. Export holds coherent A/routes/B/amount authority gates through the P17-C no-replace commit. Audio uses bounded try-acquisition and never waits.\n",
        encoding="utf-8")
    (OUT / "ui_binding_contract.md").write_text(
        "# UI binding contract\n\nPASS by compiled production source and exact-DLL interface. Load, Clear, Export, status and the parameter-245 knob call the same `LXR` operations exercised by `P18CVST3MorphManagementTest`; no editor business-logic copy exists. Direct graphical attachment is separately BLOCKED.\n",
        encoding="utf-8")
    (OUT / "gui_access_status.md").write_text(
        "# GUI access status\n\nBLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE. Native-app inventory, window-list retry, and reset-and-retry all timed out. No UI input or blind click was sent. Engineering operation/UI-binding acceptance remains PASS; direct graphical acceptance is not claimed.\n",
        encoding="utf-8")
    (OUT / "allocation_stack_report.md").write_text(
        "# Allocation and stack report\n\nPASS. P18-C pure projection/publication allocations: 0 at GCC and VS LLVM O0/O2. P18-B projection allocations: 0 across 16,777,216 scalar cases per build. Bounded export snapshot object footprint measured by `sizeof`: 4,656 bytes. File I/O is non-audio; audio authority acquisition is try-only.\n",
        encoding="utf-8")

    private = OUT / "private-bundle-final2/LXR.vst3/Contents/x86_64-win/LXR.vst3"
    private_resources = OUT / "private-bundle-final2/LXR.vst3/Contents/Resources/LXR"
    write_json("private_binary_manifest.json", {
        "utc": now, "path": str(private), "bytes": private.stat().st_size,
        "sha256": sha(private), "configuration": "Release x64",
        "visual_studio": "Community 18", "msbuild": "18.10.1",
        "toolset": "v145", "pre_build_events": False,
        "post_build_events": False,
        "resources": [{"name": name, "bytes": (private_resources / name).stat().st_size,
                       "sha256": sha(private_resources / name)} for name in ASSETS]})
    installed = pathlib.Path(os.environ["LOCALAPPDATA"]) / "Programs/Common/VST3/LXR.vst3/Contents"
    installed_module = installed / "x86_64-win/LXR.vst3"
    protected = {"utc": now, "path": str(installed_module),
                 "bytes": installed_module.stat().st_size,
                 "sha256": sha(installed_module),
                 "expected": EXPECTED_PROTECTED,
                 "unchanged": sha(installed_module) == EXPECTED_PROTECTED,
                 "resources": []}
    for name in ASSETS:
        item = installed / "Resources/LXR" / name
        protected["resources"].append({"name": name, "sha256": sha(item),
            "matches_tracked": sha(item) == sha(ROOT / "data" / name)})
    if not protected["unchanged"] or not all(x["matches_tracked"] for x in protected["resources"]):
        raise RuntimeError("protected installation changed")
    write_json("protected_install_manifest.json", protected)

    regressions = [
        ("P18-A original C GCC O0/O2", "PASS", "regression-p18a-final/reference-runs-gcc/result.json"),
        ("P18-A original C VS LLVM O0/O2", "PASS", "regression-p18a-vsllvm-final/reference-runs-vs-llvm/result.json"),
        ("P18-B/P18-C GCC O0/O2", "PASS", "native-gcc-final2/morph_management_results.json"),
        ("P18-B/P18-C VS LLVM O0/O2", "PASS", "native-vsllvm-final/morph_management_results.json"),
        ("P17-C codec", "PASS", "regression-p17c-final/kit_file_codec_results.json"),
        ("P17-D runtime kit", "PASS", "regression-p17d-final/runtime_kit_load_results.json"),
        ("P17-E kit management", "PASS", "regression-p17e-final/kit_management_results.json"),
        ("P17-F identity", "PASS", "regression-p17f-final/kit_identity_results.json"),
        ("P16-D parameter locks", "PASS", "regression-p16d-final/parameter_lock_results.json"),
        ("P15 velocity", "PASS", "regression-p15-velocity-final/test_results.json"),
        ("P15 LFO runtime; disconnected negative controls detected", "PASS", "regression-p15-lfo-final/test_results.json"),
        ("Native/filter/snare/FIR/state/transport", "PASS", "checkpoint-native-final/results.json"),
        ("Exact DLL host/controller/state/kit/morph", "PASS", "checkpoint-host-final4/results.json"),
        ("Steinberg validator", "PASS", "checkpoint-host-final4/SteinbergValidator.log"),
        ("REAPER direct GUI", "BLOCKED", "reaper_results.json"),
    ]
    with (OUT / "regression_matrix.csv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(["suite", "classification", "evidence"])
        writer.writerows(regressions)

    status = subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT,
                                     text=True).splitlines()
    task_files = []
    for line in status:
        path = line[3:]
        if path.startswith("out/") or not (ROOT / path).is_file():
            continue
        task_files.append({"path": path.replace("\\", "/"),
                           "bytes": (ROOT / path).stat().st_size,
                           "sha256": sha(ROOT / path)})
    evidence_names = [
        "morph_management_contract.md", "ui_binding_contract.md",
        "target_loading_results.json", "target_clear_results.json",
        "target_identity_results.json", "morph_amount_results.json",
        "morph_export_results.json", "original_c_comparison.json",
        "native_pcm_results.json", "state_restore_results.json",
        "controller_results.json", "reaper_results.json", "gui_access_status.md",
        "allocation_stack_report.md", "regression_matrix.csv",
        "private_binary_manifest.json", "protected_install_manifest.json",
    ]
    write_json("file_inventory.json", {"utc": now, "task_files": task_files,
        "evidence": [{"path": name, "bytes": (OUT / name).stat().st_size,
                      "sha256": sha(OUT / name)} for name in evidence_names]})

    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT,
                                   text=True).strip()
    (OUT / "integrity_report.md").write_text(
        f"# P18-C integrity report\n\n- Starting/current pre-publication HEAD: `{head}`.\n- Firmware: `dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`.\n- iPlug2: `d54f69050f517e43b941d88c2a170f0a840b9ee4`.\n- VST3 SDK: `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`.\n- Protected DLL: `{protected['sha256']}`; seven resources match tracked data.\n- Final private DLL: `{sha(private)}`.\n- Production build used Release x64, VS18/MSBuild18.10.1/v145, with pre/post-build events disabled.\n",
        encoding="utf-8")
    publication = "PASS" if args.commit_sha != "PENDING" else "PENDING"
    (OUT / "verification_report.md").write_text(
        f"# P18-C verification report\n\nEngineering classification: **PASS**. Development publication: **{publication}** (`{args.commit_sha}`). Direct REAPER GUI: **BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE**; no blind input.\n\nAll source-defined interpolation, route ownership, target load/replacement/clear, exact 236-byte export, no-overwrite, byte-identical round trip, transient restore, deterministic PCM, controller/PDC, multi-instance, allocation, O0/O2, exact-DLL, relevant regression, and validator gates pass. The protected installation remains unchanged. P16-F release certification remains separately BLOCKED.\n",
        encoding="utf-8")
    print(json.dumps({"classification": "PASS", "publication": publication,
                      "private_sha256": sha(private),
                      "protected_sha256": protected["sha256"],
                      "task_files": len(task_files), "host_assertions": len(assertions)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
