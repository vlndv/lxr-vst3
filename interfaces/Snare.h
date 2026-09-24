// FILE: dsp/Snare.h
#pragma once
#include <cstdint>
#include "Oscillator.h"
#include "ResonantFilter.h"
#include "Distortion.h"
#include "Envelopes.h"
#include "TransientGen.h"
#include "Lfo.h"

namespace lxr {

struct SnareVoice {
    OscInfo osc;
    OscInfo noiseOsc;
    uint8_t filterType = 1; // FILTER_LP
    float vol = 0.8f;
    uint8_t pan = 0;
    float mix = 0.5f; // 0 = 100% osc, 1 = 100% noise
    float velo = 0.f;
    int16_t oscSample = 0;
    int32_t noiseSample = 0;
    
    ResonantFilter filter;
    PitchDecayEg oscPitchEg;
    float egPitchModAmount = 0.5f;
    
    Lfo lfo; // STUB for P10
    TransientGen transGen;
    AmpEg oscVolEg;
    float egValueOscVol = 0.f;
    
    Distortion distortion;
    SnapEg snapEg;
    uint8_t volumeMod = 1;
    
    OscRng rng;

    void init();
    void trigger(uint8_t vel, uint8_t note, const float* noteFreq);
    void calcAsync(const float* noteFreq);
    void calcSyncBlock(int16_t* buf, uint8_t size, const OscTables& tables);
    void setPan(uint8_t pan);
};

} // namespace lxr