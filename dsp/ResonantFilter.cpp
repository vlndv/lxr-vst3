// dsp/ResonantFilter.cpp
#include "ResonantFilter.h"
#include <cmath>
#include <algorithm>

namespace lxr {

static constexpr double kPi = 3.14159265358979323846; // same value as M_PI (M_PI is not defined by default on MSVC)

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

void ResonantFilter::init()
{
    s1 = 0;
    s2 = 0;
    a = b = 0;
    f = 0.20f;
    q = 0.9f;
    drive = 0.5f;
    directSetFilterValue(0.25f);
}

void ResonantFilter::reset()
{
  s1 = 0;
  s2 = 0;
  zi = 0;
  a = b = 0;
}

void ResonantFilter::setReso(float feedback)
{
  q = 1-feedback;
  if(q<0.1f)q = 0.02f; // ORIGINAL QUIRK: q is set to 0.02 when 1-feedback < 0.1
}

void ResonantFilter::setDrive(uint8_t drive)
{
  this->drive =  0.4f + (drive/127.f)*(drive/127.f)*6;
}

void ResonantFilter::directSetFilterValue(float val)
{
  f = val*(0.5f*0.90f);
  g  = fastTan(kPi * f );
}

void ResonantFilter::recalcFreq()
{
  g  = fastTan(kPi * f );
}

void ResonantFilter::calcBlockZDF(uint8_t type, int16_t* buf, uint8_t size)
{
  uint8_t i;
  const float f   = this->g;
  const float R   = this->f >= 0.4499f ? 1 : this->q; // ORIGINAL QUIRK: R is forced to 1 when f >= 0.4499f
  const float ff   = f*f;
  if(type == FILTER_NAIVE_2_POLE)
  {
    float f_lp2 = this->f * 2.21f;
    for(i=0;i<size;i++)
    {
      float x = softClipTwo((buf[i]/((float)0x7fff))*drive);
      float q = (1-this->q) *1.4 + (1-this->q) / (1.0 - f_lp2);
      this->a += f_lp2 * ((x - this->a)  + q * (this->a - this->b ));
      if(this->a > 1) this->a = 1;
      else if(this->a < -1) this->a = -1;
      this->b  += f_lp2 * (this->a - this->b );
      if(this->b > 1) this->b = 1;
      else if(this->b < -1) this->b = -1;
      int32_t tmp;
      tmp = (this->b  * kFilterGain);
      buf[i] = std::clamp(tmp, -32768, 32767);
    }
  } else {
    for(i=0;i<size;i++)
    {
      const float x = softClipTwo((buf[i]/((float)0x7fff))*drive);
      float ih = 0.5f * (x + zi);
      zi = x;
      const float scale = 0.5f;
      const float t0 = tanhXdX(scale* (ih - 2*R*s1 - s2 ) );
      const float t1 = tanhXdX(scale* (s1 ) );
      const float g0 = 1.f / (1.f + f*t0*2*R);
      const float s1 = this->s1;
      const float s2 = this->s2;
      const float f1 = ff*g0*t0*t1;
      float y1=(f1*x+s2+f*g0*t1*s1)/(f1+1);
       const float xx = t0*(x - y1);
       const float y0 = (softClipTwo(s1) + f*xx)*g0;
      this->s1   = softClipTwo(this->s1) + 2*f*(xx - t0*2*R*y0);
      this->s2   = (this->s2)    + 2*f* t1*y0;
      int32_t tmp;
      switch(type)
      {
      default:
        return;
        break;
      case FILTER_LP:
        tmp = fastTanh(y1) * 0x7fff ; // ORIGINAL QUIRK: LP branch scales by 0x7fff
        buf[i] = std::clamp(tmp, -32768, 32767);
        break;
      case FILTER_HP:
      {
        const float ugb = 2*R*y0;
        const float h = x - ugb - y1;
        tmp = h * kFilterGain;
        buf[i] = std::clamp(tmp, -32768, 32767);
      }
        break;
      case FILTER_BP:
        tmp = y0 * kFilterGain;
        buf[i] = std::clamp(tmp, -32768, 32767);
        break;
      case FILTER_UNITY_BP:
      {
        const float ugb = 2*R*y0;
        tmp = ugb * kFilterGain;
        buf[i] = std::clamp(tmp, -32768, 32767);
      }
        break;
      case FILTER_NOTCH:
      {
        const float ugb = 2*R*y0;
        tmp = (x-ugb) * kFilterGain;
        buf[i] = std::clamp(tmp, -32768, 32767);
      }
        break;
      case FILTER_PEAK:
      {
        const float ugb = 2*R*y0;
        const float h = x - ugb - y1;
        tmp = (y1-h) * kFilterGain;
        buf[i] = std::clamp(tmp, -32768, 32767);
      }
        break;
        }
      }
  }
}

} // namespace lxr
