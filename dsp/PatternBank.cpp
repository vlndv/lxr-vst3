#include "PatternBank.h"

namespace lxr::sequencer
{

namespace
{

constexpr bool ValidAutomationParameter(std::uint8_t value) noexcept
{
    return value == kNoAutomation || value <= kMaximumAutomationParameter;
}

Status ValidateCoordinates(std::uint8_t pattern, std::uint8_t track) noexcept
{
    if (!IsValidPattern(pattern))
        return Status::InvalidPattern;
    if (!IsValidTrack(track))
        return Status::InvalidTrack;
    return Status::Ok;
}

void ResetPatternData(PatternData& pattern) noexcept
{
    for (std::size_t track = 0; track < kTrackCount; ++track)
    {
        for (std::size_t position = 0; position < kPhysicalPositionCount; ++position)
        {
            pattern.steps[track][position] =
                MakeDefaultStep((position % kSubstepCount) == 0u);
        }
        pattern.mainGates[track] = 0u;
        pattern.lengthCodes[track] = 0u;
        pattern.rotations[track] = 0u;
    }
}

} // namespace

Status ValidateStep(const StepData& step) noexcept
{
    // Every volume byte is canonical: bit 7 is active and bits 0..6 velocity.
    if (step.probability > kDefaultProbability)
        return Status::InvalidProbability;
    if (step.note > 127u)
        return Status::InvalidNote;
    if (!ValidAutomationParameter(step.parameter1) ||
        !ValidAutomationParameter(step.parameter2))
        return Status::InvalidAutomationParameter;
    if (step.value1 > 127u || step.value2 > 127u)
        return Status::InvalidAutomationValue;
    return Status::Ok;
}

Status ValidatePattern(const PatternData& pattern) noexcept
{
    for (std::size_t track = 0; track < kTrackCount; ++track)
    {
        if (pattern.lengthCodes[track] > 15u)
            return Status::InvalidTrackLength;
        if (pattern.rotations[track] > 15u)
            return Status::InvalidRotation;
        for (const auto& step : pattern.steps[track])
        {
            const Status status = ValidateStep(step);
            if (status != Status::Ok)
                return status;
        }
    }
    if (pattern.repeat > kMaximumPatternRepeat)
        return Status::InvalidRepeat;
    if (pattern.nextPattern > kMaximumPatternNextCode)
        return Status::InvalidNextPattern;
    return Status::Ok;
}

Status ValidateBank(const PatternBank& bank) noexcept
{
    if (bank.shuffle > kMaximumShuffle)
        return Status::InvalidShuffle;
    for (const auto& pattern : bank.patterns)
    {
        const Status status = ValidatePattern(pattern);
        if (status != Status::Ok)
            return status;
    }
    return Status::Ok;
}

void ResetPattern(PatternBank& bank, std::uint8_t pattern) noexcept
{
    if (!IsValidPattern(pattern))
        return;
    // ORIGINAL QUIRK: seq_clearPattern leaves repeat/next metadata untouched.
    const std::uint8_t repeat = bank.patterns[pattern].repeat;
    const std::uint8_t next = bank.patterns[pattern].nextPattern;
    ResetPatternData(bank.patterns[pattern]);
    bank.patterns[pattern].repeat = repeat;
    bank.patterns[pattern].nextPattern = next;
}

void ResetBank(PatternBank& bank) noexcept
{
    bank.shuffle = 0u;
    for (std::size_t pattern = 0; pattern < kPatternCount; ++pattern)
    {
        ResetPatternData(bank.patterns[pattern]);
        bank.patterns[pattern].repeat = 0u;
        bank.patterns[pattern].nextPattern = static_cast<std::uint8_t>(pattern);
    }
}

PatternBank MakeDefaultPatternBank() noexcept
{
    PatternBank bank {};
    ResetBank(bank);
    return bank;
}

Status GetStep(const PatternBank& bank, std::uint8_t pattern,
               std::uint8_t track, std::uint8_t position,
               StepData& result) noexcept
{
    const Status coordinates = ValidateCoordinates(pattern, track);
    if (coordinates != Status::Ok)
        return coordinates;
    if (!IsValidPhysicalPosition(position))
        return Status::InvalidPhysicalPosition;
    result = bank.patterns[pattern].steps[track][position];
    return Status::Ok;
}

Status SetStep(PatternBank& bank, std::uint8_t pattern,
               std::uint8_t track, std::uint8_t position,
               const StepData& value) noexcept
{
    const Status coordinates = ValidateCoordinates(pattern, track);
    if (coordinates != Status::Ok)
        return coordinates;
    if (!IsValidPhysicalPosition(position))
        return Status::InvalidPhysicalPosition;
    const Status validation = ValidateStep(value);
    if (validation != Status::Ok)
        return validation;
    bank.patterns[pattern].steps[track][position] = value;
    return Status::Ok;
}

Status GetMainGate(const PatternBank& bank, std::uint8_t pattern,
                   std::uint8_t track, std::uint8_t mainStep,
                   bool& result) noexcept
{
    const Status coordinates = ValidateCoordinates(pattern, track);
    if (coordinates != Status::Ok)
        return coordinates;
    if (!IsValidMainStep(mainStep))
        return Status::InvalidMainStep;
    result = (bank.patterns[pattern].mainGates[track] &
              (std::uint16_t{1} << mainStep)) != 0u;
    return Status::Ok;
}

Status SetMainGate(PatternBank& bank, std::uint8_t pattern,
                   std::uint8_t track, std::uint8_t mainStep,
                   bool active) noexcept
{
    const Status coordinates = ValidateCoordinates(pattern, track);
    if (coordinates != Status::Ok)
        return coordinates;
    if (!IsValidMainStep(mainStep))
        return Status::InvalidMainStep;
    const std::uint16_t mask = static_cast<std::uint16_t>(std::uint16_t{1} << mainStep);
    auto& gates = bank.patterns[pattern].mainGates[track];
    gates = active ? static_cast<std::uint16_t>(gates | mask)
                   : static_cast<std::uint16_t>(gates & static_cast<std::uint16_t>(~mask));
    return Status::Ok;
}

Status GetTrackLength(const PatternBank& bank, std::uint8_t pattern,
                      std::uint8_t track, std::uint8_t& effectiveLength) noexcept
{
    const Status coordinates = ValidateCoordinates(pattern, track);
    if (coordinates != Status::Ok)
        return coordinates;
    const std::uint8_t code = bank.patterns[pattern].lengthCodes[track];
    if (code > 15u)
        return Status::InvalidTrackLength;
    effectiveLength = code == 0u ? 16u : code;
    return Status::Ok;
}

Status SetTrackLength(PatternBank& bank, std::uint8_t pattern,
                      std::uint8_t track, std::uint8_t effectiveLength) noexcept
{
    const Status coordinates = ValidateCoordinates(pattern, track);
    if (coordinates != Status::Ok)
        return coordinates;
    if (effectiveLength < 1u || effectiveLength > 16u)
        return Status::InvalidTrackLength;
    bank.patterns[pattern].lengthCodes[track] =
        effectiveLength == 16u ? 0u : effectiveLength;
    return Status::Ok;
}

Status GetRotation(const PatternBank& bank, std::uint8_t pattern,
                   std::uint8_t track, std::uint8_t& rotation) noexcept
{
    const Status coordinates = ValidateCoordinates(pattern, track);
    if (coordinates != Status::Ok)
        return coordinates;
    rotation = bank.patterns[pattern].rotations[track];
    return rotation <= 15u ? Status::Ok : Status::InvalidRotation;
}

Status SetRotation(PatternBank& bank, std::uint8_t pattern,
                   std::uint8_t track, std::uint8_t rotation) noexcept
{
    const Status coordinates = ValidateCoordinates(pattern, track);
    if (coordinates != Status::Ok)
        return coordinates;
    if (rotation > 15u)
        return Status::InvalidRotation;
    bank.patterns[pattern].rotations[track] = rotation;
    return Status::Ok;
}

Status SetPatternChain(PatternBank& bank, std::uint8_t pattern,
                       std::uint8_t repeat, std::uint8_t nextPattern) noexcept
{
    if (!IsValidPattern(pattern))
        return Status::InvalidPattern;
    if (repeat > kMaximumPatternRepeat)
        return Status::InvalidRepeat;
    if (nextPattern > kMaximumPatternNextCode)
        return Status::InvalidNextPattern;
    bank.patterns[pattern].repeat = repeat;
    bank.patterns[pattern].nextPattern = nextPattern;
    return Status::Ok;
}

Status SetShuffle(PatternBank& bank, std::uint8_t shuffle) noexcept
{
    if (shuffle > kMaximumShuffle)
        return Status::InvalidShuffle;
    bank.shuffle = shuffle;
    return Status::Ok;
}

Status CopyPattern(PatternBank& bank, std::uint8_t source,
                   std::uint8_t destination) noexcept
{
    if (!IsValidPattern(source) || !IsValidPattern(destination))
        return Status::InvalidPattern;
    if (source == destination)
        return Status::Ok;
    // ORIGINAL QUIRK: seq_copyPattern excludes repeat/next metadata.
    const std::uint8_t repeat = bank.patterns[destination].repeat;
    const std::uint8_t next = bank.patterns[destination].nextPattern;
    bank.patterns[destination] = bank.patterns[source];
    bank.patterns[destination].repeat = repeat;
    bank.patterns[destination].nextPattern = next;
    return Status::Ok;
}

} // namespace lxr::sequencer
