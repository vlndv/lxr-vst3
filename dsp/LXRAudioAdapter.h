#pragma once

#include "Engine.h"
#include "PolyphaseResampler.h"
#if defined(LXR_P20D_DIAGNOSTIC_CAPTURE)
#include "NativeBatchCapture.h"
#endif

#include <array>
#include <cstddef>
#include <cstdint>

namespace lxr
{

namespace sequencer
{
class TriggerScheduler;
}

class LXRAudioAdapter final
{
public:
  static constexpr std::size_t kEngineBlockSize = 32;

  // Reset native buffering and FIR history.
  void reset() noexcept;

  // Call when the host sample rate changes.
  bool configure(double hostSampleRate) noexcept;

  bool ready() const noexcept
  {
    return mResampler.ready();
  }

  double hostRate() const noexcept
  {
    return mResampler.hostRate();
  }

  // Generate one resampled floating-point stereo frame.
  void getNextHostST1Sample(
    Engine& engine,
    double& left,
    double& right,
    sequencer::TriggerScheduler* scheduler = nullptr,
    std::int32_t hostSampleOffset = -1
  ) noexcept;

  // Preserve the original native PCM interface.
  void getNextST1Sample(
    Engine& engine,
    int16_t& left,
    int16_t& right,
    sequencer::TriggerScheduler* scheduler = nullptr,
    std::int32_t hostSampleOffset = -1
  ) noexcept;

  std::uint64_t nativeFrame() const noexcept
  {
    return mNativeFrame;
  }

#if defined(LXR_P20D_DIAGNOSTIC_CAPTURE)
  // TEST-ONLY DIAGNOSTIC: ordinary production candidates do not contain this
  // storage or interface. The controlled host calls configure only while the
  // instance is stopped and consumes records only after process() returns.
  diagnostic::NativeBatchCapture& diagnosticCapture() noexcept
  {
    return mDiagnosticCapture;
  }
  const diagnostic::NativeBatchCapture& diagnosticCapture() const noexcept
  {
    return mDiagnosticCapture;
  }
  void beginDiagnosticHostCallback(std::int32_t frames) noexcept
  {
    mDiagnosticCapture.beginHostCallback(frames);
  }
#endif

private:
  // Native LXR output buffers, 32 frames each.
  std::array<int16_t, kEngineBlockSize> mST1Left {};
  std::array<int16_t, kEngineBlockSize> mST1Right {};
  std::array<int16_t, kEngineBlockSize> mST2Left {};
  std::array<int16_t, kEngineBlockSize> mST2Right {};

  std::size_t mReadIndex = kEngineBlockSize;
  std::uint64_t mNativeFrame = 0;

  PolyphaseResampler mResampler {};

#if defined(LXR_P20D_DIAGNOSTIC_CAPTURE)
  diagnostic::NativeBatchCapture mDiagnosticCapture {};
#endif

  void renderNextEngineBlock(
    Engine& engine,
    sequencer::TriggerScheduler* scheduler,
    std::int32_t hostSampleOffset
  ) noexcept;
};

} // namespace lxr
