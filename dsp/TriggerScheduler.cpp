#include "TriggerScheduler.h"

#include "Engine.h"

namespace lxr::sequencer
{

namespace
{

constexpr std::array<std::uint8_t, 14> kRollPeriods {{
    0xffu, 128u, 64u, 43u, 32u, 50u, 16u,
    11u, 8u, 6u, 4u, 3u, 2u, 1u
}};

constexpr std::uint8_t EffectiveLength(std::uint8_t code) noexcept
{
    return code == 0u ? static_cast<std::uint8_t>(kMainStepCount) : code;
}

} // namespace

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

Status TriggerScheduler::setShuffle(std::uint8_t shuffle) noexcept
{
    if (shuffle > kMaximumShuffle)
        return Status::InvalidShuffle;
    mRequestedShuffle.store(shuffle, std::memory_order_release);
    return Status::Ok;
}

Status TriggerScheduler::selectPattern(std::uint8_t pattern) noexcept
{
    if (!IsValidPattern(pattern))
        return Status::InvalidPattern;
    mPendingPattern.store(pattern, std::memory_order_release);
    if (mClock.running())
        mManualPatternPending.store(true, std::memory_order_release);
    else
    {
        mActivePattern = pattern;
        mPublishedActivePattern.store(pattern, std::memory_order_release);
        mManualPatternPending.store(false, std::memory_order_release);
    }
    return Status::Ok;
}

Status TriggerScheduler::setTrackMuted(std::uint8_t track, bool muted) noexcept
{
    if (!IsValidTrack(track))
        return Status::InvalidTrack;
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << track);
    if (muted)
        (void) mMutedTracks.fetch_or(mask, std::memory_order_acq_rel);
    else
        (void) mMutedTracks.fetch_and(static_cast<std::uint8_t>(~mask),
                                     std::memory_order_acq_rel);
    return Status::Ok;
}

bool TriggerScheduler::trackMuted(std::uint8_t track) const noexcept
{
    return IsValidTrack(track) &&
        (mMutedTracks.load(std::memory_order_acquire) &
         static_cast<std::uint8_t>(1u << track)) != 0u;
}

Status TriggerScheduler::setRollRate(std::uint8_t selector) noexcept
{
    if (selector >= kRollPeriods.size())
        return Status::InvalidRollRate;
    mRollPeriod.store(kRollPeriods[selector], std::memory_order_release);
    mRollRateSelector.store(selector, std::memory_order_release);
    return Status::Ok;
}

Status TriggerScheduler::setRoll(std::uint8_t track, bool active) noexcept
{
    if (!IsValidTrack(track))
        return Status::InvalidTrack;
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << track);
    if (active)
    {
        (void) mRollState.fetch_or(mask, std::memory_order_acq_rel);
        if (mRollPeriod.load(std::memory_order_acquire) == 0xffu)
            (void) mPendingOneShotRolls.fetch_or(mask, std::memory_order_acq_rel);
    }
    else
        (void) mRollState.fetch_and(static_cast<std::uint8_t>(~mask),
                                   std::memory_order_acq_rel);
    return Status::Ok;
}

void TriggerScheduler::cachePatternControls(const PatternBank& bank) noexcept
{
    for (std::size_t pattern = 0; pattern < kPatternCount; ++pattern)
    {
        const PatternData& source = bank.patterns[pattern];
        mCachedRepeats[pattern] = source.repeat;
        mCachedNextPatterns[pattern] = source.nextPattern;
        for (std::size_t track = 0; track < kTrackCount; ++track)
        {
            if (mCacheValid &&
                source.rotations[track] != mCachedRotations[pattern][track])
            {
                mZeroRotationMasks[pattern] = static_cast<std::uint8_t>(
                    mZeroRotationMasks[pattern] &
                    static_cast<std::uint8_t>(~(1u << track)));
            }
            mCachedLengthCodes[pattern][track] = source.lengthCodes[track];
            mCachedRotations[pattern][track] = source.rotations[track];
        }
    }
    if (!mCacheValid || bank.shuffle != mCachedBankShuffle)
    {
        mCachedBankShuffle = bank.shuffle;
        mRequestedShuffle.store(bank.shuffle, std::memory_order_release);
    }
    mCacheValid = true;
}

