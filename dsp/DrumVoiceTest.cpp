// FILE: dsp/DrumVoiceTest.cpp
#include "DrumVoice.h"
#include <cstdio>
#include <cmath>

static int pass = 0, fail = 0;

static void check(const char* name, float actual, float expected, float tol) {
    float diff = std::fabs(actual - expected);
    if (diff <= tol) { printf("PASS %s\n", name); pass++; }
    else { printf("FAIL %s: expected %g, got %g (diff %g)\n", name, expected, actual, diff); fail++; }
}

static void check_u32(const char* name, uint32_t actual, uint32_t expected) {
    if (actual == expected) { printf("PASS %s\n", name); pass++; }
    else { printf("FAIL %s: expected %u, got %u\n", name, expected, actual); fail++; }
}

int main() {
    float mockNoteFreq[128] = {0.f};
    for (int i = 0; i < 128; i++) {
        mockNoteFreq[i] = 440.f * std::pow(2.f, (i - 69) / 12.f);
    }

    lxr::OscTables mockTables;
    lxr::DrumVoice voice;
    voice.init();
    
    check("init vol", voice.vol, 0.8f, 1e-5f);
    check("init fmModAmount", voice.fmModAmount, 0.5f, 1e-5f);
    check("init mixOscs", (float)voice.mixOscs, 1.0f, 0.1f);
    check("init volumeMod", (float)voice.volumeMod, 1.0f, 0.1f);
    
    // Test unconditional reset: even if state is 1 (A) and value is high, it MUST reset.
    voice.osc.waveform = lxr::OSC_SINE;
    voice.oscVolEg.state = 1; 
    voice.oscVolEg.value = 0.5f; 
    voice.trigger(127, 60, mockNoteFreq);
    check("trigger velo", voice.velo, 1.0f, 1e-5f);
    check_u32("trigger sine phase (unconditional)", voice.osc.phase, 1072693248u);
    
    voice.osc.waveform = lxr::OSC_TRI;
    voice.oscVolEg.state = 1;
    voice.trigger(127, 60, mockNoteFreq);
    check_u32("trigger tri phase (unconditional)", voice.osc.phase, 267386880u);
    
    voice.transGen.waveform = 1;
    voice.transGen.volume = 0.5f;
    voice.osc.waveform = lxr::OSC_SINE;
    voice.trigger(127, 60, mockNoteFreq);
    check_u32("trigger offset phase", voice.osc.phase, 536347136u);
    
    voice.oscPitchEg.value = 0.5f;
    voice.oscPitchEg.decay = 0.0f;
    voice.egPitchModAmount = 0.5f;
    voice.calcAsync(mockNoteFreq);
    check("calcAsync pitchMod", voice.osc.pitchMod, 1.25f, 1e-5f);
    check("calcAsync fmMod", voice.osc.fmMod, 0.25f, 1e-5f);
    
    int16_t buf[32] = {0};
    for (int i = 0; i < 32; i++) buf[i] = 1000;
    voice.lastGain = 0.0f;
    voice.targetGain = 1.0f;
    
    try {
        voice.calcSyncBlock(buf, 32, mockTables);
        printf("PASS calcSyncBlock executed without crash\n");
        pass++;
    } catch (...) {
        printf("FAIL calcSyncBlock crashed\n");
        fail++;
    }

    printf("SUMMARY pass=%d fail=%d\n", pass, fail);
    return fail > 0 ? 1 : 0;
}