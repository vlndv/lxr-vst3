// FILE: dsp/Lfo.h
#pragma once
#include <cstdint>

namespace lxr {

enum LfoWave : uint8_t {
    LFO_SINE = 0,
    LFO_TRI = 1,
    LFO_SAW_UP = 2,
    LFO_SAW_DOWN = 3,
    LFO_SQUARE = 4,
    LFO_RANDOM = 5,  // S&H
    LFO_EXP_UP = 6,
    LFO_EXP_DOWN = 7
};

struct Lfo {
    uint32_t phase = 0;
    uint32_t phaseInc = 0;
    uint8_t waveform = LFO_SINE;
    uint8_t retrigger = 0;  // 0 = off, 1..6 = voice number
    uint8_t sync = 0;       // 0 = free, 1..11 = tempo sync
    uint32_t phaseOffset = 0;
    float amount = 0.f;
    float output = 0.f;     // 0..1 unipolar
    
    // For S&H waveform
    float heldValue = 0.f;
    uint32_t lastPhase = 0;
    
    void init();
    void setRate(float hz);
    void setTempoSync(uint8_t syncValue, float bpm);
    void trigger(uint8_t voiceNum);
    void tick();
    float getOutput();
};

// Maps 0..127 to LFO frequency in Hz
// Formula: ((v+1)/128)^3 x 200
float lfoFrequencyFromMidi(uint8_t v);

// Maps 0..127 to phase offset (uint32)
// Formula: v/127.f x 0xffffffff, with ARM saturation at v=127
uint32_t lfoPhaseOffsetFromMidi(uint8_t v);

} // namespace lxr