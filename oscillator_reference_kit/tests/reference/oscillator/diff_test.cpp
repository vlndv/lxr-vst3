// Differential test: the port (dsp/Oscillator.cpp) against the ORIGINAL Oscillator.c (ARM float->uint32 semantics), bitwise.
// Built and run by tests/reference/oscillator/make_ref.sh. Needs data/*.bin (tools/export_data.py) in the current directory.
#include "Oscillator.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include "MidiNoteNumbers.h"   // defines static const float MidiNoteFrequencies[128] (the original table)
using lxr::OscInfo; using lxr::OscTables; using lxr::OscTableStore; using lxr::OscRng;

extern "C" {
typedef struct { int16_t output; uint32_t phaseInc; uint32_t phase; float freq; uint8_t waveform; uint8_t tableOffset; float pitchMod;
                 float fmMod; float modNodeValue; uint16_t midiFreq; uint8_t baseNote; uint32_t startPhase; } COsc;
void osc_setFreq(COsc*); void osc_setBaseNote(COsc*, uint8_t); void osc_recalcFreq(COsc*);
void calcNextOscSampleBlock(COsc*, int16_t*, const uint8_t, const float);
void calcNextOscSampleFmBlock(COsc*, int16_t*, int16_t*, uint8_t, const float);
uint32_t freq2PhaseIncr(float); uint32_t freq2PhaseIncr1024(float); uint32_t freq2PhaseIncr32767(float); uint8_t freqToTableIndex(float);
extern const int16_t sine_table[4097]; extern const int16_t sawTable[11][1024]; extern const int16_t triTable[11][1024]; extern const int16_t recTable[11][1024];
extern const unsigned char crashSample[32768];
static uint32_t crng = 0x9E3779B9u;
uint32_t GetRngValue(void) { uint32_t x = crng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; crng = x; return x; }
float midiParser_calcDetune(uint8_t value) {   // verbatim from MidiParser.c
    float frac = (value / 127.f - 0.5f); float cent = 1;
    if (cent >= 0) { cent += frac * (1.0594630943592952645618252949463f - 1); } else { cent += frac * (1.0594630943592952645618252949463f - 1); }
    return cent;
}
}

static uint32_t rs = 20260921u;
static uint32_t rnd() { rs = rs * 1664525u + 1013904223u; return rs >> 8; }
static float rndf(float lo, float hi) { return lo + (hi - lo) * ((rnd() % 100001) / 100000.f); }

static bool same(const COsc& c, const OscInfo& p) {
    return c.output == p.output && c.phaseInc == p.phaseInc && c.phase == p.phase && c.freq == p.freq && c.waveform == p.waveform &&
           c.tableOffset == p.tableOffset && c.pitchMod == p.pitchMod && c.fmMod == p.fmMod && c.modNodeValue == p.modNodeValue &&
           c.midiFreq == p.midiFreq && c.baseNote == p.baseNote;
}

