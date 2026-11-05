#include "TriggerScheduler.h"

#include "Engine.h"

namespace lxr::sequencer
{

TriggerScheduler::TriggerScheduler(std::uint32_t seed) noexcept
    : mRng(seed)
{
    reset(0u);
}

void TriggerScheduler::incrementSaturating(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max())
        ++value;
}

Status TriggerScheduler::setTempo(std::uint16_t bpm) noexcept
{
    return mClock.setTempo(bpm);
}

Status TriggerScheduler::selectPattern(std::uint8_t pattern) noexcept
{
    if (!IsValidPattern(pattern))
        return Status::InvalidPattern;
    // Pattern chaining/manual boundary transitions are deliberately excluded.
    // Selecting a different current pattern while running would invent timing.
    if (mClock.running())
        return Status::Busy;
    mSelectedPattern = pattern;
    return Status::Ok;
}

Status TriggerScheduler::initializeTrackPositions() noexcept
{
    PatternStateAuthority::ReadHandle snapshot;
    const Status acquired = mPatterns.tryAcquire(snapshot);
    if (acquired != Status::Ok)
        return acquired;
    const PatternData& pattern = snapshot.bank().patterns[mSelectedPattern];
    for (std::size_t track = 0; track < kTrackCount; ++track)
    {
        mTrackLengthCodes[track] = pattern.lengthCodes[track];
        std::uint8_t effectiveLength = pattern.lengthCodes[track];
        if (effectiveLength == 0u)
            effectiveLength = static_cast<std::uint8_t>(kMainStepCount);
        std::uint8_t rotation = pattern.rotations[track];
        if (effectiveLength < kMainStepCount && rotation > effectiveLength)
            rotation = static_cast<std::uint8_t>(rotation % effectiveLength);
        mTrackPositions[track] = static_cast<std::int16_t>(rotation * kSubstepCount) - 1;
    }
    return Status::Ok;
}

Status TriggerScheduler::start() noexcept
{
    if (mClock.running())
        return Status::Ok;
    const Status initialized = initializeTrackPositions();
    if (initialized != Status::Ok)
        return initialized;
    mClock.start();
    return Status::Ok;
}

void TriggerScheduler::stop() noexcept
{
    mClock.stop();
    mLastBatch.clear();
}

void TriggerScheduler::reset(std::uint64_t nativeFrame) noexcept
{
    mClock.reset(nativeFrame);
    mRng.reset();
    mProbability.reset(0u);
    mLastBatch.clear();
    for (auto& position : mTrackPositions)
        position = -1;
    for (auto& length : mTrackLengthCodes)
        length = 0u;
}

void TriggerScheduler::advanceTrack(std::size_t track,
                                    std::uint8_t lengthCode) noexcept
{
    std::uint8_t effectiveLength = lengthCode;
    if (effectiveLength == 0u)
        effectiveLength = static_cast<std::uint8_t>(kMainStepCount);
    std::int16_t next = static_cast<std::int16_t>(mTrackPositions[track] + 1);
    const std::int16_t end = static_cast<std::int16_t>(effectiveLength * kSubstepCount);
    if (next >= end || next >= static_cast<std::int16_t>(kPhysicalPositionCount))
        next = 0;
    mTrackPositions[track] = next;
}

