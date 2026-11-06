#pragma once
#include <cstdint>
#include "Engine.h"

namespace lxr {

constexpr int kNumParams = 228;  // Indices 0-227, 0 unused

class ParameterArray {
public:
    uint8_t values[kNumParams];
    
    void init();
    void set(uint8_t par, uint8_t value, Engine& engine);
    uint8_t get(uint8_t par) const;
    void applyAllToEngine(Engine& engine);
};

// Apply a raw firmware parameter value to the effective Engine only. This does
// not modify ParameterArray and is used by transient sequencer automation.
void ApplyRawParameterToEngine(uint8_t par, uint8_t value, Engine& engine) noexcept;

} // namespace lxr
