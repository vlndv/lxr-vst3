// FILE: dsp/Engine.h
#pragma once
#include <cstdint>
#include "DrumVoice.h"
#include "Snare.h"
#include "Cymbal.h"
#include "HiHat.h"
#include "Mixer.h"
#include "OscTables.h"
#include "Modulation.h"
#include "VelocityRouting.h"
#include "PhysicalModulation.h"
#include "SequencerParameterLocks.h"

namespace lxr {

constexpr float kEngineSampleRate = 44002.7573529412f;

class Engine {
public:
    DrumVoice drums[3];
    SnareVoice snare;
    CymbalVoice cymbal;
    HiHatVoice hihat;
    Mixer mixer;
    
    VelocityModulator velocityModulators[6];
    
    OscTables tables;
    float noteFreq[128];

    Engine();
    void init();
    bool loadAssets(const char* dataDir);

    // Internal, transient configuration. Call ONLY on the processing owner (or
    // while processing is stopped). Neither host selector bytes nor LXRS state.
    VelocityRouteStatus configureVelocityDestination(modulation_contract::SourceIdentity source,
                                                     modulation_contract::OriginalTargetIndex target) noexcept;
    VelocityRouteStatus configureLegacyVelocitySelector(modulation_contract::HostParameterId selector,
                                                        modulation_contract::SevenBitValue value) noexcept;
    const VelocityRouteState* velocityRoute(modulation_contract::Voice voice) const noexcept;
    void velocityBaseParameterChanged(modulation_contract::HostParameterId parameter) noexcept;
    void authoritativeBaseParameterChanged(
        modulation_contract::HostParameterId parameter,
        std::uint8_t rawValue) noexcept;
    bool applySequencerAutomationPair(
        std::uint8_t track,
        const sequencer::AutomationSlotCommand* slot1,
        const sequencer::AutomationSlotCommand* slot2) noexcept;
    modulation_contract::HostParameterId sequencerAutomationDestination(
        std::uint8_t track, std::uint8_t slot) const noexcept;
    // Original mixer stage: restore converted bases, replay held sources, then
    // recalculate affected filter coefficients. Exactly once per native tick.
    void beginVelocityModulationTick() noexcept;
    LfoRouteStatus configureLfoDestination(modulation_contract::SourceIdentity source,
                                          modulation_contract::OriginalTargetIndex target) noexcept;
    const LfoRouteState* lfoRoute(modulation_contract::Voice voice) const noexcept;
    Lfo* nativeLfo(unsigned owner) noexcept;
    const Lfo* nativeLfo(unsigned owner) const noexcept;
    bool setNativeTempo(uint16_t bpm) noexcept;
    uint16_t nativeTempo() const noexcept {return mNativeTempo;}
    void beginModulationTick() noexcept;

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

private:
    VelocityRouteState mVelocityRoutes[6] {};
    LfoRouteState mLfoRoutes[6] {};
    PhysicalModulationBase mPhysicalBases[12] {};
    uint16_t mNativeTempo=120;
    uint32_t mBaseRevision=0;
    bool mVelocityFilterDirty[6] {};
    std::uint8_t mAuthoritativeRawParameters[modulation_contract::kSoundSlotCount] {};
    bool mAuthoritativeRawKnown[modulation_contract::kSoundSlotCount] {};
    modulation_contract::HostParameterId mSequencerAutomationDestinations[14] {};
    float* velocityFloatField(modulation_contract::FirmwareParameterId parameter) noexcept;
    std::uint8_t* velocityByteField(modulation_contract::FirmwareParameterId parameter) noexcept;
    VelocityRouteStatus configureModulationDestination(unsigned node,
        modulation_contract::OriginalTargetIndex target) noexcept;
    void restorePhysicalBases() noexcept;
    void recalculateModulatedFilters() noexcept;
    void applyModulationValue(VelocityRouteState& route,float depth,float value) noexcept;
    void applyVelocityValue(unsigned voice) noexcept;
    bool triggerVelocity(unsigned voice, std::uint8_t velocity) noexcept;
    bool applySequencerRawParameter(modulation_contract::HostParameterId parameter,
                                    std::uint8_t value) noexcept;
    bool setSequencerAutomationDestination(
        std::size_t node,
        const sequencer::AutomationSlotCommand& command) noexcept;
};

} // namespace lxr
