#!/usr/bin/env bash
# Builds the reference harness (and the differential test) from the ORIGINAL 0.37 source.
# Usage (from the repo root, in an MSYS2/Linux bash):  bash tests/reference/oscillator/make_ref.sh [path to ref/LXR-0.37]
# Output: out/osc_ref/harness_out.txt and out/osc_ref/diff_test(.exe)
set -e
REF="${1:-ref/LXR-0.37}"
SRC="$REF/mainboard/LxrStm32/src"
KIT="tests/reference/oscillator"
OUT="out/osc_ref"
rm -rf "$OUT"; mkdir -p "$OUT/DSPAudio" "$OUT/MIDI" "$OUT/SampleRom"
for f in Oscillator.c Oscillator.h wavetable.c wavetable.h Samples.c Samples.h random.h; do cp "$SRC/DSPAudio/$f" "$OUT/DSPAudio/"; done
cp "$SRC/MIDI/MidiNoteNumbers.h" "$OUT/MIDI/"; cp "$SRC/config.h" "$OUT/"
cp "$KIT/stub/SampleRom/SampleMemory.h" "$OUT/SampleRom/"
# The ONLY change to the original code: float -> uint32 conversions use ARM VCVT.U32.F32 semantics (saturating)
# instead of x86 behaviour. arm-none-eabi-gcc 13.2 with the firmware flags emits vcvt.u32.f32 for these casts.
sed -e 's/(uint32_t)(modBuffer\[i\]\*osc->fmMod)/f2u_sat(modBuffer[i]*osc->fmMod)/g' \
    -e 's/(uint32_t)(modOsc->output\*osc->fmMod)/f2u_sat(modOsc->output*osc->fmMod)/g' \
    -e 's/(uint32_t)(f\/440.f)/f2u_sat(f\/440.f)/g' \
    -e 's/return (((TABLESIZE\*f)\/REAL_FS))\*1048576 ;/return f2u_sat((((TABLESIZE*f)\/REAL_FS))*1048576);/' \
    -e 's/return (((1024\*f)\/REAL_FS))\*4194304 ;/return f2u_sat((((1024*f)\/REAL_FS))*4194304);/' \
    -e 's/return (((1024\*f)\/REAL_FS))\*131072 ;/return f2u_sat((((1024*f)\/REAL_FS))*131072);/' \
    "$SRC/DSPAudio/Oscillator.c" > "$OUT/DSPAudio/Oscillator.c"
CFLAGS="-O0 -ffp-contract=off -fgnu89-inline -w -I$KIT/stub -I$OUT -I$OUT/MIDI -I$OUT/DSPAudio -include $KIT/stub/arm_sat.h"
gcc $CFLAGS -c "$OUT/DSPAudio/Oscillator.c" -o "$OUT/osc.o"
gcc $CFLAGS -c "$OUT/DSPAudio/wavetable.c" -o "$OUT/wt.o"
gcc $CFLAGS -c "$OUT/DSPAudio/Samples.c" -o "$OUT/smp.o"
gcc $CFLAGS -o "$OUT/harness" "$KIT/harness.c" "$OUT/osc.o" "$OUT/wt.o" "$OUT/smp.o" -lm
"$OUT/harness" > "$OUT/harness_out.txt"
echo "wrote $OUT/harness_out.txt"
if [ -f "$KIT/diff_test.cpp" ] && [ -f dsp/Oscillator.cpp ]; then
  g++ -std=c++17 -O0 -ffp-contract=off -Idsp -I"$OUT/MIDI" -include cstdint "$KIT/diff_test.cpp" dsp/Oscillator.cpp dsp/OscTables.cpp dsp/ParamMapMisc.cpp \
      "$OUT/osc.o" "$OUT/wt.o" "$OUT/smp.o" -lm -o "$OUT/diff_test"
  "$OUT/diff_test"
fi
