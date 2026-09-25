// FILE: dsp/Engine.cpp
#include "Engine.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace lxr {

Engine::Engine() {
    init();
}

void Engine::init() {
    for (int i = 0; i < 3; i++) drums[i].init();
    snare.init();
    cymbal.init();
    hihat.init();
    mixer.init();

    for (int i = 0; i < 128; i++) {
        noteFreq[i] = 440.0f * std::pow(2.0f, (i - 69) / 12.0f);
    }
}

static bool readBin(const char* path, void* buffer, size_t expectedBytes) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    size_t read = fread(buffer, 1, expectedBytes, f);
    fclose(f);
    return read == expectedBytes;
}

bool Engine::loadAssets(const char* dataDir) {
    char path[256];
    
    static int16_t sineBuf[4097];
    snprintf(path, sizeof(path), "%s/sine_table.bin", dataDir);
    if (!readBin(path, sineBuf, sizeof(sineBuf))) return false;
    tables.sine = sineBuf;

    static int16_t sawBuf[11 * 1024];
    static int16_t triBuf[11 * 1024];
    static int16_t recBuf[11 * 1024];
    snprintf(path, sizeof(path), "%s/saw_table.bin", dataDir);
    if (!readBin(path, sawBuf, sizeof(sawBuf))) return false;
    snprintf(path, sizeof(path), "%s/tri_table.bin", dataDir);
    if (!readBin(path, triBuf, sizeof(triBuf))) return false;
    snprintf(path, sizeof(path), "%s/rec_table.bin", dataDir);
    if (!readBin(path, recBuf, sizeof(recBuf))) return false;
    tables.saw = sawBuf;
    tables.tri = triBuf;
    tables.rec = recBuf;

    static uint8_t crashBuf[32768];
    snprintf(path, sizeof(path), "%s/crash_sample.bin", dataDir);
    if (!readBin(path, crashBuf, sizeof(crashBuf))) return false;
    tables.crash = crashBuf;

    return true;
}

void Engine::triggerDrum(uint8_t voiceIdx, uint8_t vel, uint8_t note) {
    if (voiceIdx < 3) drums[voiceIdx].trigger(vel, note, noteFreq);
}

void Engine::triggerSnare(uint8_t vel, uint8_t note) {
    snare.trigger(vel, note, noteFreq);
}

void Engine::triggerCymbal(uint8_t vel, uint8_t note) {
    cymbal.trigger(vel, note, noteFreq);
}

void Engine::triggerHiHat(uint8_t vel, bool isOpen, uint8_t note) {
    hihat.trigger(vel, isOpen ? 1 : 0, note, noteFreq);
}

void Engine::processBlock(int16_t* outSt1L, int16_t* outSt1R, 
                          int16_t* outSt2L, int16_t* outSt2R) {
    int16_t* voiceBufs[6];
    static int16_t v0[32], v1[32], v2[32], v3[32], v4[32], v5[32];
    
    for (int i = 0; i < 3; i++) drums[i].calcAsync(noteFreq);
    snare.calcAsync(noteFreq);
    cymbal.calcAsync(noteFreq);
    hihat.calcAsync(noteFreq);

    drums[0].calcSyncBlock(v0, 32, tables); voiceBufs[0] = v0;
    drums[1].calcSyncBlock(v1, 32, tables); voiceBufs[1] = v1;
    drums[2].calcSyncBlock(v2, 32, tables); voiceBufs[2] = v2;
    snare.calcSyncBlock(v3, 32, tables);    voiceBufs[3] = v3;
    cymbal.calcSyncBlock(v4, 32, tables);   voiceBufs[4] = v4;
    hihat.calcSyncBlock(v5, 32, tables);    voiceBufs[5] = v5;

    mixer.processBlock(voiceBufs, outSt1L, outSt1R, outSt2L, outSt2R, 32);
}

