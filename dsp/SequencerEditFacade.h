#pragma once

#include "PatternStateAuthority.h"
#include "SequencerParameterLocks.h"
#include "TriggerScheduler.h"

#include <cstdint>

namespace lxr::sequencer
{

// P19-E non-real-time editing boundary. Every musical edit starts from one
// immutable generation, modifies one inactive full-bank candidate, validates
// it and publishes it once. The facade never exposes a mutable bank pointer.
class SequencerEditFacade final
{
public:
    enum class Result : std::uint8_t
    {
        Ok = 0,
        Busy,
        Stale,
        InvalidPattern,
        InvalidTrack,
        InvalidMainStep,
        InvalidSubstep,
        InvalidValue,
        InvalidLockSlot,
        InvalidLockTarget,
        GenerationExhausted,
        InvalidOperation
    };

    enum class Operation : std::uint8_t
    {
        ToggleMainGate = 0,
        SetNote,
        SetVelocity,
        SetProbability,
        SetSubstep,
        SetLockTarget,
        SetLockValue,
        ClearLock,
        SetTrackLength,
        SetRotation,
        SetPatternChain,
        SetShuffle,
        GenerateEuclidean,
        CopyPattern,
        CopyTrack,
        ClearPattern,
        ClearTrack
    };

    struct Request final
    {
        Operation operation = Operation::ToggleMainGate;
        std::uint8_t pattern = 0u;
        std::uint8_t track = 0u;
        std::uint8_t mainStep = 0u;
        std::uint8_t substep = 0u;
        std::uint8_t slot = 0u;
        std::uint8_t value = 0u;
        std::uint8_t value2 = 0u;
        std::uint8_t value3 = 0u;
        std::uint8_t source = 0u;
        std::uint8_t destination = 0u;
        bool active = false;
    };

    struct PatternSnapshot final
    {
        PatternData pattern {};
        std::uint64_t generation = 0u;
        std::uint8_t patternIndex = 0u;
        std::uint8_t shuffle = 0u;
    };

    struct ApplyResult final
    {
        Result status = Result::InvalidOperation;
        std::uint64_t generation = 0u;
    };

    explicit SequencerEditFacade(TriggerScheduler& scheduler) noexcept
        : mScheduler(&scheduler), mAuthority(scheduler.patternAuthority()) {}
    explicit SequencerEditFacade(PatternStateAuthority& authority) noexcept
        : mAuthority(authority) {}

    Result readPattern(std::uint8_t pattern, PatternSnapshot& output,
                       unsigned attempts = PatternStateAuthority::kDefaultAcquireAttempts) noexcept
    {
        if (!IsValidPattern(pattern))
            return Result::InvalidPattern;
        PatternStateAuthority::ReadHandle snapshot;
        const Status acquired = mAuthority.tryAcquire(snapshot, attempts);
        if (acquired != Status::Ok)
            return map(acquired);
        output.pattern = snapshot.bank().patterns[pattern];
        output.generation = snapshot.generation();
        output.patternIndex = pattern;
        output.shuffle = snapshot.bank().shuffle;
        return Result::Ok;
    }

    ApplyResult apply(const Request& request,
                      std::uint64_t expectedGeneration) noexcept
    {
        PatternStateAuthority::WriteHandle candidate;
        const Status begun = mAuthority.tryBeginEdit(candidate);
        if (begun != Status::Ok)
            return {map(begun), mAuthority.generation()};
        if (candidate.baseGeneration() != expectedGeneration)
        {
            candidate.cancel();
            return {Result::Stale, mAuthority.generation()};
        }

        const Result edited = mutate(candidate, request);
        if (edited != Result::Ok)
        {
            candidate.cancel();
            return {edited, mAuthority.generation()};
        }
        std::uint64_t generation = expectedGeneration;
        const Status committed = mAuthority.commit(candidate, &generation);
        return {map(committed), generation};
    }

    Result selectPattern(std::uint8_t pattern) noexcept
    {
        return mScheduler != nullptr
            ? map(mScheduler->selectPattern(pattern)) : Result::InvalidOperation;
    }
    std::uint8_t activePattern() const noexcept
    {
        return mScheduler != nullptr ? mScheduler->selectedPattern() : 0u;
    }
    std::uint8_t pendingPattern() const noexcept
    {
        return mScheduler != nullptr ? mScheduler->pendingPattern() : 0u;
    }
    Result setTrackMuted(std::uint8_t track, bool muted) noexcept
    {
        return mScheduler != nullptr
            ? map(mScheduler->setTrackMuted(track, muted)) : Result::InvalidOperation;
    }
    bool trackMuted(std::uint8_t track) const noexcept
    {
        return mScheduler != nullptr && mScheduler->trackMuted(track);
    }
    Result setRoll(std::uint8_t track, bool active) noexcept
    {
        return mScheduler != nullptr
            ? map(mScheduler->setRoll(track, active)) : Result::InvalidOperation;
    }

private:
    static Result map(Status status) noexcept
    {
        switch (status)
        {
        case Status::Ok: return Result::Ok;
        case Status::Busy: return Result::Busy;
        case Status::Stale: return Result::Stale;
        case Status::InvalidPattern: return Result::InvalidPattern;
        case Status::InvalidTrack: return Result::InvalidTrack;
        case Status::InvalidMainStep: return Result::InvalidMainStep;
        case Status::InvalidSubstep:
        case Status::InvalidPhysicalPosition: return Result::InvalidSubstep;
        case Status::GenerationExhausted: return Result::GenerationExhausted;
        default: return Result::InvalidValue;
        }
    }

