/*
 * P16-A source-contract adapter.
 *
 * This compiles the pinned firmware's actual sequencer.h and reports its
 * constants and ABI. Timing helpers transcribe short expressions from the
 * audited source and are deliberately not described as a differential oracle.
 */
#include "original_sequencer_adapter.h"

#include <stddef.h>

#include "sequencer.h"

#ifndef NO_AUTOMATION
#define NO_AUTOMATION 0xffu
#endif

static const float kShuffle[16] = {
    0.f,       0.015625f, 0.0625f,   0.140625f,
    0.25f,    0.390625f, 0.5625f,   0.765625f,
    1.f,      0.984375f, 0.9375f,   0.859375f,
    0.75f,    0.609375f, 0.4375f,   0.234375f,
};

uint32_t p16a_track_count(void) { return NUM_TRACKS; }
uint32_t p16a_pattern_count(void) { return NUM_PATTERN; }
uint32_t p16a_position_count(void) { return NUM_STEPS; }
uint32_t p16a_step_size(void) { return (uint32_t) sizeof(Step); }
uint32_t p16a_pattern_setting_size(void) { return (uint32_t) sizeof(PatternSetting); }
uint32_t p16a_length_rotate_size(void) { return (uint32_t) sizeof(LengthRotate); }
uint32_t p16a_pattern_set_size(void) { return (uint32_t) sizeof(PatternSet); }
uint32_t p16a_temp_pattern_size(void) { return (uint32_t) sizeof(TempPattern); }
uint32_t p16a_step_offset(enum P16AField field) {
  static const uint32_t offsets[P16A_FIELD_COUNT] = {
      (uint32_t) offsetof(Step, volume),
      (uint32_t) offsetof(Step, prob),
      (uint32_t) offsetof(Step, note),
      (uint32_t) offsetof(Step, param1Nr),
      (uint32_t) offsetof(Step, param1Val),
      (uint32_t) offsetof(Step, param2Nr),
      (uint32_t) offsetof(Step, param2Val),
  };
  return field < P16A_FIELD_COUNT ? offsets[field] : UINT32_MAX;
}
uint32_t p16a_step_active_mask(void) { return STEP_ACTIVE_MASK; }
uint32_t p16a_step_volume_mask(void) { return STEP_VOLUME_MASK; }
uint32_t p16a_default_note(void) { return SEQ_DEFAULT_NOTE; }
uint32_t p16a_no_automation(void) { return NO_AUTOMATION; }
double p16a_internal_tick_quarter_ms(uint16_t bpm) {
  return bpm == 0 ? 0.0 : ((1000.0 * 60.0) / (double) bpm / 96.0) * 4.0;
}
double p16a_substep_seconds(uint16_t bpm) {
  return bpm == 0 ? 0.0 : 60.0 / ((double) bpm * 32.0);
}
float p16a_shuffle_factor(uint32_t index) { return kShuffle[index & 15u]; }