// 808 Kick: Direct float assignment to bypass mapping quirks
void Engine::setup808Kick(uint8_t voiceIdx) {
    if (voiceIdx >= 3) return;
    DrumVoice& d = drums[voiceIdx];
    
    d.osc.waveform = OSC_SINE;
    d.osc.midiFreq = (40 << 8) | 64; // Note 40 (E2), fine=64 (center)
    
    // Direct envelope state (1.0 = instant attack, 0.002 = ~1s decay)
    d.oscVolEg.attack = 1.0f;
    d.oscVolEg.decay = 0.002f;
    d.oscVolEg.slope = 0.0f;
    d.oscVolEg.invSlope = 0.0f;
    
    // Pitch sweep
    d.oscPitchEg.decay = 0.01f;
    d.oscPitchEg.slope = 0.0f;
    d.egPitchModAmount = 2.0f; // Sweeps down 2 octaves
    
    // Filter (safe values well below 0.45 NaN threshold)
    d.filterType = FILTER_LP;
    d.filter.f = 0.05f; 
    d.filter.q = 0.2f;
    d.filter.drive = 1.0f;
    d.filter.recalcFreq();
    
    d.transGen.waveform = 0;
    d.transGen.volume = 0.0f;
    d.vol = 0.9f;
}

// 808 Snare
void Engine::setup808Snare() {
    snare.osc.waveform = OSC_TRI;
    snare.noiseOsc.waveform = OSC_NOISE;
    snare.mix = 0.5f;
    
    snare.oscVolEg.attack = 1.0f;
    snare.oscVolEg.decay = 0.005f; // ~200ms
    snare.oscVolEg.slope = 0.0f;
    snare.oscVolEg.invSlope = 0.0f;
    
    snare.filterType = FILTER_HP;
    snare.filter.f = 0.1f;
    snare.filter.q = 0.2f;
    snare.filter.drive = 1.0f;
    snare.filter.recalcFreq();
    
    snare.transGen.waveform = 0;
    snare.transGen.volume = 0.0f;
    snare.vol = 0.8f;
}

// 808 Hi-Hat Closed
void Engine::setup808HiHatClosed() {
    hihat.osc.waveform = OSC_NOISE;
    hihat.modOsc.waveform = OSC_SINE;
    hihat.modOsc2.waveform = OSC_NOISE;
    
    hihat.oscVolEg.attack = 1.0f;
    hihat.decayClosed = 0.02f; // 50ms
    
    hihat.filterType = FILTER_HP;
    hihat.filter.f = 0.2f; // High
    hihat.filter.q = 0.2f;
    hihat.filter.drive = 1.0f;
    hihat.filter.recalcFreq();
    
    hihat.vol = 0.6f;
}

// 808 Hi-Hat Open
void Engine::setup808HiHatOpen() {
    setup808HiHatClosed();
    hihat.decayOpen = 0.005f; // 200ms
}

// 808 Cymbal
void Engine::setup808Cymbal() {
    cymbal.osc.waveform = OSC_NOISE;
    cymbal.modOsc.waveform = OSC_SINE;
    cymbal.modOsc2.waveform = OSC_NOISE;
    
    cymbal.oscVolEg.attack = 1.0f;
    cymbal.oscVolEg.decay = 0.002f; // Long
    
    cymbal.filterType = FILTER_HP;
    cymbal.filter.f = 0.15f;
    cymbal.filter.q = 0.2f;
    cymbal.filter.drive = 1.0f;
    cymbal.filter.recalcFreq();
    
    cymbal.vol = 0.7f;
}

// 808 Tom
void Engine::setup808Tom(uint8_t voiceIdx) {
    if (voiceIdx >= 3) return;
    DrumVoice& d = drums[voiceIdx];
    
    d.osc.waveform = OSC_SINE;
    d.osc.midiFreq = (50 << 8) | 64; // Note 50 (D3)
    
    d.oscVolEg.attack = 1.0f;
    d.oscVolEg.decay = 0.003f;
    d.oscVolEg.slope = 0.0f;
    d.oscVolEg.invSlope = 0.0f;
    
    d.oscPitchEg.decay = 0.008f;
    d.oscPitchEg.slope = 0.0f;
    d.egPitchModAmount = 1.0f;
    
    d.filterType = FILTER_LP;
    d.filter.f = 0.08f;
    d.filter.q = 0.2f;
    d.filter.drive = 1.0f;
    d.filter.recalcFreq();
    
    d.transGen.waveform = 0;
    d.transGen.volume = 0.0f;
    d.vol = 0.85f;
}

void Engine::setup808Kit() {
    setup808Kick(0);
    setup808Tom(1);
    setup808Tom(2);
    drums[2].osc.midiFreq = (55 << 8) | 64; // Higher tom
    
    setup808Snare();
    setup808Cymbal();
    setup808HiHatClosed();
}

} // namespace lxr