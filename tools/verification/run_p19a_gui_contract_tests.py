#!/usr/bin/env python3
"""Generate/verify the P19-A control map and run standalone GUI contracts."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
import time
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[2]
DESIGN = ROOT / "design" / "p19a"
INVENTORY = DESIGN / "parameter_control_inventory.csv"
OPERATIONS = DESIGN / "operation_binding_matrix.csv"

COLUMNS = [
    "parameter_id", "control_id", "stable_internal_name", "user_label",
    "owner_voice", "type", "minimum", "maximum", "default", "unit",
    "display_conversion", "normalized_conversion", "automatable",
    "implementation_status", "view", "widget", "validation_rules",
    "visibility_rules", "accessibility_label", "ui_representation", "source",
]


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def run(command: list[str], env: dict[str, str], timeout: int = 900) -> dict:
    started = time.monotonic()
    try:
        result = subprocess.run(command, cwd=ROOT, env=env, text=True,
                                capture_output=True, timeout=timeout)
        return {"status": "PASS" if result.returncode == 0 else "FAIL",
                "exit_code": result.returncode, "stdout": result.stdout,
                "stderr": result.stderr, "command": command,
                "elapsed_seconds": time.monotonic() - started}
    except OSError as error:
        return {"status": "BLOCKED" if getattr(error, "winerror", None) == 4551 else "ERROR",
                "error": str(error), "command": command,
                "elapsed_seconds": time.monotonic() - started}


def native_descriptors() -> list[dict]:
    text = (ROOT / "vendor/iPlug2/Examples/LXR/LXRParamContract.h").read_text(encoding="utf-8")
    rows = []
    pattern = re.compile(r'P\("([^"]+)"(?:,\s*(\d+))?(?:,\s*ParamSemantic::(\w+))?\),?\s*//\s*(\d+)')
    for name, default, semantic, index in pattern.findall(text):
        rows.append({"id": int(index), "name": name,
                     "default": int(default or 0), "min": 0, "max": 127,
                     "semantic": semantic or "Raw7Bit"})
    rows.sort(key=lambda row: row["id"])
    if [row["id"] for row in rows] != list(range(228)):
        raise RuntimeError("failed to parse the exact 228 native descriptors")
    return rows


def param_map() -> dict[int, dict[str, str]]:
    rows: dict[int, dict[str, str]] = {}
    for line in (ROOT / "docs/param_map.md").read_text(encoding="utf-8").splitlines():
        match = re.match(r"\|\s*(\d+)\s*\|\s*([^|]+)\|\s*([^|]+)\|\s*([^|]+)\|\s*([^|]+)\|\s*([^|]+)\|\s*([^|]+)\|", line)
        if match:
            ident = int(match.group(1))
            if ident < 228:
                rows[ident] = {"name": match.group(2).strip(),
                               "midi": match.group(3).strip(),
                               "owner": match.group(4).strip(),
                               "scale": match.group(5).strip(),
                               "engine": match.group(6).strip().strip("`"),
                               "tag": match.group(7).strip()}
    return rows


def title(name: str) -> str:
    replacements = {"OSC": "Oscillator", "WAVE": "Waveform", "FREQ": "Frequency",
                    "VOL": "Volume", "VELO": "Velocity", "AMNT": "Amount",
                    "RESO": "Resonance", "DEST": "Destination", "HH": "Hi-hat",
                    "CYM": "Cymbal", "LFO": "LFO", "MIDI": "MIDI",
                    "NRPN": "NRPN", "EG": "Envelope", "MOD": "Modulation"}
    words = []
    for token in name.split("_"):
        words.append(replacements.get(token, token.capitalize()))
    return " ".join(words)


def owner_for(ident: int, mapped: dict[int, dict[str, str]]) -> str:
    if ident in mapped and mapped[ident]["owner"] != "-":
        return mapped[ident]["owner"]
    if 221 <= ident <= 227:
        return ("D1", "D2", "D3", "SN", "CY", "CH", "OH")[ident - 221]
    return "Global"


def hidden_reason(ident: int) -> str:
    if ident in (0, 127):
        return "Hidden: firmware-reserved byte; host compatibility storage only."
    if ident in (5, 97, 98):
        return "Hidden: MIDI protocol placeholder, not a sound control."
    if 149 <= ident <= 154:
        return "Hidden: legacy seven-bit velocity destination shadow; use typed IDs 228-233."
    if 161 <= ident <= 166:
        return "Hidden: legacy LFO voice-hint byte owned by typed route state."
    if 167 <= ident <= 172:
        return "Hidden: legacy seven-bit LFO destination shadow; use typed IDs 234-239."
    return ""


def native_row(descriptor: dict, mapped: dict[int, dict[str, str]]) -> dict[str, str]:
    ident = descriptor["id"]
    info = mapped.get(ident, {"owner": "-", "scale": "raw", "engine": "-", "tag": "raw"})
    hidden = hidden_reason(ident)
    owner = owner_for(ident, mapped)
    semantic = descriptor["semantic"]
    if hidden:
        view, widget, ui = "Hidden compatibility", "none", hidden
        status = "storage-only" if ident not in range(149, 173) else "superseded-by-typed-route"
    else:
        status = "implemented"
        if ident == 114:
            view = "Overview"
        elif 115 <= ident <= 190:
            view = "Modulation"
        else:
            view = "Sound"
        if semantic in ("Waveform", "FilterType", "Transient", "OutputRouting"):
            widget = "enumerated-selector"
        elif semantic == "Boolean":
            widget = "toggle"
        elif semantic == "MidiNote":
            widget = "note-selector"
        else:
            widget = "knob-with-value-entry"
        ui = f"{view}: {owner}"
    scale = info.get("scale", "raw")
    display = {
        "PM63": "signed value = raw - 63",
        "NOTE_NAME": "MIDI note name plus integer",
        "MENU:lfo_waves": "eight verified LFO waveform names",
        "MENU:filter": "source filter enumeration; 0..8 valid",
        "MENU:waveform": "built-in waveform names; user SD waves unsupported",
        "MENU:trans": "source transient enumeration",
        "MENU:audio_out": "source hardware-route enumeration with plugin topology warning",
        "ON_OFF": "Off/On",
        "MIX_FM": "FM/PM mode",
    }.get(scale, f"source scale tag {scale}")
    validation = "integer 0..127; reject nonfinite/out-of-range input"
    if semantic == "Waveform":
        validation += "; preserve unsupported source values but disable unsupported user-SD selections"
    if semantic == "FilterType":
        validation += "; present only source-defined filter types 0..8"
    if semantic == "OutputRouting":
        validation += "; explain ST1-only exposed plugin output topology"
    return {
        "parameter_id": str(ident), "control_id": f"lxr.param.{ident:03d}",
        "stable_internal_name": descriptor["name"], "user_label": title(descriptor["name"]),
        "owner_voice": owner, "type": "discrete-integer", "minimum": "0",
        "maximum": "127", "default": str(descriptor["default"]), "unit": "raw",
        "display_conversion": display, "normalized_conversion": "raw / 127",
        "automatable": "yes", "implementation_status": status, "view": view,
        "widget": widget, "validation_rules": validation,
        "visibility_rules": hidden or "Visible for selected owner; exact readback always available.",
        "accessibility_label": f"{owner} {title(descriptor['name'])}",
        "ui_representation": ui,
        "source": f"LXRParamContract.h:{ident}; param_map.md; {info.get('engine', '-')}",
    }


def extension_rows() -> list[dict[str, str]]:
    owners = ("D1", "D2", "D3", "SN", "CY", "HH")
    result = []
    for ident in range(228, 240):
        source = "Velocity" if ident < 234 else "LFO"
        owner = owners[(ident - 228) % 6]
        result.append({
            "parameter_id": str(ident), "control_id": f"lxr.param.{ident:03d}",
            "stable_internal_name": f"{source.upper()}_DESTINATION_{owner}",
            "user_label": f"{source} Destination {owner}", "owner_voice": owner,
            "type": "typed-route-token", "minimum": "0", "maximum": "65535",
            "default": "0", "unit": "target identity",
            "display_conversion": "OFF or original target label plus full-width identity",
            "normalized_conversion": "token / 65535", "automatable": "yes",
            "implementation_status": "implemented", "view": "Modulation",
            "widget": "voice-filtered-searchable-selector",
            "validation_rules": "OFF=0; accept only typed own-voice targets; retain prior route on invalid token",
            "visibility_rules": "Visible for selected modulation owner.",
            "accessibility_label": f"{owner} {source} destination",
            "ui_representation": "Modulation route selector",
            "source": f"RouteHostControls.h:{ident}; RoutingStateAuthority",
        })
    host = [
        (240, "SEQUENCER_ENABLE", "Sequencer Enable", "toggle", 0, 1, 0, "Sequencer", "Off/On"),
        (241, "SEQUENCER_CLOCK", "Sequencer Clock", "segmented-selector", 0, 1, 0, "Overview", "Internal/Host Sync"),
        (242, "SEQUENCER_INTERNAL_BPM", "Internal BPM", "numeric-stepper", 1, 255, 120, "Overview", "BPM integer"),
        (243, "SEQUENCER_ROLL_RATE", "Roll Rate", "enumerated-selector", 0, 13, 8, "Sequencer", "One-shot, 1/1 through 1/128"),
        (244, "SEQUENCER_ROLL_TRACKS", "Roll Tracks", "seven-track-momentary-pad-bank", 0, 127, 0, "Sequencer", "Released or held-track names"),
        (245, "MORPH_AMOUNT", "Morph Amount", "knob-with-value-entry", 0, 255, 0, "Kits and Morph", "integer 0..255"),
    ]
    for ident, name, label, widget, minimum, maximum, default, view, display in host:
        transient = ident >= 243
        result.append({
            "parameter_id": str(ident), "control_id": f"lxr.param.{ident:03d}",
            "stable_internal_name": name, "user_label": label, "owner_voice": "Global",
            "type": "bitmask" if ident == 244 else "discrete-integer",
            "minimum": str(minimum), "maximum": str(maximum), "default": str(default),
            "unit": "BPM" if ident == 242 else "raw", "display_conversion": display,
            "normalized_conversion": f"(raw - {minimum}) / {maximum - minimum}",
            "automatable": "yes", "implementation_status": "implemented",
            "view": view, "widget": widget,
            "validation_rules": f"integer {minimum}..{maximum}; bounded stale-generation handling",
            "visibility_rules": "Always visible in its owning view.",
            "accessibility_label": label,
            "ui_representation": "Transient; not LXRS-persisted." if transient else "Primary view control.",
            "source": f"LXRParamContract.h:{ident}; LXR.cpp registration and OnParamChange",
        })
    return result


def write_design() -> None:
    DESIGN.mkdir(parents=True, exist_ok=True)
    mapped = param_map()
    rows = [native_row(row, mapped) for row in native_descriptors()] + extension_rows()
    with INVENTORY.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=COLUMNS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def verify_inventory() -> dict:
    with INVENTORY.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    errors = []
    if list(rows[0].keys()) != COLUMNS:
        errors.append("column contract mismatch")
    ids = [int(row["parameter_id"]) for row in rows]
    if ids != list(range(246)):
        errors.append("parameter IDs are not exactly 0..245")
    controls = [row["control_id"] for row in rows]
    if len(set(controls)) != 246:
        errors.append("control IDs are not unique")
    for row in rows:
        if any(row[column] == "" for column in COLUMNS):
            errors.append(f"empty field in parameter {row['parameter_id']}")
        if row["widget"] == "none" and not row["visibility_rules"].startswith("Hidden:"):
            errors.append(f"hidden reason absent for parameter {row['parameter_id']}")
    expected = native_descriptors()
    for descriptor, row in zip(expected, rows[:228]):
        for key, csv_key in (("name", "stable_internal_name"), ("default", "default"),
                             ("min", "minimum"), ("max", "maximum")):
            if str(descriptor[key]) != row[csv_key]:
                errors.append(f"descriptor mismatch {descriptor['id']} {csv_key}")
    for ident in range(228, 240):
        if rows[ident]["maximum"] != "65535":
            errors.append(f"route {ident} was truncated")
    hidden = [row for row in rows if row["widget"] == "none"]
    return {"rows": len(rows), "visible": len(rows) - len(hidden), "hidden": len(hidden),
            "errors": errors, "status": "PASS" if not errors else "FAIL"}


def verify_operations() -> dict:
    with OPERATIONS.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    errors = []
    for row in rows:
        source = ROOT / row["source_file"]
        if not source.is_file():
            errors.append(f"missing source {row['source_file']}")
        elif row["source_symbol"] not in source.read_text(encoding="utf-8"):
            errors.append(f"missing symbol {row['source_symbol']}")
    return {"rows": len(rows), "errors": errors,
            "status": "PASS" if not errors else "FAIL"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--compiler", choices=("gcc", "vs-llvm"), default="gcc")
    parser.add_argument("--cc", default="C:/msys64/ucrt64/bin/gcc.exe")
    parser.add_argument("--cxx", default="C:/msys64/ucrt64/bin/g++.exe")
    parser.add_argument("--write-design", action="store_true")
    args = parser.parse_args()
    if args.write_design:
        write_design()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    inventory = verify_inventory()
    operations = verify_operations()
    with INVENTORY.open(newline="", encoding="utf-8") as source, \
         (output / "parameter_control_inventory.csv").open("w", newline="", encoding="utf-8") as target:
        target.write(source.read())
    with OPERATIONS.open(newline="", encoding="utf-8") as source, \
         (output / "operation_binding_matrix.csv").open("w", newline="", encoding="utf-8") as target:
        target.write(source.read())

    env = os.environ.copy()
    compiler = pathlib.Path(args.cxx)
    clang = args.compiler == "vs-llvm"
    if clang:
        sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
        from run_checkpoint_tests import msvc_environment, visual_studio
        vs = visual_studio()
        env = msvc_environment(vs, "14.51")
        compiler = vs / "VC/Tools/Llvm/x64/bin/clang-cl.exe"
    else:
        env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")

    test = ROOT / "tests/P19AGUIContractTest.cpp"
    production = [ROOT / "dsp/PatternBank.cpp"]
    runs = []
    for optimization in ("O0", "O2"):
        build = output / optimization
        build.mkdir()
        executable = build / "P19AGUIContractTest.exe"
        if clang:
            command = [str(compiler), "/nologo", "/std:c++17", "/GR-", "/EHs-c-", "/MD",
                       "/Od" if optimization == "O0" else "/O2", "/W4",
                       "/I" + str(ROOT), str(test), *map(str, production), "/Fe" + str(executable)]
        else:
            command = [str(compiler), "-std=c++17", "-fno-exceptions", "-fno-rtti",
                       "-Wall", "-Wextra", f"-{optimization}", "-I" + str(ROOT),
                       str(test), *map(str, production), "-o", str(executable)]
        built = run(command, env)
        row = {"optimization": optimization, "build": built}
        if built["status"] == "PASS":
            executed = run([str(executable)], env)
            if executed["status"] == "PASS":
                try:
                    executed["metrics"] = json.loads(executed["stdout"])
                    if executed["metrics"]["failures"] != 0:
                        executed["status"] = "FAIL"
                except (json.JSONDecodeError, KeyError):
                    executed["status"] = "FAIL"
            executed["executable_sha256"] = sha(executable)
            row["execute"] = executed
        runs.append(row)

    status = [row.get("execute", row["build"])["status"] for row in runs]
    classification = "PASS" if inventory["status"] == operations["status"] == "PASS" and status == ["PASS", "PASS"] else "FAIL"
    result = {"schema": "P19A-GUI-contract-v1", "utc": datetime.now(timezone.utc).isoformat(),
              "classification": classification, "compiler": args.compiler,
              "inventory": inventory, "operations": operations, "runs": runs,
              "source_hashes": {str(path.relative_to(ROOT)).replace("\\", "/"): sha(path)
                                for path in (test, INVENTORY, OPERATIONS,
                                             ROOT / "vendor/iPlug2/Examples/LXR/LXRParamContract.h",
                                             ROOT / "dsp/RouteHostControls.h", ROOT / "dsp/PatternBank.h")}}
    (output / "control_contract_results.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"classification": classification, "compiler": args.compiler,
                      "inventory": inventory, "operations": operations,
                      "runs": status}))
    return 0 if classification == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
