# P15-M2B original-C base mappings and timing

Run from the repository root with the GCC/G++ setup in docs/build_and_test.md:

```powershell
python tools/verification/run_base_mapping_tests.py --output out/new-base-mapping-run
```

The runner refuses existing output directories, snapshots DSP/tests/assets and
the pinned original mainboard sources, then generates and compiles the oracle
as GNU C11 at O0/O2. C++17 uses the same optimization and `-ffp-contract=off`,
with exceptions and RTTI disabled. Compilers can be selected with `--cc`/`--cxx`.
Objects, generated sources, commands, executable hashes, snapshots and CSV/JSON
results remain in the new ignored output. It never rebuilds/installs a plugin,
changes Windows policy or writes to the original firmware.

`generate_reference.py` reuses the established snare hardware shim and original
C extraction approach from P15-M1. Original DSP translation units are included
unchanged. Actual original drum/cymbal/hat structures, parameter enums/types,
two buffer-gain functions and complete CC/NRPN functions are extracted verbatim;
function source paths, lines, text and hashes are recorded. Extraction does not
replace synthesis expressions. The small adapter provides initialization,
parameter writes, direct voice/operator/filter rendering, field observation and
matched snapshots. No renderer is stubbed. Trigger output/peripherals are inert;
test RNG is the established deterministic xorshift supplied on both sides.
Routes remain off. Test-only UART hooks and LFO/node code satisfy the complete
original handler's dependencies; no plugin modulation integration is involved.

Both sides load the same original exported note/wave/transient tables. The
fixture explicitly clears previous-input filter storage to match zeroed firmware
globals, as the existing snare comparison does. It does not alter runtime
initialization, factory defaults or original filter equations. The full base
patch is the current plugin's explicit raw defaults followed by disclosed test
knobs; it is not claimed to be an extracted hardware factory kit.

Coverage:

- Every 0..127 operator frequency/gain byte, source-note 0/63/127, preserved
  fine bytes, independent operators and absence of drum side effects.
- Independent operator PCM at min/mid/max with live edits and trigger/retrigger;
  combined two-operator FM at all nine min/mid/max gain pairs on CY and HH.
- Every cutoff byte on six voices, all eight filter types, resonance endpoints,
  matched synthetic filter inputs, live voice PCM and repeated triggers.
- Every coarse/fine byte on SN/CY/HH with source-note extremes, pre/post-trigger
  writes, unchanged phase and deferred increment, exact native tick consumption.
- Independent edited/unedited original-C and C++ counterfactuals identify the
  first affected sample for all six live pitch IDs, including after-async edits.
- Drum attacks 0/1/127 on all three voices, zero/full velocity, retriggers,
  decay, STOPPED/ATTACK/DECAY/REPEAT initial states and gain interpolation endpoints.
- Allocation counting during mapped setters and representative render blocks.

All deterministic PCM assertions require integer equality; field checks require
float bit equality. No tolerance was relaxed. `pcm_comparison.csv` records each
32-frame case, mismatch count, first differing sample, maximum and RMS error.
`pitch_timing.csv` separately reports the first changed sample against independent
unedited paths, not an error against the reference.

The original C negative-float-to-uint FM conversion is undefined on x64, while
the port deliberately models ARM saturation. Combined FM cases therefore set
identical positive-crest initial operator phases and low operator frequencies
(40/48 coarse) so the entire compared 32-frame interval has nonnegative modulation.
This is an explicit matched initial condition using original assets, not a source
patch or relaxed assertion. All operator pitch endpoints are additionally tested
through direct original-C operator PCM; pitch timing and filter cases mute FM.
These tests do not certify arbitrary negative-FM hardware output, ARM fast-math/
FMA, unknown ROM guards, factory-kit bytes or uncontrolled hardware RNG.

Original Copyright 2013 Julian Schmidt. Included/extracted firmware and derived
fixtures remain subject to LICENSE-LXR.txt; preserve the original notices and
full-source/non-commercial conditions.
