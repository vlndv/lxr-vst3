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
void setFilterFreq(ResonantFilter& filter, uint8_t value) { filter.f = cutoffShape(value); }
void setFilterReso(ResonantFilter& filter, uint8_t value) {
    float q = 1.0f - value / 127.0f;
    filter.q = (q < 0.1f) ? 0.02f : q;
}
void setAmpAttack(AmpEg& eg, uint8_t value) { eg.attack = egAttackStep(value); }
void setAmpDecay(AmpEg& eg, uint8_t value) { eg.decay = egDecayStep(value); }
void setVolume(float& vol, uint8_t value) { vol = value / 127.0f; }
}

void ParameterArray::init() {
    for (int i = 0; i < kNumParams; i++) values[i] = 0;
    values[1] = values[2] = values[3] = 1;
    values[4] = values[6] = values[7] = 1;
    values[8] = values[10] = values[12] = 63;
    values[14] = values[16] = values[18] = 63;
    values[9] = values[11] = values[13] = 63;
    values[15] = values[17] = values[19] = 63;
    values[88] = values[89] = values[90] = 100;
    values[91] = values[92] = values[93] = 100;
    values[94] = values[95] = values[96] = 63;
    values[99] = values[100] = values[101] = 63;
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