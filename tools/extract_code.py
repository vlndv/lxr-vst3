#!/usr/bin/env python3
"""Extract the code blocks of a Qwen answer into files.

Usage (from the repo root):  python tools/extract_code.py out/P2a_answer.md
Each ``` block whose first line is a comment holding a path (for example `// dsp/ParamMapEnv.h`) is written to that path.
Files whose name ends in Test.cpp are never written (the tests are supplied by hand). Existing files are overwritten.
Terminal wrap artefacts (ESC[nD ESC[K) in text copied from a console are removed first.
"""
import re, sys, os

def clean_terminal(t):
    """Undo terminal word-wrap redraws copied from a console: '<partial word>ESC[nD ESC[K newline' -> drop the n
    characters before the sequence and the newline (the word is printed again right after). Also drops CR and other ESC codes."""
    while True:
        m = re.search(r"\x1b\[(\d+)D\x1b\[K\r?\n", t)
        if not m:
            break
        t = t[:m.start() - int(m.group(1))] + t[m.end():]
    t = re.sub(r"\x1b\[[0-9;?]*[A-Za-z]", "", t)
    return t.replace("\r\n", "\n").replace("\r", "")

def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    text = clean_terminal(open(sys.argv[1], encoding="utf-8-sig", errors="replace").read())
    written = 0
    for m in re.finditer(r"```[a-zA-Z+]*\r?\n(.*?)```", text, flags=re.S):
        body = m.group(1).replace("\r\n", "\n")
        first = body.split("\n", 1)[0].strip()
        p = re.match(r"(?://|#)\s*([\w./\\-]+\.(?:h|hpp|cpp|c))\s*$", first)
        if not p:
            continue
        path = p.group(1).replace("\\", "/")
        if path.startswith("/") or ".." in path.split("/"):
            print("SKIP (unsafe path):", path); continue
        if path.endswith("Test.cpp"):
            print("SKIP (test file is supplied by hand):", path); continue
        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(body)
        print("WROTE", path, len(body), "bytes"); written += 1
    if not written:
        sys.exit("no code block with a path comment on its first line was found")

if __name__ == "__main__":
    main()
