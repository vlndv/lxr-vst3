#pragma once

// Stable VST3 projections for the P15 native routing authority.
// Original target identities Copyright 2013 Julian Schmidt.
// Derived material is subject to LICENSE-LXR.txt.

#include "RoutingStateAuthority.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace lxr::route_host
{

inline constexpr std::uint16_t kFirstParameter = 228;
inline constexpr std::uint16_t kParameterCount = 12;
inline constexpr std::uint16_t kLastParameter = 239;
inline constexpr std::uint32_t kMaximumToken = 65535;

struct RouteHostParamId
{
    static constexpr RouteHostParamId fromRaw(std::uint32_t raw) noexcept
    {
        return raw >= kFirstParameter && raw <= kLastParameter
            ? RouteHostParamId(static_cast<std::uint16_t>(raw))
            : RouteHostParamId();
    }
    constexpr bool valid() const noexcept { return mValue >= kFirstParameter; }
    constexpr std::uint16_t value() const noexcept { return mValue; }
private:
    constexpr RouteHostParamId() noexcept = default;
    constexpr explicit RouteHostParamId(std::uint16_t value) noexcept
        : mValue(value) {}
    std::uint16_t mValue = 0;
};

struct RouteToken16
{
    constexpr RouteToken16() noexcept = default;
    static constexpr RouteToken16 fromRaw(std::uint32_t raw) noexcept
    {
        return raw <= kMaximumToken
            ? RouteToken16(static_cast<std::uint16_t>(raw), true)
            : RouteToken16();
    }
    constexpr bool valid() const noexcept { return mValid; }
    constexpr std::uint16_t value() const noexcept { return mValue; }
private:
    constexpr RouteToken16(std::uint16_t value, bool valid) noexcept
        : mValue(value), mValid(valid) {}
    std::uint16_t mValue = 0;
    bool mValid = false;
};

enum class TokenStatus : std::uint8_t
{
    Off,
    Accepted,
    Unavailable,
    Reserved,
    InvalidParameter
};

struct Descriptor
{
    RouteHostParamId id;
    const char* name;
    std::uint8_t source;
    std::uint8_t owner;
};

inline constexpr std::array<Descriptor, kParameterCount> kDescriptors {{
    {RouteHostParamId::fromRaw(228), "Velocity Destination D1", 0, 0},
    {RouteHostParamId::fromRaw(229), "Velocity Destination D2", 0, 1},
    {RouteHostParamId::fromRaw(230), "Velocity Destination D3", 0, 2},
    {RouteHostParamId::fromRaw(231), "Velocity Destination SN", 0, 3},
    {RouteHostParamId::fromRaw(232), "Velocity Destination CY", 0, 4},
    {RouteHostParamId::fromRaw(233), "Velocity Destination HH", 0, 5},
    {RouteHostParamId::fromRaw(234), "LFO Destination D1", 1, 0},
    {RouteHostParamId::fromRaw(235), "LFO Destination D2", 1, 1},
    {RouteHostParamId::fromRaw(236), "LFO Destination D3", 1, 2},
    {RouteHostParamId::fromRaw(237), "LFO Destination SN", 1, 3},
    {RouteHostParamId::fromRaw(238), "LFO Destination CY", 1, 4},
    {RouteHostParamId::fromRaw(239), "LFO Destination HH", 1, 5}
}};

constexpr std::size_t slot(RouteHostParamId id) noexcept
{
    return id.valid() ? static_cast<std::size_t>(id.value() - kFirstParameter)
                      : kParameterCount;
}

constexpr RouteHostParamId parameterForSlot(std::size_t routeSlot) noexcept
{
    return routeSlot < kParameterCount
        ? RouteHostParamId::fromRaw(
              static_cast<std::uint32_t>(kFirstParameter + routeSlot))
        : RouteHostParamId::fromRaw(0);
}

struct DecodeResult
{
    TokenStatus status = TokenStatus::InvalidParameter;
    PersistentRouteStatus validation = PersistentRouteStatus::InvalidTarget;
    PersistentRouteRecord record {};
    constexpr explicit operator bool() const noexcept
    {
        return status == TokenStatus::Off || status == TokenStatus::Accepted;
    }
};

constexpr DecodeResult decode(RouteHostParamId id, RouteToken16 token) noexcept
{
    const auto routeSlot = slot(id);
    if (routeSlot >= kDescriptors.size() || !token.valid())
        return {};
    const auto& descriptor = kDescriptors[routeSlot];
    auto record = MakeDisabledRoute(descriptor.source, descriptor.owner);
    if (token.value() == 0u)
        return {TokenStatus::Off, PersistentRouteStatus::Disabled, record};
    if (token.value() > 204u)
        return {TokenStatus::Reserved, PersistentRouteStatus::InvalidTarget,
                record};

    const auto* target = modulation_contract::lookup(
        modulation_contract::OriginalTargetIndex::fromRaw(token.value()));
    if (target == nullptr)
        return {TokenStatus::Unavailable, PersistentRouteStatus::InvalidTarget,
                record};
    record.flags = 1;
    record.identityNamespace = 1;
    record.originalIndex = token.value();
    record.firmwareParameter = target->parameter.value();
    record.valueType = static_cast<std::uint8_t>(target->valueType);
    record.scope = static_cast<std::uint8_t>(target->scope);
    record.physicalKey = PersistentPhysicalKey(*target);
    record.menuVoiceHint = descriptor.source == 1u
        ? static_cast<std::uint8_t>(descriptor.owner + 1u) : 0u;
    const auto validation = ValidatePersistentRoute(record);
    return validation == PersistentRouteStatus::Ok
        ? DecodeResult {TokenStatus::Accepted, validation, record}
        : DecodeResult {TokenStatus::Unavailable, validation, record};
}

constexpr RouteToken16 encode(const PersistentRouteRecord& record) noexcept
{
    return RouteToken16::fromRaw(record.flags == 0u ? 0u
                                                    : record.originalIndex);
}

constexpr bool validateDescriptors() noexcept
{
    for (std::size_t i = 0; i < kDescriptors.size(); ++i)
    {
        const auto& descriptor = kDescriptors[i];
        if (!descriptor.id.valid() || slot(descriptor.id) != i ||
            descriptor.name == nullptr || descriptor.name[0] == '\0' ||
            descriptor.source != i / 6u || descriptor.owner != i % 6u)
            return false;
    }
    return true;
}

static_assert(validateDescriptors(), "Stable route-host descriptor contract");
static_assert(kFirstParameter == 228 && kLastParameter == 239,
              "P15-M2H owns exactly VST3 IDs 228-239");

} // namespace lxr::route_host
