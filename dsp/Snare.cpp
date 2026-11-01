#include "Snare.h"
#include "Oscillator.h"

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
    // ORIGINAL QUIRK: Snare_init passes 2.f to a uint8_t parameter.
    distortion.setShape(2);
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
    // ORIGINAL QUIRK: the firmware leaves the pitched-noise frequency
    // independent of the triggering MIDI note ("TODO noise muss mit
    // transponiert werden").
    
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
    int16_t tonalAndTransient[32];

    // Keep the firmware's operation and conversion order: pitched S&H noise
    // is attenuated before filtering, and the transient is added to this path
    // before the noise/tonal mix is applied.
    calcNoiseBlock(&noiseOsc, buf, size, 0.9f, rng);
    filter.calcBlockZDF(filterType, buf, size);

    transGen.calcBlock(tonalAndTransient, size);
    for (uint8_t i = 0; i < size; ++i) {
        int32_t sum = static_cast<int32_t>(buf[i]) + tonalAndTransient[i];
        if (sum > 32767) sum = 32767;
        else if (sum < -32768) sum = -32768;
        buf[i] = static_cast<int16_t>(sum);
    }

    calcNextOscSampleBlock(&osc, tonalAndTransient, size, 1.0f - mix,
                           tables, rng);

    const float gain = (volumeMod ? velo : 1.0f) * vol * egValueOscVol;
    for (uint8_t i = 0; i < size; ++i) {
        // ORIGINAL QUIRK: compound assignment narrows the noise/transient
        // product to int16 before the saturating tonal addition.
        buf[i] = static_cast<int16_t>(buf[i] * mix);
        int32_t sum = static_cast<int32_t>(buf[i]) + tonalAndTransient[i];
        if (sum > 32767) sum = 32767;
        else if (sum < -32768) sum = -32768;
        buf[i] = static_cast<int16_t>(sum);

        // ORIGINAL QUIRK: SN/CY/HH use one envelope value for the block.
        buf[i] = static_cast<int16_t>(buf[i] * gain);
    }

    distortion.calcBlock(buf, size);
}

} // namespace lxr
