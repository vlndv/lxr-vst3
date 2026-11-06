#pragma once

#include "SequencerTypes.h"

#include <array>
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
inline constexpr std::uint64_t kShuffleOffsetDenominator = 1524u;
inline constexpr std::array<std::uint8_t, 16> kShufflePhaseNumerators {{
    0u, 1u, 4u, 9u, 16u, 25u, 36u, 49u,
    64u, 63u, 60u, 55u, 48u, 39u, 28u, 15u
}};

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
    Status setShuffle(std::uint8_t shuffle) noexcept;
    std::uint8_t shuffle() const noexcept { return mShuffle; }

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
    void nextAdjusted(std::uint64_t& whole, std::uint64_t& fraction,
                      std::uint64_t& denominator) const noexcept;

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
    std::uint64_t mLastQuantized = 0;
    std::uint16_t mTempo = kDefaultTempo;
    std::uint8_t mShuffle = 0;
    bool mRunning = false;
    bool mHasEmitted = false;
};

} // namespace lxr::sequencer
