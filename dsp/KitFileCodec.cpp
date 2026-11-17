#include "KitFileCodec.h"
#include <limits>

namespace lxr::kit::snd {
namespace {
Result Translate(kit::Result result, std::uint16_t field = 0xffffu,
                 std::uint8_t route = 0xffu) noexcept {
    if (result) return {};
    const auto id = field == 0xffffu ? result.field : field;
    const auto error = result.status == kit::Status::Nonrepresentable ? Error::NonrepresentableRoute :
        result.status == kit::Status::Unsupported ? Error::UnsupportedRoute :
        result.issue == Issue::Selector ? Error::InvalidSelector :
        route != 0xffu ? Error::InvalidRoute : Error::InvalidField;
    return {error, id, route, result.issue};
}
Result CheckRoutes(const RawKit& raw) noexcept {
    using namespace modulation_contract;
    for (std::uint8_t source = 0; source < 2; ++source) {
        for (std::uint8_t owner = 0; owner < kOwnerCount; ++owner) {
            const auto field = static_cast<std::uint16_t>((source ? 167 : 149) + owner);
            const auto slot = static_cast<std::uint8_t>(source * kOwnerCount + owner);
            OriginalSelector selector {};
            auto result = ResolveOriginalSelector({source ? SourceKind::Lfo : SourceKind::Velocity,
                voiceFromZeroBased(owner)}, raw.values[field], source ? raw.values[161 + owner] : 0, selector);
            if (!result) return Translate(result, field, slot);
            PersistentRouteRecord route {};
            result = ToTypedRoute(selector, route);
            if (!result) return Translate(result, field, slot);
        }
    }
    return {};
}
Result CanonicalRaw(const CanonicalKit& kit, RawKit& raw) noexcept {
    const auto converted = EncodeRawKit(kit, raw);
    return converted ? CheckRoutes(raw) : Translate(converted);
}
Result CanonicalRaw(const CanonicalKit& kit,
                    const std::array<PersistentRouteRecord, kPersistentRouteCount>& routes,
                    RawKit& raw) noexcept {
    auto converted = EncodeRawKit(kit, raw);
    if (!converted) return Translate(converted);
    for (std::uint8_t slot = 0; slot < kPersistentRouteCount; ++slot) {
        const auto source = static_cast<std::uint8_t>(slot / kOwnerCount);
        const auto owner = static_cast<std::uint8_t>(slot % kOwnerCount);
        const auto field = static_cast<std::uint16_t>((source ? 167 : 149) + owner);
        if (routes[slot].source != source || routes[slot].owner != owner)
            return {Error::InvalidRoute, field, slot, Issue::Source};
        OriginalSelector selector {};
        converted = FromTypedRoute(routes[slot], selector);
        if (!converted) return Translate(converted, field, slot);
        raw.values[field] = static_cast<std::uint8_t>(selector.index.value()); // Proven0..204.
        if (source && routes[slot].flags != 0) raw.values[161 + owner] = selector.voiceHint;
    }
    return CheckRoutes(raw);
}
Result Output(const void* source, std::size_t sourceBytes,
              std::uint8_t* output, std::size_t capacity) noexcept {
    if (!output) return {Error::InvalidArgument};
    if (capacity < kSize) return {Error::BufferTooSmall};
    // Pointer-integer conversion is used only to reject overlapping caller
    // buffers; no object image is read, written, or reinterpreted through it.
    const auto a = reinterpret_cast<std::uintptr_t>(source);
    const auto b = reinterpret_cast<std::uintptr_t>(output);
    const auto limit = std::numeric_limits<std::uintptr_t>::max();
    if (a > limit - sourceBytes || b > limit - kSize ||
        (a < b + kSize && b < a + sourceBytes)) return {Error::InvalidArgument};
    return {};
}
} // namespace
Result DecodeRaw(const std::uint8_t* input, std::size_t size, RawKit& output) noexcept {
    if (!input && size != 0) return {Error::InvalidArgument};
    if (size < kSize) return {Error::Truncated};
    if (size > kSize) return {Error::Oversized};
    RawKit candidate {};
    for (std::size_t i = 0; i < kNameBytes; ++i) candidate.name[i] = input[i];
    for (std::size_t i = 0; i < kFieldCount; ++i) candidate.values[i] = input[kPayloadOffset + i];
    output = candidate;
    return {};
}
Result EncodeRaw(const RawKit& raw, std::uint8_t* output, std::size_t capacity) noexcept {
    const auto result = Output(&raw, sizeof(raw), output, capacity);
    if (!result) return result;
    // Explicit fields, independent of object layout/endian/packing.
    Bytes candidate {};
    for (std::size_t i = 0; i < kNameBytes; ++i) candidate[i] = raw.name[i];
    for (std::size_t i = 0; i < kFieldCount; ++i) candidate[kPayloadOffset + i] = raw.values[i];
    for (std::size_t i = 0; i < kSize; ++i) output[i] = candidate[i];
    return {};
}
Result DecodeCanonical(const std::uint8_t* input, std::size_t size, CanonicalKit& output) noexcept {
    RawKit raw {};
    auto result = DecodeRaw(input, size, raw);
    if (!result) return result;
    CanonicalKit candidate {};
    const auto converted = DecodeRawKit(raw, candidate);
    if (!converted) return Translate(converted);
    result = CheckRoutes(raw);
    if (!result) return result;
    output = candidate;
    return {};
}
Result CheckRepresentability(const CanonicalKit& kit) noexcept {
    RawKit raw {};
    return CanonicalRaw(kit, raw);
}
Result CheckRepresentability(const CanonicalKit& kit,
    const std::array<PersistentRouteRecord, kPersistentRouteCount>& routes) noexcept {
    RawKit raw {};
    return CanonicalRaw(kit, routes, raw);
}
Result EncodeCanonical(const CanonicalKit& kit, std::uint8_t* output, std::size_t capacity) noexcept {
    auto result = Output(&kit, sizeof(kit), output, capacity);
    if (!result) return result;
    RawKit raw {};
    result = CanonicalRaw(kit, raw);
    return result ? EncodeRaw(raw, output, capacity) : result;
}
Result EncodeCanonical(const CanonicalKit& kit,
    const std::array<PersistentRouteRecord, kPersistentRouteCount>& routes,
    std::uint8_t* output, std::size_t capacity) noexcept {
    auto result = Output(&kit, sizeof(kit), output, capacity);
    if (!result) return result;
    result = Output(routes.data(), sizeof(routes), output, capacity);
    if (!result) return result;
    RawKit raw {};
    result = CanonicalRaw(kit, routes, raw);
    return result ? EncodeRaw(raw, output, capacity) : result;
}
} // namespace lxr::kit::snd
