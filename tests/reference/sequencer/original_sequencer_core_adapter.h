#ifndef P16B_ORIGINAL_SEQUENCER_CORE_ADAPTER_H
#define P16B_ORIGINAL_SEQUENCER_CORE_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t p16b_original_step_size(void);
uint32_t p16b_original_step_offset(uint32_t field);
void p16b_original_default_step(uint32_t position, uint8_t output[7]);
uint16_t p16b_original_default_main_gates(void);
uint8_t p16b_original_default_length_code(void);
uint8_t p16b_original_default_rotation(void);
uint8_t p16b_original_default_repeat(void);
uint8_t p16b_original_default_next(uint8_t pattern);
uint8_t p16b_original_owner(uint8_t track);
uint8_t p16b_original_articulation(uint8_t track);
float p16b_original_internal_tick_quarter_ms(uint16_t bpm);

#ifdef __cplusplus
}
#endif

#endif
