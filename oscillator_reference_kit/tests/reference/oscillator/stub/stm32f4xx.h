#pragma once
#include <stdint.h>
#include <stdio.h>
static inline uint8_t __CLZ(uint32_t v){ return v ? (uint8_t)__builtin_clz(v) : 32; }
static inline int32_t __SSAT(int32_t v,int b){int32_t hi=(1<<(b-1))-1,lo=-(1<<(b-1));return v>hi?hi:(v<lo?lo:v);}
typedef union { struct { uint32_t _reserved0:16; uint32_t GE:4; uint32_t _reserved1:7; uint32_t Q:1; uint32_t V:1; uint32_t C:1; uint32_t Z:1; uint32_t N:1; } b; uint32_t w; } APSR_Type;
static inline uint32_t __get_APSR(void){ return 0; }
