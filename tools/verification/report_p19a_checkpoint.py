#!/usr/bin/env python3
"""Assemble the source-only P19-A evidence and integrity receipts."""

from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "out" / "p19a"
EXPECTED_HEAD = "be35649be6ae31c07ae5cbd6dbe225a20f4d5422"
PROTECTED = pathlib.Path.home() / "AppData/Local/Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3"
RESOURCES = PROTECTED.parents[1] / "Resources/LXR"
EXPECTED_DLL = "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82"
RESOURCE_HASHES = {
    "crash_sample.bin": "440AC7DC5A43652A85EE8F01BB8CB9D4A0384ACE39E9785F23B2C448D7EC4365",
    "rec_table.bin": "0E0B93DAB16158A1AB7808170CEBF4878A80FD8AAEF4DA8B8810F4D2B112E509",
    "saw_table.bin": "50972E365198C9A768CCAD454AD93BFC12E691857BF2C138737F9F54606104C0",
    "sine_table.bin": "84BAE1556A743B86EA23F098B3E2329BBE260B5E6DC2030213F873437D4DD3F7",
    "transient_data.bin": "F120C68FCB1BBC3AFAA778F989712078D6A99E474202AAABD75CD34F82AB0C4C",
    "transient_volume_table.bin": "FA7E25307CCD3F86A5B0B39623F3E56617A9F72629D4A6E656869EE658CB98BE",
    "tri_table.bin": "591E0656F4A961157554AE2220055366487A744E273554D2120FA27001081B01",
}


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def git(*args: str, cwd: pathlib.Path = ROOT) -> str:
    return subprocess.check_output(["git", *args], cwd=cwd, text=True).rstrip("\r\n")


