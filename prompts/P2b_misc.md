GOAL
Port the remaining 0..127 to engine-value mappings of the LXR drum synth to C++17: filter cutoff shaping, decimation rate, distortion shape, snare noise frequency, transient pitch and waveform, LFO frequency and phase offset, filter type, and oscillator pitch (coarse + fine tune). Each is a pure function returning what the original stores in its engine struct.

INPUTS
1. Original C (verbatim except that comments were removed, tabs replaced by spaces, and handler lines shown without their switch/case wrappers):
```c
// MidiParser.c, handlers (msg.data2 is the uint8_t 0..127 value):
snareVoice.noiseOsc.freq = msg.data2/127.f*22000;
voiceArray[0].transGen.pitch = 1.f + ((msg.data2/33.9f)-0.75f);
voiceArray[msg.data1-CC2_OFFSET_LFO1].lfo.phaseOffset = msg.data2/127.f * 0xffffffff;
snareVoice.filterType = msg.data2 + 1;
mixer_decimation_rate[msg.data1-VOICE_DECIMATION1] = valueShaperI2F(msg.data2,-0.7f);
const float f = msg.data2/127.f;
SVF_directSetFilterValue(&voiceArray[msg.data1-FILTER_FREQ_DRUM1].filter,valueShaperF2F(f,FILTER_SHAPER) );
voiceArray[0].vol = msg.data2/127.f;
#define FILTER_SHAPER -0.9f

// valueShaper.h
static inline float valueShaperI2F(uint8_t data2, float shape)
{
  const float k = 2*shape/(1.0001f-shape);
  const float val = (data2)/127.f;
  return  ((1+k)*val/(1+k*fabsf(val)));
}
static inline float valueShaperF2F(float val, float shape)
{
  const float k = 2*shape/(1.0001f-shape);
  return  ((1+k)*val/(1+k*fabsf(val)));
}

// distortion.c
__inline void setDistortionShape(Distortion *dist, uint8_t shape)
{
  dist->shape = 2*(shape/128.f)/(1-(shape/128.f));
}

// lfo.h / lfo.c (lfo_calcPhaseInc and the tempo sync are out of scope; port only the freq part)
#define LFO_MAX_F 200 //[Hz]
void lfo_setFreq(Lfo *lfo, float f)
{
  f += 1;
  f = f/128.f;
  f = f*f*f;
  lfo->freq = f*LFO_MAX_F;
  lfo->phaseInc = lfo_calcPhaseInc(lfo->freq,lfo->sync);
}

// transientTables.h / transientGenerator.c
#define NUM_TRANSIENTS 12
void transient_setWaveform(TransientGenerator* transient, const uint8_t waveform)
{
  if(waveform < NUM_TRANSIENTS + 2)
    transient->waveform = waveform;
  else
    transient->waveform = 0;
}

// MidiParser.c / sequencer.h / Oscillator.c
#define SEMITONE_UP 1.0594630943592952645618252949463f
#define SEQ_DEFAULT_NOTE 63
float midiParser_calcDetune(uint8_t value)
{
  float frac = (value/127.f -0.5f);
  float cent = 1;
  if(cent>=0)
  {
    cent += frac*(SEMITONE_UP - 1);
  }
  else
  {
    cent += frac*(SEMITONE_UP - 1);
  }
  return cent;
}

void osc_recalcFreq(OscInfo* osc)
 {
   const float cent = midiParser_calcDetune(osc->midiFreq&0xff);
   int16_t note =  (osc->midiFreq>>8) + (osc->baseNote-SEQ_DEFAULT_NOTE);
   if(note>127)note=127;
    if(note<0)note=0;
   osc->freq = MidiNoteFrequencies[note]*cent;
 }
```
Notes: `fabsf` becomes `std::fabs` on a float. `SEMITONE_UP` is a float literal: keep it float. In `osc_recalcFreq` the global `MidiNoteFrequencies[128]` (float table of note frequencies) becomes a pointer argument, and `osc->midiFreq` / `osc->baseNote` become arguments; return the frequency instead of storing it. `lfo_setFreq` receives the 0..127 value as a float `f`. `midiParser_calcDetune` has two identical branches: keep the behaviour, the `if/else` may be merged (mark it `// ORIGINAL QUIRK: both branches identical`).

