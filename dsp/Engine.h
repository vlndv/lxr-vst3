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
    
    // 808-style presets
    void setup808Kick(uint8_t voiceIdx);
    void setup808Snare();
    void setup808HiHatClosed();
    void setup808HiHatOpen();
    void setup808Cymbal();
    void setup808Tom(uint8_t voiceIdx);
    
    // Setup a complete 808 kit (D1=kick, D2/D3=toms, SN=snare, CY=cymbal, HH=hihat)
    void setup808Kit();
};

} // namespace lxr