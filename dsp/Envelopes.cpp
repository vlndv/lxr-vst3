// FILE: dsp/Envelopes.cpp
#include "Envelopes.h"
#include <cmath>

// ============================================================================
// AmpEg (SlopeEg2)
// ============================================================================

static float slopeEg2_calcSlopeValue(float val, float slope) {
    return (1.0f + slope) * val / (1.0f + slope * std::fabs(val));
}

#define TIME_AMOUNT_DECAY 0.999f
#define TIME_AMOUNT_ATTACK 0.99f
#define TIME_K_ATTACK (2.0f * TIME_AMOUNT_ATTACK / (1.0f - TIME_AMOUNT_ATTACK))
#define TIME_K_DECAY (2.0f * TIME_AMOUNT_DECAY / (1.0f - TIME_AMOUNT_DECAY))

static float slopeEg2_calcTime(uint8_t data2, float amount) {
    const float val = data2 / 127.0f;
    return 1.0f - ((1.0f + amount) * val / (1.0f + amount * std::fabs(val)));
}

void AmpEg::init() {
    attack = 0.01f;
    decay = 0.01f;
    value = 0.0f;
    state = 0; // EG_STOPPED
    repeat = 0;
    repeatCnt = 0;
    setSlope(64); // default midpoint
}

void AmpEg::trigger() {
    if (!repeat) {
        state = 1; // EG_A
    } else {
        state = 3; // EG_REPEAT
        value = 1.0f;
    }
    repeatCnt = repeat;
}

float AmpEg::calc() {
    float val = value;
    
    switch (state) {
    case 0: // EG_STOPPED
        return 0.0f;
        
    case 3: // EG_REPEAT
        if (val > 0.0f) {
            val -= attack;
        } else {
            repeatCnt--;
            if (repeatCnt > 0) {
                val = 1.0f;
            } else {
                state = 2; // EG_D
                val = 1.0f;
            }
        }
        value = val;
        return slopeEg2_calcSlopeValue(val, slope);
        
    case 1: // EG_A
        val += attack;
        if (val >= 1.0f) {
            val = 1.0f;
            state = 2; // EG_D
        }
        value = val;
        return slopeEg2_calcSlopeValue(val, invSlope);
        
    case 2: // EG_D
        if (val > 0.0f) {
            val -= decay;
            value = val;
            return slopeEg2_calcSlopeValue(val, slope);
        } else {
            value = 0.0f;
            return 0.0f;
        }
    }
    
    return 0.0f;
}

void AmpEg::setAttack(uint8_t data2, bool isSync) {
    attack = slopeEg2_calcTime(data2, TIME_K_ATTACK);
    if (isSync) {
        attack /= 16.0f;
    }
}

void AmpEg::setDecay(uint8_t data2, bool isSync) {
    decay = slopeEg2_calcTime(data2, TIME_K_DECAY);
    if (isSync) {
        decay /= 16.0f;
    }
}

void AmpEg::setSlope(uint8_t data2) {
    const float amount = ((data2 / 127.0f) - 0.5f) * 1.999f;
    const float invAmount = -amount;
    
    slope = 2.0f * amount / (1.0f - amount);
    invSlope = 2.0f * invAmount / (1.0f - invAmount);
}

void AmpEg::setRepeat(uint8_t count) {
    repeat = count;
}

// ============================================================================
// PitchDecayEg (DecayEg)
// ============================================================================

static float DecayEg_calcSlopeValue(float val, float slope) {
    return (1.0f + slope) * val / (1.0f + slope * std::fabs(val));
}

#define DECAY_TIME_K (2.0f * 0.99f / (1.0f - 0.99f))

static float DecayEg_calcTime(uint8_t data2) {
    const float val = data2 / 127.0f;
    return 1.0f - ((1.0f + DECAY_TIME_K) * val / (1.0f + DECAY_TIME_K * std::fabs(val)));
}

void PitchDecayEg::init() {
    decay = 0.01f;
    value = 0.0f;
    slope = 0.0f;
}

void PitchDecayEg::trigger() {
    value = 1.0f;
}

float PitchDecayEg::calc() {
    float val = value;
    
    if (val > 0.0f) {
        val -= decay;
    } else {
        val = 0.0f;
    }
    
    value = val;
    return DecayEg_calcSlopeValue(val, slope);
}

void PitchDecayEg::setDecay(uint8_t data2) {
    decay = DecayEg_calcTime(data2);
}

void PitchDecayEg::setSlope(uint8_t data2) {
    // ORIGINAL QUIRK: at data2=127, amount=1.0, denominator (1-amount) is 0 -> slope becomes +inf/NaN
    const float amount = ((data2 / 127.0f) - 0.5f) * 2.0f;
    slope = 2.0f * amount / (1.0f - amount);
}

// ============================================================================
// SnapEg
// ============================================================================

#define SNAP_MAX_VALUE 24.0f
#define SNAP_REDUCTION 0.2f

void SnapEg::init() {
    value = 0.0f;
}

void SnapEg::trigger() {
    value = 1.0f;
}

float SnapEg::calc(float time) {
    float ret = 0.0f;
    
    if (value > 0.0f) {
        ret = value * value * SNAP_MAX_VALUE;
        value -= SNAP_REDUCTION * time;
    }
    
    return ret;
}