void TriggerScheduler::initializeTrackPositions(std::uint8_t pattern) noexcept
{
    for (std::size_t track = 0; track < kTrackCount; ++track)
    {
        const std::uint8_t lengthCode = mCachedLengthCodes[pattern][track];
        const std::uint8_t length = EffectiveLength(lengthCode);
        std::uint8_t rotation =
            (mZeroRotationMasks[pattern] &
             static_cast<std::uint8_t>(1u << track)) != 0u
            ? 0u : mCachedRotations[pattern][track];
        if (length < kMainStepCount && rotation > length)
            rotation = static_cast<std::uint8_t>(rotation % length);
        mTrackLengthCodes[track] = lengthCode;
        mTrackPositions[track] =
            static_cast<std::int16_t>(rotation * kSubstepCount) - 1;
    }
}

Status TriggerScheduler::initializeTrackPositions() noexcept
{
    PatternStateAuthority::ReadHandle snapshot;
    const Status acquired = mPatterns.tryAcquire(snapshot);
    if (acquired != Status::Ok)
        return acquired;
    cachePatternControls(snapshot.bank());
    mActivePattern = mPublishedActivePattern.load(std::memory_order_acquire);
    initializeTrackPositions(mActivePattern);
    snapshot.release();
    return Status::Ok;
}

Status TriggerScheduler::start() noexcept
{
    if (mClock.running())
        return Status::Ok;
    const Status initialized = initializeTrackPositions();
    if (initialized != Status::Ok)
        return initialized;
    (void) mClock.setShuffle(mRequestedShuffle.load(std::memory_order_acquire));
    mClock.start();
    return Status::Ok;
}

void TriggerScheduler::stop() noexcept
{
    mClock.stop();
    // ORIGINAL QUIRK: firmware stop zeroes all rotations of the playing
    // pattern. The immutable-bank adaptation records equivalent runtime
    // overrides; a later explicit rotation publication clears each override.
    mZeroRotationMasks[mActivePattern] = 0x7fu;
    mBarCounter = 0u;
    mMasterStepCounter = 0u;
    mLastBatch.clear();
    mLastAutomationBatch.clear();
}

void TriggerScheduler::reset(std::uint64_t nativeFrame) noexcept
{
    mClock.reset(nativeFrame);
    mRng.reset();
    mProbability.reset(0u);
    mLastBatch.clear();
    mLastAutomationBatch.clear();
    for (auto& position : mTrackPositions)
        position = -1;
    for (auto& length : mTrackLengthCodes)
        length = 0u;
    mActivePattern = kInitialPattern;
    mPublishedActivePattern.store(kInitialPattern, std::memory_order_relaxed);
    mPendingPattern.store(kInitialPattern, std::memory_order_relaxed);
    mManualPatternPending.store(false, std::memory_order_relaxed);
    mRequestedShuffle.store(0u, std::memory_order_relaxed);
    mMutedTracks.store(0u, std::memory_order_relaxed);
    mRollState.store(0u, std::memory_order_relaxed);
    mPendingOneShotRolls.store(0u, std::memory_order_relaxed);
    mRollPeriod.store(8u, std::memory_order_relaxed);
    mRollRateSelector.store(8u, std::memory_order_relaxed);
    mBarCounter = 0u;
    mMasterStepCounter = 0u;
    mCachedBankShuffle = 0u;
    mCacheValid = false;
    for (auto& mask : mZeroRotationMasks)
        mask = 0u;
}

