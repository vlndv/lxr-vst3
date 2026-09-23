// FILE: dsp/Decimator.cpp
#include "Decimator.h"

namespace lxr {

void Decimator::init() {
    cnt = 0.0f;
    heldSample = 0;
}

void Decimator::processBlock(int16_t* buf, uint8_t size, float voiceRate, float allRate) {
    float increment = voiceRate * allRate;
    for (uint8_t i = 0; i < size; i++) {
        cnt += increment;
        if (cnt >= 1.0f) {
            cnt -= 1.0f;
            heldSample = buf[i];
        }
        buf[i] = heldSample;
    }
}

} // namespace lxr