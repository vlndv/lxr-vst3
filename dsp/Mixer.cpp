// FILE: dsp/Mixer.cpp
#include "Mixer.h"
#include <cmath>

namespace lxr {

void Mixer::init() {
    for (int i = 0; i < 128; i++) {
        sqrtLut[i] = std::sqrt(i / 127.0f);
    }
    
    for (uint8_t v = 0; v < kNumVoices; v++) {
        routing[v] = ROUTE_ST1;
        pan[v] = 64;
        decimators[v].init();
        decimationRate[v] = 1.0f;
    }
    decimationRate[6] = 1.0f; // global ALL
}

void Mixer::setPan(uint8_t voice, uint8_t panVal) {
    if (voice < kNumVoices) pan[voice] = panVal;
}

void Mixer::setRouting(uint8_t voice, uint8_t route) {
    if (voice < kNumVoices && route <= ROUTE_ST2_R) routing[voice] = route;
}

void Mixer::setDecimationRate(uint8_t voice, float rate) {
    if (voice < kNumVoices) decimationRate[voice] = rate;
}

void Mixer::setGlobalDecimationRate(float rate) {
    decimationRate[6] = rate;
}

void Mixer::processBlock(int16_t* voiceBuffers[kNumVoices],
                          int16_t* outSt1L, int16_t* outSt1R,
                          int16_t* outSt2L, int16_t* outSt2R,
                          uint8_t size) {
    for (uint8_t i = 0; i < size; i++) {
        outSt1L[i] = 0;
        outSt1R[i] = 0;
        outSt2L[i] = 0;
        outSt2R[i] = 0;
    }
    
    for (uint8_t v = 0; v < kNumVoices; v++) {
        int16_t* buf = voiceBuffers[v];
        if (buf == nullptr) continue;
        
        decimators[v].processBlock(buf, size, decimationRate[v], decimationRate[6]);
        
        float panL = sqrtLut[127 - pan[v]];
        float panR = sqrtLut[pan[v]];
        
        uint8_t route = routing[v];
        
        switch (route) {
            case ROUTE_ST1:
                for (uint8_t i = 0; i < size; i++) {
                    outSt1L[i] = saturatingAdd(outSt1L[i], static_cast<int16_t>(buf[i] * panL));
                    outSt1R[i] = saturatingAdd(outSt1R[i], static_cast<int16_t>(buf[i] * panR));
                }
                break;
            case ROUTE_ST2:
                for (uint8_t i = 0; i < size; i++) {
                    outSt2L[i] = saturatingAdd(outSt2L[i], static_cast<int16_t>(buf[i] * panL));
                    outSt2R[i] = saturatingAdd(outSt2R[i], static_cast<int16_t>(buf[i] * panR));
                }
                break;
            case ROUTE_ST1_L:
                for (uint8_t i = 0; i < size; i++) outSt1L[i] = saturatingAdd(outSt1L[i], buf[i]);
                break;
            case ROUTE_ST1_R:
                for (uint8_t i = 0; i < size; i++) outSt1R[i] = saturatingAdd(outSt1R[i], buf[i]);
                break;
            case ROUTE_ST2_L:
                for (uint8_t i = 0; i < size; i++) outSt2L[i] = saturatingAdd(outSt2L[i], buf[i]);
                break;
            case ROUTE_ST2_R:
                for (uint8_t i = 0; i < size; i++) outSt2R[i] = saturatingAdd(outSt2R[i], buf[i]);
                break;
            default:
                break;
        }
    }
}

} // namespace lxr