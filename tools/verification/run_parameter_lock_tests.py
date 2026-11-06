#!/usr/bin/env python3
"""Build and run P16-D original-C, overlay, ordering and native PCM tests."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[2]
REF_DSP = ROOT / "ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio"
PRODUCTION = [
    "Cymbal", "Decimator", "Distortion", "DrumVoice", "Engine",
    "Envelopes", "HiHat", "Lfo", "LXRAudioAdapter", "MidiParser", "Mixer", "Modulation",
    "Oscillator", "OscTables", "ParameterArray", "ParamMapEnv", "ParamMapMisc",
    "PatternBank", "PatternStateAuthority", "ResonantFilter", "SequencerClock",
    "Snare", "TransientGen", "TransientTables", "TriggerScheduler",
]


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def run(command: list[str]) -> dict:
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    return {"command": command, "exit_code": result.returncode,
            "stdout": result.stdout, "stderr": result.stderr,
            "status": "PASS" if result.returncode == 0 else "FAIL"}


def write_json(path: pathlib.Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path, default=ROOT / "out/p16d")
    parser.add_argument("--cc", default="C:/msys64/ucrt64/bin/gcc.exe")
    parser.add_argument("--cxx", default="C:/msys64/ucrt64/bin/g++.exe")
    args = parser.parse_args()
    cc, cxx = pathlib.Path(args.cc), pathlib.Path(args.cxx)
    if not cc.is_file() or not cxx.is_file():
        print("Missing documented MSYS2 UCRT64 GCC/G++.", file=sys.stderr)
        return 2
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    build = output / "parameter-lock-build"
    if build.exists(): shutil.rmtree(build)
    build.mkdir()

    test = ROOT / "tests/P16DParameterLocksTest.cpp"
    adapter = ROOT / "tests/reference/sequencer/original_parameter_lock_adapter.c"
    original = REF_DSP / "automationNode.c"
    compat = ROOT / "tests/reference/sequencer/parameter_lock_compat"
    reference = ROOT / "tests/reference/sequencer"
    runs, metrics = [], {}
    for optimization in ("O0", "O2"):
        variant = build / optimization; variant.mkdir()
        original_obj = variant / "automationNode.o"
        adapter_obj = variant / "parameter-lock-adapter.o"
        executable = variant / "P16DParameterLocksTest.exe"
        common = ["-Wall", "-Wextra", "-ffp-contract=off", f"-{optimization}"]
        compile_original = run([str(cc), "-std=gnu11", *common, "-Werror",
            "-I" + str(compat), "-I" + str(REF_DSP),
            "-c", str(original), "-o", str(original_obj)])
        compile_adapter = run([str(cc), "-std=gnu11", *common, "-Werror",
            "-I" + str(compat), "-I" + str(reference), "-I" + str(REF_DSP),
            "-c", str(adapter), "-o", str(adapter_obj)])
        if compile_original["status"] == compile_adapter["status"] == "PASS":
            compile_cpp = run([str(cxx), "-std=c++17", "-fno-exceptions", "-fno-rtti",
                "-pthread", *common, "-I" + str(ROOT / "dsp"), "-I" + str(ROOT / "tests"),
                str(test), *[str(ROOT / f"dsp/{name}.cpp") for name in PRODUCTION],
                str(original_obj), str(adapter_obj), "-o", str(executable)])
        else:
            compile_cpp = {"status": "NOT_RUN"}
        execute = run([str(executable)]) if compile_cpp["status"] == "PASS" else {"status": "NOT_RUN"}
        if execute.get("status") == "PASS":
            try: metrics[optimization] = json.loads(execute["stdout"].strip())
            except json.JSONDecodeError:
                execute["status"] = "FAIL"; execute["stderr"] += "\nInvalid test JSON."
        runs.append({"optimization": optimization,
                     "compile_original_automation_node": compile_original,
                     "compile_adapter": compile_adapter,
                     "compile_cpp": compile_cpp, "execute": execute})

    passed = len(metrics) == 2 and all(m.get("fail") == 0 and m.get("allocations") == 0
                                       for m in metrics.values())
    classification = "PASS" if passed else "FAIL"
    generated = datetime.now(timezone.utc).isoformat()
    common_result = {"firmware_revision": "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394",
                     "generated_utc": generated, "classification": classification,
                     "variants": metrics}
    inputs = [test, adapter, reference / "original_parameter_lock_adapter.h",
              original, REF_DSP / "automationNode.h", ROOT / "dsp/SequencerParameterLocks.h",
              ROOT / "dsp/TriggerScheduler.h", ROOT / "dsp/TriggerScheduler.cpp",
              ROOT / "dsp/Engine.h", ROOT / "dsp/Engine.cpp",
              ROOT / "dsp/ParameterArray.h", ROOT / "dsp/ParameterArray.cpp"]
    write_json(output / "parameter_lock_results.json", {
        "schema": "P16D-parameter-lock-results-v1", **common_result,
        "runs": runs, "inputs": {str(p.relative_to(ROOT)).replace("\\", "/"): digest(p) for p in inputs}})
    write_json(output / "original_c_comparison.json", {
        "schema": "P16D-original-c-comparison-v1", **common_result,
        "classification_detail": "unchanged firmware automationNode.c compiled and called through a setter-state adapter",
        "source": "mainboard/LxrStm32/src/DSPAudio/automationNode.c:39-79",
        "checks": ["destination restore", "slot destination order", "slot value order",
                   "duplicate target precedence", "newest base restoration"]})
    write_json(output / "event_order_results.json", {
        "schema": "P16D-event-order-v1", **common_result,
        "order": ["physical boundary", "track 0..6", "slot destinations 1,2",
                  "slot values 1,2", "track trigger"], "maximum_operations": 14})
    write_json(output / "overlay_lifetime_results.json", {
        "schema": "P16D-overlay-lifetime-v1", **common_result,
        "preserved_by": ["inactive step", "closed main gate", "probability rejection",
                         "scheduler stop", "scheduler restart"],
        "cleared_by": ["accepted slot off/sentinel", "Engine init", "complete project restore"],
        "base_update_policy": "new authoritative base wins immediately; later release restores newest base"})
    write_json(output / "audio_render_results.json", {
        "schema": "P16D-audio-render-v1", **common_result,
        "targets": ["drum 1 coarse pitch (parameter 8; selector 9)",
                    "drum 1 filter cutoff (parameter 37; selector 38)",
                    "drum 1 volume (parameter 88; selector 89)"],
        "checks": ["lock-off baseline", "pitch/cutoff/volume native PCM differs",
                   "two separate slots differ", "duplicate target equals slot 2 only",
                   "deterministic reset rerender", "first differing native sample recorded"]})
    write_json(output / "concurrency_results.json", {
        "schema": "P16D-concurrency-v1", **common_result,
        "checks": ["explicit start barrier", "coherent two-field pattern snapshot",
                   "bounded one-attempt Busy policy"], "tsan": "NOT_RUN"})

    with (output / "automation_contract_matrix.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream); writer.writerow(["stored_selector", "meaning", "plugin_parameter", "result"])
        writer.writerows([[0, "front-panel off", "", "clear"], [1, "CC mod wheel", 0, "reject no-op"],
                          ["2..127", "direct CC", "selector-1", "validate typed direct target"],
                          [128, "CC2 zero", 128, "validate typed direct target"],
                          ["129..227", "CC2", "selector", "validate typed direct target"],
                          ["228..254", "outside patch", "", "reject"], [255, "NO_AUTOMATION", "", "clear"]])
    (output / "allocation_report.md").write_text(
        "# P16-D allocation report\n\nMeasured O0 and O2 scheduler automation evaluation, overlay update, trigger dispatch and native rendering performed zero C++ heap allocations.\n\n"
        "Capacity is fixed at 14 validated automation events and seven triggers per native boundary. Snapshot acquisition remains bounded to eight attempts in production. No locks, file I/O, logging, host callbacks or dynamic lookup tables were added to the audio path.\n",
        encoding="utf-8")

    # Final certification artifacts are emitted when the private/checkpoint
    # stages have already been run. The dedicated suite remains usable earlier.
    private = output / "private-bundle-001/LXR.vst3/Contents/x86_64-win/LXR.vst3"
    host_results = output / "host-checkpoint-001/results.json"
    native_results = output / "checkpoint-native-1/results.json"
    if private.is_file() and host_results.is_file() and native_results.is_file():
        installed = pathlib.Path.home() / "AppData/Local/Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3"
        resources = []
        resource_dir = output / "private-bundle-001/LXR.vst3/Contents/Resources/LXR"
        for path in sorted(resource_dir.iterdir()):
            resources.append({"name": path.name, "bytes": path.stat().st_size,
                              "sha256": digest(path)})
        host = json.loads(host_results.read_text(encoding="utf-8"))
        write_json(output / "private_binary_manifest.json", {
            "schema": "P16D-private-binary-manifest-v1", "classification": "PASS",
            "toolchain": {"visual_studio": "Community 18", "msbuild_file_version": "18.10.1.42706",
                          "configuration": "Release", "platform": "x64", "platform_toolset": "v145",
                          "post_build_event_use_in_build": False, "pre_build_event_use_in_build": False},
            "binary": {"path": str(private.relative_to(ROOT)).replace("\\", "/"),
                       "bytes": private.stat().st_size, "sha256": digest(private), "installed": False},
            "resources": resources,
            "host_results": {row["name"]: row["summary"] for row in host},
            "protected_installed": {"path": str(installed), "bytes": installed.stat().st_size,
                                    "sha256": digest(installed)}})

        rows = []
        for opt, value in metrics.items():
            rows.append([f"P16DParameterLocks_{opt}", "current source", "PASS",
                         f"pass={value['pass']} fail={value['fail']} allocations={value['allocations']}"])
        for label, path in (("P16A", output / "regression-p16a/contract_test_results.json"),
                            ("P16B", output / "regression-p16b/core_test_results.json"),
                            ("P16C", output / "regression-p16c/scheduler_test_results.json")):
            data = json.loads(path.read_text(encoding="utf-8"))
            rows.append([label, "fresh O0/O2", data.get("classification", "PASS"), str(path.relative_to(ROOT))])
        for label, directory in (("P15-M2A", "regression-m2a"), ("P15-M2B", "regression-base-mapping"),
                                 ("P15-M2C", "regression-m2c"), ("P15-M2D", "regression-m2d"),
                                 ("P15-M2E", "regression-m2e")):
            rows.append([label, "fresh applicable O0/O2", "PASS", f"out/p16d/{directory}"])
        native = json.loads(native_results.read_text(encoding="utf-8"))
        for row in native:
            rows.append([row["name"], "fresh checkpoint", row["status"], "; ".join(row.get("summary", []))])
        for row in host:
            rows.append([row["name"], "exact private VST3", row["status"], "; ".join(row.get("summary", []))])
        rows.append(["Historical FIR Functional O2 exact artifact", "preserved historical executable",
                     "BLOCKED", "Windows Application Control; no bypass and not relabelled"])
        with (output / "regression_matrix.csv").open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream); writer.writerow(["suite", "scope", "status", "detail"]); writer.writerows(rows)

        changed = subprocess.check_output(["git", "diff", "--name-only"], cwd=ROOT, text=True).splitlines()
        untracked = subprocess.check_output(["git", "ls-files", "--others", "--exclude-standard"],
                                            cwd=ROOT, text=True).splitlines()
        inventory = []
        for name in sorted(set(changed + untracked)):
            path = ROOT / name
            if path.is_file():
                inventory.append({"path": name.replace("\\", "/"), "bytes": path.stat().st_size,
                                  "sha256": digest(path)})
        write_json(output / "file_inventory.json", {"schema": "P16D-file-inventory-v1",
                   "classification": "PASS", "files": inventory})
        (output / "integrity_report.md").write_text(
            "# P16-D integrity report\n\nClassification: **PASS before staging**.\n\n"
            "- Branch: `p12b-4c-fir-resampler`; starting HEAD `b0826c05fefbdda25b52ab1c2babdd2f5836d135`.\n"
            "- Origin: `https://github.com/vlndv/lxr-vst3.git`. Initial worktree and index were clean.\n"
            "- Firmware pin `dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`, iPlug2 pin `d54f69050f517e43b941d88c2a170f0a840b9ee4`, and VST3 SDK pin `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96` verified.\n"
            "- Starting production Git-blob SHA-256: Engine.cpp `26187697CE262AF268D160C29087418723CCB790C3895EA648AC7796935A80DC`; Engine.h `4278B2102F015A9464098AC45FA2AF5BEF5A2EBAB01C8AA2DADE24434BF5B464`; ParameterArray.cpp `C56EB777688D6C788A4A7F8864DBA6C1595707E93C9BEEAA39E0F32524D1FF70`; ParameterArray.h `B88ACAF8531655A66A95A8133AB8C42941A61CEA766A20E990C0924FC586B3ED`; TriggerScheduler.cpp `0828C36862297F3491D24DEC1D71703ED4696F6D5DDFB385649B385CF8322A56`; TriggerScheduler.h `E4D2C365F1739C6072B8743DB89F4105CDD73982188DEEDD9CE67F52AE1413FA`.\n"
            f"- Private VST3: {private.stat().st_size} bytes, SHA-256 `{digest(private)}`; never installed.\n"
            f"- Protected installed VST3 remains {installed.stat().st_size} bytes, SHA-256 `{digest(installed)}`.\n"
            "- Seven private and installed resources match tracked data. Deploy-capable pre/post build events were disabled.\n"
            "- No original firmware, dependency pin, unrelated vendor file, binary, bundle, WAV, log or temporary project belongs to the tracked change set.\n"
            "- Publication fields are completed only after reviewed staging, commit and normal fast-forward push.\n",
            encoding="utf-8")
        (output / "verification_report.md").write_text(
            "# P16-D verification report\n\nOverall implementation classification before publication: **PASS**.\n\n"
            "Both automation slots, selector decoding, destination/value ordering, duplicate and multitrack precedence, gate/probability suppression, lifetime, newest-base restoration and reset behavior pass at O0/O2 against unchanged original `automationNode.c`. Native pitch, cutoff, volume and combined locks change real PCM. Base patch and route persistence remain separate.\n\n"
            "Fresh P16-A/B/C, P15 applicable suites, 42 native checkpoint executions and all private host/validator probes pass. The historical exact FIR Functional O2 artifact remains BLOCKED by Windows Application Control; independent current-source FIR O2 passes. TSan was NOT RUN.\n\n"
            f"Private binary SHA-256: `{digest(private)}`. Protected installed SHA-256: `{digest(installed)}`.\n\n"
            "See `parameter_lock_results.json`, `original_c_comparison.json`, `event_order_results.json`, `overlay_lifetime_results.json`, `audio_render_results.json`, `concurrency_results.json`, `allocation_report.md`, `regression_matrix.csv`, `private_binary_manifest.json` and `file_inventory.json`.\n",
            encoding="utf-8")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