void TriggerScheduler::generateEvents(
    const ClockEvent& clockEvent,
    const PatternStateAuthority::ReadHandle& snapshot) noexcept
{
    const PatternData& pattern = snapshot.bank().patterns[mSelectedPattern];
    for (std::size_t track = 0; track < kTrackCount; ++track)
    {
        mTrackLengthCodes[track] = pattern.lengthCodes[track];
        advanceTrack(track, mTrackLengthCodes[track]);
        const auto position = static_cast<std::uint8_t>(mTrackPositions[track]);
        const std::uint8_t mainStep = MainStepOf(position);
        const std::uint8_t substep = SubstepOf(position);
        if ((pattern.mainGates[track] & (std::uint16_t{1u} << mainStep)) == 0u)
            continue;

        const StepData& step = pattern.steps[track][position];
        if (!IsStepActive(step))
            continue;

        const Track logicalTrack = static_cast<Track>(track);
        if (substep == 0u)
            (void) mProbability.boundary(logicalTrack, true, mRng);
        if (!mProbability.passes(logicalTrack, step.probability))
            continue;

        const TrackIdentity identity = IdentityForTrack(logicalTrack);
        TriggerEvent event {};
        event.nativeBoundary = clockEvent.quantizedNativeFrame;
        event.clockOrdinal = clockEvent.ordinal;
        event.snapshotGeneration = snapshot.generation();
        event.stableOrder = clockEvent.ordinal * kTrackCount + track;
        event.pattern = mSelectedPattern;
        event.track = static_cast<std::uint8_t>(track);
        event.synthesisOwner = identity.synthesisOwner;
        event.articulation = identity.articulation;
        event.physicalPosition = position;
        event.mainStep = mainStep;
        event.substep = substep;
        event.note = step.note;
        event.velocity = StepVelocity(step);
        if (!mLastBatch.push(event))
            incrementSaturating(mDiagnostics.rejectedOverflowEvents);
    }
}

void TriggerScheduler::dispatch(const TriggerEvent& event, Engine& engine) noexcept
{
    switch (event.synthesisOwner)
    {
        case 0u:
        case 1u:
        case 2u:
            engine.triggerDrum(event.synthesisOwner, event.velocity, event.note);
            break;
        case 3u:
            engine.triggerSnare(event.velocity, event.note);
            break;
        case 4u:
            engine.triggerCymbal(event.velocity, event.note);
            break;
        case 5u:
            engine.triggerHiHat(
                event.velocity,
                event.articulation == Articulation::OpenHiHat,
                event.note);
            break;
        default:
            break;
    }
}

Status TriggerScheduler::processNativeBlock(
    std::uint64_t nativeBoundary,
    Engine& engine,
    unsigned snapshotAttempts) noexcept
{
    incrementSaturating(mDiagnostics.processedNativeBlocks);
    mLastBatch.clear();

    ClockEvent clockEvent {};
    std::size_t written = 0;
    const std::uint64_t endFrame = nativeBoundary == std::numeric_limits<std::uint64_t>::max()
        ? nativeBoundary : nativeBoundary + 1u;
    const Status clockStatus = mClock.advanceTo(endFrame, &clockEvent, 1u, written);
    if (clockStatus == Status::NonMonotonicTime)
    {
        incrementSaturating(mDiagnostics.nonMonotonicBoundaries);
        return clockStatus;
    }
    if (clockStatus == Status::InsufficientCapacity)
        incrementSaturating(mDiagnostics.clockCapacityFaults);
    if (written == 0u)
        return clockStatus;

    incrementSaturating(mDiagnostics.duePhysicalPositions);
    PatternStateAuthority::ReadHandle snapshot;
    const Status acquired = mPatterns.tryAcquire(snapshot, snapshotAttempts);
    if (acquired != Status::Ok)
    {
        // ORIGINAL-ORDER ADAPTATION: the clock boundary remains consumed when
        // snapshot publication churn prevents a coherent read. Advance using
        // the last seven validated length bytes, but emit no unpinned data.
        for (std::size_t track = 0; track < kTrackCount; ++track)
            advanceTrack(track, mTrackLengthCodes[track]);
        incrementSaturating(mDiagnostics.snapshotBusyBoundaries);
        return acquired;
    }

    generateEvents(clockEvent, snapshot);
    snapshot.release();
    for (const TriggerEvent& event : mLastBatch)
    {
        dispatch(event, engine);
        incrementSaturating(mDiagnostics.emittedEvents);
    }
    return clockStatus;
}

std::uint8_t TriggerScheduler::probabilityLatch(std::uint8_t track) const noexcept
{
    return IsValidTrack(track)
        ? mProbability.value(static_cast<Track>(track)) : 0u;
}

std::uint8_t TriggerScheduler::trackPosition(std::uint8_t track) const noexcept
{
    return IsValidTrack(track) && mTrackPositions[track] >= 0
        ? static_cast<std::uint8_t>(mTrackPositions[track]) : 0u;
}

} // namespace lxr::sequencer
