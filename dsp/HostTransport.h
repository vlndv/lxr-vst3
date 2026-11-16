#pragma once

#include <cstdint>

namespace lxr::sequencer
{

// VST3 host synchronization is an explicit plug-in adaptation. Values enter
// this type only after their native validity flags have been checked.
enum HostContextField : std::uint32_t
{
    HostSamplePosition = 1u << 0u,
    HostPpqPosition = 1u << 1u,
    HostTempo = 1u << 2u,
    HostCycle = 1u << 3u,
    HostTimeSignature = 1u << 4u,
    HostBarPosition = 1u << 5u,
    HostTransportState = 1u << 6u
};

struct HostTransportContext
{
    std::uint32_t validFields = 0u;
    std::int64_t projectSamplePosition = 0;
    double projectPpqPosition = 0.0;
    double tempoBpm = 0.0;
    double cycleStartPpq = 0.0;
    double cycleEndPpq = 0.0;
    double barPositionPpq = 0.0;
    double sampleRate = 0.0;
    std::int32_t timeSigNumerator = 0;
    std::int32_t timeSigDenominator = 0;
    std::int32_t callbackFrames = 0;
    bool contextPresent = false;
    bool playing = false;
    bool cycleActive = false;
    bool offline = false;
};

enum class HostTransition : std::uint8_t
{
    None = 0,
    Start,
    Resume,
    Stop,
    Seek,
    Loop,
    TempoChange,
    InvalidContext
};

struct HostCallbackDecision
{
    HostTransition transition = HostTransition::None;
    bool usable = false;
    bool playing = false;
    bool reanchor = false;
    bool usedPpqFallback = false;
    bool usedTempoFallback = false;
    std::int64_t anchorPpqQ32 = 0;
    std::int64_t nextTimelineOrdinal = 0;
};

class HostTransportTimeline final
{
public:
    static constexpr std::int64_t kPpqOne = INT64_C(1) << 32u;
    static constexpr std::int64_t kMaximumAbsolutePpq = INT64_C(1000000);
    static constexpr std::int64_t kBoundaryDenominator = 32 * 1524;

    void reset() noexcept;
    HostCallbackDecision beginCallback(const HostTransportContext& context,
                                       std::uint8_t shuffle) noexcept;

    // Called at the host sample which caused the native block to be rendered.
    // At most one physical boundary is returned; a shuffle catch-up remains
    // pending for the following native block.
    bool popDueBoundary(std::int32_t hostSampleOffset,
                        std::uint8_t shuffle,
                        std::int64_t& timelineOrdinal,
                        std::uint64_t& eventOrdinal) noexcept;

    bool running() const noexcept { return mRunning && mCallbackUsable; }
    bool callbackUsable() const noexcept { return mCallbackUsable; }
    std::int64_t nextTimelineOrdinal() const noexcept { return mNextTimelineOrdinal; }
    std::uint64_t nextEventOrdinal() const noexcept { return mEventOrdinal; }
    std::int64_t callbackPpqQ32(std::int32_t sampleOffset) const noexcept;

    static bool ppqToQ32(double ppq, std::int64_t& value) noexcept;
    static std::int64_t boundaryPpqQ32(std::int64_t ordinal,
                                      std::uint8_t shuffle) noexcept;
    static std::int64_t firstBoundaryAtOrAfter(std::int64_t ppqQ32,
                                              std::uint8_t shuffle) noexcept;

private:
    static bool tempoToMicroBpm(double bpm, std::uint64_t& value) noexcept;
    static bool sampleRateToInteger(double sampleRate, std::uint64_t& value) noexcept;
    static std::int64_t addSamples(std::int64_t ppqQ32,
                                   std::int64_t samples,
                                   std::uint64_t microBpm,
                                   std::uint64_t sampleRate) noexcept;
    static std::int64_t absolute(std::int64_t value) noexcept;

    std::int64_t mCallbackAnchorQ32 = 0;
    std::int64_t mPreviousAnchorQ32 = 0;
    std::int64_t mPreviousEndQ32 = 0;
    std::int64_t mPreviousSamplePosition = 0;
    std::int64_t mNextTimelineOrdinal = 0;
    std::uint64_t mEventOrdinal = 0u;
    std::uint64_t mTempoMicroBpm = 0u;
    std::uint64_t mPreviousTempoMicroBpm = 0u;
    std::uint64_t mSampleRate = 0u;
    std::uint64_t mPreviousSampleRate = 0u;
    std::int32_t mCallbackFrames = 0;
    std::int32_t mPreviousFrames = 0;
    bool mCallbackUsable = false;
    bool mRunning = false;
    bool mPreviousPlaying = false;
    bool mHavePrevious = false;
    bool mHaveTimeline = false;
    bool mPreviousSampleValid = false;
};

} // namespace lxr::sequencer
