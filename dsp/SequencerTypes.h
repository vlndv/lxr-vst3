#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace lxr::sequencer
{

inline constexpr std::size_t kPatternCount = 8;
inline constexpr std::size_t kTrackCount = 7;
inline constexpr std::size_t kSynthesisOwnerCount = 6;
inline constexpr std::size_t kMainStepCount = 16;
inline constexpr std::size_t kSubstepCount = 8;
inline constexpr std::size_t kPhysicalPositionCount =
    kMainStepCount * kSubstepCount;
inline constexpr std::size_t kStoredStepCount =
    kPatternCount * kTrackCount * kPhysicalPositionCount;

inline constexpr std::uint8_t kStepActiveMask = 0x80u;
inline constexpr std::uint8_t kStepValueMask = 0x7fu;
inline constexpr std::uint8_t kDefaultVelocity = 100u;
inline constexpr std::uint8_t kDefaultProbability = 127u;
inline constexpr std::uint8_t kDefaultNote = 63u;
inline constexpr std::uint8_t kNoAutomation = 0xffu;
inline constexpr std::uint8_t kMaximumAutomationParameter = 227u;
inline constexpr std::uint8_t kMaximumPatternNextCode = 14u;
inline constexpr std::uint8_t kMaximumPatternRepeat = 127u;
inline constexpr std::uint8_t kMaximumShuffle = 127u;
inline constexpr std::uint16_t kDefaultTempo = 120u;
inline constexpr std::uint16_t kMinimumTempo = 1u;
inline constexpr std::uint16_t kMaximumTempo = 255u;
inline constexpr std::uint8_t kInitialPattern = 0u;

enum class Track : std::uint8_t
{
    Drum1 = 0,
    Drum2 = 1,
    Drum3 = 2,
    Snare = 3,
    Cymbal = 4,
    ClosedHiHat = 5,
    OpenHiHat = 6
};

enum class Articulation : std::uint8_t
{
    Standard = 0,
    ClosedHiHat = 1,
    OpenHiHat = 2
};

struct TrackIdentity
{
    std::uint8_t synthesisOwner = 0;
    Articulation articulation = Articulation::Standard;
};

inline constexpr std::array<TrackIdentity, kTrackCount> kTrackIdentities {{
    {0u, Articulation::Standard},
    {1u, Articulation::Standard},
    {2u, Articulation::Standard},
    {3u, Articulation::Standard},
    {4u, Articulation::Standard},
    {5u, Articulation::ClosedHiHat},
    {5u, Articulation::OpenHiHat}
}};

constexpr bool IsValidPattern(std::uint8_t value) noexcept
{
    return value < kPatternCount;
}

constexpr bool IsValidTrack(std::uint8_t value) noexcept
{
    return value < kTrackCount;
}

constexpr bool IsValidMainStep(std::uint8_t value) noexcept
{
    return value < kMainStepCount;
}

constexpr bool IsValidSubstep(std::uint8_t value) noexcept
{
    return value < kSubstepCount;
}

constexpr bool IsValidPhysicalPosition(std::uint8_t value) noexcept
{
    return value < kPhysicalPositionCount;
}

constexpr std::uint8_t PhysicalPosition(std::uint8_t mainStep,
                                        std::uint8_t substep) noexcept
{
    return static_cast<std::uint8_t>(mainStep * kSubstepCount + substep);
}

constexpr std::uint8_t MainStepOf(std::uint8_t physicalPosition) noexcept
{
    return static_cast<std::uint8_t>(physicalPosition / kSubstepCount);
}

constexpr std::uint8_t SubstepOf(std::uint8_t physicalPosition) noexcept
{
    return static_cast<std::uint8_t>(physicalPosition % kSubstepCount);
}

constexpr TrackIdentity IdentityForTrack(Track track) noexcept
{
    return kTrackIdentities[static_cast<std::size_t>(track)];
}

enum class Status : std::uint8_t
{
    Ok = 0,
    InvalidPattern,
    InvalidTrack,
    InvalidMainStep,
    InvalidSubstep,
    InvalidPhysicalPosition,
    InvalidProbability,
    InvalidNote,
    InvalidAutomationParameter,
    InvalidAutomationValue,
    InvalidTrackLength,
    InvalidRotation,
    InvalidNextPattern,
    InvalidRepeat,
    InvalidShuffle,
    InvalidTempo,
    InvalidBank,
    NonMonotonicTime,
    InsufficientCapacity,
    Busy,
    Stale,
    GenerationExhausted
};

} // namespace lxr::sequencer
