#pragma once

#include "SequencerTypes.h"

#include <array>
#include <cstdint>

namespace lxr::sequencer
{

// Deterministic VST3 adaptation. The original STM32 uses hardware entropy, so
// this fixed xorshift32 sequence is reproducible but is not hardware-bit-exact.
class DeterministicRng final
{
public:
    explicit constexpr DeterministicRng(std::uint32_t seed = 0x9e3779b9u) noexcept
        : mInitialSeed(seed), mState(seed) {}

    constexpr void reset() noexcept { mState = mInitialSeed; }
    constexpr void reset(std::uint32_t seed) noexcept
    {
        mInitialSeed = seed;
        mState = seed;
    }

    constexpr std::uint32_t next() noexcept
    {
        std::uint32_t value = mState;
        value ^= value << 13u;
        value ^= value >> 17u;
        value ^= value << 5u;
        mState = value;
        return value;
    }

    constexpr std::uint8_t next7Bit() noexcept
    {
        return static_cast<std::uint8_t>(next() & 0x7fu);
    }

    constexpr std::uint32_t state() const noexcept { return mState; }
    constexpr std::uint32_t initialSeed() const noexcept { return mInitialSeed; }

private:
    std::uint32_t mInitialSeed;
    std::uint32_t mState;
};

constexpr bool ProbabilityPasses(std::uint8_t randomValue,
                                 std::uint8_t probability) noexcept
{
    // ORIGINAL QUIRK: inclusive comparison makes probability zero pass for RNG 0.
    return randomValue <= probability;
}

class ClusterProbability final
{
public:
    constexpr void reset(std::uint8_t value = 0u) noexcept
    {
        for (auto& item : mLatched)
            item = static_cast<std::uint8_t>(value & 0x7fu);
    }

    constexpr std::uint8_t boundary(Track track, bool boundaryStepActive,
                                    DeterministicRng& rng) noexcept
    {
        const std::size_t index = static_cast<std::size_t>(track);
        // ORIGINAL QUIRK: an inactive cluster boundary preserves the prior RNG.
        if (boundaryStepActive)
            mLatched[index] = rng.next7Bit();
        return mLatched[index];
    }

    constexpr std::uint8_t value(Track track) const noexcept
    {
        return mLatched[static_cast<std::size_t>(track)];
    }

    constexpr bool passes(Track track, std::uint8_t probability) const noexcept
    {
        return ProbabilityPasses(value(track), probability);
    }

private:
    std::array<std::uint8_t, kTrackCount> mLatched {};
};

} // namespace lxr::sequencer
