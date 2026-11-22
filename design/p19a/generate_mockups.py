#!/usr/bin/env python3
"""Generate the nine dependency-free P19-A SVG concept sheets."""

from __future__ import annotations

import html
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPEC = json.loads((HERE / "mockup_spec.json").read_text(encoding="utf-8"))
W, H = SPEC["canvas"]["width"], SPEC["canvas"]["height"]

COLORS = {
    "bg": "#101318", "panel": "#181d24", "raised": "#222934",
    "line": "#353e4b", "text": "#edf2f7", "muted": "#9aa7b5",
    "accent": "#f1b84b", "cyan": "#49c6d4", "red": "#ef6a67",
    "green": "#70c48f", "focus": "#f8df8c",
}


def esc(value: object) -> str:
    return html.escape(str(value))


def rect(x, y, w, h, fill="panel", stroke="line", radius=8, cls="") -> str:
    return f'<rect class="{cls}" x="{x}" y="{y}" width="{w}" height="{h}" rx="{radius}" fill="{COLORS.get(fill, fill)}" stroke="{COLORS.get(stroke, stroke)}"/>'


def text(x, y, value, size=14, color="text", weight=400, anchor="start", cls="") -> str:
    return f'<text class="{cls}" x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" text-anchor="{anchor}" fill="{COLORS.get(color, color)}">{esc(value)}</text>'


def line(x1, y1, x2, y2, color="line", width=1) -> str:
    return f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{COLORS.get(color, color)}" stroke-width="{width}"/>'


def button(x, y, w, label, active=False, danger=False) -> str:
    fill = "accent" if active else ("#4b2528" if danger else "raised")
    foreground = "#16130c" if active else ("#ffd8d4" if danger else COLORS["text"])
    return rect(x, y, w, 34, fill, "line", 5) + text(x + w / 2, y + 22, label, 12, foreground, 500, "middle")


def knob(x, y, label, value, accent="accent") -> str:
    parts = [f'<circle cx="{x}" cy="{y}" r="27" fill="{COLORS["raised"]}" stroke="{COLORS["line"]}" stroke-width="2"/>',
             line(x, y, x + 14, y - 18, accent, 3),
             text(x, y + 48, label, 11, "muted", 400, "middle"),
             text(x, y + 63, value, 12, "text", 500, "middle")]
    return "".join(parts)


def compact_knob(x, y, label, value, accent="accent") -> str:
    """Sound-view knob sized for the certified 1280x800 concept canvas."""
    parts = [f'<circle cx="{x}" cy="{y}" r="20" fill="{COLORS["raised"]}" stroke="{COLORS["line"]}" stroke-width="2"/>',
             line(x, y, x + 10, y - 13, accent, 3),
             text(x, y + 33, label, 10, "muted", 400, "middle"),
             text(x, y + 46, value, 11, "text", 500, "middle")]
    return "".join(parts)


def panel(x, y, w, h, title_value) -> list[str]:
    return [rect(x, y, w, h), text(x + 16, y + 26, title_value.upper(), 12, "muted", 500),
            line(x + 16, y + 37, x + w - 16, y + 37)]


def shell(title_value, selected_voice="D1", selected_tab=None) -> list[str]:
    tabs = ["Overview", "Sound", "Modulation", "Sequencer", "Kits + Morph"]
    selected_tab = selected_tab or title_value.split(" /")[0]
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="Segoe UI, Arial, sans-serif" role="img" aria-labelledby="title desc">',
             f'<title id="title">LXR {esc(title_value)} concept</title>',
             f'<desc id="desc">{esc(SPEC["notice"])}. Dark industrial drum synthesizer interface.</desc>',
             f'<rect width="{W}" height="{H}" fill="{COLORS["bg"]}"/>',
             text(24, 25, SPEC["notice"], 11, "muted", 500),
             text(24, 62, "LXR", 28, "accent", 500),
             text(91, 60, title_value, 18, "text", 500),
             text(910, 40, "KIT", 10, "muted"), text(910, 60, "NEON 808", 14, "text", 500),
             text(1040, 40, "CLOCK", 10, "muted"), text(1040, 60, "HOST 124.0", 14, "cyan", 500),
             text(1180, 40, "STATUS", 10, "muted"), text(1180, 60, "PLAYING", 14, "green", 500),
             line(24, 78, 1256, 78)]
    x = 24
    for tab in tabs:
        width = 146 if tab != "Kits + Morph" else 170
        parts.append(button(x, 92, width, tab, tab == selected_tab))
        x += width + 8
    voices = ["D1", "D2", "D3", "SN", "CY", "CH", "OH"]
    x = 940
    for voice in voices:
        parts.append(button(x, 92, 40, voice, voice == selected_voice))
        x += 44
    return parts


