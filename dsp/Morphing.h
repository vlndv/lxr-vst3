#pragma once

// Firmware-faithful preset morphing for Sonic Potions LXR 0.37.
// Original algorithm Copyright 2013 Julian Schmidt.
// Derived material is subject to LICENSE-LXR.txt.

#include "KitContract.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace lxr::morph
{

inline constexpr std::size_t kParameterCount = kit::kFieldCount;
inline constexpr std::size_t kNameBytes = kit::kNameBytes;

using Values = std::array<std::uint8_t, kParameterCount>;

struct Target final
{
    std::array<std::uint8_t, kNameBytes> name {};
    Values values {};
};

enum class PublicationStatus : std::uint8_t
{
    Ok,
    InvalidCandidate,
    Busy,
    Stale,
    GenerationExhausted
};

struct PublicationResult final
{
    PublicationStatus status = PublicationStatus::InvalidCandidate;
    std::uint64_t order = 0;
    std::uint64_t generation = 0;

    constexpr explicit operator bool() const noexcept
    {
        return status == PublicationStatus::Ok;
    }
};

struct LoadResult final
{
    kit::Result candidate {};
    PublicationResult publication {};

    constexpr explicit operator bool() const noexcept
    {
        return static_cast<bool>(candidate) &&
               static_cast<bool>(publication);
    }
};

struct Snapshot final
{
    Target target {};
    std::uint8_t amount = 0;
    bool targetPresent = false;
    std::uint64_t order = 0;
    std::uint64_t generation = 0;
};

// ORIGINAL QUIRK: denominator 256 means amount 255 may miss target B.
// ORIGINAL QUIRK: low-byte remainders 127..255 round upward; this is not
// banker's rounding despite the comment in the original firmware.
constexpr std::uint8_t InterpolateByte(
    std::uint8_t source, std::uint8_t target,
    std::uint8_t amount) noexcept
{
    const int fixed = static_cast<int>(source) * 256 +
        (static_cast<int>(target) - static_cast<int>(source)) *
            static_cast<int>(amount);
    const auto value = static_cast<std::uint16_t>(fixed);
    const auto quotient = static_cast<std::uint8_t>(value / 256u);
    return static_cast<std::uint8_t>(quotient +
        ((value & 0xffu) < 0x7fu ? 0u : 1u));
}

constexpr bool IsSourceOwnedRouteByte(std::size_t parameter) noexcept
{
    return (parameter >= 149u && parameter <= 154u) ||
           (parameter >= 161u && parameter <= 172u);
}

inline void InterpolateAll(
    const Values& source, const Values& target,
    std::uint8_t amount, Values& output) noexcept
{
    for (std::size_t i = 0; i < output.size(); ++i)
        output[i] = InterpolateByte(source[i], target[i], amount);
}

// The original sweep calculates every byte. Effective typed routing is a
// separate source-owned operation, so the VST3 adaptation restores those
// eighteen provenance bytes after reproducing the complete numeric sweep.
inline void BuildEffectiveProjection(
    const Values& source, const Values& target,
    std::uint8_t amount, Values& output) noexcept
{
    InterpolateAll(source, target, amount, output);
    for (std::size_t i = 0; i < output.size(); ++i)
        if (IsSourceOwnedRouteByte(i))
            output[i] = source[i];
}

inline kit::Result ValidateTarget(
    const kit::CanonicalKit& candidate, Target& output) noexcept
{
    const auto valid = kit::ValidateForRuntime(candidate);
    if (!valid)
        return valid;

    kit::RawKit raw;
    const auto encoded = kit::EncodeRawKit(candidate, raw);
    if (!encoded)
        return encoded;

    Target accepted;
    accepted.name = raw.name;
    accepted.values = raw.values;
    output = accepted;
    return {};
}

// Reconstruct original full-byte selector identities from the typed routing
// authority. The 228-byte patch uses zero compatibility bytes for selectors;
// narrowing a 0..204 identity back through that storage would be lossy.
inline kit::Result BuildSourceValues(
    const std::array<std::uint8_t, kit::kFieldCount>& patch,
    const std::array<PersistentRouteRecord, kPersistentRouteCount>& routes,
    Values& output) noexcept
{
    for (std::size_t i = 0; i < patch.size(); ++i)
    {
        if (patch[i] > 127u)
            return {kit::Status::Invalid, kit::Issue::Range,
                    static_cast<std::uint16_t>(i)};
    }
    const auto routeStatus = ValidatePersistentRoutes(routes);
    if (routeStatus != PersistentRouteStatus::Ok)
        return {kit::Status::Invalid, kit::Issue::RouteMetadata};

    Values candidate = patch;
    for (std::size_t slot = 0; slot < routes.size(); ++slot)
    {
        const auto source = static_cast<std::uint8_t>(
            slot / kit::kOwnerCount);
        const auto owner = static_cast<std::uint8_t>(
            slot % kit::kOwnerCount);
        const auto selectorField = static_cast<std::size_t>(
            (source == 0u ? 149u : 167u) + owner);
        if (routes[slot].source != source || routes[slot].owner != owner)
            return {kit::Status::Invalid, kit::Issue::Source,
                    static_cast<std::uint16_t>(selectorField)};

        kit::OriginalSelector selector;
        auto converted = kit::FromTypedRoute(routes[slot], selector);
        if (!converted)
        {
            converted.field = static_cast<std::uint16_t>(selectorField);
            return converted;
        }
        candidate[selectorField] = static_cast<std::uint8_t>(
            selector.index.value()); // Source contract proves 0..204.
        // Disabled LFO routes do not carry a hint in LXRS. Keep the exact
        // source patch hint in that case; enabled routes own the typed hint.
        if (source != 0u && routes[slot].flags != 0u)
            candidate[161u + owner] = selector.voiceHint;
    }
    output = candidate;
    return {};
}

// Fixed, per-instance seqlock-style publication. Every payload byte is atomic,
// so an overlapping reader has no data race and rejects a torn attempt. Writers
// make one atomic_flag attempt and never wait.
class StateAuthority final
{
public:
    static constexpr unsigned kDefaultSnapshotAttempts = 16;

    struct WriteHandle final
    {
        WriteHandle() noexcept = default;
        WriteHandle(const WriteHandle&) = delete;
        WriteHandle& operator=(const WriteHandle&) = delete;
        bool active() const noexcept { return owner != nullptr; }
    private:
        friend class StateAuthority;
        StateAuthority* owner = nullptr;
    };

    StateAuthority() noexcept { initialize(); }

    // Construction-time initialization only.
    void initialize() noexcept
    {
        for (auto& value : mTarget)
            value.store(0u, std::memory_order_relaxed);
        for (auto& value : mName)
            value.store(0u, std::memory_order_relaxed);
        mAmount.store(0u, std::memory_order_relaxed);
        mTargetPresent.store(false, std::memory_order_relaxed);
        mLastOrder.store(0u, std::memory_order_relaxed);
        mSequence.store(0u, std::memory_order_relaxed);
        mWriter.clear(std::memory_order_relaxed);
    }

    bool tryBeginWrite(WriteHandle& handle) noexcept
    {
        if (handle.active() || mWriter.test_and_set(std::memory_order_acquire))
            return false;
        handle.owner = this;
        return true;
    }

    void cancelUnmodified(WriteHandle& handle) noexcept
    {
        if (handle.owner != this)
            return;
        handle.owner = nullptr;
        mWriter.clear(std::memory_order_release);
    }

    PublicationResult tryPublishTarget(
        const Target& target, std::uint64_t order,
        std::uint64_t expectedGeneration) noexcept
    {
        PublicationResult result;
        result.order = order;
        kit::RawKit raw;
        raw.name = target.name;
        raw.values = target.values;
        kit::CanonicalKit canonical;
        if (!kit::DecodeRawKit(raw, canonical) ||
            !kit::ValidateForRuntime(canonical))
        {
            result.status = PublicationStatus::InvalidCandidate;
            return result;
        }
        if (order == 0u)
        {
            result.status = PublicationStatus::Stale;
            return result;
        }
        WriteHandle writer;
        if (!tryBeginWrite(writer))
        {
            result.status = PublicationStatus::Busy;
            return result;
        }
        const auto stable = mSequence.load(std::memory_order_relaxed);
        const auto lastOrder = mLastOrder.load(std::memory_order_relaxed);
        if ((stable & 1u) != 0u || stable != expectedGeneration ||
            order <= lastOrder)
        {
            cancelUnmodified(writer);
            result.status = PublicationStatus::Stale;
            return result;
        }
        if (stable > UINT64_MAX - 2u)
        {
            cancelUnmodified(writer);
            result.status = PublicationStatus::GenerationExhausted;
            return result;
        }

        mSequence.store(stable + 1u, std::memory_order_release);
        for (std::size_t i = 0; i < target.values.size(); ++i)
            mTarget[i].store(target.values[i], std::memory_order_relaxed);
        for (std::size_t i = 0; i < target.name.size(); ++i)
            mName[i].store(target.name[i], std::memory_order_relaxed);
        mTargetPresent.store(true, std::memory_order_relaxed);
        mLastOrder.store(order, std::memory_order_relaxed);
        result.generation = stable + 2u;
        mSequence.store(result.generation, std::memory_order_release);
        writer.owner = nullptr;
        mWriter.clear(std::memory_order_release);
        result.status = PublicationStatus::Ok;
        return result;
    }

    PublicationResult tryPublishAmount(
        std::uint8_t amount, std::uint64_t order) noexcept
    {
        PublicationResult result;
        result.order = order;
        if (order == 0u)
        {
            result.status = PublicationStatus::Stale;
            return result;
        }
        WriteHandle writer;
        if (!tryBeginWrite(writer))
        {
            result.status = PublicationStatus::Busy;
            return result;
        }
        const auto stable = mSequence.load(std::memory_order_relaxed);
        const auto lastOrder = mLastOrder.load(std::memory_order_relaxed);
        if ((stable & 1u) != 0u || order <= lastOrder)
        {
            cancelUnmodified(writer);
            result.status = PublicationStatus::Stale;
            return result;
        }
        if (stable > UINT64_MAX - 2u)
        {
            cancelUnmodified(writer);
            result.status = PublicationStatus::GenerationExhausted;
            return result;
        }

        mSequence.store(stable + 1u, std::memory_order_release);
        mAmount.store(amount, std::memory_order_relaxed);
        mLastOrder.store(order, std::memory_order_relaxed);
        result.generation = stable + 2u;
        mSequence.store(result.generation, std::memory_order_release);
        writer.owner = nullptr;
        mWriter.clear(std::memory_order_release);
        result.status = PublicationStatus::Ok;
        return result;
    }

    PublicationResult tryClear(
        std::uint64_t order, std::uint64_t expectedGeneration) noexcept
    {
        PublicationResult result;
        result.order = order;
        if (order == 0u)
        {
            result.status = PublicationStatus::Stale;
            return result;
        }
        WriteHandle writer;
        if (!tryBeginWrite(writer))
        {
            result.status = PublicationStatus::Busy;
            return result;
        }
        const auto stable = mSequence.load(std::memory_order_relaxed);
        const auto lastOrder = mLastOrder.load(std::memory_order_relaxed);
        if ((stable & 1u) != 0u || stable != expectedGeneration ||
            order <= lastOrder)
        {
            cancelUnmodified(writer);
            result.status = PublicationStatus::Stale;
            return result;
        }
        if (stable > UINT64_MAX - 2u)
        {
            cancelUnmodified(writer);
            result.status = PublicationStatus::GenerationExhausted;
            return result;
        }

        mSequence.store(stable + 1u, std::memory_order_release);
        for (auto& value : mTarget)
            value.store(0u, std::memory_order_relaxed);
        for (auto& value : mName)
            value.store(0u, std::memory_order_relaxed);
        mTargetPresent.store(false, std::memory_order_relaxed);
        // The firmware amount is an independent performance control. Clearing
        // only the target keeps parameter 245 and its host automation intact.
        mLastOrder.store(order, std::memory_order_relaxed);
        result.generation = stable + 2u;
        mSequence.store(result.generation, std::memory_order_release);
        writer.owner = nullptr;
        mWriter.clear(std::memory_order_release);
        result.status = PublicationStatus::Ok;
        return result;
    }

    bool copyLocked(
        const WriteHandle& writer, Snapshot& output) const noexcept
    {
        if (writer.owner != this)
            return false;
        Snapshot candidate;
        for (std::size_t i = 0; i < candidate.target.values.size(); ++i)
            candidate.target.values[i] =
                mTarget[i].load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < candidate.target.name.size(); ++i)
            candidate.target.name[i] =
                mName[i].load(std::memory_order_relaxed);
        candidate.amount = mAmount.load(std::memory_order_relaxed);
        candidate.targetPresent =
            mTargetPresent.load(std::memory_order_relaxed);
        candidate.order = mLastOrder.load(std::memory_order_relaxed);
        candidate.generation = mSequence.load(std::memory_order_relaxed);
        output = candidate;
        return true;
    }

    bool trySnapshot(
        Snapshot& output,
        unsigned attempts = kDefaultSnapshotAttempts) const noexcept
    {
        for (unsigned attempt = 0; attempt < attempts; ++attempt)
        {
            const auto before = mSequence.load(std::memory_order_acquire);
            if ((before & 1u) != 0u)
                continue;
            Snapshot candidate;
            for (std::size_t i = 0; i < candidate.target.values.size(); ++i)
                candidate.target.values[i] =
                    mTarget[i].load(std::memory_order_relaxed);
            for (std::size_t i = 0; i < candidate.target.name.size(); ++i)
                candidate.target.name[i] =
                    mName[i].load(std::memory_order_relaxed);
            candidate.amount = mAmount.load(std::memory_order_relaxed);
            candidate.targetPresent =
                mTargetPresent.load(std::memory_order_relaxed);
            candidate.order = mLastOrder.load(std::memory_order_relaxed);
            const auto after = mSequence.load(std::memory_order_acquire);
            if (before == after && (after & 1u) == 0u)
            {
                candidate.generation = after;
                output = candidate;
                return true;
            }
        }
        return false;
    }

    std::uint64_t generation() const noexcept
    {
        return mSequence.load(std::memory_order_acquire);
    }

private:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "LXR morph generations must be lock-free on the target");

    std::array<std::atomic<std::uint8_t>, kParameterCount> mTarget {};
    std::array<std::atomic<std::uint8_t>, kNameBytes> mName {};
    std::atomic<std::uint8_t> mAmount {0};
    std::atomic<bool> mTargetPresent {false};
    std::atomic<std::uint64_t> mLastOrder {0};
    std::atomic<std::uint64_t> mSequence {0};
    std::atomic_flag mWriter = ATOMIC_FLAG_INIT;
};

} // namespace lxr::morph
