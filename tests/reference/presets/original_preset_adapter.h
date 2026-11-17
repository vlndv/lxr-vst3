#pragma once
/* Test-only boundary adapter for unchanged Sonic Potions firmware 0.37.
 * See LICENSE-LXR.txt. No production importer or serializer is provided. */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum P17Format { P17_SOUND, P17_PATTERN, P17_ALL, P17_PERFORMANCE, P17_GLOBALS };
struct P17OriginalContract {
    unsigned sound_count, globals_begin, parameter_count, globals_count;
    unsigned step_size, step_offsets[7], tracks, patterns, positions, version;
    unsigned modulation_targets;
};
struct P17Message { uint8_t status, first, second; };
void p17_original_contract(struct P17OriginalContract* result);
void p17_original_defaults(uint8_t* values, size_t capacity);
size_t p17_original_write(unsigned format, const uint8_t* values, size_t count,
                          const uint8_t name[8], uint8_t* output, size_t capacity,
                          char filename[16]);
int p17_original_read_sound(const uint8_t* data, size_t size, int morph,
                            uint8_t* base, uint8_t* target);
size_t p17_original_messages(struct P17Message* messages, size_t capacity);
unsigned p17_original_target_parameter(unsigned index);
unsigned p17_original_parameter_enum(unsigned id);
#ifdef __cplusplus
}
#endif
