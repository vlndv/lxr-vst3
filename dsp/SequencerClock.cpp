#include "SequencerClock.h"

#include <limits>

namespace lxr::sequencer
{

SequencerClock::SequencerClock() noexcept
{
    configureStep(kDefaultTempo);
    reset(0u);
}

void SequencerClock::configureStep(std::uint16_t bpm) noexcept
{
    mTempo = bpm;
    mStepDenominator = kClockDenominatorScale * bpm;
    mStepWhole = kClockStepNumerator / mStepDenominator;
    mStepRemainder = kClockStepNumerator % mStepDenominator;
}

Status SequencerClock::setTempo(std::uint16_t bpm) noexcept
{
    if (bpm < kMinimumTempo || bpm > kMaximumTempo)
        return Status::InvalidTempo;
    if (bpm == mTempo)
        return Status::Ok;

    configureStep(bpm);
    if (mRunning && mHasEmitted)
    {
        reanchorNextFromLast();
        advanceExact();
    }
    return Status::Ok;
}

void SequencerClock::reset(std::uint64_t nativeFrame) noexcept
{
    mCursor = nativeFrame;
    mNextWhole = nativeFrame;
    mNextFraction = 0u;
    mLastWhole = nativeFrame;
    mLastFraction = 0u;
    mLastDenominator = mStepDenominator;
    mOrdinal = 0u;
    mRunning = false;
    mHasEmitted = false;
}

void SequencerClock::start() noexcept
{
    if (mRunning)
        return;
    // Source stop/start semantics restart position zero immediately at the
    // current time; this is not a pause that preserves the prior phase.
    mNextWhole = mCursor;
    mNextFraction = 0u;
    mLastWhole = mCursor;
    mLastFraction = 0u;
    mLastDenominator = mStepDenominator;
    mOrdinal = 0u;
    mHasEmitted = false;
    mRunning = true;
}

void SequencerClock::stop() noexcept
{
    mRunning = false;
    mHasEmitted = false;
    mOrdinal = 0u;
}

void SequencerClock::advanceExact() noexcept
{
    const std::uint64_t oldWhole = mNextWhole;
    mNextWhole += mStepWhole;
    mNextFraction += mStepRemainder;
    if (mNextFraction >= mStepDenominator)
    {
        ++mNextWhole;
        mNextFraction -= mStepDenominator;
    }
    if (mNextWhole < oldWhole)
    {
        mNextWhole = std::numeric_limits<std::uint64_t>::max();
        mNextFraction = 0u;
    }
}

void SequencerClock::reanchorNextFromLast() noexcept
{
    mNextWhole = mLastWhole;
    // Denominators are at most 1,044,480, so this product is bounded.
    mNextFraction =
        (mLastFraction * mStepDenominator) / mLastDenominator;
}

std::uint64_t SequencerClock::nextQuantizedFrame() const noexcept
{
    std::uint64_t ceiling = mNextWhole;
    if (mNextFraction != 0u && ceiling != std::numeric_limits<std::uint64_t>::max())
        ++ceiling;
    const std::uint64_t remainder = ceiling % kNativeBlockFrames;
    if (remainder == 0u)
        return ceiling;
    const std::uint64_t add = kNativeBlockFrames - remainder;
    return ceiling > std::numeric_limits<std::uint64_t>::max() - add
        ? std::numeric_limits<std::uint64_t>::max()
        : ceiling + add;
}

Status SequencerClock::advanceTo(std::uint64_t endFrame, ClockEvent* events,
                                 std::size_t capacity,
                                 std::size_t& written) noexcept
{
    written = 0u;
    if (endFrame < mCursor)
        return Status::NonMonotonicTime;
    if (!mRunning)
    {
        mCursor = endFrame;
        return Status::Ok;
    }

    while (written < capacity && nextQuantizedFrame() < endFrame)
    {
        if (events == nullptr)
        {
            mCursor = nextQuantizedFrame();
            return Status::InsufficientCapacity;
        }

        ClockEvent& event = events[written++];
        event.exactFrameFloor = mNextWhole;
        event.quantizedNativeFrame = nextQuantizedFrame();
        event.ordinal = mOrdinal;
        event.physicalPosition = static_cast<std::uint8_t>(
            mOrdinal % kPhysicalPositionCount);

        mLastWhole = mNextWhole;
        mLastFraction = mNextFraction;
        mLastDenominator = mStepDenominator;
        mHasEmitted = true;
        ++mOrdinal;
        advanceExact();
    }
    if (nextQuantizedFrame() < endFrame)
    {
        mCursor = nextQuantizedFrame();
        return Status::InsufficientCapacity;
    }
    mCursor = endFrame;
    return Status::Ok;
}

} // namespace lxr::sequencer
