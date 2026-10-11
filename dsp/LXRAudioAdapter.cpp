#include "LXRAudioAdapter.h"

namespace lxr
{

void LXRAudioAdapter::reset() noexcept
{
  // Remove any cached samples left from the previous processing state.
  mST1Left.fill(0);
  mST1Right.fill(0);
  mST2Left.fill(0);
  mST2Right.fill(0);

  // Force a fresh Engine render when the next native sample is
  // requested.
  mReadIndex = kEngineBlockSize;
}

void LXRAudioAdapter::renderNextEngineBlock(
  Engine& engine
)
{
  // Clear all output buffers defensively before rendering.
  mST1Left.fill(0);
  mST1Right.fill(0);
  mST2Left.fill(0);
  mST2Right.fill(0);

  // Render exactly one native LXR Engine block.
  //
  // The Engine owns synthesis/DSP.
  // LXRAudioAdapter owns only buffering and, in later phases,
  // conversion to the host's processing domain.
  engine.processBlock(
    mST1Left.data(),
    mST1Right.data(),
    mST2Left.data(),
    mST2Right.data()
  );

  // Start consuming the freshly rendered block from sample zero.
  mReadIndex = 0;
}

void LXRAudioAdapter::getNextST1Sample(
  Engine& engine,
  int16_t& left,
  int16_t& right
)
{
  // Refill the native cache only after every sample from the previous
  // Engine block has been consumed.
  if (mReadIndex >= kEngineBlockSize)
  {
    renderNextEngineBlock(engine);
  }

  left = mST1Left[mReadIndex];
  right = mST1Right[mReadIndex];

  ++mReadIndex;
}

} // namespace lxr