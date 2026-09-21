// dsp/Oscillator.h
// Port of DSPAudio/Oscillator.c (LXR firmware 0.37): sine, band-limited wavetables, sample-and-hold noise, crash sample, FM.
#pragma once
#include <cstdint>
#include "OscTables.h"

namespace lxr {

enum OscWave : uint8_t { OSC_SINE = 0, OSC_TRI = 1, OSC_SAW = 2, OSC_REC = 3, OSC_NOISE = 4, OSC_CRASH = 5, OSC_SAMPLE_START = 6 };

constexpr float kRealFs = 44002.7573529412f;   // REAL_FS in config.h (a float constant)

struct OscInfo {                 // same fields as the C struct OscInfo
    int16_t  output = 0;
    uint32_t phaseInc = 0;
    uint32_t phase = 0;
    float    freq = 0.f;         // Hz
    uint8_t  waveform = 0;
    uint8_t  tableOffset = 0;    // overtone table (0..10)
    float    pitchMod = 1.f;
    float    fmMod = 0.f;
    float    modNodeValue = 1.f;
    uint16_t midiFreq = 0;       // high byte coarse, low byte fine
    uint8_t  baseNote = 0;
    uint32_t startPhase = 0;
};

// Replacement for the hardware RNG (GetRngValue). xorshift32; one instance per plugin voice group, audio thread only.
struct OscRng {
    uint32_t state = 0x9E3779B9u;
    uint32_t next() { uint32_t x = state; x ^= x << 13; x ^= x >> 17; x ^= x << 5; state = x; return x; }
};

// float -> uint32 with ARM VCVT.U32.F32 semantics (round toward zero, negative and NaN -> 0, too large -> 0xFFFFFFFF).
uint32_t floatToU32Sat(float x);

uint32_t freq2PhaseIncr(float f);        // 4096-entry sine table  (<<20)
uint32_t freq2PhaseIncr1024(float f);    // wavetables             (<<22)
uint32_t freq2PhaseIncr32767(float f);   // crash sample           (<<17)
uint8_t  freqToTableIndex(float f);      // overtone table for a frequency (before the clamp to 10)

void osc_setFreq(OscInfo* osc);                                          // recompute phaseInc (and tableOffset)
void osc_setBaseNote(OscInfo* osc, uint8_t baseNote, const float* noteFreq);   // noteFreq: 128 floats
void osc_recalcFreq(OscInfo* osc, const float* noteFreq);

// Renders `size` samples into buf: buf[i] = int16(osc output * gain). One call per voice per 32-sample block in the original.
void calcNextOscSampleBlock(OscInfo* osc, int16_t* buf, uint8_t size, float gain, const OscTables& t, OscRng& rng);
// Same with phase modulation by modBuffer (int16, `size` entries). Noise ignores the modulator.
void calcNextOscSampleFmBlock(OscInfo* osc, const int16_t* modBuffer, int16_t* buf, uint8_t size, float gain,
                              const OscTables& t, OscRng& rng);
void calcNoiseBlock(OscInfo* osc, int16_t* buf, uint8_t size, float gain, OscRng& rng);

} // namespace lxr
