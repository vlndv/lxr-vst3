#pragma once
#include <stdint.h>
#include <string.h>
#define PROGMEM
static inline uint8_t pgm_read_byte(const void* p) {
    uint8_t value;
    memcpy(&value, p, sizeof(value));
    return value;
}
static inline uint16_t pgm_read_word(const void* p) {
    uint16_t value;
    memcpy(&value, p, sizeof(value));
    return value;
}
