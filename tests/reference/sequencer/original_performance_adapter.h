#ifndef P16E_ORIGINAL_PERFORMANCE_ADAPTER_H
#define P16E_ORIGINAL_PERFORMANCE_ADAPTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t p16e_original_euclidean(uint8_t length, uint8_t pulses,
                                uint8_t rotation);

typedef struct P16EChainState
{
    uint8_t active;
    uint8_t pending;
    uint8_t bar;
    uint32_t rng;
    uint32_t rng_calls;
} P16EChainState;

void p16e_original_chain_wrap(P16EChainState* state, uint8_t repeat,
                              uint8_t next_code, uint8_t manual_pending);
uint64_t p16e_original_shuffle_boundary(uint64_t ordinal, uint16_t bpm,
                                        uint8_t shuffle,
                                        uint64_t previous_boundary,
                                        uint8_t has_previous);

#ifdef __cplusplus
}
#endif

#endif
