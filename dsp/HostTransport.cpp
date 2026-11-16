#include "HostTransport.h"

#include "SequencerClock.h"
#include "SequencerTypes.h"

#include <cmath>
#include <limits>

namespace lxr::sequencer
{

namespace
{
constexpr std::uint64_t kMicro = UINT64_C(1000000);
constexpr std::uint64_t kSecondsPerMinute = 60u;
constexpr std::int64_t kDiscontinuitySamples = 2;

std::int64_t FloorDiv(std::int64_t value, std::int64_t divisor) noexcept
{
    std::int64_t quotient = value / divisor;
    const std::int64_t remainder = value % divisor;
    if (remainder < 0)
        --quotient;
    return quotient;
}
}

void HostTransportTimeline::reset() noexcept
{
    mCallbackAnchorQ32 = 0;
    mPreviousAnchorQ32 = 0;
    mPreviousEndQ32 = 0;
    mPreviousSamplePosition = 0;
    mNextTimelineOrdinal = 0;
    mEventOrdinal = 0u;
    mTempoMicroBpm = 0u;
    mPreviousTempoMicroBpm = 0u;
    mSampleRate = 0u;
    mPreviousSampleRate = 0u;
    mCallbackFrames = 0;
    mPreviousFrames = 0;
    mCallbackUsable = false;
    mRunning = false;
    mPreviousPlaying = false;
    mHavePrevious = false;
    mHaveTimeline = false;
    mPreviousSampleValid = false;
}

std::int64_t HostTransportTimeline::absolute(std::int64_t value) noexcept
{
    return value < 0 ? (value == std::numeric_limits<std::int64_t>::min()
        ? std::numeric_limits<std::int64_t>::max() : -value) : value;
}

bool HostTransportTimeline::ppqToQ32(double ppq, std::int64_t& value) noexcept
{
    if (!std::isfinite(ppq) ||
        ppq < -static_cast<double>(kMaximumAbsolutePpq) ||
        ppq > static_cast<double>(kMaximumAbsolutePpq))
        return false;
    const long double scaled = static_cast<long double>(ppq) *
        static_cast<long double>(kPpqOne);
    value = static_cast<std::int64_t>(
        scaled >= 0.0L ? std::floor(scaled + 0.5L)
                       : std::ceil(scaled - 0.5L));
    return true;
}

bool HostTransportTimeline::tempoToMicroBpm(double bpm,
                                             std::uint64_t& value) noexcept
{
    if (!std::isfinite(bpm) || bpm < static_cast<double>(kMinimumTempo) ||
        bpm > static_cast<double>(kMaximumTempo))
        return false;
    value = static_cast<std::uint64_t>(std::floor(bpm * kMicro + 0.5));
    return value >= kMicro && value <= UINT64_C(255000000);
}

bool HostTransportTimeline::sampleRateToInteger(double sampleRate,
                                                 std::uint64_t& value) noexcept
{
    if (!std::isfinite(sampleRate) || sampleRate < 1.0 || sampleRate > 768000.0)
        return false;
    const double rounded = std::floor(sampleRate + 0.5);
    if (std::abs(sampleRate - rounded) > 1.0e-6)
        return false;
    value = static_cast<std::uint64_t>(rounded);
    return value != 0u;
}

std::int64_t HostTransportTimeline::addSamples(
    std::int64_t ppqQ32, std::int64_t samples,
    std::uint64_t microBpm, std::uint64_t sampleRate) noexcept
{
    if (samples == 0 || microBpm == 0u || sampleRate == 0u)
        return ppqQ32;
    if (samples == std::numeric_limits<std::int64_t>::min())
        return std::numeric_limits<std::int64_t>::min();
    const bool negative = samples < 0;
    const std::uint64_t magnitude = static_cast<std::uint64_t>(
        negative ? -samples : samples);
    const std::uint64_t denominator =
        kSecondsPerMinute * kMicro * sampleRate;
    const std::uint64_t numerator = microBpm *
        static_cast<std::uint64_t>(kPpqOne);
    const std::uint64_t whole = numerator / denominator;
    const std::uint64_t remainder = numerator % denominator;
    if (magnitude > UINT64_C(1000000000) ||
        (remainder != 0u && magnitude >
            std::numeric_limits<std::uint64_t>::max() / remainder))
        return negative ? std::numeric_limits<std::int64_t>::min()
                        : std::numeric_limits<std::int64_t>::max();
    const std::uint64_t delta = magnitude * whole +
        (magnitude * remainder) / denominator;
    const std::uint64_t signedMaximum = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    if (delta > signedMaximum)
        return negative ? std::numeric_limits<std::int64_t>::min()
                        : std::numeric_limits<std::int64_t>::max();
    const std::int64_t signedDelta = static_cast<std::int64_t>(delta);
    if (!negative)
    {
        if (ppqQ32 > 0 && signedDelta >
            std::numeric_limits<std::int64_t>::max() - ppqQ32)
            return std::numeric_limits<std::int64_t>::max();
        return ppqQ32 + signedDelta;
    }
    if (ppqQ32 < 0 && signedDelta >
        ppqQ32 - std::numeric_limits<std::int64_t>::min())
        return std::numeric_limits<std::int64_t>::min();
    return ppqQ32 - signedDelta;
}

std::int64_t HostTransportTimeline::boundaryPpqQ32(
    std::int64_t ordinal, std::uint8_t shuffle) noexcept
{
    const std::int64_t phaseIndex = ((ordinal % 16) + 16) % 16;
    const std::int64_t numerator = ordinal * 1524 +
        static_cast<std::int64_t>(kShufflePhaseNumerators[
            static_cast<std::size_t>(phaseIndex)]) * shuffle;
    const std::int64_t whole = FloorDiv(numerator, kBoundaryDenominator);
    const std::int64_t remainder = numerator - whole * kBoundaryDenominator;
    return whole * kPpqOne + static_cast<std::int64_t>(
        (static_cast<std::uint64_t>(remainder) *
         static_cast<std::uint64_t>(kPpqOne)) /
        static_cast<std::uint64_t>(kBoundaryDenominator));
}

std::int64_t HostTransportTimeline::firstBoundaryAtOrAfter(
    std::int64_t ppqQ32, std::uint8_t shuffle) noexcept
{
    const std::int64_t estimate = ppqQ32 / (kPpqOne / 32);
    std::int64_t candidate = estimate - 8;
    for (unsigned i = 0; i < 32u; ++i, ++candidate)
        if (boundaryPpqQ32(candidate, shuffle) >= ppqQ32)
            return candidate;
    return candidate;
}

std::int64_t HostTransportTimeline::callbackPpqQ32(
    std::int32_t sampleOffset) const noexcept
{
    if (!mCallbackUsable || sampleOffset < 0 ||
        sampleOffset > mCallbackFrames)
        return mCallbackAnchorQ32;
    return addSamples(mCallbackAnchorQ32, sampleOffset,
                      mTempoMicroBpm, mSampleRate);
}

HostCallbackDecision HostTransportTimeline::beginCallback(
    const HostTransportContext& context, std::uint8_t shuffle) noexcept
{
    HostCallbackDecision result {};
    mCallbackUsable = false;
    mCallbackFrames = context.callbackFrames;
    if (!context.contextPresent ||
        (context.validFields & HostTransportState) == 0u ||
        context.callbackFrames < 0 || shuffle > kMaximumShuffle ||
        !sampleRateToInteger(context.sampleRate, mSampleRate))
    {
        result.transition = HostTransition::InvalidContext;
        result.playing = false;
        mPreviousPlaying = false;
        mRunning = false;
        return result;
    }

    std::uint64_t tempo = 0u;
    if ((context.validFields & HostTempo) != 0u &&
        tempoToMicroBpm(context.tempoBpm, tempo))
        mTempoMicroBpm = tempo;
    else if (mHavePrevious && mPreviousTempoMicroBpm != 0u)
    {
        mTempoMicroBpm = mPreviousTempoMicroBpm;
        result.usedTempoFallback = true;
    }
    else
    {
        result.transition = HostTransition::InvalidContext;
        mPreviousPlaying = context.playing;
        mRunning = false;
        return result;
    }

    std::int64_t anchor = 0;
    const bool ppqValid = (context.validFields & HostPpqPosition) != 0u &&
        ppqToQ32(context.projectPpqPosition, anchor);
    const bool sampleValid = (context.validFields & HostSamplePosition) != 0u;
    // Host sample time is signed64 and may be inconsistent with musical PPQ.
    // Do not evaluate an overflowing endpoint sum, even for a rejected seek.
    const bool expectedSampleRepresentable = mPreviousFrames >= 0 &&
        mPreviousSamplePosition <= std::numeric_limits<std::int64_t>::max() -
            static_cast<std::int64_t>(mPreviousFrames);
    const bool sampleContinuous = mHavePrevious && sampleValid &&
        mPreviousSampleValid && expectedSampleRepresentable &&
        context.projectSamplePosition == mPreviousSamplePosition + mPreviousFrames;
    if (!ppqValid)
    {
        if (!sampleContinuous)
        {
            result.transition = HostTransition::InvalidContext;
            mPreviousPlaying = context.playing;
            mRunning = false;
            return result;
        }
        anchor = mPreviousEndQ32;
        result.usedPpqFallback = true;
    }

    mCallbackAnchorQ32 = anchor;
    result.anchorPpqQ32 = anchor;
    result.usable = true;
    result.playing = context.playing;
    mCallbackUsable = true;

    const std::int64_t tolerance = addSamples(
        0, kDiscontinuitySamples, mPreviousTempoMicroBpm != 0u
            ? mPreviousTempoMicroBpm : mTempoMicroBpm,
        mPreviousSampleRate != 0u ? mPreviousSampleRate : mSampleRate) + 2;
    const bool ppqContinuous = mHavePrevious &&
        absolute(anchor - mPreviousEndQ32) <= tolerance;
    // A host can expose only one BPM per callback even when a tempo marker
    // lies inside it. On the next callback its authoritative PPQ integral is
    // then between the old- and new-tempo predictions. With continuous sample
    // time this is a tempo correction, not evidence of a seek. Retain the next
    // pending ordinal and RNG/chain history; do not replay elapsed positions.
    // Outside this bounded interval the existing discontinuity policy applies.
    const bool changedTempo = mHavePrevious &&
        mPreviousTempoMicroBpm != mTempoMicroBpm;
    const std::int64_t endAtNewTempo = addSamples(
        mPreviousAnchorQ32, mPreviousFrames, mTempoMicroBpm, mSampleRate);
    const std::int64_t minimumTempoEnd =
        endAtNewTempo < mPreviousEndQ32 ? endAtNewTempo : mPreviousEndQ32;
    const std::int64_t maximumTempoEnd =
        endAtNewTempo > mPreviousEndQ32 ? endAtNewTempo : mPreviousEndQ32;
    const bool boundedTempoEnds =
        minimumTempoEnd >= std::numeric_limits<std::int64_t>::min() + tolerance &&
        maximumTempoEnd <= std::numeric_limits<std::int64_t>::max() - tolerance;
    const bool tempoCorrection = sampleContinuous && changedTempo && boundedTempoEnds &&
        anchor >= minimumTempoEnd - tolerance &&
        anchor <= maximumTempoEnd + tolerance;
    const bool discontinuity = mHavePrevious && mPreviousPlaying &&
        ((!sampleContinuous && sampleValid && mPreviousSampleValid) ||
         (!ppqContinuous && !tempoCorrection));

    if (!context.playing)
    {
        const bool relocatedWhileStopped = mHavePrevious &&
            ((!sampleContinuous && sampleValid && mPreviousSampleValid) ||
             !ppqContinuous);
        if (relocatedWhileStopped)
        {
            result.transition = HostTransition::Seek;
            result.reanchor = true;
            mNextTimelineOrdinal = firstBoundaryAtOrAfter(anchor, shuffle);
            mEventOrdinal = 0u;
            mHaveTimeline = true;
        }
        else
            result.transition = mPreviousPlaying ? HostTransition::Stop
                                                 : HostTransition::None;
        mRunning = false;
    }
    else if (!mPreviousPlaying)
    {
        const bool sameStoppedPosition = mHavePrevious &&
            absolute(anchor - mPreviousAnchorQ32) <= tolerance;
        if (mHaveTimeline && sameStoppedPosition)
        {
            result.transition = HostTransition::Resume;
            mRunning = true;
        }
        else
        {
            result.transition = mHaveTimeline ? HostTransition::Seek
                                              : HostTransition::Start;
            result.reanchor = true;
            mNextTimelineOrdinal = firstBoundaryAtOrAfter(anchor, shuffle);
            mEventOrdinal = 0u;
            mHaveTimeline = true;
            mRunning = true;
        }
    }
    else if (discontinuity)
    {
        std::int64_t cycleStart = 0;
        std::int64_t cycleEnd = 0;
        const bool cycleValid = context.cycleActive &&
            (context.validFields & HostCycle) != 0u &&
            ppqToQ32(context.cycleStartPpq, cycleStart) &&
            ppqToQ32(context.cycleEndPpq, cycleEnd) && cycleEnd > cycleStart;
        const bool wrapped = cycleValid && anchor >= cycleStart - tolerance &&
            anchor <= cycleStart + tolerance &&
            mPreviousEndQ32 >= cycleEnd - tolerance;
        result.transition = wrapped ? HostTransition::Loop : HostTransition::Seek;
        result.reanchor = true;
        mNextTimelineOrdinal = firstBoundaryAtOrAfter(anchor, shuffle);
        mEventOrdinal = 0u;
        mHaveTimeline = true;
        mRunning = true;
    }
    else
    {
        result.transition = mHavePrevious &&
            mPreviousTempoMicroBpm != mTempoMicroBpm
            ? HostTransition::TempoChange : HostTransition::None;
        mRunning = true;
    }

    result.nextTimelineOrdinal = mNextTimelineOrdinal;
    mPreviousAnchorQ32 = anchor;
    mPreviousEndQ32 = addSamples(anchor, context.playing
        ? context.callbackFrames : 0, mTempoMicroBpm, mSampleRate);
    mPreviousSamplePosition = context.projectSamplePosition;
    mPreviousFrames = context.playing ? context.callbackFrames : 0;
    mPreviousTempoMicroBpm = mTempoMicroBpm;
    mPreviousSampleRate = mSampleRate;
    mPreviousSampleValid = sampleValid;
    mPreviousPlaying = context.playing;
    mHavePrevious = true;
    return result;
}

bool HostTransportTimeline::popDueBoundary(
    std::int32_t hostSampleOffset, std::uint8_t shuffle,
    std::int64_t& timelineOrdinal, std::uint64_t& eventOrdinal) noexcept
{
    if (!running() || hostSampleOffset < 0 ||
        hostSampleOffset >= mCallbackFrames || shuffle > kMaximumShuffle)
        return false;
    const std::int64_t now = callbackPpqQ32(hostSampleOffset);
    // A valid VST3 PPQ anchor arrives as double and is rounded once to Q32.32.
    // Admit its maximum one-LSB conversion error without moving an event by a
    // host sample or accumulating error between callbacks.
    if (boundaryPpqQ32(mNextTimelineOrdinal, shuffle) > now + 1)
        return false;
    timelineOrdinal = mNextTimelineOrdinal++;
    eventOrdinal = mEventOrdinal++;
    return true;
}

} // namespace lxr::sequencer
