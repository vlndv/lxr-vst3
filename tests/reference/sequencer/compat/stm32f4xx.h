#ifndef P16A_COMPAT_STM32F4XX_H
#define P16A_COMPAT_STM32F4XX_H

#include <stdint.h>

static inline uint32_t __CLZ(uint32_t value)
{
    return value == 0u ? 32u : (uint32_t) __builtin_clz(value);
}

#endif
