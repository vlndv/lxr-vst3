#ifndef P16A_ORIGINAL_SEQUENCER_ADAPTER_H
#define P16A_ORIGINAL_SEQUENCER_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum P16AField {
  P16A_VOLUME = 0,
  P16A_PROBABILITY,
  P16A_NOTE,
  P16A_PARAM1_NR,
  P16A_PARAM1_VALUE,
  P16A_PARAM2_NR,
  P16A_PARAM2_VALUE,
  P16A_FIELD_COUNT
};

uint32_t p16a_track_count(void);
uint32_t p16a_pattern_count(void);
uint32_t p16a_position_count(void);
uint32_t p16a_step_size(void);
uint32_t p16a_pattern_setting_size(void);
uint32_t p16a_length_rotate_size(void);
uint32_t p16a_pattern_set_size(void);
uint32_t p16a_temp_pattern_size(void);
uint32_t p16a_step_offset(enum P16AField field);
uint32_t p16a_step_active_mask(void);
uint32_t p16a_step_volume_mask(void);
uint32_t p16a_default_note(void);
uint32_t p16a_no_automation(void);
double p16a_internal_tick_quarter_ms(uint16_t bpm);
double p16a_substep_seconds(uint16_t bpm);
float p16a_shuffle_factor(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif
