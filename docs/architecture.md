# architecture.md — LXR firmware 0.37 voice architecture

Baseline: `github.com/SonicPotions/LXR` @ `dee4968` ("firmware image 0.37"). All facts below verified in that tree
(`mainboard/LxrStm32/src/`). `brendanclarke/LXR` is Catalyst v1.02 and differs in 23 of ~45 DSPAudio files
(Oscillator.c, lfo.c, DrumVoice.c, SlopeEg2.c, mixer.c ...). ResonantFilter.c and distortion.c are identical. Do not use it as reference.

Licence: custom non-commercial (LICENSE.txt). No sale or commercial use; modified redistribution must include full source;
keep copyright notice. Not GPL. Incompatible with AGPL JUCE (JUCE commercial licence would itself be commercial use).

## 1. Timing model (config.h, mixer.c)
- Real sample rate `REAL_FS = 44002.7573529412` Hz. Block = `OUTPUT_DMA_SIZE = 32` samples.
- Once per block ("async" step, ~1375.09 Hz, 0.727 ms): LFOs, modulation, filter coefficients, all envelopes, oscillator phase increments.
- Then per-sample ("sync" step): oscillators, filter, transient, gain, distortion.
- Envelope times are per-tick decrements (e.g. decay v=64 -> step 0.000492 -> ~1.5 s full ramp; v=1 -> ~12 ms; v=127 -> infinite).
- Drum voices D1-D3 interpolate the amp EG linearly across the block (`bufferTool_addGainInterpolated`); SN/CY/HH apply the amp EG as a per-block constant (steps).
- PORT DECISION (open): run engine at 44002.757 Hz with 32-sample blocks and resample to host rate, or rescale all per-tick constants to host rate. Decimator and LFO rates are defined relative to engine rate.
- Audio path is int16 buffers with float math inside; saturating adds (`__QADD16`). Port: keep int16 semantics at the same clip points, or use float with explicit clamps at those points.

## 2. Voices
6 sound voices. 7 sequencer tracks (hi-hat closed and open both trigger the hat voice, `isOpen` picks `decayOpen`/`decayClosed`).
`MidiVoiceControl.c`: voice 0-2 -> `Drum_trigger`, 3 -> `Snare_trigger`, 4 -> `Cymbal_trigger`, 5-6 -> `HiHat_trigger(vel, voice-5)`.

### D1-D3 drum voice (DrumVoice.c)
Trigger: LFO retrigger; velocity modulator update; phase reset (always, the guard is compiled out): sine start = 1023<<20 (`sine_table` is a -cos shape: index 0 = -32767, 1024 ~ 0, 2048 = +32766, so this starts at the rising zero crossing),
tri/saw/rec start = 0xff<<20, others 0; transient "offset" mode (wave 1) scales start phase by (1 - transVol). Base note set, pitch EG = 1, amp EG start, transient phase 0, snap EG = 1, filter state reset.
Per tick: pitchMod = 1 + pitchEG x pitchAmt; if transient wave 0 (snap): pitchMod += snapEG x transVol. osc.fmMod = fmAmount x pitchEG value.
Per sample block:
1. modOsc block, gain = fmAmount.
2. If mixOscs (default): osc block gain (1 - fmAmount), saturating add modOsc. Else: osc phase-modulated by modOsc buffer (fmMod scaled, gain 1.0).
3. + transient buffer (saturating).
4. Filter (SVF, type per voice).
5. x amp EG (interpolated), x velocity (if volumeMod), distortion, x channel volume.
Then decimator, pan, route.

### Snare (Snare.c)
noise osc (S&H white noise, rate = noiseFreq Hz) x0.9 -> filter -> + transient. osc (tonal, own pitch EG + snap) x (1-mix). Sum: noise x mix + osc, x velo x vol x ampEG, distortion.
Filter is on the noise path only. Amp EG has `repeat` (repeat count).

