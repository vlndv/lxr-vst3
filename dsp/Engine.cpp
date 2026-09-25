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
    
    // Sine table: 4097 int16_t
    static int16_t sineBuf[4097];
    snprintf(path, sizeof(path), "%s/sine_table.bin", dataDir);
    if (!readBin(path, sineBuf, sizeof(sineBuf))) return false;
    tables.sine = sineBuf;

    // Wavetables: 11 x 1024 int16_t
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

    // Crash sample: 32768 uint8_t
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

} // namespace lxr