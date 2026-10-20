#include "LXRAudioAdapter.h"

namespace lxr
{

void LXRAudioAdapter::reset() noexcept
{
  // Discard any cached native engine samples.
  mST1Left.fill(0);
  mST1Right.fill(0);
  mST2Left.fill(0);
  mST2Right.fill(0);

  mReadIndex = kEngineBlockSize;

  // Discard FIR history, but retain configuration.
  mResampler.reset();
}

bool LXRAudioAdapter::configure(
  double hostSampleRate
) noexcept
{
  // Invalid rates are rejected.
  if (!mResampler.configure(hostSampleRate))
  {
    return false;
  }

  // The next sample must come from a fresh engine block.
  mReadIndex = kEngineBlockSize;

  return true;
}

void LXRAudioAdapter::renderNextEngineBlock(
  Engine& engine
) noexcept
{
  // Preserve the original four-output Engine contract.
  mST1Left.fill(0);
  mST1Right.fill(0);
  mST2Left.fill(0);
  mST2Right.fill(0);

  engine.processBlock(
    mST1Left.data(),
    mST1Right.data(),
    mST2Left.data(),
    mST2Right.data()
  );

  mReadIndex = 0;
}

void LXRAudioAdapter::getNextST1Sample(
  Engine& engine,
  int16_t& left,
  int16_t& right
) noexcept
{
  // Refill only after consuming the current 32 frames.
  if (mReadIndex >= kEngineBlockSize)
  {
    renderNextEngineBlock(engine);
  }

  left = mST1Left[mReadIndex];
  right = mST1Right[mReadIndex];

  ++mReadIndex;
}

void LXRAudioAdapter::getNextHostST1Sample(
  Engine& engine,
  double& left,
  double& right
) noexcept
{
  // Native PCM producer for the resampler.
  auto source =
    [this, &engine](
      double& sourceLeft,
      double& sourceRight
    ) noexcept
    {
      int16_t nativeLeft = 0;
      int16_t nativeRight = 0;

      getNextST1Sample(
        engine,
        nativeLeft,
        nativeRight
      );

      // Signed PCM to normalized floating-point.
      sourceLeft =
        static_cast<double>(nativeLeft) / 32768.0;

      sourceRight =
        static_cast<double>(nativeRight) / 32768.0;
    };

  mResampler.next(
    source,
    left,
    right
  );
}

} // namespace lxr