def overview(parts: list[str]) -> None:
    parts += panel(24, 144, 300, 210, "Kit and clock")
    parts += [text(42, 198, "Current kit", 12, "muted"), text(42, 222, "NEON 808", 22, "text", 500),
              button(42, 246, 112, "Import .snd"), button(164, 246, 112, "Export .snd"),
              text(42, 312, "Internal BPM", 12, "muted"), text(145, 312, "120", 18, "text", 500),
              button(190, 294, 86, "Host Sync", True)]
    parts += panel(340, 144, 916, 210, "Seven-track activity")
    tracks = ["D1", "D2", "D3", "SN", "CY", "CH", "OH"]
    for i, track in enumerate(tracks):
        x = 362 + i * 122
        parts += [text(x + 42, 188, track, 13, "text", 500, "middle"),
                  rect(x, 204, 84, 82, "raised", "line", 6),
                  f'<rect x="{x+10}" y="{270-i*5}" width="64" height="{i*5+6}" rx="3" fill="{COLORS["cyan"]}"/>',
                  button(x, 298, 84, "MUTE" if i in (3, 6) else "SELECT", i == 0, i == 6)]
    parts += panel(24, 372, 618, 314, "Pattern and performance")
    for p in range(8):
        parts.append(button(42 + p * 68, 420, 58, str(p + 1), p == 2))
    for track in range(7):
        parts.append(text(44, 493 + track * 25, tracks[track], 11, "muted"))
        for step in range(16):
            fill = "accent" if (step + track) % 5 == 0 else ("raised" if step != 6 else "cyan")
            parts.append(rect(76 + step * 31, 480 + track * 25, 23, 18, fill, fill, 3))
    parts += panel(658, 372, 598, 314, "Morph and output")
    parts += [text(678, 420, "Target", 12, "muted"), text(750, 420, r"METAL\x01", 16, "text", 500),
              knob(744, 494, "Morph Amount", "127 / 255"), knob(854, 494, "Master Level", "100"),
              button(934, 458, 130, "Clear Target"), button(1076, 458, 150, "Export Morphed"),
              text(678, 582, "Last operation", 11, "muted"),
              text(678, 607, "Morph target loaded - source kit unchanged", 13, "green"),
              text(678, 652, "Target and amount are transient on project reload", 12, "muted")]


def sound(parts: list[str], kind: str) -> None:
    labels = {
        "drum": [("Oscillators", ["Wave", "Coarse", "Fine", "Mod Wave", "FM Freq", "FM Amount", "Mix/PM"]),
                 ("Pitch envelope", ["Decay", "Amount", "Slope"]),
                 ("Amplitude envelope", ["Attack", "Decay", "Slope"]),
                 ("Transient", ["Wave", "Level", "Pitch"]),
                 ("Filter and output", ["Type", "Cutoff", "Resonance", "Drive", "Volume", "Pan", "Distortion", "Decimate"])],
        "snare": [("Tone and noise", ["Tone Wave", "Coarse", "Fine", "Noise Freq", "Noise Mix"]),
                  ("Pitch envelope", ["Decay", "Amount", "Slope"]),
                  ("Amplitude envelope", ["Attack", "Decay", "Slope", "Repeat"]),
                  ("Transient", ["Wave", "Level", "Pitch"]),
                  ("Noise filter and output", ["Type", "Cutoff", "Resonance", "Drive", "Volume", "Pan", "Distortion", "Decimate"])],
        "cymbal": [("Three oscillators", ["Main Wave", "Coarse", "Fine", "Mod 1 Wave", "Mod 1 Pitch", "Mod 1 Gain", "Mod 2 Wave", "Mod 2 Pitch", "Mod 2 Gain"]),
                   ("Amplitude envelope", ["Attack", "Decay", "Slope", "Repeat"]),
                   ("Transient", ["Wave", "Level", "Pitch"]),
                   ("Filter and output", ["Type", "Cutoff", "Resonance", "Drive", "Volume", "Pan", "Distortion", "Decimate"])],
        "hihat": [("Shared hi-hat engine", ["Main Wave", "Coarse", "Fine", "Mod 1 Wave", "Mod 1 Pitch", "Mod 1 Gain", "Mod 2 Wave", "Mod 2 Pitch", "Mod 2 Gain"]),
                  ("Articulation envelope", ["Attack", "Closed Decay", "Open Decay", "Slope"]),
                  ("Transient", ["Wave", "Level", "Pitch"]),
                  ("Filter and output", ["Type", "Cutoff", "Resonance", "Drive", "Volume", "Pan", "Distortion", "Decimate"])],
    }[kind]
    # Compact source-faithful bands keep every concept element inside the
    # approved 800-pixel canvas. The former 132/150-pixel stack overflowed the
    # drum and snare sheets below y=878/896.
    y = 144
    for section, controls in labels:
        height = 98
        parts += panel(24, y, 1232, height, section)
        for i, label in enumerate(controls):
            x = 70 + i * (1140 / max(1, len(controls) - 1))
            parts.append(compact_knob(x, y + 49, label,
                str((i * 19 + 63) % 128),
                "cyan" if i % 3 == 1 else "accent"))
        y += height + 6
    parts += [text(24, min(720, y + 14), "Exact values: click label or press Enter. Shift/arrow = fine adjustment. Double-click = registered default.", 12, "muted"),
              f'<rect x="48" y="173" width="44" height="44" rx="6" fill="none" stroke="{COLORS["focus"]}" stroke-width="3"/>']


