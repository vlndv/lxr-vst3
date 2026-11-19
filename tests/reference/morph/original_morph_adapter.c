/* The established P17-A adapter supplies only bounded host shims. Including it
 * here keeps presetManager.c unchanged and makes its file-local interpolate()
 * available to this P18-A translation unit. */
#include "../presets/original_preset_adapter.c"
#include "original_morph_adapter.h"

uint8_t p18_original_interpolate(uint8_t source, uint8_t target,
                                 uint8_t amount) {
    return interpolate(source, target, amount);
}

uint8_t p18_original_get_morph_value(uint16_t index, uint8_t amount) {
    return preset_getMorphValue(index, amount);
}

size_t p18_original_run_morph(const uint8_t source[228],
                              const uint8_t target[228], uint8_t amount,
                              struct P17Message* output, size_t capacity) {
    memcpy(parameter_values, source, END_OF_SOUND_PARAMETERS);
    memcpy(parameters2, target, END_OF_SOUND_PARAMETERS);
    message_count = 0;
    preset_morph(amount);
    require_adapter(capacity >= message_count);
    memcpy(output, messages, message_count * sizeof(*output));
    return message_count;
}

size_t p18_original_save_morph(const uint8_t source[228],
                               const uint8_t target[228], uint8_t amount,
                               const uint8_t name[8], uint8_t* output,
                               size_t capacity) {
    memcpy(parameter_values, source, END_OF_SOUND_PARAMETERS);
    memcpy(parameters2, target, END_OF_SOUND_PARAMETERS);
    parameter_values[PAR_MORPH] = amount;
    memcpy(preset_currentName, name, 8);
    preset_saveDrumset(7, 1);
    require_adapter(capacity >= io_size);
    memcpy(output, io_bytes, io_size);
    return io_size;
}

unsigned p18_original_route_id(unsigned family, unsigned voice) {
    if (voice >= 6) return 0xffffu;
    if (family == 0) return PAR_VEL_DEST_1 + voice;
    if (family == 1) return PAR_VOICE_LFO1 + voice;
    if (family == 2) return PAR_TARGET_LFO1 + voice;
    return 0xffffu;
}
