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
}
