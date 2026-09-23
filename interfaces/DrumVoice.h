// FILE: dsp/DrumVoice.h
#pragma once
#include <cstdint>
#include "Oscillator.h"
#include "ResonantFilter.h"
#include "Distortion.h"
#include "Envelopes.h"
#include "TransientGen.h"

namespace lxr {

// STUB: LFO is implemented in P10. This minimal struct preserves memory layout.
struct Lfo {
    float phase = 0.f;
    float rate = 0.f;
    uint8_t waveform = 0;
    uint8_t retrigger = 0;
    uint8_t sync = 0;
    uint32_t phaseOffset = 0;
    float amount = 0.f;
};

struct DrumVoice {
    OscInfo osc;
    OscInfo modOsc;
    float fmModAmount = 0.5f;
    float vol = 0.8f;
    float velo = 0.f;
    uint8_t pan = 0;
    int16_t oscSample = 0;
    
    PitchDecayEg oscPitchEg;
    float egPitchModAmount = 0.5f;
    float offset = 0.f;
    
    TransientGen transGen;
    Lfo lfo; // STUB for P10
    AmpEg oscVolEg;
    float egValueOscVol = 0.f;
    float volEgValueBlock[32]; // OUTPUT_DMA_SIZE
    
    Distortion distortion;
    ResonantFilter filter;
    uint8_t filterType = 1; // FILTER_LP
    
    bool mixOscs = true;
    float decimationCnt = 0.f;
    float decimationRate = 1.f;
    SnapEg snapEg;
    
    uint8_t volumeMod = 1;
    
    float lastGain = 0.f;
    float targetGain = 0.f;
    
    OscRng rng; // Per-voice RNG for noise

    void init();
    void trigger(uint8_t vol, uint8_t note, const float* noteFreq);
    void calcAsync(const float* noteFreq);
    void calcSyncBlock(int16_t* buf, uint8_t size, const OscTables& tables);
    void setPan(uint8_t p);
};

} // namespace lxr