int main() {
    OscTableStore store;
    if (!store.load("data")) { std::printf("FAIL: cannot load data/*.bin (run tools/export_data.py first)\n"); return 2; }
    // The original reads one element past sawTable/triTable/recTable/crashSample; feed the port the same values (host memory)
    store.setGuards(reinterpret_cast<const int16_t*>(sawTable)[11 * 1024], reinterpret_cast<const int16_t*>(triTable)[11 * 1024],
                    reinterpret_cast<const int16_t*>(recTable)[11 * 1024], crashSample[32768]);
    const OscTables& t = store.tables();
    long bad = 0, blocks = 0;
    if (std::memcmp(t.sine, sine_table, 4097 * 2) || std::memcmp(t.saw, sawTable, 11 * 1024 * 2) || std::memcmp(t.tri, triTable, 11 * 1024 * 2) ||
        std::memcmp(t.rec, recTable, 11 * 1024 * 2) || std::memcmp(t.crash, crashSample, 32768) || std::memcmp(t.noteFreq, MidiNoteFrequencies, 128 * 4)) {
        std::printf("FAIL: exported data tables differ from the original arrays\n"); return 1;
    }
    for (int it = 0; it < 20000; ++it) {
        COsc c; OscInfo p; std::memset(&c, 0, sizeof c);
        c.waveform = (uint8_t)(rnd() % 6); c.freq = std::exp(rndf(std::log(5.f), std::log(24000.f)));
        c.pitchMod = rndf(0.25f, 4.f); c.modNodeValue = rndf(0.5f, 1.f); c.fmMod = rndf(0.f, 1.f); c.phase = rnd() * 4099u + rnd();
        p.waveform = c.waveform; p.freq = c.freq; p.pitchMod = c.pitchMod; p.modNodeValue = c.modNodeValue; p.fmMod = c.fmMod; p.phase = c.phase;
        osc_setFreq(&c); lxr::osc_setFreq(&p);
        if (!same(c, p)) { if (bad < 5) std::printf("MISMATCH setFreq it=%d wave=%d f=%g\n", it, c.waveform, c.freq); ++bad; }
        static const float gains[4] = {1.f, 0.9f, 0.5f, 0.7f}; const float gain = gains[rnd() % 4]; const bool fm = rnd() % 2;
        crng = 0x9E3779B9u; OscRng rng;
        for (int blk = 0; blk < 4; ++blk) {
            const uint8_t size = (uint8_t)(1 + rnd() % 40); int16_t a[64], b[64], m[64];
            for (int i = 0; i < 64; ++i) { m[i] = (int16_t)(rnd() % 65536 - 32768); a[i] = b[i] = 0x1234; }
            if (fm) { calcNextOscSampleFmBlock(&c, m, a, size, gain); lxr::calcNextOscSampleFmBlock(&p, m, b, size, gain, t, rng); }
            else    { calcNextOscSampleBlock(&c, a, size, gain);      lxr::calcNextOscSampleBlock(&p, b, size, gain, t, rng); }
            ++blocks;
            if (std::memcmp(a, b, size * 2) || !same(c, p)) { if (bad < 5) std::printf("MISMATCH it=%d blk=%d wave=%d fm=%d f=%g\n", it, blk, c.waveform, (int)fm, c.freq); ++bad; break; }
        }
    }
    for (int it = 0; it < 3000; ++it) {   // frequency helpers, including huge and tiny values
        const float f = it < 100 ? (float)it * 0.5f : std::exp(rndf(std::log(1.f), std::log(2.0e6f)));
        if (freq2PhaseIncr(f) != lxr::freq2PhaseIncr(f) || freq2PhaseIncr1024(f) != lxr::freq2PhaseIncr1024(f) ||
            freq2PhaseIncr32767(f) != lxr::freq2PhaseIncr32767(f) || freqToTableIndex(f) != lxr::freqToTableIndex(f)) {
            if (bad < 5) std::printf("MISMATCH freq helpers f=%g\n", f); ++bad; }
    }
    for (int it = 0; it < 5000; ++it) {   // base note / detune
        COsc c; OscInfo p; std::memset(&c, 0, sizeof c);
        c.midiFreq = (uint16_t)(rnd() % 65536); p.midiFreq = c.midiFreq; const uint8_t bn = (uint8_t)(rnd() % 128);
        osc_setBaseNote(&c, bn); lxr::osc_setBaseNote(&p, bn, t.noteFreq);
        COsc c2 = c; OscInfo p2 = p; c2.midiFreq = (uint16_t)(rnd() % 65536); p2.midiFreq = c2.midiFreq; osc_recalcFreq(&c2); lxr::osc_recalcFreq(&p2, t.noteFreq);
        if (c.freq != p.freq || c.baseNote != p.baseNote || c2.freq != p2.freq) { if (bad < 5) std::printf("MISMATCH base note it=%d\n", it); ++bad; }
    }
    std::printf("differential: %ld render blocks + helpers, %ld mismatches (bitwise on buffers and all state)\n", blocks, bad);
    return bad ? 1 : 0;
}
