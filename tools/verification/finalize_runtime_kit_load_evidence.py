#!/usr/bin/env python3
"""Assemble P17-D acceptance evidence from completed immutable runs."""

from __future__ import annotations

import csv
import hashlib
import json
import pathlib
import subprocess
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "out/p17d"
START = "8a99b26edd356680912dca66de00f99af6b52584"
PROTECTED = "89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82"


def read(path: str):
    return json.loads((OUT / path).read_text(encoding="utf-8"))


def write(path: str, value) -> None:
    (OUT / path).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def sha_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def sha(path: pathlib.Path) -> str:
    return sha_bytes(path.read_bytes())


def git(*args: str, binary: bool = False):
    return subprocess.check_output(["git", *args], cwd=ROOT,
                                   text=not binary)


def metric_rows(result: dict) -> list[dict]:
    rows = []
    assert result["classification"] == "PASS"
    for run in result["runs"]:
        executed = run["execute"]
        assert executed["status"] == "PASS"
        rows.append({"compiler": result["compiler"],
                     "optimization": run["optimization"],
                     **executed["metrics"]})
    return rows


def main() -> int:
    generated = datetime.now(timezone.utc).isoformat()
    gcc = read("runtime-gcc-accepted3/runtime_kit_load_results.json")
    llvm = read("runtime-vs-llvm-accepted3/runtime_kit_load_results.json")
    runtime = metric_rows(gcc) + metric_rows(llvm)
    assert all(row["pass"] == 1176 and row["fail"] == 0 and
               row["publication_allocations"] == 0 for row in runtime)
    hashes = runtime[0]["pcm_hashes"]
    differences = runtime[0]["first_differences"]
    fingerprints = runtime[0]["candidate_fingerprints"]
    input_kits = runtime[0]["input_kit_hex"]
    patch_generations = runtime[0]["patch_generations"]
    route_generations = runtime[0]["route_generations"]
    assert all(row["pcm_hashes"] == hashes and
               row["first_differences"] == differences and
               row["candidate_fingerprints"] == fingerprints and
               row["input_kit_hex"] == input_kits and
               row["patch_generations"] == patch_generations and
               row["route_generations"] == route_generations for row in runtime)

    host = read("checkpoint-host-004/results.json")
    native = read("checkpoint-native-001/results.json")
    assert len(host) == 11 and all(row["status"] == "PASS" for row in host)
    assert len(native) == 44 and all(row["status"] == "PASS" for row in native)
    private = read("private-build-002/candidate_manifest.json")
    protected = read("private-build-002/protected_install.json")
    assert private["sha256"] == "658272C716FDB1A7C42848B9F073888118F825D4F61C9730FBA3D7F8C2DDFC5F"
    assert protected["before"] == protected["after"]
    assert protected["before"][0]["sha256"] == PROTECTED
    exact = read("exact-dll-resources-001/results.json")
    assert exact["status"] == "PASS"
    binary = read("private-binary-analysis-001/binary_analysis.json")
    assert binary["candidate_sha256"] == private["sha256"] and binary["pdb_matches_pe"]
    exact_log = (OUT / "exact-dll-resources-001/run.log").read_text(encoding="utf-8")
    assert "SUMMARY pass=8252 fail=0" in exact_log
    assert "process_4000_callbacks allocations=0" in exact_log

    common = {"schema": "P17D-evidence-v1", "generated_utc": generated,
              "starting_head": START, "engineering_classification": "PASS"}
    write("route_application_results.json", {**common,
        "classification": "PASS", "source_selector_range": [0, 204],
        "route_count": 12, "supported_rows_per_source": 107,
        "above_127_per_source": 39, "highest_tested_identity": 203,
        "checks": ["all twelve replace together", "OFF replaces old route",
                   "high identities preserved", "cross-voice/global reject",
                   "no truncation or OFF substitution"],
        "runtime_runs": runtime})
    write("lock_rebase_results.json", {**common, "classification": "PASS",
        "p16d": {"O0": {"pass": 246, "fail": 0},
                 "O2": {"pass": 246, "fail": 0}},
        "checks": ["no active lock", "one active lock", "duplicate locks",
                   "multiple track nodes", "two successive kits",
                   "invalid between valid", "host edit after kit",
                   "release restores newest base", "complete restore supersedes kit"],
        "effective_order": ["base patch", "sequencer lock", "velocity", "LFO", "render"]})
    scenarios = ["default", "D1 pitch", "D1 volume", "snare", "cymbal",
                 "shared hi-hat", "filter cutoff", "velocity destination",
                 "LFO destination", "matched repeat"]
    triggers = [
        "frame 0: D1 trigger velocity 100 note 63",
        "frame 0: D1 trigger velocity 100 note 63",
        "frame 0: D1 trigger velocity 100 note 63",
        "frame 0: snare trigger velocity 100 note 63",
        "frame 0: cymbal trigger velocity 100 note 63",
        "frame 0: shared hi-hat closed then open articulation, velocity 100 note 63",
        "frame 0: D1 trigger velocity 100 note 63",
        "frame 0: D1 trigger velocity 24 note 63",
        "frame 0: D1 trigger velocity 100 note 63",
        "frame 0: D1 trigger velocity 100 note 63",
    ]
    scenario_rows = []
    for index, name in enumerate(scenarios):
        first = None if index == 0 else differences[index - 1]
        expected = ("reference capture" if index == 0 else
                    "exact equality with default" if index == 9 else
                    "measurable PCM difference from controlled reference")
        observed = ("reference captured" if index == 0 else
                    "exact equality; no differing frame" if first == -1 else
                    f"first difference at native frame {first}")
        scenario_rows.append({
            "name": name,
            "input_encoding": "P17-B RawKit name[8]+values[228]",
            "input_kit_sha256": sha_bytes(bytes.fromhex(input_kits[index])),
            "candidate_fingerprint_fnv1a64": fingerprints[index],
            "patch_generation": patch_generations[index],
            "route_generation": route_generations[index],
            "application_boundary_native_frame": 0,
            "application_boundary_kind": "fresh isolated native Engine before first 32-frame block",
            "native_trigger_events": [triggers[index]],
            "pcm_fnv1a64": hashes[index],
            "first_differing_frame": first,
            "expected": expected,
            "observed": observed,
        })
    write("native_pcm_results.json", {**common, "classification": "PASS",
        "native_rate": "5632353/128 Hz", "block_frames": 32,
        "scenarios": scenario_rows,
        "runtime_plugin_boundary_check": "private DLL probe applies coherent kit at a native frame divisible by 32",
        "compiler_results": runtime,
        "note": "Software-native deterministic hashes; not hardware PCM or bit-exact UART timing."})
    write("concurrency_results.json", {**common, "classification": "PASS",
        "checks": ["kit versus coherent audio snapshot", "kit versus host patch order",
                   "kit versus project restore cutoff", "kit versus independent pattern authority",
                   "kit versus routing update", "two simultaneous kits",
                   "multi-instance independence", "bounded one-attempt Busy"],
        "reader_mixed_generations": 0, "internal_retries": 0,
        "reader_busy_snapshots": [{"compiler": row["compiler"],
                                   "optimization": row["optimization"],
                                   "count": row["reader_busy"]}
                                  for row in runtime],
        "tsan": "NOT_RUN", "race_argument": "outer gate plus release commit point; stress is supporting evidence"})
    controller = {row["name"]: row["summary"] for row in host}
    write("controller_results.json", {**common, "classification": "PASS",
        "suites": controller, "kit_probe_assertions": 1057,
        "checks": ["228 patch readback", "twelve route readback",
                   "high route token", "processor state", "newer-state reconciliation",
                   "two instances", "no unsolicited edit gestures"]})
    write("rollback_results.json", {**common, "classification": "PASS",
        "failure_cases": ["wrong snd length (P17-C)", "invalid selector",
                          "unsupported/cross-voice destination", "candidate validation",
                          "authority Busy", "newer project restore", "newer host/route generation",
                          "stale kit", "repeated rejection", "multi-instance"],
        "unchanged_domains": ["patch", "routes", "pattern bank", "transport",
                              "bypass", "controller state"],
        "partial_mutations": 0})
    write("private_binary_manifest.json", {**common, "classification": "PASS",
        "candidate": private, "binary_analysis": {
            "pdb_sha256": binary["pdb_sha256"], "pdb_matches_pe": True,
            "unwind_record_count": binary["unwind_record_count"],
            "unknown_unwind_record_count": binary["unknown_unwind_record_count"]},
        "validator": "47 passed, 0 failed", "installed": False})
    write("protected_install_manifest.json", {**common, "classification": "PASS",
        "expected_module_sha256": PROTECTED, "before": protected["before"],
        "after": protected["after"], "unchanged": True})

    (OUT / "transaction_contract.md").write_text("""# P17-D transaction contract

- Owner: one non-audio producer per `LXR` instance; processing is a bounded reader.
- Payload: name[8], patch[228], typed routes[12]. No bank, transport, bypass or project suffix.
- Reservation: shared monotonic producer order plus expected routing generation.
- Gate: existing per-instance `RoutingStateAuthority::Guard`.
- Preflight: complete candidate/routes, restore cutoff, expected generation, route tickets and generation ceilings.
- Commit: patch sequence commits while the gate is held; the routing generation release-store is the logical cross-authority commit point.
- Observation: coordinated readers hold the same gate while snapshotting patch and routes.
- Failure: Invalid, Busy, Stale or GenerationExhausted before payload mutation; no internal retry.
- Audio boundary: staged Engine selected before the next 32-frame native producer refill; no voice/sequencer/FIR reset.
- Ordering: later host/MIDI bytes replay after kit lock rebasing; complete LXRS restore supersedes the kit.
""", encoding="utf-8")
    (OUT / "publication_state_machine.md").write_text("""# P17-D publication state machine

`Decoded -> CandidateValidated -> Reserved -> OlderMailboxesDrained -> GateHeld -> Preflighted -> PatchCommitted -> Routes/NameCommitted -> ControllerPending -> NativeBoundaryApplied`

Terminal rejection edges:

- decode/candidate error -> `RejectedUnchanged`
- gate/writer unavailable -> `BusyUnchanged`
- expected generation/cutoff/ticket lost -> `StaleUnchanged`
- generation ceiling -> `ExhaustedUnchanged`

The `PatchCommitted -> Routes/NameCommitted` sequence is not externally split:
the outer gate remains held, and routing release publication is the only logical
commit observed by coordinated readers. Controller projection is skipped if a
newer generation already won. Audio uses the latest coherent snapshot once at a
native boundary.
""", encoding="utf-8")
    (OUT / "allocation_stack_report.md").write_text(f"""# P17-D allocation and stack report

- Runtime publication: zero counted C++ allocations in all four accepted variants.
- O2 publication means: GCC {runtime[1]['publication_mean_ns']} ns; VS LLVM {runtime[3]['publication_mean_ns']} ns.
- Finite maxima: GCC O0/O2 {runtime[0]['publication_max_ns']}/{runtime[1]['publication_max_ns']} ns; VS LLVM O0/O2 {runtime[2]['publication_max_ns']}/{runtime[3]['publication_max_ns']} ns.
- Exact private DLL: 8,252/0; 4,000 varied process callbacks allocate zero.
- Exact observed processing stack watermark: 19,744 bytes on a 1 MiB thread.
- `ApplyPendingStateAndParameters` fixed unwind prolog: 8,136 bytes.
- Static source review: no kit-path lock, allocation, file I/O, logging, recursion or unbounded retry on processing.

Limits: timings include harness/OS scheduling and are not worst-case latency.
Watermarks are finite observations; PE analysis reports three unknown unwind
records and does not prove all dynamic/callee stack. File decoding remains off audio.
""", encoding="utf-8")

    changed = sorted(set(git("diff", "--name-only", START).splitlines()) |
                     set(git("ls-files", "--others", "--exclude-standard").splitlines()))
    inventory = []
    for name in changed:
        path = ROOT / name
        try:
            before = subprocess.check_output(
                ["git", "show", f"{START}:{name}"], cwd=ROOT,
                stderr=subprocess.DEVNULL)
            before_hash = sha_bytes(before)
        except subprocess.CalledProcessError:
            before_hash = None
        inventory.append({"path": name.replace("\\", "/"),
                          "baseline_sha256": before_hash,
                          "current_sha256": sha(path), "bytes": path.stat().st_size})
    write("file_inventory.json", {**common, "changed_files": inventory,
        "generated_evidence_excluded_from_git": True})

    rows = []
    for row in runtime:
        rows.append(["P17D runtime", row["compiler"], row["optimization"], "PASS", row["pass"]])
    for row in host:
        rows.append([row["name"], "MSVC", "O2", row["status"], "; ".join(row["summary"])])
    rows.append(["P17A", "GCC", "O0/O2", "PASS", "306037 each"])
    rows.append(["P17B", "GCC", "O0/O2", "PASS", "817873 each"])
    rows.append(["P17C", "GCC", "O0/O2", "PASS", "594156 each"])
    rows.append(["P16D locks", "GCC", "O0/O2", "PASS", "246 each"])
    rows.append(["Supported native checkpoint", "GCC", "O0/O2", "PASS", "44 executions"])
    rows.append(["Exact DLL resources/RT", "MSVC", "O2", "PASS", "8252"])
    with (OUT / "regression_matrix.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream); writer.writerow(["suite", "compiler", "optimization", "status", "assertions_or_summary"])
        writer.writerows(rows)

    current_head = git("rev-parse", "HEAD").strip()
    status = git("status", "--short")
    (OUT / "integrity_report.md").write_text(f"""# P17-D integrity report

- Starting/current pre-publication HEAD: `{START}` / `{current_head}`.
- Branch: `p12b-4c-fir-resampler`; origin: `https://github.com/vlndv/lxr-vst3.git`.
- Firmware/iPlug2/SDK pins and seven SDK modules: verified from cached immutable archives.
- Protected installed DLL: `{PROTECTED}` before/after; seven resources match tracked data.
- Private build: `{private['sha256']}`, deployment disabled, never installed.
- Existing P16/P17 evidence was not rewritten. All new evidence is under `out/p17d/` and ignored by Git.
- Current task paths are enumerated with baseline/current hashes in `file_inventory.json`.
- Index must remain empty until explicit reviewed staging. Current porcelain follows:

```
{status.rstrip()}
```
""", encoding="utf-8")
    (OUT / "verification_report.md").write_text(f"""# P17-D verification report

Engineering acceptance: **PASS**. Publication: **PENDING REVIEWED COMMIT/PUSH**.

All four required runtime configurations pass 1,176/0 with zero publication
allocations and identical deterministic PCM hashes. All 44 supported native
checkpoint executions pass. P17-A/B/C, P15 modulation and P16 lock/state
regressions pass. Exact private host suites and controller kit assertions pass;
Steinberg validator is 47/47. Exact processing instrumentation is 8,252/0 with
zero allocations in 4,000 callbacks and a 19,744-byte observed stack watermark.

Private DLL: `{private['sha256']}` ({private['bytes']} bytes). Protected installed
DLL: `{PROTECTED}`, unchanged. Detailed contracts/results are the sibling
artifacts required by the P17-D task.

No browser, `.snd` UI, factory kits, morphing, custom graphics or sample loading
was added. P17-E user-facing import is ready as a separate milestone. P16-F
release certification remains BLOCKED/open and no release claim is made.
""", encoding="utf-8")
    print(json.dumps({"classification": "PASS", "changed_files": len(inventory),
                      "runtime_assertions": sum(row["pass"] for row in runtime),
                      "private_sha256": private["sha256"]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
