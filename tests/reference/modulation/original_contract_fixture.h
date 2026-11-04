#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
unsigned lxr_ref_target_count(void);
unsigned lxr_ref_parameter(unsigned index);
unsigned lxr_ref_label(unsigned index);
unsigned lxr_ref_voice(unsigned index); // Original one-based menu voice,0 for off/global.
unsigned lxr_ref_range_start(unsigned voice);
unsigned lxr_ref_range_end(unsigned voice);
unsigned lxr_ref_gap_count(void);
unsigned lxr_ref_gap(unsigned slot);
unsigned lxr_ref_logical_slot(unsigned index); // Original0xff for off/global.
unsigned lxr_ref_remap(unsigned voice, unsigned slot);
void lxr_ref_initialize_stm32_table(void);
unsigned lxr_ref_parameter_type(unsigned parameter); //0xff for null pointer.
int lxr_ref_same_storage(unsigned a, unsigned b);
unsigned lxr_ref_destination_storage_bytes(void);
#ifdef __cplusplus
}
#endif
