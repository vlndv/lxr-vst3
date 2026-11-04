// FILE: dsp/Lfo.h
#pragma once
#include <cstdint>
#include "Oscillator.h" // For OscRng

namespace lxr {

enum LfoWave : uint8_t {
    LFO_SINE = 0,
    LFO_TRI = 1,
    LFO_SAW_UP = 2,
    LFO_SAW_DOWN = 3,
    LFO_SQUARE = 4,
    LFO_RANDOM = 5,
    LFO_EXP_UP = 6,
    LFO_EXP_DOWN = 7
};

struct Lfo {
    uint32_t phase = 0;
    uint32_t phaseInc = 1;
    float frequency = 1.f;
    float modNodeValue = 1.f;
    uint8_t waveform = LFO_SINE;
    uint8_t retrigger = 0;
    uint8_t sync = 0;
    uint32_t phaseOffset = 0;
    float amount = 0.f;
    float output = 0.f;
    float heldValue = 0.f;
    uint32_t lastPhase = 0;
    
    OscRng rng; // Per-instance PRNG for S&H (replaces unsafe global rand())

    void init();
    void setRate(float hz, uint16_t nativeBpm = 120);
    void setSync(uint8_t value, uint16_t nativeBpm = 120);
    void setTempoSync(uint8_t syncValue, float bpm);
    void trigger(uint8_t voiceNum);
    // Immutable original table supplied by the processing owner's asset view.
    void tick(const int16_t* sine = nullptr);
    float getOutput();
};

float lfoFrequencyFromMidi(uint8_t v);
uint32_t lfoPhaseOffsetFromMidi(uint8_t v);

} // namespace lxr
