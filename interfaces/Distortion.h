// FILE: dsp/Distortion.h
#pragma once
#include <cstdint>

namespace lxr {

struct Distortion {
    float shape;
    float inv_shape; // ORIGINAL QUIRK: declared in original C but never used.

    void init();
    void setShape(uint8_t shape);
    void calcBlock(int16_t* buf, uint8_t size);
    float calcSampleFloat(float x);
};

} // namespace lxr