bool TriggerScheduler::trackWillWrap(std::size_t track,
                                     std::uint8_t lengthCode) const noexcept
{
    if (mTrackPositions[track] < 0)
        return false;
    const std::int16_t next = static_cast<std::int16_t>(mTrackPositions[track] + 1);
    const std::int16_t end = static_cast<std::int16_t>(
        EffectiveLength(lengthCode) * kSubstepCount);
    return next >= end || next >= static_cast<std::int16_t>(kPhysicalPositionCount);
}

void TriggerScheduler::advanceTrack(std::size_t track,
                                    std::uint8_t lengthCode) noexcept
{
    std::int16_t next = static_cast<std::int16_t>(mTrackPositions[track] + 1);
    const std::int16_t end = static_cast<std::int16_t>(
        EffectiveLength(lengthCode) * kSubstepCount);
    if (next >= end || next >= static_cast<std::int16_t>(kPhysicalPositionCount))
        next = 0;
    mTrackPositions[track] = next;
}

void TriggerScheduler::preparePhysicalBoundary() noexcept
{
    ++mMasterStepCounter;
    const std::uint8_t track0Length = mCachedLengthCodes[mActivePattern][0];
    if (!trackWillWrap(0u, track0Length))
        return;

    ++mBarCounter;
    const bool manual = mManualPatternPending.exchange(
        false, std::memory_order_acq_rel);
    std::uint8_t pending = mPendingPattern.load(std::memory_order_acquire);
    if (mActivePattern == pending)
    {
        const std::uint16_t period = static_cast<std::uint16_t>(
            mCachedRepeats[mActivePattern]) + 1u;
        pending = (static_cast<std::uint16_t>(mBarCounter) % period) == 0u
            ? mCachedNextPatterns[mActivePattern] : mActivePattern;
        if (pending >= kPatternCount)
        {
            const std::uint8_t limit = static_cast<std::uint8_t>(
                pending - static_cast<std::uint8_t>(kPatternCount) + 2u);
            pending = static_cast<std::uint8_t>(mRng.next() % limit);
            incrementSaturating(mDiagnostics.randomPatternSelections);
        }
        mPendingPattern.store(pending, std::memory_order_release);
    }

    if (mActivePattern != pending || manual)
    {
        mActivePattern = pending;
        mPublishedActivePattern.store(pending, std::memory_order_release);
        initializeTrackPositions(pending);
        incrementSaturating(mDiagnostics.patternTransitions);
    }
}

void TriggerScheduler::advanceWithoutSnapshot() noexcept
{
    if (!mCacheValid)
        return;
    preparePhysicalBoundary();
    for (std::size_t track = 0; track < kTrackCount; ++track)
    {
        mTrackLengthCodes[track] = mCachedLengthCodes[mActivePattern][track];
        advanceTrack(track, mTrackLengthCodes[track]);
    }
}

void TriggerScheduler::appendTrigger(
    const ClockEvent& clockEvent,
    const PatternStateAuthority::ReadHandle& snapshot,
    std::size_t track, std::uint8_t position, const StepData& step,
    std::uint8_t velocity, std::uint8_t note, TriggerKind kind) noexcept
{
    const Track logicalTrack = static_cast<Track>(track);
    const TrackIdentity identity = IdentityForTrack(logicalTrack);
    TriggerEvent event {};
    event.nativeBoundary = clockEvent.quantizedNativeFrame;
    event.clockOrdinal = clockEvent.ordinal;
    event.snapshotGeneration = snapshot.generation();
    const std::uint64_t boundaryOrder =
        clockEvent.ordinal * kMaximumTriggerEventsPerNativeBlock;
    event.stableOrder = boundaryOrder + track +
        static_cast<std::uint8_t>(kind) * kTrackCount;
    event.pattern = mActivePattern;
    event.track = static_cast<std::uint8_t>(track);
    event.synthesisOwner = identity.synthesisOwner;
    event.articulation = identity.articulation;
    event.physicalPosition = position;
    event.mainStep = MainStepOf(position);
    event.substep = SubstepOf(position);
    event.note = note;
    event.velocity = velocity;
    event.kind = kind;
    if (!mLastBatch.push(event))
    {
        incrementSaturating(mDiagnostics.rejectedOverflowEvents);
        return;
    }
    generateAutomationEvents(clockEvent, snapshot, event, step);
}

