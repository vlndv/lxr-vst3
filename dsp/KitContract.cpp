#include "KitContract.h"

namespace lxr::kit {
namespace {
using namespace modulation_contract;
Result CheckField(std::uint16_t id, std::uint16_t value) noexcept {
    if (id >= kFields.size()) return {Status::Invalid, Issue::FieldId, id};
    const auto& field = kFields[id];
    if (value > field.maximum) return {Status::Invalid,
        field.kind == FieldKind::Selector ? Issue::Selector :
        field.kind == FieldKind::VoiceHint ? Issue::VoiceHint : Issue::Range, id};
    return {};
}
Result SelectorFor(const CanonicalKit& kit, unsigned source, unsigned owner,
                   OriginalSelector& output) noexcept {
    std::uint16_t index = 0, hint = 0;
    const auto id = static_cast<std::uint16_t>((source == 0 ? 149 : 167) + owner);
    const auto read = ReadField(kit, id, index);
    if (!read) return read;
    if (source == 1) {
        const auto h = ReadField(kit, static_cast<std::uint16_t>(161 + owner), hint);
        if (!h) return h;
    }
    auto result = ResolveOriginalSelector({source == 0 ? SourceKind::Velocity : SourceKind::Lfo,
        voiceFromZeroBased(owner)}, index, hint, output);
    if (!result) result.field = id;
    return result;
}
} // namespace

RawKit MakeFirmwareInitializationKit() noexcept {
    // menu_init zeroes every sound slot. preset_currentName is a BSS name[8].
    // No factory kit is present; MCU initialization is not a stored kit.
    return {};
}
CanonicalKit MakeCanonicalInitializationKit() noexcept { return {}; }

Result ReadField(const CanonicalKit& kit, std::uint16_t id, std::uint16_t& value) noexcept {
    switch (id) {
#define K_BYTE(id, symbol, path, owner, kind, maximum, native) case id: value = kit.path; return {};
#define K_SELECTOR(id, symbol, path, owner, kind, maximum, native) case id: value = kit.path.value(); return {};
#include "KitFields.inc"
#undef K_SELECTOR
#undef K_BYTE
    default: return {Status::Invalid, Issue::FieldId, id};
    }
}
Result ValidateRawKit(const RawKit& kit) noexcept {
    for (std::uint16_t i = 0; i < kFieldCount; ++i) {
        const auto result = CheckField(i, kit.values[i]);
        if (!result) return result;
    }
    return {};
}
Result ValidateCanonicalKit(const CanonicalKit& kit) noexcept {
    for (std::uint16_t i = 0; i < kFieldCount; ++i) {
        std::uint16_t value = 0;
        const auto read = ReadField(kit, i, value);
        if (!read) return read;
        const auto result = CheckField(i, value);
        if (!result) return result;
    }
    return {};
}
Result DecodeRawKit(const RawKit& raw, CanonicalKit& output) noexcept {
    const auto result = ValidateRawKit(raw);
    if (!result) return result;
    CanonicalKit candidate {};
    candidate.name = raw.name;
#define K_BYTE(id, symbol, path, owner, kind, maximum, native) candidate.path = raw.values[id];
#define K_SELECTOR(id, symbol, path, owner, kind, maximum, native) candidate.path = StoredSelector::fromRaw(raw.values[id]);
#include "KitFields.inc"
#undef K_SELECTOR
#undef K_BYTE
    output = candidate;
    return {};
}
Result EncodeRawKit(const CanonicalKit& kit, RawKit& output) noexcept {
    const auto result = ValidateCanonicalKit(kit);
    if (!result) return result;
    RawKit candidate {};
    candidate.name = kit.name;
    for (std::uint16_t i = 0; i < kFieldCount; ++i) {
        std::uint16_t value = 0;
        const auto read = ReadField(kit, i, value);
        if (!read) return read;
        candidate.values[i] = static_cast<std::uint8_t>(value); // Proven <=255, selectors<=204.
    }
    output = candidate;
    return {};
}

Result ResolveOriginalSelector(modulation_contract::SourceIdentity source, std::uint64_t raw,
                               std::uint64_t voiceHint, OriginalSelector& output) noexcept {
    using namespace modulation_contract;
    if (validateSource(source) != modulation_contract::Status::Ok)
        return {Status::Invalid, Issue::Source};
    const auto index = OriginalTargetIndex::fromRaw(raw);
    if (!index.valid()) return {Status::Invalid, Issue::Selector};
    if (voiceHint > 6 || (source.kind == SourceKind::Velocity && voiceHint != 0))
        return {Status::Invalid, Issue::VoiceHint};
    output = {source, index, static_cast<std::uint8_t>(voiceHint)};
    return {};
}
Result ValidateOriginalSelector(const OriginalSelector& selection) noexcept {
    OriginalSelector checked {};
    return ResolveOriginalSelector(selection.source, selection.index.value(), selection.voiceHint, checked);
}
Result ToTypedRoute(const OriginalSelector& selection, PersistentRouteRecord& output) noexcept {
    using namespace modulation_contract;
    const auto result = ValidateOriginalSelector(selection);
    if (!result) return result;
    const auto source = static_cast<std::uint8_t>(selection.source.kind == SourceKind::Lfo);
    const auto owner = static_cast<std::uint8_t>(selection.source.owner);
    auto candidate = MakeDisabledRoute(source, owner);
    if (selection.index.value() != 0) {
        const auto& target = *lookup(selection.index);
        if (target.scope != RoutingScope::Voice) return {Status::Unsupported, Issue::GlobalRoute};
        if (target.voice != selection.source.owner) return {Status::Unsupported, Issue::CrossVoice};
        candidate.flags = 1;
        candidate.identityNamespace = 1;
        candidate.originalIndex = selection.index.value();
        candidate.firmwareParameter = target.parameter.value();
        candidate.valueType = static_cast<std::uint8_t>(target.valueType);
        candidate.scope = static_cast<std::uint8_t>(target.scope);
        candidate.physicalKey = PersistentPhysicalKey(target);
        candidate.menuVoiceHint = selection.voiceHint;
        if (ValidatePersistentRoute(candidate) != PersistentRouteStatus::Ok)
            return {Status::Unsupported, Issue::NativeTarget};
    }
    // Off has no hint field in LXRS. Raw hint provenance remains in the kit.
    output = candidate;
    return {};
}
Result FromTypedRoute(const PersistentRouteRecord& route, OriginalSelector& output) noexcept {
    using namespace modulation_contract;
    if (route.owner >= kOwnerCount || route.source > 1)
        return {Status::Invalid, Issue::Source};
    if (route.reserved != 0 || route.flags > 1)
        return {Status::Invalid, Issue::RouteMetadata};
    // Future namespaces/identities cannot be squeezed into source bytes. Current
    // host codes205..65535 are reserved, not supported plugin-only destinations.
    if (route.flags != 0 && route.identityNamespace != 1)
        return {Status::Nonrepresentable, Issue::RouteNamespace};
    if (route.flags != 0 && route.originalIndex == 0)
        return {Status::Invalid, Issue::RouteMetadata}; // Enabled OFF is corrupt.
    if (route.flags != 0 && route.originalIndex > 204)
        return {Status::Nonrepresentable, Issue::RouteIdentity};
    const auto validation = ValidatePersistentRoute(route);
    if (validation == PersistentRouteStatus::UnsupportedRoute)
        return {Status::Unsupported, Issue::NativeTarget};
    if (validation != PersistentRouteStatus::Ok && validation != PersistentRouteStatus::Disabled)
        return {Status::Invalid, Issue::RouteMetadata};
    return ResolveOriginalSelector({route.source == 0 ? SourceKind::Velocity : SourceKind::Lfo,
        voiceFromZeroBased(route.owner)}, route.originalIndex, route.menuVoiceHint, output);
}
Result ValidateForRuntime(const CanonicalKit& kit) noexcept {
    const auto result = ValidateCanonicalKit(kit);
    if (!result) return result;
    for (const auto& field : kFields) {
        std::uint16_t value = 0;
        const auto read = ReadField(kit, field.id, value);
        if (!read) return read;
        if (field.kind == FieldKind::OscillatorWave && value >= 6)
            return {Status::Unsupported, Issue::ExternalSample, field.id};
        if (field.kind == FieldKind::Reserved && value > 127)
            return {Status::Unsupported, Issue::ReservedPatchByte, field.id};
    }
    for (unsigned source = 0; source < 2; ++source) {
        for (unsigned owner = 0; owner < kOwnerCount; ++owner) {
            OriginalSelector selector {};
            auto r = SelectorFor(kit, source, owner, selector);
            if (!r) return r;
            PersistentRouteRecord route {};
            r = ToTypedRoute(selector, route);
            if (!r) {
                r.field = static_cast<std::uint16_t>((source == 0 ? 149 : 167) + owner);
                return r;
            }
        }
    }
    return {};
}
Result BuildPatchCandidate(const CanonicalKit& kit, PatchCandidate& output) noexcept {
    const auto result = ValidateForRuntime(kit);
    if (!result) return result;
    PatchCandidate candidate {};
    candidate.name = kit.name;
    for (const auto& field : kFields) {
        std::uint16_t value = 0;
        const auto read = ReadField(kit, field.id, value);
        if (!read) return read;
        // Explicit compatibility projection, not a seven-bit route encoding.
        candidate.patch[field.id] = field.kind == FieldKind::Selector
            ? 0 : static_cast<std::uint8_t>(value);
    }
    for (unsigned source = 0; source < 2; ++source) {
        for (unsigned owner = 0; owner < kOwnerCount; ++owner) {
            OriginalSelector selector {};
            const auto select = SelectorFor(kit, source, owner, selector);
            if (!select) return select;
            const auto route = ToTypedRoute(selector, candidate.routes[source * kOwnerCount + owner]);
            if (!route) return route;
        }
    }
    output = candidate;
    return {};
}
} // namespace lxr::kit
