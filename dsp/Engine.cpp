// FILE: dsp/Engine.cpp
#include "Engine.h"
#include "TransientTables.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace lxr {

Engine::Engine() {
    init();
}

void Engine::init() {
    mNativeTempo=120; mBaseRevision=0;
    for (auto& base:mPhysicalBases) base=PhysicalModulationBase{};
    for (unsigned v=0;v<6;++v) {
        mLfoRoutes[v]=LfoRouteState{};
        nativeLfo(v)->heldValue=0.f;
        nativeLfo(v)->rng.state=0x9e3779b9u;
        nativeLfo(v)->init();
        mVelocityRoutes[v]=VelocityRouteState{};
        mVelocityFilterDirty[v]=false;
        velocityModulators[v].init();
    }
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
    
    static int16_t sineBuf[4097];
    snprintf(path, sizeof(path), "%s/sine_table.bin", dataDir);
    if (!readBin(path, sineBuf, sizeof(sineBuf))) return false;
    tables.sine = sineBuf;

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

    static uint8_t crashBuf[32768];
    snprintf(path, sizeof(path), "%s/crash_sample.bin", dataDir);
    if (!readBin(path, crashBuf, sizeof(crashBuf))) return false;
    tables.crash = crashBuf;

    // Transient sample tables are part of the native voice resource contract.
    // Loading occurs during plugin initialization, never on the audio thread.
    if (!gTransientTables.load(dataDir)) return false;

    return true;
}

void Engine::triggerDrum(uint8_t voiceIdx, uint8_t vel, uint8_t note) {
    if (voiceIdx < 3) {
        if (!triggerVelocity(voiceIdx,vel)) return;
        drums[voiceIdx].trigger(vel, note, noteFreq);
    }
}

void Engine::triggerSnare(uint8_t vel, uint8_t note) {
    if (!triggerVelocity(3,vel)) return;
    snare.trigger(vel, note, noteFreq);
}

void Engine::triggerCymbal(uint8_t vel, uint8_t note) {
    if (!triggerVelocity(4,vel)) return;
    cymbal.trigger(vel, note, noteFreq);
}

void Engine::triggerHiHat(uint8_t vel, bool isOpen, uint8_t note) {
    if (!triggerVelocity(5,vel)) return;
    hihat.trigger(vel, isOpen ? 1 : 0, note, noteFreq);
}

