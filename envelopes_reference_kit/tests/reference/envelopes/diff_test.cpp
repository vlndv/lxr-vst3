// Differential test: dsp/Envelopes.cpp against the ORIGINAL SlopeEg2.c, Decay.c and snapEg.c (bitwise). Run via make_ref.sh.
#include "Envelopes.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
extern "C" {
typedef struct { float attack, decay, slope, invSlope; uint8_t repeat, repeatCnt; float value; uint8_t state; } SlopeEg2;
typedef struct { float decay, slope, value; } DecayEg;
typedef struct { float value; } CSnap;
void slopeEg2_init(SlopeEg2*); void slopeEg2_trigger(SlopeEg2*); float slopeEg2_calc(SlopeEg2*);
void slopeEg2_setAttack(SlopeEg2*, uint8_t, uint8_t); void slopeEg2_setDecay(SlopeEg2*, uint8_t, uint8_t); void slopeEg2_setSlope(SlopeEg2*, uint8_t);
void DecayEg_init(DecayEg*); void DecayEg_trigger(DecayEg*); float DecayEg_calc(DecayEg*); void DecayEg_setDecay(DecayEg*, uint8_t); void DecayEg_setSlope(DecayEg*, uint8_t);
void SnapEg_init(CSnap*); void SnapEg_trigger(CSnap*); float SnapEg_calc(CSnap*, float);
}
static uint32_t rs = 4242; static uint32_t rnd() { rs = rs * 1664525u + 1013904223u; return rs >> 8; }
static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
int main() {
    long bad = 0;
    { SlopeEg2 c; AmpEg p; std::memset(&c, 0, sizeof c); slopeEg2_init(&c); p.init();   // init() defaults, including the slope
      std::printf("init: original attack=%.9g decay=%.9g slope=%.9g invSlope=%.9g value=%g state=%d repeat=%d\n", c.attack, c.decay, c.slope, c.invSlope, c.value, c.state, c.repeat);
      std::printf("init: port     attack=%.9g decay=%.9g slope=%.9g invSlope=%.9g value=%g state=%d repeat=%d\n", p.attack, p.decay, p.slope, p.invSlope, p.value, p.state, p.repeat);
      if (bits(c.attack) != bits(p.attack) || bits(c.decay) != bits(p.decay) || bits(c.slope) != bits(p.slope) || bits(c.invSlope) != bits(p.invSlope) || bits(c.value) != bits(p.value) || c.state != p.state || c.repeat != p.repeat) { std::printf("MISMATCH AmpEg::init\n"); ++bad; } }
    for (int it = 0; it < 20000; ++it) {
        SlopeEg2 c; AmpEg p; std::memset(&c, 0, sizeof c); slopeEg2_init(&c); p.init();
        for (int op = 0; op < 8; ++op) {
            uint8_t a = rnd() % 128, d = rnd() % 128, s = rnd() % 128, r = (rnd() % 3 == 0) ? (uint8_t)(rnd() % 5) : 0; uint8_t sync = rnd() % 2;
            slopeEg2_setAttack(&c, a, sync); p.setAttack(a, sync); slopeEg2_setDecay(&c, d, sync); p.setDecay(d, sync);
            slopeEg2_setSlope(&c, s); p.setSlope(s); c.repeat = r; p.setRepeat(r);
            slopeEg2_trigger(&c); p.trigger();
            int ticks = 1 + rnd() % 300;
            for (int t = 0; t < ticks; ++t) {
                float x = slopeEg2_calc(&c), y = p.calc();
                if (bits(x) != bits(y) || bits(c.value) != bits(p.value) || c.state != p.state || c.repeatCnt != p.repeatCnt) { if (bad < 5) std::printf("MISMATCH AmpEg it=%d op=%d t=%d\n", it, op, t); ++bad; goto next; }
            }
        }
        next:;
    }
    for (int it = 0; it < 20000; ++it) {
        DecayEg c; PitchDecayEg p; std::memset(&c, 0, sizeof c); DecayEg_init(&c); p.init(); p.slope = c.slope;
        for (int op = 0; op < 6; ++op) {
            uint8_t d = rnd() % 128, s = rnd() % 128; DecayEg_setDecay(&c, d); p.setDecay(d); DecayEg_setSlope(&c, s); p.setSlope(s);
            DecayEg_trigger(&c); p.trigger(); int ticks = 1 + rnd() % 300;
            for (int t = 0; t < ticks; ++t) { float x = DecayEg_calc(&c), y = p.calc(); if (bits(x) != bits(y) || bits(c.value) != bits(p.value)) { if (bad < 5) std::printf("MISMATCH DecayEg it=%d s=%d\n", it, s); ++bad; goto next2; } }
        }
        next2:;
    }
    for (int it = 0; it < 20000; ++it) {
        CSnap c; SnapEg p; SnapEg_init(&c); p.init(); float tm = 0.25f + (rnd() % 1000) / 250.f;
        SnapEg_trigger(&c); p.trigger();
        for (int t = 0; t < 40; ++t) { float x = SnapEg_calc(&c, tm), y = p.calc(tm); if (bits(x) != bits(y) || bits(c.value) != bits(p.value)) { if (bad < 5) std::printf("MISMATCH Snap it=%d\n", it); ++bad; break; } }
    }
    std::printf("differential: %ld mismatches\n", bad);
    return bad ? 1 : 0;
}