### Cymbal (CymbalVoice.c)
modOsc (default sine) x fm1 + modOsc2 (default noise) x fm2 -> saturating sum -> phase-modulates main osc (fmMod fixed 1, gain 1.0). -> filter -> + transient -> x velo x vol x EG -> distortion. No pitch EG. Amp EG has `repeat`. Wave 5 = crash sample.

### Hi-hat (HiHat.c)
Same as cymbal, main osc gain 0.5, amp decay picked per trigger (closed/open). No repeat. No pitch EG.

## 3. Oscillators (Oscillator.c, wavetable.h)
- Phase: uint32. Sine table 4096+1 entries (index phase>>20). Tri/saw/rec: `[11][1024]` int16 tables, one per octave (note 0,12,...,120), index phase>>22, table index = `freqToTableIndex(f)` = (12 x integer-log2(f/440) + 70)/12, clamped to 10 (see quirk 10).
- Phase increment = table size x f / REAL_FS x 2^(32 - log2 size). Computed per tick, not per sample.
- FM: index = (uint32)(mod sample x fmMod) << 17 (sine) or << 19 (wavetable) added to phase.
- Noise: new `GetRngValue()` (hardware RNG, low 16 bits) when phase wraps; held between. Replace RNG with any white PRNG.
- Crash: 8-bit unsigned sample `crashSample[32768]`, out = (s - 127) x 256, phase inc uses <<17.
- Pitch = MidiNoteFrequencies[clamp(coarse + baseNote - 63)] x fineDetune x pitchMod x modNodeValue.
- User SD samples (wave >= 6): out of scope; treat as unsupported.

## 4. Filter (ResonantFilter.c)
Nonlinear ZDF SVF (Zavalishin fig 3.11 style), one per voice.
- input = softClipTwo(x/32767 x drive), softClipTwo(x) = x x tanhXdX(0.5x), tanhXdX = Pade ((a+105)a+945)/((15a+420)a+945), a = x^2.
- g = fastTan(pi f), fastTan(x) = (-15x + x^3)/(3(-5 + 2x^2)). R = q (forced to 1 if f >= 0.4499). Nonlinear integrator gains t0, t1 via tanhXdX; state update uses softClipTwo(s1).
- Outputs: LP = fastTanh(y1) x 0x7fff; HP, BP, unity-BP (2R y0), notch, peak (y1 - h) x FILTER_GAIN 0x70ff, saturated to int16.
- Type 7 (LP2, added 0.35): naive 2-pole with clamped states (+-1), q = (1-q) x 1.4 + (1-q)/(1 - 2.21 f), out x 0x70ff.
- Type 8 ("off"): switch default returns, buffer passes unfiltered.
- Exact code needed for a faithful port: copy `SVF_calcBlockZDF` verbatim (see file), do not substitute a textbook SVF.

## 5. Envelopes
- `SlopeEg2` (amp): states STOPPED/A/D/REPEAT. Attack: value += attack; output = warp(value, invSlope). Decay: value -= decay; output = warp(value, slope). Repeat mode: value ramps down by `attack` per tick, repeatCnt times, then decay. warp(x,s) = (1+s)x/(1+s abs(x)).
- `DecayEg` (pitch): value = 1 on trigger, -= decay per tick to 0; output = warp(value, slope).
- `SnapEg`: on transient wave 0. value = 1; each tick output = value^2 x 24, value -= 0.2 x transient pitch factor.

## 6. Transient generator (transientGenerator.c)
Wave 0 snap, 1 offset, 2..13 = 12 int8 samples (2205 samples each = 50 ms at 44.1 kHz) `transientData[12][2205]`, out = vol x (s<<8), phase += pitch x 2^20 per sample, stops at 2205. `transientVolumeTable` is only used by unused per-sample code.

