// dsp/ResonantFilter.h
#pragma once
#include <cstdint>
namespace lxr {
enum FilterType : uint8_t { FILTER_LP = 1, FILTER_HP, FILTER_BP, FILTER_UNITY_BP,
                            FILTER_NOTCH, FILTER_PEAK, FILTER_NAIVE_2_POLE };
constexpr int32_t kFilterGain = 0x70ff;
struct ResonantFilter {
    // Firmware voices live in zero-filled .ccmz startup storage. A fresh C++
    // Engine temporary has automatic storage; SVF_init omits zi, so establish
    // that startup value here without changing the original init/reset maths.
    float f, g, q, s1, s2, a, b, zi = 0.f, drive; // same fields/layout as C
    void init();                                                   // SVF_init
    void reset();                                                  // SVF_reset
    void setReso(float feedback);                                  // SVF_setReso
    void setDrive(uint8_t drive);                                  // SVF_setDrive
    void directSetFilterValue(float val);                          // SVF_directSetFilterValue
    void recalcFreq();                                             // SVF_recalcFreq
    void calcBlockZDF(uint8_t type, int16_t* buf, uint8_t size);   // SVF_calcBlockZDF, in place
};
float fastTan(float x);
float tanhXdX(float x);
float softClipTwo(float in);
} // namespace lxr
