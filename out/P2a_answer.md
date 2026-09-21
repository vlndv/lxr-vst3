```cpp
// dsp/ParamMapEnv.h
#pragma once
#include <cstdint>
#include <cmath>

namespace lxr {
struct AmpSlope { float slope; float invSlope; };
float    egAttackStep(uint8_t v);      // slopeEg2_setAttack (isSync = 0)
float    egDecayStep(uint8_t v);       // slopeEg2_setDecay (isSync = 0) ==[2D[K
== slopeEg2_calcDecay
AmpSlope ampEgSlope(uint8_t v);        // slopeEg2_setSlope
float    pitchEgDecayStep(uint8_t v);  // DecayEg_setDecay
float    pitchEgSlope(uint8_t v);      // DecayEg_setSlope
float    pitchModAmount(uint8_t v);    // calcPitchModAmount
} // namespace lxr
```

```cpp
// dsp/ParamMapEnv.cpp
#include "ParamMapEnv.h"

namespace {

constexpr float PITCH_AMOUNT_FACTOR = 32.0f;

constexpr float TIME_AMOUNT_DECAY = 0.999f;
constexpr float TIME_AMOUNT_ATTACK = 0.99f;
constexpr float TIME_K_ATTACK = 2.0f * TIME_AMOUNT_ATTACK / (1.0f - TIME_AM[7D[K
TIME_AMOUNT_ATTACK);
constexpr float TIME_K_DECAY = 2.0f * TIME_AMOUNT_DECAY / (1.0f - TIME_AMOU[9D[K
TIME_AMOUNT_DECAY);

constexpr float TIME_K = 2.0f * 0.99f / (1.0f - 0.99f);

float slopeEg2_calcTime(uint8_t data2, float amount) {
  const float val = data2 / 127.0f;
  return 1.0f - ((1.0f + amount) * val / (1.0f + amount * std::fabs(val)));[17D[K
std::fabs(val)));
}

float pitchEg_calcTime(uint8_t data2) {
  const float val = data2 / 127.0f;
  return 1.0f - ((1.0f + TIME_K) * val / (1.0f + TIME_K * std::fabs(val)));[17D[K
std::fabs(val)));
}

} // namespace

namespace lxr {

float egAttackStep(uint8_t v) {
  return slopeEg2_calcTime(v, TIME_K_ATTACK);
}

float egDecayStep(uint8_t v) {
  return slopeEg2_calcTime(v, TIME_K_DECAY);
}

AmpSlope ampEgSlope(uint8_t v) {
  const float amount = ((v / 127.0f) - 0.5f) * 1.999f;
  const float invAmount = -amount;
  AmpSlope result;
  result.slope = 2.0f * amount / (1.0f - amount);
  result.invSlope = 2.0f * invAmount / (1.0f - invAmount);
  return result;
}

float pitchEgDecayStep(uint8_t v) {
  return pitchEg_calcTime(v);
}

float pitchEgSlope(uint8_t v) {
  const float amount = ((v / 127.0f) - 0.5f) * 2.0f;
  return 2.0f * amount / (1.0f - amount);
}

float pitchModAmount(uint8_t v) {
  const float val = v / 127.0f;
  return val * val * PITCH_AMOUNT_FACTOR;
}

} // namespace lxr
```

UNSURE:
- None

QUIRKS:
- None

