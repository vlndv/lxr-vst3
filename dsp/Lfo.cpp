// FILE: dsp/Lfo.cpp
#include "Lfo.h"
#include "Oscillator.h"  // for kRealFs, sine table access
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
}

void Lfo::setRate(float hz) {
    // phaseInc = hz / REAL_FS * 2^32
    float normalized = hz / kRealFs;
    phaseInc = static_cast<uint32_t>(normalized * 4294967296.0f);
}

void Lfo::setTempoSync(uint8_t syncValue, float bpm) {
    // ORIGINAL QUIRK: sync values 1..11 map to scalers
    // 1=4/1, 2=2/1, 3=1/1, 4=1/2, 5=1/3, 6=1/4, 7=1/6, 8=1/8, 9=1/12, 10=1/16, 11=1/32
    // UNSURE: exact mapping from source, using architecture.md description
    static const float scalers[] = {0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f, 16.0f, 32.0f};
    
    if (syncValue == 0 || syncValue > 11) {
        phaseInc = 0;
        return;
    }
    
    float barRate = bpm / 60.0f / 4.0f;  // bars per second
    float scaler = scalers[syncValue - 1];
    float hz = barRate * scaler;
    setRate(hz);
}

void Lfo::trigger(uint8_t voiceNum) {
    // ORIGINAL QUIRK: retrigger when voiceNum matches retrigger value
    if (retrigger == voiceNum) {
        phase = phaseOffset;
    }
}

void Lfo::tick() {
    lastPhase = phase;
    phase += phaseInc;
    
    // Generate waveform output (0..1 unipolar)
    float phaseFrac = phase / 4294967296.0f;  // 0..1
    
    switch (waveform) {
        case LFO_SINE: {
            // Use sine table, convert from bipolar (-1..1) to unipolar (0..1)
            // ORIGINAL QUIRK: need to access sine table - using simple sin() for now
            // UNSURE: should use OscTables.sine for bit-identical output
            float bipolar = std::sin(phaseFrac * 6.28318530718f);
            output = (bipolar + 1.0f) * 0.5f;
            break;
        }
        case LFO_TRI: {
            // Triangle: 0..1..0..-1..0 mapped to 0..1
            float bipolar;
            if (phaseFrac < 0.25f) {
                bipolar = phaseFrac * 4.0f;
            } else if (phaseFrac < 0.75f) {
                bipolar = 1.0f - (phaseFrac - 0.25f) * 4.0f;
            } else {
                bipolar = -1.0f + (phaseFrac - 0.75f) * 4.0f;
            }
            output = (bipolar + 1.0f) * 0.5f;
            break;
        }
        case LFO_SAW_UP: {
            output = phaseFrac;
            break;
        }
        case LFO_SAW_DOWN: {
            output = 1.0f - phaseFrac;
            break;
        }
        case LFO_SQUARE: {
            output = (phaseFrac < 0.5f) ? 1.0f : 0.0f;
            break;
        }
        case LFO_RANDOM: {
            // S&H: hold value until phase wraps
            if (phase < lastPhase) {  // phase wrapped
                // ORIGINAL QUIRK: need random value - using simple PRNG
                // UNSURE: should match original GetRngValue() behavior
                heldValue = static_cast<float>(rand() % 65536) / 65535.0f;
            }
            output = heldValue;
            break;
        }
        case LFO_EXP_UP: {
            // Exponential up: x^3
            output = phaseFrac * phaseFrac * phaseFrac;
            break;
        }
        case LFO_EXP_DOWN: {
            // Exponential down: inverted x^3
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
    // ORIGINAL QUIRK: (v+1)/128, not v/127
    float normalized = (v + 1) / 128.0f;
    return normalized * normalized * normalized * 200.0f;
}

uint32_t lfoPhaseOffsetFromMidi(uint8_t v) {
    // ORIGINAL QUIRK: v=127 gives 2^32, out of range, ARM saturates to 0xFFFFFFFF
    float normalized = v / 127.0f;
    float product = normalized * 4294967296.0f;
    
    // ARM float-to-uint32 saturation behavior
    if (product >= 4294967296.0f) {
        return 0xFFFFFFFFu;
    }
    return static_cast<uint32_t>(product);
}

} // namespace lxr