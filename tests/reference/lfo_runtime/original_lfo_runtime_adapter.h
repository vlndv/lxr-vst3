#pragma once
#include "../lfo/original_lfo_adapter.h"
#include "../velocity/original_velocity_adapter.h"
#ifdef __cplusplus
extern "C" {
#endif
void runtime_ref_tick(void);
void runtime_ref_rng(uint32_t seed);
#ifdef __cplusplus
}
#endif
