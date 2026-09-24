// FILE: dsp/Lfo.h
#pragma once
#include <cstdint>

namespace lxr {

// STUB: LFO is implemented in P10. This minimal struct preserves memory layout.
struct Lfo {
    uint32_t phase = 0;      // ORIGINAL: Phase is uint32, not float (required for bit-shifting/wrapping in P10)
    float rate = 0.f;
    uint8_t waveform = 0;
    uint8_t retrigger = 0;
    uint8_t sync = 0;
    uint32_t phaseOffset = 0;
    float amount = 0.f;
};

} // namespace lxr