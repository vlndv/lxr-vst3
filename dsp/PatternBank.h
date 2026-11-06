#pragma once

#include "SequencerTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace lxr::sequencer
{

struct StepData
{
    std::uint8_t volume = kDefaultVelocity;
    std::uint8_t probability = kDefaultProbability;
    std::uint8_t note = kDefaultNote;
    std::uint8_t parameter1 = kNoAutomation;
    std::uint8_t value1 = 0;
    std::uint8_t parameter2 = kNoAutomation;
    std::uint8_t value2 = 0;
};

static_assert(sizeof(StepData) == 7, "StepData must match the seven source bytes");
static_assert(std::is_trivially_copyable<StepData>::value,
              "Pattern snapshots require trivial fixed-width records");
static_assert(offsetof(StepData, volume) == 0 &&
              offsetof(StepData, probability) == 1 &&
              offsetof(StepData, note) == 2 &&
              offsetof(StepData, parameter1) == 3 &&
              offsetof(StepData, value1) == 4 &&
              offsetof(StepData, parameter2) == 5 &&
              offsetof(StepData, value2) == 6,
              "StepData field order must remain source-faithful");

constexpr bool IsStepActive(const StepData& step) noexcept
{
    return (step.volume & kStepActiveMask) != 0u;
}

constexpr std::uint8_t StepVelocity(const StepData& step) noexcept
{
    return static_cast<std::uint8_t>(step.volume & kStepValueMask);
}

constexpr void SetStepActive(StepData& step, bool active) noexcept
{
    step.volume = static_cast<std::uint8_t>(
        (step.volume & kStepValueMask) | (active ? kStepActiveMask : 0u));
}

constexpr void SetStepVelocity(StepData& step, std::uint8_t velocity) noexcept
{
    step.volume = static_cast<std::uint8_t>(
        (step.volume & kStepActiveMask) | (velocity & kStepValueMask));
}

struct PatternData
{
    std::array<std::array<StepData, kPhysicalPositionCount>, kTrackCount> steps {};
    std::array<std::uint16_t, kTrackCount> mainGates {};
    // Explicit bytes replace the source's compiler-dependent bitfield union.
    // Length is the source encoding: 0 means 16, 1..15 mean themselves.
    std::array<std::uint8_t, kTrackCount> lengthCodes {};
    std::array<std::uint8_t, kTrackCount> rotations {};
    std::uint8_t repeat = 0;
    std::uint8_t nextPattern = 0;
};

struct PatternBank
{
    std::array<PatternData, kPatternCount> patterns {};
    std::uint8_t shuffle = 0;
};

static_assert(std::is_trivially_copyable<PatternData>::value, "fixed snapshot payload");
static_assert(std::is_trivially_copyable<PatternBank>::value, "fixed snapshot payload");

constexpr StepData MakeDefaultStep(bool boundarySubstep) noexcept
{
    StepData step {};
    if (boundarySubstep)
        SetStepActive(step, true);
    return step;
}

Status ValidateStep(const StepData& step) noexcept;
Status ValidatePattern(const PatternData& pattern) noexcept;
Status ValidateBank(const PatternBank& bank) noexcept;

void ResetPattern(PatternBank& bank, std::uint8_t pattern) noexcept;
void ResetBank(PatternBank& bank) noexcept;
PatternBank MakeDefaultPatternBank() noexcept;

Status GetStep(const PatternBank& bank, std::uint8_t pattern,
               std::uint8_t track, std::uint8_t position,
               StepData& result) noexcept;
Status SetStep(PatternBank& bank, std::uint8_t pattern,
               std::uint8_t track, std::uint8_t position,
               const StepData& value) noexcept;
Status GetMainGate(const PatternBank& bank, std::uint8_t pattern,
                   std::uint8_t track, std::uint8_t mainStep,
                   bool& result) noexcept;
Status SetMainGate(PatternBank& bank, std::uint8_t pattern,
                   std::uint8_t track, std::uint8_t mainStep,
                   bool active) noexcept;
Status GetTrackLength(const PatternBank& bank, std::uint8_t pattern,
                      std::uint8_t track, std::uint8_t& effectiveLength) noexcept;
Status SetTrackLength(PatternBank& bank, std::uint8_t pattern,
                      std::uint8_t track, std::uint8_t effectiveLength) noexcept;
Status GetRotation(const PatternBank& bank, std::uint8_t pattern,
                   std::uint8_t track, std::uint8_t& rotation) noexcept;
Status SetRotation(PatternBank& bank, std::uint8_t pattern,
                   std::uint8_t track, std::uint8_t rotation) noexcept;
Status SetPatternChain(PatternBank& bank, std::uint8_t pattern,
                       std::uint8_t repeat, std::uint8_t nextPattern) noexcept;
Status SetShuffle(PatternBank& bank, std::uint8_t shuffle) noexcept;
Status CopyPattern(PatternBank& bank, std::uint8_t source,
                   std::uint8_t destination) noexcept;
Status CopyTrack(PatternBank& bank, std::uint8_t pattern,
                 std::uint8_t source, std::uint8_t destination) noexcept;
Status ClearTrack(PatternBank& bank, std::uint8_t pattern,
                  std::uint8_t track) noexcept;
Status GenerateEuclidean(PatternBank& bank, std::uint8_t pattern,
                         std::uint8_t track, std::uint8_t length,
                         std::uint8_t pulses, std::uint8_t rotation) noexcept;

} // namespace lxr::sequencer
