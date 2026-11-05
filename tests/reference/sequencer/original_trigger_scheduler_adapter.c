/*
 * P16-C source-derived scheduler oracle.
 *
 * The pinned firmware's full sequencer.c cannot be linked without STM32 clock,
 * UART, trigger, MIDI, automation and front-panel services. This adapter
 * compiles the unchanged pinned sequencer.h for constants/Step layout and
 * transcribes only sequencer.c:488-543 plus seq_setStepIndexToStart:1468-1485.
 * It is therefore labelled source-derived, not an execution of the whole
 * hardware sequencer. The injected xorshift stream is the documented plugin
 * adaptation replacing hardware entropy; decision/order logic remains C.
 */
#include "original_trigger_scheduler_adapter.h"

#include <string.h>

#include "sequencer.h"

static uint32_t p16c_next_rng(P16CReferenceState* state)
{
    uint32_t value = state->rng_state;
    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    state->rng_state = value;
    ++state->rng_calls;
    return value;
}
void p16c_reference_init(P16CReferenceState* state,
                         const uint8_t rotations[P16C_TRACKS],
                         const uint8_t length_codes[P16C_TRACKS],
                         uint32_t seed)
{
    size_t track;
    memset(state, 0, sizeof(*state));
    state->rng_state = seed;
    for (track = 0; track < P16C_TRACKS; ++track)
    {
        uint8_t length = length_codes[track];
        uint8_t rotation = rotations[track];
        if (length != 0u && rotation > length)
            rotation = (uint8_t) (rotation % length);
        state->positions[track] = (int16_t) (rotation * 8u) - 1;
    }
}

size_t p16c_reference_tick(
    P16CReferenceState* state,
    const uint8_t steps[P16C_TRACKS][P16C_POSITIONS][P16C_STEP_BYTES],
    const uint16_t gates[P16C_TRACKS],
    const uint8_t length_codes[P16C_TRACKS],
    P16CReferenceEvent* events,
    size_t capacity,
    size_t* rejected)
{
    size_t written = 0;
    size_t track;
    if (rejected != NULL)
        *rejected = 0;
    for (track = 0; track < P16C_TRACKS; ++track)
    {
        uint8_t length = length_codes[track];
        int16_t next;
        uint8_t position;
        uint8_t main_step;
        uint8_t substep;
        const uint8_t* step;
        if (length == 0u)
            length = 16u;
        next = (int16_t) (state->positions[track] + 1);
        if (next >= (int16_t) (length * 8u) || next >= P16C_POSITIONS)
            next = 0;
        state->positions[track] = next;
        position = (uint8_t) next;
        main_step = (uint8_t) (position / 8u);
        substep = (uint8_t) (position % 8u);
        if ((gates[track] & (uint16_t) (1u << main_step)) == 0u)
            continue;
        step = steps[track][position];
        if ((step[0] & STEP_ACTIVE_MASK) == 0u)
            continue;
        if (substep == 0u)
            state->random_latches[track] = (uint8_t) (p16c_next_rng(state) & 0x7fu);
        if (state->random_latches[track] > step[1])
            continue;
        if (written >= capacity)
        {
            if (rejected != NULL)
                ++*rejected;
            continue;
        }
        events[written].track = (uint8_t) track;
        events[written].owner = track < 5u ? (uint8_t) track : 5u;
        events[written].articulation = track == 5u ? 1u : (track == 6u ? 2u : 0u);
        events[written].position = position;
        events[written].main_step = main_step;
        events[written].substep = substep;
        events[written].velocity = (uint8_t) (step[0] & STEP_VOLUME_MASK);
        events[written].note = step[2];
        events[written].random_value = state->random_latches[track];
        ++written;
    }
    return written;
}
