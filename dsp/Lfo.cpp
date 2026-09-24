// FILE: dsp/Lfo.cpp
#include "Lfo.h"
#include <cmath>

namespace lxr {

void Lfo::init() {
    phase = 0;
    phaseInc = 0;
    waveform = LFO_SINE;
    retrigger = 0;
    sync = 0;
    phaseOffset = 0;
    amount = 0.f;
    output = 0.f;
    heldValue = 0.f;
    lastPhase = 0;
    // Seed PRNG with a non-zero constant (can be XORed with voice index later if needed)
    rng.state = 0x9E3779B9u;
}

void Lfo::setRate(float hz) {
    float normalized = hz / kRealFs;
    phaseInc = static_cast<uint32_t>(normalized * 4294967296.0f);
}

void Lfo::setTempoSync(uint8_t syncValue, float bpm) {
    // ORIGINAL: sync == 0 falls back to free-running freq (handled by setRate).
    // We do NOT zero phaseInc here, to avoid freezing free-running LFOs.
    if (syncValue == 0) return; 
    if (syncValue > 11) syncValue = 11;
    
    static const float scalers[] = {0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f, 16.0f, 32.0f};
    float barRate = bpm / 60.0f / 4.0f;
    float hz = barRate * scalers[syncValue - 1];
    setRate(hz);
}

void Lfo::trigger(uint8_t voiceNum) {
    if (retrigger == voiceNum) {
        phase = phaseOffset;
    }
}

void Lfo::tick() {
    lastPhase = phase;
    phase += phaseInc;
    
    float phaseFrac = phase / 4294967296.0f;
    
    switch (waveform) {
        case LFO_SINE: {
            // UNSURE: Uses std::sin() instead of OscTables.sine for simplicity. 
            // Should verify bit-identical output against original in P13.
            float bipolar = std::sin(phaseFrac * 6.28318530718f);
            output = (bipolar + 1.0f) * 0.5f;
            break;
        }
        case LFO_TRI: {
            float bipolar;
            if (phaseFrac < 0.25f) bipolar = phaseFrac * 4.0f;
            else if (phaseFrac < 0.75f) bipolar = 1.0f - (phaseFrac - 0.25f) * 4.0f;
            else bipolar = -1.0f + (phaseFrac - 0.75f) * 4.0f;
            output = (bipolar + 1.0f) * 0.5f;
            break;
        }
        case LFO_SAW_UP:
            output = phaseFrac;
            break;
        case LFO_SAW_DOWN:
            output = 1.0f - phaseFrac;
            break;
        case LFO_SQUARE:
            output = (phaseFrac < 0.5f) ? 1.0f : 0.0f;
            break;
        case LFO_RANDOM: {
            if (phase < lastPhase) {
                // ORIGINAL QUIRK: Replaced unsafe global rand() with per-instance xorshift32
                heldValue = static_cast<float>(rng.next() & 0xFFFF) / 65535.0f;
            }
            output = heldValue;
            break;
        }
        case LFO_EXP_UP:
            output = phaseFrac * phaseFrac * phaseFrac;
            break;
        case LFO_EXP_DOWN: {
            float inv = 1.0f - phaseFrac;
            output = inv * inv * inv;
            break;
        }
        default:
            output = 0.0f;
    }
}

float Lfo::getOutput() {
    return output;
}

float lfoFrequencyFromMidi(uint8_t v) {
    float normalized = (v + 1) / 128.0f;
    return normalized * normalized * normalized * 200.0f;
}

uint32_t lfoPhaseOffsetFromMidi(uint8_t v) {
    float normalized = v / 127.0f;
    float product = normalized * 4294967296.0f;
    if (product >= 4294967296.0f) return 0xFFFFFFFFu;
    return static_cast<uint32_t>(product);
}

} // namespace lxr