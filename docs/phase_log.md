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
| P1 | Export data assets to binary/CSV: sine table, saw/tri/rec `[11][1024]`, crash sample, transient data `[12][2205]`, note frequencies, sqrt LUT | DSPAudio/{wavetable,Samples,transientTables,squareRootLut}.c, MIDI/MidiNoteNumbers.h | none | TODO |
| P2 | Parameter value mappings (all scale tags in `param_map.md`) with unit tests | MIDI/MidiParser.c, DSPAudio/{SlopeEg2,Decay,lfo,distortion}.c, ResonantFilter.c | P1 (constants) | TODO |
| P3 | Nonlinear ZDF filter, all 8 types incl. LP2 and passthrough | DSPAudio/ResonantFilter.c | D3 | TODO |
| P4 | Oscillators: sine, wavetables, noise (S&H), crash sample, FM phase modulation | DSPAudio/Oscillator.c | P1, D2, D4 | TODO |
| P5 | Envelopes: amp (attack, decay, slope, repeat), pitch decay, snap EG | DSPAudio/{SlopeEg2,Decay,snapEg}.c | P2, D2 | TODO |
| P6 | Transient generator | DSPAudio/transientGenerator.c | P1, P5 | TODO |
| P7 | Distortion and per-voice decimator | DSPAudio/distortion.c, mixer.c | P2 | TODO |
| P8 | Drum voice D1-D3 (mix and FM modes, pitch EG, transient, filter, amp, distortion) | DSPAudio/DrumVoice.c | P3-P7 | TODO |
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
P1 -> P3 -> P2 -> P4 -> P5 -> P6 -> P7 -> P8 -> P9 -> P10 -> P11 -> P12 -> P13. P14 can run in parallel at any point.
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
