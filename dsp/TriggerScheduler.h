#pragma once

#include "PatternStateAuthority.h"
#include "SequencerClock.h"
#include "SequencerParameterLocks.h"
#include "SequencerRng.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace lxr
{
class Engine;

namespace sequencer
{

// A control-thread one-shot can be quantized to the same native boundary as
// seven ordinary and seven periodic-roll triggers: 7 * 3 is the hard bound.
inline constexpr std::size_t kMaximumTriggerEventsPerNativeBlock = kTrackCount * 3u;
inline constexpr std::size_t kMaximumAutomationEventsPerNativeBlock =
    kMaximumTriggerEventsPerNativeBlock * 2u;

enum class TriggerKind : std::uint8_t
{
    Step = 0,
    PeriodicRoll = 1,
    OneShotRoll = 2
};

struct AutomationEvent
{
    std::uint64_t nativeBoundary = 0;
    std::uint64_t clockOrdinal = 0;
    std::uint64_t snapshotGeneration = 0;
    std::uint64_t stableOrder = 0;
    std::uint64_t triggerOrder = 0;
    std::uint8_t pattern = 0;
    std::uint8_t track = 0;
    std::uint8_t synthesisOwner = 0;
    std::uint8_t physicalPosition = 0;
    std::uint8_t slot = 0;
    AutomationSlotCommand command {};
};

class AutomationEventBatch final
{
public:
    static constexpr std::size_t capacity() noexcept
    {
        return kMaximumAutomationEventsPerNativeBlock;
    }
    void clear() noexcept { mSize=0; mRejected=0; }
    bool push(const AutomationEvent& event) noexcept
    {
        if(mSize>=mEvents.size()) {
            if(mRejected!=std::numeric_limits<std::uint64_t>::max()) ++mRejected;
            return false;
        }
        mEvents[mSize++]=event;
        return true;
    }
    std::size_t size() const noexcept {return mSize;}
    bool empty() const noexcept {return mSize==0;}
    std::uint64_t rejected() const noexcept {return mRejected;}
    const AutomationEvent& operator[](std::size_t index) const noexcept
    {return mEvents[index];}
    const AutomationEvent* begin() const noexcept {return mEvents.data();}
    const AutomationEvent* end() const noexcept {return mEvents.data()+mSize;}
private:
    std::array<AutomationEvent,kMaximumAutomationEventsPerNativeBlock> mEvents {};
    std::size_t mSize=0;
    std::uint64_t mRejected=0;
};

struct TriggerEvent
{
    std::uint64_t nativeBoundary = 0;
    std::uint64_t clockOrdinal = 0;
    std::uint64_t snapshotGeneration = 0;
    std::uint64_t stableOrder = 0;
    std::uint8_t pattern = 0;
    std::uint8_t track = 0;
    std::uint8_t synthesisOwner = 0;
    Articulation articulation = Articulation::Standard;
    std::uint8_t physicalPosition = 0;
    std::uint8_t mainStep = 0;
    std::uint8_t substep = 0;
    std::uint8_t note = 0;
    std::uint8_t velocity = 0;
    TriggerKind kind = TriggerKind::Step;
};

class TriggerEventBatch final
{
public:
    static constexpr std::size_t capacity() noexcept
    {
        return kMaximumTriggerEventsPerNativeBlock;
    }

    void clear() noexcept
    {
        mSize = 0;
        mRejected = 0;
    }

    bool push(const TriggerEvent& event) noexcept
    {
        if (mSize >= mEvents.size())
        {
            if (mRejected != std::numeric_limits<std::uint64_t>::max())
                ++mRejected;
            return false;
        }
        mEvents[mSize++] = event;
        return true;
    }

    std::size_t size() const noexcept { return mSize; }
    bool empty() const noexcept { return mSize == 0; }
    std::uint64_t rejected() const noexcept { return mRejected; }
    const TriggerEvent& operator[](std::size_t index) const noexcept
    {
        return mEvents[index]; // Precondition: index < size().
    }
    const TriggerEvent* begin() const noexcept { return mEvents.data(); }
    const TriggerEvent* end() const noexcept { return mEvents.data() + mSize; }

private:
    std::array<TriggerEvent, kMaximumTriggerEventsPerNativeBlock> mEvents {};
    std::size_t mSize = 0;
    std::uint64_t mRejected = 0;
};

struct TriggerSchedulerDiagnostics
{
    std::uint64_t processedNativeBlocks = 0;
    std::uint64_t duePhysicalPositions = 0;
    std::uint64_t emittedEvents = 0;
    std::uint64_t rejectedOverflowEvents = 0;
    std::uint64_t emittedAutomationEvents = 0;
    std::uint64_t rejectedAutomationEvents = 0;
    std::uint64_t rejectedAutomationOverflowEvents = 0;
    std::uint64_t engineAutomationApplicationFailures = 0;
    std::uint64_t snapshotBusyBoundaries = 0;
    std::uint64_t clockCapacityFaults = 0;
    std::uint64_t nonMonotonicBoundaries = 0;
    std::uint64_t patternTransitions = 0;
    std::uint64_t randomPatternSelections = 0;
    std::uint64_t mutedOrdinaryTracks = 0;
    std::uint64_t emittedRollEvents = 0;
};

class TriggerScheduler final
{
public:
    explicit TriggerScheduler(std::uint32_t seed = 0x9e3779b9u) noexcept;
    TriggerScheduler(const TriggerScheduler&) = delete;
    TriggerScheduler& operator=(const TriggerScheduler&) = delete;

    PatternStateAuthority& patternAuthority() noexcept { return mPatterns; }
    const PatternStateAuthority& patternAuthority() const noexcept { return mPatterns; }

    Status setTempo(std::uint16_t bpm) noexcept;
    std::uint16_t tempo() const noexcept { return mClock.tempo(); }
    Status selectPattern(std::uint8_t pattern) noexcept;
    std::uint8_t selectedPattern() const noexcept
    { return mPublishedActivePattern.load(std::memory_order_acquire); }
    std::uint8_t pendingPattern() const noexcept
    { return mPendingPattern.load(std::memory_order_acquire); }
    std::uint8_t barCounter() const noexcept { return mBarCounter; }

    Status setShuffle(std::uint8_t shuffle) noexcept;
    std::uint8_t shuffle() const noexcept
    { return mRequestedShuffle.load(std::memory_order_acquire); }
    Status setTrackMuted(std::uint8_t track, bool muted) noexcept;
    bool trackMuted(std::uint8_t track) const noexcept;
    void unmuteAll() noexcept { mMutedTracks.store(0u, std::memory_order_release); }
    Status setRollRate(std::uint8_t selector) noexcept;
    std::uint8_t rollRateSelector() const noexcept
    { return mRollRateSelector.load(std::memory_order_acquire); }
    Status setRoll(std::uint8_t track, bool active) noexcept;

    Status start() noexcept;
    void stop() noexcept;
    void reset(std::uint64_t nativeFrame = 0) noexcept;
    bool running() const noexcept { return mClock.running(); }

    // Called exactly once immediately before the Engine block beginning at
    // nativeBoundary. snapshotAttempts is exposed only to make the bounded Busy
    // policy deterministic in standalone tests; production uses the default 8.
    Status processNativeBlock(
        std::uint64_t nativeBoundary,
        Engine& engine,
        unsigned snapshotAttempts = PatternStateAuthority::kDefaultAcquireAttempts) noexcept;

    const TriggerEventBatch& lastBatch() const noexcept { return mLastBatch; }
    const AutomationEventBatch& lastAutomationBatch() const noexcept
    { return mLastAutomationBatch; }
    const TriggerSchedulerDiagnostics& diagnostics() const noexcept
    {
        return mDiagnostics;
    }
    std::uint32_t rngState() const noexcept { return mRng.state(); }
    std::uint8_t probabilityLatch(std::uint8_t track) const noexcept;
    std::uint8_t trackPosition(std::uint8_t track) const noexcept;

private:
    static void incrementSaturating(std::uint64_t& value) noexcept;
    Status initializeTrackPositions() noexcept;
    void initializeTrackPositions(std::uint8_t pattern) noexcept;
    void advanceTrack(std::size_t track, std::uint8_t lengthCode) noexcept;
    bool trackWillWrap(std::size_t track, std::uint8_t lengthCode) const noexcept;
    void cachePatternControls(const PatternBank& bank) noexcept;
    void preparePhysicalBoundary() noexcept;
    void advanceWithoutSnapshot() noexcept;
    void generateEvents(const ClockEvent& clockEvent,
                        const PatternStateAuthority::ReadHandle& snapshot) noexcept;
    void generateOneShotRolls(
        const ClockEvent& clockEvent,
        const PatternStateAuthority::ReadHandle& snapshot,
        std::uint8_t pendingMask) noexcept;
    void appendTrigger(const ClockEvent& clockEvent,
                       const PatternStateAuthority::ReadHandle& snapshot,
                       std::size_t track, std::uint8_t position,
                       const StepData& step, std::uint8_t velocity,
                       std::uint8_t note, TriggerKind kind) noexcept;
    void generateAutomationEvents(const ClockEvent& clockEvent,
                                  const PatternStateAuthority::ReadHandle& snapshot,
                                  const TriggerEvent& trigger,
                                  const StepData& step) noexcept;
    static void dispatch(const TriggerEvent& event, Engine& engine) noexcept;

    PatternStateAuthority mPatterns {};
    SequencerClock mClock {};
    DeterministicRng mRng {};
    ClusterProbability mProbability {};
    TriggerEventBatch mLastBatch {};
    AutomationEventBatch mLastAutomationBatch {};
    TriggerSchedulerDiagnostics mDiagnostics {};
    std::array<std::int16_t, kTrackCount> mTrackPositions {};
    std::array<std::uint8_t, kTrackCount> mTrackLengthCodes {};
    std::array<std::array<std::uint8_t, kTrackCount>, kPatternCount>
        mCachedLengthCodes {};
    std::array<std::array<std::uint8_t, kTrackCount>, kPatternCount>
        mCachedRotations {};
    std::array<std::uint8_t, kPatternCount> mCachedRepeats {};
    std::array<std::uint8_t, kPatternCount> mCachedNextPatterns {};
    std::array<std::uint8_t, kPatternCount> mZeroRotationMasks {};
    std::atomic<std::uint8_t> mPublishedActivePattern {kInitialPattern};
    std::atomic<std::uint8_t> mPendingPattern {kInitialPattern};
    std::atomic<bool> mManualPatternPending {false};
    std::atomic<std::uint8_t> mRequestedShuffle {0u};
    std::atomic<std::uint8_t> mMutedTracks {0u};
    std::atomic<std::uint8_t> mRollState {0u};
    std::atomic<std::uint8_t> mPendingOneShotRolls {0u};
    std::atomic<std::uint8_t> mRollPeriod {8u};
    std::atomic<std::uint8_t> mRollRateSelector {8u};
    std::uint8_t mActivePattern = kInitialPattern;
    std::uint8_t mBarCounter = 0u;
    std::uint8_t mMasterStepCounter = 0u;
    std::uint8_t mCachedBankShuffle = 0u;
    bool mCacheValid = false;
};

} // namespace sequencer
} // namespace lxr