void TriggerScheduler::generateEvents(
    const ClockEvent& clockEvent,
    const PatternStateAuthority::ReadHandle& snapshot) noexcept
{
    preparePhysicalBoundary();
    const PatternData& pattern = snapshot.bank().patterns[mActivePattern];
    const std::uint8_t muted = mMutedTracks.load(std::memory_order_acquire);
    const std::uint8_t rollState = mRollState.load(std::memory_order_acquire);
    const std::uint8_t rollPeriod = mRollPeriod.load(std::memory_order_acquire);

    for (std::size_t track = 0; track < kTrackCount; ++track)
    {
        mTrackLengthCodes[track] = pattern.lengthCodes[track];
        advanceTrack(track, mTrackLengthCodes[track]);
        const auto position = static_cast<std::uint8_t>(mTrackPositions[track]);
        const std::uint8_t mainStep = MainStepOf(position);
        const std::uint8_t substep = SubstepOf(position);
        const StepData& step = pattern.steps[track][position];

        const std::uint8_t trackMask = static_cast<std::uint8_t>(1u << track);
        if ((muted & trackMask) != 0u)
            incrementSaturating(mDiagnostics.mutedOrdinaryTracks);
        else if ((pattern.mainGates[track] &
                  (std::uint16_t{1u} << mainStep)) != 0u && IsStepActive(step))
        {
            const Track logicalTrack = static_cast<Track>(track);
            if (substep == 0u)
                (void) mProbability.boundary(logicalTrack, true, mRng);
            if (mProbability.passes(logicalTrack, step.probability))
            {
                appendTrigger(clockEvent, snapshot, track, position, step,
                              StepVelocity(step), step.note, TriggerKind::Step);
            }
        }

        if (rollPeriod != 0xffu && (rollState & trackMask) != 0u &&
            (position % rollPeriod) == 0u)
        {
            appendTrigger(clockEvent, snapshot, track, position, step,
                          kDefaultVelocity, step.note,
                          TriggerKind::PeriodicRoll);
        }
    }
}

void TriggerScheduler::generateOneShotRolls(
    const ClockEvent& clockEvent,
    const PatternStateAuthority::ReadHandle& snapshot,
    std::uint8_t pendingMask) noexcept
{
    const PatternData& pattern = snapshot.bank().patterns[mActivePattern];
    for (std::size_t track = 0; track < kTrackCount; ++track)
    {
        if ((pendingMask & static_cast<std::uint8_t>(1u << track)) == 0u)
            continue;
        const std::uint8_t position = mTrackPositions[track] >= 0
            ? static_cast<std::uint8_t>(mTrackPositions[track]) : 0u;
        const StepData& step = pattern.steps[track][position];
        appendTrigger(clockEvent, snapshot, track, position, step,
                      kDefaultVelocity, kDefaultNote,
                      TriggerKind::OneShotRoll);
    }
}

