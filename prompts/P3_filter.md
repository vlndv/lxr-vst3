GOAL
Port the nonlinear zero-delay-feedback state-variable filter of the LXR drum synth to C++17. It filters one voice's int16 buffer in place, once per 32-sample block, and sits after the oscillator/transient sum and before the amp envelope.

INPUTS
1. Original C (preprocessed only for the build configuration USE_SHAPER_NONLINEARITY=0 and ENABLE_NONLINEAR_INTEGRATORS=1: dead #if branches, comments and one unreachable block after a return were removed; nothing else changed):
```c
#define FILTER_GAIN           0x70ff
enum filterTypeEnum
{
  FILTER_LP=1,
  FILTER_HP,
  FILTER_BP,
  FILTER_UNITY_BP,
  FILTER_NOTCH,
  FILTER_PEAK,
  FILTER_NAIVE_2_POLE
};
typedef struct ResoFilterStruct
{
  float f;
  float g;
  float q;
  float s1;
  float s2;
  float a,b;
  float zi;
  float drive;
} ResonantFilter;
void SVF_setReso(ResonantFilter* filter, float feedback)
{
  filter->q = 1-feedback;
  if(filter->q<0.1f)filter->q = 0.02f;
}
void SVF_init(ResonantFilter* filter)
{
    filter->s1 = 0;
    filter->s2 = 0;
    filter->a =filter->b = 0;
    filter->f = 0.20f;
    filter->q = 0.9f;
    filter->drive = 0.5f;
    SVF_directSetFilterValue(filter,0.25f);
}
void SVF_reset(ResonantFilter* filter)
{
  filter->s1 = 0;
  filter->s2 = 0;
  filter->zi = 0;
  filter->a = filter->b = 0;
}
static float fastTanh(float var)
{
   if(var < -1.95f)     return -1.0f;
   else if(var > 1.95f) return  1.0f;
   else          return  4.15f*var/(4.29f+var*var);
}
float fastTan(float x)
{
  float A = -15*x+x*x*x;
  float B = 3*(-5+2*x*x);
  return A/B;
}
void SVF_recalcFreq(ResonantFilter* filter)
{
  filter->g  = fastTan(M_PI * filter->f );
}
void SVF_setDrive(ResonantFilter* filter,uint8_t drive)
{
  filter->drive =  0.4f + (drive/127.f)*(drive/127.f)*6;
}
void SVF_directSetFilterValue(ResonantFilter* filter, float val)
{
  filter->f = val*(0.5f*0.90f);
  filter->g  = fastTan(M_PI * filter->f );
}
float tanhXdX(float x)
{
  float a = x*x;
  x = ((a + 105)*a + 945) / ((15*a + 420)*a + 945);
  return x;
}
float softClipTwo(float in)
{
  return in * tanhXdX(0.5*in);
}
void SVF_calcBlockZDF(ResonantFilter* filter, const uint8_t type, int16_t* buf, const uint8_t size)
{
  uint8_t i;
  const float f   = filter->g;
  const float R   = filter->f >= 0.4499f ? 1 : filter->q;
  const float ff   = f*f;
  if(type == FILTER_NAIVE_2_POLE)
  {
    float f_lp2 = filter->f * 2.21f;
    for(i=0;i<size;i++)
    {
      float x = softClipTwo((buf[i]/((float)0x7fff))*filter->drive);
      float q = (1-filter->q) *1.4 + (1-filter->q) / (1.0 - f_lp2);
      filter->a += f_lp2 * ((x - filter->a)  + q * (filter->a - filter->b ));
      if(filter->a > 1) filter->a = 1;
      else if(filter->a < -1) filter->a = -1;
      filter->b  += f_lp2 * (filter->a - filter->b );
      if(filter->b > 1) filter->b = 1;
      else if(filter->b < -1) filter->b = -1;
      int32_t tmp;
      tmp = (filter->b  * FILTER_GAIN);
      buf[i] = __SSAT(tmp,16);
    }
  } else {
    for(i=0;i<size;i++)
    {
      const float x = softClipTwo((buf[i]/((float)0x7fff))*filter->drive);
      float ih = 0.5f * (x + filter->zi);
      filter->zi = x;
      const float scale = 0.5f;
      const float t0 = tanhXdX(scale* (ih - 2*R*filter->s1 - filter->s2 ) );
      const float t1 = tanhXdX(scale* (filter->s1 ) );
      const float g0 = 1.f / (1.f + f*t0*2*R);
      const float s1 = filter->s1;
      const float s2 = filter->s2;
      const float f1 = ff*g0*t0*t1;
      float y1=(f1*x+s2+f*g0*t1*s1)/(f1+1);
       const float xx = t0*(x - y1);
       const float y0 = (softClipTwo(s1) + f*xx)*g0;
      filter->s1   = softClipTwo(filter->s1) + 2*f*(xx - t0*2*R*y0);
      filter->s2   = (filter->s2)    + 2*f* t1*y0;
      int32_t tmp;
      switch(type)
      {
      default:
        return;
        break;
      case FILTER_LP:
        tmp = fastTanh(y1) * 0x7fff ;
        buf[i] = __SSAT(tmp,16);
        break;
      case FILTER_HP:
      {
        const float ugb = 2*R*y0;
        const float h = x - ugb - y1;
        tmp = h * FILTER_GAIN;
        buf[i] = __SSAT(tmp,16);
      }
        break;
      case FILTER_BP:
        tmp = y0 * FILTER_GAIN;
        buf[i] = __SSAT(tmp,16);
        break;
      case FILTER_UNITY_BP:
      {
        const float ugb = 2*R*y0;
        tmp = ugb * FILTER_GAIN;
        buf[i] = __SSAT(tmp,16);
      }
        break;
      case FILTER_NOTCH:
      {
        const float ugb = 2*R*y0;
        tmp = (x-ugb) * FILTER_GAIN;
        buf[i] = __SSAT(tmp,16);
      }
        break;
      case FILTER_PEAK:
      {
        const float ugb = 2*R*y0;
        const float h = x - ugb - y1;
        tmp = (y1-h) * FILTER_GAIN;
        buf[i] = __SSAT(tmp,16);
      }
        break;
        }
      }
  }
}```
Notes on the original C: `M_PI` is the double 3.14159265358979323846. `__SSAT(v,16)` saturates a signed 32-bit value to [-32768, 32767]. Float-to-int32 assignment truncates toward zero. Several expressions mix double and float on purpose (`0.5*in`, `(1-filter->q) *1.4`, `1.0 - f_lp2`, `M_PI * filter->f`): keep the same double/float types.

2. Parameter rows: none needed. Callers pass already-mapped values (f, q, drive) through the setters; the 0..127 to engine mappings belong to another phase.

3. Dependencies: none.

OUTPUT FILES
- dsp/ResonantFilter.h
- dsp/ResonantFilter.cpp
- dsp/ResonantFilterTest.cpp: test main, see ACCEPTANCE

INTERFACE (must match exactly)
```cpp
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
```
Thread-safety: one instance per voice, used only from the audio thread. No shared state.

BEHAVIOUR
1. Each method is a line-by-line port of the C function named in its comment. `fastTanh` stays a file-local static.
2. `calcBlockZDF` processes `size` samples in place. Types 1..6 use the nonlinear ZDF branch, type 7 (FILTER_NAIVE_2_POLE) the naive 2-pole branch. Any other type value (including 8) takes the `default: return;` path of the original.
3. Engine constants (for context only, this module has no time base): REAL_FS = 44002.7573529412 Hz, block = 32 samples, but `size` is an argument.

CONSTRAINTS
- Keep int16 buffers in and out. Do float math inside exactly as the original.
- No heap allocation, no locks, no logging. No NaN/inf/denormal guards inside this module (the plugin shell will set flush-to-zero; do not change the algorithm).
- Mark the following with `// ORIGINAL QUIRK:` comments: (a) type 8 (or any unknown type) returns from inside the sample loop, so it leaves the buffer unfiltered but still updates s1, s2, zi once for the first sample of each call; (b) R is forced to 1 when f >= 0.4499f; (c) the LP branch scales by 0x7fff, all other branches by FILTER_GAIN; (d) `q` is set to 0.02 when 1-feedback < 0.1.

ACCEPTANCE (test program must print PASS or FAIL per line)
Test input for one block: `in[i] = (int16_t)(((i*7919 + 13) % 2001 - 1000) * 20)` for i = 0..31 (i is int). For each configuration and each type 1..8: `reset()`, set the config, copy the input into two buffers, run `calcBlockZDF(type, buf1, 32)` then `calcBlockZDF(type, buf2, 32)` on the same instance (state carries over). Compare buf2.
Tolerances: each int16 output: PASS if exact, WARN if off by 1 or 2 LSB, FAIL otherwise; sum: +-64; float values: relative 1e-4.

A. Scalar functions
1. fastTan(0.5f) = 0.546296299
2. fastTan(1.0f) = 1.55555558
3. tanhXdX(1.0f) = 0.761594176
4. softClipTwo(1.0f) = 0.924234331
5. softClipTwo(-2.5f) = -1.69656801
6. setDrive(0) -> drive = 0.400000006; setDrive(64) -> 1.923715; setDrive(127) -> 6.4000001
7. setReso(0.0f) -> q = 1; setReso(0.5f) -> 0.5; setReso(0.95f) -> 0.0199999996
8. directSetFilterValue(0.5f) -> f = 0.224999994, g = 0.853991151; directSetFilterValue(1.0f) -> f = 0.449999988, g = 6.10959816
9. init() -> f = 0.112499997, g = 0.368918985, q = 0.899999976, drive = 0.5, s1 = s2 = 0

B. Config C1: directSetFilterValue(0.5f), setReso(0.5f), setDrive(64)
type | out[0] out[1] out[15] out[31] | sum
1 LP    | 570 -12159 -7734 15494 | 112027
2 HP    | -15626 32767 23 -444 | 3556
3 BP    | -15109 2238 -1660 -1888 | 4711
4 UBP   | -15109 2238 -1660 -1888 | 4711
5 NOTCH | -15106 26518 -7135 14585 | 133871
6 PEAK  | 16147 -32768 -7183 15474 | 111654
7 LP2   | 3897 -199 -9334 19044 | 99708
8 OFF   | -19740 18580 -5220 7600 | 85900
State after block 1 / block 2, type 1: s1 = -0.0784097761 / -0.0784097239, s2 = 0.46384269 / 0.46384263, zi = 0.438929796 / 0.438929796
State after block 1 / block 2, type 7: a = 0.505028367 / 0.505154192, b = 0.658275604 / 0.658368826 (s1 = s2 = zi = 0)
Type 8 output equals the input unchanged; state after block 2: s1=-0.157155514 s2=-1.25570238 zi=-1.04454124

C. Config C3 (tests the R = 1 branch): directSetFilterValue(1.0f), setReso(0.5f), setDrive(64)
type | out[0] out[1] out[15] out[31] | sum
1 LP    | -18742 6509 -8738 13435 | 125667
2 HP    | 526 2785 362 -1682 | -1088
3 BP    | -5977 9986 -518 778 | -327
4 UBP   | -11955 19972 -1037 1557 | -657
5 NOTCH | -18259 8784 -7758 11139 | 134968
6 PEAK  | -19312 3214 -8483 14505 | 115688
7 LP2   | -28703 7907 -10245 11645 | 92243
8 OFF   | -19740 18580 -5220 7600 | 85900
State after block 1 / block 2, type 1: s1 = -0.319866836 / -0.319862872, s2 = 0.603727698 / 0.603718877, zi = 0.438929796 / 0.438929796
State after block 1 / block 2, type 7: a = 0.402049124 / 0.402049124, b = 0.402584493 / 0.402584493
Type 8 output equals the input unchanged; state after block 2: s1=0.360988557 s2=-0.873997808 zi=-1.04454124

D. Silence: after reset() with config C1 and an all-zero input, both blocks stay all zero for every type 1..8.

Expected values come from the original C compiled with gcc -O0 -ffp-contract=off, not from you. Do not adjust them to make tests pass.

OUT OF SCOPE
Mapping 0..127 parameters to cutoff/resonance/drive, filter type selection, envelopes, oscillators, distortion, voices, modulation, sample-rate conversion, SIMD, and any filter algorithm other than the one given.
