# risks.md — LXR VST3 port risks

Status tags: OPEN = decision needed, DECIDED = settled, UNVERIFIED = claim from reading code, not tested.
Baseline: SonicPotions/LXR @ dee4968 (fw 0.37). Not legal advice.

## 1. Licensing
| # | risk | status |
|---|---|---|
| L1 | Source licence is custom non-commercial (LICENSE.txt), **not GPL**. No sale or commercial use. Modified redistribution must include full source. Copyright notice must be kept. | DECIDED (fact) |
| L2 | Applies to data assets too (wavetables, crash sample, transient samples, note table). Exporting them to binary/CSV does not remove the licence. | DECIDED (fact) |
| L3 | AGPL JUCE cannot be combined with code carrying a non-commercial restriction. JUCE's commercial licence would itself be commercial use. Options: (a) another framework with a permissive licence (e.g. DPF, iPlug2), (b) JUCE with the non-commercial constraint reviewed by a lawyer. | OPEN |
| L4 | Plugin must ship free, with full source and the original notice. Do not put it on any paid store or bundle. | DECIDED (consequence of L1) |
| L5 | Clean-room rewrite (specs only, no copied code) may avoid the licence, but the goal is a faithful port, so treat all derived code as covered. | OPEN |
| L6 | "LXR" / "Sonic Potions" names and the LXR-01 panel design: trademark/design status not checked. Front panel copy without logo is your plan, not verified as safe. | OPEN |

## 2. Fidelity
| # | risk | status |
|---|---|---|
| F1 | Engine runs at 44002.757 Hz in 32-sample blocks; envelopes, LFOs, filter coefficients and pitch update once per block (~1375 Hz). Host rates differ. Option A: run engine at 44002.757 Hz and resample (faithful, adds resampler cost/latency). Option B: rescale per-tick constants to host rate (cheaper, timing and stair-stepping differ). Decimator and LFO rates are relative to engine rate. | OPEN |
| F2 | Audio path is int16 with saturating adds. Keep int16 clip points or use float with explicit clamps at the same points. Mixed choice changes distortion character. | OPEN |
| F3 | Filter must be copied from `SVF_calcBlockZDF` verbatim (Padé fastTan, softClipTwo, tanhXdX). A textbook SVF will sound different. | DECIDED |
| F4 | FM negative-value cast: `(uint32_t)` of a negative float. Cortex-M4 likely saturates to 0 (half-wave FM); x86 wraps. Need disassembly or hardware recording. | UNVERIFIED |
| F5 | Wavetable table index for f < 440 Hz likely wraps to table 4 via uint8 overflow in `fast_log2`. | UNVERIFIED |
| F6 | Wavetable non-FM path has effectively no interpolation (fraction from table index). Reads index 1024 (one past table) at wrap. Fixing changes aliasing/tone. | OPEN (keep vs fix) |
| F7 | Crash sample interpolation uses `index & 20000` (decimal). | OPEN (keep vs fix) |
| F8 | `PITCH_SLOPE` = 127 divides by zero (inf/NaN). Clamp in port, or reproduce? Hardware behaviour unknown. | OPEN |
| F9 | Amp EG is interpolated per block on D1-D3 but stepped per block on SN/CY/HH. Keep. | DECIDED (keep) |
| F10 | `RNG`: hardware RNG in original. Any white PRNG is fine but noise will not be bit-identical. | DECIDED |
| F11 | Original has no bit-depth reduction, no reverb, delay, EQ or limiter. Do not add them and call it "faithful". If added later, mark as extensions. | DECIDED |
| F12 | Verification method: none yet. No reference audio from a real LXR. Recommend recording test patches (single hits, sweeps) from the hardware for A/B comparison. | OPEN |

## 3. Scope and MIDI compatibility
| # | risk | status |
|---|---|---|
| S1 | Sequencer, patterns, kits, presets, SysEx, sync, and the AVR front-panel logic are **not reviewed**. Do not write prompts for them from `architecture.md`. | OPEN |
| S2 | `VEL_DEST`, `VOICE_LFO`, `TARGET_LFO` are not settable over MIDI in the original. The plugin needs its own extension (e.g. extra NRPN range) or host automation only. Mark as non-original. | OPEN |
| S3 | Kit/preset defaults are not extracted, so initial sound (and engine init values that differ from a real startup kit) is unknown. | OPEN |
| S4 | SD-card user samples (wave >= 6) are out of scope. Front-panel wave menu will show fewer waves than a fully loaded hardware unit. | DECIDED |
| S5 | Two DACs / 6 routing modes map to plugin outputs; jack-detect fallback logic dropped. Decide number of output buses. | OPEN |
| S6 | 7 MIDI channels + global channel, note-override logic: host routing in VST3 differs from hardware. Decide how channels map (per-track channel vs per-bus). | OPEN |

## 4. Real-time safety and CPU
| # | risk | status |
|---|---|---|
| R1 | No allocation, locks, file I/O or logging in the audio thread. Load wavetables/samples at init into preallocated arrays. | DECIDED |
| R2 | Original mutates engine values in place for modulation. Port with a per-parameter `base` + `modMultiplier` pair evaluated once per tick, so parameters stay reproducible and thread-safe. | DECIDED |
| R3 | Host automation and UI changes must reach the DSP through atomics or a lock-free queue, not shared mutable structs. | DECIDED |
| R4 | Guard NaN/inf at parameter mapping (pitch slope, reso, EG slopes) and denormals in filter states (flush-to-zero or small DC offset). | OPEN (add to each DSP prompt) |
| R5 | CPU: 6 voices, one nonlinear SVF each, block rate 32 samples. **Not measured.** The original ran on a Cortex-M4, so the workload is small, but Option A of F1 (resampling) and oversampling for distortion would add cost. Measure before deciding. | UNVERIFIED |
| R6 | Aliasing: original oscillators, decimator and distortion alias by design. Oversampling would change the sound. Keep off by default. | DECIDED |

## 5. Workflow (local Qwen 2.5)
| # | risk | status |
|---|---|---|
| W1 | A local model may paraphrase or "improve" DSP code. For filter, EG and oscillator prompts, paste the original C verbatim and require a line-by-line port with named differences. | DECIDED |
| W2 | Each prompt has to be standalone, but the model will not remember earlier files. Always include the relevant `interfaces/*.h` and the specific rows of `param_map.md`. | DECIDED |
| W3 | Every DSP prompt needs a numeric acceptance test (e.g. impulse response, EG time at v=64 = ~1.5 s, filter coefficients for fixed inputs) produced from the original C, not from the model's own output. | OPEN |
| W4 | Context size: `param_map.md` (~22 KB) will not fit in every prompt. Split by voice or by scale tag per prompt. | OPEN |

## 6. Top three to resolve first
1. L3: framework/licence route (JUCE vs alternative).
2. F1: engine rate strategy.
3. F12/W3: how fidelity will be verified (hardware recordings and reference vectors from the original C).
