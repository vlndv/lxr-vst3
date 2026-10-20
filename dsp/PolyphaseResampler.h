
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace lxr
{

// Streaming, causal, stereo polyphase FIR resampler.
// No dynamic allocation is performed.
class PolyphaseResampler final
{
public:
  static constexpr std::size_t kTaps = 48;
  static constexpr std::size_t kPhases = 512;
  static constexpr std::size_t kHistory = 128;

  // Match the float constant in Engine.h exactly.
  static constexpr double kSourceRate =
    static_cast<double>(44002.7573529412f);

  // Call outside the audio processing callback.
  // Supported host rates: 32 kHz through 192 kHz.
  bool configure(double rate) noexcept
  {
    if (!std::isfinite(rate) ||
        rate < 32000.0 ||
        rate > 192000.0)
    {
      return false;
    }

    constexpr double pi =
      3.1415926535897932384626433832795;

    constexpr double delay =
      (kTaps - 1) * 0.5;

    // Conservative transition band below Nyquist.
    const double cutoff =
      0.47 * ((rate < kSourceRate)
        ? rate / kSourceRate
        : 1.0);

    // Generate each fractional-delay filter.
    for (std::size_t phase = 0;
         phase < kPhases;
         ++phase)
    {
      const double fraction =
        static_cast<double>(phase) /
        static_cast<double>(kPhases);

      double sum = 0.0;

      for (std::size_t tap = 0;
           tap < kTaps;
           ++tap)
      {
        const double distance =
          static_cast<double>(tap) +
          fraction - delay;

        const double angle =
          2.0 * pi * cutoff * distance;

        const double sinc =
          (std::abs(angle) < 1.0e-12)
            ? 1.0
            : std::sin(angle) / angle;

        // Symmetric Blackman window.
        const double window =
          0.42 -
          0.5 * std::cos(
            2.0 * pi * tap / (kTaps - 1)
          ) +
          0.08 * std::cos(
            4.0 * pi * tap / (kTaps - 1)
          );

        const double coefficient =
          2.0 * cutoff * sinc * window;

        mCoefficients[
          phase * kTaps + tap
        ] = coefficient;

        sum += coefficient;
      }

      if (!(sum > 0.0) || !std::isfinite(sum))
      {
        return false;
      }

      // Unity DC gain for every phase.
      for (std::size_t tap = 0;
           tap < kTaps;
           ++tap)
      {
        mCoefficients[
          phase * kTaps + tap
        ] /= sum;
      }
    }

    mStep = kSourceRate / rate;
    mRate = rate;

    reset();
    return true;
  }

  void reset() noexcept
  {
    mLeft.fill(0.0);
    mRight.fill(0.0);
    mPosition = 0.0;
    mNextIndex = 0;
  }

  bool ready() const noexcept
  {
    return mRate > 0.0;
  }

  double hostRate() const noexcept
  {
    return mRate;
  }

  double sourcePosition() const noexcept
  {
    return mPosition;
  }

  static constexpr double
  groupDelaySourceFrames() noexcept
  {
    return (kTaps - 1) * 0.5;
  }

  // Source must provide:
  // void operator()(double& left, double& right)
  //
  // Both stereo channels share the same phase.
  template<class Source>
  void next(
    Source& source,
    double& outLeft,
    double& outRight
  ) noexcept
  {
    outLeft = 0.0;
    outRight = 0.0;

    if (!ready())
    {
      return;
    }

    const std::uint64_t base =
      static_cast<std::uint64_t>(mPosition);

    // Load native samples only when needed.
    while (mNextIndex <= base)
    {
      double left = 0.0;
      double right = 0.0;

      source(left, right);

      const std::size_t slot =
        static_cast<std::size_t>(
          mNextIndex % kHistory
        );

      mLeft[slot] =
        std::isfinite(left) ? left : 0.0;

      mRight[slot] =
        std::isfinite(right) ? right : 0.0;

      ++mNextIndex;
    }

    const double fraction =
      mPosition - static_cast<double>(base);

    std::size_t phase =
      static_cast<std::size_t>(
        fraction * static_cast<double>(kPhases)
      );

    if (phase >= kPhases)
    {
      phase = kPhases - 1;
    }

    const std::size_t first =
      phase * kTaps;

    // Causal convolution. Missing pre-start samples
    // are defined as zero.
    for (std::size_t tap = 0;
         tap < kTaps;
         ++tap)
    {
      if (base < tap)
      {
        continue;
      }

      const std::uint64_t index =
        base - tap;

      const std::size_t slot =
        static_cast<std::size_t>(
          index % kHistory
        );

      const double coefficient =
        mCoefficients[first + tap];

      outLeft +=
        coefficient * mLeft[slot];

      outRight +=
        coefficient * mRight[slot];
    }

    mPosition += mStep;
  }

private:
  // Preallocated FIR coefficient bank.
  std::array<double, kTaps * kPhases>
    mCoefficients {};

  // Circular histories.
  std::array<double, kHistory> mLeft {};
  std::array<double, kHistory> mRight {};

  double mStep = 1.0;
  double mRate = 0.0;
  double mPosition = 0.0;

  std::uint64_t mNextIndex = 0;
};

} // namespace lxr
