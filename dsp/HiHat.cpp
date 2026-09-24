// FILE: dsp/HiHat.cpp
#include "HiHat.h"

namespace lxr {

void HiHatVoice::init() {
    snapEg.init();
    pan = 0;
    vol = 0.8f;
    
    transGen.init();
    fmModAmount1 = 0.5f;
    fmModAmount2 = 0.5f;
    
    distortion.init();
    distortion.setShape(64);
    
    modOsc.output = 0; modOsc.phaseInc = 0; modOsc.phase = 0;
    modOsc.freq = 440.f; modOsc.waveform = OSC_SINE; modOsc.fmMod = 0.f;
    modOsc.midiFreq = 70 << 8; modOsc.pitchMod = 1.f; modOsc.modNodeValue = 1.f;
    
    modOsc2.output = 0; modOsc2.phaseInc = 0; modOsc2.phase = 0;
    modOsc2.freq = 440.f; modOsc2.waveform = OSC_NOISE; modOsc2.fmMod = 0.f;
    modOsc2.midiFreq = 70 << 8; modOsc2.pitchMod = 1.f; modOsc2.modNodeValue = 1.f;
    
    osc.output = 0; osc.phaseInc = 0; osc.phase = 0;
    osc.freq = 440.f; osc.waveform = OSC_TRI; osc.fmMod = 1.f;
    osc.midiFreq = 70 << 8; osc.pitchMod = 1.f; osc.modNodeValue = 1.f;
    
    volumeMod = 1;
    oscVolEg.init();
    
    filter.init();
    filterType = FILTER_LP;
}

void HiHatVoice::trigger(uint8_t vel, uint8_t isOpen, uint8_t note, const float* noteFreq) {
    float offset = 1.0f;
    if (transGen.waveform == 1) {
        offset -= transGen.volume;
    }
    
    // ORIGINAL QUIRK: HiHat uses (0x3ff << 20) for SINE
    if (osc.waveform == OSC_SINE) {
        osc.phase = (0x3ffu << 20) * offset;
    } else if (osc.waveform >= OSC_TRI && osc.waveform <= OSC_REC) {
        osc.phase = (0xffu << 20) * offset;
    } else {
        osc.phase = 0;
    }
    
    osc_setBaseNote(&osc, note, noteFreq);
    osc_setBaseNote(&modOsc, note, noteFreq);
    osc_setBaseNote(&modOsc2, note, noteFreq);
    
    this->isOpen = isOpen;
    // ORIGINAL QUIRK: HiHat picks decay per trigger
    oscVolEg.decay = isOpen ? decayOpen : decayClosed;
    
    oscVolEg.trigger();
    velo = vel / 127.0f;
    transGen.trigger();
    snapEg.trigger();
}

void HiHatVoice::calcAsync(const float* noteFreq) {
    // ORIGINAL QUIRK: HiHat applies amp EG as per-block constant
    egValueOscVol = oscVolEg.calc();
    
    // ORIGINAL QUIRK: HiHat only updates osc.pitchMod when transient wave == 0
    if (transGen.waveform == 0) {
        float snapVal = snapEg.calc(transGen.pitch);
        osc.pitchMod = 1.0f + snapVal * transGen.volume;
    }
    
    osc_setFreq(&osc);
    osc_setFreq(&modOsc);
    osc_setFreq(&modOsc2);
}

void HiHatVoice::calcSyncBlock(int16_t* buf, uint8_t size, const OscTables& tables) {
    int16_t mod1[32];
    int16_t mod2[32];
    
    // 1. Calc mod oscs
    calcNextOscSampleBlock(&modOsc, mod1, size, fmModAmount1, tables, rng);
    calcNextOscSampleBlock(&modOsc2, mod2, size, fmModAmount2, tables, rng);
    
    // 2. Combine mod oscs (saturating)
    for (uint8_t i = 0; i < size; i++) {
        int32_t sum = (int32_t)mod1[i] + (int32_t)mod2[i];
        if (sum > 32767) sum = 32767;
        else if (sum < -32768) sum = -32768;
        mod1[i] = (int16_t)sum;
    }
    
    // 3. Phase-modulate main osc (fmMod fixed 1, gain 0.5 for HiHat)
    // ORIGINAL QUIRK: HiHat uses gain 0.5, Cymbal uses 1.0
    calcNextOscSampleFmBlock(&osc, mod1, buf, size, 0.5f, tables, rng);
    
    // 4. Filter
    filter.calcBlockZDF(filterType, buf, size);
    
    // 5. Transient
    transGen.calcBlock(mod1, size);
    
    // 6. Mix and Scale
    for (uint8_t i = 0; i < size; i++) {
        float gain = vol * egValueOscVol;
        if (volumeMod) gain *= velo;
        
        int32_t mixed = (int32_t)buf[i] + (int32_t)mod1[i];
        if (mixed > 32767) mixed = 32767;
        else if (mixed < -32768) mixed = -32768;
        
        buf[i] = (int16_t)(mixed * gain);
    }
    
    // 7. Distortion
    distortion.calcBlock(buf, size);
}

void HiHatVoice::setPan(uint8_t p) {
    pan = p;
}

} // namespace lxr