#!/usr/bin/env python3
"""Audit all P19-A concept geometry against the fixed 1280x800 canvas."""

from __future__ import annotations

import argparse
import json
import pathlib
import xml.etree.ElementTree as ET


ROOT = pathlib.Path(__file__).resolve().parents[2]
DESIGN = ROOT / "design" / "p19a"
NS = "{http://www.w3.org/2000/svg}"


def number(element: ET.Element, name: str, default: float = 0.0) -> float:
    return float(element.attrib.get(name, default))


def bounds(element: ET.Element) -> tuple[float, float, float, float] | None:
    tag = element.tag.removeprefix(NS)
    if tag == "rect":
        x, y = number(element, "x"), number(element, "y")
        return x, y, x + number(element, "width"), y + number(element, "height")
    if tag == "circle":
        x, y, radius = number(element, "cx"), number(element, "cy"), number(element, "r")
        return x - radius, y - radius, x + radius, y + radius
    if tag == "line":
        return (min(number(element, "x1"), number(element, "x2")),
                min(number(element, "y1"), number(element, "y2")),
                max(number(element, "x1"), number(element, "x2")),
                max(number(element, "y1"), number(element, "y2")))
    if tag == "text":
        x, baseline = number(element, "x"), number(element, "y")
        size = number(element, "font-size", 14.0)
        # Segoe UI/Arial conservative average-glyph estimate. This does not
        # claim pixel-identical shaping; it detects obvious unbounded labels.
        width = len("".join(element.itertext())) * size * 0.52
        anchor = element.attrib.get("text-anchor", "start")
        left = x - width if anchor == "end" else x - width / 2 if anchor == "middle" else x
        return left, baseline - size, left + width, baseline + size * 0.25
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path,
                        default=ROOT / "out" / "p19c" / "svg_overflow_audit.json")
    args = parser.parse_args()
    files = sorted(DESIGN.glob("*.svg"))
    reports = []
    all_pass = len(files) == 9
    for path in files:
        root = ET.parse(path).getroot()
        view_box = [float(item) for item in root.attrib.get("viewBox", "").split()]
        width = float(root.attrib.get("width", 0))
        height = float(root.attrib.get("height", 0))
        errors: list[dict[str, object]] = []
        inspected = 0
        for index, element in enumerate(root.iter()):
            box = bounds(element)
            if box is None:
                continue
            inspected += 1
            if box[0] < -0.5 or box[1] < -0.5 or box[2] > width + 0.5 or box[3] > height + 0.5:
                errors.append({
                    "element_index": index,
                    "tag": element.tag.removeprefix(NS),
                    "bounds": [round(value, 3) for value in box],
                    "text": "".join(element.itertext())[:120],
                })
        valid_canvas = width == 1280 and height == 800 and view_box == [0, 0, 1280, 800]
        font_stack = root.attrib.get("font-family", "")
        valid_font = "Segoe UI" in font_stack and "sans-serif" in font_stack
        status = "PASS" if valid_canvas and valid_font and not errors else "FAIL"
        all_pass &= status == "PASS"
        reports.append({
            "file": str(path.relative_to(ROOT)).replace("\\", "/"),
            "status": status,
            "canvas": [int(width), int(height)],
            "view_box": view_box,
            "font_stack": font_stack,
            "elements_checked": inspected,
            "overflow": errors,
        })
    report = {
        "schema": "lxr-p19c-svg-overflow-audit-v1",
        "status": "PASS" if all_pass else "FAIL",
        "files": reports,
        "text_metric": "conservative Segoe UI/Arial average-glyph extent estimate",
        "sequencer_overflow_policy":
            "Main 7x16 grid remains bounded; per-step fields use the separate bounded detail panel in 08-step-detail.svg (P19-E implementation deferred).",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "files": len(reports),
                      "overflow": sum(len(item["overflow"]) for item in reports)}))
    return 0 if all_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