2. Parameter rows (param_map.md): FILTER_FREQ_1..6 -> cutoff, VOICE_DECIMATION1..6 and _ALL -> decim, DRIVE1..3/SNARE_DISTORTION/CYMBAL_DISTORTION/HAT_DISTORTION -> dist, NOISE_FREQ1 -> noisef, TRANS1..6_FREQ -> trF, TRANS1..6_WAVE -> trW, FREQ_LFO1..6 -> lfoF, OFFSET_LFO1..6 -> lfoOfs, FILTER_TYPE_1..6 -> ftype, COARSE/FINE -> coarse/fine, and every `lin` row (v/127).

3. Dependencies: none. (The filter itself already exists: `cutoffShape(v)` is later passed to `ResonantFilter::directSetFilterValue`.)

OUTPUT FILES
- dsp/ParamMapMisc.h
- dsp/ParamMapMisc.cpp
Do not write a test file: a test program is supplied separately and will be compiled against your two files.

INTERFACE (must match exactly)
```cpp
// dsp/ParamMapMisc.h
#pragma once
#include <cstdint>
namespace lxr {
constexpr uint8_t kSeqDefaultNote = 63;   // SEQ_DEFAULT_NOTE
float    unitFromParam(uint8_t v);         // v/127.f
float    cutoffShape(uint8_t v);           // valueShaperF2F(v/127.f, FILTER_SHAPER)
float    decimationRate(uint8_t v);        // valueShaperI2F(v, -0.7f)
float    distortionShape(uint8_t v);       // setDistortionShape
float    noiseFrequencyHz(uint8_t v);      // v/127.f*22000
float    transientPitch(uint8_t v);        // 1.f + ((v/33.9f)-0.75f)
uint8_t  transientWaveform(uint8_t v);     // transient_setWaveform
float    lfoFrequencyHz(uint8_t v);        // lfo_setFreq, frequency only
uint32_t lfoPhaseOffset(uint8_t v);        // v/127.f * 0xffffffff
uint8_t  filterTypeFromParam(uint8_t v);   // v + 1
float    fineDetune(uint8_t v);            // midiParser_calcDetune
float    oscFrequencyHz(uint16_t midiFreq, uint8_t baseNote, const float* noteTable); // osc_recalcFreq
} // namespace lxr
```
All functions are pure, stateless and safe to call from the audio thread. `noteTable` points to 128 floats.

BEHAVIOUR
1. Each function is a line-by-line port of the C code named in its comment, keeping operation order and float types. Put file-local helpers (`valueShaperI2F`, `valueShaperF2F`, constants) in an anonymous namespace.
2. `lfoPhaseOffset`: the original computes `v/127.f * 0xffffffff` (a float, because 0xffffffff converts to 4294967296.0f) and assigns it to a uint32_t. At v = 127 the float is exactly 2^32, which is out of range for uint32_t (undefined behaviour in C; the ARM hardware saturates). Port decision: convert with truncation, and if the float is >= 4294967296.0f return 0xFFFFFFFF. Mark `// ORIGINAL QUIRK:`.
3. `transientWaveform`: values below NUM_TRANSIENTS + 2 (that is 0..13) pass through, anything else returns 0.
4. Engine constants for context only: LFO control rate = 44002.7573529412 / 32 Hz; these mappings do not use it.

CONSTRAINTS
- Float arithmetic only (no double). No heap allocation, no globals with state.

ACCEPTANCE
The supplied test compares against values computed from the original C within 4 ULP (relative 4.8e-7); integer results must match exactly. A few of the expected values, to sanity check your work:
```
v=0   cutoffShaped=0  decim=0  dist=0  noiseF=0  lfoHz=9.53674316e-05  trPitch=0.25  detune=0.970268428
v=1   cutoffShaped=0.000417931995  decim=0.00139898481  dist=0.0157480314  noiseF=173.228348  lfoHz=0.000762939453  trPitch=0.279498518  detune=0.970736623
v=64  cutoffShaped=0.0507990122  decim=0.152054384  dist=2  noiseF=11086.6143  lfoHz=26.1902809  trPitch=2.1379056  detune=1.00023413
v=127 cutoffShaped=1  decim=1  dist=254  noiseF=22000  lfoHz=200  trPitch=3.99631262  detune=1.02973151
lfoPhaseOffset: v=0 -> 0, v=1 -> 33818640, v=64 -> 2164392960, v=127 -> 4294967295
transientWaveform: 13 -> 13, 14 -> 0
```

OUT OF SCOPE
Envelope mappings, pan and routing, LFO phase increment and tempo sync, the filter, oscillators, and the MIDI/NRPN parser.
