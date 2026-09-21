# Oscillator reference (P4)

`make_ref.sh` builds the ORIGINAL 0.37 `Oscillator.c` (with two kinds of stubs) and runs two programs:
- `harness.c` prints the expected values that are embedded in `dsp/OscillatorTest.cpp` (`expected_output.txt` is the output I got with gcc x86-64).
- `diff_test.cpp` compares the port (`dsp/Oscillator.cpp`) with the original bitwise over 20000 random oscillators (80000 render blocks: all waveforms, FM with any modulator values, random pitch, gain, phase, block sizes), plus the frequency helpers and base-note code. It also checks that the exported `data/*.bin` tables equal the original arrays.

The only change to the original code is a sed step in `make_ref.sh`: `(uint32_t)float` casts and the float returns of `freq2PhaseIncr*` go through `f2u_sat`
(`stub/arm_sat.h`), which reproduces the Cortex-M4 instruction `vcvt.u32.f32` (negative and NaN give 0, too large gives 0xFFFFFFFF).
`arm-none-eabi-gcc` 13.2 with the firmware flags emits exactly that instruction for these casts. On x86 the same casts would wrap.

Run from the repo root in an MSYS2/Linux bash (needs `ref/LXR-0.37`, gcc, g++, and `data/` from `tools/export_data.py`):
```
bash tests/reference/oscillator/make_ref.sh
```
Expected last line: `differential: 80000 render blocks + helpers, 0 mismatches (bitwise on buffers and all state)`.
Outputs go to `out/osc_ref/`. Not tested on Windows/MSYS2 by me.

Host-only detail: the original reads one element past the end of `sawTable`, `triTable`, `recTable` (row 10) and `crashSample`. `diff_test.cpp` gives the port the values the host build reads there;
`harness_out.txt` shows them on the `G` line. On the hardware that value is unknown (see `docs/risks.md`).
