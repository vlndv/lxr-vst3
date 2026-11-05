/*
 * Source-extracted C contract fixture for P16-B. It compiles the pinned
 * firmware's unchanged sequencer.h types and transcribes only seq_resetNote,
 * seq_clearTrack and seq_init defaults. It is not described as a full runtime
 * differential oracle because the hardware sequencer depends on STM32 drivers.
 */
#include "original_sequencer_core_adapter.h"

#include <stddef.h>

#include "sequencer.h"

#ifndef NO_AUTOMATION
#define NO_AUTOMATION 0xffu
#endif

uint32_t p16b_original_step_size(void) { return (uint32_t) sizeof(Step); }

uint32_t p16b_original_step_offset(uint32_t field)
{
    static const uint32_t offsets[7] = {
        (uint32_t) offsetof(Step, volume),
        (uint32_t) offsetof(Step, prob),
        (uint32_t) offsetof(Step, note),
        (uint32_t) offsetof(Step, param1Nr),
        (uint32_t) offsetof(Step, param1Val),
        (uint32_t) offsetof(Step, param2Nr),
        (uint32_t) offsetof(Step, param2Val),
    };
    return field < 7u ? offsets[field] : UINT32_MAX;
}

void p16b_original_default_step(uint32_t position, uint8_t output[7])
{
    output[0] = (uint8_t) (100u | ((position % 8u) == 0u ? STEP_ACTIVE_MASK : 0u));
    output[1] = 127u;
    output[2] = SEQ_DEFAULT_NOTE;
    output[3] = NO_AUTOMATION;
    output[4] = 0u;
    output[5] = NO_AUTOMATION;
    output[6] = 0u;
}

uint16_t p16b_original_default_main_gates(void) { return 0u; }
uint8_t p16b_original_default_length_code(void) { return 0u; }
uint8_t p16b_original_default_rotation(void) { return 0u; }
uint8_t p16b_original_default_repeat(void) { return 0u; }
uint8_t p16b_original_default_next(uint8_t pattern) { return pattern; }
uint8_t p16b_original_owner(uint8_t track) { return track < 5u ? track : 5u; }
uint8_t p16b_original_articulation(uint8_t track)
{
    return track == 5u ? 1u : (track == 6u ? 2u : 0u);
}
float p16b_original_internal_tick_quarter_ms(uint16_t bpm)
{
    float delta = (1000.f * 60.f) / bpm;
    delta /= 96.f;
    delta *= 4.f;
    return delta;
}