    static Result validateCoordinates(const Request& request) noexcept
    {
        if (!IsValidPattern(request.pattern)) return Result::InvalidPattern;
        if (!IsValidTrack(request.track)) return Result::InvalidTrack;
        if (!IsValidMainStep(request.mainStep)) return Result::InvalidMainStep;
        if (!IsValidSubstep(request.substep)) return Result::InvalidSubstep;
        return Result::Ok;
    }

    static Result setStepField(PatternStateAuthority::WriteHandle& candidate,
                               const Request& request) noexcept
    {
        const Result coordinates = validateCoordinates(request);
        if (coordinates != Result::Ok) return coordinates;
        StepData value = candidate.candidate().patterns[request.pattern]
            .steps[request.track][PhysicalPosition(request.mainStep, request.substep)];
        switch (request.operation)
        {
        case Operation::SetNote:
            if (request.value > 127u) return Result::InvalidValue;
            value.note = request.value;
            break;
        case Operation::SetVelocity:
            if (request.value > 127u) return Result::InvalidValue;
            SetStepVelocity(value, request.value);
            break;
        case Operation::SetProbability:
            if (request.value > 127u) return Result::InvalidValue;
            value.probability = request.value;
            break;
        case Operation::SetSubstep:
            SetStepActive(value, request.active);
            break;
        case Operation::SetLockTarget:
        {
            if (request.slot > 1u) return Result::InvalidLockSlot;
            const std::uint8_t currentValue = request.slot == 0u
                ? value.value1 : value.value2;
            const auto decoded = DecodeAutomationSlot(request.value, currentValue);
            if (!decoded) return Result::InvalidLockTarget;
            (request.slot == 0u ? value.parameter1 : value.parameter2) = request.value;
            break;
        }
        case Operation::SetLockValue:
        {
            if (request.slot > 1u) return Result::InvalidLockSlot;
            if (request.value > 127u) return Result::InvalidValue;
            const std::uint8_t target = request.slot == 0u
                ? value.parameter1 : value.parameter2;
            const auto decoded = DecodeAutomationSlot(target, request.value);
            if (!decoded) return Result::InvalidLockTarget;
            (request.slot == 0u ? value.value1 : value.value2) = request.value;
            break;
        }
        case Operation::ClearLock:
            if (request.slot > 1u) return Result::InvalidLockSlot;
            if (request.slot == 0u) { value.parameter1 = kNoAutomation; value.value1 = 0u; }
            else { value.parameter2 = kNoAutomation; value.value2 = 0u; }
            break;
        default: return Result::InvalidOperation;
        }
        return map(candidate.setStep(request.pattern, request.track,
            PhysicalPosition(request.mainStep, request.substep), value));
    }

    static Result mutate(PatternStateAuthority::WriteHandle& candidate,
                         const Request& request) noexcept
    {
        switch (request.operation)
        {
        case Operation::ToggleMainGate:
        {
            const Result coordinates = validateCoordinates(request);
            if (coordinates != Result::Ok) return coordinates;
            const auto mask = static_cast<std::uint16_t>(1u << request.mainStep);
            const bool active = (candidate.candidate().patterns[request.pattern]
                .mainGates[request.track] & mask) == 0u;
            return map(candidate.setMainGate(request.pattern, request.track,
                                             request.mainStep, active));
        }
        case Operation::SetNote:
        case Operation::SetVelocity:
        case Operation::SetProbability:
        case Operation::SetSubstep:
        case Operation::SetLockTarget:
        case Operation::SetLockValue:
        case Operation::ClearLock:
            return setStepField(candidate, request);
        case Operation::SetTrackLength:
            return map(candidate.setTrackLength(request.pattern, request.track,
                                                request.value));
        case Operation::SetRotation:
            return map(candidate.setRotation(request.pattern, request.track,
                                             request.value));
        case Operation::SetPatternChain:
            return map(candidate.setPatternChain(request.pattern, request.value,
                                                 request.value2));
        case Operation::SetShuffle:
            return map(candidate.setShuffle(request.value));
        case Operation::GenerateEuclidean:
            return map(candidate.generateEuclidean(request.pattern, request.track,
                request.value, request.value2, request.value3));
        case Operation::CopyPattern:
            return map(candidate.copyPattern(request.source, request.destination));
        case Operation::CopyTrack:
            return map(candidate.copyTrack(request.pattern, request.source,
                                           request.destination));
        case Operation::ClearPattern:
            return map(candidate.resetPattern(request.pattern));
        case Operation::ClearTrack:
            return map(candidate.clearTrack(request.pattern, request.track));
        }
        return Result::InvalidOperation;
    }

    TriggerScheduler* mScheduler = nullptr;
    PatternStateAuthority& mAuthority;
};

} // namespace lxr::sequencer
