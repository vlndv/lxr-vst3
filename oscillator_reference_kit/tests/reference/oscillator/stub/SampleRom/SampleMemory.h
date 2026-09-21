#pragma once
#include <stdint.h>
typedef struct { uint32_t offset; uint32_t size; } SampleInfo;
static inline int sampleMemory_getNumSamples(void){ return 0; }
static inline SampleInfo sampleMemory_getSampleInfo(uint8_t n){ SampleInfo i={0,0}; (void)n; return i; }
