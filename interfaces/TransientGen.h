// FILE: dsp/TransientGen.h
#pragma once
#include <cstdint>

namespace lxr {

struct TransientGen {
    int16_t output;
    uint32_t phase;
    float pitch;
    uint8_t waveform;
    float volume;
    // midiFreq omitted from init to match original, kept structurally if needed later.

    void init();
    void trigger();
    void calc();
    void calcBlock(int16_t* buf, uint8_t size);
    void setWaveform(uint8_t waveform);
};

} // namespace lxr