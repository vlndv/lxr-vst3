#pragma once

#include "PatternStateAuthority.h"
#include "SequencerClock.h"
#include "SequencerParameterLocks.h"
#include "SequencerRng.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace lxr
{
class Engine;

namespace sequencer
{

inline constexpr std::size_t kMaximumTriggerEventsPerNativeBlock = kTrackCount;
inline constexpr std::size_t kMaximumAutomationEventsPerNativeBlock =
    kTrackCount * 2u;

struct AutomationEvent
{
    std::uint64_t nativeBoundary = 0;
    std::uint64_t clockOrdinal = 0;
    std::uint64_t snapshotGeneration = 0;
    std::uint64_t stableOrder = 0;
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
    std::uint8_t selectedPattern() const noexcept { return mSelectedPattern; }

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
    void advanceTrack(std::size_t track, std::uint8_t lengthCode) noexcept;
    void generateEvents(const ClockEvent& clockEvent,
                        const PatternStateAuthority::ReadHandle& snapshot) noexcept;
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
    std::uint8_t mSelectedPattern = kInitialPattern;
};

} // namespace sequencer
} // namespace lxr
