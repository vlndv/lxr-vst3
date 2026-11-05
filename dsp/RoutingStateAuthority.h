#pragma once

// Persistent modulation routing for Sonic Potions LXR 0.37.
// Original target identities Copyright 2013 Julian Schmidt.
// Derived material is subject to LICENSE-LXR.txt.

#include "PhysicalModulation.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace lxr
{

inline constexpr std::size_t kPersistentRouteCount = 12;
inline constexpr std::size_t kMaximumOpaqueRoutingExtensionBytes = 3576;

enum class PersistentRouteStatus : std::uint8_t
{
    Ok,
    Disabled,
    InvalidOwner,
    InvalidSource,
    InvalidFlags,
    InvalidOffRecord,
    UnsupportedNamespace,
    InvalidTarget,
    MetadataMismatch,
    UnsupportedRoute,
    ReservedField,
    DuplicateRecord,
    RecordOrder,
    InvalidOptionalData,
    Busy,
    Stale,
    GenerationExhausted
};

struct PersistentRouteRecord
{
    std::uint8_t owner = 0;
    std::uint8_t source = 0;
    std::uint16_t flags = 0;
    std::uint16_t identityNamespace = 0;
    std::uint16_t originalIndex = 0;
    std::uint16_t firmwareParameter = 0;
    std::uint8_t valueType = 0xffu;
    std::uint8_t scope = 0;
    std::uint16_t physicalKey = 0;
    std::uint8_t menuVoiceHint = 0;
    std::uint8_t reserved = 0;
};

static_assert(sizeof(PersistentRouteRecord) == 16,
              "The logical record is fixed-size, but is still encoded field-by-field");

struct PersistentRoutingState
{
    std::array<PersistentRouteRecord, kPersistentRouteCount> routes {};
    // Exact unknown optional TLV bytes, including their 16-byte headers. Known
    // routing data is never stored here. This is fixed-size per instance.
    std::array<std::uint8_t, kMaximumOpaqueRoutingExtensionBytes> optionalTlvs {};
    std::uint16_t optionalTlvBytes = 0;
    bool bypass = false;
};

inline constexpr std::array<std::uint8_t, 32> kOriginalRoutingTableSha256 {{
    0x9a, 0x26, 0x4f, 0x0d, 0xfd, 0x6b, 0x3e, 0xe6,
    0xc8, 0x8d, 0x88, 0x4a, 0x38, 0x59, 0x27, 0x0a,
    0xfc, 0xeb, 0x7b, 0x9f, 0x4f, 0xb0, 0xf2, 0x5b,
    0x75, 0xcc, 0xd0, 0x2c, 0x74, 0xcf, 0x11, 0xae
}};

constexpr std::uint16_t PersistentPhysicalKey(
    const modulation_contract::TargetMetadata& target) noexcept
{
    std::uint16_t key = target.parameter.value();
    for (const auto& row : modulation_contract::kOriginalTargets)
    {
        if (row.destination == target.destination && row.parameter.value() < key)
            key = row.parameter.value();
    }
    for (const auto& row : modulation_contract::kInternalOnlyParameters)
    {
        if (row.destination == target.destination && row.parameter.value() < key)
            key = row.parameter.value();
    }
    return key;
}

constexpr PersistentRouteRecord MakeDisabledRoute(
    std::uint8_t source, std::uint8_t owner) noexcept
{
    PersistentRouteRecord record {};
    record.source = source;
    record.owner = owner;
    return record;
}

constexpr PersistentRoutingState MakeDisabledRoutingState() noexcept
{
    PersistentRoutingState state {};
    for (std::uint8_t source = 0; source < 2; ++source)
    {
        for (std::uint8_t owner = 0; owner < 6; ++owner)
            state.routes[static_cast<std::size_t>(source) * 6u + owner] =
                MakeDisabledRoute(source, owner);
    }
    return state;
}

constexpr PersistentRouteStatus ValidatePersistentRoute(
    const PersistentRouteRecord& record) noexcept
{
    using namespace modulation_contract;
    if (record.owner >= 6u)
        return PersistentRouteStatus::InvalidOwner;
    if (record.source > 1u)
        return PersistentRouteStatus::InvalidSource;
    if (record.reserved != 0u)
        return PersistentRouteStatus::ReservedField;
    if (record.flags > 1u)
        return PersistentRouteStatus::InvalidFlags;

    if (record.flags == 0u)
    {
        return record.identityNamespace == 0u && record.originalIndex == 0u &&
               record.firmwareParameter == 0u && record.valueType == 0xffu &&
               record.scope == 0u && record.physicalKey == 0u &&
               record.menuVoiceHint == 0u
            ? PersistentRouteStatus::Disabled
            : PersistentRouteStatus::InvalidOffRecord;
    }

    if (record.identityNamespace != 1u)
        return PersistentRouteStatus::UnsupportedNamespace;
    if (record.originalIndex == 0u || record.originalIndex > 204u)
        return PersistentRouteStatus::InvalidTarget;

    const auto index = OriginalTargetIndex::fromRaw(record.originalIndex);
    const auto* target = lookup(index);
    if (target == nullptr)
        return PersistentRouteStatus::InvalidTarget;
    if (record.firmwareParameter != target->parameter.value() ||
        record.valueType != static_cast<std::uint8_t>(target->valueType) ||
        record.scope != static_cast<std::uint8_t>(target->scope) ||
        record.physicalKey != PersistentPhysicalKey(*target))
    {
        return PersistentRouteStatus::MetadataMismatch;
    }
    if ((record.source == 0u && record.menuVoiceHint != 0u) ||
        record.menuVoiceHint > 6u)
    {
        return PersistentRouteStatus::MetadataMismatch;
    }

    const SourceIdentity source {
        record.source == 0u ? SourceKind::Velocity : SourceKind::Lfo,
        voiceFromZeroBased(record.owner)
    };
    const auto nativeStatus = record.source == 0u
        ? validateVelocityDestination(source, index)
        : validateLfoDestination(source, index);
    return nativeStatus == VelocityRouteStatus::Ok
        ? PersistentRouteStatus::Ok
        : PersistentRouteStatus::UnsupportedRoute;
}

constexpr PersistentRouteStatus ValidatePersistentRoutes(
    const std::array<PersistentRouteRecord, kPersistentRouteCount>& routes) noexcept
{
    std::array<bool, kPersistentRouteCount> seen {};
    for (std::size_t i = 0; i < routes.size(); ++i)
    {
        const auto status = ValidatePersistentRoute(routes[i]);
        if (status != PersistentRouteStatus::Ok &&
            status != PersistentRouteStatus::Disabled)
        {
            return status;
        }
        const std::size_t slot =
            static_cast<std::size_t>(routes[i].source) * 6u + routes[i].owner;
        if (seen[slot])
            return PersistentRouteStatus::DuplicateRecord;
        seen[slot] = true;
        if (slot != i)
            return PersistentRouteStatus::RecordOrder;
    }
    return PersistentRouteStatus::Ok;
}

// Per-instance fixed routing authority and outer patch+routing coordinator.
// Every ordinary routing/extension byte is accessed only while Guard is held.
class RoutingStateAuthority final
{
public:
    struct Guard
    {
        Guard() noexcept = default;
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
        ~Guard() { if (owner != nullptr) owner->release(*this); }
        bool active() const noexcept { return owner != nullptr; }
    private:
        friend class RoutingStateAuthority;
        RoutingStateAuthority* owner = nullptr;
    };

    struct Snapshot
    {
        PersistentRoutingState state = MakeDisabledRoutingState();
        std::array<std::uint64_t, kPersistentRouteCount> routeGenerations {};
        std::uint64_t generation = 0;
        std::uint64_t restoreEpoch = 0;
        std::uint64_t restoreCutoff = 0;
    };

    void initialize(const PersistentRoutingState& state =
                        MakeDisabledRoutingState()) noexcept
    {
        mState = state;
        mGeneration.store(0, std::memory_order_relaxed);
        mRestoreEpoch = 0;
        mRestoreCutoff = 0;
        mRouteTickets.fill(0);
        mRouteGenerations.fill(0);
        mGate.clear(std::memory_order_relaxed);
    }

    // One bounded attempt. Audio callbacks never spin or block.
    bool tryAcquire(Guard& guard) noexcept
    {
        if (guard.active() || mGate.test_and_set(std::memory_order_acquire))
            return false;
        guard.owner = this;
        return true;
    }

    void release(Guard& guard) noexcept
    {
        if (guard.owner != this)
            return;
        guard.owner = nullptr;
        mGate.clear(std::memory_order_release);
    }

    bool copyLocked(const Guard& guard, Snapshot& snapshot) const noexcept
    {
        if (guard.owner != this)
            return false;
        snapshot.state = mState;
        snapshot.routeGenerations = mRouteGenerations;
        snapshot.generation = mGeneration.load(std::memory_order_acquire);
        snapshot.restoreEpoch = mRestoreEpoch;
        snapshot.restoreCutoff = mRestoreCutoff;
        return true;
    }

    bool routeLocked(const Guard& guard, std::size_t routeSlot,
                     PersistentRouteRecord& record,
                     std::uint64_t& routeGeneration,
                     std::uint64_t& restoreEpoch) const noexcept
    {
        if (guard.owner != this || routeSlot >= kPersistentRouteCount)
            return false;
        record = mState.routes[routeSlot];
        routeGeneration = mRouteGenerations[routeSlot];
        restoreEpoch = mRestoreEpoch;
        return true;
    }

    std::uint64_t commitPatchLocked(const Guard& guard) noexcept
    {
        if (guard.owner != this)
            return 0;
        return publishNextGeneration();
    }

    bool canPublishLocked(const Guard& guard) const noexcept
    {
        return guard.owner == this &&
               mGeneration.load(std::memory_order_relaxed) != UINT64_MAX;
    }

    PersistentRouteStatus validateRestoreLocked(
        const Guard& guard, const PersistentRoutingState& candidate,
        std::uint64_t cutoff) const noexcept
    {
        if (guard.owner != this)
            return PersistentRouteStatus::Busy;
        if (ValidatePersistentRoutes(candidate.routes) != PersistentRouteStatus::Ok ||
            candidate.optionalTlvBytes > candidate.optionalTlvs.size())
            return PersistentRouteStatus::InvalidOptionalData;
        if (cutoff <= mRestoreCutoff)
            return PersistentRouteStatus::Stale;
        if (mRestoreEpoch == UINT64_MAX ||
            mGeneration.load(std::memory_order_relaxed) == UINT64_MAX)
            return PersistentRouteStatus::GenerationExhausted;
        return PersistentRouteStatus::Ok;
    }

    std::uint64_t commitRestoreLocked(const Guard& guard,
                                      const PersistentRoutingState& candidate,
                                      std::uint64_t cutoff) noexcept
    {
        if (validateRestoreLocked(guard, candidate, cutoff) !=
            PersistentRouteStatus::Ok)
            return 0;
        const std::uint64_t next = nextGeneration();
        if (next == 0)
            return 0;
        mState = candidate;
        mRestoreCutoff = cutoff;
        ++mRestoreEpoch;
        mRouteTickets.fill(cutoff);
        mRouteGenerations.fill(next);
        mGeneration.store(next, std::memory_order_release);
        return next;
    }

    PersistentRouteStatus commitRouteLocked(
        const Guard& guard, const PersistentRouteRecord& record,
        std::uint64_t ticket, std::uint64_t expectedEpoch) noexcept
    {
        const auto validation = ValidatePersistentRoute(record);
        if (validation != PersistentRouteStatus::Ok &&
            validation != PersistentRouteStatus::Disabled)
        {
            return validation;
        }
        if (guard.owner != this)
            return PersistentRouteStatus::Busy;
        const std::size_t slot =
            static_cast<std::size_t>(record.source) * 6u + record.owner;
        if (expectedEpoch != mRestoreEpoch || ticket <= mRestoreCutoff ||
            ticket <= mRouteTickets[slot])
        {
            return PersistentRouteStatus::Stale;
        }
        const std::uint64_t next = nextGeneration();
        if (next == 0)
            return PersistentRouteStatus::GenerationExhausted;
        mState.routes[slot] = record;
        mRouteTickets[slot] = ticket;
        mRouteGenerations[slot] = next;
        mGeneration.store(next, std::memory_order_release);
        return validation;
    }

    PersistentRouteStatus republishRouteLocked(
        const Guard& guard, std::size_t routeSlot,
        std::uint64_t ticket, std::uint64_t expectedEpoch) noexcept
    {
        if (guard.owner != this)
            return PersistentRouteStatus::Busy;
        if (routeSlot >= kPersistentRouteCount)
            return PersistentRouteStatus::InvalidOwner;
        if (expectedEpoch != mRestoreEpoch || ticket <= mRestoreCutoff ||
            ticket <= mRouteTickets[routeSlot])
            return PersistentRouteStatus::Stale;
        const std::uint64_t next = nextGeneration();
        if (next == 0)
            return PersistentRouteStatus::GenerationExhausted;
        mRouteTickets[routeSlot] = ticket;
        mRouteGenerations[routeSlot] = next;
        mGeneration.store(next, std::memory_order_release);
        return PersistentRouteStatus::Ok;
    }

    std::uint64_t generation() const noexcept
    {
        return mGeneration.load(std::memory_order_acquire);
    }

private:
    std::uint64_t nextGeneration() const noexcept
    {
        const std::uint64_t current = mGeneration.load(std::memory_order_relaxed);
        return current == UINT64_MAX ? 0 : current + 1u;
    }
    std::uint64_t publishNextGeneration() noexcept
    {
        const std::uint64_t next = nextGeneration();
        if (next != 0)
            mGeneration.store(next, std::memory_order_release);
        return next;
    }

    PersistentRoutingState mState = MakeDisabledRoutingState();
    std::array<std::uint64_t, kPersistentRouteCount> mRouteTickets {};
    std::array<std::uint64_t, kPersistentRouteCount> mRouteGenerations {};
    std::uint64_t mRestoreEpoch = 0;
    std::uint64_t mRestoreCutoff = 0;
    std::atomic<std::uint64_t> mGeneration {0};
    std::atomic_flag mGate = ATOMIC_FLAG_INIT;
};

static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "Routing generations must be lock-free on the x64 target");

} // namespace lxr
