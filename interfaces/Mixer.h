// FILE: dsp/Mixer.h
#pragma once
#include <cstdint>
#include "Decimator.h"

namespace lxr {

constexpr uint8_t kNumVoices = 6;
constexpr uint8_t kNumTracks = 7; // 6 voices + 1 master/hihat-open
constexpr uint8_t kBlockSize = 32; // OUTPUT_DMA_SIZE

enum MixerRoute : uint8_t {
    ROUTE_ST1 = 0,     // Stereo 1 (DAC1 stereo)
    ROUTE_ST2 = 1,     // Stereo 2 (DAC2 stereo)
    ROUTE_ST1_L = 2,   // DAC1 Left only
    ROUTE_ST1_R = 3,   // DAC1 Right only
    ROUTE_ST2_L = 4,   // DAC2 Left only
    ROUTE_ST2_R = 5    // DAC2 Right only
};

struct Mixer {
    uint8_t routing[kNumVoices];
    uint8_t pan[kNumVoices];
    bool mute[kNumTracks]; // 0-5 = voices, 6 = hihat-open track
    
    Decimator decimators[kNumVoices];
    float decimationRate[kNumVoices + 1]; // 0-5 per voice, 6 = global ALL
    
    float sqrtLut[128];
    
    void init();
    void setPan(uint8_t voice, uint8_t panVal);
    void setRouting(uint8_t voice, uint8_t route);
    void setMute(uint8_t track, bool muted);
    void setDecimationRate(uint8_t voice, float rate);
    void setGlobalDecimationRate(float rate);
    
    // Process 6 voice buffers into 2 stereo output pairs.
    // voiceBuffers: array of 6 pointers to int16 buffers (each `size` samples)
    // Outputs are cleared before summing.
    void processBlock(int16_t* voiceBuffers[kNumVoices],
                      int16_t* outSt1L, int16_t* outSt1R,
                      int16_t* outSt2L, int16_t* outSt2R,
                      uint8_t size);
};

// Saturating int16 add (replaces ARM __QADD16)
inline int16_t saturatingAdd(int16_t a, int16_t b) {
    int32_t sum = static_cast<int32_t>(a) + static_cast<int32_t>(b);
    if (sum > 32767) return 32767;
    if (sum < -32768) return -32768;
    return static_cast<int16_t>(sum);
}

} // namespace lxr