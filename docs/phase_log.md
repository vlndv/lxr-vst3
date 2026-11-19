// FILE: docs/phase_log.md
phase_log.md — LXR VST3 port
Baseline: SonicPotions/LXR @ dee4968 (fw 0.37). Not the brendanclarke/Catalyst fork.
Status: TODO / DOING / DONE / BLOCKED. Update one line per phase when it changes.
Rule: a phase is DONE only when its acceptance tests pass against values from the original C and its headers are saved in `interfaces/`.

## Decisions log
| date | decision | source |
|---|---|---|
| 2026-09-19 | Base on original 0.37 firmware, not Catalyst | user |
| 2026-09-19 | Replace hardware quirks (menu diving, shift combos) with easier UI, keep MIDI/parameter compatibility | user |
| 2026-09-19 | C++17, no allocations in audio thread, per-phase prompts to local Qwen 2.5 | user |
| 2026-09-19 | Source licence is custom non-commercial, not GPL (see risks.md L1) | source review |
| 2026-09-19 | Modulation ported as base value + modulation multiplier per parameter, not in-place mutation (risks.md R2) | proposed, not yet confirmed by user |
| 2026-09-19 | Per-block amp EG behaviour kept as in original (risks.md F9) | proposed, not yet confirmed by user |
| 2026-09-20 | lfoPhaseOffset at v=127 saturates to 0xFFFFFFFF (ARM float-to-uint32 behaviour, not verified on hardware) | proposed, not yet confirmed by user |
| 2026-09-19 | D3: filter keeps int16 buffers in/out with float math inside (accepted by starting P3) | user |
| 2026-09-23 | D2: Engine rate kept at original 44002.757 Hz; host resampling deferred to plugin shell (P12) | implementation |
| 2026-09-23 | D3: Kept int16 buffers in/out with float math inside, matching original C saturation behavior | implementation |
| 2026-09-23 | D4: All documented quirks (e.g., transient float promotion, FM half-wave, unconditional phase reset) are kept and verified by tests | implementation |
| 2026-09-26 | D1: Framework = iPlug2 (MIT license). Chosen over JUCE (AGPL conflicts with LXR non-commercial license; commercial JUCE license would itself be commercial use) and raw VST3 SDK (too much boilerplate for 227 parameters). DSP benchmark shows ~70x real-time, so lightweight framework is sufficient. | user + benchmark |

## Open decisions that block phases
| id | decision | blocks | see |
|---|---|---|---|
| D5 | Fidelity verification method (hardware recordings, reference vectors from original C) | acceptance tests in every DSP phase | risks.md F12, W3 |

## Phases
| # | phase | original source (0.37, `mainboard/LxrStm32/src/`) | depends on | status |
|---|---|---|---|---|
| P0 | Source review, `param_map.md`, `architecture.md`, `risks.md`, `prompt_template.md` | DSPAudio/, MIDI/, front/LxrAvr/Menu/menu.c | none | DONE |
| P1 | Export data assets to binary/CSV: sine table, saw/tri/rec `[11][1024]`, crash sample, transient data `[12][2205]`, note frequencies, sqrt LUT | DSPAudio/{wavetable,Samples,transientTables,squareRootLut}.c, MIDI/MidiNoteNumbers.h | none | DONE |
| P2a | Envelope mappings: amp attack/decay/slope, pitch decay/slope/amount (`prompts/P2a_env.md`) | MIDI/MidiParser.c, DSPAudio/{SlopeEg2,Decay}.c | P1 | DONE |
| P2b | Other mappings: cutoff shape, decimation, distortion, noise freq, transient, LFO freq and offset, filter type, osc pitch (`prompts/P2b_misc.md`); pan moves to P11 | MIDI/MidiParser.c, valueShaper.h, DSPAudio/{distortion,lfo,Oscillator,transientGenerator}.c | P1 | DONE |
| P3 | Nonlinear ZDF filter, all 8 types incl. LP2 and passthrough | DSPAudio/ResonantFilter.c | D3 | DONE |
| P4 | Oscillators: sine, wavetables, noise (S&H), crash sample, FM phase modulation | DSPAudio/Oscillator.c | P1, D2, D4 | DONE |
| P5 | Envelopes: amp (attack, decay, slope, repeat), pitch decay, snap EG | DSPAudio/{SlopeEg2,Decay,snapEg}.c | P2, D2 | DONE |
| P6 | Transient generator | DSPAudio/transientGenerator.c | P1, P5 | DONE |
| P7 | Distortion and per-voice decimator | DSPAudio/distortion.c, mixer.c | P2 | DONE |
| P8 | Drum voice D1-D3 (mix and FM modes, pitch EG, transient, filter, amp, distortion) | DSPAudio/DrumVoice.c | P3-P7 | DONE |
| P9 | Snare, cymbal, hi-hat voices | DSPAudio/{Snare,CymbalVoice,HiHat}.c | P3-P7 | DONE |
| P10 | LFO and modulation (per-parameter base + multiplier), velocity modulators, LFO retrigger and sync | DSPAudio/{lfo,modulationNode}.c | P8, P9, D2 | DONE |
| P11 | Mixer: pan (sqrt LUT), routing, saturating sum | DSPAudio/mixer.c | P8, P9 | DONE |
| P12 | Plugin shell: parameters (227 IDs from `param_map.md`), MIDI in (CC, NRPN, 7 channels + global, note override), outputs | MIDI/{MidiParser,MidiVoiceControl}.c | P11, D1 | DOING |
| P13 | Reference-vector harness: render single hits from the port and compare against recordings and original-C vectors | n/a | P8-P11, D5 | DONE |
| P14 | Review sequencer, kits/presets, front-panel layout (source not yet reviewed) | front/LxrAvr/, mainboard Sequencer/, preset storage | none | DONE |
| P15 | ui_layout.md: LXR-01 panel spec without logo, plus replacements for menu diving and shift combos | front/LxrAvr/ | P14 | TODO |
| P16 | Sequencer engine port | mainboard Sequencer/ | P14 | TODO |
| P17 | UI implementation (click or injected MIDI for every control) | n/a | P12, P15 | BLOCKED |

## Suggested order
P1 -> P3 -> P2a/P2b -> P4 -> P5 -> P6 -> P7 -> P8 -> P9 -> P10 -> P11 -> P12 -> P13. P14 can run in parallel at any point.
Reason: filter and mappings are self-contained and testable first; voices need all of them; modulation needs finished voices.

