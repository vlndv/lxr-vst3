#!/usr/bin/env python3
"""P1: export LXR 0.37 data tables from the original C source to binary + CSV.

Usage: python tools/export_data.py --src ref/LXR-0.37/mainboard/LxrStm32/src --out data
Python 3 stdlib only. Fails loudly if any element count differs from the expected one.
Licence: exported data is derived from the original firmware (custom non-commercial licence).
"""
import argparse, hashlib, json, os, re, struct, sys

# name, file (relative to src), C identifier, struct fmt, expected count, csv
TABLES = [
    ("sine_table",             "DSPAudio/wavetable.c",       "sine_table",           "h", 4097,       False),
    ("saw_table",              "DSPAudio/wavetable.c",       "sawTable",             "h", 11 * 1024,  False),
    ("tri_table",              "DSPAudio/wavetable.c",       "triTable",             "h", 11 * 1024,  False),
    ("rec_table",              "DSPAudio/wavetable.c",       "recTable",             "h", 11 * 1024,  False),
    ("crash_sample",           "DSPAudio/Samples.c",         "crashSample",          "B", 32768,      False),
    ("transient_data",         "DSPAudio/transientTables.c", "transientData",        "b", 12 * 2205,  False),  # last row has 13 implicit zeros
    ("transient_volume_table", "DSPAudio/transientTables.c", "transientVolumeTable", "f", 69,         True),
    ("midi_note_frequencies",  "MIDI/MidiNoteNumbers.h",     "MidiNoteFrequencies",  "f", 128,        True),
    ("sqrt_lut",               "DSPAudio/squareRootLut.c",   "squareRootLut",        "f", 128,        True),
]
NUM = re.compile(r"[-+]?(?:0[xX][0-9a-fA-F]+|\d+\.?\d*(?:[eE][-+]?\d+)?|\.\d+(?:[eE][-+]?\d+)?)[fF]?")

def strip_comments(t):
    t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
    return re.sub(r"//[^\n]*", "", t)

def extract(text, ident):
    m = re.search(r"\b%s\s*(?:\[[^\]]*\]\s*)+=\s*\{" % re.escape(ident), text)
    if not m:
        m = re.search(r"\b%s\s*(?:\[[^\]]*\]\s*)*=\s*\{" % re.escape(ident), text)
    if not m:
        sys.exit("identifier not found: " + ident)
    i, depth, start = m.end() - 1, 0, m.end() - 1
    while True:
        c = text[i]
        depth += (c == "{") - (c == "}")
        if depth == 0:
            break
        i += 1
    body = text[start:i + 1]
    out = []
    for tok in NUM.findall(body):
        t = tok.rstrip("fF") if not tok.lower().startswith("0x") else tok
        out.append(int(t, 16) if t.lower().startswith(("0x", "-0x", "+0x")) else
                   (float(t) if any(ch in t for ch in ".eE") else int(t)))
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    cache, manifest = {}, {}
    for name, rel, ident, fmt, count, csv in TABLES:
        if rel not in cache:
            with open(os.path.join(a.src, rel), encoding="latin-1") as f:
                cache[rel] = strip_comments(f.read())
        vals = extract(cache[rel], ident)
        explicit = len(vals)
        if explicit != count:
            # C zero-fills missing trailing initialisers; only tolerated for this known case
            if ident == "transientData" and explicit == count - 13:
                vals += [0] * (count - explicit)
            else:
                sys.exit("%s: expected %d values, got %d" % (ident, count, len(vals)))
        if fmt == "b":  # hex literals such as 0xFF stored in int8_t wrap to negative (gcc behaviour)
            vals = [v - 256 if v > 127 else v for v in vals]
        raw = struct.pack("<%d%s" % (count, fmt), *vals)
        with open(os.path.join(a.out, name + ".bin"), "wb") as f:
            f.write(raw)
        if csv:
            with open(os.path.join(a.out, name + ".csv"), "w") as f:
                f.write("index,value\n")
                for i, v in enumerate(vals):
                    f.write("%d,%s\n" % (i, repr(v)))
        manifest[name] = {"source": rel, "c_name": ident, "count": count, "explicit_values_in_source": explicit,
                          "dtype": {"h": "int16", "B": "uint8", "b": "int8", "f": "float32"}[fmt],
                          "endian": "little", "sha256": hashlib.sha256(raw).hexdigest()}
        print("OK %-24s %6d x %-7s %s" % (name, count, manifest[name]["dtype"], manifest[name]["sha256"][:12]))
    manifest["_note"] = "Derived from SonicPotions/LXR fw 0.37 (custom non-commercial licence). Do not sell or use commercially."
    with open(os.path.join(a.out, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)

if __name__ == "__main__":
    main()
