// FILE: dsp/Envelopes.h
#pragma once
#include <cstdint>

// Amp Envelope (SlopeEg2)
// States: STOPPED(0), A(1), D(2), REPEAT(3)
struct AmpEg {
    float attack, decay, value, slope, invSlope;
    int state;
    int repeat, repeatCnt;
    
    void init();
    void trigger();
    float calc();
    void setAttack(uint8_t data2, bool isSync);
    void setDecay(uint8_t data2, bool isSync);
    void setSlope(uint8_t data2);
    void setRepeat(uint8_t count);
};

// Pitch Decay Envelope (DecayEg)
// Starts at 1.0 on trigger, decays to 0.0
struct PitchDecayEg {
    float decay, value, slope;
    
    void init();
    void trigger();
    float calc();
    void setDecay(uint8_t data2);
    void setSlope(uint8_t data2);
};

// Snap Envelope (SnapEg)
// Returns value^2 * 24, decrements by 0.2 * time per tick
struct SnapEg {
    float value;
    
    void init();
    void trigger();
    float calc(float time);
};