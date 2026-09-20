GOAL
Port the 0..127 to engine-value mappings of the LXR amp envelope (attack step, decay step, slope) and pitch envelope (decay step, slope, amount) to C++17. Each mapping is a pure function: given a 0..127 value it returns the float the original stores in its envelope struct. They sit between the MIDI/UI parameter layer and the envelope state machines (not part of this task).

INPUTS
1. Original C (verbatim except that comments were removed and tabs replaced by spaces):
```c
// config.h
#define PITCH_AMOUNT_FACTOR 32

// MidiParser.c
static inline float calcPitchModAmount(uint8_t data2)
{
  const float val = data2/127.f;
  return val*val*PITCH_AMOUNT_FACTOR;
}

// SlopeEg2.c
#define TIME_AMOUNT_DECAY 0.999f
#define TIME_AMOUNT_ATTACK 0.99f
#define TIME_K_ATTACK (2*TIME_AMOUNT_ATTACK/(1.f-TIME_AMOUNT_ATTACK))
#define TIME_K_DECAY (2*TIME_AMOUNT_DECAY/(1.f-TIME_AMOUNT_DECAY))

float slopeEg2_calcTime(uint8_t data2, float amount)
{
  const float val = (data2)/127.f;
  return 1.f-((1.f+amount)*val/(1.f+amount*fabsf(val)));
}
void slopeEg2_setAttack(SlopeEg2* eg, uint8_t data2, uint8_t isSync)
{
  eg->attack = slopeEg2_calcTime(data2,TIME_K_ATTACK);
  if(isSync)
    eg->attack /= 16;
}
void slopeEg2_setDecay(SlopeEg2* eg, uint8_t data2, uint8_t isSync)
{
  eg->decay = slopeEg2_calcTime(data2,TIME_K_DECAY);
  if(isSync)
      eg->decay /= 16;
}
float slopeEg2_calcDecay(uint8_t data2)
{
  return slopeEg2_calcTime(data2,TIME_K_DECAY);
}
void slopeEg2_setSlope(SlopeEg2* eg, uint8_t data2)
{
  const float amount      = ((data2/127.f)-0.5f)*1.999f;
  const float invAmount   = -amount;
  eg->slope     = 2*amount/(1-amount);
  eg->invSlope  = 2*invAmount/(1-invAmount);
}

// Decay.c
#define TIME_K (2*0.99f/(1.f-0.99f))

float DecayEg_calcTime(uint8_t data2)
{
  const float val = (data2)/127.f;
  return 1.f-((1.f+TIME_K)*val/(1.f+TIME_K*fabsf(val)));
}
void DecayEg_setDecay(DecayEg* eg, uint8_t data2)
{
  eg->decay = DecayEg_calcTime(data2);
}
void DecayEg_setSlope(DecayEg* eg, uint8_t data2)
{
  const float amount   = ((data2/127.f)-0.5f)*2;
  eg->slope       = 2*amount/(1-amount);
}
```
Notes: `TIME_K_ATTACK`, `TIME_K_DECAY` and `TIME_K` are compile-time expressions of FLOAT constants (0.99f, 0.999f, 1.f): compute them as `constexpr float` in float arithmetic, never in double, and never replace them by round numbers such as 198 or 1998 (the real values are about 198.000198 and 1998.02576). `fabsf` becomes `std::fabs` on a float. The `isSync` argument is always 0 in this build (`AMP_EG_SYNC` is 0), so the division by 16 is not ported. In the C code the results are stored in struct fields (`eg->attack`, `eg->decay`, `eg->slope`, `eg->invSlope`, `eg->decay` of `DecayEg`); in C++ they are return values.

2. Parameter rows (param_map.md): VELOA1..6 -> egA, VELOD1..6 -> egD, VOL_SLOPE1..6 -> slopeA, MOD_EG1..4 -> pDecay, MODAMNT1..4 -> pAmt, PITCH_SLOPE1..4 -> slopeP.

3. Dependencies: none.

OUTPUT FILES
- dsp/ParamMapEnv.h
- dsp/ParamMapEnv.cpp
Do not write a test file: a test program is supplied separately and will be compiled against your two files.

INTERFACE (must match exactly)
```cpp
// dsp/ParamMapEnv.h
#pragma once
#include <cstdint>
namespace lxr {
struct AmpSlope { float slope; float invSlope; };
float    egAttackStep(uint8_t v);      // slopeEg2_setAttack (isSync = 0)
float    egDecayStep(uint8_t v);       // slopeEg2_setDecay (isSync = 0) == slopeEg2_calcDecay
AmpSlope ampEgSlope(uint8_t v);        // slopeEg2_setSlope
float    pitchEgDecayStep(uint8_t v);  // DecayEg_setDecay
float    pitchEgSlope(uint8_t v);      // DecayEg_setSlope
float    pitchModAmount(uint8_t v);    // calcPitchModAmount
} // namespace lxr
```
All functions are pure, stateless and safe to call from the audio thread.

BEHAVIOUR
1. Each function is a line-by-line port of the C function named in its comment, keeping the operation order and float types.
2. Put file-local helpers and constants in an anonymous namespace.
3. Engine constants for context only: tick rate = 44002.7573529412 / 32 Hz. These mappings do not use it.

CONSTRAINTS
- Float arithmetic only (no double). No heap allocation, no globals with state.
- Mark with `// ORIGINAL QUIRK:` that `pitchEgSlope(127)` divides by zero and returns +infinity. Do not clamp or guard it.

ACCEPTANCE
The supplied test compares against values computed from the original C within 4 ULP (relative 4.8e-7); +infinity is expected for pitchEgSlope(127). A few of the expected values, to sanity check your work:
```
v=0   egA=1  egD=1  slopeA=-0.999749899  invSlopeA=3997.81299  pDecay=1  slopeP=-1  pAmt=0
v=1   egA=0.387692094  egD=0.0592934489  slopeA=-0.991813421  invSlopeA=121.151344  pDecay=0.387692094  slopeP=-0.992063522  pAmt=0.00198400393
v=64  egA=0.00492227077  egD=0.000492155552  slopeA=0.0158650074  invSlopeA=-0.0156172402  pDecay=0.00492227077  slopeP=0.0158730075  pAmt=8.1264801
v=127 egA=0  egD=0  slopeA=3997.81299  invSlopeA=-0.999749899  pDecay=0  slopeP=inf  pAmt=32
```

OUT OF SCOPE
Envelope state machines, the slope warp function, tick timing, the isSync path, resonance, filter, LFO and any other mapping.
