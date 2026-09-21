#!/usr/bin/env bash
# Builds the original 0.37 envelope code (SlopeEg2.c, Decay.c, snapEg.c) and compares dsp/Envelopes.cpp with it, bitwise.
# Usage (repo root, MSYS2/Linux bash):  bash tests/reference/envelopes/make_ref.sh [path to ref/LXR-0.37]
set -e
REF="${1:-ref/LXR-0.37}"; SRC="$REF/mainboard/LxrStm32/src/DSPAudio"; KIT="tests/reference/envelopes"; OUT="out/env_ref"
mkdir -p "$OUT"
for f in SlopeEg2 Decay snapEg; do gcc -O0 -ffp-contract=off -fgnu89-inline -w -I"$KIT/stub" -I"$SRC" -c "$SRC/$f.c" -o "$OUT/$f.o"; done
g++ -std=c++17 -O0 -ffp-contract=off -Idsp "$KIT/diff_test.cpp" dsp/Envelopes.cpp "$OUT/SlopeEg2.o" "$OUT/Decay.o" "$OUT/snapEg.o" -lm -o "$OUT/diff_test"
"$OUT/diff_test"
