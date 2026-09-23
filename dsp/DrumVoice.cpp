// FILE: dsp/DrumVoice.cpp
#include "DrumVoice.h"
#include <cmath>

namespace lxr {

void DrumVoice::init() {
    osc.output = 0;
    osc.phaseInc = 0;
    osc.phase = 0;
    osc.freq = 440.f;
    osc.waveform = OSC_TRI;
    osc.tableOffset = 0;
    osc.pitchMod = 1.f;
    osc.fmMod = 0.f;
    osc.modNodeValue = 1.f;
    osc.midiFreq = 70 << 8;
    osc.baseNote = 0;
    osc.startPhase = 0;

    modOsc = osc;
    modOsc.waveform = OSC_SINE;

    fmModAmount = 0.5f;
    vol = 0.8f;
    velo = 0.f;
    pan = 0;
    oscSample = 0;
    
    oscPitchEg.init();
    egPitchModAmount = 0.5f;
    offset = 0.f;
    
    transGen.init();
    oscVolEg.init();
    egValueOscVol = 0.f;
    for (int i = 0; i < 32; i++) volEgValueBlock[i] = 0.f;
    
    distortion.init();
    distortion.setShape(64);
    
    filter.init();
    filterType = FILTER_LP;
    
    mixOscs = true;
    decimationCnt = 0.f;
    decimationRate = 1.f;
    snapEg.init();
    volumeMod = 1;
    
    lastGain = 0.f;
    targetGain = 0.f;
}

void DrumVoice::trigger(uint8_t vol, uint8_t note, const float* noteFreq) {
    // ORIGINAL QUIRK: USE_AMP_FILTER is never defined in 0.37, so the #else branch is always live.
    // Phase reset is UNCONDITIONAL on every trigger, regardless of envelope state.
    float offsetVal = 1.0f;
    if (transGen.waveform == 1) {
        offsetVal = 1.0f - transGen.volume;
    }
    
    if (osc.waveform == OSC_SINE) {
        osc.phase = 1024 + ((1023u << 20) - 1024) * offsetVal;
    } else if (osc.waveform >= OSC_TRI && osc.waveform <= OSC_REC) {
        osc.phase = (0xffu << 20) * offsetVal;
    } else {
        osc.phase = 0;
    }

    osc_setBaseNote(&osc, note, noteFreq);
    osc_setBaseNote(&modOsc, note, noteFreq);
    
    oscPitchEg.trigger();
    oscVolEg.trigger();
    this->velo = vol / 127.0f;
    
    transGen.trigger();
    snapEg.trigger();
    
    filter.reset();
}

void DrumVoice::calcAsync(const float* noteFreq) {
    float egPitchVal = oscPitchEg.calc();
    float pitchEgValue = egPitchVal * egPitchModAmount;
    osc.pitchMod = 1.0f + pitchEgValue;

    if (transGen.waveform == 0) {
        float snapVal = snapEg.calc(transGen.pitch);
        osc.pitchMod += snapVal * transGen.volume;
    }

    osc.fmMod = fmModAmount * egPitchVal;

    lastGain = targetGain;
    targetGain = oscVolEg.calc();
    
    osc_recalcFreq(&osc, noteFreq);
    osc_recalcFreq(&modOsc, noteFreq);
}

void DrumVoice::calcSyncBlock(int16_t* buf, uint8_t size, const OscTables& tables) {
    int16_t modBuf[32];

    calcNextOscSampleBlock(&modOsc, modBuf, size, fmModAmount, tables, rng);

    if (mixOscs) {
        calcNextOscSampleBlock(&osc, buf, size, 1.0f - fmModAmount, tables, rng);
        for (uint8_t i = 0; i < size; i++) {
            int32_t sum = (int32_t)buf[i] + (int32_t)modBuf[i];
            if (sum > 32767) sum = 32767;
            else if (sum < -32768) sum = -32768;
            buf[i] = (int16_t)sum;
        }
    } else {
        calcNextOscSampleFmBlock(&osc, modBuf, buf, size, 1.0f, tables, rng);
    }

    transGen.calcBlock(modBuf, size);

    for (uint8_t i = 0; i < size; i++) {
        int32_t sum = (int32_t)buf[i] + (int32_t)modBuf[i];
        if (sum > 32767) sum = 32767;
        else if (sum < -32768) sum = -32768;
        buf[i] = (int16_t)sum;
    }

    filter.calcBlockZDF(filterType, buf, size);

    for (uint8_t i = 0; i < size; i++) {
        float frac = i / (float)(size - 1);
        float currentGain = lastGain + frac * (targetGain - lastGain);
        buf[i] = (int16_t)((float)buf[i] * currentGain);
    }

    if (volumeMod) {
        for (uint8_t i = 0; i < size; i++) {
            buf[i] = (int16_t)((float)buf[i] * velo);
        }
    }

    distortion.calcBlock(buf, size);

    for (uint8_t i = 0; i < size; i++) {
        buf[i] = (int16_t)((float)buf[i] * vol);
    }
}

void DrumVoice::setPan(uint8_t p) {
    pan = p;
}

} // namespace lxr