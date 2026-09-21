// dsp/Oscillator.cpp
#include "Oscillator.h"
#include "ParamMapMisc.h"

namespace lxr {
namespace {

constexpr float kFrac19 = 0.0000019073486328125f;   // 2^-19
constexpr float kFrac22 = 2.38418579101562e-07f;    // ~2^-22
constexpr float kFrac17 = 0.00000762939453125f;     // 2^-17

uint8_t clz32(uint32_t v) {                         // ARM CLZ: returns 32 for 0
    if (v == 0) return 32;
    uint8_t n = 0;
    while (!(v & 0x80000000u)) { v <<= 1; ++n; }
    return n;
}
uint8_t fastLog2(uint32_t val) { return static_cast<uint8_t>(31 - clz32(val)); }   // ORIGINAL QUIRK: 255 for val = 0
int freqToMidiNote(float f) { return (12 * fastLog2(floatToU32Sat(f / 440.f))) + 70; }

inline int16_t toI16(float x) { return static_cast<int16_t>(x); }

void calcSineBlock(OscInfo* osc, int16_t* buf, uint8_t size, float gain, const int16_t* sine) {
    for (uint8_t i = 0; i < size; i++) {
        const uint32_t oscPhase = osc->phase;
        const uint32_t index = oscPhase;
        uint32_t itg = index >> 20;
        int16_t oscOut = sine[itg++];
        // ORIGINAL QUIRK: the mask is 0x7ffff (19 bits) but the fraction has 20 bits, so frac runs 0..1 twice per table step
        const float frac = (index & 0x7ffff) * kFrac19;
        oscOut = toI16(static_cast<float>(oscOut) + frac * static_cast<float>(sine[itg] - oscOut));
        osc->phase = oscPhase + osc->phaseInc;
        buf[i] = toI16(oscOut * gain);
    }
}

void calcFmSineBlock(OscInfo* osc, const int16_t* modBuffer, int16_t* buf, uint8_t size, float gain, const int16_t* sine) {
    for (uint8_t i = 0; i < size; i++) {
        const uint32_t oscPhase = osc->phase;
        // ORIGINAL QUIRK: (uint32_t) of a negative float; on the Cortex-M4 this saturates to 0 (half-wave FM)
        const uint32_t index = oscPhase + (floatToU32Sat(modBuffer[i] * osc->fmMod) << 17);
        uint32_t itg = index >> 20;
        int16_t oscOut = sine[itg++];
        const float frac = (index & 0x7ffff) * kFrac19;      // same 19-bit quirk as above
        oscOut = toI16(static_cast<float>(oscOut) + frac * static_cast<float>(sine[itg] - oscOut));
        osc->phase = oscPhase + osc->phaseInc;
        buf[i] = toI16(oscOut * gain);
    }
}

void calcWavetableOscBlock(OscInfo* osc, const int16_t* table, int16_t* buf, uint8_t size, float gain) {
    const int16_t* row = table + osc->tableOffset * 1024;
    for (uint8_t i = 0; i < size; i++) {
        const uint32_t oscPhase = osc->phase;
        uint32_t itg = oscPhase >> 22;
        int16_t oscOut = row[itg++];
        // ORIGINAL QUIRK: the fraction is computed from the table index itg (already incremented), not from the phase bits,
        // so it is at most 1024 * 2.4e-7 (effectively no interpolation). row[1024] is the first element of the next row.
        const float frac = (itg & 0x003FFFFF) * kFrac22;
        oscOut = toI16(static_cast<float>(oscOut) + frac * static_cast<float>(row[itg] - oscOut));
        osc->phase = oscPhase + osc->phaseInc;
        buf[i] = toI16(oscOut * gain);
    }
}

void calcFmBlock(OscInfo* osc, const int16_t* table, const int16_t* modBuffer, int16_t* buf, uint8_t size, float gain) {
    const int16_t* row = table + osc->tableOffset * 1024;
    for (uint8_t i = 0; i < size; i++) {
        const uint32_t oscPhase = osc->phase;
        const uint32_t index = oscPhase + (floatToU32Sat(modBuffer[i] * osc->fmMod) << 19);   // wraps modulo 2^32 like the original
        uint32_t itg = index >> 22;
        int16_t oscOut = row[itg++];
        const float frac = (index & 0x003FFFFF) * kFrac22;
        oscOut = toI16(static_cast<float>(oscOut) + frac * static_cast<float>(row[itg] - oscOut));
        osc->phase = oscPhase + osc->phaseInc;
        osc->output = oscOut;
        buf[i] = toI16(oscOut * gain);
    }
}

void calcSampleOscBlock(OscInfo* osc, int16_t* buf, uint8_t size, float gain, const uint8_t* crash) {
    for (uint8_t i = 0; i < size; i++) {
        const uint32_t oscPhase = osc->phase;
        uint32_t itg = oscPhase >> 17;
        int16_t oscOut = crash[itg++];
        // ORIGINAL QUIRK: the mask is the decimal 20000 (0x4E20), not 0x1FFFF: the "fraction" picks scattered phase bits (max 0.153)
        const float frac = (oscPhase & 20000) * kFrac17;
        oscOut = toI16(static_cast<float>(oscOut) + frac * static_cast<float>(crash[itg] - oscOut));
        oscOut = static_cast<int16_t>((oscOut - 127) * 256);   // wraps for 255 -> -32768, like the original int16 store
        osc->phase = oscPhase + osc->phaseInc;
        buf[i] = toI16(oscOut * gain);
    }
}

void calcSampleOscFmBlock(OscInfo* osc, const int16_t* modBuffer, int16_t* buf, uint8_t size, float gain, const uint8_t* crash) {
    for (uint8_t i = 0; i < size; i++) {
        const uint32_t oscPhase = osc->phase;
        const uint32_t index = oscPhase + (floatToU32Sat(modBuffer[i] * osc->fmMod) << 14);
        uint32_t itg = index >> 17;
        int16_t oscOut = crash[itg++];
        const float frac = (index & 20000) * kFrac17;         // same decimal-mask quirk
        oscOut = toI16(static_cast<float>(oscOut) + frac * static_cast<float>(crash[itg] - oscOut));
        oscOut = static_cast<int16_t>((oscOut - 127) * 256);
        osc->phase = oscPhase + osc->phaseInc;
        osc->output = oscOut;
        buf[i] = toI16(oscOut * gain);
    }
}

void fillZero(int16_t* buf, uint8_t size) { for (uint8_t i = 0; i < size; i++) buf[i] = 0; }

} // namespace

uint32_t floatToU32Sat(float x) {
    if (!(x > 0.f)) return 0u;                       // negative, zero and NaN
    if (x >= 4294967296.0f) return 0xFFFFFFFFu;
    return static_cast<uint32_t>(x);
}

uint32_t freq2PhaseIncr(float f)     { return floatToU32Sat(((4096 * f) / kRealFs) * 1048576); }
uint32_t freq2PhaseIncr1024(float f) { return floatToU32Sat(((1024 * f) / kRealFs) * 4194304); }
// ORIGINAL QUIRK: named "32767" but uses 1024 with <<17, so the crash sample (32768 entries) plays 32x slower than a full table cycle at f
uint32_t freq2PhaseIncr32767(float f) { return floatToU32Sat(((1024 * f) / kRealFs) * 131072); }

// ORIGINAL QUIRK: for f < 440 Hz, fast_log2(0) returns 255 (uint8 of 31 - 32), so the table index wraps to 4
uint8_t freqToTableIndex(float f) { return static_cast<uint8_t>(freqToMidiNote(f) / 12); }

void osc_setFreq(OscInfo* osc) {
    switch (osc->waveform) {
    case OSC_SINE:
    case OSC_NOISE:
        osc->phaseInc = freq2PhaseIncr(osc->freq * osc->pitchMod * osc->modNodeValue);
        break;
    case OSC_SAW:
    case OSC_TRI:
    case OSC_REC: {
        const float currentFreq = osc->freq * osc->pitchMod * osc->modNodeValue;
        osc->phaseInc = freq2PhaseIncr1024(currentFreq);
        const uint8_t overtoneIndex = freqToTableIndex(currentFreq);
        osc->tableOffset = overtoneIndex > 10 ? 10 : overtoneIndex;
        break;
    }
    default: {   // OSC_CRASH and user samples (user samples are not ported)
        const float currentFreq = osc->freq * osc->pitchMod * osc->modNodeValue;
        osc->phaseInc = freq2PhaseIncr32767(currentFreq);
        break;
    }
    }
}

void osc_setBaseNote(OscInfo* osc, uint8_t baseNote, const float* noteFreq) {
    osc->freq = oscFrequencyHz(osc->midiFreq, baseNote, noteFreq);
    osc->baseNote = baseNote;
}

void osc_recalcFreq(OscInfo* osc, const float* noteFreq) {
    osc->freq = oscFrequencyHz(osc->midiFreq, osc->baseNote, noteFreq);
}

void calcNoiseBlock(OscInfo* osc, int16_t* buf, uint8_t size, float gain, OscRng& rng) {
    for (int i = 0; i < size; i++) {
        const uint32_t lastPhase = osc->phase;
        osc->phase += osc->phaseInc;
        if (lastPhase > osc->phase) {
            osc->output = static_cast<int16_t>(rng.next());   // new random value when the phase wraps ("pitched" white noise)
        }
        buf[i] = toI16(osc->output * gain);
    }
}

void calcNextOscSampleBlock(OscInfo* osc, int16_t* buf, uint8_t size, float gain, const OscTables& t, OscRng& rng) {
    switch (osc->waveform) {
    case OSC_SINE:  calcSineBlock(osc, buf, size, gain, t.sine); break;
    case OSC_SAW:   calcWavetableOscBlock(osc, t.saw, buf, size, gain); break;
    case OSC_TRI:   calcWavetableOscBlock(osc, t.tri, buf, size, gain); break;
    case OSC_REC:   calcWavetableOscBlock(osc, t.rec, buf, size, gain); break;
    case OSC_NOISE: calcNoiseBlock(osc, buf, size, gain, rng); break;
    case OSC_CRASH: calcSampleOscBlock(osc, buf, size, gain, t.crash); break;
    default:        // UNSURE: user samples (waveform >= 6) are out of scope; the original leaves buf untouched or reads SD data. The port outputs silence.
        fillZero(buf, size);
        break;
    }
}

void calcNextOscSampleFmBlock(OscInfo* osc, const int16_t* modBuffer, int16_t* buf, uint8_t size, float gain,
                              const OscTables& t, OscRng& rng) {
    switch (osc->waveform) {
    case OSC_SINE:  calcFmSineBlock(osc, modBuffer, buf, size, gain, t.sine); break;
    case OSC_SAW:   calcFmBlock(osc, t.saw, modBuffer, buf, size, gain); break;
    case OSC_TRI:   calcFmBlock(osc, t.tri, modBuffer, buf, size, gain); break;
    case OSC_REC:   calcFmBlock(osc, t.rec, modBuffer, buf, size, gain); break;
    case OSC_NOISE: calcNoiseBlock(osc, buf, size, gain, rng); break;
    case OSC_CRASH: calcSampleOscFmBlock(osc, modBuffer, buf, size, gain, t.crash); break;
    default:        fillZero(buf, size); break;   // UNSURE: user samples not ported
    }
}

} // namespace lxr
