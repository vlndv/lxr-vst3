// Derived from Sonic Potions LXR 0.37 lfo.c, Copyright 2013 Julian Schmidt.
// See LICENSE-LXR.txt. Numeric qualifications are documented in P15-M2E.
#include "Lfo.h"
#include <cmath>
namespace lxr {
namespace {
uint32_t narrowPhase(float x) {
    // ARM-qualified saturation, avoiding undefined out-of-range C++ casts.
    if (!std::isfinite(x) || x <= 0.f) return 0;
    if (x >= 4294967296.f) return 0xffffffffu;
    return static_cast<uint32_t>(x);
}
uint32_t increment(float frequency, uint8_t sync, uint16_t bpm) {
    static constexpr float scales[] = {.25f,.5f,1.f,2.f,3.f,4.f,6.f,8.f,12.f,16.f,32.f};
    if (sync >= 1 && sync <= 11) frequency = (bpm / 60.f / 4.f) * scales[sync-1];
    return narrowPhase(frequency / (kRealFs / 32.f) * 0xffffffffu);
}
}
void Lfo::init() {
    phase=0; phaseInc=1; frequency=1.f; modNodeValue=1.f;
    waveform=LFO_SINE; retrigger=0; sync=0; phaseOffset=0;
    amount=0.f; output=0.f; lastPhase=0;
    // ORIGINAL QUIRK: lfo_init does not clear held noise. Full Engine reset does.
}
void Lfo::setRate(float hz,uint16_t bpm) {
    if (!std::isfinite(hz) || hz<0.f || hz>200.f) return;
    frequency=hz; phaseInc=increment(frequency,sync,bpm);
}
void Lfo::setSync(uint8_t value,uint16_t bpm) {
    sync=value; phaseInc=increment(frequency,sync,bpm);
}
void Lfo::setTempoSync(uint8_t value,float bpm) {
    // Only explicit integer native BPM in the verified domain; no host coupling.
    if (!std::isfinite(bpm)||bpm<0.f||bpm>300.f||std::floor(bpm)!=bpm) return;
    setSync(value,static_cast<uint16_t>(bpm));
}
void Lfo::trigger(uint8_t voiceNum) {
    if (voiceNum<6 && retrigger==voiceNum+1) phase=phaseOffset;
}
void Lfo::tick(const int16_t* sine) {
    lastPhase=phase;
    // ORIGINAL QUIRK: uint32 -> float -> multiply -> uint32 even at multiplier1.
    phase+=narrowPhase(phaseInc*modNodeValue);
    switch(waveform) {
    case LFO_SINE: output=sine ? ((sine[phase>>20]/32767.f)+1)/2.f : 0.f; break;
    case LFO_TRI: output=1.f-std::fabs((phase/(float)0xffffffffu)*2-1); break;
    case LFO_SAW_UP: output=phase/(float)0xffffffffu; break;
    case LFO_SAW_DOWN: output=(0xffffffffu-phase)/(float)0xffffffffu; break;
    case LFO_SQUARE: output=phase>0x7fffffffu?1.f:0.f; break;
    case LFO_RANDOM:
        if (lastPhase>phase) {
            heldValue=static_cast<float>(rng.next());
            heldValue=heldValue/(float)0xffffffffu;
        }
        output=heldValue; break;
    case LFO_EXP_UP: {const float x=phase/(float)0xffffffffu;output=x*x*x;break;}
    case LFO_EXP_DOWN: {const float x=(0xffffffffu-phase)/(float)0xffffffffu;output=x*x*x;break;}
    default: output=0.f;
    }
}
float Lfo::getOutput() {return output;}
float lfoFrequencyFromMidi(uint8_t v) {
    float x=v; x+=1; x/=128.f; x=x*x*x; x*=200;
    return x;
}
uint32_t lfoPhaseOffsetFromMidi(uint8_t v) {return narrowPhase((v/127.f)*0xffffffffu);}
} // namespace lxr
