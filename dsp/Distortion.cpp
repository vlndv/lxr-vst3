// FILE: dsp/Distortion.cpp
#include "Distortion.h"
#include <cmath>

namespace lxr {

void Distortion::init() {
    shape = 0.0f;
    inv_shape = 0.0f;
}

void Distortion::setShape(uint8_t s) {
    // ORIGINAL QUIRK: Uses 128.f as denominator, not 127.f. 
    // At v=127, shape = 254.0f exactly.
    float norm = s / 128.0f;
    shape = 2.0f * norm / (1.0f - norm);
}

void Distortion::calcBlock(int16_t* buf, uint8_t size) {
    for (uint8_t i = 0; i < size; i++) {
        float x = buf[i] / 32767.0f;
        x = (1.0f + shape) * x / (1.0f + shape * std::fabs(x));
        // ORIGINAL QUIRK: float to int16_t cast truncates towards zero. 
        // No explicit clamping, but math guarantees result is in (-32768, 32768).
        buf[i] = static_cast<int16_t>(x * 32767.0f);
    }
}

float Distortion::calcSampleFloat(float x) {
    return (1.0f + shape) * x / (1.0f + shape * std::fabs(x));
}

} // namespace lxr