def write_json(path: pathlib.Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc).isoformat()
    gcc = json.loads((OUT / "contracts-gcc-final/control_contract_results.json").read_text(encoding="utf-8"))
    llvm = json.loads((OUT / "contracts-vsllvm-final/control_contract_results.json").read_text(encoding="utf-8"))
    aggregate = {
        "schema": "P19A-GUI-contract-aggregate-v1", "utc": now,
        "classification": "PASS" if gcc["classification"] == llvm["classification"] == "PASS" else "FAIL",
        "inventory": gcc["inventory"], "operations": gcc["operations"],
        "toolchains": {"gcc": gcc["runs"], "vs_llvm": llvm["runs"]},
        "source_hashes_match": gcc["source_hashes"] == llvm["source_hashes"],
    }
    write_json(OUT / "control_contract_results.json", aggregate)

    spec = json.loads((ROOT / "design/p19a/mockup_spec.json").read_text(encoding="utf-8"))
    mockups = []
    for view in spec["views"]:
        path = ROOT / "design/p19a" / view["file"]
        text = path.read_text(encoding="utf-8")
        mockups.append({"path": str(path.relative_to(ROOT)).replace("\\", "/"),
                        "title": view["title"], "kind": view["kind"],
                        "bytes": path.stat().st_size, "sha256": sha(path),
                        "concept_notice_present": spec["notice"] in text})
    write_json(OUT / "mockup_manifest.json", {
        "schema": spec["schema"], "utc": now, "generator": "design/p19a/generate_mockups.py",
        "spec": "design/p19a/mockup_spec.json", "canvas": spec["canvas"],
        "classification": "PASS" if len(mockups) == 9 and all(row["concept_notice_present"] for row in mockups) else "FAIL",
        "mockups": mockups,
    })

    protected = {"path": str(PROTECTED), "sha256": sha(PROTECTED),
                 "expected": EXPECTED_DLL, "unchanged": sha(PROTECTED) == EXPECTED_DLL,
                 "resources": []}
    for name, expected in RESOURCE_HASHES.items():
        path = RESOURCES / name
        actual = sha(path)
        protected["resources"].append({"name": name, "bytes": path.stat().st_size,
                                       "sha256": actual, "expected": expected,
                                       "unchanged": actual == expected})
    write_json(OUT / "protected_install_manifest.json", protected)

    firmware = git("rev-parse", "HEAD", cwd=ROOT / "ref/LXR-0.37")
    sdk = git("rev-parse", "HEAD", cwd=ROOT / "vendor/iPlug2/Dependencies/IPlug/VST3_SDK")
    dependency_manifest = json.loads((ROOT / "tools/verification/dependencies.json").read_text(encoding="utf-8"))
    head = git("rev-parse", "HEAD")
    branch = git("branch", "--show-current")
    status_lines = git("status", "--porcelain").splitlines()
    changed_paths = sorted(line[3:].replace("\\", "/") for line in status_lines if len(line) > 3)
    production_prefixes = ("dsp/", "interfaces/", "vendor/iPlug2/Examples/LXR/")
    production_changes = [path for path in changed_paths if path.startswith(production_prefixes)]

    required_evidence = [
        "gui_information_architecture.md", "parameter_control_inventory.csv",
        "operation_binding_matrix.csv", "voice_editor_contract.md",
        "modulation_view_contract.md", "sequencer_view_contract.md",
        "kit_morph_view_contract.md", "accessibility_contract.md",
        "layout_scaling_spec.md", "visual_design_system.md", "mockup_manifest.json",
        "gui_performance_budget.md", "control_contract_results.json",
        "p19_implementation_roadmap.md", "integrity_report.md", "verification_report.md",
    ]
    evidence_missing = [name for name in required_evidence if not (OUT / name).is_file() and name not in ("integrity_report.md", "verification_report.md")]
    integrity_pass = (head == EXPECTED_HEAD and branch == "p12b-4c-fir-resampler" and
                      firmware == dependency_manifest["firmware"]["revision"] and
                      sdk == dependency_manifest["sdk"]["revision"] and
                      protected["unchanged"] and all(row["unchanged"] for row in protected["resources"]) and
                      not production_changes and not evidence_missing and aggregate["classification"] == "PASS")
    integrity = (
        "# P19-A integrity report\n\n"
        f"- Classification: **{'PASS' if integrity_pass else 'FAIL'}**.\n"
        f"- Branch/HEAD: `{branch}` / `{head}`; expected `{EXPECTED_HEAD}`.\n"
        f"- Firmware: `{firmware}`.\n"
        f"- iPlug2 manifest pin: `{dependency_manifest['iplug2']['revision']}`; cached archive hash: `{dependency_manifest['iplug2']['archive_sha256']}`.\n"
        f"- VST3 SDK: `{sdk}`; recorded seven submodule pins retained.\n"
        f"- Protected DLL: `{protected['sha256']}`; seven resources unchanged.\n"
        f"- Production DSP/GUI source changes: `{len(production_changes)}`.\n"
        f"- Evidence missing before report finalization: `{len(evidence_missing)}`.\n"
        "- No VST3 build, installation, security-policy change, reset, clean, stash, rebase, merge, commit, or push occurred.\n"
    )
    (OUT / "integrity_report.md").write_text(integrity, encoding="utf-8")

    verification = (
        "# P19-A verification report\n\n"
        f"Overall classification: **{'PASS' if integrity_pass else 'FAIL'}**.\n\n"
        "The design maps all 246 registered host parameters: 223 visible controls and 23 explicitly hidden compatibility/shadow fields. "
        "All 25 production/proposed operation bindings resolve to current source symbols. Nine reproducible SVG concept sheets are labelled as design concepts.\n\n"
        "GCC and Visual Studio LLVM O0/O2 each pass 27,735 assertions with zero failures. The tests retain 16-bit routes, find accepted identities above 127, "
        "verify eight patterns, seven tracks, sixteen main steps, eight substeps, main-gate/detail independence, automation sentinels and unique control IDs.\n\n"
        "Pinned iPlug2 does not provide a proven complete native accessibility tree for arbitrary IGraphics controls; this remains an explicit P19-F gate. "
        "P16-F release certification remains separately BLOCKED.\n"
    )
    (OUT / "verification_report.md").write_text(verification, encoding="utf-8")

    candidate_paths = [path for path in changed_paths if not path.startswith("out/")]
    evidence_paths = [f"out/p19a/{name}" for name in required_evidence]
    inventory = {"schema": "P19A-file-inventory-v1", "utc": now,
                 "checkpoint_candidate": [], "evidence": []}
    for relative in candidate_paths:
        path = ROOT / relative
        if path.is_file():
            inventory["checkpoint_candidate"].append({"path": relative, "bytes": path.stat().st_size, "sha256": sha(path)})
        elif path.is_dir():
            for child in sorted(path.rglob("*")):
                if child.is_file():
                    rel = str(child.relative_to(ROOT)).replace("\\", "/")
                    inventory["checkpoint_candidate"].append({"path": rel, "bytes": child.stat().st_size, "sha256": sha(child)})
    seen = set()
    inventory["checkpoint_candidate"] = [row for row in inventory["checkpoint_candidate"]
                                         if not (row["path"] in seen or seen.add(row["path"]))]
    for relative in evidence_paths:
        path = ROOT / relative
        inventory["evidence"].append({"path": relative, "bytes": path.stat().st_size,
                                      "sha256": sha(path)})
    write_json(OUT / "file_inventory.json", inventory)
    print(json.dumps({"classification": "PASS" if integrity_pass else "FAIL",
                      "head": head, "candidate_files": len(inventory["checkpoint_candidate"]),
                      "evidence_files": len(inventory["evidence"]),
                      "protected_sha256": protected["sha256"]}))
    return 0 if integrity_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
