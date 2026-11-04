#pragma once

// Original Sonic Potions LXR 0.37 contract, not connected to the renderer.
// Copyright 2013 Julian Schmidt (original tables/semantics).
// Derived material is subject to LICENSE-LXR.txt (non-commercial, full source).
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace lxr::modulation_contract {

inline constexpr std::size_t kOriginalTargetCount = 205;
inline constexpr std::size_t kSoundSlotCount = 228;
inline constexpr std::size_t kVoiceCount = 6;
inline constexpr std::size_t kLogicalSlotCount = 43;
inline constexpr std::size_t kDistinctStorageCount = 198;
inline constexpr std::size_t kInternalOnlyParameterCount = 7;

// Invalid construction remains invalid; no unchecked/narrowing public ctor.
// These are separate C++ types even when their numeric domains overlap.
template <class Tag, std::uint16_t Maximum>
class Id final {
public:
    constexpr Id() noexcept = default;
    static constexpr Id fromRaw(std::uint64_t raw) noexcept {
        return raw <= Maximum ? Id(static_cast<std::uint16_t>(raw)) : Id();
    }
    constexpr bool valid() const noexcept { return mValue <= Maximum; }
    constexpr std::uint16_t value() const noexcept { return mValue; }
    friend constexpr bool operator==(Id a, Id b) noexcept { return a.mValue == b.mValue; }
    friend constexpr bool operator!=(Id a, Id b) noexcept { return !(a == b); }
private:
    explicit constexpr Id(std::uint16_t raw) noexcept : mValue(raw) {}
    std::uint16_t mValue = 0xffffu;
};
struct OriginalTargetTag;
struct HostParameterTag;
struct FirmwareParameterTag;
struct SevenBitTag;
struct LogicalSlotTag;
struct OriginalVoiceSelectorTag;
using OriginalTargetIndex = Id<OriginalTargetTag, 204>;
using HostParameterId = Id<HostParameterTag, 227>;
using FirmwareParameterId = Id<FirmwareParameterTag, 227>;
using SevenBitValue = Id<SevenBitTag, 127>;
using LogicalTargetSlot = Id<LogicalSlotTag, 42>;
using OriginalVoiceSelector = Id<OriginalVoiceSelectorTag, 6>;

enum class Voice : std::uint8_t { Drum1, Drum2, Drum3, Snare, Cymbal, HiHat, Invalid = 0xff };
enum class Track : std::uint8_t { Drum1, Drum2, Drum3, Snare, Cymbal, ClosedHat, OpenHat, Invalid = 0xff };
enum class SourceKind : std::uint8_t { Velocity, Lfo };
struct SourceIdentity { SourceKind kind; Voice owner; };
constexpr bool valid(Voice v) noexcept { return static_cast<unsigned>(v) < kVoiceCount; }
constexpr Voice voiceFromZeroBased(std::uint64_t n) noexcept {
    return n < kVoiceCount ? static_cast<Voice>(n) : Voice::Invalid;
}
constexpr Voice voiceForTrack(Track t) noexcept {
    const auto n = static_cast<unsigned>(t);
    return n < 7 ? voiceFromZeroBased(n < 5 ? n : 5) : Voice::Invalid;
}
constexpr Voice destinationVoice(OriginalVoiceSelector s) noexcept {
    return s.valid() && s.value() != 0 ? voiceFromZeroBased(s.value() - 1u) : Voice::Invalid;
}
constexpr OriginalVoiceSelector originalVoiceSelector(Voice v) noexcept {
    return valid(v) ? OriginalVoiceSelector::fromRaw(static_cast<unsigned>(v) + 1u) : OriginalVoiceSelector();
}

enum class ParameterValueType : std::uint8_t {
    UInt8 = 0, Float = 1, FrequencyMultiplier = 2, UInt32 = 3,
    SpecialPanUnused = 4, SpecialFilterUnused = 5, None = 0xff
};
enum class ValueSemantic : std::uint8_t {
    None, UnitFloat, FrequencyMultiplier, CutoffCyclesPerSample, Resonance,
    EnvelopeStep, SignedSlopeCoefficient, PitchEnvelopeAmount, Pan,
    DistortionCoefficient, FilterDrive, DecimationRate, Boolean,
    OscillatorWaveform, LfoWaveform, FilterEnumeration, RepeatCount,
    MutableDestinationLowByte, SyncEnumeration, RetriggerVoice,
    PhaseOffset, TransientWaveform, TransientPitch, MidiNoteFilter, OutputRouting
};
enum class RoutingScope : std::uint8_t { None, Voice, Global };
// Describes the NORMAL menu. Preset/internal message dispatch has fewer checks.
enum class TargetAvailability : std::uint8_t { Off, VoiceMenu, AutomationOnlyMenu, InternalMessageOnly };
enum class RuntimePolicy : std::uint8_t { NoDestination, TypedField, RequiresDestinationMutationReview };
enum class OriginalMidiDisposition : std::uint8_t {
    None, DirectStm32, Stm32NoOpAvrRelayUnverified, Stm32NoOpAvrStorageOnly
};
enum class Consumption : std::uint8_t {
    None = 0, OrderedTick = 1, AudioSample = 2, Trigger = 4,
    MidiDispatch = 8, SetterOrTempoRecalculation = 16, MutableRoute = 32
};
constexpr Consumption operator|(Consumption a, Consumption b) noexcept {
    return static_cast<Consumption>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
constexpr bool has(Consumption a, Consumption b) noexcept {
    return (static_cast<unsigned>(a) & static_cast<unsigned>(b)) != 0;
}
struct SourceLocation { const char* file; std::uint16_t line; };

// DestinationId identifies a physical storage field, not an IParam or menu row.
// Its enumerators and all original rows are defined in the generated data file.
enum class DestinationId : std::uint16_t;
struct TargetMetadata {
    OriginalTargetIndex index;
    FirmwareParameterId parameter;
    DestinationId destination;
    Voice voice;
    RoutingScope scope;
    TargetAvailability availability;
    ParameterValueType valueType;
    ValueSemantic semantic;
    Consumption consumption;
    RuntimePolicy runtimePolicy;
    OriginalMidiDisposition midi;
    std::uint16_t originalLabelId;
    const char* symbol;
    const char* labelSymbol;
    const char* storageField; // Evidence only: never a live pointer.
    const char* nominalNativeRange; // Not a new runtime clamp/interpolator.
    SourceLocation tableSource;
    SourceLocation parameterSource;
    SourceLocation setterSource;
};
struct VoiceTargetRange { OriginalTargetIndex first; OriginalTargetIndex last; };

// Generated from the pinned original AVR/STM32 sources; no runtime construction.
#include "ModulationContractData.inc"

enum class Status : std::uint8_t {
    Ok, InvalidIndex, InvalidHostParameter, InvalidSource, InvalidVoice,
    NotInTargetTable, UnavailableInMenu, VoiceMismatch, AmbiguousAlias,
    LegacyEncodingUnspecified, NoSevenBitRepresentation,
    UnsupportedSelectorParameter, InvalidSevenBitValue, InvalidInternalMessage
};
template <class T> struct Result {
    Status status;
    T value;
    constexpr explicit operator bool() const noexcept { return status == Status::Ok; }
};
constexpr const TargetMetadata* lookup(OriginalTargetIndex i) noexcept {
    return i.valid() ? &kOriginalTargets[i.value()] : nullptr;
}
constexpr const TargetMetadata* lookupFirmwareParameter(FirmwareParameterId id) noexcept {
    if (!id.valid()) return nullptr;
    for (const auto& t : kOriginalTargets) if (t.parameter == id) return &t;
    for (const auto& t : kInternalOnlyParameters) if (t.parameter == id) return &t;
    return nullptr;
}
constexpr HostParameterId hostParameter(FirmwareParameterId id) noexcept {
    return id.valid() ? HostParameterId::fromRaw(id.value()) : HostParameterId();
}
constexpr FirmwareParameterId firmwareParameter(HostParameterId id) noexcept {
    return id.valid() ? FirmwareParameterId::fromRaw(id.value()) : FirmwareParameterId();
}
constexpr Result<OriginalTargetIndex> targetForHost(HostParameterId id) noexcept {
    if (!id.valid()) return {Status::InvalidHostParameter, {}};
    for (const auto& t : kOriginalTargets)
        if (t.parameter.value() == id.value()) return {Status::Ok, t.index};
    return {Status::NotInTargetTable, {}};
}
constexpr Result<HostParameterId> hostForTarget(OriginalTargetIndex index) noexcept {
    const auto* t = lookup(index);
    return t ? Result<HostParameterId>{Status::Ok, hostParameter(t->parameter)}
             : Result<HostParameterId>{Status::InvalidIndex, {}};
}
// Six coarse/fine pairs alias a storage field: never choose one host ID silently.
constexpr Result<HostParameterId> uniqueHostForDestination(DestinationId id) noexcept {
    HostParameterId found;
    for (const auto& t : kOriginalTargets) {
        if (t.destination != id) continue;
        if (found.valid()) return {Status::AmbiguousAlias, {}};
        found = hostParameter(t.parameter);
    }
    for (const auto& t : kInternalOnlyParameters)
        if (t.destination == id) found = hostParameter(t.parameter);
    return found.valid() ? Result<HostParameterId>{Status::Ok, found}
                         : Result<HostParameterId>{Status::NotInTargetTable, {}};
}

constexpr Status validateSource(SourceIdentity s) noexcept {
    return valid(s.owner) && (s.kind == SourceKind::Velocity || s.kind == SourceKind::Lfo)
        ? Status::Ok : Status::InvalidSource;
}
constexpr Status validateMenuSelection(SourceIdentity source, Voice targetVoice,
                                       OriginalTargetIndex index) noexcept {
    if (validateSource(source) != Status::Ok) return Status::InvalidSource;
    const auto* t = lookup(index);
    if (!t) return Status::InvalidIndex;
    if (!valid(targetVoice)) return Status::InvalidVoice;
    if (source.kind == SourceKind::Velocity && targetVoice != source.owner)
        return Status::VoiceMismatch;
    if (t->availability == TargetAvailability::Off) return Status::Ok;
    if (t->availability != TargetAvailability::VoiceMenu) return Status::UnavailableInMenu;
    return t->voice == targetVoice ? Status::Ok : Status::VoiceMismatch;
}
struct ExplicitSelection {
    SourceIdentity source;
    OriginalTargetIndex target;
    DestinationId destination;
    Voice resolvedTargetVoice; // Invalid for off/global; never a source selector.
    OriginalVoiceSelector originalMenuVoice; // Kept independently for original kits.
    bool menuConsistent;
};
// Preset restore only checks the table-wide bound in the original C. It can
// configure cross-voice velocity/global targets that the normal menu excludes.
// This resolver reports the actual row identity without blessing those routes.
constexpr Result<ExplicitSelection> resolveOriginalPresetSelection(
    SourceIdentity source, OriginalVoiceSelector menuVoice,
    OriginalTargetIndex index) noexcept {
    if (validateSource(source) != Status::Ok) return {Status::InvalidSource, {}};
    const auto* t = lookup(index);
    if (!t) return {Status::InvalidIndex, {}};
    if (!menuVoice.valid()) return {Status::InvalidVoice, {}};
    const auto hint = source.kind == SourceKind::Velocity ? source.owner : destinationVoice(menuVoice);
    const auto consistent = validateMenuSelection(source, hint, index) == Status::Ok;
    return {Status::Ok, {source, index, t->destination, t->voice, menuVoice, consistent}};
}
// ORIGINAL QUIRK: preset_sendDrumsetParameters normalizes only VOICE_LFO to1..6;
// it does not adjust TARGET_LFO to the selected voice. Keep that mismatch visible.
constexpr OriginalVoiceSelector normalizeOriginalKitVoice(std::uint64_t raw) noexcept {
    return OriginalVoiceSelector::fromRaw(raw >= 1 && raw <= 6 ? raw : 1);
}

// The logical-slot bitmap is an original AVR menu coordinate, NOT a new host
// encoding. It is used only when an explicit original menu operation is asked.
constexpr Result<LogicalTargetSlot> logicalSlotForTarget(OriginalTargetIndex index) noexcept {
    const auto* t = lookup(index);
    if (!t) return {Status::InvalidIndex, {}};
    if (t->scope != RoutingScope::Voice) return {Status::UnavailableInMenu, {}};
    const auto v = static_cast<unsigned>(t->voice);
    unsigned ordinal = index.value() - kVoiceTargetRanges[v].first.value();
    for (unsigned i = 0; i < kLogicalSlotCount; ++i) {
        if ((kOriginalGapMap[i] & (1u << (5u - v))) == 0) continue;
        if (ordinal == 0) return {Status::Ok, LogicalTargetSlot::fromRaw(i)};
        --ordinal;
    }
    return {Status::UnavailableInMenu, {}};
}
constexpr Result<OriginalTargetIndex> targetForLogicalSlot(Voice voice, LogicalTargetSlot slot) noexcept {
    if (!valid(voice)) return {Status::InvalidVoice, {}};
    if (!slot.valid()) return {Status::InvalidIndex, {}};
    const auto v = static_cast<unsigned>(voice);
    const auto mask = 1u << (5u - v);
    if ((kOriginalGapMap[slot.value()] & mask) == 0)
        return {Status::Ok, OriginalTargetIndex::fromRaw(0)}; // Original gap -> off.
    unsigned ordinal = 0;
    for (unsigned i = 0; i < slot.value(); ++i) if ((kOriginalGapMap[i] & mask) != 0) ++ordinal;
    return {Status::Ok, OriginalTargetIndex::fromRaw(kVoiceTargetRanges[v].first.value() + ordinal)};
}
constexpr Result<OriginalTargetIndex> retargetOriginalMenu(OriginalTargetIndex oldTarget,
                                                         Voice newVoice) noexcept {
    if (!valid(newVoice)) return {Status::InvalidVoice, {}};
    const auto* old = lookup(oldTarget);
    if (!old) return {Status::InvalidIndex, {}};
    if (old->availability == TargetAvailability::Off)
        return {Status::Ok, OriginalTargetIndex::fromRaw(0)};
    const auto slot = logicalSlotForTarget(oldTarget);
    return slot ? targetForLogicalSlot(newVoice, slot.value)
                : Result<OriginalTargetIndex>{slot.status, {}};
}

// Byte-packing feasibility ONLY. This does not assign a routing meaning to v1.
constexpr Result<SevenBitValue> originalIndexAsSevenBits(OriginalTargetIndex index) noexcept {
    if (!index.valid()) return {Status::InvalidIndex, {}};
    if (index.value() > 127) return {Status::NoSevenBitRepresentation, {}};
    return {Status::Ok, SevenBitValue::fromRaw(index.value())};
}
enum class SelectorKind : std::uint8_t { VelocityTarget, LfoDestinationVoice, LfoTarget, NotSelector };
struct SelectorParameter { SelectorKind kind; Voice sourceOwner; };
constexpr SelectorParameter selectorParameter(HostParameterId id) noexcept {
    if (!id.valid()) return {SelectorKind::NotSelector, Voice::Invalid};
    const auto n = id.value();
    if (n >= 149 && n <= 154) return {SelectorKind::VelocityTarget, voiceFromZeroBased(n - 149)};
    if (n >= 161 && n <= 166) return {SelectorKind::LfoDestinationVoice, voiceFromZeroBased(n - 161)};
    if (n >= 167 && n <= 172) return {SelectorKind::LfoTarget, voiceFromZeroBased(n - 167)};
    return {SelectorKind::NotSelector, Voice::Invalid};
}
struct LegacySelectorValue { HostParameterId parameter; SevenBitValue byte; };
constexpr Result<LegacySelectorValue> preserveLegacySelector(HostParameterId id, SevenBitValue byte) noexcept {
    if (selectorParameter(id).kind == SelectorKind::NotSelector)
        return {Status::UnsupportedSelectorParameter, {}};
    if (!byte.valid()) return {Status::InvalidSevenBitValue, {}};
    return {Status::Ok, {id, byte}};
}
// ALL v1 selector bytes were inert in the certified runtime. Even an in-range
// original index is not proof of its encoding/provenance. No guessed migration.
constexpr Result<ExplicitSelection> decodeLegacyForRouting(LegacySelectorValue value) noexcept {
    const auto preserved = preserveLegacySelector(value.parameter, value.byte);
    return {preserved ? Status::LegacyEncodingUnspecified : preserved.status, {}};
}

enum class MidiSpace : std::uint8_t { None, CC, NRPN };
enum class Ingress : std::uint8_t { Direct, ProtocolOnly, ConditionalDataEntry, NrpnSelectorOnly, None };
struct MidiAddress { MidiSpace space; std::uint16_t number; Ingress ingress; };
constexpr MidiAddress certifiedPluginAddress(HostParameterId id) noexcept {
    if (!id.valid() || id.value() == 0) return {MidiSpace::None, 0, Ingress::None};
    const auto n = id.value();
    if (n >= 128) return {MidiSpace::NRPN, static_cast<std::uint16_t>(n - 127u), Ingress::Direct};
    return {MidiSpace::CC, n, n == 98 || n == 99 ? Ingress::NrpnSelectorOnly :
        n == 6 ? Ingress::ConditionalDataEntry : Ingress::Direct};
}
constexpr MidiAddress originalFirmwareAddress(FirmwareParameterId id) noexcept {
    if (!id.valid() || id.value() == 0 || id.value() == 127)
        return {MidiSpace::None, 0, Ingress::None}; // CC128 is not a MIDI byte.
    const auto n = id.value();
    if (n >= 128) return {MidiSpace::NRPN, static_cast<std::uint16_t>(n - 128u), Ingress::Direct};
    return {MidiSpace::CC, static_cast<std::uint16_t>(n + 1u),
        n == 5 || n == 97 || n == 98 ? Ingress::ProtocolOnly : Ingress::Direct};
}
constexpr OriginalMidiDisposition selectorMidiDisposition(HostParameterId id) noexcept {
    const auto kind = selectorParameter(id).kind;
    return kind == SelectorKind::LfoDestinationVoice ? OriginalMidiDisposition::Stm32NoOpAvrStorageOnly :
        kind == SelectorKind::VelocityTarget || kind == SelectorKind::LfoTarget ?
            OriginalMidiDisposition::Stm32NoOpAvrRelayUnverified : OriginalMidiDisposition::None;
}
// Internal UART destination message: upper holds SOURCE owner and target PAR bit7,
// NOT the destination voice or the AVR menu index. No MIDI assignment is changed.
struct InternalDestinationMessage { SevenBitValue upper; SevenBitValue lower; };
struct InternalDestination { SourceIdentity source; FirmwareParameterId parameter; };
constexpr Result<InternalDestinationMessage> encodeOriginalInternalDestination(
    SourceIdentity source, OriginalTargetIndex index) noexcept {
    if (validateSource(source) != Status::Ok) return {Status::InvalidSource, {}};
    const auto* t = lookup(index);
    if (!t) return {Status::InvalidIndex, {}};
    const auto p = t->parameter.value();
    return {Status::Ok, {SevenBitValue::fromRaw((static_cast<unsigned>(source.owner) << 1u) | (p >> 7u)),
                         SevenBitValue::fromRaw(p & 127u)}};
}
constexpr Result<InternalDestinationMessage> encodeOriginalInternalParameter(
    SourceIdentity source, FirmwareParameterId parameter) noexcept {
    if (validateSource(source) != Status::Ok) return {Status::InvalidSource, {}};
    if (!lookupFirmwareParameter(parameter)) return {Status::NotInTargetTable, {}};
    const auto p = parameter.value();
    return {Status::Ok, {SevenBitValue::fromRaw((static_cast<unsigned>(source.owner) << 1u) | (p >> 7u)),
                         SevenBitValue::fromRaw(p & 127u)}};
}
constexpr Result<InternalDestination> decodeOriginalInternalDestination(
    SourceKind kind, InternalDestinationMessage message) noexcept {
    if (!message.upper.valid() || !message.lower.valid()) return {Status::InvalidInternalMessage, {}};
    const auto owner = voiceFromZeroBased(message.upper.value() >> 1u);
    const auto p = FirmwareParameterId::fromRaw(((message.upper.value() & 1u) << 7u) | message.lower.value());
    const SourceIdentity source{kind, owner};
    if (validateSource(source) != Status::Ok || !p.valid()) return {Status::InvalidInternalMessage, {}};
    if (!lookupFirmwareParameter(p)) return {Status::NotInTargetTable, {}};
    return {Status::Ok, {source, p}};
}

struct OperationSupport {
    bool originalMultiplicative;
    bool additive; // Never supported by the original ModulationNode contract.
    bool narrowsAfterEachNode;
    bool requiresTriggerOrConfiguration;
};
constexpr OperationSupport operations(OriginalTargetIndex index) noexcept {
    const auto* t = lookup(index);
    if (!t) return {false, false, false, false};
    const auto type = t->valueType;
    return {type == ParameterValueType::UInt8 || type == ParameterValueType::UInt32 ||
                type == ParameterValueType::Float || type == ParameterValueType::FrequencyMultiplier,
            false, type == ParameterValueType::UInt8 || type == ParameterValueType::UInt32,
            has(t->consumption, Consumption::Trigger) || has(t->consumption, Consumption::MidiDispatch) ||
                has(t->consumption, Consumption::SetterOrTempoRecalculation) ||
                has(t->consumption, Consumption::MutableRoute)};
}

constexpr bool validateMetadata() noexcept {
    unsigned fieldCount = 0, distinct = 0;
    std::array<bool, kSoundSlotCount> seenParameters {};
    std::array<bool, kSoundSlotCount> seenDestinations {};
    for (std::size_t i = 0; i < kOriginalTargets.size(); ++i) {
        const auto& t = kOriginalTargets[i];
        if (!t.index.valid() || t.index.value() != i || !t.parameter.valid() ||
            t.symbol == nullptr || t.labelSymbol == nullptr || t.storageField == nullptr ||
            t.nominalNativeRange == nullptr || t.tableSource.file == nullptr || t.tableSource.line == 0)
            return false;
        if (t.scope == RoutingScope::Voice && !valid(t.voice)) return false;
        if (seenParameters[t.parameter.value()]) return false;
        seenParameters[t.parameter.value()] = true;
        if (i == 0) {
            if (t.valueType != ParameterValueType::None || t.availability != TargetAvailability::Off)
                return false;
        } else {
            if (t.valueType == ParameterValueType::None || t.parameterSource.line == 0) return false;
            ++fieldCount;
            const auto destination = static_cast<unsigned>(t.destination);
            if (destination >= kSoundSlotCount) return false;
            if (!seenDestinations[destination]) ++distinct;
            seenDestinations[destination] = true;
        }
    }
    for (unsigned v = 0; v < kVoiceCount; ++v) {
        const auto& r = kVoiceTargetRanges[v];
        unsigned n = 0;
        for (auto bits : kOriginalGapMap) if ((bits & (1u << (5u - v))) != 0) ++n;
        if (n != r.last.value() - r.first.value() + 1u) return false;
    }
    for (const auto& t : kInternalOnlyParameters) {
        if (t.index.valid() || t.availability != TargetAvailability::InternalMessageOnly ||
            !t.parameter.valid() || t.valueType == ParameterValueType::None ||
            t.parameterSource.line == 0) return false;
        if (seenParameters[t.parameter.value()]) return false;
        seenParameters[t.parameter.value()] = true;
    }
    return fieldCount == 204 && distinct == kDistinctStorageCount;
}
static_assert(kOriginalTargets.size() == kOriginalTargetCount, "Original target table size");
static_assert(kOriginalGapMap.size() == kLogicalSlotCount, "Original gap map size");
static_assert(kInternalOnlyParameters.size() == kInternalOnlyParameterCount, "Non-menu STM32 pointer targets");
static_assert(validateMetadata(), "Original modulation metadata invariant");
static_assert(!std::is_convertible_v<HostParameterId, OriginalTargetIndex>, "Separate host and table index types");
static_assert(!std::is_convertible_v<OriginalTargetIndex, SevenBitValue>, "No implicit seven-bit truncation");
static_assert(!std::is_convertible_v<unsigned, OriginalTargetIndex>, "Validated explicit construction only");
static_assert(std::is_trivially_copyable_v<TargetMetadata>, "Fixed immutable metadata");

} // namespace lxr::modulation_contract
