/*
 * P16-E original/source-derived adapter.
 *
 * Euclidean results execute the pinned, unchanged EuklidGenerator.c against
 * the real PatternSet layout. Chain and shuffle helpers are narrowly
 * source-derived because the complete sequencer.c depends on STM32 timers,
 * UART, MIDI, front-panel and synthesis services.
 */
#include "original_performance_adapter.h"

#include <string.h>

#include "EuklidGenerator.h"
#include "sequencer.h"

PatternSet seq_patternSet;

uint16_t p16e_original_euclidean(uint8_t length, uint8_t pulses,
                                uint8_t rotation)
{
    memset(&seq_patternSet, 0, sizeof(seq_patternSet));
    euklid_init();
    euklid_setLength(0u, length);
    euklid_setSteps(0u, pulses, 0u);
    euklid_setRotation(0u, rotation, 0u);
    return seq_patternSet.seq_mainSteps[0][0];
}

static uint32_t p16e_rng(P16EChainState* state)
{
    uint32_t value = state->rng;
    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    state->rng = value;
    ++state->rng_calls;
    return value;
}

void p16e_original_chain_wrap(P16EChainState* state, uint8_t repeat,
                              uint8_t next_code, uint8_t manual_pending)
{
    uint8_t pending = state->pending;
    ++state->bar;
    if (state->active == pending)
    {
        pending = ((uint16_t) state->bar % ((uint16_t) repeat + 1u)) == 0u
            ? next_code : state->active;
        if (pending >= 8u)
        {
            const uint8_t limit = (uint8_t) (pending - 8u + 2u);
            pending = (uint8_t) (p16e_rng(state) % limit);
        }
        state->pending = pending;
    }
    if (state->active != pending || manual_pending != 0u)
        state->active = pending;
}

uint64_t p16e_original_shuffle_boundary(uint64_t ordinal, uint16_t bpm,
                                        uint8_t shuffle,
                                        uint64_t previous_boundary,
                                        uint8_t has_previous)
{
    static const uint8_t phase[16] = {
        0u, 1u, 4u, 9u, 16u, 25u, 36u, 49u,
        64u, 63u, 60u, 55u, 48u, 39u, 28u, 15u
    };
    const uint64_t step_numerator = UINT64_C(5632353) * 60u;
    const uint64_t step_denominator = UINT64_C(128) * 32u * bpm;
    const uint64_t common_denominator = step_denominator * 1524u;
    const uint64_t numerator =
        step_numerator *
        (ordinal * 1524u + (uint64_t) phase[ordinal & 15u] * shuffle);
    uint64_t ceiling = numerator / common_denominator;
    if ((numerator % common_denominator) != 0u)
        ++ceiling;
    if ((ceiling & 31u) != 0u)
        ceiling += 32u - (ceiling & 31u);
    if (has_previous != 0u && ceiling < previous_boundary + 32u)
        ceiling = previous_boundary + 32u;
    return ceiling;
}
