// FILE: dsp/Modulation.cpp
#include "Modulation.h"

namespace lxr {

void VelocityModulator::init() {
    destination = 0;
    amount = 0.f;
    modValue = 0.f;
}

void VelocityModulator::updateVelocity(uint8_t velocity) {
    modValue = velocity / 127.0f;
}

void VelocityModulator::applyTo(ModTarget& target) {
    target.applyModulation(modValue, amount);
}

void ModulationSystem::init() {
    for (int i = 0; i < 6; i++) {
        velocityMods[i].init();
    }
}

void ModulationSystem::resetTargets() {
    // Stub: Deferred to P11/P12. Will iterate over active modulators and 
    // reset their target ModTarget::multiplier to 1.0f before each block.
}

void ModulationSystem::updateVelocity(uint8_t voiceNum, uint8_t velocity) {
    if (voiceNum < 6) {
        velocityMods[voiceNum].updateVelocity(velocity);
    }
}

} // namespace lxr