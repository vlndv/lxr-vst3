// FILE: dsp/ParameterArray.cpp
#include "ParameterArray.h"
#include "ParamMapEnv.h"
#include "ParamMapMisc.h"
#include "Envelopes.h"

namespace lxr {

namespace {
void setWave(OscInfo& osc, uint8_t value) { osc.waveform = value; }
void setCoarse(OscInfo& osc, uint8_t value) { osc.midiFreq = (value << 8) | (osc.midiFreq & 0xFF); }
void setFine(OscInfo& osc, uint8_t value) { osc.midiFreq = (osc.midiFreq & 0xFF00) | value; }
void setFilterFreq(ResonantFilter& filter, uint8_t value) { filter.f = cutoffShape(value); filter.recalcFreq(); }
void setFilterReso(ResonantFilter& filter, uint8_t value) {
    float q = 1.0f - value / 127.0f;
    filter.q = (q < 0.1f) ? 0.02f : q;
}
void setFilterDrive(ResonantFilter& filter, uint8_t value) { filter.setDrive(value); }
void setFilterType(uint8_t& filterType, uint8_t value) { filterType = filterTypeFromParam(value); }
void setAmpAttack(AmpEg& eg, uint8_t value) { eg.attack = egAttackStep(value); }
void setAmpDecay(AmpEg& eg, uint8_t value) { eg.decay = egDecayStep(value); }
void setAmpSlope(AmpEg& eg, uint8_t value) {
    AmpSlope s = ampEgSlope(value);
    eg.slope = s.slope;
    eg.invSlope = s.invSlope;
}
void setVolume(float& vol, uint8_t value) { vol = value / 127.0f; }
}

void ParameterArray::init() {
    for (int i = 0; i < kNumParams; i++) values[i] = 0;
    values[1] = values[2] = values[3] = values[4] = 1;
    values[6] = values[7] = 1;
    values[20] = values[21] = values[22] = 1;
    values[23] = values[24] = values[25] = values[26] = 1;
    values[8] = values[10] = values[12] = values[14] = values[16] = values[18] = 63;
    values[9] = values[11] = values[13] = values[15] = values[17] = values[19] = 63;
    for (int i = 37; i <= 42; i++) values[i] = 64;
    for (int i = 43; i <= 48; i++) values[i] = 64;
    for (int i = 49; i <= 59; i += 2) values[i] = 127;
    for (int i = 50; i <= 61; i += 2) values[i] = 64;
    for (int i = 62; i <= 67; i++) values[i] = 0;
    for (int i = 88; i <= 93; i++) values[i] = 100;
    values[94] = values[95] = values[96] = 63;
    values[99] = values[100] = values[101] = 63;
    for (int i = 102; i <= 107; i++) values[i] = 0;
    for (int i = 191; i <= 196; i++) values[i] = 0;
    values[134] = values[135] = values[136] = 1;
    for (int i = 137; i <= 142; i++) values[i] = 1;
}

void ParameterArray::set(uint8_t par, uint8_t value, Engine& engine) {
    if (par >= kNumParams) return;
    values[par] = value;
    
    switch (par) {
        case 1: setWave(engine.drums[0].osc, value); break;
        case 2: setWave(engine.drums[1].osc, value); break;
        case 3: setWave(engine.drums[2].osc, value); break;
        case 4: setWave(engine.snare.osc, value); break;
        case 6: setWave(engine.cymbal.osc, value); break;
        case 7: setWave(engine.hihat.osc, value); break;
        case 20: setWave(engine.drums[0].modOsc, value); break;
        case 21: setWave(engine.drums[1].modOsc, value); break;
        case 22: setWave(engine.drums[2].modOsc, value); break;
        case 23: setWave(engine.cymbal.modOsc, value); break;
        case 24: setWave(engine.cymbal.modOsc2, value); break;
        case 25: setWave(engine.hihat.modOsc, value); break;
        case 26: setWave(engine.hihat.modOsc2, value); break;
        
        case 8: setCoarse(engine.drums[0].osc, value); break;
        case 10: setCoarse(engine.drums[1].osc, value); break;
        case 12: setCoarse(engine.drums[2].osc, value); break;
        case 14: setCoarse(engine.snare.osc, value); break;
        case 16: setCoarse(engine.cymbal.osc, value); break;
        case 18: setCoarse(engine.hihat.osc, value); break;
        
        case 9: setFine(engine.drums[0].osc, value); break;
        case 11: setFine(engine.drums[1].osc, value); break;
        case 13: setFine(engine.drums[2].osc, value); break;
        case 15: setFine(engine.snare.osc, value); break;
        case 17: setFine(engine.cymbal.osc, value); break;
        case 19: setFine(engine.hihat.osc, value); break;
        
        case 27: engine.snare.noiseOsc.freq = noiseFrequencyHz(value); break;
        case 28: engine.snare.mix = value / 127.0f; break;
        
        case 29: engine.cymbal.modOsc.freq = noiseFrequencyHz(value); break;
        case 30: engine.cymbal.modOsc2.freq = noiseFrequencyHz(value); break;
        case 31: engine.cymbal.fmModAmount1 = value / 127.0f; break;
        case 32: engine.cymbal.fmModAmount2 = value / 127.0f; break;
        case 33: engine.drums[0].modOsc.freq = noiseFrequencyHz(value); break;
        case 34: engine.drums[1].modOsc.freq = noiseFrequencyHz(value); break;
        case 35: engine.drums[2].modOsc.freq = noiseFrequencyHz(value); break;
        case 36: engine.hihat.modOsc.freq = noiseFrequencyHz(value); break;
        
        case 37: setFilterFreq(engine.drums[0].filter, value); break;
        case 38: setFilterFreq(engine.drums[1].filter, value); break;
        case 39: setFilterFreq(engine.drums[2].filter, value); break;
        case 40: setFilterFreq(engine.snare.filter, value); break;
        case 41: setFilterFreq(engine.cymbal.filter, value); break;
        case 42: setFilterFreq(engine.hihat.filter, value); break;
        
        case 43: setFilterReso(engine.drums[0].filter, value); break;
        case 44: setFilterReso(engine.drums[1].filter, value); break;
        case 45: setFilterReso(engine.drums[2].filter, value); break;
        case 46: setFilterReso(engine.snare.filter, value); break;
        case 47: setFilterReso(engine.cymbal.filter, value); break;
        case 48: setFilterReso(engine.hihat.filter, value); break;
        
        case 49: setAmpAttack(engine.drums[0].oscVolEg, value); break;
        case 51: setAmpAttack(engine.drums[1].oscVolEg, value); break;
        case 53: setAmpAttack(engine.drums[2].oscVolEg, value); break;
        case 55: setAmpAttack(engine.snare.oscVolEg, value); break;
        case 57: setAmpAttack(engine.cymbal.oscVolEg, value); break;
        case 59: setAmpAttack(engine.hihat.oscVolEg, value); break;
        
        case 50: setAmpDecay(engine.drums[0].oscVolEg, value); break;
        case 52: setAmpDecay(engine.drums[1].oscVolEg, value); break;
        case 54: setAmpDecay(engine.drums[2].oscVolEg, value); break;
        case 56: setAmpDecay(engine.snare.oscVolEg, value); break;
        case 58: setAmpDecay(engine.cymbal.oscVolEg, value); break;
        case 60: engine.hihat.decayClosed = egDecayStep(value); break;
        case 61: engine.hihat.decayOpen = egDecayStep(value); break;
        
        case 62: setAmpSlope(engine.drums[0].oscVolEg, value); break;
        case 63: setAmpSlope(engine.drums[1].oscVolEg, value); break;
        case 64: setAmpSlope(engine.drums[2].oscVolEg, value); break;
        case 65: setAmpSlope(engine.snare.oscVolEg, value); break;
        case 66: setAmpSlope(engine.cymbal.oscVolEg, value); break;
        case 67: setAmpSlope(engine.hihat.oscVolEg, value); break;
        
        // === Repeat (SN, CY only) ===
        case 68: engine.snare.oscVolEg.repeat = value; break;
        case 69: engine.cymbal.oscVolEg.repeat = value; break;
        
        // === Pitch EG decay (D1-D3, SN) ===
        case 70: engine.drums[0].oscPitchEg.decay = pitchEgDecayStep(value); break;
        case 71: engine.drums[1].oscPitchEg.decay = pitchEgDecayStep(value); break;
        case 72: engine.drums[2].oscPitchEg.decay = pitchEgDecayStep(value); break;
        case 73: engine.snare.oscPitchEg.decay = pitchEgDecayStep(value); break;
        
        // === Pitch EG amount (D1-D3, SN) ===
        case 74: engine.drums[0].egPitchModAmount = pitchModAmount(value); break;
        case 75: engine.drums[1].egPitchModAmount = pitchModAmount(value); break;
        case 76: engine.drums[2].egPitchModAmount = pitchModAmount(value); break;
        case 77: engine.snare.egPitchModAmount = pitchModAmount(value); break;
        
        // === Pitch EG slope (D1-D3, SN) ===
        case 78: engine.drums[0].oscPitchEg.slope = pitchEgSlope(value); break;
        case 79: engine.drums[1].oscPitchEg.slope = pitchEgSlope(value); break;
        case 80: engine.drums[2].oscPitchEg.slope = pitchEgSlope(value); break;
        case 81: engine.snare.oscPitchEg.slope = pitchEgSlope(value); break;
        
        // === FM amount / FM freq alternating (D1, D2, D3) ===
        case 82: engine.drums[0].fmModAmount = value / 127.0f; break;
        case 83: setCoarse(engine.drums[0].modOsc, value); break;
        case 84: engine.drums[1].fmModAmount = value / 127.0f; break;
        case 85: setCoarse(engine.drums[1].modOsc, value); break;
        case 86: engine.drums[2].fmModAmount = value / 127.0f; break;
        case 87: setCoarse(engine.drums[2].modOsc, value); break;
        
        case 88: setVolume(engine.drums[0].vol, value); break;
        case 89: setVolume(engine.drums[1].vol, value); break;
        case 90: setVolume(engine.drums[2].vol, value); break;
        case 91: setVolume(engine.snare.vol, value); break;
        case 92: setVolume(engine.cymbal.vol, value); break;
        case 93: setVolume(engine.hihat.vol, value); break;
        
        case 94: engine.mixer.setPan(0, value); break;
        case 95: engine.mixer.setPan(1, value); break;
        case 96: engine.mixer.setPan(2, value); break;
        case 99: engine.mixer.setPan(3, value); break;
        case 100: engine.mixer.setPan(4, value); break;
        case 101: engine.mixer.setPan(5, value); break;
        
        case 102: engine.drums[0].distortion.setShape(value); break;
        case 103: engine.drums[1].distortion.setShape(value); break;
        case 104: engine.drums[2].distortion.setShape(value); break;
        case 105: engine.snare.distortion.setShape(value); break;
        case 106: engine.cymbal.distortion.setShape(value); break;
        case 107: engine.hihat.distortion.setShape(value); break;
        
        case 108: case 109: case 110: case 111: case 112: case 113: case 114: break;
        
        case 128: setFilterDrive(engine.drums[0].filter, value); break;
        case 129: setFilterDrive(engine.drums[1].filter, value); break;
        case 130: setFilterDrive(engine.drums[2].filter, value); break;
        case 131: setFilterDrive(engine.snare.filter, value); break;
        case 132: setFilterDrive(engine.cymbal.filter, value); break;
        case 133: setFilterDrive(engine.hihat.filter, value); break;
        
        case 134: engine.drums[0].mixOscs = (value != 0); break;
        case 135: engine.drums[1].mixOscs = (value != 0); break;
        case 136: engine.drums[2].mixOscs = (value != 0); break;
        
        case 137: engine.drums[0].volumeMod = (value != 0); break;
        case 138: engine.drums[1].volumeMod = (value != 0); break;
        case 139: engine.drums[2].volumeMod = (value != 0); break;
        case 140: engine.snare.volumeMod = (value != 0); break;
        case 141: engine.cymbal.volumeMod = (value != 0); break;
        case 142: engine.hihat.volumeMod = (value != 0); break;
        
        case 143: case 144: case 145: case 146: case 147: case 148: break;
        case 149: case 150: case 151: case 152: case 153: case 154: break;
        
        case 191: setFilterType(engine.drums[0].filterType, value); break;
        case 192: setFilterType(engine.drums[1].filterType, value); break;
        case 193: setFilterType(engine.drums[2].filterType, value); break;
        case 194: setFilterType(engine.snare.filterType, value); break;
        case 195: setFilterType(engine.cymbal.filterType, value); break;
        case 196: setFilterType(engine.hihat.filterType, value); break;
        
        case 197: engine.drums[0].transGen.volume = value / 127.0f; break;
        case 198: engine.drums[1].transGen.volume = value / 127.0f; break;
        case 199: engine.drums[2].transGen.volume = value / 127.0f; break;
        case 200: engine.snare.transGen.volume = value / 127.0f; break;
        case 201: engine.cymbal.transGen.volume = value / 127.0f; break;
        case 202: engine.hihat.transGen.volume = value / 127.0f; break;
        
        case 203: engine.drums[0].transGen.waveform = transientWaveform(value); break;
        case 204: engine.drums[1].transGen.waveform = transientWaveform(value); break;
        case 205: engine.drums[2].transGen.waveform = transientWaveform(value); break;
        case 206: engine.snare.transGen.waveform = transientWaveform(value); break;
        case 207: engine.cymbal.transGen.waveform = transientWaveform(value); break;
        case 208: engine.hihat.transGen.waveform = transientWaveform(value); break;
        
        case 209: engine.drums[0].transGen.pitch = transientPitch(value); break;
        case 210: engine.drums[1].transGen.pitch = transientPitch(value); break;
        case 211: engine.drums[2].transGen.pitch = transientPitch(value); break;
        case 212: engine.snare.transGen.pitch = transientPitch(value); break;
        case 213: engine.cymbal.transGen.pitch = transientPitch(value); break;
        case 214: engine.hihat.transGen.pitch = transientPitch(value); break;
        
        case 215: case 216: case 217: case 218: case 219: case 220: break;
        
        default: break;
    }
}

uint8_t ParameterArray::get(uint8_t par) const {
    if (par >= kNumParams) return 0;
    return values[par];
}

void ParameterArray::applyAllToEngine(Engine& engine) {
    for (int i = 0; i < kNumParams; i++) {
        set(i, values[i], engine);
    }
}

} // namespace lxr