void TriggerScheduler::generateAutomationEvents(
    const ClockEvent& clockEvent,
    const PatternStateAuthority::ReadHandle& snapshot,
    const TriggerEvent& trigger,
    const StepData& step) noexcept
{
    const std::uint8_t selectors[2] = {step.parameter1, step.parameter2};
    const std::uint8_t values[2] = {step.value1, step.value2};
    for (std::uint8_t slot = 0; slot < 2u; ++slot)
    {
        const AutomationDecodeResult decoded =
            DecodeAutomationSlot(selectors[slot], values[slot]);
        if (!decoded)
        {
            incrementSaturating(mDiagnostics.rejectedAutomationEvents);
            continue;
        }
        AutomationEvent event {};
        event.nativeBoundary = clockEvent.quantizedNativeFrame;
        event.clockOrdinal = clockEvent.ordinal;
        event.snapshotGeneration = snapshot.generation();
        event.stableOrder = trigger.stableOrder * 2u + slot;
        event.triggerOrder = trigger.stableOrder;
        event.pattern = trigger.pattern;
        event.track = trigger.track;
        event.synthesisOwner = trigger.synthesisOwner;
        event.physicalPosition = trigger.physicalPosition;
        event.slot = slot;
        event.command = decoded.command;
        if (!mLastAutomationBatch.push(event))
            incrementSaturating(mDiagnostics.rejectedAutomationOverflowEvents);
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
    mLastAutomationBatch.clear();

    (void) mClock.setShuffle(mRequestedShuffle.load(std::memory_order_acquire));
    ClockEvent clockEvent {};
    std::size_t written = 0u;
    const std::uint64_t endFrame =
        nativeBoundary == std::numeric_limits<std::uint64_t>::max()
        ? nativeBoundary : nativeBoundary + 1u;
    const Status clockStatus =
        mClock.advanceTo(endFrame, &clockEvent, 1u, written);
    if (clockStatus == Status::NonMonotonicTime)
    {
        incrementSaturating(mDiagnostics.nonMonotonicBoundaries);
        return clockStatus;
    }
    if (clockStatus == Status::InsufficientCapacity)
        incrementSaturating(mDiagnostics.clockCapacityFaults);

    const std::uint8_t pendingOneShots =
        mPendingOneShotRolls.load(std::memory_order_acquire);
    if (written == 0u && pendingOneShots == 0u)
        return clockStatus;
    if (written != 0u)
        incrementSaturating(mDiagnostics.duePhysicalPositions);

    PatternStateAuthority::ReadHandle snapshot;
    const Status acquired = mPatterns.tryAcquire(snapshot, snapshotAttempts);
    if (acquired != Status::Ok)
    {
        if (written != 0u)
            advanceWithoutSnapshot();
        incrementSaturating(mDiagnostics.snapshotBusyBoundaries);
        return acquired;
    }

    cachePatternControls(snapshot.bank());
    if (written != 0u)
        generateEvents(clockEvent, snapshot);

    const std::uint8_t oneShots =
        mPendingOneShotRolls.exchange(0u, std::memory_order_acq_rel);
    if (oneShots != 0u)
    {
        ClockEvent rollClock = clockEvent;
        if (written == 0u)
        {
            rollClock.quantizedNativeFrame = nativeBoundary;
            rollClock.exactFrameFloor = nativeBoundary;
            rollClock.ordinal = mClock.nextOrdinal();
        }
        generateOneShotRolls(rollClock, snapshot, oneShots);
    }
    snapshot.release();

    for (const TriggerEvent& event : mLastBatch)
    {
        const AutomationSlotCommand* slots[2] = {nullptr, nullptr};
        std::uint8_t automationCount = 0u;
        for (const AutomationEvent& automation : mLastAutomationBatch)
        {
            if (automation.triggerOrder != event.stableOrder)
                continue;
            if (automation.slot < 2u)
                slots[automation.slot] = &automation.command;
            ++automationCount;
        }
        if (engine.applySequencerAutomationPair(event.track, slots[0], slots[1]))
        {
            for (std::uint8_t index = 0u; index < automationCount; ++index)
                incrementSaturating(mDiagnostics.emittedAutomationEvents);
        }
        else
            incrementSaturating(mDiagnostics.engineAutomationApplicationFailures);
        dispatch(event, engine);
        incrementSaturating(mDiagnostics.emittedEvents);
        if (event.kind != TriggerKind::Step)
            incrementSaturating(mDiagnostics.emittedRollEvents);
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