void Engine::processBlock(int16_t* outSt1L, int16_t* outSt1R, 
                          int16_t* outSt2L, int16_t* outSt2R) {
    beginModulationTick();
    int16_t* voiceBufs[6];
    int16_t v0[32], v1[32], v2[32], v3[32], v4[32], v5[32];
    
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

// 808 Kick: Direct float assignment to bypass mapping quirks
void Engine::setup808Kick(uint8_t voiceIdx) {
    if (voiceIdx >= 3) return;
    DrumVoice& d = drums[voiceIdx];
    
    d.osc.waveform = OSC_SINE;
    d.osc.midiFreq = (40 << 8) | 64; // Note 40 (E2), fine=64 (center)
    
    // Direct envelope state (1.0 = instant attack, 0.002 = ~1s decay)
    d.oscVolEg.attack = 1.0f;
    d.oscVolEg.decay = 0.002f;
    d.oscVolEg.slope = 0.0f;
    d.oscVolEg.invSlope = 0.0f;
    
    // Pitch sweep
    d.oscPitchEg.decay = 0.01f;
    d.oscPitchEg.slope = 0.0f;
    d.egPitchModAmount = 2.0f; // Sweeps down 2 octaves
    
    // Filter (safe values well below 0.45 NaN threshold)
    d.filterType = FILTER_LP;
    d.filter.f = 0.05f; 
    d.filter.q = 0.2f;
    d.filter.drive = 1.0f;
    d.filter.recalcFreq();
    
    d.transGen.waveform = 0;
    d.transGen.volume = 0.0f;
    d.vol = 0.9f;
}

// 808 Snare
void Engine::setup808Snare() {
    snare.osc.waveform = OSC_TRI;
    snare.noiseOsc.waveform = OSC_NOISE;
    snare.mix = 0.5f;
    
    snare.oscVolEg.attack = 1.0f;
    snare.oscVolEg.decay = 0.005f; // ~200ms
    snare.oscVolEg.slope = 0.0f;
    snare.oscVolEg.invSlope = 0.0f;
    
    snare.filterType = FILTER_HP;
    snare.filter.f = 0.1f;
    snare.filter.q = 0.2f;
    snare.filter.drive = 1.0f;
    snare.filter.recalcFreq();
    
    snare.transGen.waveform = 0;
    snare.transGen.volume = 0.0f;
    snare.vol = 0.8f;
}

// 808 Hi-Hat Closed
void Engine::setup808HiHatClosed() {
    hihat.osc.waveform = OSC_NOISE;
    hihat.modOsc.waveform = OSC_SINE;
    hihat.modOsc2.waveform = OSC_NOISE;
    
    hihat.oscVolEg.attack = 1.0f;
    hihat.decayClosed = 0.02f; // 50ms
    
    hihat.filterType = FILTER_HP;
    hihat.filter.f = 0.2f; // High
    hihat.filter.q = 0.2f;
    hihat.filter.drive = 1.0f;
    hihat.filter.recalcFreq();
    
    hihat.vol = 0.6f;
}

// 808 Hi-Hat Open
void Engine::setup808HiHatOpen() {
    setup808HiHatClosed();
    hihat.decayOpen = 0.005f; // 200ms
}

// 808 Cymbal
void Engine::setup808Cymbal() {
    cymbal.osc.waveform = OSC_NOISE;
    cymbal.modOsc.waveform = OSC_SINE;
    cymbal.modOsc2.waveform = OSC_NOISE;
    
    cymbal.oscVolEg.attack = 1.0f;
    cymbal.oscVolEg.decay = 0.002f; // Long
    
    cymbal.filterType = FILTER_HP;
    cymbal.filter.f = 0.15f;
    cymbal.filter.q = 0.2f;
    cymbal.filter.drive = 1.0f;
    cymbal.filter.recalcFreq();
    
    cymbal.vol = 0.7f;
}

// 808 Tom
void Engine::setup808Tom(uint8_t voiceIdx) {
    if (voiceIdx >= 3) return;
    DrumVoice& d = drums[voiceIdx];
    
    d.osc.waveform = OSC_SINE;
    d.osc.midiFreq = (50 << 8) | 64; // Note 50 (D3)
    
    d.oscVolEg.attack = 1.0f;
    d.oscVolEg.decay = 0.003f;
    d.oscVolEg.slope = 0.0f;
    d.oscVolEg.invSlope = 0.0f;
    
    d.oscPitchEg.decay = 0.008f;
    d.oscPitchEg.slope = 0.0f;
    d.egPitchModAmount = 1.0f;
    
    d.filterType = FILTER_LP;
    d.filter.f = 0.08f;
    d.filter.q = 0.2f;
    d.filter.drive = 1.0f;
    d.filter.recalcFreq();
    
    d.transGen.waveform = 0;
    d.transGen.volume = 0.0f;
    d.vol = 0.85f;
}

void Engine::setup808Kit() {
    setup808Kick(0);
    setup808Tom(1);
    setup808Tom(2);
    drums[2].osc.midiFreq = (55 << 8) | 64; // Higher tom
    
    setup808Snare();
    setup808Cymbal();
    setup808HiHatClosed();
}


// Source-backed subset of original ParameterArray.c; destinations are resolved
// on this Engine each time, so Engine copies never retain another instance's pointers.
float* Engine::velocityFloatField(modulation_contract::FirmwareParameterId id) noexcept {
    if (!id.valid()) return nullptr;
    switch (id.value()) {
        case 8: return &drums[0].osc.modNodeValue;
        case 9: return &drums[0].osc.modNodeValue;
        case 49: return &drums[0].oscVolEg.attack;
        case 50: return &drums[0].oscVolEg.decay;
        case 62: return &drums[0].oscVolEg.slope;
        case 70: return &drums[0].oscPitchEg.decay;
        case 74: return &drums[0].egPitchModAmount;
        case 82: return &drums[0].fmModAmount;
        case 83: return &drums[0].modOsc.modNodeValue;
        case 209: return &drums[0].transGen.pitch;
        case 37: return &drums[0].filter.f;
        case 43: return &drums[0].filter.q;
        case 128: return &drums[0].filter.drive;
        case 88: return &drums[0].vol;
        case 108: return &mixer.decimationRate[0];
        case 102: return &drums[0].distortion.shape;
        case 10: return &drums[1].osc.modNodeValue;
        case 11: return &drums[1].osc.modNodeValue;
        case 51: return &drums[1].oscVolEg.attack;
        case 52: return &drums[1].oscVolEg.decay;
        case 63: return &drums[1].oscVolEg.slope;
        case 71: return &drums[1].oscPitchEg.decay;
        case 75: return &drums[1].egPitchModAmount;
        case 84: return &drums[1].fmModAmount;
        case 85: return &drums[1].modOsc.modNodeValue;
        case 210: return &drums[1].transGen.pitch;
        case 38: return &drums[1].filter.f;
        case 44: return &drums[1].filter.q;
        case 129: return &drums[1].filter.drive;
        case 89: return &drums[1].vol;
        case 109: return &mixer.decimationRate[1];
        case 103: return &drums[1].distortion.shape;
        case 12: return &drums[2].osc.modNodeValue;
        case 13: return &drums[2].osc.modNodeValue;
        case 53: return &drums[2].oscVolEg.attack;
        case 54: return &drums[2].oscVolEg.decay;
        case 64: return &drums[2].oscVolEg.slope;
        case 72: return &drums[2].oscPitchEg.decay;
        case 76: return &drums[2].egPitchModAmount;
        case 86: return &drums[2].fmModAmount;
        case 87: return &drums[2].modOsc.modNodeValue;
        case 211: return &drums[2].transGen.pitch;
        case 39: return &drums[2].filter.f;
        case 45: return &drums[2].filter.q;
        case 130: return &drums[2].filter.drive;
        case 90: return &drums[2].vol;
        case 110: return &mixer.decimationRate[2];
        case 104: return &drums[2].distortion.shape;
        case 14: return &snare.osc.modNodeValue;
        case 15: return &snare.osc.modNodeValue;
        case 27: return &snare.noiseOsc.modNodeValue;
        case 28: return &snare.mix;
        case 55: return &snare.oscVolEg.attack;
        case 56: return &snare.oscVolEg.decay;
        case 65: return &snare.oscVolEg.slope;
        case 73: return &snare.oscPitchEg.decay;
        case 77: return &snare.egPitchModAmount;
        case 212: return &snare.transGen.pitch;
        case 40: return &snare.filter.f;
        case 46: return &snare.filter.q;
        case 131: return &snare.filter.drive;
        case 91: return &snare.vol;
        case 111: return &mixer.decimationRate[3];
        case 105: return &snare.distortion.shape;
        case 16: return &cymbal.osc.modNodeValue;
        case 17: return &cymbal.osc.modNodeValue;
        case 57: return &cymbal.oscVolEg.attack;
        case 58: return &cymbal.oscVolEg.decay;
        case 66: return &cymbal.oscVolEg.slope;
        case 29: return &cymbal.modOsc.modNodeValue;
        case 30: return &cymbal.modOsc2.modNodeValue;
        case 31: return &cymbal.fmModAmount1;
        case 32: return &cymbal.fmModAmount2;
        case 213: return &cymbal.transGen.pitch;
        case 41: return &cymbal.filter.f;
        case 47: return &cymbal.filter.q;
        case 132: return &cymbal.filter.drive;
        case 92: return &cymbal.vol;
        case 112: return &mixer.decimationRate[4];
        case 106: return &cymbal.distortion.shape;
        case 18: return &hihat.osc.modNodeValue;
        case 19: return &hihat.osc.modNodeValue;
        case 59: return &hihat.oscVolEg.attack;
        case 67: return &hihat.oscVolEg.slope;
        case 33: return &hihat.modOsc.modNodeValue;
        case 34: return &hihat.modOsc2.modNodeValue;
        case 35: return &hihat.fmModAmount1;
        case 36: return &hihat.fmModAmount2;
        case 214: return &hihat.transGen.pitch;
        case 42: return &hihat.filter.f;
        case 48: return &hihat.filter.q;
        case 133: return &hihat.filter.drive;
        case 93: return &hihat.vol;
        case 113: return &mixer.decimationRate[5];
        case 107: return &hihat.distortion.shape;
        default: return nullptr;
    }
}
std::uint8_t* Engine::velocityByteField(modulation_contract::FirmwareParameterId id) noexcept {
    if (!id.valid()) return nullptr;
    switch (id.value()) {
        case 191: return &drums[0].filterType;
        case 94: return &mixer.pan[0];
        case 192: return &drums[1].filterType;
        case 95: return &mixer.pan[1];
        case 193: return &drums[2].filterType;
        case 96: return &mixer.pan[2];
        case 194: return &snare.filterType;
        case 99: return &mixer.pan[3];
        case 195: return &cymbal.filterType;
        case 100: return &mixer.pan[4];
        case 196: return &hihat.filterType;
        case 101: return &mixer.pan[5];
        default: return nullptr;
    }
}

const VelocityRouteState* Engine::velocityRoute(modulation_contract::Voice voice) const noexcept {
    return modulation_contract::valid(voice)?&mVelocityRoutes[static_cast<unsigned>(voice)]:nullptr;
}

Lfo* Engine::nativeLfo(unsigned v) noexcept {
    return v<3?&drums[v].lfo:v==3?&snare.lfo:v==4?&cymbal.lfo:v==5?&hihat.lfo:nullptr;
}
const Lfo* Engine::nativeLfo(unsigned v) const noexcept {
    return v<3?&drums[v].lfo:v==3?&snare.lfo:v==4?&cymbal.lfo:v==5?&hihat.lfo:nullptr;
}
bool Engine::setNativeTempo(uint16_t bpm) noexcept {
    // Explicit native service, not a host transport connection. Verified domain.
    if (bpm>300) return false;
    mNativeTempo=bpm;
    for(unsigned v=0;v<6;++v) nativeLfo(v)->setSync(nativeLfo(v)->sync,bpm);
    return true;
}
const LfoRouteState* Engine::lfoRoute(modulation_contract::Voice v) const noexcept {
    return modulation_contract::valid(v)?&mLfoRoutes[static_cast<unsigned>(v)]:nullptr;
}
void Engine::restorePhysicalBases() noexcept {
    using namespace modulation_contract;
    for (const auto& base:mPhysicalBases) {
        if (!base.sources) continue;
        if (!std::isfinite(base.value)) continue;
        if (base.type==ParameterValueType::UInt8)
            *velocityByteField(base.parameter)=static_cast<uint8_t>(base.value);
        else *velocityFloatField(base.parameter)=base.value;
        const auto p=base.parameter.value();
        if(p>=37&&p<=42) mVelocityFilterDirty[p-37]=true;
    }
}
VelocityRouteStatus Engine::configureModulationDestination(unsigned node,
    modulation_contract::OriginalTargetIndex index) noexcept {
    using namespace modulation_contract;
    auto& route=node<6?mVelocityRoutes[node]:mLfoRoutes[node-6];
    const auto* target=lookup(index);
    const float depth=node<6?velocityModulators[node].amount:nativeLfo(node-6)->amount;
    if(index.value()!=0 && (!std::isfinite(depth)||depth<0.f||depth>1.f))
        return VelocityRouteStatus::InvalidDepth;
    PhysicalModulationBase candidate;
    int shared=-1;
    if(index.value()!=0) {
        candidate.destination=target->destination;candidate.parameter=target->parameter;
        candidate.type=target->valueType;
        for(unsigned i=0;i<12;++i) if(mPhysicalBases[i].sources &&
            mPhysicalBases[i].destination==candidate.destination) shared=static_cast<int>(i);
        if(shared>=0) candidate=mPhysicalBases[shared];
        else if(candidate.type==ParameterValueType::UInt8) {
            const auto* field=velocityByteField(candidate.parameter);
            if(!field) return VelocityRouteStatus::UnsupportedTarget;
            candidate.value=*field;
        } else {
            const auto* field=velocityFloatField(candidate.parameter);
            if(!field) return VelocityRouteStatus::UnsupportedTarget;
            candidate.value=candidate.type==ParameterValueType::FrequencyMultiplier?1.f:*field;
        }
        if(!std::isfinite(candidate.value)) return VelocityRouteStatus::NonFiniteBase;
    }
    // ORIGINAL QUIRK: a valid route change restores ALL participating fields,
    // with no velocity/LFO replay until the next native boundary.
    restorePhysicalBases();
    const uint16_t bit=static_cast<uint16_t>(1u<<node);
    for(auto& base:mPhysicalBases) base.sources &= static_cast<uint16_t>(~bit);
    route=VelocityRouteState{};
    if(index.value()==0) return VelocityRouteStatus::Ok;
    if(shared<0) for(unsigned i=0;i<12;++i) if(!mPhysicalBases[i].sources) {shared=static_cast<int>(i);break;}
    // Twelve sources imply a free/reusable slot; no unbounded fallback.
    auto& base=mPhysicalBases[shared];
    if(!base.sources) {base=candidate;base.sources=0;base.revision=++mBaseRevision;}
    base.sources|=bit;
    route.target=index;route.destination=target->destination;route.type=target->valueType;
    route.baseFloat=base.value;route.baseByte=base.type==ParameterValueType::UInt8?static_cast<uint8_t>(base.value):0;
    route.enabled=true;route.status=VelocityRouteStatus::Ok;
    return VelocityRouteStatus::Ok;
}
VelocityRouteStatus Engine::configureVelocityDestination(modulation_contract::SourceIdentity source,
    modulation_contract::OriginalTargetIndex index) noexcept {
    const auto status=validateVelocityDestination(source,index);
    return status==VelocityRouteStatus::Ok?configureModulationDestination(static_cast<unsigned>(source.owner),index):status;
}
LfoRouteStatus Engine::configureLfoDestination(modulation_contract::SourceIdentity source,
    modulation_contract::OriginalTargetIndex index) noexcept {
    const auto status=validateLfoDestination(source,index);
    return status==LfoRouteStatus::Ok?configureModulationDestination(6+static_cast<unsigned>(source.owner),index):status;
}

VelocityRouteStatus Engine::configureLegacyVelocitySelector(
    modulation_contract::HostParameterId id,
    modulation_contract::SevenBitValue value) noexcept {
    if (!id.valid()||id.value()<149||id.value()>154) return VelocityRouteStatus::UnsupportedTarget;
    if (!value.valid()) return VelocityRouteStatus::InvalidLegacyValue;
    return VelocityRouteStatus::AmbiguousLegacySelector;
}

void Engine::velocityBaseParameterChanged(modulation_contract::HostParameterId id) noexcept {
    using namespace modulation_contract;
    if(!id.valid()) return;
    for(auto& base:mPhysicalBases) {
        if(!base.sources || base.type==ParameterValueType::FrequencyMultiplier || base.parameter.value()!=id.value()) continue;
        base.value=base.type==ParameterValueType::UInt8?*velocityByteField(base.parameter):*velocityFloatField(base.parameter);
        base.revision=++mBaseRevision;
        // Compatibility inspection views only. Restoration reads the registry.
        for(unsigned n=0;n<12;++n) if(base.sources&(1u<<n)) {
            auto& r=n<6?mVelocityRoutes[n]:mLfoRoutes[n-6];
            r.baseFloat=base.value;
            r.baseByte=base.type==ParameterValueType::UInt8?static_cast<uint8_t>(base.value):0;
            r.status=std::isfinite(base.value)?VelocityRouteStatus::Ok:VelocityRouteStatus::NonFiniteBase;
        }
    }
}
void Engine::applyModulationValue(VelocityRouteState& route,float depth,float value) noexcept {
    if(!route.enabled) return;
    if(!std::isfinite(depth)||depth<0.f||depth>1.f) {route.status=VelocityRouteStatus::InvalidDepth;return;}
    if(!std::isfinite(value)||value<0.f||value>1.f) {route.status=VelocityRouteStatus::InvalidVelocity;return;}
    const auto* target=modulation_contract::lookup(route.target);
    // ORIGINAL QUIRK: do not factor, combine gains, or postpone per-node narrowing.
    if(route.type==modulation_contract::ParameterValueType::UInt8) {
        auto* field=velocityByteField(target->parameter);
        const float current=*field;
        *field=static_cast<uint8_t>(current*depth*value+(1.f-depth)*current);
    } else {
        auto* field=velocityFloatField(target->parameter);
        const float current=*field;
        if(!std::isfinite(current)||!std::isfinite(route.baseFloat)) {route.status=VelocityRouteStatus::NonFiniteBase;return;}
        *field=current*depth*value+(1.f-depth)*current;
    }
    if(target->semantic==modulation_contract::ValueSemantic::CutoffCyclesPerSample)
        mVelocityFilterDirty[static_cast<unsigned>(target->voice)]=true;
    route.status=VelocityRouteStatus::Ok;
}
void Engine::applyVelocityValue(unsigned v) noexcept {
    applyModulationValue(mVelocityRoutes[v],velocityModulators[v].amount,velocityModulators[v].modValue);
}

bool Engine::triggerVelocity(unsigned v,std::uint8_t velocity) noexcept {
    if (velocity>127) {mVelocityRoutes[v].status=VelocityRouteStatus::InvalidVelocity;return false;}
    for(unsigned owner=0;owner<6;++owner) nativeLfo(owner)->trigger(static_cast<uint8_t>(v));
    velocityModulators[v].updateVelocity(velocity);
    // Original trigger operates on the current field. Multiple triggers before
    // a tick can transiently compound; the next tick ALWAYS restores the base.
    applyVelocityValue(v);
    return true;
}

void Engine::beginVelocityModulationTick() noexcept {
    restorePhysicalBases();
    for (unsigned v=0;v<6;++v) applyVelocityValue(v);
    recalculateModulatedFilters();
}
void Engine::beginModulationTick() noexcept {
    restorePhysicalBases();
    for(unsigned v=0;v<6;++v) applyVelocityValue(v);
    for(unsigned v=0;v<6;++v) {
        auto& lfo=*nativeLfo(v);
        lfo.tick(tables.sine);
        applyModulationValue(mLfoRoutes[v],lfo.amount,lfo.output);
    }
    recalculateModulatedFilters();
}
void Engine::recalculateModulatedFilters() noexcept {
    // Only routed cutoff fields require a new coefficient; absent routes leave
    // the certified unmodulated path and filter initialization untouched.
    for (unsigned v=0;v<6;++v) {
        if (!mVelocityFilterDirty[v]) continue;
        if (v<3) drums[v].filter.recalcFreq();
        else if(v==3) snare.filter.recalcFreq();
        else if(v==4) cymbal.filter.recalcFreq();
        else hihat.filter.recalcFreq();
        mVelocityFilterDirty[v]=false;
    }
}

} // namespace lxr
