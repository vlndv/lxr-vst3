#pragma once

#if defined(LXR_P20D_DIAGNOSTIC_CAPTURE)

#include "TriggerScheduler.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace lxr::diagnostic
{

// Test-only exact-DLL observation. This type is compiled out of ordinary
// production candidates and never participates in musical state.
struct NativeBatchRecord final
{
  std::uint64_t captureSequence = 0;
  std::uintptr_t instanceIdentity = 0;
  std::uint64_t hostCallbackIndex = 0;
  std::uint64_t nativeBatchIndex = 0;
  std::uint64_t nativeFrame = 0;
  std::uint64_t publishedPatternGeneration = 0;
  std::uint64_t consumedPatternGeneration = 0;
  std::int32_t hostCallbackFrames = 0;
  std::int32_t hostSampleOffset = -1;
  std::uint8_t activePattern = 0;
  std::uint8_t triggerCount = 0;
  std::uint8_t lockCount = 0;
  std::array<sequencer::TriggerEvent,
             sequencer::TriggerEventBatch::capacity()> triggers {};
  std::array<sequencer::AutomationEvent,
             sequencer::AutomationEventBatch::capacity()> locks {};
};

class NativeBatchCapture final
{
public:
  // 1024 * (5632353/128) / 48000 is below 939 native frames, or
  // 30 refills. Sixty-four records retain more than twice that bound.
  static constexpr std::size_t kCapacity = 64;

  void configure(bool enabled, std::size_t limit = kCapacity) noexcept
  {
    mEnabled = enabled;
    mLimit = limit <= kCapacity ? limit : kCapacity;
    mRetained = 0;
    mDropped = 0;
    mCompleted = 0;
    mOverflow = false;
  }

  void beginHostCallback(std::int32_t frames) noexcept
  {
    if (!mEnabled)
      return;
    if (mHostCallbackIndex != std::numeric_limits<std::uint64_t>::max())
      ++mHostCallbackIndex;
    mHostCallbackFrames = frames;
    mRetained = 0;
    mDropped = 0;
    mCompleted = 0;
    mOverflow = false;
  }

  void retain(const void* instance,
              const sequencer::TriggerScheduler& scheduler,
              std::uint64_t nativeFrame,
              std::int32_t hostSampleOffset) noexcept
  {
    if (!mEnabled)
      return;
    if (mCompleted != std::numeric_limits<std::uint64_t>::max())
      ++mCompleted;
    if (mRetained >= mLimit)
    {
      if (mDropped != std::numeric_limits<std::uint64_t>::max())
        ++mDropped;
      mOverflow = true;
      return;
    }

    // The synchronous controlled host consumes only after process() returns.
    // Publish mRetained last so a record is never visible while incomplete.
    auto& record = mRecords[mRetained];
    record.captureSequence = mNextSequence;
    if (mNextSequence != std::numeric_limits<std::uint64_t>::max())
      ++mNextSequence;
    record.instanceIdentity = reinterpret_cast<std::uintptr_t>(instance);
    record.hostCallbackIndex = mHostCallbackIndex;
    record.nativeBatchIndex = nativeFrame / 32u;
    record.nativeFrame = nativeFrame;
    record.publishedPatternGeneration =
      scheduler.patternAuthority().generation();
    record.consumedPatternGeneration = 0u;
    record.hostCallbackFrames = mHostCallbackFrames;
    record.hostSampleOffset = hostSampleOffset;
    record.activePattern = scheduler.selectedPattern();

    const auto& triggers = scheduler.lastBatch();
    record.triggerCount = static_cast<std::uint8_t>(triggers.size());
    for (std::size_t index = 0; index < triggers.size(); ++index)
      record.triggers[index] = triggers[index];
    if (!triggers.empty())
      record.consumedPatternGeneration = triggers[0].snapshotGeneration;

    const auto& locks = scheduler.lastAutomationBatch();
    record.lockCount = static_cast<std::uint8_t>(locks.size());
    for (std::size_t index = 0; index < locks.size(); ++index)
      record.locks[index] = locks[index];
    if (record.consumedPatternGeneration == 0u && !locks.empty())
      record.consumedPatternGeneration = locks[0].snapshotGeneration;

    ++mRetained;
  }

  bool enabled() const noexcept { return mEnabled; }
  std::size_t size() const noexcept { return mRetained; }
  std::size_t limit() const noexcept { return mLimit; }
  std::uint64_t completed() const noexcept { return mCompleted; }
  std::uint64_t dropped() const noexcept { return mDropped; }
  bool overflow() const noexcept { return mOverflow; }
  std::uint64_t callbackIndex() const noexcept { return mHostCallbackIndex; }
  const NativeBatchRecord& operator[](std::size_t index) const noexcept
  {
    return mRecords[index]; // Precondition: index < size().
  }

private:
  std::array<NativeBatchRecord, kCapacity> mRecords {};
  std::size_t mRetained = 0;
  std::size_t mLimit = kCapacity;
  std::uint64_t mCompleted = 0;
  std::uint64_t mDropped = 0;
  std::uint64_t mNextSequence = 0;
  std::uint64_t mHostCallbackIndex = 0;
  std::int32_t mHostCallbackFrames = 0;
  bool mEnabled = false;
  bool mOverflow = false;
};

} // namespace lxr::diagnostic

#endif // LXR_P20D_DIAGNOSTIC_CAPTURE
