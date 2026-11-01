/* Host equivalent of the Cortex-M signed saturation intrinsic, test-only. */
#pragma once
#include <stdint.h>
static inline int32_t __SSAT(int32_t value, unsigned bits) {
  const int32_t high = (1 << (bits - 1)) - 1;
  const int32_t low = -(1 << (bits - 1));
  return value > high ? high : value < low ? low : value;
}
