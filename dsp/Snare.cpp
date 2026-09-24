// FILE: dsp/Snare.cpp
#include "Snare.h"

namespace lxr {

void SnareVoice::init() {
    snapEg.init();
    pan = 0;
    vol = 0.8f;
    mix = 0.5f;
    
    noiseOsc.output = 0; noiseOsc.phaseInc = 0; noiseOsc.phase = 0;
    noiseOsc.freq = 440.f; noiseOsc.waveform = OSC_NOISE; noiseOsc.fmMod = 0.f;
    noiseOsc.midiFreq = 70 << 8; noiseOsc.pitchMod = 1.f; noiseOsc.modNodeValue = 1.f;
    
    osc.output = 0; osc.phaseInc = 0; osc.phase = 0;
    osc.freq = 440.f; osc.waveform = OSC_TRI; osc.fmMod = 0.f;
    osc.midiFreq = 70 << 8; osc.modNodeValue = 1.f;
    
    distortion.init();
    distortion.setShape(64);
    volumeMod = 1;
    
    transGen.init();
    oscPitchEg.init();
    egPitchModAmount = 0.5f;
    oscVolEg.init();
    
    filter.init();
    filterType = FILTER_LP;
}

void SnareVoice::trigger(uint8_t vel, uint8_t note, const float* noteFreq) {
    float offset = 1.0f;
    if (transGen.waveform == 1) {
        offset -= transGen.volume;
    }
    
    // ORIGINAL QUIRK: Snare uses (0x3ff << 20) for SINE, unlike DrumVoice which uses 1024 + ((1023<<20)-1024)*offset
    if (osc.waveform == OSC_SINE) {
        osc.phase = (0x3ffu << 20) * offset;
    } else if (osc.waveform >= OSC_TRI && osc.waveform <= OSC_REC) {
        osc.phase = (0xffu << 20) * offset;
    } else {
        osc.phase = 0;
    }

    oscPitchEg.trigger();
    oscVolEg.trigger();
    velo = vel / 127.0f;
    
    osc_setBaseNote(&osc, note, noteFreq);
    osc_setBaseNote(&noiseOsc, note, noteFreq);
    
    transGen.trigger();
    snapEg.trigger();
}

void SnareVoice::calcAsync(const float* noteFreq) {
    float egPitchVal = oscPitchEg.calc();
    float pitchEgValue = egPitchVal * egPitchModAmount;
    osc.pitchMod = 1.0f + pitchEgValue;
    
    egValueOscVol = oscVolEg.calc();
    
    if (transGen.waveform == 0) {
        float snapVal = snapEg.calc(transGen.pitch);
        osc.pitchMod += snapVal * transGen.volume;
    }
    
    osc_setFreq(&osc);
    osc_setFreq(&noiseOsc);
}

void SnareVoice::calcSyncBlock(int16_t* buf, uint8_t size, const OscTables& tables) {
    int16_t transBuf[32];
    
    // 1. Noise -> Filter
    calcNoiseBlock(&noiseOsc, buf, size, 0.9f, rng); // Note: rng needs to be added to struct or passed, using a local stub for now or assuming OscRng is available. Wait, calcNoiseBlock takes OscRng&. I'll add OscRng to SnareVoice.
    // Correction: I will add OscRng to SnareVoice struct.
}

} // namespace lxr