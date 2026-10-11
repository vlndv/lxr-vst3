#pragma once

#include "Engine.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace lxr
{

class LXRAudioAdapter final
{
public:
  // Native LXR Engine processing quantum.
  static constexpr std::size_t kEngineBlockSize = 32;

  LXRAudioAdapter() = default;

  // Clear all adapter-owned buffering state.
  //
  // This does not reset Engine. Engine lifecycle remains owned by
  // the plug-in wrapper.
  void reset() noexcept;

  // Return the next native ST1 stereo sample.
  //
  // P12b-4a deliberately preserves the Engine's int16_t sample
  // representation. Host floating-point conversion and sample-rate
  // conversion are introduced in later phases.
  void getNextST1Sample(
    Engine& engine,
    int16_t& left,
    int16_t& right
  );

private:
  // Native Engine output buffers.
  //
  // Engine::processBlock() renders all four outputs together:
  //
  //   ST1 Left
  //   ST1 Right
  //   ST2 Left
  //   ST2 Right
  //
  // ST2 is retained even though the first plug-in stereo output will
  // eventually expose ST1 only.
  std::array<int16_t, kEngineBlockSize> mST1Left {};
  std::array<int16_t, kEngineBlockSize> mST1Right {};
  std::array<int16_t, kEngineBlockSize> mST2Left {};
  std::array<int16_t, kEngineBlockSize> mST2Right {};

  // Index of the next unread sample.
  //
  // kEngineBlockSize means the cache is exhausted and a new native
  // Engine block must be rendered before another sample is returned.
  std::size_t mReadIndex = kEngineBlockSize;

  // Render one complete native Engine block into the adapter cache.
  void renderNextEngineBlock(
    Engine& engine
  );
};

} // namespace lxr