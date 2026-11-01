# LXR snare renderer restoration and fidelity verification

Date: 2026-10-26  
Branch: `p12b-4c-fir-resampler`  
Starting HEAD: `280279debe94cb26810ea89b89f36b9bf965a747`  
Overall classification: **BLOCKED** (implementation and host integration pass;
two required regression executables are blocked by Windows Application Control)

## Authoritative reference and root cause

The authoritative implementation inspected was Sonic Potions LXR firmware 0.37
at commit `dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`, specifically
`ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio/Snare.c` and its oscillator,
envelope, filter, transient and distortion dependencies. The Catalyst fork was
not used.

The inherited `SnareVoice::calcSyncBlock()` in `dsp/Snare.cpp` filled every
sample with zero. It originated before this branch and made the native snare
silent regardless of correct engine and MIDI routing. Two integration omissions
were also exposed: transient tables were not loaded by `Engine::loadAssets()`,
and the two transient resource files were not copied into the packaged VST3.

## Production repair

`SnareVoice::calcSyncBlock()` now preserves the original firmware operation
order and numeric behaviour:

1. Render the noise oscillator at gain `0.9f`.
2. Run the noise buffer through `SVF_calcBlockZDF`.
3. Render the transient and add it to the filtered noise with saturating
   arithmetic.
4. Render the tonal oscillator with gain `1.0f - mix`.
5. Scale the noise/transient path by `mix`, preserving the original narrowing
   to signed 16-bit, then saturating-add the tonal path.
6. Compute the amplitude-envelope gain once per block, multiply by velocity
   only when velocity modulation is enabled, then multiply by channel volume.
7. Apply that gain to the mixed buffer and run the existing distortion stage.

The original block/sample boundary, pitch-envelope updates in `calcAsync()`,
phase resets, float-to-int16 conversions, and saturation points are retained.
The original distortion initialization conversion (`2.0f` into its 8-bit
setting) and the firmware quirk that does not transpose the snare noise
oscillator from the incoming note are preserved and annotated. No FIR, MIDI
mapping, preset, UI, sequencer, engine-rate or block-size logic was changed.

`Engine::loadAssets()` now loads the exported transient tables before rendering,
outside the audio thread. The Windows resource packager now installs
`transient_data.bin` and `transient_volume_table.bin` alongside the existing
assets.

## Independent original-C differential test

`tests/reference/snare/original_snare_harness.c` directly compiles the unchanged
firmware C sources (`Decay.c`, `SlopeEg2.c`, `snapEg.c`, `distortion.c`,
`ResonantFilter.c`, `transientGenerator.c`, `Oscillator.c`, and `Snare.c`) behind
a narrow compatibility layer. It does not reuse the C++ renderer as its
reference. Hardware random generation is replaced only in the test harness by
a deterministic xorshift stream; the same sequence is supplied to both sides.
The comparison uses exported assets from `data/`.

Compiler modes:

- C reference: GCC GNU11, `-fgnu89-inline`, `-ffp-contract=off`, at `-O0` and
  `-O2`.
- C++ port: GCC C++17, `-ffp-contract=off`, at `-O0` and `-O2`.

Both modes passed 61 assertions with zero counted render-time allocations.
Across 12 cases and 4,864 compared samples per optimization level, the result
was bit-exact: zero mismatches, maximum absolute difference 0, and difference
RMS 0. Cases covered tonal-only, noise-only, mixed, transient, low-pass,
naive-two-pole filter, maximum distortion, velocity-zero with modulation on and
off, repeated/retriggered notes, and parameter minima/maxima. Canary checks
found no buffer overruns and all PCM was valid signed 16-bit data.

Representative PCM measurements:

| Case | Frames | Peak | RMS |
|---|---:|---:|---:|
| Mixed original/C++ reference | 384 | 18,088 | 7,623.03238701 |
| Transient enabled | 384 | 16,629 | 6,483.35763063 |
| Native `Engine::processBlock()` snare | 3,200 | 12,038 | 822.108288469 |
| FIR adapter at 48 kHz | test render | 0.408415547799 | 0.0163783420329 |

The zero-velocity, envelope-decay, repeated-trigger, filter, transient,
distortion and tonal/noise isolation assertions all passed at both optimization
levels.

## Regression and package results