## 7. LFO (lfo.c) and modulation (modulationNode.c)
- One LFO per voice, ticked once per block. Phase uint32; waveforms sine, tri, saw up, saw down, square, S&H random, exp up (x^3), exp down. Output 0..1 (unipolar). Rate `((v+1)/128)^3 x 200` Hz or tempo-synced (bar rate = bpm/60/4 x scaler).
- Retrigger: LFO with `retrigger == n` resets phase to `phaseOffset` when voice n-1 triggers.
- Destination = one parameter (`parameterArray[PAR]` -> pointer + type). Each tick: `modNode_resetTargets` restores every modulated pointer to its stored original value; then each modulator applies `target = target x (amount x mod + (1-amount) x 1)` on the live engine value (uint8 targets are re-truncated).
- Velocity: 6 modulators, same mechanism, mod = velocity/127, updated on trigger.
- Special targets (`modNodeValue` on osc/lfo): multiplier, original = 1.
- PORT NOTE: this design mutates engine values in place. In C++ use a per-parameter "base value + modulation multiplier" pair, evaluated once per tick, to keep it real-time safe and reproducible.

## 8. Mixer, pan, decimator, routing (mixer.c)
Order per block: LFOs -> filter coefficients -> per-voice async -> per-voice sync -> decimate -> pan -> route+sum.
- Decimator per voice: counter += rate x rate_ALL; when >= 1 take a new sample, else hold. Sample-rate reduction only, no bit reduction.
- Pan: L = sqrt(127-pan)/sqrt(127), R = sqrt(pan)/sqrt(127) (LUT). Applied for stereo routes only.
- Routing per voice: St1, St2, L1, R1, L2, R2 (two hardware DACs). Jack-detect fallback logic does not apply to a plugin. Plugin: 2 stereo outputs (or more), summed with saturating int16 adds.
- No master EQ/limiter/reverb/delay in 0.37.

## 9. Fidelity quirks (decide: keep or fix)
| # | where | behaviour |
|---|---|---|
| 1 | Oscillator.c `calcWavetableOsc*` | fraction computed from table index, not phase -> effectively no interpolation; reads index 1024 (past table) at wrap |
| 2 | Oscillator.c crash sample | fraction uses `index & 20000` (decimal) instead of 0x20000 |
| 3 | Oscillator.c FM | `(uint32_t)` cast of negative float. On Cortex-M4 likely saturates to 0 (half-wave FM); on x86 wraps. UNVERIFIED |
| 4 | Decay.c `DecayEg_setSlope` | amount = (v/127 - 0.5) x 2; v = 127 divides by zero -> inf/NaN |
| 5 | DrumVoice.c | pitch, amp, filter update once per 32-sample block (stepped) |
| 6 | Snare/Cymbal/HiHat | amp EG per-block constant (no interpolation) |
| 7 | Cymbal/HiHat | `osc.pitchMod` only updated when transient wave = 0 |
| 8 | ResonantFilter.c | filter type 8 ("off") passes audio unfiltered; header comment on bit meanings is stale |
| 10 | Oscillator.c `freqToTableIndex` | `fast_log2` returns uint8 of `31-CLZ(0)` = 255 when f < 440 Hz, so index wraps to 4 (by reading the code; UNVERIFIED). Integer log2 also means table changes only per octave from 440 Hz |
| 9 | MidiParser.c | OSC3_DIST branch under disabled `USE_FILTER_DRIVE` writes voiceArray[3] (dead code) |

## 10. Not yet reviewed
Sequencer (patterns, steps, probability, roll, Euclid, SOM generator, morph, automation, shuffle), pattern/kit/preset storage and default kit, SysEx, trigger I/O, MTC/clock sync, AVR front-panel layout (pages, encoders, buttons, LEDs).
Do not assume anything about these from this file.

## 11. Data assets to export (not text, keep as binary/CSV)
`sine_table[4097]`, `sawTable/triTable/recTable[11][1024]`, `crashSample[32768]`, `transientData[12][2205]`, `transientVolumeTable[69]` (unused), `MidiNoteFrequencies[128]`, `squareRootLut[128]`. Located in `DSPAudio/{wavetable,Samples,transientTables,squareRootLut}.c` and `MIDI/MidiNoteNumbers.h`. Licence applies to them too.
