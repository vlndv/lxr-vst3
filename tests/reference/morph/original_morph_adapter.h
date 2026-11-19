#pragma once
/* Test-only boundary for the unchanged Sonic Potions LXR 0.37 morph code.
 * See LICENSE-LXR.txt. This is not production plugin code. */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t p18_original_interpolate(uint8_t source, uint8_t target,
                                 uint8_t amount);
uint8_t p18_original_get_morph_value(uint16_t index, uint8_t amount);
size_t p18_original_run_morph(const uint8_t source[228],
                              const uint8_t target[228], uint8_t amount,
                              struct P17Message* messages, size_t capacity);
size_t p18_original_save_morph(const uint8_t source[228],
                               const uint8_t target[228], uint8_t amount,
                               const uint8_t name[8], uint8_t* output,
                               size_t capacity);
unsigned p18_original_route_id(unsigned family, unsigned voice);

#ifdef __cplusplus
}
#endif
