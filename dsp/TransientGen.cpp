// FILE: dsp/TransientGen.cpp
#include "TransientGen.h"
#include "TransientTables.h"
#include <cstring>

namespace lxr {

void TransientGen::init() {
    pitch = 1.0f;
    output = 0;
    phase = 0;
    waveform = 0;
    volume = 1.0f;
}

void TransientGen::trigger() {
    phase = 0;
}

void TransientGen::setWaveform(uint8_t wf) {
    // ORIGINAL QUIRK: NUM_TRANSIENTS (12) + 2 = 14. Values >= 14 clamp to 0.
    if (wf < 14) {
        waveform = wf;
    } else {
        waveform = 0;
    }
}

void TransientGen::calc() {
    // ORIGINAL QUIRK: Exact C float promotion behavior. 
    // phase is converted to float, added to float increment, then cast back to uint32_t.
    // This causes precision loss for phase > ~16.7M, matching original firmware.
    float increment = static_cast<float>(phase < 2311061504u) * (pitch * 1048576.0f);
    phase = static_cast<uint32_t>(static_cast<float>(phase) + increment);
    
    uint32_t phase_idx = phase >> 20;
    
    if (waveform == 0) {
        output = static_cast<int16_t>(volume * 32512.0f * gTransientTables.volumeTable[phase_idx >> 5]);
    } else {
        // ORIGINAL QUIRK: potential out-of-bounds read if phase_idx >= 2205 due to pitch overshoot.
        // Original C evaluates array access before the bounds check multiplication.
        output = static_cast<int16_t>(volume * (gTransientTables.data[(waveform - 1) * 2205 + phase_idx] << 8) * gTransientTables.volumeTable[phase_idx >> 5]);
    }
}

void TransientGen::calcBlock(int16_t* buf, uint8_t size) {
    if (waveform <= 1) {
        std::memset(buf, 0, size * sizeof(int16_t));
        return;
    }

    for (uint8_t i = 0; i < size; i++) {
        uint32_t phase_idx = phase >> 20;
        
        // ORIGINAL QUIRK: potential out-of-bounds read if phase_idx >= 2205.
        // Original C evaluates array access before the bounds check multiplication.
        buf[i] = static_cast<int16_t>(volume * (gTransientTables.data[(waveform - 2) * 2205 + phase_idx] << 8) * (phase_idx < 2205));
        
        // ORIGINAL QUIRK: Exact C float promotion behavior for phase accumulation.
        float increment = static_cast<float>(phase < 2311061504u) * (pitch * 1048576.0f);
        phase = static_cast<uint32_t>(static_cast<float>(phase) + increment);
    }
}

} // namespace lxr