def modulation(parts: list[str]) -> None:
    parts += panel(24, 144, 1232, 542, "Six source-owned modulation lanes")
    headers = ["Owner", "Velocity destination", "Velocity amount", "LFO waveform", "Rate", "Amount", "LFO destination", "Retrigger", "Phase"]
    widths = [70, 220, 120, 150, 90, 90, 220, 110, 90]
    x = 42
    for header, width in zip(headers, widths):
        parts.append(text(x + width / 2, 188, header, 11, "muted", 500, "middle")); x += width
    owners = ["D1", "D2", "D3", "SN", "CY", "HH"]
    destinations = ["Pitch / Coarse [8]", "Filter Cutoff [38]", "Volume [90]", "Noise Mix [28]", "FM Gain 2 [32]", "Open Decay [61]"]
    for row, owner in enumerate(owners):
        y = 210 + row * 68
        parts.append(rect(42, y, 1196, 56, "raised" if row % 2 == 0 else "panel", "line", 4))
        values = [owner, destinations[row], str(96-row*9), ["Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H"][row],
                  str(32+row*13), str(84-row*7), destinations[(row+2)%6], "Voice" if row < 3 else "Off", f"{row*21}/127"]
        x = 42
        for value, width in zip(values, widths):
            parts.append(text(x + width / 2, y + 34, value, 12, "text" if value != "Off" else "muted", 400, "middle")); x += width
    parts += [text(42, 650, "Destination selectors show label plus original identity; stored tokens remain 16-bit and voice-valid.", 12, "muted"),
              text(42, 670, "OFF is explicit. Invalid or unavailable targets retain the last accepted route and surface an error.", 12, "muted")]


def sequencer(parts: list[str]) -> None:
    parts += panel(24, 144, 1232, 542, "Pattern 3 - seven tracks x sixteen main steps")
    for p in range(8): parts.append(button(42 + p * 66, 184, 54, str(p+1), p == 2))
    parts += [button(600, 184, 98, "Copy"), button(708, 184, 98, "Clear", danger=True),
              button(816, 184, 116, "Euclidean"), button(942, 184, 94, "Shuffle"),
              text(1050, 207, "34 / 127", 13, "text", 500)]
    tracks = ["D1", "D2", "D3", "SN", "CY", "CH", "OH"]
    for row, track in enumerate(tracks):
        y = 252 + row * 55
        parts += [button(42, y, 48, track, row == 3), text(102, y + 21, f"L{16-row} R{row}", 11, "muted")]
        for step in range(16):
            x = 170 + step * 61
            active = (step + row * 2) % 5 == 0
            stored = (step + row) % 7 == 0
            fill = "accent" if active else "raised"
            parts.append(rect(x, y, 50, 34, fill, "focus" if step == 6 else "line", 5))
            if stored and not active:
                parts.append(f'<circle cx="{x+40}" cy="{y+9}" r="4" fill="{COLORS["cyan"]}"/>')
            if step == 4:
                parts.append(f'<rect x="{x+4}" y="{y+28}" width="42" height="3" fill="{COLORS["green"]}"/>')
        parts += [button(1158, y, 42, "M" if row != 6 else "M", danger=row == 6), button(1206, y, 32, "R")]
    parts += [text(42, 654, "Amber = gate on  |  Cyan dot = stored substep/detail even when main gate is off  |  Green underline = playhead", 12, "muted")]


