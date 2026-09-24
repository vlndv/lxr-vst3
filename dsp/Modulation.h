// FILE: dsp/Modulation.h
#pragma once
#include <cstdint>

namespace lxr {

struct ModTarget {
    float baseValue = 1.0f;
    float multiplier = 1.0f;
    
    void reset() {
        multiplier = 1.0f;
    }
    
    void applyModulation(float modValue, float amount) {
        multiplier *= (1.0f + amount * (modValue - 1.0f));
    }
    
    float getValue() const {
        return baseValue * multiplier;
    }
};

struct VelocityModulator {
    uint8_t destination = 0;
    float amount = 0.f;
    float modValue = 0.f;
    
    void init();
    void updateVelocity(uint8_t velocity);
    void applyTo(ModTarget& target);
};

// NOTE: Removed global `gModulationSystem`. 
// ModulationSystem should be instantiated per-plugin-instance in P12 to avoid 
// cross-instance data races in a VST3 host.
struct ModulationSystem {
    VelocityModulator velocityMods[6];
    
    void init();
    void resetTargets(); // Stub: deferred to P11/P12 when PAR routing is wired
    void updateVelocity(uint8_t voiceNum, uint8_t velocity);
};

} // namespace lxr