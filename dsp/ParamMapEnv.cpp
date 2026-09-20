// dsp/ParamMapEnv.cpp
#include "ParamMapEnv.h"
#include <cmath>
namespace lxr {
namespace {
constexpr float kTimeAmountDecay  = 0.999f;
constexpr float kTimeAmountAttack = 0.99f;
constexpr float kTimeKAttack = (2 * kTimeAmountAttack / (1.f - kTimeAmountAttack));
constexpr float kTimeKDecay  = (2 * kTimeAmountDecay / (1.f - kTimeAmountDecay));
constexpr float kPitchKTime  = (2 * 0.99f / (1.f - 0.99f));
constexpr int   kPitchAmountFactor = 32;
float calcTime(uint8_t data2, float amount) {
    const float val = (data2) / 127.f;
    return 1.f - ((1.f + amount) * val / (1.f + amount * std::fabs(val)));
}
}
float egAttackStep(uint8_t v) { return calcTime(v, kTimeKAttack); }
float egDecayStep(uint8_t v)  { return calcTime(v, kTimeKDecay); }
AmpSlope ampEgSlope(uint8_t v) {
    const float amount = ((v / 127.f) - 0.5f) * 1.999f;
    const float invAmount = -amount;
    return { 2 * amount / (1 - amount), 2 * invAmount / (1 - invAmount) };
}
float pitchEgDecayStep(uint8_t v) {
    const float val = (v) / 127.f;
    return 1.f - ((1.f + kPitchKTime) * val / (1.f + kPitchKTime * std::fabs(val)));
}
float pitchEgSlope(uint8_t v) {
    // ORIGINAL QUIRK: v = 127 gives amount = 1 -> division by zero -> +inf
    const float amount = ((v / 127.f) - 0.5f) * 2;
    return 2 * amount / (1 - amount);
}
float pitchModAmount(uint8_t v) {
    const float val = v / 127.f;
    return val * val * kPitchAmountFactor;
}
}
