#pragma once
/* Emulates the ARM Cortex-M4 VCVT.U32.F32 instruction (round toward zero, saturating: negative and NaN -> 0, too large -> 0xFFFFFFFF).
   arm-none-eabi-gcc 13.2 with the firmware flags emits vcvt.u32.f32 for every (uint32_t)float cast in Oscillator.c. */
#include <stdint.h>
static inline uint32_t f2u_sat(float x){ if(!(x>0.f)) return 0u; if(x>=4294967296.0f) return 0xFFFFFFFFu; return (uint32_t)x; }
