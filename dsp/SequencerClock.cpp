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

Status SequencerClock::setShuffle(std::uint8_t shuffle) noexcept
{
    if (shuffle > kMaximumShuffle)
        return Status::InvalidShuffle;
    mShuffle = shuffle;
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
    mLastQuantized = nativeFrame;
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
    mLastQuantized = mCursor;
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
    std::uint64_t adjustedWhole = 0u;
    std::uint64_t adjustedFraction = 0u;
    std::uint64_t adjustedDenominator = 1u;
    nextAdjusted(adjustedWhole, adjustedFraction, adjustedDenominator);
    std::uint64_t ceiling = adjustedWhole;
    if (adjustedFraction != 0u &&
        ceiling != std::numeric_limits<std::uint64_t>::max())
        ++ceiling;
    const std::uint64_t remainder = ceiling % kNativeBlockFrames;
    const std::uint64_t add = remainder == 0u
        ? 0u : kNativeBlockFrames - remainder;
    std::uint64_t quantized =
        ceiling > std::numeric_limits<std::uint64_t>::max() - add
        ? std::numeric_limits<std::uint64_t>::max()
        : ceiling + add;
    // The source timer processes at most one physical position per poll even
    // when the descending shuffle phase produces a negative nominal delta.
    // The VST adaptation therefore retains P16-C's one-event-per-native-block
    // contract and catches back up when the ideal phase moves ahead again.
    if (mHasEmitted &&
        mLastQuantized <= std::numeric_limits<std::uint64_t>::max() - kNativeBlockFrames)
    {
        const std::uint64_t earliest = mLastQuantized + kNativeBlockFrames;
        if (quantized < earliest)
            quantized = earliest;
    }
    // A live shuffle decrease can move the ideal next point behind the block
    // cursor. Dispatch it at the next available native boundary, never with a
    // timestamp in the already-rendered past.
    const std::uint64_t cursorRemainder = mCursor % kNativeBlockFrames;
    const std::uint64_t cursorAdd = cursorRemainder == 0u
        ? 0u : kNativeBlockFrames - cursorRemainder;
    const std::uint64_t cursorBoundary =
        mCursor > std::numeric_limits<std::uint64_t>::max() - cursorAdd
        ? std::numeric_limits<std::uint64_t>::max() : mCursor + cursorAdd;
    if (quantized < cursorBoundary)
        quantized = cursorBoundary;
    return quantized;
}

void SequencerClock::nextAdjusted(std::uint64_t& whole,
                                  std::uint64_t& fraction,
                                  std::uint64_t& denominator) const noexcept
{
    denominator = mStepDenominator * kShuffleOffsetDenominator;
    const std::uint8_t phase = kShufflePhaseNumerators[
        static_cast<std::size_t>(mOrdinal & 0x0fu)];
    const std::uint64_t offsetNumerator =
        kClockStepNumerator * phase * mShuffle;
    const std::uint64_t combined =
        mNextFraction * kShuffleOffsetDenominator + offsetNumerator;
    whole = mNextWhole + combined / denominator;
    fraction = combined % denominator;
    if (whole < mNextWhole)
    {
        whole = std::numeric_limits<std::uint64_t>::max();
        fraction = 0u;
    }
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
        std::uint64_t adjustedFraction = 0u;
        std::uint64_t adjustedDenominator = 1u;
        nextAdjusted(event.exactFrameFloor, adjustedFraction,
                     adjustedDenominator);
        event.quantizedNativeFrame = nextQuantizedFrame();
        event.ordinal = mOrdinal;
        event.physicalPosition = static_cast<std::uint8_t>(
            mOrdinal % kPhysicalPositionCount);

        mLastWhole = mNextWhole;
        mLastFraction = mNextFraction;
        mLastDenominator = mStepDenominator;
        mHasEmitted = true;
        mLastQuantized = event.quantizedNativeFrame;
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
