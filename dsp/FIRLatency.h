#pragma once

#include "Engine.h"
#include "PolyphaseResampler.h"

#include <cmath>

namespace lxr { namespace firLatency {

// Same continuous rate range accepted by PolyphaseResampler::configure().
inline bool supported(double hostRate) noexcept
{
  return std::isfinite(hostRate) && hostRate >= 32000.0 && hostRate <= 192000.0;
}

// Nominal linear-phase delay. Phase quantization and windowing mean that an
// individual sampled impulse peak need not fall exactly at this position.
inline double exactHostFrames(double hostRate) noexcept
{
  return supported(hostRate)
    ? PolyphaseResampler::groupDelaySourceFrames() * hostRate /
        static_cast<double>(kEngineSampleRate)
    : 0.0;
}

// VST3 accepts integer samples, with no mandated rounding direction. Nearest
// minimizes nominal PDC error (at most 0.5 frame); ties round upward. Reporting
// does not delay PCM. Residual after compensation = exactHostFrames - hostSamples.
inline int hostSamples(double hostRate) noexcept
{
  return static_cast<int>(std::floor(exactHostFrames(hostRate) + 0.5));
}

// Invalid/unconfigured rates consistently report zero, including NaN and inf.
inline double nominalMilliseconds(double hostRate) noexcept
{
  return supported(hostRate)
    ? 1000.0 * PolyphaseResampler::groupDelaySourceFrames() /
        static_cast<double>(kEngineSampleRate)
    : 0.0;
}

}} // namespace lxr::firLatency
