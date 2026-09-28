// FILE: dsp/Snare.cpp
#include "Snare.h"

namespace lxr {

void SnareVoice::calcSyncBlock(int16_t* buf, uint8_t size, const OscTables& tables) {
    // ORIGINAL STUB: Reverted to a minimal compiling state to isolate VST3 shell issues.
    // TODO: Implement full snare chain: noise -> filter, +transient, mix with tonal osc, x velocity x vol x ampEG, distortion.
    for (uint8_t i = 0; i < size; ++i) {
        buf[i] = 0;
    }
}

} // namespace lxr