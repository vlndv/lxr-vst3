// FILE: dsp/Cymbal.h
#pragma once
#include <cstdint>
#include "Oscillator.h"
#include "ResonantFilter.h"
#include "Distortion.h"
#include "Envelopes.h"
#include "TransientGen.h"
#include "Lfo.h"

namespace lxr {

struct CymbalVoice {
    OscInfo osc;
    OscInfo modOsc;
    OscInfo modOsc2;
    float fmModAmount1 = 0.5f;
    float fmModAmount2 = 0.5f;
    
    float vol = 0.8f;
    uint8_t pan = 0;
    float velo = 0.f;
    int32_t noiseSample = 0;
    
    ResonantFilter filter;
    uint8_t filterType = 1; // FILTER_LP
    
    Lfo lfo; // STUB for P10
    TransientGen transGen;
    Distortion distortion;
    
    AmpEg oscVolEg;
    float egValueOscVol = 0.f;
    
    uint8_t volumeMod = 1;
    SnapEg snapEg;
    
    OscRng rng;

    void init();
    void trigger(uint8_t vel, uint8_t note, const float* noteFreq);
    void calcAsync(const float* noteFreq);
    void calcSyncBlock(int16_t* buf, uint8_t size, const OscTables& tables);
    void setPan(uint8_t pan);
};

} // namespace lxr