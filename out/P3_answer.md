```cpp
// dsp/ResonantFilter.h
#pragma once
#include <cstdint>
namespace lxr {
enum FilterType : uint8_t { FILTER_LP = 1, FILTER_HP, FILTER_BP, FILTER_UNI[10D[K
FILTER_UNITY_BP,
                            FILTER_NOTCH, FILTER_PEAK, FILTER_NAIVE_2_POLE [K
};
constexpr int32_t kFilterGain = 0x70ff;
struct ResonantFilter {
    float f, g, q, s1, s2, a, b, zi, drive;   // same fields as the C struc[5D[K
struct
    void init();                                                   // SVF_i[5D[K
SVF_init
    void reset();                                                  // SVF_r[5D[K
SVF_reset
    void setReso(float feedback);                                  // SVF_s[5D[K
SVF_setReso
    void setDrive(uint8_t drive);                                  // SVF_s[5D[K
SVF_setDrive
    void directSetFilterValue(float val);                          // SVF_d[5D[K
SVF_directSetFilterValue
    void recalcFreq();                                             // SVF_r[5D[K
SVF_recalcFreq
    void calcBlockZDF(uint8_t type, int16_t* buf, uint8_t size);   // SVF_c[5D[K
SVF_calcBlockZDF, in place
};
float fastTan(float x);
float tanhXdX(float x);
float softClipTwo(float in);
} // namespace lxr
```

```cpp
// dsp/ResonantFilter.cpp
#include "ResonantFilter.h"
#include <cmath>
#include <algorithm>

namespace lxr {

static float fastTanh(float var)
{
   if(var < -1.95f)     return -1.0f;
   else
