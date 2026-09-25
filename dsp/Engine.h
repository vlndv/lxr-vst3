// FILE: dsp/Engine.h
#pragma once
#include <cstdint>
#include "DrumVoice.h"
#include "Snare.h"
#include "Cymbal.h"
#include "HiHat.h"
#include "Mixer.h"
#include "OscTables.h"

namespace lxr {

constexpr float kEngineSampleRate = 44002.7573529412f;
// kBlockSize is already defined in Mixer.h

class Engine {
public:
    DrumVoice drums[3];
    SnareVoice snare;
    CymbalVoice cymbal;
    HiHatVoice hihat;
    Mixer mixer;
    
    OscTables tables;
    float noteFreq[128];

    Engine();
    void init();
    bool loadAssets(const char* dataDir);

    void triggerDrum(uint8_t voiceIdx, uint8_t vel, uint8_t note);
    void triggerSnare(uint8_t vel, uint8_t note);
    void triggerCymbal(uint8_t vel, uint8_t note);
    void triggerHiHat(uint8_t vel, bool isOpen, uint8_t note);

    void processBlock(int16_t* outSt1L, int16_t* outSt1R, 
                      int16_t* outSt2L, int16_t* outSt2R);
};

} // namespace lxr