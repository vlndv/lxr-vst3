// FILE: dsp/Decimator.h
#pragma once
#include <cstdint>

namespace lxr {

struct Decimator {
    float cnt;
    int16_t heldSample;

    void init();
    // voiceRate is the per-voice decimation rate, allRate is the global "ALL" rate (index 6).
    void processBlock(int16_t* buf, uint8_t size, float voiceRate, float allRate);
};

} // namespace lxr