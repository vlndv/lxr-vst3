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
