"""Reconcile preserved PCM and assemble P20-B evidence from local run records."""

import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess
import wave

from build_p16f3_candidate import protected

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "out/p20b"
START = "0155e153ae34ff2fb5740ea7bfe6855ef5d1c220"
BASE = "out/p19f/candidate-certified"


def read(path):
    return json.loads((ROOT / path).read_text(encoding="utf-8-sig"))


def write(name, value):
    (OUT / f"{name}.json").write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def sha(path):
    return digest(Path(path).read_bytes())


def inventory(path):
    return {"path": str(path.relative_to(ROOT)), "bytes": path.stat().st_size, "sha256": sha(path)}


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def write_csv(name, rows):
    with (OUT / f"{name}.csv").open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def compare_pcm(row):
    def pcm(side):
        path = ROOT / row[f"{side}_file"]
        if sha(path) != row[f"{side}_file_sha256"]:
            raise ValueError(f"Historical WAV hash differs: {path}")
        with wave.open(str(path), "rb") as stream:
            channels, width = stream.getnchannels(), stream.getsampwidth()
            assert (channels, width * 8, stream.getframerate(), stream.getnframes()) == tuple(
                int(row[key]) for key in ("channels", "pcm_bits", "sample_rate", "frames")
            )
            data = stream.readframes(stream.getnframes())
        assert digest(data) == row[f"{side}_pcm_sha256"]
        return data, channels, width

    for key in ("input_fixture", "other_fixture"):
        assert sha(ROOT / row[key]) == row[f"{key}_sha256"]

    reference, channels, width = pcm("reference")
    actual, _, _ = pcm("candidate")
    first = None
    count = peak = last = 0
    for offset in range(0, len(reference), width):
        x, y = reference[offset:offset + width], actual[offset:offset + width]
        if x != y:
            expected, observed = (int.from_bytes(v, "little", signed=True) for v in (x, y))
            frame, channel = divmod(offset // width, channels)
            if first is None:
                first = [frame, channel, expected, observed]
            peak = max(peak, abs(observed - expected))
            count += 1
            last = frame
    assert first == [int(row[key]) for key in (
        "first_differing_frame", "first_differing_channel", "expected_integer_value", "actual_integer_value"
    )]
    assert (count, peak, last) == tuple(int(row[key]) for key in (
        "differing_scalar_samples", "maximum_absolute_integer_difference", "last_differing_frame"
    ))
    return dict(test_id=row["test_id"], raw_status="FAIL", disposition="UNRESOLVED",
                earliest_observed_host_frame=first[0], channel=first[1],
                expected=first[2], observed=first[3], differing_scalar_samples=count,
                max_absolute_difference=peak, last_host_frame=last,
                original_binary_sha256=row["original_binary_sha256"],
                original_source_fingerprint=row["original_source_fingerprint"],
                earliest_native_sample="NOT RECORDED",
                historical_replay="Original transient state and complete callback history unavailable",
                rationale=row["f12_case_reasoning"])


def recorded_manifest(path):
    data = read(path)
    path = ROOT / path
    module = Path(data["module"])
    assert sha(module) == data["sha256"] and module.stat().st_size == data["bytes"]
    resources = module.parents[1] / "Resources/LXR"
    actual = {p.name: sha(p) for p in resources.iterdir() if p.is_file()}
    assert actual == {item["name"]: item["sha256"] for item in data["resources"]} and len(actual) == 7
    source = read(path.parent / "source_manifest.json")
    fingerprint = digest("".join(x["path"] + "\0" + x["sha256"] + "\n" for x in source["files"]).encode())
    assert fingerprint == source["fingerprint"] == data["source_fingerprint"]
    return data, source


def main():
    base, base_source = recorded_manifest(f"{BASE}/candidate_manifest.json")
    candidate, candidate_source = recorded_manifest("out/p20b/candidate-001/candidate_manifest.json")
    before = read("out/p19f/protected_install_manifest.json")["after"]
    after = protected()
    assert before == after
    write("protected_install_manifest", {"status": "PASS", "before": before, "after": after})
    baseline_paths = {item["path"]: item["sha256"] for item in base_source["files"]}
    production_delta = [
        item["path"] for item in candidate_source["files"]
        if item["path"].startswith(("vendor/", "data/", "dsp/", "interfaces/", "plugin/"))
        and baseline_paths.get(item["path"]) != item["sha256"]
    ]
    assert production_delta == ["vendor/iPlug2/Examples/LXR/LXR.cpp"]
    assert all(sha(ROOT / row["path"]) == row["sha256"] for row in candidate_source["files"]
               if row["path"].startswith(("vendor/", "data/", "dsp/", "interfaces/", "plugin/")))
    write("baseline_inventory", {
        "status": "PASS", "branch": git("branch", "--show-current"), "origin": git("remote", "get-url", "origin"),
        "starting_local_tracking_live_head": START, "initial_worktree": "clean",
        "repository": str(ROOT), "dependencies": {key: base[key] for key in ("firmware", "iplug2", "vst3sdk")},
        "dependency_verification": "prepare_dependencies.py --firmware --verify-only completed with pinned archive, SDK dependencies and firmware reconciliation",
        "candidate": base, "source_manifest_files": len(baseline_paths),
        "starting_worktree_manifest_delta": "582 byte-matched, docs/phase_log.md differs, three new P20-A documents or tools",
        "expected_delta_evidence": "git show --stat 0155e15 and Git comparison against 66a6181",
        "tested_production_delta": production_delta,
        "p20a_host_receipt": "out/p20a/host-003/results.json",
    })

    with (ROOT / "out/p16f13/historical_pcm_reconciliation.csv").open(encoding="utf-8-sig", newline="") as f:
        original = list(csv.DictReader(f))
    assert len(original) == 4 and all(row["f13_raw_status"] == "FAIL" for row in original)
    comparison = [compare_pcm(row) for row in original]
    write_csv("historical_pcm_reconciliation", comparison)
    write("historical_pcm_inventory", {"classification": "UNRESOLVED", "original_cases": original,
          "fresh_remeasurement": comparison, "source_revision": "7749ef1 plus F4/F7 worktree snapshots",
          "host": "REAPER 7.80 x64, 48 kHz, offline 24-bit", "current_replay": "out/p20b/roll-current.log"})
    assert comparison[0]["differing_scalar_samples"] == comparison[1]["differing_scalar_samples"]
    for side in ("reference_pcm_sha256", "candidate_pcm_sha256"):
        assert original[0][side] == original[1][side]

    frozen = read("out/p16f6/frozen_reference_manifest.json")["files"]
    checked = []
    for row in frozen:
        path = ROOT / row["original_F4"]
        actual = OUT / "frozen-current" / path.name
        assert sha(path) == sha(ROOT / row["frozen_copy_F5"]) == row["sha256"]
        checked.append({"reference": row["original_F4"], "sha256": row["sha256"],
                        "fresh_sha256": sha(actual), "match": sha(actual) == row["sha256"]})
    assert len(checked) == 76
    write("native_pcm_results", {"status": "PASS" if all(row["match"] for row in checked) else "FAIL",
          "candidate_sha256": candidate["sha256"], "frozen_reference_matches": checked,
          "midi_partition_execution": read("out/p20b/frozen-run/results.json"),
          "roll_replay_execution": inventory(OUT / "roll-current.log")})

    source_records = []
    lineage = []
    for report in sorted((ROOT / "docs").glob("p16f*.md")):
        phase = report.stem.split("_")[0]
        evidence = ROOT / "out" / phase
        files = sorted(p for p in evidence.glob("*") if p.is_file() and p.suffix in {".json", ".csv", ".md"})
        source_records.append({"phase": phase, "report": inventory(report),
                               "original_machine_readable_evidence": [inventory(p) for p in files]})
    write("historical_evidence_inventory", source_records)

    for folder in sorted((ROOT / "out").glob("p1[6789]*")) + [OUT]:
        manifests = [*folder.glob("*/candidate_manifest.json"), *folder.glob("private_binary_manifest.json")]
        for path in sorted(manifests):
            info = read(path)
            if not isinstance(info, dict) or "sha256" not in info:
                continue
            # Retain values actually recorded by the original builder. Some
            # phases built from a worktree and have no recorded Git commit.
            lineage.append({"phase": folder.name, "manifest": inventory(path),
                            "source_commit": info.get("git_commit"),
                            "source_fingerprint": info.get("source_fingerprint"),
                            "module": info.get("module"), "sha256": info.get("sha256"),
                            "toolchain": info.get("configuration"), "resource_hashes": info.get("resources"),
                            "verification_report": str(folder / "verification_report.md")
                            if (folder / "verification_report.md").exists() else None,
                            "build_command": str(path.parent / "build_command.json")
                            if (path.parent / "build_command.json").exists() else None})
    write("artifact_lineage", {"status": "PARTIAL", "candidate_manifests": lineage,
          "qualification_bindings": {
              "P16-F13": {"source_checkpoint": "02c4005", "receipt": "out/p16f13/private_binary_manifest.json"},
              "P20-A": {"source_checkpoint": START, "sha256": base["sha256"], "receipt": "out/p20a/exact_dll_results.json"},
              "P20-B": {"base_revision": START, "sha256": candidate["sha256"], "receipt": "out/p20b/exact_candidate_results.json"},
          }})
    write("private_binary_manifest", candidate)

    host = read("out/p20b/host-001/results.json")
    native = read("out/p20b/native-001/results.json")
    focus = read("out/p20b/focused-003/results.json")
    import_busy = read("out/p20b/import-busy/results.json")
    resources = read("out/p20b/resources/results.json")
    assert all(row["sha256"] == candidate["sha256"] for row in (
        read("out/p20b/host-001/plugin-before.json"), read("out/p20b/host-001/plugin-after.json")
    ))
    write("exact_candidate_results", {
        "status": "PASS" if len(host) == 20 and all(r["status"] == "PASS" for r in host) else "FAIL",
        "sha256": candidate["sha256"], "source": candidate["source_fingerprint"], "host_rows": host,
        "focused_probe": focus, "import_busy": import_busy, "allocation_probe": resources,
        "original_artifact_qualification": "PARTIAL",
    })
    rows = [dict(suite=r["name"], status=r["status"], evidence=f"out/p20b/{scope}-001/{r['name']}.log",
                 summary=" | ".join(r.get("summary", [])))
            for scope, group in (("host", host), ("native", native)) for r in group]
    for name, record in (("P20BHostEventCaptureTest", focus), ("P16F3ImportRejectionTest", import_busy),
                         ("P16F3ExactDllResourcesTest", resources)):
        rows.append(dict(suite=name, status=record["status"], evidence=record["command"][0], summary=""))
    write_csv("regression_matrix", rows)

    log = (OUT / "focused-003/run.log").read_text(encoding="utf-8")
    captures = [line for line in log.splitlines() if line.startswith(("CAPTURE ", "EVENT "))]
    for label, name in (("import", "timing_comparison_results"), ("euclidean", "euclidean_event_results"),
                        ("copy", "copy_clear_event_results")):
        labels = (label, "clear") if label == "copy" else (label,)
        write(name, {"status": "PARTIAL" if focus["status"] == "PASS" else focus["status"],
                     "candidate_sha256": candidate["sha256"], "controlled_host_trace": [
                         line for line in captures if line.split()[1] in labels],
                     "live_clear": [line for line in log.splitlines() if line.startswith("LIVE_CLEAR ")]
                     if label == "copy" else [],
                     "direct_REAPER_trace": "BLOCKED", "focus_log": "out/p20b/focused-003/run.log"})
    write("event_observer_results", {"status": "PARTIAL", "focused_test": focus["status"],
          "capture": "synchronous one-frame 48 kHz VST3 process", "capacity_per_kind": 256,
          "trace_summary": [line for line in captures if line.startswith("CAPTURE ")],
          "historical_collector": read("out/p16f12/observer_qualification_results.json")})
    write("lxqb_import_results", {"production_path": focus["status"], "graphical": "GRAPHICAL_BLOCKED",
          "source_regression_commit": "06d37c12510998d60ac940ba4d6ae1e2aceaad30",
          "candidate_sha256": candidate["sha256"], "exact_test": focus, "busy_test": import_busy,
          "historical_malformed_fixtures": read("out/p16f11/import_rejection_results.json")})
    with (OUT / "release_blocker_matrix.csv").open(encoding="utf-8-sig", newline="") as f:
        blockers = list(csv.DictReader(f))
    dispositions = {
        "A1": "PRODUCTION_PASS", "A2": "GRAPHICAL_BLOCKED", "B": "PARTIAL", "C": "PARTIAL",
        "D1": "UNRESOLVED", "D2": "UNRESOLVED", "D3": "UNRESOLVED", "D4": "UNRESOLVED", "E": "PARTIAL",
    }
    write_csv("release_blocker_final", [
        {**row, "final_disposition": dispositions[row["id"]]} for row in blockers
    ])
    write("file_inventory", [inventory(p) for p in sorted(OUT.iterdir())
                             if p.is_file() and p.name != "file_inventory.json"])


if __name__ == "__main__":
    main()