- `SnareCymbalHiHatTest`: 47/47 at `-O0` and `-O2`.
- New snare renderer suite: 61/61 at `-O0` and `-O2`, bit-exact to original C.
- Runtime silence suite: 419/419 at `-O0` and `-O2`; the retained snare-audibility
  assertion that previously failed now passes.
- Original-C filter differential: 80,000 blocks, zero mismatches at both levels.
- FIR functional: 7/7 at both levels.
- FIR quality: 38/38 at `-O0`, alias rejection -77.74 dB. The required `-O2`
  executable was blocked by Windows Application Control.
- FIR latency: 214/214 at both levels; reported counts remain
  17/24/26/47/51/103 samples at 32/44.1/48/88.2/96/192 kHz.
- General DSP batch: all runnable tests passed. The required `Mixer_test.exe`
  at `-O0` was blocked by Windows Application Control; the `-O2` mixer suite
  passed 28/28.
- Installed-VST3 runtime test: 1,143/1,143. Channel 4 produced peak
  `0.408415548`; all channel-isolation, state, finite-output and latency checks
  passed.
- Steinberg validator: 47 passed, 0 failed.
- MSBuild 18.10, Release x64, v145: exit 0. Packaging included all seven assets,
  whose installed hashes match `data/`.

The exact installed binary tested was:

`C:\Users\valen\AppData\Local\Programs\Common\VST3\LXR.vst3\Contents\x86_64-win\LXR.vst3`

SHA-256:
`F51FF17924512FD364ADE5704E0A6CEFBFCD930AE91F803E576343F8DF0DA5B8`

## REAPER verification

REAPER 7.80/x64 ran fresh isolated project instances against that exact SHA-256.
At 48 kHz, channel 4, note 60, velocity 110, three snare hits (six MIDI events)
rendered 115,200 stereo frames with peak `0.410337090492249` and RMS
`0.00756627536108755`. The output was finite and nonzero; per-hit decay-window
ratios were `0.0606927033`, `0.0884991543`, and `0.0516645248`.

FIR/PDC remained unchanged: the plugin and chain reported 26 samples. The normal
render began at frame 23,538 and the ignore-PDC render at 23,564, an independently
measured 26-frame shift with zero alignment error and zero aligned-waveform RMS
error. The nominal nearest-integer fractional residual was
`-0.365243087569262` frame.

An unaffected channel-1 D1 render also passed (peak `0.0500143766403198`, RMS
`0.00161204972515369`, 67,200 frames, 26-frame PDC shift). A saved-project reload
loaded two LXR instances with PDC `[26, 26]` and completed two start/stop cycles.
Machine-readable evidence is under `out/reaper-p12b4c/`, including:

- `snare_repair_final_48000_results.json` and its Markdown report, project and
  paired WAV renders;
- `snare_repair_final_d1_48000_results.json`;
- `snare_repair_final_transport_reload.json`.

The renders objectively verify snare audio and decay. Subjective loudspeaker or
headphone listening after this repair was not performed and is **NOT RUN**.

## Process safety and remaining risks

The authorized hung isolated REAPER PID 8128 was verified by executable path,
command line, creation time and test-script argument, then it alone was
force-terminated. Its exit and release of the installed VST3 DLL lock were
verified before rebuilding. Existing reports, renders and logs were preserved.
No unrelated REAPER process or project was terminated or altered.

Two task-created isolated unsaved REAPER instances remain open after their
successful renders (PIDs 27340 and 11696); they were not terminated because the
authorization applied only to PID 8128. Their artifacts are already saved.

Remaining fidelity qualifications are limited to the test environment: the
firmware's hardware RNG is represented by a deterministic compatible stream;
the differential build ran on x64 GCC rather than the original Cortex-M4
compiler/FPU; and user SD samples (waveforms 6 and above) remain out of scope.
Warnings from original oscillator code's embedded user-sample pointer casts are
not exercised by these snare cases. No observed numerical difference remains in
the tested paths.

Because Windows Application Control still prevents execution of two explicitly
required regression binaries, the strict task result is **BLOCKED**, not PASS.
The snare renderer repair, VST3 build, installed-host validation and REAPER
render/PDC verification themselves pass, with no test failure observed. Do not
close the phase unconditionally until the blocked O0 mixer and O2 FIR-quality
executables can run under the normal machine policy.
