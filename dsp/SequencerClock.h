#pragma once

#include "SequencerTypes.h"

#include <cstddef>
#include <cstdint>

namespace lxr::sequencer
{

// Engine float rate 44002.7578125 Hz expressed exactly as a rational.
inline constexpr std::uint64_t kNativeRateNumerator = 5632353u;
inline constexpr std::uint64_t kNativeRateDenominator = 128u;
inline constexpr std::uint64_t kNativeBlockFrames = 32u;
inline constexpr std::uint64_t kClockStepNumerator =
    kNativeRateNumerator * 60u;
inline constexpr std::uint64_t kClockDenominatorScale =
    kNativeRateDenominator * 32u;

struct ClockEvent
{
    std::uint64_t exactFrameFloor = 0;
    std::uint64_t quantizedNativeFrame = 0;
    std::uint64_t ordinal = 0;
    std::uint8_t physicalPosition = 0;
};

class SequencerClock final
{
public:
    SequencerClock() noexcept;

    Status setTempo(std::uint16_t bpm) noexcept;
    std::uint16_t tempo() const noexcept { return mTempo; }

    void reset(std::uint64_t nativeFrame = 0) noexcept;
    void start() noexcept;
    void stop() noexcept;
    void resume() noexcept { start(); }
    bool running() const noexcept { return mRunning; }

    // Emits every event whose next-native-block timestamp is before endFrame.
    // Capacity exhaustion is resumable: emitted events remain consumed and a
    // second call with the same endFrame continues at the first un-emitted one.
    Status advanceTo(std::uint64_t endFrame, ClockEvent* events,
                     std::size_t capacity, std::size_t& written) noexcept;

    std::uint64_t cursor() const noexcept { return mCursor; }
    std::uint64_t nextExactFrameFloor() const noexcept { return mNextWhole; }
    std::uint64_t nextQuantizedFrame() const noexcept;
    std::uint64_t nextOrdinal() const noexcept { return mOrdinal; }

private:
    void configureStep(std::uint16_t bpm) noexcept;
    void advanceExact() noexcept;
    void reanchorNextFromLast() noexcept;

    std::uint64_t mCursor = 0;
    std::uint64_t mNextWhole = 0;
    std::uint64_t mNextFraction = 0;
    std::uint64_t mLastWhole = 0;
    std::uint64_t mLastFraction = 0;
    std::uint64_t mLastDenominator = kClockDenominatorScale * kDefaultTempo;
    std::uint64_t mStepWhole = 0;
    std::uint64_t mStepRemainder = 0;
    std::uint64_t mStepDenominator = kClockDenominatorScale * kDefaultTempo;
    std::uint64_t mOrdinal = 0;
    std::uint16_t mTempo = kDefaultTempo;
    bool mRunning = false;
    bool mHasEmitted = false;
};

} // namespace lxr::sequencer
