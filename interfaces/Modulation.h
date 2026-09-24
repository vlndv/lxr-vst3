// FILE: dsp/Modulation.h
#pragma once
#include <cstdint>
#include "Lfo.h"

namespace lxr {

// Base + multiplier approach for modulation
// Each modulated parameter has a base value and a multiplier
// Final value = base * multiplier
// Modulators contribute: multiplier *= (1 + amount * (mod - 1))

struct ModTarget {
    float baseValue = 1.0f;
    float multiplier = 1.0f;
    
    void reset() {
        multiplier = 1.0f;
    }
    
    void applyModulation(float modValue, float amount) {
        // ORIGINAL QUIRK: formula from architecture.md
        // target = target x (amount x mod + (1-amount) x 1)
        // Simplified: target = target x (1 + amount * (mod - 1))
        multiplier *= (1.0f + amount * (modValue - 1.0f));
    }
    
    float getValue() const {
        return baseValue * multiplier;
    }
};

struct VelocityModulator {
    uint8_t destination = 0;  // PAR index
    float amount = 0.f;
    float modValue = 0.f;     // velocity/127
    
    void init();
    void updateVelocity(uint8_t velocity);
    void applyTo(ModTarget& target);
};

// Global modulation system
class ModulationSystem {
public:
    VelocityModulator velocityMods[6];  // one per voice
    
    void init();
    void resetTargets();
    void updateVelocity(uint8_t voiceNum, uint8_t velocity);
};

extern ModulationSystem gModulationSystem;

} // namespace lxr