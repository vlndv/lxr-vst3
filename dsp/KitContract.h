#pragma once
#include "KitTypes.h"

namespace lxr::kit {
enum class Status : std::uint8_t { Ok, Invalid, Unsupported, Nonrepresentable };
enum class Issue : std::uint8_t {
    None, FieldId, Range, Source, Selector, VoiceHint, CrossVoice, GlobalRoute,
    NativeTarget, ExternalSample, ReservedPatchByte, RouteMetadata, RouteNamespace,
    RouteIdentity
};
struct Result final {
    Status status = Status::Ok;
    Issue issue = Issue::None;
    std::uint16_t field = 0xffffu;
    constexpr explicit operator bool() const noexcept { return status == Status::Ok; }
};
struct FieldContract final {
    std::uint16_t id;
    const char* symbol;
    const char* canonicalField;
    Owner owner;
    FieldKind kind;
    std::uint16_t maximum;
    std::uint8_t initializationDefault;
    NativeField native;
};
inline constexpr std::array<FieldContract, kFieldCount> kFields {{
#define K_BYTE(id, symbol, path, owner, kind, maximum, native) \
    {id, #symbol, #path, Owner::owner, FieldKind::kind, maximum, 0, NativeField::native},
#define K_SELECTOR K_BYTE
#include "KitFields.inc"
#undef K_SELECTOR
#undef K_BYTE
}};
constexpr bool ValidFieldTable() noexcept {
    std::array<unsigned, 8> counts {};
    for (std::size_t i = 0; i < kFields.size(); ++i) {
        if (kFields[i].id != i || static_cast<unsigned>(kFields[i].owner) >= counts.size()) return false;
        ++counts[static_cast<unsigned>(kFields[i].owner)];
    }
    constexpr std::array<unsigned, 8> expected {{37,37,37,36,37,38,1,5}};
    for (std::size_t i = 0; i < counts.size(); ++i) if (counts[i] != expected[i]) return false;
    return true;
}
static_assert(ValidFieldTable(), "Every source slot owned exactly once");

// All APIs are bounded and pure. Failed operations leave output unchanged.
// Source validation admits unsupported native routes/waves without normalizing.
RawKit MakeFirmwareInitializationKit() noexcept;
CanonicalKit MakeCanonicalInitializationKit() noexcept;
Result ValidateRawKit(const RawKit&) noexcept;
Result ValidateCanonicalKit(const CanonicalKit&) noexcept;
Result DecodeRawKit(const RawKit&, CanonicalKit&) noexcept;
Result EncodeRawKit(const CanonicalKit&, RawKit&) noexcept;
Result ReadField(const CanonicalKit&, std::uint16_t id, std::uint16_t& value) noexcept;
Result ResolveOriginalSelector(modulation_contract::SourceIdentity, std::uint64_t raw,
                               std::uint64_t voiceHint, OriginalSelector&) noexcept;
Result ValidateOriginalSelector(const OriginalSelector&) noexcept;
Result ToTypedRoute(const OriginalSelector&, PersistentRouteRecord&) noexcept;
Result FromTypedRoute(const PersistentRouteRecord&, OriginalSelector&) noexcept;
Result ValidateForRuntime(const CanonicalKit&) noexcept;
// Selector149..154/167..172 become explicit zero compatibility bytes. Their
// full identities are in routes, never &127, wrapped, or inferred from bytes.
// Voice hints161..166 and all other scalar/reserved bytes remain exact.
Result BuildPatchCandidate(const CanonicalKit&, PatchCandidate&) noexcept;
} // namespace lxr::kit
