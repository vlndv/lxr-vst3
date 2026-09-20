// dsp/ParamMapMisc.cpp
#include "ParamMapMisc.h"
#include <cmath>
namespace lxr {
namespace {
constexpr float kFilterShaper = -0.9f;
constexpr float kSemitoneUp = 1.0594630943592952645618252949463f;
constexpr int   kLfoMaxF = 200;
constexpr int   kNumTransients = 12;
float valueShaperI2F(uint8_t data2, float shape) {
    const float k = 2 * shape / (1.0001f - shape);
    const float val = (data2) / 127.f;
    return ((1 + k) * val / (1 + k * std::fabs(val)));
}
float valueShaperF2F(float val, float shape) {
    const float k = 2 * shape / (1.0001f - shape);
    return ((1 + k) * val / (1 + k * std::fabs(val)));
}
}
float unitFromParam(uint8_t v) { return v / 127.f; }
float cutoffShape(uint8_t v) { const float f = v / 127.f; return valueShaperF2F(f, kFilterShaper); }
float decimationRate(uint8_t v) { return valueShaperI2F(v, -0.7f); }
float distortionShape(uint8_t v) { return 2 * (v / 128.f) / (1 - (v / 128.f)); }
float noiseFrequencyHz(uint8_t v) { return v / 127.f * 22000; }
float transientPitch(uint8_t v) { return 1.f + ((v / 33.9f) - 0.75f); }
uint8_t transientWaveform(uint8_t v) { return v < kNumTransients + 2 ? v : 0; }
float lfoFrequencyHz(uint8_t v) {
    float f = v;
    f += 1; f = f / 128.f; f = f * f * f;
    return f * kLfoMaxF;
}
uint32_t lfoPhaseOffset(uint8_t v) {
    // ORIGINAL QUIRK: at v = 127 the float product is 2^32, out of uint32 range (undefined in C).
    // Port decision: saturate to 0xFFFFFFFF (ARM VCVT behaviour).
    const float x = v / 127.f * 0xffffffff;
    return x >= 4294967296.0f ? 0xFFFFFFFFu : (uint32_t)x;
}
uint8_t filterTypeFromParam(uint8_t v) { return (uint8_t)(v + 1); }
float fineDetune(uint8_t value) {
    float frac = (value / 127.f - 0.5f);
    float cent = 1;
    cent += frac * (kSemitoneUp - 1);
    return cent;
}
float oscFrequencyHz(uint16_t midiFreq, uint8_t baseNote, const float* noteTable) {
    const float cent = fineDetune(midiFreq & 0xff);
    int16_t note = (midiFreq >> 8) + (baseNote - kSeqDefaultNote);
    if (note > 127) note = 127;
    if (note < 0) note = 0;
    return noteTable[note] * cent;
}
}