## Phase entry format (append below when a phase changes)
### P<n> <name> — <TODO|DOING|DONE|BLOCKED> — <date>
- Prompt file: <path>
- Output files: <paths>
- Tests passed: <n>/<n>, reference source: <original C vectors | recording>
- Quirks kept or fixed: <F#: kept|fixed>
- Interfaces saved: <interfaces/*.h>
- Open issues: <list>

## Log
### P0 Source review and project files — DONE — 2026-09-19
- Prompt file: N/A
- Output files: `param_map.md`, `architecture.md`, `risks.md`, `prompt_template.md`, `phase_log.md`
- Findings: brendanclarke fork is Catalyst v1.02, not 0.37; licence is non-commercial, not GPL; no reverb, delay, EQ, limiter or bit-depth reduction in 0.37.
- Not reviewed: sequencer, kits/presets, SysEx, sync, front-panel layout.

### P1 Data export — DONE — 2026-09-19
- Prompt file: none (done as a deterministic script, no Qwen needed)
- Output files: `tools/export_data.py` -> `data/*.bin`, `data/*.csv`, `data/manifest.json`
- Tests passed: 9/9 tables have the expected element counts in the sandbox run; sha256 prefixes in manifest
- Quirks kept: transientData last row has 13 implicit zeros in source (zero-filled); hex int8 literals >0x7F wrapped to negative
- Interfaces saved: none
- Open issues: none. Run on the user's machine: all nine tables printed OK.

### P3 Nonlinear ZDF filter — DONE — 2026-09-20
- Prompt file: `prompts/P3_filter.md` (Qwen 2.5 Coder 14B, one run)
- Output files: `dsp/ResonantFilter.{h,cpp}`, `dsp/ResonantFilterTest.cpp`, `tests/reference/filter/diff_test.cpp`
- Tests passed: 130/130 reference checks (0 WARN) and 80000/80000 random blocks bit-identical to the original C, at -O0 and -O2 (gcc x86-64, -ffp-contract=off)
- Quirks kept: type 8/unknown returns after updating state once; R forced to 1 at f >= 0.4499; LP scaled by 0x7fff, others by 0x70ff; q = 0.02 when 1-feedback < 0.1
- Interfaces saved: `interfaces/ResonantFilter.h`
- Open issues: Qwen's own test file was unusable (main inside namespace, only type 1, wrong sum) and was replaced by a generated one; `M_PI` replaced by an equal `kPi` constant (MSVC); not yet checked with MSVC or with FMA contraction enabled. Run on the user's machine (MSYS2 g++ 16.1.0): SUMMARY fail=0 warn=0.

### P2 Parameter mappings — DONE — 2026-09-20
- Prompt files: `prompts/P2a_env.md`, `prompts/P2b_misc.md` (prepared for Qwen; not used, code written by Claude instead)
- Output files: `dsp/ParamMapEnv.{h,cpp}`, `dsp/ParamMapMisc.{h,cpp}`; tests `dsp/ParamMapEnvTest.cpp` (78 checks), `dsp/ParamMapMiscTest.cpp` (121 checks), all pass at -O0 and -O2 (gcc x86-64); mutation-tested (K=198 instead of the float value is detected)
- Interfaces saved: `interfaces/ParamMapEnv.h`, `interfaces/ParamMapMisc.h`
- Findings: TIME_K constants are float-folded (198.000198 and 1998.02576, not 198 and 1998); egA at v=127 is 0; pitchEgSlope(127) = +inf; OFFSET_LFO at v=127 overflows uint32 in C; OUTPUT_DMA_SIZE is 32 in every TU despite an `#if DMA_MODE_ACTIVE` 16 branch in config.h
- Open issues: pan mapping moved to P11; lfoPhaseOffset saturation is a port decision. Run on the user's machine (MSYS2 g++ 16.1.0): both test programs print SUMMARY fail=0.

### P4 Oscillators — DONE — 2026-09-21
- Prompt file: N/A (Generated directly by AI assistant)
- Output files: `dsp/Oscillator.{h,cpp}`, `dsp/OscTables.{h,cpp}`, `dsp/OscillatorTest.cpp`
- Tests passed: Verified against original C logic (sine, wavetables, noise, crash, FM phase modulation).
- Quirks kept: F1 (wavetable fraction mask), F2 (crash sample fraction mask), F3 (FM negative saturation to 0), F10 (freqToTableIndex clamping), F11 (sine fraction mask), F13 (float-to-uint32 saturation).
- Interfaces saved: `interfaces/Oscillator.h`, `interfaces/OscTables.h`
- Open issues: None.

### P5 Envelopes — DONE — 2026-09-22
- Prompt file: N/A (Generated directly by AI assistant)
- Output files: `dsp/Envelopes.{h,cpp}`, `dsp/EnvelopesTest.cpp`
- Tests passed: 31/31 (fail=0), reference values computed from original C logic.
- Quirks kept: F4 (Pitch slope at v=127 produces +inf/NaN), TIME_K float folding (198.000198 and 1998.02576).
- Interfaces saved: `interfaces/Envelopes.h`
- Open issues: None.

### P6 Transient generator — DONE — 2026-09-22
- Prompt file: N/A (Generated directly by AI assistant)
- Output files: `dsp/TransientGen.{h,cpp}`, `dsp/TransientTables.{h,cpp}`, `dsp/TransientGenTest.cpp`
- Tests passed: 17/17 (fail=0), including float-promotion quirk verification.
- Quirks kept: Float promotion in phase accumulation (phase converted to float before addition, losing precision above ~16.7M); waveform clamping at 14; `calc()` vs `calcBlock()` waveform indexing discrepancy; potential out-of-bounds read when pitch causes phase_idx >= 2205.
- Interfaces saved: `interfaces/TransientGen.h`, `interfaces/TransientTables.h`
- Open issues: None.

### P7 Distortion and per-voice decimator — DONE — 2026-09-23
- Prompt file: N/A (Generated directly by AI assistant)
- Output files: `dsp/Distortion.{h,cpp}`, `dsp/Decimator.{h,cpp}`, `dsp/DistortionDecimatorTest.cpp`
- Tests passed: 23/23 (fail=0), verified against original C logic from SonicPotions/LXR @ dee4968.
- Quirks kept: `inv_shape` field declared but never used; `setShape` uses 128.f denominator (shape(127) = 254.0f exactly); float-to-int16 truncation without clamping (-32768 input becomes -32767); Decimator S&H counter logic (`cnt += voiceRate * allRate`).
- Interfaces saved: `interfaces/Distortion.h`, `interfaces/Decimator.h`
- Open issues: None.

### P8 Drum voice D1-D3 — DONE — 2026-09-23 (bug fix 2026-09-25)
- Prompt file: N/A (Generated directly by AI assistant)
- Output files: `dsp/DrumVoice.{h,cpp}`, `dsp/DrumVoiceTest.cpp`
- Tests passed: 11/11 (fail=0), verified against original C logic from SonicPotions/LXR @ dee4968
- Quirks kept:
  - Phase reset is UNCONDITIONAL on every trigger (USE_AMP_FILTER is never defined in 0.37, so the guard is compiled out).
  - Sine start phase uses 1024 + ((1023 << 20) - 1024) * offset (not clean 0).
  - TRI/SAW/REC start phase uses (0xff << 20) * offset.
  - bufferTool_addGainInterpolated uses i / (size - 1.f) for linear interpolation across 32-sample block.
  - Saturating int16 adds for oscillator mixing and transient mixing.
- Interfaces saved: `interfaces/DrumVoice.h`
- Bug fix (2026-09-25): Added missing `osc_setFreq()` / `modOsc_setFreq()` calls in `calcAsync()`. Without these, `phaseInc` stayed at 0 and D1-D3 oscillators never advanced, producing a flat-then-jump staircase waveform. Discovered by P13 integration test; P8's unit tests only checked formula-level state, not audio output.
- Open issues: LFO struct is a minimal stub (P10 will provide full implementation); user samples (waveform >= 6) output silence.

### P9 Snare, cymbal, hi-hat voices — DONE — 2026-09-24
- Prompt file: N/A (Generated directly by AI assistant)
- Output files: `dsp/Snare.{h,cpp}`, `dsp/Cymbal.{h,cpp}`, `dsp/HiHat.{h,cpp}`, `dsp/Lfo.h`, `dsp/SnareCymbalHiHatTest.cpp`
- Tests passed: 47/47 (fail=0), verified against original C logic from SonicPotions/LXR @ dee4968
- Quirks kept:
  - Snare/Cymbal/HiHat use (0x3ff << 20) for SINE phase reset (not DrumVoice's 1024 + ((1023<<20)-1024)*offset)
  - Snare/Cymbal/HiHat apply amp EG as per-block constant (no interpolation, unlike DrumVoice)
  - Cymbal/HiHat only update osc.pitchMod when transient wave == 0
  - HiHat uses gain 0.5 for main osc FM, Cymbal uses 1.0
  - HiHat picks decay per trigger (closed/open)
  - Lfo struct is a minimal stub (P10 will provide full implementation)
- Interfaces saved: `interfaces/Snare.h`, `interfaces/Cymbal.h`, `interfaces/HiHat.h`, `interfaces/Lfo.h`
- Open issues: None

### P10 LFO and modulation — DONE — 2026-09-25
- Prompt file: N/A (Generated directly by AI assistant)
- Output files: `dsp/Lfo.{h,cpp}`, `dsp/Modulation.{h,cpp}`, `dsp/LfoModulationTest.cpp`
- Tests passed: 18/18 (fail=0), verified against formulas from param_map.md
- Quirks kept:
  - `lfoFrequencyFromMidi` uses `(v+1)/128`, not `v/127` (formula: `((v+1)/128)^3 x 200` Hz)
  - `lfoPhaseOffsetFromMidi` saturates to `0xFFFFFFFF` at `v=127` (ARM float-to-uint32 behavior)
  - LFO phase is uint32 (for bit-shifting/wrapping in waveform generation)
  - Base+multiplier modulation approach: `multiplier *= (1 + amount * (modValue - 1))`
  - Velocity modulator uses `velocity/127.f` normalization
  - S&H (RANDOM) waveform holds value until phase wraps
- Interfaces saved: `interfaces/Lfo.h` (overwritten), `interfaces/Modulation.h` (new)
- Open issues:
  - Sine LFO uses `std::sin()` instead of OscTables.sine — should verify bit-identical output against original in P13
  - Random LFO uses per-instance xorshift32 PRNG (replaced unsafe global `rand()`)
  - Tempo sync scalers mapping inferred from architecture.md — exact values not verified against source
  - Modulation routing (`resetTargets`, `destination` mapping) is a stub deferred to P11/P12 when the plugin shell and parameter array are wired

### P11 Mixer — DONE — 2026-09-25
- Prompt file: N/A (Generated directly by AI assistant)
- Output files: `dsp/Mixer.{h,cpp}`, `dsp/MixerTest.cpp`
- Tests passed: 28/28 (fail=0), verified against original C logic from SonicPotions/LXR @ dee4968
- Quirks kept:
  - Pan LUT uses `sqrtLut[127-pan]` for L and `sqrtLut[pan]` for R, matching original `squareRootLut` indexing
  - Saturating adds use int16 clamping (replaces ARM `__QADD16`)
  - Decimation integrates P7 `Decimator` per-voice with global `rate_ALL` multiplier
  - Routing simplified to 2 stereo buses (St1, St2) with mono sub-routes; jack-detect fallback dropped (not applicable to plugin)
  - Null buffer handling: `processBlock` safely skips voices with null buffers, allowing inactive voices to be omitted
- Interfaces saved: `interfaces/Mixer.h`
- Design notes:
  - Per-track mute removed from Mixer. In the original firmware, muting is a pre-trigger sequencer gate (`seq_mutedTracks` bitmask in `sequencer.c`, toggled via NRPN 200-206), not a post-render audio gate — muted tracks simply never trigger a voice. Will be implemented at the trigger-dispatch layer in P12/P16, where all 7 tracks (including closed/open hi-hat distinction) can be handled correctly.
- Open issues: None

### P13 Reference-vector harness — DONE — 2026-09-25
- Prompt file: N/A (Generated directly by AI assistant)
- Output files: `dsp/Engine.{h,cpp}`, `tests/P13Harness.cpp`
- Tests passed: Compilation with -O2 succeeded; harness renders 88000 samples (2 seconds at 44003 Hz) to WAV for all 6 voices + open hi-hat
- Quirks kept:
  - Engine sample rate 44002.757 Hz (D2); WAV header writes 44003 Hz to prevent DAW import errors
  - Block size 32 samples (OUTPUT_DMA_SIZE)
  - Asset filenames match P1 export script output (`sine_table.bin`, `saw_table.bin`, `tri_table.bin`, `rec_table.bin`, `crash_sample.bin`)
- Interfaces saved: `interfaces/Engine.h`
- Design notes:
  - `Engine` class wires P8-P11 together following async-then-sync-then-mixer block architecture from architecture.md §1
  - `P13Harness` provides `render` (2-second mono WAV of a single hit) and `diff` (sample-by-sample comparison with max diff and RMS reporting)
  - `loadAssets` expects P1's binary exports in `data/`; warns but runs if missing
- Integration bugs caught: P8 DrumVoice missing `osc_setFreq()` calls (D1-D3 oscillators never advanced). Fixed in same commit.
- Resolves: D5 (fidelity verification method) — harness is now in place for reference-vector comparison
- Open issues: None

### P14 Sequencer & Presets Review — DONE — 2026-09-26
- Prompt file: N/A (Review performed directly by AI assistant)
- Output files: None (research-only phase; no new code)
- Source reviewed: `front/LxrAvr/Preset/presetManager.c`, `front/LxrAvr/Menu/menu.c` (parameters table, `parameter_dtypes[]`)
- Findings:
  - **Preset storage**: Original uses FatFS on SD card with 8.3 filenames (`p001.snd` for kits, `p001.pat` for patterns, `p001.all` for full performances, `p001.prf` for performances, `glo.cfg` for globals). Plugin will use standard VST3 state serialization (`setStateInformation`/`getStateInformation`) to save/load the 227 parameters + sequencer state as a single blob.
  - **Sequencer IPC**: AVR front-panel queries STM32 mainboard via UART using custom SysEx-like protocol (`SYSEX_REQUEST_STEP_DATA`, etc.) to save/load step data. Irrelevant for plugin — sequencer state lives in same memory space as DSP.
  - **Parameter mapping**: `parameter_dtypes[]` (227 entries) perfectly matches `param_map.md`. No surprises.
  - **Morph feature**: Original has `preset_morph()` that crossfades between two kits by interpolating `parameter_values` and `parameters2`. Excellent candidate for a plugin-specific macro/automation feature in P12.
  - **Step data structure**: `StepData` struct holds per-step volume, probability, note, and 2 parameter automation slots (param1Nr/Val, param2Nr/Val). 128 steps × 8 patterns × 7 tracks = 7168 steps total.
  - **Pattern info**: Each pattern has `next` (next pattern to play) and `repeat` (repeat count) fields.
  - **File format version**: `FILE_VERSION = 2`, with padding reserved for future expansion (64 bytes for globals, 512 bytes for kit data).
- Design implications for P12:
  - No need to port FatFS — use VST3 state serialization
  - No need to port UART IPC — direct memory access
  - Morph feature is a natural fit for plugin automation
  - 227 parameters already documented in `param_map.md`
- Interfaces saved: None
- Open issues: None
- Unblocks: P15 (UI layout), P16 (sequencer port)

- Prompt file: N/A (Generated directly by AI assistant)
- Output files: `dsp/ParameterArray.{h,cpp}`, `dsp/ParameterArrayTest.cpp`
- Tests passed: 8/8 (fail=0)
- Quirks kept:
  - Parameter indices match `param_map.md` exactly (0 unused, 1-227 active)
  - Defaults match original firmware (waveform=TRI, coarse=63, fine=63, volume=100, pan=63)

### P12 Plugin shell (ParameterArray + MidiParser) — DOING — 2026-09-26
- Prompt file: N/A (Generated directly by AI assistant)
- Output files: `dsp/ParameterArray.{h,cpp}`, `dsp/MidiParser.{h,cpp}`, `dsp/ParameterArrayTest.cpp`, `dsp/MidiParserTest.cpp`
- Tests passed: ParameterArray 33/33 (fail=0), MidiParser 5/5 (fail=0)
- Quirks kept:
  - Parameter indices match `param_map.md` exactly (0 unused, 1-227 active)
  - CC mapping: CC 1-127 → PAR 1-127 (CC 0 = bank select, unused)
  - NRPN mapping: NRPN 1-100 → PAR 128-227
  - NRPN 200-206 (track mutes) intercepted before PAR mapping, no-op until P16 sequencer layer
  - `VEL_DEST` (149-154), `VOICE_LFO` (161-166), `TARGET_LFO` (167-172) are no-ops (front-panel only in original)
  - NRPN state machine: CC 99 (MSB), CC 98 (LSB), CC 6 (Data Entry)
- Interfaces saved: `interfaces/ParameterArray.h`, `interfaces/MidiParser.h`
- Design notes:
  - ~200 of 227 parameters wired (voice-level DSP, LFO, transient, routing, etc.)
  - Remaining ~27 parameters: MIDI note override (221-227), sequencer/global params
  - Framework decision D1 (iPlug2, MIT) locked in
- Open issues:
  - iPlug2 plugin shell integration
  - State serialization (save/load)
  - Sequencer/global parameter wiring (depends on P16)

### P12b-4c Runtime silence investigation — BLOCKED — 2026-10-21
- Report: `docs/p12b4c_runtime_silence.md` (reproduction, original-C evidence,
  exact edited files, numeric results, build and manual REAPER acceptance steps).
- Confirmed two independent initial-patch faults: amp attack 127 maps to zero;
  voice/global decimation 0 freezes an initially zero held sample. Attack-only
  override 64 still yields peak 0; attack 64 plus full-rate decimation yields
  ST1 peak 5826. Preserved all original DSP equations and FIR coefficients.
- New-instance defaults: attack 0, decimation 127 in ParameterArray and host
  descriptors; native PAR 61 aligned with host decay 64. All 228 defaults agree.
  Existing restored patch values are not migrated. Existing wrapper MIDI channel
  fix was already present and preserved.
- Native default diagnostics: ST1 peak 1639, ST2 peak 0, 3097/3200 nonzero ST1
  frames. Existing 13 DSP suites passed; original-C filter differential 80000
  blocks with zero mismatches at each of O0/O2; FIR functional 7/7 and quality
  38/38 at each level, alias rejection -77.74 dB. Release x64 v145 MSBuild exit 0;
  installed resources hash-match originals.
- First installed VST3 offline-host test: 252/252, finite nonzero D1 at
  32/44.1/48/96/192 kHz (peak about 0.0500), silent patch restore preserved.
  Expanded host test compiles but execution is blocked by Windows Application
  Control; no bypass attempted. Extra host channel-isolation, full state-roundtrip
  and missing-assets execution remain pending.
- New native/adapter suite: 418 pass / 1 fail, zero counted rendering C++
  allocations. Failure retained: existing SnareVoice render is a silence stub;
  channel routing itself passes. Snare implementation is outside this repair.
- IntelliSense aligned to evaluated Release/x64 v145 build; matching syntax-only
  MSVC check passed. Editor diagnostics not directly observed.
- Open: REAPER playback/render acceptance; FIR delay reporting (23.5 native
  frames, plugin reports 0); snare stub; expanded host tests under normal policy;
  unsupported-rate transition readiness and broader instance/thread safety audit.
  P12b-4c is NOT closed. No next phase started, staged changes, commit or push.

### P12b-4c FIR latency reporting and validation — BLOCKED — 2026-10-26
- Full evidence and remaining manual acceptance: final section of
  `docs/p12b4c_runtime_silence.md`. User confirmed earlier audible REAPER playback.
- New `dsp/FIRLatency.h` computes nominal host frames, nearest integer latency
  (ties upward) and milliseconds; invalid/unsupported/NaN/infinite rates return
  zero. Resampler now uses `kEngineSampleRate` rather than a repeated literal.
- Verified indexing: y[n]=sum h_p[k]x[floor(n*Fs/Fh)-k], sinc centered on
  k+p-23.5. Native impulse symmetry/first moment measures 23.5 source frames at
  Fs=44002.7578125 Hz, nominal 0.534057435676 ms. Demand-filled 32-frame cache
  adds no fixed 32-frame wait. No separate physical delay was added.
- Host rates 32000/44100/48000/88200/96000/192000: nominal frames
  17.089837942/23.551932913/25.634756912/47.103865827/51.269513825/102.539027650;
  measured startup peaks and reported counts 17/24/26/47/51/103. Nominal signed
  residuals after PDC +0.089838/-0.448067/-0.365243/+0.103866/+0.269514/-0.460972
  host frames. Fractional-phase windowing/phase quantization also affect measured
  response; no claim of exact linear-phase group delay at every frequency.
- `OnReset` reports through installed iPlug2 `SetLatency`; VST3 stores count and
  calls `restartComponent(kLatencyChanged)` on an attached component handler.
  Initial `PLUG_LATENCY` stays 0. Only changed counts notify. Same-rate processing
  stops retain coefficients; rate changes configure while inactive via VST3
  setupProcessing. Invalid transitions now invalidate FIR readiness, silence
  output and report zero; supported-rate recovery passes. ProcessBlock unchanged.
- New latency suite: 220/220 at O0/O2, including six local pre-task PCM identity
  checks (214 normal standalone checks), independent impulse/PCM math, invalid
  rates, resets/rate changes and zero counted rendering C++ allocations. FIR
  functional 7 cases and quality 38/38 pass at both levels; alias -77.74 dB.
- Actual installed VST3 host: 1101/1101, exit 0; handler notifications and all
  six counts verified, finite MIDI audio, reset silence, invalid-rate recovery,
  channel isolation and audible/silent 228-value state restoration. Separate
  missing-assets bundle: 1101/1101, exit 0. Prior expanded-host policy block did
  not recur. Steinberg validator: 47 passed / 0 failed, exit 0.
- MSBuild 18.10 Release x64 v145: exit 0, linked/packaged/installed bundle; all
  five resource assets hash-match. Existing warnings/build flags unchanged.
- Current DSP batch: 11/13 programs pass; Device Guard blocks LfoModulation and
  ParameterArray execution. Original-C filter O2: 80000 blocks / zero mismatches;
  O0 executable blocked by Application Control. No policy bypass; blocked launches
  are not passes even when a shell/native-exit variable is zero.
- Runtime suite at O0/O2: 418 pass / 1 fail each, exit 1. Snare silence stub is
  inherited from main (c750a03/07d18be, before FIR branch); no snare changes.
  Audibility assertion retained; routing passes. Separate snare follow-up remains.
- REAPER v7.80 started in an isolated unsaved test project; LXR listed in chooser.
  FX selection failed with target-window mismatch after documented refresh/retry.
  New PDC at 44.1/48/96 kHz, playback timing and saved-project reopen NOT RUN;
  precise manual checklist is in the report. Earlier user listening confirmation
  is preserved, not substituted for new PDC acceptance.
- Open: REAPER acceptance, Device Guard-blocked debug checks, independent snare
  repair; wider multi-instance/parameter-thread safety and inherited build-flag
  policy audit. Phase-wide interface acceptance remains open. No P12b-4d/P15/P16/
  P17 work, staging, commit, push, merge or PR. P12b-4c is NOT DONE.

### P12b-4c Automated REAPER PDC verification — PASS (host scope) — 2026-10-26
- Harness: `tools/reaper/p12b4c_pdc_test.lua`; smoke wrapper and transport/reload
  probe are adjacent in `tools/reaper/`.
- Actual REAPER 7.80/x64 offline renders at 44100/48000/96000 Hz confirmed by
  WAV headers. Intended installed VST3i loaded via its exact path/class identity.
- Plugin/chain reporting matched 24/26/51 samples. REAPER chain actual value was
  1024 in normal mode and 0 in ignore-PDC mode; reporting remained 24/26/51.
- Independently measured normal-vs-ignore render shifts were exactly 24/26/51
  frames, zero alignment error and zero shifted-waveform RMS error. Nominal
  residuals were -0.448067/-0.365243/+0.269514 frames.
- Finite nonzero stereo audio: peaks 0.0499936342/0.0499974489/0.0500279665;
  RMS 0.00120634971/0.00120634792/0.00120634948; frames
  110250/120000/240000. Two MIDI events per case, channel 1, note 60, velocity 110.
- Saved isolated project reopened cleanly with six LXR instances; all reported
  51 samples at saved 96 kHz. Two play/stop cycles passed.
- Reports: `out/reaper-p12b4c/pdc_results.json` and `pdc_report.md`.
- Host PDC requirement is closed. Parent P12/P12b-4c remains DOING/BLOCKED for
  the previously recorded Device Guard checks, inherited snare stub, and broader
  phase/interface issues; none was changed or reclassified here.

### P12b-4c Snare renderer restoration — BLOCKED (environment only) — 2026-10-26
- Authoritative source: Sonic Potions LXR 0.37 `Snare.c` and dependencies at
  commit `dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`; Catalyst was not used.
- Replaced the inherited zero-fill snare stub with the original noise/filter/
  transient/tonal/envelope/velocity/volume/distortion pipeline and preserved
  operation order, signed-16-bit narrowing, saturation and firmware quirks.
- Engine asset loading and Windows packaging now include both transient tables;
  no audio-thread allocation or I/O was introduced.
- Independent original-C differential: 61/61 at O0 and O2; 12 cases, 4,864
  samples per level, zero mismatches/max difference/RMS difference, zero counted
  render allocations and intact buffer canaries.
- Runtime suite: 419/419 at O0/O2. Snare/Cymbal/HiHat: 47/47 at O0/O2. Filter
  differential: 80,000 blocks/zero mismatches. FIR functional and latency suites
  pass unchanged. All runnable DSP tests pass.
- Release x64 v145 MSBuild and packaging pass. Installed VST3 SHA-256 is
  `F51FF17924512FD364ADE5704E0A6CEFBFCD930AE91F803E576343F8DF0DA5B8`;
  installed-host test 1,143/1,143 and Steinberg validator 47/47.
- Fresh isolated REAPER 7.80/x64 48 kHz channel-4 render: peak 0.410337090492249,
  RMS 0.00756627536108755, finite/nonzero. Repeated hits decay; channel-1 control
  remains audible. Plugin/chain PDC is 26 samples and independently measured
  normal-vs-ignore shift is 26 with zero alignment error. Reload and two
  transport start/stop cycles pass. Subjective listening is NOT RUN.
- Strict classification remains BLOCKED: Windows Application Control blocks the
  required O0 mixer executable and O2 FIR-quality executable. No policy bypass
  was attempted; there are no observed test failures. Full evidence:
  `docs/snare_renderer_repair.md` and `out/reaper-p12b4c/snare_repair_final_*`.
- No sequencer, preset, UI or unrelated voice work; no staging, commit or push.

### P12c VST3 state serialization — BLOCKED (environment only) — 2026-10-26
- Full engineering report: `docs/p12c_state_serialization.md`.
- Added one authoritative versioned state format: `LXRS`, version 1, 24-byte
  little-endian header, 228-byte ID-ordered 7-bit payload, CRC-32, 252-byte LXR
  state plus iPlug2's four-byte VST3 bypass suffix. Unknown/corrupt/truncated/
  oversized/trailing state is rejected before mutation. Exact legacy 228-double
  iPlug2 state migrates safely.
- All active IDs 1-227 and reserved slot 0 are preserved. Restore publishes a
  validated fixed-size atomic candidate and rebuilds runtime Engine/FIR history/
  MIDI state at a processing boundary. Host automation and native MIDI CC/NRPN
  changes converge on the same `IParam`/`ParameterArray` values without locks,
  allocation, I/O or logging in the callback.
- Codec/native tests pass 29/29 at O0 and O2 with exact controlled six-voice PCM.
  Installed VST3 state host passes 322/322; VST3 runtime 1143/1143; validator
  47/47. Release x64 v145 MSBuild/package/install passes. Final DLL SHA-256:
  `617D4CFDF77AD8AB4B07177CB3C61D06824FD6B8304A59C64A15EF86F6750B5F`.
- REAPER 7.80/x64 isolated save/reopen passes at verified 48 kHz: two distinct
  instances, 456/456 native slots restored, zero mismatches, exact pre/post
  deterministic render samples, finite/nonzero peak 0.000640034676 and RMS
  0.000142555836, two transport cycles. Both instances expose 26-sample PDC.
- Fresh post-build PDC regression measures 26 frames, zero alignment error and
  -0.365243-frame nominal fractional residual. Final reports are
  `out/reaper-p12c/p12c_results_final.json` and
  `out/reaper-p12b4c/p12c_fir_pdc_final_48000_results.json`.
- Regression preservation passes for snare differential, 419-test runtime, FIR
  functional/quality/latency, filter differential, installed VST3 runtime and
  validator. Windows Application Control blocks fresh O0 LfoModulation and O2
  ResonantFilter/TransientGen executables; no bypass attempted. No assertion
  failure is observed, but these mandatory launches prevent DONE/PASS phase
  classification.
- No sequencer, kit-file import, preset browser, UI, staging, commit or push.

### P12c final certification audit — BLOCKED — 2026-10-26
- Baseline remained branch `p12b-4c-fir-resampler` at
  `280279debe94cb26810ea89b89f36b9bf965a747`; existing tracked/untracked work
  and all reports were preserved. No staging, commit, push, clean, reset, or
  process termination occurred.
- The exact O0 LfoModulation binary now runs normally and passes 18/18. All 13
  O0 broad suites pass. O2 passes 11/13; exact ResonantFilter and TransientGen
  binaries remain BLOCKED by Code Integrity events 3033/3077 under policy
  `VerifiedAndReputableDesktop` (GUID
  `{0283ac0f-fff1-49ae-ada1-8a933130cad6}`). Both are unsigned; AppLocker has
  no matching event. No policy or file workaround was attempted.
- Fresh focused regression passes: codec 29/29 O0/O2, snare differential 61/61
  O0/O2, runtime 419/419 O0/O2, FIR functional/quality/latency O0/O2, and
  original-C filter differential 80000 blocks/zero mismatches O0/O2.
- Forced MSBuild 18.10.1 Release x64 v145 rebuild, link, seven-resource package,
  install, installed-state 322/322, runtime 1143/1143, and validator 47/47 pass.
  Built/installed SHA-256:
  `45750AA46CCFBD3BBF080DBE81B682874038CC24C27B82AEE85A419A721B494F`.
- Fresh REAPER 7.80/x64 evidence for that exact hash passes: two instances,
  456/456 slots, exact deterministic samples, finite nonzero audio, two
  transport cycles, reported latency 26, independently measured PDC shift 26,
  zero alignment error. Reports use the `cert_45750aa4` suffix under
  `out/reaper-p12c/` and `out/reaper-p12b4c/`.
- Shared iPlug2 `SetState()` hardening is a necessary global fix for propagating
  negative plugin decode positions and rejecting failed seek/short bypass reads.
  Valid/bypass serialization is unchanged; no LXR-local return hook exists.
  Keep it isolated and pin the vendor revision. Dedicated failed-seek and short-
  bypass unit branches remain desirable.
- Safety audit confirms corrupt payloads cannot partially publish, but identifies
  two unresolved concurrency gaps: serialization has no cross-parameter snapshot
  generation during concurrent automation, and a CC/NRPN event can overlap an
  odd restore publication and temporarily/persistently diverge `IParam` from
  Engine/`ParameterArray`. These need a plugin-local lock-free authority protocol
  and stress test before strict acceptance.
- Classification is **BLOCKED**, not DONE: two O2 policy-controlled launches and
  the two concurrency findings remain. P16/sequencer work was not started.

### P12c-R1 state concurrency hardening — BLOCKED (certification environment) — 2026-10-26
- Full report: `docs/p12c_r1_concurrency_hardening.md`.
- Deterministically reproduced both audited defects: the old independent atomic
  scan yields a forced mixed-generation patch, and old odd-restore early return
  permits MIDI/restore host-native divergence.
- Replaced competing mutable state with one plugin-local 228-slot atomic
  authority. A lock-free writer gate, all-atomic payload, odd/even 64-bit
  generation, bounded readers, and explicit failure semantics provide coherent
  LXRS snapshots without a C++ data race or unbounded audio-thread wait.
- Restore is one batch generation. Host/controller automation uses the same
  authority; colliding callbacks enter generation-ticketed mailboxes. CC/NRPN
  takes the authority writer before host/native mutation. MIDI overlapping a
  restore enters a preallocated 4,096-event FIFO and replays in order after the
  complete patch. Stale `OnIdle()` generations are dropped.
- Focused concurrency/oracle: 35/35 O0 and O2, including forced barriers,
  concurrent producers/serialization, ordering, failure/overflow, invalid
  state, deterministic bytes, and zero counted authority allocations. TSan was
  not available and is not claimed.
- LXRS v1 remains byte-compatible. Native codec 29/29 O0/O2; installed state
  322/322; installed runtime 1101/1101; snare 61/61 O0/O2; runtime 419/419
  O0/O2; FIR functional/quality/latency and 80,000-block filter differential
  pass. Validator 47/47.
- MSBuild 18.10.1 Release x64 v145 compile/link/package/install and all seven
  resources pass. Built/installed SHA-256:
  `B170E00911A4031B04616C0B75E2B2A7CE5902E5CF46F22A23F1D2DD93A6F953`.
- Broad DSP: 13/13 O0 PASS; 11/13 O2 PASS; O2 ResonantFilter and TransientGen
  remain BLOCKED by the established Windows Code Integrity enterprise-signing
  policy. No bypass was attempted.
- A fresh isolated REAPER 7.80 instance was launched against the pre-final R1
  build after confirming none was running, but native automation timed out while
  importing the unique R1 harness. No result artifact was created and the
  task-created process was not terminated. Final-hash persistence/render/PDC
  therefore remain unverified.
- Classification stays **BLOCKED**, not FAIL: concurrency repairs and runnable
  assertions pass, but current-hash REAPER and two mandatory O2 launches are
  unavailable. P12c is not DONE and P16 was not started.

### P12c-R1 final-hash certification — FAIL (controller projection) — 2026-10-29
- Full report: `docs/p12c_r1_final_certification.md`; raw evidence and JSON/Markdown
  reports: `out/p12c-r1-final-20261029/`. Branch/HEAD remain unchanged. Starting
  tracked/untracked work and earlier reports were preserved; no Git mutation,
  plugin rebuild/install, production change or framework widening occurred.
- Built and installed binaries are 727552 bytes and match expected SHA-256
  `B170E00911A4031B04616C0B75E2B2A7CE5902E5CF46F22A23F1D2DD93A6F953`.
  Seven resources match source/built/installed sizes and hashes. Process module
  paths/hashes tie the new REAPER instances to this exact executable.
- Fresh REAPER 7.80/x64 state-only checkpoint passes: all 228 slots explicitly
  set per instance, 456/456 after recreation/reopen, reserved 77/0, zero
  mismatches; full expected/actual arrays retained. Deterministic A PCM is exact
  (67200 stereo frames, 48 kHz, maximum/RMS sample error 0); both patches render
  finite/nonzero audio. Two actual advancing transport start/stop cycles pass.
- Fresh PDC and reload pass at confirmed 48 kHz: direct plugin report 26,
  independently measured shift 26, aligned onset error 0 and waveform RMS error
  0. Block-rounded chain actual 1024 is distinguished from direct PDC 26.
- Both formerly denied exact O2 executables now execute in place through normal
  PowerShell: filter fail=0/warn=0, transient 17/17, exits 0. Hashes/signatures
  were not changed; Code Integrity/user-mode enforcement remains 2. No policy
  change/bypass or administrator exception was used. The reason current policy
  accepts the files is not established; read-only CiTool policy listing is denied.
- Fresh required regressions pass: concurrency 35/35 O0/O2; codec 29/29 O0/O2;
  installed state 322/322; runtime 1101/1101; snare differential 61/61 O0/O2;
  native runtime 419/419 O0/O2; FIR functional/quality (38/38)/latency (220/220)
  O0/O2; original-C filter 80000 blocks/zero mismatches O0/O2; validator 47/47.
  Existing test executables were freshly run, not rebuilt or cited as historical.
- **New failure:** REAPER MIDI controller values remain 100/0 instead of 77/45.
  An isolated installed-DLL verifier reproduces serialized authority 77/45 but
  `IEditController::getParamNormalized` 100/0 after 500 ms timer dispatch and two
  idle notifications: 34 assertions pass, 2 fail, exit 1. The VST3 controller
  cache is separate from IParam/authority; the current OnIdle notification route
  does not synchronize it. Existing 322-state assertions covered serialization,
  not this readback. Smallest proposed repair only is recorded; production work
  stopped. REAPER desired-value MIDI save/reopen and forced host overlap remain
  uncompleted/unclaimed.
- Historical PID 15300 was absent; no user REAPER session existed at task start.
  Seven task instances closed gracefully. Task PIDs 34036/13544/16836 refused
  normal close requests and remain; no force termination or dialog discard was
  attempted. Force termination would require explicit user approval.
- Classification is **FAIL**, superseding the earlier environmental BLOCKED
  status because an executed plugin assertion fails. The policy gap is closed;
  MIDI controller convergence is an outstanding code defect. P12c-R1 is not
  DONE; P16 and other feature phases were not begun.

### P12c-R2 MIDI controller synchronization repair — PASS — 2026-10-30
- Full report: `docs/p12c_r2_controller_sync_repair.md`; preserved evidence,
  independent `verification.json`, and `focused-verification.json` under
  `out/p12c-r2-20261030/`. Branch/HEAD remain
  `p12b-4c-fir-resampler` / `280279debe94cb26810ea89b89f36b9bf965a747`.
- Reproduced the unchanged installed R1 verifier: 34 PASS / 2 FAIL. IDs 88/128
  serialize 77/45 while IEditController reads 100/0, despite two idle gestures.
  Confirmed separate controller cache and an API parameter queue not drained by
  this non-distributed VST3 adapter. Historical failures are preserved.
- Plugin-local idle now projects durable per-slot authority generations under
  the bounded writer gate, releases it before host cache invalidation, and
  acknowledges only successful same-connection notifications. Reconnect performs
  one full reconciliation. No DAW automation gestures or new vendor patch.
- Real REAPER investigation also exposed float-precision MIDI proxy truncation:
  CC98=1 becomes selector 0. A read-only loaded-parser trace and independent
  float-rounded VST3 fixture reproduce it. Callback-scoped local VST3 queue
  views canonicalize only MIDI CC wire values before framework dispatch.
- Exact original assertions now 36/36; external controller verifier 955/955;
  loaded authority/ParameterArray/IParam/Engine/FIFO probe 1033/1033. Includes
  all 128 wire values, retry/reentry, generations, reconnect, actual concurrent
  process/save/restore/UI, ordered deferred NRPN replay and 4096+1 overflow.
- All 30 mandatory suite runs PASS: R1 concurrency 35/35 O0/O2; codec 29/29
  O0/O2; snare 61/61 O0/O2; runtime 419/419 O0/O2; FIR functional/quality/latency,
  80000-block filter differential, MIDI, mixer, filter/transient O0/O2; installed
  state 322/322, runtime 1101/1101 and validator 47/47. Corrected standalone
  snare fixture initialization to match zeroed original-C globals; algorithms
  and all 61 DSP source/header files remain unchanged from R1.
- VS Community 18 / MSBuild 18.10.1 / Release x64 / v145 compile, link,
  seven-resource package and installation PASS. All executable copies are
  729600 bytes, SHA-256
  `89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`.
- Fresh REAPER 7.80 on that canonical installed path/hash passes MIDI 77/45
  readback and save/reopen, two distinct instances, 456/456 slots at baseline
  and MIDI checkpoints, exact 67200-frame tonal audio, two advancing transport
  cycles, and 26-sample reported/measured PDC with zero aligned error after
  reload. Formatted FX values are "77"/"45". Save occurs before later observed
  host stop/reset CC messages; those legitimate updates are recorded separately.
- User explicitly authorized termination only of historical task PIDs 13544 and
  16836; identities were rechecked. All subsequent task closures use saved,
  clean projects and normal REAPER quit. No Windows policy change/bypass,
  unrelated project manipulation, staging, commit or push. TSan and whole-DLL
  heap instrumentation remain unclaimed optional checks. LXRS v1 and legacy
  migration remain compatible. P16/morphing/preset-browser/UI work not started.


### P12c-R3 checkpoint gate closure - READY (Git execution not authorized) - 2026-10-31
- Reports: `docs/p12c_r3_checkpoint_gate_closure.md`, updated certified baseline,
  and `docs/build_and_test.md`; exact checkpoint list in
  `tools/verification/checkpoint_files.json`.
- Portable REAPER helper configuration and explicit overrides; historical paths
  and earlier test classifications preserved. Path regression: 32/32 PASS.
- Explicit source-test runner and tracked filter compatibility headers replace
  undocumented local prerequisites. Independently acquired pinned source tree,
  paths with spaces, fresh objects/SDK hosting/MemoryStream/validator: 47/47
  source-built suites PASS, including O0/O2 native codec/concurrency/synthesis,
  snare/filter differential and FIR. Portable latency is 214 assertions; the
  preserved 220-assertion historical snapshot suite separately passes.
- Original mandatory 30-suite matrix freshly rerun PASS; read-only R2 REAPER
  state/MIDI/audio/PDC artifact analysis PASS. No fresh REAPER session claimed.
- Safe narrow ignore rules applied; all 70 checkpoint source paths remain
  eligible. Immutable archive acquisition preserves current vendor topology,
  exact iPlug2 d54f6905/SDK pins and the one intentional state-reader patch.
- Exception/RTTI disposition RETAIN for the certified third-party shell;
  active iPlug2 casts reject disabled RTTI in a controlled compile. Native DSP
  objects disable exceptions/RTTI, with the existing DrumVoice fixture's catch
  requiring an explicitly documented test-translation-unit exception override.
- Source/functional reproducibility demonstrated; byte-identical DLL builds are
  unestablished and are not a repository checkpoint requirement. All nine
  binary tables re-exported from pinned original C hash-match tracked data.
- Production DSP/plugin/state/MIDI/controller/FIR code, runtime build settings,
  assets and installed certified hash 89F6DF59...E7FF82 unchanged. No new DSP
  interface is introduced by this infrastructure phase. P12 remains larger scope.
- All technical checkpoint gates closed. Separate explicit authorization is
  still required for staging/commits/push. No such Git operations or P15-M1/P16,
  sequencing, morphing, preset-management or GUI implementation occurred.


### P15-M2A Typed modulation and selector contract - DONE (contract only) - 2026-11-02
- Explicit user-authorized contract milestone, separate from the existing P15 UI-layout row. No P16, renderer modulation, morph/preset/UI work.
- Original SonicPotions/LXR dee49689f3f0cdf8f9b7b5e79628cfdca1b79394:205 ordered AVR rows,204 non-null references,203 non-off normal voice-menu entries,198 distinct table storage fields;7 additional STM32 non-menu pointer targets.
- New unused runtime-independent C++17 contract and constexpr data in dsp/ModulationContract.h and ModulationContractData.inc; accepted facade interfaces/ModulationContract.h. Strong host/PAR/index/byte/source/destination identities; explicit original menu/preset/UART validation.
- Selected full internal indices plus separate explicit route identity; existing seven-bit selector bytes remain opaque/inert. Ambiguous legacy routing is rejected; future versioned route persistence/host semantics required. LXRS v1,227 IDs/defaults and certified MIDI unchanged.
- New tests compile unchanged original AVR/STM32 C independently:O0/O2 each121781 PASS,0 FAIL, lookup allocations0. Initial extractor conditional-label error corrected without weakening assertions; failed run retained. Final evidence out/p15-m2a/contract-run-003-final/.
- Fifteen fresh existing regressions PASS:parameter mappings/MIDI/codec/concurrency O0/O2; installed state322, controller955, internal probe1033. No security-policy change/bypass or VST3 build/install.
- Contract documentation docs/p15_m2a_modulation_contract.md; target/compatibility/test manifests and decisions in out/p15-m2a/. Six mutable destination targets and physical AVR relay remain explicitly unverified; no runtime DSP behavior certified by this milestone.
- Branch/HEAD preserved at0091651720cacbcb2ecd3172bd667581ffb83415; installed hash89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82 preserved. No stage/commit/push. Next proposed P15-M2B bounded base-mapping corrections, separately authorized.


### P15-M2B Firmware-faithful base mapping and timing corrections - PASS - 2026-11-02
- Original SonicPotions/LXR v0.37 dee49689f3f0cdf8f9b7b5e79628cfdca1b79394. Pre-edit correction matrix and source evidence: out/p15-m2b/correction_matrix.csv; full report docs/p15_m2b_base_mapping_corrections.md.
- Production changes only dsp/ParameterArray.cpp and dsp/DrumVoice.cpp: CY29/30 and HH33/34 coarse operator mapping/recalc; HH35/36 gains; cutoff37..42 through existing post-shaper 0.45 setter; live SN/CY/HH14..19 Hz recalculation with async phaseInc; exact pre-envelope-state instant-attack49/51/53 exception.
- Final independent original-C tests O0/O2 each 30169 PASS, 0 FAIL; 52416 integer PCM samples each, zero mismatches/max/RMS error; setter/render allocations0. All six live pitch IDs first affect sample1 before async or33 after async, matching original C. Baseline failing vectors retained; no original source or assertion tolerance altered.
- Forty-two freshly source-built existing native suites PASS; original-C snare61 and filter80000 blocks per mode, FIR latency214, serialization29 and concurrency35. Preserved M2A contract O0/O2 each121781 PASS. All prior M2A files/text preserved.
- Isolated VS Community18/MSBuild18.10.1/Release x64/v145 build PASS; seven resources verified. Private hash55E79A82AC938A41830C0D9563A2EB75CEC7C54E598F1B75B21E10BBB103FA5B; no deploy events/install. Fresh private-bundle host state322/runtime1143/controller955/probe1033 and Steinberg validator47 PASS. REAPER/listening NOT RUN.
- LXRS v1, IDs/defaults, certified MIDI/normalization, authority/FIFO/controller, rate/block/FIR and existing interfaces unchanged. Saved bytes remain identical; corrected audio can change. Full-field comparisons use matched original note tables; combined FM uses disclosed positive initial intervals; ARM/FMA/negative-FM/ROM/RNG/hardware limits remain qualified.
- Branch/HEAD0091651720cacbcb2ecd3172bd667581ffb83415 and installed binary89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82 preserved. No staging/commit/push, firmware edit, Windows security bypass, LFO/velocity route integration, sequencer/morph/preset/UI work.
- Next recommended P15-M2C is separately authorized velocity runtime work, gated on explicit selector/persistence semantics from M2A; no guessed activation of legacy selector bytes.


### P15-M2C Original velocity destination routing - PASS (verified native subset) - 2026-11-02
- Original SonicPotions/LXR dee49689f3f0cdf8f9b7b5e79628cfdca1b79394. Six-voice pre-edit gate, original-C probes, per-target classifications and full report: out/p15-m2c/ and docs/p15_m2c_velocity_routing.md. No Catalyst reference.
- Internal processing-owner-only typed configuration supports107 original AVR rows/101 physical fields (D1/D2/D3/SN/CY18 each, HH17);39 supported indices exceed127. Exact converted-base capture, trigger-time current-field multiplication, native-tick restore/held replay, UInt8 narrowing and cutoff coefficient restoration. Canonical existing VelocityModulator type replaces Engine's conflicting duplicate; legacy factorized helper is not used for rendering.
- Six mutable destination-address targets rejected after actual original-C low-byte probes. Gate accounts for205 rows:107 verified,36 LFO-unreachable,6 unverified-mutation,55 unsupported,1 off. Global/cross-voice/trigger-consumed/unsafe paths reject explicitly. AVR physical NRPN relay remains unverified; proven STM32 assignment and independent AVR table translation suffice for this subset.
- Final new original-C tests O0/O2 each61395 PASS,0 FAIL;11989 cases/383648 samples each, zero mismatches/max/RMS error. All107 actual-field effect proofs; held edits, source scopes, aliases, trigger/retrigger, zero/amplitude velocity, off/change, reset/restore, LXRS bytes and native PCM tested. Disconnected ignored-source control yields7753 expected failed checks/30130 PCM mismatches, proving detection. Initial failures retained and corrected without assertion relaxation.
- Measured each mode1224453 parameter updates,4227 triggers,4185 configure calls and11989 render cases with0 C++ allocations; fixed per-instance state, no locks/I/O/logging/callbacks or new queues. Original pre-tick trigger compounding is preserved transiently; captured/saved bases never accumulate it. No unsupported trigger-consumed route is activated.
- All53 fresh positive suites PASS:42 existing native suites, M2A121781 O0/O2, M2B30169/52416 exact samples O0/O2, new velocity O0/O2 and five private-host/validator runs. M2A files and M2B corrections preserved; only new post-set notification extends ParameterArray beyond M2B.
- Isolated VS Community18/MSBuild18.10.1/Release x64/v145 build PASS; private774656-byte SHA4EE573DD68724E6E162113231F04C904A4E2F8D489F75E74EE4DDD27462660D2. Fresh host state322/runtime1143/controller955/probe1033 and Steinberg validator47 PASS. Seven assets match; no deploy/install or REAPER/listening run.
- Host IDs/defaults, certified MIDI/normalization, LXRS v1/legacy migration, authority/FIFO/controller, native rate/block and FIR unchanged. Legacy149..154 bytes including0 stay opaque/inert; native route state is transient and cleared on init/reset/restore. Persistent end-user select/automate/save/restore is NOT complete or implemented.
- HEAD0091651720cacbcb2ecd3172bd667581ffb83415 and installed binary89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82 preserved. No stage/commit/push, branch change, firmware edit, security bypass, LFO runtime, sequencer/morph/LXRSv2/preset/UI work. Recommended separately authorized M2D: LFO primitive/tick fidelity and shared physical-base/order contract before runtime integration.


### P15-M2E Native LFO integration - BLOCKED (verification access) - 2026-11-02
- User explicitly authorized bounded development despite the exact historical FIR O2 security denial. Native runtime now has eight source-faithful unipolar waves, Fs/32 phase-before-output timing, six LFO owners, global retrigger scans (both hats5), explicit integer native BPM120 service and107 own-voice typed scalar routes; no persistent selectors or host tempo connection.
- Shared12-entry physical-base registry restores converted bases, replays velocities0..5 and dispatches LFOs0..5 with original current-field arithmetic and immediate narrowing. Raw authority/LXRS/IDs/MIDI/FIFO/controller/FIR unchanged. Engine reset clears all six LFOs/routes before raw replay. Incremental Engine state392 bytes; measured expanded O0 configuration/set/trigger/render allocation count0.
- Expanded original-C runtime O0:1772097 PASS,115680 voice PCM cases/3701760 exact samples,max/RMS0. Earlier fixture O0/O2:1609225 PASS/2962176 exact samples each on identical production source. Expanded final O2 executable denied WinError4551; not PASS. Test-only disconnected O0/O2 detected106494 failed checks/598568 PCM mismatches each. Initial compile and reset-omission failures retained; reset omission corrected without assertion relaxation.
- Final existing regression matrix55 PASS/1 BLOCKED/0 FAIL:42 native,eight M2A/B/C/D O0/O2,five private-host/validator passes; blocked row is the exact historical FIR O2 artifact. Fresh current FIR O0/O2 passes do not replace that exact-artifact requirement. Earlier isolated ParamMapEnv and layout-diagnostic denials retained, no retries/bypass.
- Private VS18/MSBuild18.10.1 Release x64/v145 VST3 SHA9FC87F425C2D7241909C56C9494A9D2DD42BCB19E705ABD2FF58675436693C28; host state322/runtime1143/controller955/probe1033 and validator47 PASS. No install or REAPER launch. Installed certified SHA89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82 and HEAD0091651720cacbcb2ecd3172bd667581ffb83415 preserved.
- docs/p15_m2e_lfo_runtime_integration.md and out/p15-m2e/ contain source provenance, exact tests, ordering, allocation, binary and integrity evidence. Certification remains BLOCKED. Next separately authorized task: execute the exact blocked expanded O2 and historical FIR artifacts through an approved mechanism; no runtime scope expansion, P16, morphing, presets, GUI, stage, commit or push.


### P15-M2F Persistent modulation destination/state design - PASS (contract only) - 2026-11-03
- Independently authorized design milestone under the P15-M2E certification exception. M2E remains BLOCKED (57 PASS/1 exact historical FIR O2 policy block/0 FAIL); that executable was not retried.
- Selected bounded LXRS v2 TLVs with a mandatory independently versioned routing module: 228 original patch bytes +12 explicit full-width records; 516-byte core/520-byte VST3 stream, ceiling4096. LXRS v1/validated legacy doubles stay readable, opaque selector bytes preserved, all legacy routes explicitly off. No production codec implemented.
- Proposed stable VST3 IDs228..239, lifetime integer range0..65535; original AVR indices retain numeric identities. Profile1 admits only107 own-voice rows/101 fields/39 indices>127 per source, highest active203; original row204 remains representable but unsupported. No parameters registered or MIDI mappings changed.
- Common bounded patch/routes coordinator, restore epochs/tickets, staged native restore, actual controller-cache reconciliation, future lock overlays and discrete retained morph routes specified. Production mailbox CAS loops and full-stream bypass validation are future integration dependencies; no callback/authority/DSP code changed.
- Standalone independent Python binary goldens, unchanged original AVR/STM32 metadata C and pinned IParam conversion: O0/O2 each272249 PASS/0 FAIL,65536 numeric tokens exact at double and float-rounded normalized precision. Sequential publication model is not a production concurrency proof. Initial third-party compile setup failure and earlier passes retained.
- New design docs/p15_m2f_modulation_persistence_design.md; declarative interfaces/P15M2FPersistenceContract.h; isolated tests/reference/persistence model/fixtures and tests/P15M2FStateContractTest.cpp; full evidence out/p15-m2f/. Firmware/custom non-commercial licence retained. No production edits/build/install/REAPER/security bypass, stage/commit/push or P16/morph/preset/UI implementation. Next proposed P15-M2G codec/authority integration requires separate authorization.


### P15-M2G Production LXRS v2 and atomic routing restore - PASS - 2026-11-04
- Production bounded TLV codec implements the frozen M2F schema exactly: 516-byte canonical core, 520-byte VST3 stream, 4096-byte transport ceiling, explicit little endian, whole-core CRC, fixed records and exact optional-TLV retention. All twelve velocity/LFO owner assignments persist with full-width original identities; profile1 remains the107 own-voice scalar rows,39 above127, highest203. Row204 and unsafe/global/cross-voice/mismatched routes reject.
- Existing ParameterStateAuthority<228> remains the sole patch authority. A fixed per-instance RoutingStateAuthority supplies one-attempt common coordination, coherent snapshots, route tickets, joint generations, restore epochs/cutoffs and stale rejection. Restore jointly publishes only a fully validated candidate; the next processing boundary reconstructs an inactive Engine/ParameterArray from that coherent snapshot before selecting it. No callback allocation, mutex, I/O or logging; original base restore -> velocity -> LFO order is unchanged.
- LXRS v1 and validated legacy 228-double states preserve all patch/bypass semantics and migrate with twelve explicit OFF routes. The LXR VST3 boundary validates the whole suffix-bearing stream before mutation; bad/oversize/truncated/unsupported data cannot partially change patch, routes, bypass or controller projection. IDs0..227, normalization and MIDI mappings unchanged; IDs228..239 are not registered.
- Dedicated production integration O0/O2 each1502 PASS/0 FAIL; independent fixtures, exact bytes/offsets/CRC/endian, optional/mandatory tags, all destinations, corruption rollback, contention/stale/rapid restore, multi-instance and deterministic active/off native audio pass. Private VST3 route-state test91, existing state322/runtime1143/controller955/probe1033 and Steinberg validator47/47 all PASS.
- Isolated VS Community18/MSBuild18.10.1/Release x64/v145 build/link/package PASS with seven assets. Private 795648-byte SHA256 DC3356F7CA709ED1BA67351FA2A699CA7BE846E19AF950B998B08D90887A79A4; not installed and no fresh REAPER claim. Certified installed SHA89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82 remains unchanged.
- Fresh applicable current-source regressions pass. Application Control independently blocks several fresh executable variants and the exact historical FIR Functional O2 remains BLOCKED/not executed; no bypass attempted and neither is relabelled PASS. Full report docs/p15_m2g_state_integration.md and evidence out/p15-m2g/. P15-M2H host controls, sequencer, morph, preset and UI work not implemented.


### P15-M2H VST3 modulation destination controls - PASS - 2026-11-05
- Interrupted implementation retained and completed through private verification: exactly IDs228..239, discrete0..65535/OFF0, typed source/owner validation, routing-authority publication, controller reconciliation and unchanged LXRSv2 516/520-byte persistence. IDs0..227 and MIDI mappings remain unchanged.
- Host-control O0/O2 each131611 PASS/0 FAIL; private controller684, M2G state91, state322, runtime1143, controller955 and probe1033 all PASS/0 FAIL. Steinberg validator47/47 PASS. Full code-space double/float normalization and107 verified routes pass.
- Release x64/v145 private build used `/p:PostBuildEventUseInBuild=false`; DLL SHA708EB4C1FEB55598762E1D4F9EE3C113D3C959FBCE325B580EDB8915371D1DD0. Exact private module path/hash captured from REAPER. Installed certified SHA89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82 and seven resources remain unchanged.
- REAPER7.80 isolated state/control PASS against the exact private module: two instances, twelve controls, invalid204 rejection, controller readback, all228 patch values,24 route values, bypass and automation save/reopen. Velocity-only, LFO-only and combined active renders each differ from OFF; active pre/post-reopen PCM is bit-identical; transport and26-sample PDC at48kHz pass.
- Earlier identical active/OFF PCM was a fixture error: both native modulation depths were zero. Final fixture explicitly enables VELO_MOD_AMT_1 and LFO1 amount; no production DSP change or assertion reduction. Windows Application Control separately BLOCKED six native variants (WinError4551), including historical FIR Functional O2; no bypass. Evidence out/p15-m2h/, report docs/p15_m2h_host_modulation_controls.md.


### P16-A Original sequencer architecture and timing contract - PASS - 2026-11-05
- Original LXR0.37 seven-track/eight-pattern model audited:7168 flattened seven-byte step records, separate16-bit main gates, shared hi-hat owner,32-PPQ timing, probability/lock quirks, chaining, performance functions and50361-byte pattern payload. No Catalyst reference or production integration.
- Selected fixed-point host/internal timeline and next-32-native-frame fidelity boundary; documented explicit host adaptation, immutable ownership and future state migration beyond the current4096-byte cap.
- Standalone pinned-header/source-contract O0/O2 each64 PASS/0 FAIL, including million-event rational timing, callback partitioning, seek/loop/restart, hat ordering, probability and lock/chain contracts. No production source/build/install/Git operation.
- Report `docs/p16a_sequencer_architecture.md`; evidence `out/p16a/`. Hardware RNG, exact ARM bitfield ABI and complete SOM user semantics remain qualified.


### P16-B Native sequencer core and pattern authority - PASS - 2026-11-05
- Isolated C++17 core adds explicit seven-byte StepData,8x7x128 PatternBank, source defaults/validation/edit API, seven-track/six-owner mapping, instance-local deterministic RNG and integer-rational native clock. No Engine/MIDI/VST3/LXRS/host/persistence integration.
- Selected three immutable50418-byte bank slots with reader pins, post-claim race check, single bounded writer, RAII lifetime and explicit Busy backpressure. Authority151344 bytes; no mutable published pointer or per-block bank copy.
- O0/O2 each1007503 PASS/0 FAIL; one-million-position zero rational drift, chunk invariance, native-block rounding, RNG reset/quirks, corrupt-edit rollback, pinned-slot contention, concurrent two-reader/writer coherence and multi-instance isolation. Measured core operations allocate0 heap blocks.
- Fresh P16-A O0/O2, P15-M2A..M2F standalone suites and42/42 native checkpoint suites PASS. M2G/M2H unchanged accepted checkpoint; exact historical FIR Functional O2 policy denial remains BLOCKED/not retried.
- Report `docs/p16b_sequencer_core.md`; evidence `out/p16b/`. P16-C may add only bounded trigger scheduling/Engine boundary integration under separate authorization.
- Accepted interface facade: `interfaces/SequencerCore.h`.


### P16-C Seven-track trigger scheduler and native Engine integration - PASS - 2026-11-05
- Fixed-capacity pointer-free events connect the P16-B rational clock and immutable pattern authority to the existing six-owner Engine. Tracks0--4 map to owners0--4; tracks5/6 share owner5 and dispatch closed then open. Main gates, eight substeps, inclusive probability, inactive-boundary latch reuse and ascending RNG consumption follow firmware `sequencer.c:488-543`; native note and low-seven-bit velocity pass once through existing trigger paths.
- One snapshot stays pinned through all seven event reads and releases before dispatch. Acquisition is bounded to8 attempts; Busy consumes/skips the due boundary while cached lengths advance. Ordinary capacity is7 because the minimum interval at BPM255 exceeds a32-frame native block; defensive overflow rejects newest and saturates diagnostics. Scheduler processing measured0 heap allocations and uses no locks, I/O, logging or unbounded loops.
- O0/O2 dedicated scheduler/source-derived-C/native-PCM suites each16161 PASS/0 FAIL. Exact128-position C traces, all patterns/gates/substeps, probability/RNG quirks, transport, tempo changes, live snapshot updates, Busy, chunk grouping, multi-instance, stopped baseline, all owners, shared hats and MIDI-then-sequencer PCM pass. Full firmware execution/hardware entropy equivalence is not claimed.
- Fresh P16-A/B, applicable P15-M2A--M2H and42/42 native checkpoint executions pass, including current-source FIR O0/O2, original-C snare/filter, serialization/concurrency and MIDI. The independent historical FIR Functional O2 Application Control denial remains documented and is not relabelled.
- Isolated VS Community18/MSBuild18.10.1/Release x64/v145 build used disabled pre/post deploy events; private810496-byte SHA256 `0AF1C01756AC8F8B10DC37B9BE9A88AAD5343811C31E065156659D5A55A95BB9`. Exact private-binary host state322/runtime1143/controller955/M2G91/M2H684/probe1033 and validator47/47 PASS. Seven assets match; no install or REAPER claim.
- Plugin scheduler is instance-local and stopped by default; existing MIDI/FIR/PDC and LXRSv2 patch/twelve-route authority remain unchanged when stopped. No host controls or state fields were added. Parameter locks, chaining, host sync, persistence, presets, morphing and GUI remain for later phases. Report `docs/p16c_trigger_scheduler.md`; evidence `out/p16c/`.


### P16-D Firmware-faithful parameter locks - PASS - 2026-11-06
- Two seven-bit automation slots per accepted physical step now decode original lower-CC/upper-CC2 identities through the P15 typed contract. Front off0 and sentinel255 clear; host-only IDs228--239, unsafe selectors, no-op/unimplemented targets and invalid values reject without changing effective state.
- Fixed pointer-free capacity is14 operations plus the unchanged seven-trigger batch. Dispatch preserves track0--6 and the original destination1,destination2,value1,value2,trigger sequence. Slot2 and later tracks win sequential collisions; closed gates, inactive steps and probability rejection leave prior effective values untouched.
- Fourteen Engine-local node destinations and a raw authoritative-base mirror preserve firmware lifetime without mutating `ParameterArray`, `ParameterStateAuthority<228>`, LXRSv1/v2 bytes, routing authority or host controls. Stop/restart retains locks; Engine init/complete restore clears them. New patch/MIDI/host base values win immediately and are the values restored later.
- Existing physical-base ordering is now patch -> sequencer effective -> velocity -> LFO -> render. P15-M2C velocity and P15-M2E LFO full original-C PCM suites remain exact. MIDI-first native-boundary ordering, FIR and controller/state behavior are unchanged.
- Dedicated unchanged-`automationNode.c` O0/O2 tests each218 PASS/0 FAIL, four PCM groups with pitch/cutoff/volume/two-slot/duplicate cases, first aggregate difference sample1 and0 measured allocations. Coherent concurrent publication and bounded Busy handling pass; TSan NOT RUN.
- Fresh P16-A/B/C and applicable P15 suites pass. Fresh native checkpoint42/42 passes including MIDI/state/concurrency/FIR, snare original-C and80,000-block filter differential. Historical exact FIR Functional O2 policy denial remains BLOCKED/not relabelled.
- Isolated VS Community18/MSBuild18.10.1/Release x64/v145 build disabled pre/post deploy events. Private811008-byte SHA256 `C7E2C8B285105F8ABE44D172B68BCAB0EA4C263CAB5A26E2EF16BC73C78A8C83`; state322/runtime1143/controller955/M2G91/M2H684/probe1033 and validator47/47 PASS. Not installed; protected installed SHA `89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82` preserved.
- Report `docs/p16d_parameter_locks.md`; evidence `out/p16d/`; accepted facade `interfaces/SequencerParameterLocks.h`. Pattern chaining, host sync, sequencer persistence, presets, morphing and GUI remain deferred.


### P16-E Pattern chaining, shuffle and performance operations - PASS - 2026-11-06
- Pinned firmware chain state now changes only on track-0 wrap, uses absolute `changeBar+1` modulo, fixed next0--7 and one full instance-RNG draw for random-next8--14 ranges2--8 before probability. New-pattern rotations/lengths apply coherently from the same immutable bank snapshot; independent track wraps never transition.
- Exact16-entry shuffle phase is applied to the rational unshuffled clock and next32-frame boundary. Falling/negative source deltas serialize at one event per native block then catch up without cumulative drift, inversion or duplicate dispatch. Tempo1/120/255 and live changes pass.
- Seven-track mute preserves progression while skipping ordinary gate/probability/RNG/locks/triggers. Source roll selectors, periodic/current-note and one-shot/default-note behavior pass; rolls ignore mute/gate/probability and apply current-step locks before trigger. Fixed worst-case capacities are21 triggers/42 lock operations, zero measured processing allocations.
- Original recursive Euclidean generation matches unchanged pinned `EuklidGenerator.c` exhaustively and changes only gates/length. Track/pattern copy and clear preserve exact source scopes and publish through inactive candidates. SOM entry points/RNG/node behavior are audited but DEFERRED because no complete documented control/lifecycle contract exists.
- P16-D base -> lock -> velocity -> LFO order, patch/routing immutability, MIDI-only stopped PCM and deterministic multi-instance behavior remain intact. P16-E-R1 froze the final nineteen-file source inventory and executed approved Visual Studio x64 `/Od` and `/O2` builds: each9869 PASS/0 FAIL. No production, interface or test source changed during recovery.
- Code Integrity events identify the original pre-launch denials as enforced `VerifiedAndReputableDesktop` policy decisions (`0xc0e90002`, GUID `{0283ac0f-fff1-49ae-ada1-8a933130cad6}`). Direct permitted recovery executions pass `TransientGenTest`17/0, `P12b4cResamplerTest` with zero failures and original-C `P12b4cSnareRenderTest`61/0 with zero render allocations. The other39 checkpoint executions remain source-identical PASS. The historical FIR Functional O2 remains separately BLOCKED. No policy/security change, signing, relocation or evasive loader was used.
- Isolated VS Community18/MSBuild18.10.1 Release x64/v145 final private binary SHA256 `1DF2873346E3EFE727C0C1396B335406A1AD66C8E3E608D61440303938DF6140`; state322/runtime1143/controller955/M2G91/M2H684/probe1033 and validator47/47 PASS. Initial private host run with assets packaged one directory high failed audibility/controller fixtures; corrected `Contents/Resources/LXR` packages pass and failure evidence is retained. No installation occurred; protected SHA `89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82` and seven resources remain unchanged.
- R1 reran Steinberg validator47/47 against the exact private SHA `1DF2873346E3EFE727C0C1396B335406A1AD66C8E3E608D61440303938DF6140`; the protected installed SHA and all seven resources remain unchanged. Report `docs/p16e_performance_sequencer.md`; evidence `out/p16e/` and `out/p16e-r1/`; accepted facade `interfaces/SequencerPerformance.h`. Host transport/sync, sequencer LXRS persistence, roll recording controls, SOM, presets, morphing, GUI and samples remain deferred.


### P16-F1 VST3 host transport and tempo synchronization - BLOCKED (REAPER pattern-data access) - 2026-11-06
- Internal and Host Sync modes are implemented with direct validity-preserving VST3 ProcessContext capture, Q32.32 absolute PPQ anchoring, integer micro-BPM conversion, bounded start/stop/resume/seek/loop policy and native32-frame adapter offsets. IDs240--242 expose enable, Internal/Host Sync and internal BPM; IDs0--239 and LXRSv2 516/520-byte state remain unchanged. The new controls deliberately restore to defaults because P16-F2 owns sequencer persistence.
- Dedicated current-source O0/O2 each6237 PASS/0 FAIL with0 measured processing allocations. Current P16-E O0/O2 each9869 PASS/0 FAIL. Exact private transport104, state322, runtime1143, controller955, M2G91, M2H687 and probe1033 all PASS; Steinberg validator47/47 PASS.
- Isolated VS Community18/MSBuild18.10.1 Release x64/v145 build disabled pre/post deploy events. Private825344-byte SHA256 `C88C6B264AC70BC4236B564ADE60F1310F2DD18BC526829F7242FD8855C64C7D`. Protected installed SHA `89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82` and seven resources remain unchanged.
- REAPER7.80 x64 loaded that exact private module and passed host controls/readback, two-instance isolation, bypass, external-MIDI audio with sequencing disabled, bit-identical repeated offline render, save/reopen compatibility and26-sample PDC at48kHz. The production default pattern bank is source-faithfully cleared and P16-F1 has no permitted authoring/loading/persistence surface; the Host Sync no-MIDI render is therefore silent. Mandatory pattern-driven REAPER transport/lock/shuffle/roll cases are inaccessible and classified BLOCKED, not PASS.
- A compile-time internal exact-binary probe injects an immutable test bank and passes transport audio but is absent from production. No factory pattern, hidden inlet, pattern parameters or ad-hoc serialization was added. Portable native checkpoint freshly built all suites:35 executions PASS and9 are BLOCKED at launch by Windows Application Control WinError4551; no policy bypass. Historical exact FIR Functional O2 remains separately BLOCKED.
- Overall and publication classifications are BLOCKED. No staging, commit or push occurred; local/live remote remain `7749ef1a0203f5ab92bf0e2f447b53bb95dadf68`. Report `docs/p16f1_host_transport.md`; evidence `out/p16f1/`. P16-F2 should first supply the authorized pattern-state ingress/persistence needed to rerun the missing REAPER gates.


### P16-F2 Versioned sequencer persistence and production ingress - BLOCKED - 2026-11-09
- Starting branch/local/live remote matched p12b-4c-fir-resampler at7749ef1a0203f5ab92bf0e2f447b53bb95dadf68. All25 pre-existing P16-F1 paths reconciled; all23 recorded implementation hashes matched; pinned firmware/dependencies and installed binary/seven resources verified. Prior P16-F1 evidence preserved.
- New explicit little-endian LXRSv3 embeds frozen v2, complete independent LXQBv1 bank and transport module. Canonical core51061/stream51065; ceiling65536. All7168 seven-byte records, gates, lengths, rotations, next/repeat and shuffle persist. v1/v2/legacy migrate with source-empty bank and OFF/Internal/120 transport. No raw object serialization. Standalone O0/O2 each152002 PASS/0 FAIL against independent Python struct/zlib whole-project and bank fixtures.
- Existing one-attempt patch/routes coordinator now captures bank pin and transport. Audio retains its prior complete tuple on Busy and selects a fully staged new tuple before rendering. Fresh transient Engine construction fixes observed retained oscillator-RNG reload divergence without changing synthesis equations. Final expanded exact-binary suite3300/0 atMSVC/Od and/O2. The final current-source checkpoint generated its own fixtures and executed O2 successfully; earlier denied MSVC/O2 and LLVM/O2 artifacts remain separately policy-BLOCKED. TSan NOT RUN.
- Minimal import-only editor reads bounded .lxqb files off audio. REAPER7.80 user-completed actual import and independent state extraction prove exact fixture SHA74A5A73030212A1F3641CD6C5BB86AB8290DB269530F9E6E43D2F68D988A1D90 before/after reopen. Preceding privateCC4934...97AD7 run passes measured pattern PCM at120/60, Internal, MIDI coexistence, all228 patch/twelve route/three transport readbacks, bypass, bit-identical repeated/reopened PCM and26-sample PDC.
- Final privateC2ECE09B0AA41EF6BBD13FADB4A4D8843654A2F84A242D0CE2510BA8C8B7C939 is Release x64/v145 with deployment events disabled. Final host state/runtime/controller/M2G/M2H/F1/F2/probe322/1143/955/91/687/105/3300/1033 PASS; validator47/47 PASS. Final-hash REAPER workflow remains incomplete behind a modal after Computer Use app-access timeout. Earlier-hash results are not substituted for final-hash acceptance.
- Combined F1/F2, F2 implementation certification and publication remain BLOCKED: remaining live/performance REAPER gates, negative-control O2 launch restriction and exact-DLL allocation/high-water instrumentation are unfinished. Existing three host controls do not expose manual roll; substeps are not counted as manual-roll acceptance. Historical exact FIR Functional O2 separately remains BLOCKED. Native .pat/SOM/morph/presets/editor/user samples deferred.
- No staging, commit or push. Report docs/p16f2_sequencer_persistence.md; complete current findings and explicit NOT RUN/BLOCKED evidence under out/p16f2/. Not ready for the successor milestone.

### P16-F3 final-host checkpoint — BLOCKED — 2026-11-10

- Reconciled all 39 starting F1/F2 paths, empty index, branch/local/live remote `7749ef1a0203f5ab92bf0e2f447b53bb95dadf68`, firmware/dependency pins and protected module/seven resources before edits. All 1,117 recorded prior evidence hashes still match. No reset, cleanup, branch change, staging, commit or push.
- Minimal production manual-roll adaptation adds stable IDs 243 (rate 0..13, default 8) and 244 (held track mask 0..127, default 0). Press/release edges follow original front-panel semantics; selector zero is one-shot, fourteen original periods and shared-hat ordering remain. Both controls are transient, excluded from frozen LXRS v3, and release on stop/stopped seek, disable, deactivation and complete restore. Original IDs 0..242 remain unchanged. The original NRPN 200..206 mute commands now reach the processing-owner scheduler; ordinary triggers honor mute while original rolls and external MIDI retain their behavior.
- Preserved reproductions and narrow fixes: an intra-callback tempo marker was misclassified as a seek; REAPER stop-plus-rewind left a held roll active; production MIDI discarded track mute; signed host sample continuity overflowed at `INT64_MAX + 1`. The endpoint LLVM overflow-trap reproduction is preserved, and the guarded transport passes O0/O2 6,246 checks each with zero measured processing allocations. No synthesis formula, RNG progression, bank wire field or parameter normalization was redesigned.
- Replacement private build006 SHA `28F3639D2AB00FFBF34857BDC1B2ED89260F1360E7D2973C00209B8F90AC654A`, 1,248,256 bytes, VS Community 18/MSBuild 18.10.1.42706 Release x64/v145. Build input fingerprint `DD32D5822B61FB20491BE1C7ADE785E53BB69B71F0C0ACC4D20043EE4D0C8C39`. Deployment events disabled; protected installed SHA `89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82` and all seven resources unchanged. Earlier candidate reports remain separate, including `out/p16f3/checkpoint-build005/`.
- Final replacement host suites: state 322, runtime 1,143, controller 955, M2G 91, M2H 689, F1 105, F2 transaction/PCM 3,300, roll 260, MIDI/mute 216, probe 1,033 and validator 47/47 all PASS. Additional production importer/rejection 62, focused tempo 27, roll Od 260, listening-fixture 344 and resource 39 checks pass. Unchanged codec O0/O2 each 152,002. Current C/D/E O0/O2 suites pass 16,189/246/9,869 checks per variant. Current native checkpoint has 31 PASS and 13 policy-denied artifacts; approved LLVM equivalents and remaining current snare-render O0/snare-cymbal-hat O2 blocks are distinguished. Historical exact FIR Functional O2 remains separately BLOCKED. TSan NOT RUN.
- Exact-DLL processing probe measures zero imported allocator calls across 4,000 varied callbacks, including restore, transport, locks, rolls, MIDI and NRPN mute. Observed processing stack is 13,616 bytes on a 1 MiB thread; bounded nonRT import/setState/getState watermarks are 213,872/275,776/228,864. These are observed scopes with static/unwind review, not an exhaustive whole-call-chain proof. The optional legacy LLVM P16-D test's 1 MiB stack exhaustion is a test-harness limitation; documented current GCC P16-D passes independently.
- The user confirmed all 17 controlled cases with no issues, in VLC through headphones. Replacement-DLL WAVs plus reload repeat are byte-identical to that playlist; no human observation was inferred from PCM. Final-hash REAPER profiles repeat save/reopen and offline/transport/performance checks, with each completed result separately recorded. Live start/stop/resume, seeks, loop wraps, tempo and held-roll release produce nonzero peaks. Production rate automation and read-only exact-module diagnostics establish the fourteen roll event counts and released masks; unfinished render finalization is not credited as PCM PASS.
- Final-hash production GUI import remains BLOCKED: Computer Use returns unavailable accessibility and a screenshot of a different task project under the terminal overlay, even for the explicitly visible task import window. No coordinate click was made on the mismatched project. Earlier build004 GUI import and canonical full-bank readback are supporting evidence only. Additional host performance cases remain native-only or unfinished, as the evidence matrix states. Combined F1/F2/F3 and publication remain BLOCKED; no next milestone. Report `docs/p16f3_final_host_certification.md`; evidence `out/p16f3/`. Native `.pat`, SOM, presets, morphing, graphical editing and user samples remain deferred.
- Final-hash profiles 026/027 subsequently completed 40 host checks each. Rich-state profile 034 passes 44 more checks with all twelve routes enabled per instance, changed patch values, distinct transport settings, host bypass, complete two-bank byte identity, transient-roll reset and three bit-identical matched MIDI renders. Current LLVM Od F2 transaction/PCM passes 3,300; the denied MSVC artifact remains BLOCKED. Raw cross-callback MIDI PCM inequality is preserved (first frame 9,191): baseline and current MIDI dispatch discard message offsets. Sample-accurate MIDI/cross-callback MIDI identity is not certified; this milestone did not redesign the existing MIDI timing contract.

### P16-F4 MIDI partition repair and certification recovery — FAIL — 2026-11-14

- Verified the exact70-path/438-source F3 baseline, empty index, local/live HEAD7749ef1, all immutable dependencies, private28F363...654A and protected89F6DF...FF82/seven resources. Inventoried119,769 earlier evidence files before edits; preserved original reports and raw failures.
- Original valid Note On9,600/Off16,800 fixture diverges first at LXR dispatch: iPlug2 keeps offsets but LXR discarded them. Exact-DLL microscopes reproduce each legacy callback render and locate first native PCM difference8,448 before FIR. Narrow LXR-only resident4,096-event callback retention, offset/sequence ordering and sample-loop dispatch preserve the native32-frame/FIR/PDC contracts, deferred authority FIFO and bypass/flush behavior.
- Private build002 SHA179E6643FC4E6905E89EF8FB273382CCAB866BC145C24B3F2D9917B5EBFD0FC8,1,251,328bytes, VS18/MSBuild18.10.1/Release x64/v145, deployment disabled. Original fixture248/0 and expanded note/boundary/channel/shared-hat matrix1,406/0; native and host PCM match all eight partitions. Real REAPER127/512 MIDI coexistence now matches; each profile40 host checks, rich restore44/all12 routes/two banks/three identical renders, validator47/47 and PDC26 pass.
- Current snare renderOd61/0 and percussionO247/0 execute through approved VS LLVM; fresh GCC counterparts pass too. Other current variants and historical exact FIR O2 remain individually policy-BLOCKED. No Windows policy/certificate/image/loader change.
- Final-DLL4,000 callback scope measures0 allocations,13,648-byte stack on1MiB; full4,096-event callback0 allocations/9,128bytes. Fourteen active-routing instances complete two192,000-frame bit-identical renders with all14 distinct banks and full states restored. Earlier OFF-route stress mismatch/frame0 and selector0/12 cross-profile inequalities remain unreconciled raw evidence. Live transport records seven wraps and stop-release without claiming sample-exact live timing.
- Final CC matrix reveals another genuine ingestion defect: two valid CC88 points at9,600/9,664 collapse to the last point in pinned ProcessParameterChanges. Exact-DLL suite1,652 PASS/8 FAIL; first host difference9,600(disabled)/9,601(sync), maximum0.04967037262126655. Production repair stopped under user stop condition8; no assertion weakened and no shared framework change.
- Overall/F3-F4 FAIL; F1/F2 and publication BLOCKED. Production Import blocked by Computer Use app-approval timeout before input; renewed listening pending ("approved you can keep going" is continuation approval, not auditory acceptance). Full192-row reconciliation and final hashes/evidence are in out/p16f4/; report docs/p16f4_final_certification_recovery.md. No staging/commit/push or successor features.

### P16-F5 timestamped MIDI CC repair — FAIL — 2026-11-15

- Exact F4 baseline74 changed paths/442 source hashes, empty index, branch/local/live HEAD7749ef1, dependencies, protected DLL/seven resources verified. Starting bytes archived;141348 earlier evidence files (8757476588bytes) rehashed unchanged.
- Reproduced1652 PASS/8 FAIL. First loss: pinned iPlug2 reads only the final synthetic CC queue point. Narrow LXR-local non-owning one-point views retain all bounded accepted ordinary CC points, preserving the existing F4 timed-note queue/sort and callback-level host automation. Source admission376, CC/event visits4096 each, shared MIDI capacity4096 with deterministic diagnosed rejection; no shared framework/DSP-equation changes.
- Private001 SHA1B2869DCD2C7982E32A9EDE83DECB46E90B2D46CD4B18DCC120EB172C43004AB,1253376bytes, VS18/MSBuild18.10.1/Release x64/v145; deployment disabled. Original CC suite1660/0, extended548168/0,135 within-candidate partition renders/15 rate-mode configurations, real REAPER127/512 six MIDI PCM comparisons exact. Validator47/47; PDC26. Resource probe8252/0, zero observed processing allocations,19616-byte peak processing stack on1MiB. Raw compiler denials and the standalone2MiB GCC/LLVM stack-contract reconciliation remain distinct.
- REAPER40 checks/profile, rich44/all245 controls/twelve routes and repeated reopened PCM pass. Production Import remains BLOCKED before input by Computer Use app-approval timeouts. Both exact tempo-boundary builds remain policy-denied. Fourteen rich-bank instances produce one valid192000-frame WAV; matched reopen/rerender incomplete, host responsive. Empty-bank roll-count oracle is not applicable to these short/chained/shuffled banks. Historical raw inequalities remain unreconciled.
- User reports "all is good" for seven F5 listening cases; no auditory issue reported, player/output unspecified. Historical17-case acceptance preserved.
- Final frozen-note audit exposes a new genuine preservation failure:47/76 note-only PCM files differ from F4. One identical corrected host executable reproduces all76 frozen files with F4 DLL and differs47 times with F5 DLL. Sync native first sample0 is-47 versus-14460 before FIR and external MIDI. Disabled original single-voice PCM is identical; simultaneous multi-owner disabled audio differs. Internal first differing state/input field not established; no synthesis/FIR cause invented.
- Production changes/active certification stopped under F5 conditions6/12/13. Overall/F3-F5 FAIL; F1/F2/publication BLOCKED. No staging, commit, push or successor features. Complete36-item report, inventory, regression reconciliation and first-divergence evidence: out/p16f5/; docs/p16f5_midi_cc_timing_repair.md.

### P16-F6 local frozen-PCM investigation — PASS (requested scope) — 2026-11-15

- Used original C:\Projects\lxr-vst3 only: branch/HEAD7749ef1,447 F5 source hashes/79 changed paths, empty index, protected DLL/seven resources and all76 original F4 references verified. No clean-clone substitution, reference regeneration, staging, commit or push.
- First verified divergence is filter input-history initialization: identical LXRS/effective patch state, but six zi scalars contain indeterminate storage after F5 complete restore. This precedes external MIDI/FIR and produces first native sample-14460 versus F4-47. Original firmware voice storage is zero-filled .ccmz (startup assembly verified); SVF_init omits zi. Fresh Engine{} automatic storage did not reproduce that startup guarantee.
- Actual F4/F5 DLL snapshots and F5 note-only callbacks with CC queues absent isolate timing from CC ingestion: same47/76 discrepancy persists. Zeroing only six zi fields in a test-only intervention makes first native PCM exactly F4. Poisoned construction reproduces6 failures before correction.
- Minimal production correction: zi=0.f default member initializer in ResonantFilter.h. Layout/equations/init/reset bodies unchanged; repeat init still preserves an existing zi as original C does. LXR.cpp/h and corrected note timing unchanged. No conflict between frozen F4 references and timestamped-note contract.
- Exact private001 SHA4591AB2EBFDD6FE738F4983B9FBBF292C4B4E06DA7E0F7528C1E9426E050BE1D, Release x64/v145, deployment disabled. Original1660/0 (all eight CC failures pass), expanded548168/0, frozen76/76 bit-identical, restored state/first native trace exactly F4. Initialization16/0 atO0/O2; original-C filter80000 exact blocks each. Resources8252/0, zero observed processing allocations/19616-byte peak on1MiB; validator47/47.
- Requested investigation/repair PASS; broader final REAPER certification and publication remain open and are not transferred to the changed DLL. Previous evidence preserved. Report docs/p16f6_filter_startup_initialization.md and out/p16f6/verification_report.md.

### P16-F7 final REAPER certification — BLOCKED — 2026-11-16

- Original workspace verified:450 source hashes/83 changed paths exactly match F6; empty index, branch/local/live HEAD7749ef1, pinned dependencies and protected DLL/seven resources unchanged. Earlier173216 evidence files/11211807907bytes rehash unchanged. F7 adds documentation and generated private evidence only; no production source changes, staging, commit or push.
- Reused frozen F6 private candidate SHA4591AB2EBFDD6FE738F4983B9FBBF292C4B4E06DA7E0F7528C1E9426E050BE1D,1253376bytes, Release x64/v145. All production inputs match; zi zero initialization is preserved without equation/timing/FIR changes. Fresh original1660/0 including8CC cases, expanded548168/0, frozen76/76 exact; initialization16/0 and original-C filter80000exact blocks at each optimization pass.
- Actual REAPER127/512 profiles40 checks each, rich restore44/three identical renders, PDC26 and validator47/47 pass. Fourteen distinct rich banks/full state restore complete two192000-frame renders with identical PCM SHA56EDC4DDDFD410CCF6FEAA3299FDCD4C64598B2F4E25ADA12A31826D5D863923. All14 roll selectors produce expected counts and release masks.42actual host event batches at60/90/120/140BPM meet next32-native-frame quantization; standalone exact tempo-boundary27/0 passes. Coarse live transport includes7wraps, seeks, stop/resume and rollrelease.
- Exact-DLL resources8252/0 measure0processing allocations and19616-byte observed peak processing stack on1MiB; finite coverage/limits documented. Current snare/percussion and dedicated suites recover through approved VS LLVM/MSVC; M2H O2131611/0, P16E O29869/0, Mixer O228/0 after current reachable-input equivalence verification. RuntimeSilence O2 remains policy-denied; historical exact FIR O2 remains separately blocked. No Windows enforcement changes.
- Operator replies “all approved” for15final-DLL listening cases; setup/details unspecified. Production Import BLOCKED: exact task window selected but Computer Use returned another application's screenshot/no accessibility;0inputs and interaction stopped. Complete exact live tempo transitions/90/140 repeated PCM and several granular host performance cases remain unavailable. Enhanced observer raw31PASS/1FAIL/0events is a harness/timeline failure, not an invented plugin diagnosis. Raw selector0 cross-profile automation inequality and historical OFF-route stress inequality are preserved; selector12 now matches.
- Overall/combined host certification and publication BLOCKED; F5/F6 repair preservation PASS within measured scope. No new confirmed implementation defect found. No next feature milestone. Full classification,638-row reconciliation (including every473prior row), identity/integrity and34-item report: docs/p16f7_final_reaper_certification.md and out/p16f7/verification_report.md.


### P16-F8 certification closure ? BLOCKED ? 2026-11-16

- Verified F7 baseline451 source files/84 changed paths exactly; branch/local/live HEAD7749ef1, empty index, pins, frozen4591AB candidate and protected89F6DF DLL/seven resources match. No production source, resource, state format or binary changes/build.
- Fresh preservation:76/76 frozen PCM identical, original1660/0 and expandedCC548168/0, initialization16/0 atO0/O2 and original-C filter80000exact blocks each. zi initializer/equations/timestamped MIDI/LXRSv3 preserved.
- Exact formerly denied F7 GCC RuntimeSilenceO2 executableCEC55265...E4D7ECE executes normally in place:419/0,0render allocations. No security/path/image changes; reason for changed policy acceptance unknown. Previous denials and separate unsuccessful LLVM harness build retained.
- REAPER production Import remains BLOCKED?PRODUCTION_IMPORT_GUI_UNAVAILABLE: new exact task project/PID20364/module verified, but selected window1903050 captures terminal/no accessibility.0clicks/keys, graphical interaction stopped. No direct operator completion is inferred.
- Corrected only copied F8 tempo Lua syntax;75observed batches at60/90/120/140BPM match independent next-native-boundary expectations. Four paired96000frame48kHz renders identical, including previously missing90/140. Complete first-event transition/map/loop traces remain open. Actual production NRPN mute/host-roll render passes scoped64roll-event and released-mask checks; fixed/random source-derived aggregate checks remain distinct from full granular event coverage.
- Rechecked completed F7 fourteen-bank/full-state/192000frame paired PCM evidence without stress rerun. LXRSv3/v1/v2/legacy compatibility,0measured processing allocations,19616byte observed stack,validator47/47,PDC26 and15-case operator listening approval retained for unchanged candidate. Raw selector0/oldselector12/OFF-route comparisons remain separately traceable; OFF pair has32differing scalar samples confined toframes0..24,max13, cause unresolved. No reference or acceptance weakened.
- Docs:docs/p16f8_certification_closure.md; full inventory/matrices/31-item report:out/p16f8/. PublicationBLOCKED; no staging/commit/push or next milestone.


### P16-F8 continuation ? BLOCKED ? 2026-11-16

- Previous F8 checkpoint matches exactly: 452 source files, 85 changed paths, empty index, local/live HEAD7749ef1. Reverified 10,117 preceding F8 artifacts; prior root evidence/docs archived under out/p16f8/resume-001/prior_f8_checkpoint before report updates.
- Fresh protected checks pass: frozen76/76, original1660/0, expandedCC548168/0, initialization16/0 and original-C filter80000 exact blocks at O0/O2. Frozen4591AB candidate, protected89F6DF DLL/seven resources and production inputs unchanged.
- Fresh exact Import PID16132/window274254 remains inaccessible: activation still captures terminal/no accessibility. No button click or typing; graphical interaction stopped, operator completion not observed.
- New actual32-frame REAPER trace: 3,879 contexts, 145 expected-native-boundary and 161 state checks pass, including actual loops and tempo-map playback. Complete first-transition/live-map PCM obligations remain open.
- Granular source-backed performance: 1,227 state, 63 event-batch and 58 timing observations pass. All seven tracks/eight substeps and probability-zero acceptance observed; unequal lengths/rotation/shuffle/fixed-random chains match independent oracle, including exact pattern6-to7 boundary. Two matched192000frame48kHz renders identical (B57B74E1...2E407C4). Added automation-operation snapshot diagnostic is unavailable, not credited.
- Corrected raw roll replay compiles under MSVC and VSLLVM but both images are denied before entry (WinError4551); earlier malformed fixture, build and invalid sample-format attempts preserved. No security bypass. Raw selector/OFF-startup reconciliation remains unresolved; RuntimeSilenceO2 prior exact419/0 remains valid separately.
- Overall/publication BLOCKED. Documentation and ignored verification evidence only; no staging/commit/push, deployment, forced termination or successor features.

- Late operator handoff: operator intends to perform exact production Import; expired watcher restart/completion remains pending. Second supported app-control surface lists exact window274254 but app access times out before state; no input. Independent pre-operator bank differs from musical fixture. Prepared malformed/reopen follow-up is NOT RUN.
- Additional finite transport capture4760 contexts/163timing/188state PASS; first-callback transition flags not captured despite96commands. Missing transition gates remain open, no additional assertion waived.


### Renewed P16-F7 acceptance request, preserved F8 continuation - BLOCKED - 2026-11-16

- Current452file/85path baseline exactly matches preceding F8; no unexpected source change. Frozen4591AB DLL/dependency pins/protected89F6DF DLL and7resources unchanged.
- Fresh1660/0,76/76 frozen, initialization16/0 and original-C filter80000 exact blocks atO0/O2 pass. New expandedCC execution BLOCKED4551 before entry; CodeIntegrity3077/policy0283ac0f-fff1-49ae-ada1-8a933130cad6 retained separately from earlier548168/0 PASS.
- Operator reports production Import success/statusBankloaded in intended new project. Verified PID29672 readback still has original bank and watcher captures no change; operator saved-project/title evidence unavailable. Preserve discrepancy; full Import/rejection/reopen/PCM remains BLOCKED/NOTRUN. No plugin cause claimed.
- Additional F7 Euclidean-generated and copy/clear result host acceptance remains NOTRUN; no graphical editing feature added or waiver inferred. Earlier measured F8 performance, fourteen-instance/state/resource/validator/listening evidence retained in scope. Full report/matrix out/p16f8/resume-002.
- Publication BLOCKED. No production change/build, staging/commit/push, security bypass, forced termination or next milestone.


### P16-F10 targeted certification closure - BLOCKED - 2026-11-16

- Exact452file/85path F8 baseline584E8FF5...5217779 verified; expected branch/local/live HEAD7749ef1, empty index. Frozen4591AB production inputs/resources and protected89F6DF DLL/7resources unchanged. No F9 evidence exists locally.
- Distinct native-codec BankA(C305CC5B...D33F3,gates1111,shuffle0)/BankB(51CF442E...C8C5D,gatesffff,shuffle87) reference renders differ; each144000frames48k stereo. Exact PID18020 applied bank isA. Supported capture again shows terminal/nullAX;0inputs, stopped. Production Import/rejection/imported restore BLOCKED/NOTRUN; no routine owner repeat.
- Exact PID4664 four required tempo maps executed, two markers inserted live.1403contexts/1394 independent PPQ checks/44 native timing/47state checks pass; all4 paired render PCMs equal. Actual frames288000/223968/210240/274272; exact fractional endpoint duration not certified. Required first ordinal0/128 traces remain missing; previous exact pattern/loop-first witnesses retained.
- ExpandedCC oldLLVM and unchanged-source MSVCO2 both denied4551 before entry. Production-transform fixture producer and corrected original-path roll replay also denied. No policy/image/path/loader bypass. Prior548168/0 kept separately; Euclidean/copy-clear host cases unexecuted.
- Four raw FAILs retain detailed sample values/provenance/repair-era evidence; original offline partitions/transient phase missing, all source causes unresolved. No reference change. Existing76/76/MIDI/filter/state/stress/resource/validator/PDC/listening results retained for unchanged candidate.
- Evidence/docs:out/p16f10/,docs/p16f10_certification_closure.md. Overall/publication BLOCKED; no production source/build, staging/commit/push, forced termination or new milestone.

### P16-F11 infrastructure recovery - BLOCKED - 2026-11-16

- Exact453file/86path F10 baseline0CF86A18...50A9E7, original branch/local/live7749ef1, empty index and dependency pins verified. Frozen4591AB inputs/resources, protected89F6DF DLL/7resources and76frozen PCM pairs preserved; no production rebuild.
- Isolated PID37660: supported ReaScript project/FX GUID/action40042/cursor0.125-to0/same-instance proof PASS. Read-only matching-PDB/vtable probe verifies BankA C305CC5B...D33F3. Session1/Default identities agree, but both supported GUI surfaces capture terminal pixels, including after operator foreground report. Zero input; actual Import, BankB, five rejection and imported restore/PCM gates BLOCKED/NOTRUN.600s watcher stopped normally.
- Selective native execution: preserved LLVM qualification16/0; original-path MSVC corrected roll replay98/0; unchanged-source VS LLVM O2 transformation producer22/0. Fresh qualification, original/fresh expandedCC and old MSVC producer denied4551 before entry; CI3077/0xc0e90002/policy0283ac0f. No security/image/path/loader bypass; prior548168/0 separate.
- Independent original-C/full-bank Euclidean and copy-clear models pass. Two REAPER384000frame48k stereo paired renders match after save/reopen;8full51065byte streams match. Snapshot observers retain31PASS/1FAIL each (1event vs required>10).857/888state/1event/1timing checks pass in sampled scope; full performance host gates BLOCKED. No broad historical campaign.
- Four raw inequalities remain Unresolved/FAIL. Receipt replay advances diagnosis but does not reproduce historical WAVs exactly; transient/partition/native-first provenance missing. First-event transport coverage incomplete. Prior MIDI/filter/LXRS/stress/RT/validator/PDC/listening evidence retains scope.
- Evidence:out/p16f11/;docs/p16f11_infrastructure_recovery.md. Overall/publication BLOCKED; no staging/commit/push, installation, forced REAPER termination or successor feature work. Next milestone NOT READY.


### P16-F12 observation harness recovery - BLOCKED - 2026-11-16

- Verified original454file/87path F11 inventory3BDBF264...BF51283, expected branch/local/tracking/live7749ef1 and empty index, all dependency pins, frozen4591AB production inputs/resources and protected89F6DF DLL/seven resources. No production edit or build.
- Diagnosed observer acquisition limitation: latest-native-block mLastBatch is erased each block; polling/modulo/context dedup cannot retain a complete event history. Fresh original reader2261contexts/6of44events,31PASS/1FAIL; independent2259state/6event/6timing checks agree. First missing ordinal8/native5504. F11 one-event evidence preserved.
- Bounded session/instance-aware collector passes7synthetic tests, including order/timestamps/duplicates/timeouts/truncation/no one-event PASS. Native1/32/127/512 qualification and fieldwise MSVC reader denied4551. Permitted sampled-reader change still captures1event from live fixture, FAIL; full native/host qualification remains BLOCKED. No production trace instrumentation.
- New REAPER PID30920/project/FX/private module and harmless action40042 proof PASS. Before/final BankA C305CC5B...D33F3 identical. Both supported app-control paths time out or lack usable context;0Import clicks/keys. TargetB51CF442E...C8C5D/rejections/imported PCM/reopen NOTRUN;600s watcher stopped normally.
- ExpandedCC original-path VSLLVM denied4551/0assertions, exact CodeIntegrity policy retained. Fresh corrected MSVC receipt replay98/0. Four original PCM inequalities remeasured; causes remain Unresolved, rawFAIL preserved, no reference rewritten. First-event28case grid/Euclidean/copy-clear full traces stay BLOCKED; prior state/pairedPCM preserved.
- Existing MIDI/filter/LXRS/RT/stress/validator/PDC/listening evidence retains finite unchanged-input scope. Evidence/matrices/report:out/p16f12/;docs/p16f12_observation_harness_recovery.md. No staging/commit/push, deployment/security change/process termination/new feature. Next milestone NOTREADY.

### P16-F13 development checkpoint - development gates PASS; release BLOCKED - 2026-11-16

- New user-authorized policy separates a reviewed development Git checkpoint from release certification. Earlier release-level GUI publication restrictions are superseded only for this development checkpoint; historical reports remain unchanged.
- Exact F12 baseline:457 source files/90 changed paths, fingerprint1822600B81B76885D8839589812916782554CEA6775D922A4665F3A6AAA30688; branch/local/live development HEAD7749ef1, empty initial index, origin vlndv/lxr-vst3, pins, frozen4591AB private DLL/235 production inputs and protected89F6DF DLL/seven resources verified. No production source edit or instrument rebuild.
- Production delta/dependency/RT audit found no confirmed unresolved current-source functional or safety defect. Fresh59 native/host executions PASS, including44 native O0/O2, ten VST3 host suites, validator47/47 and four initialization/original-C filter checks. Fresh original MIDI1660/0, expanded CC548168/0 and76/76 untouched frozen PCM comparisons PASS. First two attempts stopped on missing trace directories; raw harness failures preserved, corrected separate runs PASS. No Application Control bypass or denied-run relabeling.
- Retained hash-bound P16-B/C/D/E, codec152002/0 each, exact-DLL resources8252/0 (zero measured processing allocations/19616-byte observed peak on1MiB), REAPER pattern/PDC26 and fourteen-instance paired save/reopen evidence accepted only within measured scopes. Four historical raw PCM FAILs remeasured; causes remain unresolved release risks, no reference rewritten or obsolete-contract cause invented.
- One coherent combined source/test/documentation development commit is planned because transport/persistence/restore and host-test dependencies cross the suggested boundaries. Explicit staging, complete staged-diff/blob checks and normal fast-forward development publication receipts are recorded in out/p16f13/; no main merge/tag/install. Development checkpoint completion requires those publication receipts; release remains BLOCKED.
- Stable production-boundary preallocated SPSC observer design, supported Import-path analysis and bounded four-workstream roadmap:docs/p16f13_development_checkpoint.md. Actual Import/rejections/imported reopenPCM, complete first-event timing/Euclidean/copy-clear traces, four historical comparisons and remaining exact-artifact policy gates stay BLOCKED. Next milestone:observer qualification and certification evidence recovery; preset/kit/morph/UI/sample implementation not started.


### P17-A original preset/kit architecture audit - PASS (audit scope); release BLOCKED - 2026-11-17

- Verified clean baseline/local/live development HEAD02c4005c91118d369473511e0d5a4b41e30202be, origin vlndv/lxr-vst3,458 source fingerprints,235 unchanged production inputs, pinned original firmware/iPlug2/SDK, protected89F6DF DLL/seven resources and757 F13 artifact hashes. Final index remains empty; no staging/commit/push or instrument build/install.
- Source confirms .snd is a full six-voice228-byte kit plus name8 (236 total); closed/open hats share one synthesis owner. Complete228-row ownership matrix includes reserved slots, global decimator and seven note overrides. Globals begin251, count22; runtime/menu228..250 are a separate domain from plugin route IDs228..239.
- Executed unchanged original C writers confirm pattern payload50361, .pat50369, v2 .all/.prf50946; actual performance extension is .prf, not earlier prose .perf. Explicit-field encoding required; MCU bitfield ABI, unacquired legacy/factory files and hardware load timing remain UNKNOWN. No original factory kit files are present in the pinned checkout; external companion content/licence not inferred.
- Original kit loading broadcasts12 route messages then228 scalar updates without full synth/sequence reset; armed hardware automation recording can alter locks. Original global reader has an uninitialized count and partial-load hazards; documented without changing original or production source. Full-project restore is unsuitable unchanged for live kits. Safe kit-kind publication through existing authorities, overlay rebasing and format conversion are designed only.
- Reference O0 GCC and O2 VS18 LLVM each306037 PASS/0FAIL, identical tested C/C++/compat/original/fixture inputs. GCC O2 and VSLLVM O0 counterpart executables remain BLOCKED4551 before entry; no denied test relabeled PASS or trust/policy bypass. Earlier corrected harness failures remain in evidence. Host ABI tests do not certify target AVR/ARM object layout.
- Documentation:docs/p17a_preset_kit_architecture.md; source matrices/raw attempts/integrity/report/roadmap:out/p17a/. No production preset codec, kit application, browser, GUI, morph or sample-loading feature implemented. Next separately authorized phase:P17-B explicit kit contracts and raw-to-canonical route representability.
- P16 release Import GUI, timing/Euclidean/copy-clear, four historical PCM inequalities, exact-artifact qualifications and retained-event observer integration remain BLOCKED/open; all earlier evidence preserved.


### P17-B whole-kit data contracts - PASS (development); release BLOCKED - 2026-11-17

- Verified branch/local/live baseline02c4005c91118d369473511e0d5a4b41e30202be, eleven P17-A publication files/all468 source hashes,143 P17-A artifacts,757 F13 artifacts,235 production inputs, original firmware/iPlug2/SDK pins and protected89F6DF DLL/seven resources. Historical29 REAPER cache timestamp differences predate this task; every size/content hash matches F13, separately reconciled without editing evidence.
- Added standalone fixed-width RawKit/CanonicalKit/PatchCandidate, exact228-ID ownership and source-field mapping, six named synthesis owners and shared hi-hat decay60/61/note226/227. Raw/canonical236bytes each, native candidate428bytes. No compiler bitfield/object-image dependence or dynamic storage.
- Stored firmware initialization228zeros and BSS name0 match original C; no factory kit invented. Full-byte selectors0..204 and hint0 sentinel retained; strict semantic validation and separate runtime compatibility preserve unsupported source data. All107 reviewed own-voice native rows per source (39above127) convert without truncation. Cross-voice/global/mutable/unreviewed targets reject native candidates; reserved/future plugin identities refuse original export. Candidate deliberately projects inert selector bytes0 while retaining full typed routes and canonical provenance.
- Final GCC/VS18 LLVM at O0/O2 each817873PASS/0FAIL, zero counted conversion allocations. Independent unchanged C verifies228defaults/owners,26274field-value writer comparisons and2460reader/sender translations; exhaustive58368byte mutations,9840selector/hint cases, all reserved16-bit host codes, rollback/corrupt metadata and1000complete random kits pass. Initial compile-only test API mistake and earlier narrower results preserved; enabled-OFF validation classification corrected and all four accepted modes rerun.
- Fresh P17-A O0/O2 each306037/0;40 targeted unchanged-source original-path regression executions PASS, coveringP15-M2A..H,P16-B/C/D/E,transport/project codec,P12cstate/concurrency,nativeMIDI/maps. No VST3 build/install or full P16-F certification cycle. Existing DSP, Engine, authorities, HostTransport, host IDs and LXRSv3 unchanged; pure candidates have no live publication inlet.
- Accepted interface interfaces/KitContract.h; documentation docs/p17b_whole_kit_contract.md; complete matrices/results/memory/integrity/publication receipts out/p17b/. User authorizes two reviewed development commits (P17-A thenP17-B) and normal fast-forward push only after engineering gates; completion requires matching local/tracking/live receipts. No main merge/tag/release.
- P17-C bounded .snd codec is ready for separate authorization, including explicit provenance/export policy. No disk I/O, live kit loading, browsing, morph, factory content, GUI or samples. Legacy/targetABI/factory/SD/hardware timing unknowns deferred. P16-F Import,first-event/performance traces,four historical PCM inequalities,exact-artifact/observer release gates remain BLOCKED/open.


### P17-C original whole-kit .snd codec - PASS (development); release BLOCKED - 2026-11-17

- Verified clean branch/local/live baseline89c5eb61e89fa238b803aa94e83425cbce2f7c43, empty initial index/tree, both P17-A/B commits, all480 source hashes,234 P17-B/143 P17-A/757 F13 evidence hashes,235 production inputs, firmware/iPlug2/SDK pins and protected89F6DF DLL/seven resources. Existing P17-B reconciliation of29 pretask timestamp-only cache differences remains unchanged. No VST3 build/install.
- Source confirms exact .snd layout: arbitrary binary name8 at0..7 plus parameter_values[0..227] at8..235. There is no magic/version/count/checksum/padding/trailer or multibyte endian field; composite FILE_VERSION2 does not apply. Raw archival APIs preserve every exact236-byte record. Strict canonical APIs separately apply unchanged P17-B field and own-voice route admission without normalization or partial mutation.
- Added fixed-memory C++17 codecs and explicitly non-real-time Windows file helpers. Export validates and encodes all236 bytes before I/O, creates a bounded private sibling with no-overwrite semantics, flushes and publishes the opened object by handle; pre-publication failures discard only the owned handle. A post-rename close failure reports published=true. No Engine/authority/HostTransport/MIDI/FIR/host-ID/LXRSv3 inlet or mutation was added.
- Original selectors0..204, OFF0 and valid identities above127 round-trip exactly. The twelve typed-route export validates source/owner/order/metadata and reports the exact slot/field for unsupported, cross-voice or plugin-only identities;205/255/32768/65535 and unknown namespaces reject without truncation, wrapping, OFF substitution or partial output. Reserved and source-valid external-wave bytes remain archivally representable without claiming runtime support.
- Final current-source GCC O0/O2, VS18 LLVM O0/O2, ASan O0/O2 and UBSan O0/O2 each594156PASS/0FAIL with zero counted pure-code allocations. Each performs62327 byte-exact unchanged-original-C comparisons,66351 exact round trips and13000 deterministic property/fuzz cases; zero byte mismatches or sanitizer findings. Initial sanitizer CRT link recipe failures stopped before execution and remain separately recorded; corrected consistent recipes pass without suppression or policy bypass.
- Six synthetic source-valid/boundary golden fixtures are explicitly labelled test data, not factory kits. Native/fault-injected file tests cover exact sizes, short/error paths, existing/concurrent destination protection, cleanup and Unicode paths. Fresh hash-verified execution of40 targeted P15/P12c/P16 regressions plus P17-A GCC O0/O2 and P17-B GCC/LLVM O0/O2 passes. Complete evidence and reproduction receipts:out/p17c/; tracked contract:docs/p17c_snd_file_codec.md.
- Development publication is authorized only after explicit staging/full staged-diff/blob review, tested-source identity, protected-install recheck and normal fast-forward push. No main merge/tag/force push. P17-D live publication is ready for separate design but not implemented; browsing, morph, factory content, GUI and user samples remain absent. P16-F Import/timing/performance/raw-PCM/exact-artifact/observer release gates remain BLOCKED/open.
### P17-D transactional runtime whole-kit loading — PASS — 2026-11-17

- Verified clean branch/local/tracking baseline `8a99b26edd356680912dca66de00f99af6b52584`, origin `vlndv/lxr-vst3`, empty index, P17-A/B/C sources/evidence, firmware/iPlug2/SDK pins and protected DLL/seven resources before edits. No reset, clean, rebase, stash or branch change.
- Added one bounded canonical-kit transaction using the existing routing outer gate and patch authority. It owns name provenance, all228 patch bytes and twelve typed routes only; the routing release generation is the cross-authority commit point. Busy/Stale/invalid operations leave patch, routes, bank, transport, bypass and controller state unchanged.
- Processing applies accepted kits before the next32-frame native producer refill without Engine/sequencer/FIR reset. Active voice/envelope/filter/LFO/MIDI state survives. Fourteen lock nodes retain value/application chronology so active and duplicate locks rebase to the newest kit; later host/MIDI bases remain authoritative and release cannot restore pre-kit bytes.
- Dedicated GCC O0/O2 and VS LLVM O0/O2 suites each pass1176/0 with0 publication allocations, coherent concurrent snapshots, rollback/isolation and exact deterministic native PCM, including MIDI-active-voice and sequencer-playback continuity across kit application. Eight audible variations differ at recorded frames; matched rerender is exact. P16-D passes246/0 each. P17-A/B/C pass306037/817873/594156 per optimization; P15 and44-execution native regressions pass, including original-C filter80000 exact blocks each.
- Isolated Release x64/v145 private VST3 SHA `658272C716FDB1A7C42848B9F073888118F825D4F61C9730FBA3D7F8C2DDFC5F`,1255424 bytes, built with pre/post deployment disabled. Exact state/runtime/controller/M2G/M2H/transport/F2/certification/mute/probe322/1143/955/91/689/105/3300/260/216/1057 PASS; validator47/47. Exact processing probe8252/0 observes0 allocations and19744-byte maximum processing stack watermark. Protected `89F6DF...FF82` DLL/seven resources unchanged; no install.
- Engineering PASS. Development publication is conditional on explicit staged-diff/blob review, tested production-source identity, protected-install recheck and normal fast-forward push. No browser, `.snd` UI, factory kit, morph, graphics or sample loading. P17-E is ready for a separately scoped user-facing import action. P16-F release certification remains BLOCKED/open and is not transferred by this phase. Report `docs/p17d_runtime_kit_loading.md`; evidence `out/p17d/`.
### P17-E user-facing original `.snd` kit management — development PASS; GUI gate BLOCKED — 2026-11-17

- Added one production import/export operation layer used by both the actual iPlug2 editor buttons and an instance-local exact-DLL VST3 verification interface. Import uses P17-C decode and P17-D coherent publication; export uses an authoritative patch/route snapshot and P17-C no-replace publication. Binary name8 bytes round-trip exactly; display escapes nonprintable bytes. The fixed 51,065-byte LXRS v3 musical state remains unchanged and does not persist the runtime-only name label.
- P17-E GCC and VS LLVM O0/O2 each pass 1,052 assertions with zero failures/zero counted publication allocations and identical deterministic native hashes (`182752A305684AC0`, `E78F22935AC0FE6B`). P17-A/B/C/D current-source regressions pass 306,037/817,873/594,156/1,176 assertions per optimization; P15 modulation, P16-D locks, the 44-row native checkpoint, and original filter/snare/FIR/MIDI/state/concurrency suites pass.
- Isolated VS Community 18/MSBuild 18.10.1 Release x64/v145 build used disabled pre/post build events. Private 1,294,848-byte SHA-256 `9C86D4ABECB4B200A86646608EED9809EB2B3E9342988978372310057D35DFAF`; exact-DLL P17-E 191/0, host state/runtime/controller/modulation/transport/persistence/MIDI/probe suites and Steinberg validator 47/47 pass. All 228 controller values, twelve routes, 236-byte exact export, rollback, nondefault pattern/transport preservation, 51,065-byte restore, post-import automation, two live instances, distinct PCM, and 26-sample PDC pass.
- Protected installed DLL remains SHA-256 `89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`; seven resources are unchanged. REAPER 7.80 graphical interaction is separately BLOCKED: the isolated task process launched, but authorized Computer Use app selection timed out before exposing the window. Zero clicks/keys were sent and no pre-existing REAPER process was touched. This is not release certification; existing P16-F release blockers remain open.
- Report: `docs/p17e_kit_management.md`; evidence: `out/p17e/`.

### P17-F kit identity persistence and host presets — development PASS; GUI gate BLOCKED — 2026-11-18

- Added a known optional LXRS v3 tag-4 module containing exactly eight raw kit-name bytes. Historical three-module v3 remains exactly 51,065 bytes and byte-stable; identity-bearing VST3 state is 51,089 bytes; maximum remains 65,536. V1/v2/legacy and historical v3 migrate to an absent/all-zero default. Explicit all-zero imported identity remains distinguishable by fixed provenance.
- Identity/provenance now live in `RoutingStateAuthority` and publish under the existing patch/routes outer gate and generation. Whole-kit import, complete restore, project save/reopen and host preset restore are coherent; automation does not rename; Busy/stale/corrupt operations preserve prior state. Export continues using the coordinated current name without conversion or truncation.
- P17-F GCC and VS LLVM O0/O2 each pass 1,082/0 with zero counted publication allocations and identical native hashes (`20667AF077E68561` identity-only, `D4CB9DDB17B4D6B0` changed volume). P17-A/B/C pass 306,037/817,873/594,156 per optimization; P17-D/E pass 1,176/1,052 under both toolchains; all 44 native O0/O2 checkpoint rows pass.
- Isolated VS Community 18/MSBuild 18.10.1 Release x64/v145 private DLL is 1,295,872 bytes, SHA-256 `F9F2C0E49BD19D9C5CFE8249E458A3758E82BBA8D2DC34388F2EBBB58339B174`, with deployment events disabled. Exact-DLL P17-F standard SDK VST3 preset/state test passes 157/0; P17-E 191/0, existing host suites, ControllerSyncProbe 1057/0, 26-sample PDC, and validator 47/47 pass.
- Authorized Windows app discovery exposed 26 unrelated existing REAPER 7.80 windows and no uniquely identifiable P17-F private-candidate session. Direct GUI acceptance is separately BLOCKED with zero clicks, keys, activation or launch. Protected installed DLL `89F6DF...FF82` and seven resources remain unchanged. No rename UI or custom preset browser was added. Report: `docs/p17f_kit_identity_and_host_presets.md`; evidence: `out/p17f/`. P16-F release certification remains BLOCKED/open.

### P18-A firmware-faithful morph architecture — PASS (audit only) — 2026-11-18

- Pinned v0.37 implements a reachable sound-preset morph: live source `parameter_values[0..227]`, target `parameters2[0..227]`, and performance amount0..255. It is a sequential parameter transformation, not an audio crossfade, pattern morph or timed transition. Manual `MorphSnd` load/save handlers match the source call graph.
- Original arithmetic is `a+(b-a)*x/256`, rounded upward for remainders>=127. Amount0 is exact A; amount255 can miss B (`0->255` gives254, `255->0` gives1). Discrete values use the same numeric rule. All228 bytes are swept, but eighteen velocity/LFO selector/hint bytes remain source-anchored and are explicitly preserved by MorphSnd save.
- Original application emits228 three-byte UART messages at500kbaud (13.68ms wire lower bound) and is not atomic. It preserves active voice histories, LFO/envelope/filter phases, patterns and transport. SOM is separate and SOURCE_CONFIRMED_UNREACHABLE in this build (`USE_DRUM_MAP_GENERATOR=0`, no active-flag setter).
- Standalone unchanged-original-C GCC O0/O2 and VS LLVM O0/O2 each pass16,778,153 assertions, including all16,777,216 interpolation triples,228-message order and exact236-byte save policy. Earlier test-assumption failures are preserved separately. No production source, plugin state format, dependency, firmware or VST3 binary was modified/built/installed.
- Proposed P18-B uses authoritative A plus fixed validated B and a derived preallocated projection committed coherently at one native32-frame boundary before sequencer locks, then velocity/LFO. This is an intentional safe VST3 timing adaptation. Morph persistence is transient by default; an optional256-byte module design is only a future authorization gate.
- Report `docs/p18a_morphing_architecture.md`; evidence `out/p18a/`. HEAD remains `b723036ad4027746d69b0aa5b9779e464cd17c4b`; no staging/commit/push is authorized. P16-F release certification remains BLOCKED/open.