def step_detail(parts: list[str]) -> None:
    parts += panel(24, 144, 760, 542, "SN - main step 7 detail")
    parts += [button(42, 186, 124, "Main Gate OFF"),
              text(184, 207, "Stored substeps remain visible and unchanged", 12, "cyan")]
    for sub in range(8):
        x = 42 + sub * 88
        parts.append(rect(x, 250, 72, 72, "accent" if sub in (2, 5) else "raised", "focus" if sub == 5 else "line", 6))
        parts.append(text(x + 36, 293, f"{sub+1}/8", 13, "#17130a" if sub in (2, 5) else "text", 500, "middle"))
    fields = [("Note", "D#3 / 51"), ("Velocity", "117 / 127"), ("Probability", "0 / 127")]
    for i, (label, value) in enumerate(fields):
        x = 42 + i * 228
        parts += [text(x, 372, label, 11, "muted"), rect(x, 384, 202, 48, "raised", "line", 5), text(x + 101, 414, value, 14, "text", 500, "middle")]
    for slot, y in ((1, 478), (2, 548)):
        parts += [text(42, y, f"Automation slot {slot}", 11, "muted"),
                  rect(170, y-24, 330, 42, "raised", "line", 5), text(186, y+2, "Filter Cutoff [40]", 13, "text"),
                  rect(516, y-24, 190, 42, "raised", "line", 5), text(611, y+2, "91 / 127" if slot == 2 else "17 / 127", 13, "text", 500, "middle")]
    parts += panel(802, 144, 454, 542, "Firmware behavior notes")
    notes = [("Probability 0 still passes when the inclusive", "random draw equals zero."),
             ("Slot 2 wins when both slots target", "the same parameter."),
             ("Turning the main gate off never erases", "stored physical positions."),
             ("Accepted locks may remain active until a later", "accepted lock or reset boundary."),
             ("OFF is stored as 0 or 255; unsupported", "targets are rejected.")]
    for i, lines in enumerate(notes):
        y = 198 + i * 82
        parts += [f'<circle cx="826" cy="{y+6}" r="5" fill="{COLORS["accent"]}"/>',
                  text(844, y, lines[0], 12, "text"),
                  text(844, y + 18, lines[1], 12, "text")]


def kits(parts: list[str]) -> None:
    parts += panel(24, 144, 598, 542, "Current kit")
    parts += [text(46, 198, "Original eight-byte identity", 11, "muted"), text(46, 230, r"NEON\x008", 26, "text", 500),
              text(46, 264, "Raw bytes are unchanged; nonprintable bytes use uppercase escapes.", 12, "muted"),
              button(46, 300, 162, "Import .snd"), button(220, 300, 162, "Export .snd"),
              button(394, 300, 182, "Host preset menu"),
              text(46, 374, "Operation status", 11, "muted"), rect(46, 390, 530, 92, "raised", "line", 5),
              text(64, 424, "Exported exactly 236 bytes", 14, "green"), text(64, 452, "C:\\Kits\\NEON_808.snd", 12, "muted"),
              text(46, 538, "Host preset restore includes kit identity when present.", 12, "muted")]
    parts += panel(638, 144, 618, 542, "Morph target")
    parts += [text(660, 198, "TARGET LOADED", 12, "green", 500), text(660, 232, r"METAL\x01", 26, "text", 500),
              button(660, 270, 176, "Load Morph Target", True), button(848, 270, 160, "Clear Target"),
              knob(726, 382, "Morph Amount", "127 / 255"),
              button(838, 354, 204, "Export Morphed Kit"),
              text(660, 474, "Source A remains the current mutable kit.", 12, "muted"),
              text(660, 500, "Target B and amount are transient and do not survive reload.", 12, "muted"),
              text(660, 548, "Last operation", 11, "muted"), rect(660, 564, 566, 72, "raised", "line", 5),
              text(678, 594, "Target replaced atomically; source kit unchanged", 13, "green"),
              text(678, 618, "No existing file overwritten", 11, "muted")]


def generate(view: dict) -> str:
    tab = {"drum": "Sound", "snare": "Sound", "cymbal": "Sound", "hihat": "Sound",
           "step": "Sequencer", "kits": "Kits + Morph"}.get(view["kind"], view["title"].split(" /")[0])
    parts = shell(view["title"], view["selected_voice"], tab)
    kind = view["kind"]
    if kind == "overview": overview(parts)
    elif kind in ("drum", "snare", "cymbal", "hihat"): sound(parts, kind)
    elif kind == "modulation": modulation(parts)
    elif kind == "sequencer": sequencer(parts)
    elif kind == "step": step_detail(parts)
    elif kind == "kits": kits(parts)
    parts.append("</svg>")
    return "\n".join(parts) + "\n"


for view in SPEC["views"]:
    (HERE / view["file"]).write_text(generate(view), encoding="utf-8", newline="\n")

print(json.dumps({"generated": [view["file"] for view in SPEC["views"]]}))
