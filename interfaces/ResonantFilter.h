// dsp/ResonantFilter.h
#pragma once
#include <cstdint>
namespace lxr {
enum FilterType : uint8_t { FILTER_LP = 1, FILTER_HP, FILTER_BP, FILTER_UNITY_BP,
                            FILTER_NOTCH, FILTER_PEAK, FILTER_NAIVE_2_POLE };
constexpr int32_t kFilterGain = 0x70ff;
struct ResonantFilter {
    float f, g, q, s1, s2, a, b, zi, drive;   // same fields as the C struct
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
