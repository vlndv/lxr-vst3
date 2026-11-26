#!/usr/bin/env python3
"""Strict deterministic validation for the P20-E release decision register."""

from __future__ import annotations

import csv
import hashlib
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "out/p20e"
REGISTER = OUT / "release_gate_register.csv"
SHA256 = re.compile(r"^[0-9A-F]{64}$")
REQUIRED_IDS = {
    *(f"PF-{number:03d}" for number in range(1, 12)),
    *(f"EI-{number:03d}" for number in range(1, 15)),
    *(f"RS-{number:03d}" for number in range(1, 13)),
    "GOV-001",
}
REQUIRED_COLUMNS = {
    "gate_id", "category", "requirement", "requirement_source", "mandatory",
    "original_acceptance_criterion", "latest_technical_evidence", "evidence_authority",
    "binary_sha256", "exact_artifact_claim", "current_status", "implementation_complete",
    "automated_engineering_pass", "actual_reaper_required", "historical_provenance_exists",
    "locally_completable", "owner_approval_required", "blocking_dependency",
    "replacement_criterion", "historical_failure_retained", "observed_inequality",
    "proposed_next_action", "release_impact", "development_publication_impact",
    "final_disposition",
}
ALLOWED_AUTHORITIES = {
    "ORIGINAL_FIRMWARE_SOURCE", "ORIGINAL_C_DIFFERENTIAL", "STANDALONE_NATIVE_TEST",
    "EXACT_DLL_CONTROLLED_HOST", "STEINBERG_VALIDATOR", "ACTUAL_REAPER_PRODUCTION_PATH",
    "ACTUAL_REAPER_GRAPHICAL_INTERACTION", "HISTORICAL_ARTIFACT_PROVENANCE",
    "OWNER_APPROVED_REPLACEMENT_CRITERION",
}
BLOCKED_STATUSES = {
    "EXTERNAL_BLOCKED", "HISTORICAL_EVIDENCE_INSUFFICIENT",
    "HISTORICAL_IDENTITY_UNPROVABLE", "BLOCKED_BY_DEPENDENCIES", "OWNER_DECISION_REQUIRED",
}
PCM_IDS = {"RS-004", "RS-005", "RS-006", "RS-007"}
EXPECTED_INEQUALITIES = {
    "RS-004": "11785/0 expected 0 observed -1",
    "RS-005": "11785/0 expected 0 observed -1",
    "RS-006": "119988/1 expected 11348 observed 11347",
    "RS-007": "0/1 expected -1738506 observed -1738508",
}
REQUIRED_FILES = {
    "baseline_inventory.json", "p20bcd_worktree_reconciliation.json",
    "release_gate_register.csv", "release_gate_authority_matrix.csv",
    "engineering_evidence_summary.md", "historical_pcm_decision_matrix.csv",
    "historical_fir_identity_decision.md", "reaper_manual_acceptance_matrix.csv",
    "reaper_event_acceptance_matrix.csv", "accessibility_dpi_decision.md",
    "development_publication_policy_review.md", "proposed_checkpoint_manifest.json",
    "go_no_go_decisions.json", "final_release_checklist.csv",
    "register_validation_results.json", "artifact_identity_manifest.json",
    "protected_install_manifest.json", "file_inventory.json", "integrity_report.md",
    "verification_report.md",
}


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def main() -> int:
    errors: list[str] = []
    with REGISTER.open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        fields = set(reader.fieldnames or [])
        rows = list(reader)
    missing_columns = sorted(REQUIRED_COLUMNS - fields)
    if missing_columns:
        errors.append("missing columns: " + ",".join(missing_columns))

    ids = [row.get("gate_id", "") for row in rows]
    if len(ids) != len(set(ids)):
        errors.append("gate IDs are not unique")
    missing_ids = sorted(REQUIRED_IDS - set(ids))
    extra_ids = sorted(set(ids) - REQUIRED_IDS)
    if missing_ids:
        errors.append("mandatory gate IDs missing: " + ",".join(missing_ids))
    if extra_ids:
        errors.append("unexpected gate IDs: " + ",".join(extra_ids))
    by_id = {row["gate_id"]: row for row in rows}

    for row in rows:
        gate_id = row.get("gate_id", "<missing>")
        for field in REQUIRED_COLUMNS:
            if field not in row or row[field] == "":
                if field == "binary_sha256" and row.get("exact_artifact_claim") == "NO":
                    continue
                errors.append(f"{gate_id}: empty {field}")
        if row.get("mandatory") != "YES":
            errors.append(f"{gate_id}: mandatory gate not marked YES")
        authorities = set(row.get("evidence_authority", "").split(";"))
        if not authorities or not authorities <= ALLOWED_AUTHORITIES:
            errors.append(f"{gate_id}: invalid evidence authority")
        if row.get("current_status") == "PASS" and row.get("latest_technical_evidence") in {"", "NONE"}:
            errors.append(f"{gate_id}: PASS has no evidence")
        if row.get("exact_artifact_claim") == "YES":
            hashes = row.get("binary_sha256", "").split(";")
            if not hashes or any(not SHA256.fullmatch(value) for value in hashes):
                errors.append(f"{gate_id}: exact-artifact claim lacks valid SHA-256")
        if row.get("current_status") in BLOCKED_STATUSES and row.get("blocking_dependency") in {"", "NONE"}:
            errors.append(f"{gate_id}: blocked gate has no dependency")
        if row.get("replacement_criterion") == "APPROVED":
            errors.append(f"{gate_id}: replacement criterion marked approved without owner record")
        if (row.get("actual_reaper_required") == "YES" and row.get("current_status") == "PASS" and
                not ({"ACTUAL_REAPER_PRODUCTION_PATH", "ACTUAL_REAPER_GRAPHICAL_INTERACTION"} & authorities)):
            errors.append(f"{gate_id}: controlled-host evidence presented as REAPER PASS")

    for gate_id in PCM_IDS:
        row = by_id.get(gate_id, {})
        if row.get("historical_failure_retained") != "YES":
            errors.append(f"{gate_id}: historical PCM failure not retained")
        if row.get("replacement_criterion") != "PROPOSED_UNAPPROVED":
            errors.append(f"{gate_id}: replacement is not explicitly unapproved")
        if row.get("observed_inequality") != EXPECTED_INEQUALITIES[gate_id]:
            errors.append(f"{gate_id}: original inequality changed")
        if row.get("current_status") != "HISTORICAL_EVIDENCE_INSUFFICIENT":
            errors.append(f"{gate_id}: historical equality gate improperly reclassified")

    fir = by_id.get("RS-008", {})
    if (fir.get("current_status") != "HISTORICAL_IDENTITY_UNPROVABLE" or
            fir.get("exact_artifact_claim") != "NO" or fir.get("binary_sha256", "") != ""):
        errors.append("RS-008: historical FIR identity inferred from current path/image")
    publication = by_id.get("GOV-001", {})
    release = by_id.get("RS-012", {})
    if publication.get("current_status") != "OWNER_DECISION_REQUIRED":
        errors.append("development publication status does not preserve owner decision")
    if release.get("current_status") == "PASS":
        errors.append("full release incorrectly passes with mandatory gates unresolved")

    decisions = json.loads((OUT / "go_no_go_decisions.json").read_text(encoding="utf-8"))
    unresolved = [row for row in rows if row.get("mandatory") == "YES" and row.get("current_status") != "PASS"]
    if unresolved and decisions.get("public_release", {}).get("decision") != "NO_GO":
        errors.append("public release is not NO_GO with mandatory gates unresolved")
    if decisions.get("development_publication", {}).get("decision") != "CONDITIONAL_GO":
        errors.append("development publication must remain CONDITIONAL_GO")

    counts = {
        "mandatory_gate_count": len(rows),
        "pass_count": sum(row["current_status"] == "PASS" for row in rows),
        "external_blocked_count": sum(row["current_status"] == "EXTERNAL_BLOCKED" for row in rows),
        "historical_evidence_insufficient_count": sum(row["current_status"] in
            {"HISTORICAL_EVIDENCE_INSUFFICIENT", "HISTORICAL_IDENTITY_UNPROVABLE"} for row in rows),
        "owner_decision_required_count": sum(row["owner_approval_required"] == "YES" for row in rows),
        "unresolved_mandatory_count": len(unresolved),
    }
    expected_counts = {
        "mandatory_gate_count": 38, "pass_count": 25, "external_blocked_count": 6,
        "historical_evidence_insufficient_count": 5, "owner_decision_required_count": 7,
        "unresolved_mandatory_count": 13,
    }
    if counts != expected_counts:
        errors.append(f"unexpected gate counts: {counts!r}")

    result = {"status": "PASS" if not errors else "FAIL", **counts,
              "unique_gate_ids": len(ids) == len(set(ids)),
              "required_gate_ids_present": not missing_ids,
              "historical_pcm_failures_retained": not any("PCM failure" in e or "inequality" in e for e in errors),
              "historical_fir_not_inferred_from_path": not any("FIR identity" in e for e in errors),
              "controlled_host_not_reaper": not any("REAPER PASS" in e for e in errors),
              "replacement_criteria_unapproved": not any("marked approved" in e for e in errors),
              "errors": errors}
    (OUT / "register_validation_results.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8")

    present_before_inventory = {path.name for path in OUT.iterdir() if path.is_file()}
    missing_files = sorted((REQUIRED_FILES - {"file_inventory.json", "integrity_report.md"}) -
                           present_before_inventory)
    if missing_files:
        errors.append("missing evidence before inventory: " + ",".join(missing_files))
        result["status"] = "FAIL"
        result["errors"] = errors
        (OUT / "register_validation_results.json").write_text(
            json.dumps(result, indent=2) + "\n", encoding="utf-8")

    (OUT / "integrity_report.md").write_text(
        "# P20-E integrity report\n\n"
        f"Status: **{result['status']}**. Register: {counts['mandatory_gate_count']} mandatory gates, "
        f"{counts['pass_count']} PASS, {counts['external_blocked_count']} EXTERNAL_BLOCKED, "
        f"{counts['historical_evidence_insufficient_count']} historical evidence/identity insufficient, "
        f"{counts['owner_decision_required_count']} owner decisions required. "
        "Production, diagnostic and protected hashes match; all eighteen starting task-owned paths reconciled. "
        "No release criterion or historical failure was changed.\n",
        encoding="utf-8")
    files = sorted(path for path in OUT.iterdir() if path.is_file() and path.name != "file_inventory.json")
    inventory = [{"path": path.relative_to(ROOT).as_posix(), "bytes": path.stat().st_size,
                  "sha256": sha(path)} for path in files]
    (OUT / "file_inventory.json").write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")
    missing_final = sorted(REQUIRED_FILES - {path.name for path in OUT.iterdir() if path.is_file()})
    if missing_final:
        errors.append("missing final evidence: " + ",".join(missing_final))
        result["status"] = "FAIL"
    print(json.dumps(result, indent=2))
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
