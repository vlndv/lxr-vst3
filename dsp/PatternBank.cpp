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

void ResetTrackData(PatternData& pattern, std::size_t track) noexcept
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

constexpr std::uint8_t ObjectSize(std::uint16_t group) noexcept
{
    std::uint8_t result = 0u;
    while (group != 0u)
    {
        ++result;
        group = static_cast<std::uint16_t>(group >> 1u);
    }
    return result == 0u ? 1u : result;
}

struct EuclideanBuilder
{
    std::uint16_t result = 0u;
    std::uint16_t nextCount = 0u;
    std::uint16_t originalLength = 0u;

    void calculate(std::uint8_t length, std::uint8_t pulses,
                   std::uint16_t group1, std::uint16_t group2,
                   std::uint8_t depth) noexcept
    {
        if (depth >= kMainStepCount)
            return;
        const std::uint16_t remainder = pulses != 0u
            ? static_cast<std::uint16_t>(length % pulses) : 0u;
        if (nextCount == 0u)
            nextCount = pulses;

        const int objectCount1 = static_cast<int>(nextCount);
        const int objectSize1 = static_cast<int>(ObjectSize(group1));
        const int objectSize2 = static_cast<int>(ObjectSize(group2));
        const int objectCount2 =
            (static_cast<int>(originalLength) - objectCount1 * objectSize1) /
            objectSize2;
        nextCount = static_cast<std::uint16_t>(
            objectCount1 > objectCount2 ? objectCount2 : objectCount1);

        if (objectCount2 <= 1)
        {
            std::uint8_t bufferPosition = 0u;
            for (int object = 0; object < objectCount1; ++object)
            {
                for (int bit = objectSize1 - 1; bit >= 0; --bit)
                {
                    if ((group1 & (std::uint16_t{1u} << bit)) != 0u)
                        result = static_cast<std::uint16_t>(
                            result | (std::uint16_t{1u} << bufferPosition));
                    ++bufferPosition;
                }
            }
            for (int object = 0; object < objectCount2; ++object)
            {
                for (int bit = objectSize2 - 1; bit >= 0; --bit)
                {
                    if ((group2 & (std::uint16_t{1u} << bit)) != 0u)
                        result = static_cast<std::uint16_t>(
                            result | (std::uint16_t{1u} << bufferPosition));
                    ++bufferPosition;
                }
            }
            return;
        }

        const std::uint16_t originalGroup1 = group1;
        group1 = static_cast<std::uint16_t>(group1 << objectSize2);
        for (int bit = 0; bit < objectSize2; ++bit)
        {
            if ((group2 & (std::uint16_t{1u} << bit)) != 0u)
                group1 = static_cast<std::uint16_t>(
                    group1 | (std::uint16_t{1u} << bit));
        }
        group2 = objectCount2 - objectCount1 > 0 ? group2 : originalGroup1;
        calculate(pulses, static_cast<std::uint8_t>(remainder),
                  group1, group2, static_cast<std::uint8_t>(depth + 1u));
    }
};

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

Status ClearTrack(PatternBank& bank, std::uint8_t pattern,
                  std::uint8_t track) noexcept
{
    const Status coordinates = ValidateCoordinates(pattern, track);
    if (coordinates != Status::Ok)
        return coordinates;
    ResetTrackData(bank.patterns[pattern], track);
    return Status::Ok;
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

Status CopyTrack(PatternBank& bank, std::uint8_t pattern,
                 std::uint8_t source, std::uint8_t destination) noexcept
{
    if (!IsValidPattern(pattern))
        return Status::InvalidPattern;
    if (!IsValidTrack(source) || !IsValidTrack(destination))
        return Status::InvalidTrack;
    if (source == destination)
        return Status::Ok;
    PatternData& data = bank.patterns[pattern];
    data.steps[destination] = data.steps[source];
    data.mainGates[destination] = data.mainGates[source];
    data.lengthCodes[destination] = data.lengthCodes[source];
    data.rotations[destination] = data.rotations[source];
    return Status::Ok;
}

Status GenerateEuclidean(PatternBank& bank, std::uint8_t pattern,
                         std::uint8_t track, std::uint8_t length,
                         std::uint8_t pulses, std::uint8_t rotation) noexcept
{
    const Status coordinates = ValidateCoordinates(pattern, track);
    if (coordinates != Status::Ok)
        return coordinates;
    // ORIGINAL QUIRK: the setters coerce zero length/pulses to one and clamp
    // excess pulses to the current length before generation.
    if (length > kMainStepCount)
        return Status::InvalidTrackLength;
    if (length == 0u)
        length = 1u;
    if (pulses == 0u)
        pulses = 1u;
    if (pulses > length)
        pulses = length;
    // ORIGINAL QUIRK: the generator accepts rotation == length as a no-op.
    if (rotation > length)
        return Status::InvalidRotation;

    EuclideanBuilder builder {};
    builder.originalLength = length;
    builder.calculate(length, pulses, 1u, 0u, 0u);
    if (rotation != 0u && length != 1u && (rotation % length) != 0u)
    {
        const std::uint8_t amount = static_cast<std::uint8_t>(rotation % length);
        builder.result = static_cast<std::uint16_t>(
            (builder.result << amount) |
            (builder.result >> (length - amount)));
        const std::uint32_t mask = (std::uint32_t{1u} << length) - 1u;
        builder.result = static_cast<std::uint16_t>(builder.result & mask);
    }

    PatternData& data = bank.patterns[pattern];
    data.mainGates[track] = builder.result;
    data.lengthCodes[track] = length == kMainStepCount ? 0u : length;
    // ORIGINAL QUIRK: generator rotation is not copied to track rotation and
    // every physical StepData record remains untouched.
    return Status::Ok;
}

} // namespace lxr::sequencer
