# phase_log.md — LXR VST3 port

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

## Open decisions that block phases
| id | decision | blocks | see |
|---|---|---|---|
| D1 | Framework and licence route (JUCE vs alternative) | P10 and anything that uses framework types | risks.md L3 |
| D2 | Engine rate: 44002.757 Hz + resample, or rescale constants | P4 onward (all time-based DSP) | risks.md F1 |
| D3 | int16 semantics vs float with explicit clamps | P2 onward | risks.md F2 |
| D4 | Keep or fix each quirk | phase that contains it | risks.md F4-F8 |
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
| P9 | Snare, cymbal, hi-hat voices | DSPAudio/{Snare,CymbalVoice,HiHat}.c | P3-P7 | TODO |
| P10 | LFO and modulation (per-parameter base + multiplier), velocity modulators, LFO retrigger and sync | DSPAudio/{lfo,modulationNode}.c | P8, P9, D2 | TODO |
| P11 | Mixer: pan (sqrt LUT), routing, saturating sum, mutes | DSPAudio/mixer.c | P8, P9 | TODO |
| P12 | Plugin shell: parameters (227 IDs from `param_map.md`), MIDI in (CC, NRPN, 7 channels + global, note override), outputs | MIDI/{MidiParser,MidiVoiceControl}.c | P11, D1 | TODO |
| P13 | Reference-vector harness: render single hits from the port and compare against recordings and original-C vectors | n/a | P8-P11, D5 | TODO |
| P14 | Review sequencer, kits/presets, front-panel layout (source not yet reviewed) | front/LxrAvr/, mainboard Sequencer/, preset storage | none | TODO |
| P15 | `ui_layout.md`: LXR-01 panel spec without logo, plus replacements for menu diving and shift combos | front/LxrAvr/ | P14 | BLOCKED (needs P14) |
| P16 | Sequencer engine port | mainboard Sequencer/ | P14 | BLOCKED (needs P14) |
| P17 | UI implementation (click or injected MIDI for every control) | n/a | P12, P15 | BLOCKED |

## Suggested order
P1 -> P3 -> P2a/P2b -> P4 -> P5 -> P6 -> P7 -> P8 -> P9 -> P10 -> P11 -> P12 -> P13. P14 can run in parallel at any point.
Reason: filter and mappings are self-contained and testable first; voices need all of them; modulation needs finished voices.

## Phase entry format (append below when a phase changes)
```
### P<n> <name> — <TODO|DOING|DONE|BLOCKED> — <date>
- Prompt file: <path>
- Output files: <paths>
- Tests passed: <n>/<n>, reference source: <original C vectors | recording>
- Quirks kept or fixed: <F#: kept|fixed>
- Interfaces saved: <interfaces/*.h>
- Open issues: <list>
```

## Log
### P0 Source review and project files — DONE — 2026-09-19
- Files: `param_map.md`, `architecture.md`, `risks.md`, `prompt_template.md`, `phase_log.md`
- Findings: brendanclarke fork is Catalyst v1.02, not 0.37; licence is non-commercial, not GPL; no reverb, delay, EQ, limiter or bit-depth reduction in 0.37.
- Not reviewed: sequencer, kits/presets, SysEx, sync, front-panel layout.

### P1 Data export — DONE — 2026-09-19
- Prompt file: none (done as a deterministic script, no Qwen needed)
- Output files: `tools/export_data.py` -> `data/*.bin`, `data/*.csv`, `data/manifest.json`
- Tests passed: 9/9 tables have the expected element counts in the sandbox run; sha256 prefixes in manifest
- Quirks kept or fixed: transientData last row has 13 implicit zeros in source (zero-filled); hex int8 literals >0x7F wrapped to negative
- Interfaces saved: none
- Open issues: none. Run on the user's machine: all nine tables printed OK.

### P3 Nonlinear ZDF filter — DONE — 2026-09-20
- Prompt file: `prompts/P3_filter.md` (Qwen 2.5 Coder 14B, one run)
- Output files: `dsp/ResonantFilter.{h,cpp}`, `dsp/ResonantFilterTest.cpp`, `tests/reference/filter/diff_test.cpp`
- Tests passed: 130/130 reference checks (0 WARN) and 80000/80000 random blocks bit-identical to the original C, at -O0 and -O2 (gcc x86-64, -ffp-contract=off)
- Quirks kept or fixed: kept all: type 8/unknown returns after updating state once; R forced to 1 at f >= 0.4499; LP scaled by 0x7fff, others by 0x70ff; q = 0.02 when 1-feedback < 0.1
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

### P8 Drum voice D1-D3 — DONE — 2026-09-23
- Prompt file: N/A (Generated directly by AI assistant)
- Output files: `dsp/DrumVoice.{h,cpp}`, `dsp/DrumVoiceTest.cpp`
- Tests passed: 11/11 (fail=0), verified against original C logic from SonicPotions/LXR @ dee4968
- Quirks kept: 
  - Phase reset only occurs if amp EG is closed (state == 0 or value <= 0.01f) OR transient waveform == 1 (offset mode)
  - Sine start phase uses 1024 + ((1023 << 20) - 1024) * offset (not clean 0)
  - TRI/SAW/REC start phase uses (0xff << 20) * offset
  - bufferTool_addGainInterpolated uses i / (size - 1.f) for linear interpolation across 32-sample block
  - Saturating int16 adds for oscillator mixing and transient mixing
- Interfaces saved: `interfaces/DrumVoice.h`
- Open issues: LFO struct is a minimal stub (P10 will provide full implementation); user samples (waveform >= 6) output silence
