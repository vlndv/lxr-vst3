// FILE: dsp/SnareCymbalHiHatTest.cpp
#include "Snare.h"
#include "Cymbal.h"
#include "HiHat.h"
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

static void check_u8(const char* name, uint8_t actual, uint8_t expected) {
    if (actual == expected) { printf("PASS %s\n", name); pass++; }
    else { printf("FAIL %s: expected %u, got %u\n", name, expected, actual); fail++; }
}

int main() {
    float mockNoteFreq[128] = {0.f};
    for (int i = 0; i < 128; i++) {
        mockNoteFreq[i] = 440.f * std::pow(2.f, (i - 69) / 12.f);
    }
    
    // Provide valid mock table data to avoid null pointer crashes
    static int16_t mockSine[4096] = {0};
    static int16_t mockSaw[11 * 1024] = {0};
    static int16_t mockTri[11 * 1024] = {0};
    static int16_t mockRec[11 * 1024] = {0};
    static uint8_t mockCrash[32768] = {127};
    
    lxr::OscTables mockTables;
    mockTables.sine = mockSine;
    mockTables.saw = mockSaw;
    mockTables.tri = mockTri;
    mockTables.rec = mockRec;
    mockTables.crash = mockCrash;
    
    // SNARE TESTS
    printf("--- SNARE ---\n");
    {
        lxr::SnareVoice snare;
        snare.init();
        check("snare init vol", snare.vol, 0.8f, 1e-5f);
        check("snare init mix", snare.mix, 0.5f, 1e-5f);
        check_u8("snare init filterType", snare.filterType, 1);
        check_u8("snare init volumeMod", snare.volumeMod, 1);
        check_u8("snare init noiseOsc waveform", snare.noiseOsc.waveform, 4);
        check_u8("snare init osc waveform", snare.osc.waveform, 1);
    }
    {
        lxr::SnareVoice snare;
        snare.init();
        snare.osc.waveform = lxr::OSC_SINE;
        snare.trigger(127, 60, mockNoteFreq);
        check_u32("snare trigger sine phase", snare.osc.phase, 1072693248u);
        check("snare trigger velo", snare.velo, 1.0f, 1e-5f);
    }
    {
        lxr::SnareVoice snare;
        snare.init();
        snare.osc.waveform = lxr::OSC_TRI;
        snare.trigger(127, 60, mockNoteFreq);
        check_u32("snare trigger tri phase", snare.osc.phase, 267386880u);
    }
    {
        lxr::SnareVoice snare;
        snare.init();
        snare.osc.waveform = lxr::OSC_SAW;
        snare.trigger(127, 60, mockNoteFreq);
        check_u32("snare trigger saw phase", snare.osc.phase, 267386880u);
    }
    {
        lxr::SnareVoice snare;
        snare.init();
        snare.transGen.waveform = 1;
        snare.transGen.volume = 0.5f;
        snare.osc.waveform = lxr::OSC_SINE;
        snare.trigger(127, 60, mockNoteFreq);
        check_u32("snare trigger offset sine phase", snare.osc.phase, 536346624u);
    }
    {
        lxr::SnareVoice snare;
        snare.init();
        snare.oscPitchEg.value = 0.5f;
        snare.oscPitchEg.decay = 0.0f;
        snare.egPitchModAmount = 0.5f;
        snare.calcAsync(mockNoteFreq);
        check("snare calcAsync pitchMod", snare.osc.pitchMod, 1.25f, 1e-5f);
    }
    {
        lxr::SnareVoice snare;
        snare.init();
        int16_t buf[32] = {0};
        snare.calcAsync(mockNoteFreq);
        snare.calcSyncBlock(buf, 32, mockTables);
        printf("PASS snare calcSyncBlock executed\n");
        pass++;
    }
    
    // CYMBAL TESTS
    printf("--- CYMBAL ---\n");
    {
        lxr::CymbalVoice cymbal;
        cymbal.init();
        check("cymbal init vol", cymbal.vol, 0.8f, 1e-5f);
        check("cymbal init fmModAmount1", cymbal.fmModAmount1, 0.5f, 1e-5f);
        check("cymbal init fmModAmount2", cymbal.fmModAmount2, 0.5f, 1e-5f);
        check_u8("cymbal init modOsc waveform", cymbal.modOsc.waveform, 0);
        check_u8("cymbal init modOsc2 waveform", cymbal.modOsc2.waveform, 4);
        check_u8("cymbal init osc waveform", cymbal.osc.waveform, 1);
        check("cymbal init osc fmMod", cymbal.osc.fmMod, 1.0f, 1e-5f);
    }
    {
        lxr::CymbalVoice cymbal;
        cymbal.init();
        cymbal.osc.waveform = lxr::OSC_SINE;
        cymbal.trigger(127, 60, mockNoteFreq);
        check_u32("cymbal trigger sine phase", cymbal.osc.phase, 1072693248u);
        check_u32("cymbal trigger modOsc phase", cymbal.modOsc.phase, 0u);
        check_u32("cymbal trigger modOsc2 phase", cymbal.modOsc2.phase, 0u);
        check("cymbal trigger velo", cymbal.velo, 1.0f, 1e-5f);
    }
    {
        lxr::CymbalVoice cymbal;
        cymbal.init();
        cymbal.osc.waveform = lxr::OSC_TRI;
        cymbal.trigger(127, 60, mockNoteFreq);
        check_u32("cymbal trigger tri phase", cymbal.osc.phase, 267386880u);
    }
    {
        lxr::CymbalVoice cymbal;
        cymbal.init();
        cymbal.transGen.waveform = 1;
        cymbal.transGen.volume = 0.5f;
        cymbal.osc.waveform = lxr::OSC_SINE;
        cymbal.trigger(127, 60, mockNoteFreq);
        check_u32("cymbal trigger offset sine phase", cymbal.osc.phase, 536346624u);
    }
    {
        lxr::CymbalVoice cymbal;
        cymbal.init();
        cymbal.osc.pitchMod = 0.0f;
        cymbal.transGen.waveform = 0;
        cymbal.transGen.pitch = 1.0f;
        cymbal.transGen.volume = 1.0f;
        cymbal.snapEg.value = 1.0f;
        cymbal.calcAsync(mockNoteFreq);
        check("cymbal calcAsync pitchMod updated", cymbal.osc.pitchMod != 0.0f, true, 0.0f);
    }
    {
        lxr::CymbalVoice cymbal;
        cymbal.init();
        int16_t buf[32] = {0};
        cymbal.calcAsync(mockNoteFreq);
        cymbal.calcSyncBlock(buf, 32, mockTables);
        printf("PASS cymbal calcSyncBlock executed\n");
        pass++;
    }
    
    // HIHAT TESTS
    printf("--- HIHAT ---\n");
    {
        lxr::HiHatVoice hat;
        hat.init();
        check("hat init vol", hat.vol, 0.8f, 1e-5f);
        check("hat init fmModAmount1", hat.fmModAmount1, 0.5f, 1e-5f);
        check("hat init fmModAmount2", hat.fmModAmount2, 0.5f, 1e-5f);
        check("hat init decayClosed", hat.decayClosed, 0.01f, 1e-5f);
        check("hat init decayOpen", hat.decayOpen, 0.005f, 1e-5f);
        check_u8("hat init modOsc waveform", hat.modOsc.waveform, 0);
        check_u8("hat init modOsc2 waveform", hat.modOsc2.waveform, 4);
        check_u8("hat init osc waveform", hat.osc.waveform, 1);
        check("hat init osc fmMod", hat.osc.fmMod, 1.0f, 1e-5f);
    }
    {
        lxr::HiHatVoice hat;
        hat.init();
        hat.osc.waveform = lxr::OSC_SINE;
        hat.trigger(127, 0, 60, mockNoteFreq);
        check_u32("hat trigger closed sine phase", hat.osc.phase, 1072693248u);
        check_u8("hat isOpen closed", hat.isOpen, 0);
        check("hat trigger velo", hat.velo, 1.0f, 1e-5f);
    }
    {
        lxr::HiHatVoice hat;
        hat.init();
        hat.osc.waveform = lxr::OSC_SINE;
        hat.trigger(127, 1, 60, mockNoteFreq);
        check_u32("hat trigger open sine phase", hat.osc.phase, 1072693248u);
        check_u8("hat isOpen open", hat.isOpen, 1);
    }
    {
        lxr::HiHatVoice hat;
        hat.init();
        hat.osc.waveform = lxr::OSC_TRI;
        hat.trigger(127, 0, 60, mockNoteFreq);
        check_u32("hat trigger tri phase", hat.osc.phase, 267386880u);
    }
    {
        lxr::HiHatVoice hat;
        hat.init();
        hat.transGen.waveform = 1;
        hat.transGen.volume = 0.5f;
        hat.osc.waveform = lxr::OSC_SINE;
        hat.trigger(127, 0, 60, mockNoteFreq);
        check_u32("hat trigger offset sine phase", hat.osc.phase, 536346624u);
    }
    {
        lxr::HiHatVoice hat;
        hat.init();
        hat.trigger(127, 0, 60, mockNoteFreq);
        check("hat closed decay", hat.oscVolEg.decay, hat.decayClosed, 1e-5f);
        
        hat.trigger(127, 1, 60, mockNoteFreq);
        check("hat open decay", hat.oscVolEg.decay, hat.decayOpen, 1e-5f);
    }
    {
        lxr::HiHatVoice hat;
        hat.init();
        int16_t buf[32] = {0};
        hat.calcAsync(mockNoteFreq);
        hat.calcSyncBlock(buf, 32, mockTables);
        printf("PASS hat calcSyncBlock executed\n");
        pass++;
    }
    
    printf("\nSUMMARY pass=%d fail=%d\n", pass, fail);
    return fail > 0 ? 1 : 0;
}