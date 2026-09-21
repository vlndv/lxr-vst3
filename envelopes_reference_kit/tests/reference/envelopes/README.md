# Envelope reference (P5)

`make_ref.sh` compiles the ORIGINAL `SlopeEg2.c`, `Decay.c` and `snapEg.c` (unchanged, with four empty stub headers) and runs `diff_test.cpp`,
which drives the original and `dsp/Envelopes.cpp` side by side over 60000 random scenarios (amp EG with random attack, decay, slope, repeat, sync and re-triggers;
pitch EG including slope 127; snap EG) and compares every output and every internal value bitwise. It also compares the state after `init()`.

Run from the repo root (needs `ref/LXR-0.37`, gcc, g++):
```
bash tests/reference/envelopes/make_ref.sh
```
Expected last line: `differential: 0 mismatches`.
