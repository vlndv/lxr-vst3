#include "../interfaces/ModulationContract.h"
#include "../dsp/MidiParser.h"
#include "../vendor/iPlug2/Examples/LXR/LXRParamContract.h"
#include "../vendor/iPlug2/Examples/LXR/LXRState.h"
#include "reference/modulation/original_contract_fixture.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>

namespace mc = lxr::modulation_contract;
static unsigned passed = 0, failed = 0;
static std::size_t allocations = 0;
void* operator new(std::size_t n) {
    ++allocations;
    if (void* p = std::malloc(n)) return p;
    std::abort();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

static void check(const char* label, bool ok, unsigned context = 0) {
    if (ok) ++passed;
    else { ++failed; std::printf("FAIL %s context=%u\n", label, context); }
}
static bool menuExpected(unsigned source, bool velocity, unsigned voice, unsigned index) {
    if (velocity && source != voice) return false;
    if (index == 0) return true;
    return index >= lxr_ref_range_start(voice) && index <= lxr_ref_range_end(voice);
}
static bool sameMessage(const mc::InternalDestinationMessage& a, const mc::InternalDestinationMessage& b) {
    return a.upper == b.upper && a.lower == b.lower;
}

static_assert(mc::kOriginalTargetCount == 205 && mc::kSoundSlotCount == 228);
static_assert(mc::OriginalTargetIndex::fromRaw(204).valid());
static_assert(!mc::OriginalTargetIndex::fromRaw(205).valid());
static_assert(!mc::OriginalTargetIndex::fromRaw(65536).valid());
static_assert(!mc::SevenBitValue::fromRaw(128).valid());
static_assert(sizeof(mc::OriginalTargetIndex) >= 2);
static_assert(!std::is_convertible_v<mc::HostParameterId, mc::FirmwareParameterId>);
static_assert(!std::is_convertible_v<mc::OriginalTargetIndex, unsigned>);
static_assert(!std::is_convertible_v<mc::SevenBitValue, mc::OriginalVoiceSelector>);
static_assert(!std::is_convertible_v<mc::Voice, mc::Track>);
static_assert(!std::is_convertible_v<mc::DestinationId, mc::HostParameterId>);
static_assert(!std::is_constructible_v<mc::OriginalTargetIndex, std::uint8_t>);
static_assert(std::is_trivially_copyable_v<mc::ExplicitSelection>);
static_assert(mc::validateMetadata());

int main() {
    lxr_ref_initialize_stm32_table();
    check("compiled original table count", lxr_ref_target_count() == mc::kOriginalTargetCount);
    check("compiled original gap count", lxr_ref_gap_count() == mc::kLogicalSlotCount);
    check("original destination uint16 storage despite TYPE_UINT8", lxr_ref_destination_storage_bytes() == 2);
    for (unsigned v = 0; v < 6; ++v) {
        check("original voice range start", mc::kVoiceTargetRanges[v].first.value() == lxr_ref_range_start(v), v);
        check("original voice range end", mc::kVoiceTargetRanges[v].last.value() == lxr_ref_range_end(v), v);
        check("one-based selector conversion", mc::destinationVoice(mc::originalVoiceSelector(mc::voiceFromZeroBased(v))) == mc::voiceFromZeroBased(v), v);
    }
    unsigned typed = 0, menu = 0, review = 0;
    for (unsigned i = 0; i < lxr_ref_target_count(); ++i) {
        const auto index = mc::OriginalTargetIndex::fromRaw(i);
        const auto* t = mc::lookup(index);
        check("row index order", t && t->index.value() == i, i);
        if (!t) continue;
        const unsigned p = lxr_ref_parameter(i);
        check("parameter from unchanged original AVR C", t->parameter.value() == p, i);
        check("label from unchanged original AVR C", t->originalLabelId == lxr_ref_label(i), i);
        check("value type from unchanged original STM32 C", static_cast<unsigned>(t->valueType) == lxr_ref_parameter_type(p), i);
        const auto originalVoice = lxr_ref_voice(i);
        check("voice from original range resolver", originalVoice == 0 ? !mc::valid(t->voice) : static_cast<unsigned>(t->voice) + 1u == originalVoice, i);
        check("symbols retained", t->symbol && std::strncmp(t->symbol, "PAR_", 4) == 0 && t->labelSymbol && std::strncmp(t->labelSymbol, "TEXT_", 5) == 0, i);
        check("source locations retained", t->tableSource.line != 0 && t->tableSource.file && (i == 0 || t->parameterSource.line != 0), i);
        const auto host = mc::hostForTarget(index);
        const auto back = mc::targetForHost(mc::HostParameterId::fromRaw(p));
        check("host and firmware identity mapping", host && host.value.value() == p && back && back.value == index, i);
        if (p != 0) check("canonical symbol matches unchanged host descriptor", std::strcmp(t->symbol + 4, lxr::vst::kParamDescriptors[p].name) == 0, p);
        const auto packing = mc::originalIndexAsSevenBits(index);
        check("no silent target index truncation", i <= 127 ? packing && packing.value.value() == i : packing.status == mc::Status::NoSevenBitRepresentation, i);
        const auto logical = mc::logicalSlotForTarget(index);
        if (i >= 2) check("logical slot from compiled original C", logical && logical.value.value() == lxr_ref_logical_slot(i), i);
        else check("off/global not logical voice slot", logical.status == mc::Status::UnavailableInMenu, i);
        if (i) ++typed;
        if (t->availability == mc::TargetAvailability::VoiceMenu) ++menu;
        if (t->runtimePolicy == mc::RuntimePolicy::RequiresDestinationMutationReview) ++review;
        const auto ops = mc::operations(index);
        check("original operation multiplicative only", ops.originalMultiplicative == (i != 0) && !ops.additive, i);
        check("original integer narrowing classification", ops.narrowsAfterEachNode == (t->valueType == mc::ParameterValueType::UInt8 || t->valueType == mc::ParameterValueType::UInt32), i);
        for (unsigned j = 0; j < lxr_ref_target_count(); ++j) {
            const auto* u = mc::lookup(mc::OriginalTargetIndex::fromRaw(j));
            const bool expected = lxr_ref_same_storage(p, lxr_ref_parameter(j)) != 0;
            check("storage alias relation from independent C pointers", (i != 0 && j != 0 && t->destination == u->destination) == expected, i * 205 + j);
        }
    }
    check("raw/selectable/review counts", typed == 204 && menu == 203 && review == 6);
    for (unsigned g = 0; g < lxr_ref_gap_count(); ++g) {
        check("original gap bitmap", mc::kOriginalGapMap[g] == lxr_ref_gap(g), g);
        for (unsigned v = 0; v < 6; ++v) {
            const auto result = mc::targetForLogicalSlot(mc::voiceFromZeroBased(v), mc::LogicalTargetSlot::fromRaw(g));
            check("original logical slot to target", result && result.value.value() == lxr_ref_remap(v, g), g * 6 + v);
        }
    }
    for (unsigned i = 0; i < 205; ++i) for (unsigned v = 0; v < 6; ++v) {
        const auto index = mc::OriginalTargetIndex::fromRaw(i);
        const auto remapped = mc::retargetOriginalMenu(index, mc::voiceFromZeroBased(v));
        if (i == 1) check("automation-only cannot menu retarget", remapped.status == mc::Status::UnavailableInMenu);
        else check("voice retarget preserves original logical type or off", remapped && remapped.value.value() == (i == 0 ? 0 : lxr_ref_remap(v, lxr_ref_logical_slot(i))), i * 6 + v);
        for (unsigned source = 0; source < 6; ++source) for (bool velocity : {false, true}) {
            const mc::SourceIdentity s{velocity ? mc::SourceKind::Velocity : mc::SourceKind::Lfo, mc::voiceFromZeroBased(source)};
            const auto status = mc::validateMenuSelection(s, mc::voiceFromZeroBased(v), index);
            check("per-voice menu validation", (status == mc::Status::Ok) == menuExpected(source, velocity, v, i), i);
        }
    }
    for (unsigned p = 0; p < 228; ++p) {
        const auto host = mc::HostParameterId::fromRaw(p);
        check("all host IDs stable", mc::hostParameter(mc::firmwareParameter(host)) == host, p);
        bool inMenu = false;
        for (unsigned i = 0; i < lxr_ref_target_count(); ++i) if (lxr_ref_parameter(i) == p) inMenu = true;
        check("non-target host IDs are rejected explicitly", static_cast<bool>(mc::targetForHost(host)) == inMenu, p);
        const auto* storage = mc::lookupFirmwareParameter(mc::firmwareParameter(host));
        check("complete STM32 pointer universe", (storage != nullptr) == (p == 0 || lxr_ref_parameter_type(p) != 0xff), p);
        const auto plugin = mc::certifiedPluginAddress(host);
        const auto firmware = mc::originalFirmwareAddress(mc::firmwareParameter(host));
        if (p == 0) check("reserved has no MIDI address", plugin.space == mc::MidiSpace::None && firmware.space == mc::MidiSpace::None);
        else if (p < 128) {
            check("existing certified CC helper unchanged", plugin.space == mc::MidiSpace::CC && plugin.number == p && lxr::parFromCC(static_cast<std::uint8_t>(p)) == p, p);
            check("original CC offset explicit", p == 127 ? firmware.space == mc::MidiSpace::None : firmware.space == mc::MidiSpace::CC && firmware.number == p + 1, p);
        } else {
            check("existing certified NRPN helper unchanged", plugin.space == mc::MidiSpace::NRPN && plugin.number == p - 127 && lxr::parFromNRPN(plugin.number) == p, p);
            check("original NRPN offset explicit", firmware.space == mc::MidiSpace::NRPN && firmware.number == p - 128, p);
        }
    }
    unsigned legacyCases = 0;
    for (unsigned p = 0; p < 228; ++p) {
        const auto host = mc::HostParameterId::fromRaw(p);
        const bool selector = (p >= 149 && p <= 154) || (p >= 161 && p <= 172);
        check("exact selector parameter set", (mc::selectorParameter(host).kind != mc::SelectorKind::NotSelector) == selector, p);
        for (unsigned b = 0; b < 128; ++b) {
            const auto saved = mc::preserveLegacySelector(host, mc::SevenBitValue::fromRaw(b));
            if (selector) {
                ++legacyCases;
                check("all legacy bytes preserved verbatim", saved && saved.value.byte.value() == b && saved.value.parameter == host, p);
                check("opaque legacy does not become a route", saved && mc::decodeLegacyForRouting(saved.value).status == mc::Status::LegacyEncodingUnspecified, p);
            } else check("non-selector cannot enter selector conversion", saved.status == mc::Status::UnsupportedSelectorParameter, p);
        }
    }
    check("2304 legacy selector cases covered", legacyCases == 18 * 128);
    for (unsigned source = 0; source < 6; ++source) for (bool velocity : {false, true}) for (unsigned i = 0; i < 205; ++i) {
        const mc::SourceIdentity s{velocity ? mc::SourceKind::Velocity : mc::SourceKind::Lfo, mc::voiceFromZeroBased(source)};
        const auto index = mc::OriginalTargetIndex::fromRaw(i);
        const auto original = mc::resolveOriginalPresetSelection(s, mc::originalVoiceSelector(s.owner), index);
        check("preset full index retained independently of menu hint", original && original.value.target == index && original.value.destination == mc::lookup(index)->destination, i);
        const auto wire = mc::encodeOriginalInternalDestination(s, index);
        const auto param = lxr_ref_parameter(i);
        check("UART message contains source and PAR, not target index", wire && wire.value.upper.value() == ((source << 1) | (param >> 7)) && wire.value.lower.value() == (param & 127), i);
        const auto decoded = wire ? mc::decodeOriginalInternalDestination(s.kind, wire.value) : mc::Result<mc::InternalDestination>{mc::Status::InvalidInternalMessage, {}};
        check("UART typed roundtrip", decoded && decoded.value.parameter.value() == param && decoded.value.source.owner == s.owner && decoded.value.source.kind == s.kind, i);
    }
    for (const auto& t : mc::kInternalOnlyParameters) {
        check("internal-only pointer independent type", static_cast<unsigned>(t.valueType) == lxr_ref_parameter_type(t.parameter.value()), t.parameter.value());
        check("internal-only not given an invented menu index", !t.index.valid() && !mc::targetForHost(mc::hostParameter(t.parameter)), t.parameter.value());
        const auto encoded = mc::encodeOriginalInternalParameter({mc::SourceKind::Lfo, mc::Voice::HiHat}, t.parameter);
        const auto decoded = encoded ? mc::decodeOriginalInternalDestination(mc::SourceKind::Lfo, encoded.value) : mc::Result<mc::InternalDestination>{mc::Status::InvalidInternalMessage, {}};
        check("internal-only original UART path preserved", decoded && decoded.value.parameter == t.parameter, t.parameter.value());
    }
    check("coarse/fine reverse alias ambiguous", mc::uniqueHostForDestination(mc::DestinationId::COARSE1).status == mc::Status::AmbiguousAlias);
    check("unique physical destination reverse host", mc::uniqueHostForDestination(mc::DestinationId::VOL1).value.value() == 88);
    check("reserved slot invalid table access", mc::lookup(mc::OriginalTargetIndex::fromRaw(205)) == nullptr);
    check("index127 source identity", mc::lookup(mc::OriginalTargetIndex::fromRaw(127))->parameter.value() == lxr_ref_parameter(127));
    check("index128 source identity", mc::lookup(mc::OriginalTargetIndex::fromRaw(128))->parameter.value() == lxr_ref_parameter(128));
    check("highest original index", mc::lookup(mc::OriginalTargetIndex::fromRaw(204))->parameter.value() == 226);
    check("voice6 is not a synthesis voice", !mc::valid(mc::voiceFromZeroBased(6)));
    check("both hats share source voice", mc::voiceForTrack(mc::Track::ClosedHat) == mc::Voice::HiHat && mc::voiceForTrack(mc::Track::OpenHat) == mc::Voice::HiHat);
    check("invalid logical slot", mc::targetForLogicalSlot(mc::Voice::Drum1, mc::LogicalTargetSlot::fromRaw(43)).status == mc::Status::InvalidIndex);
    check("original kit voice normalization", mc::normalizeOriginalKitVoice(0).value() == 1 && mc::normalizeOriginalKitVoice(7).value() == 1 && mc::normalizeOriginalKitVoice(6).value() == 6);
    check("bad seven-bit legacy byte rejected", mc::preserveLegacySelector(mc::HostParameterId::fromRaw(149), mc::SevenBitValue::fromRaw(128)).status == mc::Status::InvalidSevenBitValue);
    check("invalid source rejected", mc::validateMenuSelection({mc::SourceKind::Lfo, mc::Voice::Invalid}, mc::Voice::Drum1, mc::OriginalTargetIndex::fromRaw(0)) == mc::Status::InvalidSource);
    check("invalid UART owner rejected", mc::decodeOriginalInternalDestination(mc::SourceKind::Lfo, {mc::SevenBitValue::fromRaw(127), mc::SevenBitValue::fromRaw(0)}).status == mc::Status::InvalidInternalMessage);
    check("null-pointer UART PAR rejected", mc::decodeOriginalInternalDestination(mc::SourceKind::Lfo, {mc::SevenBitValue::fromRaw(0), mc::SevenBitValue::fromRaw(5)}).status == mc::Status::NotInTargetTable);
    check("selector relay remains UNVERIFIED", mc::selectorMidiDisposition(mc::HostParameterId::fromRaw(149)) == mc::OriginalMidiDisposition::Stm32NoOpAvrRelayUnverified);
    check("VOICE_LFO STM32 no-op versus AVR storage", mc::selectorMidiDisposition(mc::HostParameterId::fromRaw(161)) == mc::OriginalMidiDisposition::Stm32NoOpAvrStorageOnly);
    check("CC98/99 interception explicit", mc::certifiedPluginAddress(mc::HostParameterId::fromRaw(98)).ingress == mc::Ingress::NrpnSelectorOnly && mc::certifiedPluginAddress(mc::HostParameterId::fromRaw(99)).ingress == mc::Ingress::NrpnSelectorOnly);
    check("CC6 conditional data entry explicit", mc::certifiedPluginAddress(mc::HostParameterId::fromRaw(6)).ingress == mc::Ingress::ConditionalDataEntry);

    // Independent golden LXRS v1 header, computed with Python struct/zlib from
    // the pre-edit contract. Payload pattern includes all slots, selector bytes
    // and reserved0. No new route state is passed into the existing serializer.
    std::array<std::uint8_t, 228> patch {};
    for (unsigned i = 0; i < patch.size(); ++i) patch[i] = static_cast<std::uint8_t>((i * 37 + 11) & 127);
    std::array<std::uint8_t, lxr::state::kEncodedSize> encoded {};
    const std::uint8_t goldenHeader[] = { 76,88,82,83,1,0,24,0,228,0,0,0,228,0,0,0,0,0,0,0,207,54,128,108 };
    const bool encodedOk = lxr::state::Encode(patch, encoded);
    check("unchanged LXRS v1 size/header/CRC golden", encodedOk && encoded.size() == 252 && std::memcmp(encoded.data(), goldenHeader, 24) == 0);
    check("unchanged LXRS v1 payload", encodedOk && std::memcmp(encoded.data() + 24, patch.data(), patch.size()) == 0);
    std::array<std::uint8_t, 228> restored {};
    check("unchanged LXRS v1 decode", static_cast<bool>(lxr::state::Decode(encoded.data(), encoded.size(), restored)) && restored == patch);

    const auto before = allocations;
    for (unsigned repeat = 0; repeat < 100; ++repeat) for (unsigned i = 0; i < 205; ++i) {
        const auto index = mc::OriginalTargetIndex::fromRaw(i);
        const mc::SourceIdentity s{mc::SourceKind::Lfo, mc::Voice::Drum1};
        check("deterministic repeated immutable lookup", mc::lookup(index) == &mc::kOriginalTargets[i] && sameMessage(mc::encodeOriginalInternalDestination(s,index).value, mc::encodeOriginalInternalDestination(s,index).value), i);
    }
    check("metadata lookup/conversion C++ allocations zero", allocations == before);
    std::printf("SUMMARY pass=%u fail=%u originalTargets=205 typedFields=204 distinctMenuStorage=198 internalOnly=7 legacyCases=%u lookupAllocations=%zu\n", passed, failed, legacyCases, allocations - before);
    return failed ? 1 : 0;
}
