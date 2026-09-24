// FILE: dsp/Mixer.h
#pragma once
#include <cstdint>
#include "Decimator.h"

namespace lxr {

constexpr uint8_t kNumVoices = 6;
constexpr uint8_t kBlockSize = 32; // OUTPUT_DMA_SIZE

enum MixerRoute : uint8_t {
    ROUTE_ST1 = 0,
    ROUTE_ST2 = 1,
    ROUTE_ST1_L = 2,
    ROUTE_ST1_R = 3,
    ROUTE_ST2_L = 4,
    ROUTE_ST2_R = 5
};

struct Mixer {
    uint8_t routing[kNumVoices];
    uint8_t pan[kNumVoices];
    
    Decimator decimators[kNumVoices];
    float decimationRate[kNumVoices + 1]; // 0-5 per voice, 6 = global ALL
    
    float sqrtLut[128];
    
    void init();
    void setPan(uint8_t voice, uint8_t panVal);
    void setRouting(uint8_t voice, uint8_t route);
    void setDecimationRate(uint8_t voice, float rate);
    void setGlobalDecimationRate(float rate);
    
    void processBlock(int16_t* voiceBuffers[kNumVoices],
                      int16_t* outSt1L, int16_t* outSt1R,
                      int16_t* outSt2L, int16_t* outSt2R,
                      uint8_t size);
};

inline int16_t saturatingAdd(int16_t a, int16_t b) {
    int32_t sum = static_cast<int32_t>(a) + static_cast<int32_t>(b);
    if (sum > 32767) return 32767;
    if (sum < -32768) return -32768;
    return static_cast<int16_t>(sum);
}

} // namespace lxr