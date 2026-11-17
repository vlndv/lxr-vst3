#pragma once

// Transactional runtime whole-kit publication for Sonic Potions LXR 0.37.
// Original identities Copyright 2013 Julian Schmidt.
// Derived material is subject to LICENSE-LXR.txt.

#include "KitContract.h"
#include "ParameterStateAuthority.h"

#include <cstddef>
#include <cstdint>

namespace lxr::kit
{

enum class PublicationStatus : std::uint8_t
{
    Ok,
    InvalidCandidate,
    Busy,
    Stale,
    GenerationExhausted
};

struct PublicationReservation final
{
    std::uint64_t order = 0;
    std::uint64_t expectedJointGeneration = 0;
};

struct PublicationResult final
{
    PublicationStatus status = PublicationStatus::InvalidCandidate;
    std::uint16_t field = 0xffffu;
    std::uint8_t route = 0xffu;
    PersistentRouteStatus routeStatus = PersistentRouteStatus::Ok;
    std::uint64_t reservation = 0;
    std::uint64_t patchGeneration = 0;
    std::uint64_t jointGeneration = 0;

    constexpr explicit operator bool() const noexcept
    {
        return status == PublicationStatus::Ok;
    }
};

struct RuntimeLoadResult final
{
    Result candidate {};
    PublicationResult publication {};

    constexpr explicit operator bool() const noexcept
    {
        return static_cast<bool>(candidate) &&
               static_cast<bool>(publication);
    }
};

inline PublicationResult ValidatePatchCandidate(
    const PatchCandidate& candidate) noexcept
{
    for (std::size_t i = 0; i < candidate.patch.size(); ++i)
    {
        if (candidate.patch[i] > 127u)
        {
            PublicationResult result;
            result.field = static_cast<std::uint16_t>(i);
            return result;
        }
    }

    for (std::size_t i = 0; i < candidate.routes.size(); ++i)
    {
        const auto status = ValidatePersistentRoute(candidate.routes[i]);
        if (status != PersistentRouteStatus::Ok &&
            status != PersistentRouteStatus::Disabled)
        {
            PublicationResult result;
            result.route = static_cast<std::uint8_t>(i);
            result.routeStatus = status;
            return result;
        }
        const std::size_t slot =
            static_cast<std::size_t>(candidate.routes[i].source) *
                kOwnerCount + candidate.routes[i].owner;
        if (slot != i)
        {
            PublicationResult result;
            result.route = static_cast<std::uint8_t>(i);
            result.routeStatus = PersistentRouteStatus::RecordOrder;
            return result;
        }
    }

    PublicationResult result;
    result.status = PublicationStatus::Ok;
    return result;
}

inline PublicationStatus PublicationStatusFor(
    PersistentRouteStatus status) noexcept
{
    if (status == PersistentRouteStatus::Busy)
        return PublicationStatus::Busy;
    if (status == PersistentRouteStatus::Stale)
        return PublicationStatus::Stale;
    if (status == PersistentRouteStatus::GenerationExhausted)
        return PublicationStatus::GenerationExhausted;
    return PublicationStatus::InvalidCandidate;
}

// One bounded attempt. All callers, including tests, must use the routing
// authority as the outer gate for every patch writer. The route generation
// release-store is the cross-authority commit point. Coordinated readers take
// that same gate before snapshotting the patch authority.
inline PublicationResult PublishPatchCandidate(
    const PatchCandidate& candidate,
    PublicationReservation reservation,
    ParameterStateAuthority<kFieldCount>& patchAuthority,
    RoutingStateAuthority& routingAuthority) noexcept
{
    auto result = ValidatePatchCandidate(candidate);
    result.reservation = reservation.order;
    if (!result)
        return result;
    if (reservation.order == 0u)
    {
        result.status = PublicationStatus::Stale;
        return result;
    }

    RoutingStateAuthority::Guard outer;
    if (!routingAuthority.tryAcquire(outer))
    {
        result.status = PublicationStatus::Busy;
        return result;
    }

    const auto routeStatus = routingAuthority.validateKitLocked(
        outer, candidate.routes, reservation.order,
        reservation.expectedJointGeneration);
    if (routeStatus != PersistentRouteStatus::Ok)
    {
        result.status = PublicationStatusFor(routeStatus);
        result.routeStatus = routeStatus;
        return result;
    }

    // Stable generations are even. Reserving the next odd/even pair must not
    // wrap. No patch payload has been touched when this check rejects.
    if (patchAuthority.generation() > UINT64_MAX - 2u)
    {
        result.status = PublicationStatus::GenerationExhausted;
        return result;
    }

    ParameterStateAuthority<kFieldCount>::WriteHandle writer;
    if (!patchAuthority.tryBeginWrite(writer))
    {
        result.status = PublicationStatus::Busy;
        return result;
    }
    if (!patchAuthority.writeAll(writer, candidate.patch))
    {
        // ValidatePatchCandidate checked every byte before the writer began.
        // This branch can only precede payload writes in writeAll().
        patchAuthority.cancelUnmodified(writer);
        result.status = PublicationStatus::InvalidCandidate;
        return result;
    }

    result.patchGeneration = patchAuthority.commit(writer);
    result.jointGeneration = routingAuthority.commitValidatedKitLocked(
        outer, candidate.name, candidate.routes, reservation.order,
        result.patchGeneration);
    // Both generations were preflighted while outer stayed held. A zero here
    // would require memory corruption or a violated writer-ownership contract.
    if (result.patchGeneration == 0u || result.jointGeneration == 0u)
    {
        result.status = PublicationStatus::GenerationExhausted;
        return result;
    }
    result.status = PublicationStatus::Ok;
    return result;
}

} // namespace lxr::kit
