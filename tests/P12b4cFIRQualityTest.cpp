//
// FILE: tests/P12b4cFIRQualityTest.cpp
//
// P12b-4c — Independent FIR quality measurements.
//
// Tests:
//   1. DC gain
//   2. Passband amplitude response
//   3. Downsampling alias rejection
//   4. Impulse group delay
//   5. Long-run source-position drift
//   6. Numerical stability
//
// C++17, standalone test executable.
// Does not modify the native LXR Engine.
//

#include "../dsp/PolyphaseResampler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace
{

constexpr double kPi =
    3.1415926535897932384626433832795;

constexpr double kSourceRate =
    lxr::PolyphaseResampler::kSourceRate;

int gPassed = 0;
int gFailed = 0;

// Report a failed assertion without stopping other tests.
void check(const char* name, bool condition)
{
    if (condition)
    {
        ++gPassed;
        std::printf("PASS %s\n", name);
    }
    else
    {
        ++gFailed;
        std::printf("FAIL %s\n", name);
    }
}

// Convert an amplitude ratio into decibels.
double toDb(double amplitude)
{
    if (amplitude <= 0.0)
    {
        return -300.0;
    }

    return 20.0 * std::log10(amplitude);
}

// Independent signal source based on the native
// Engine sample index, not the resampler's phase.
struct SineSource
{
    double frequency = 1000.0;
    double amplitude = 0.5;
    std::uint64_t index = 0;

    void operator()(
        double& left,
        double& right
    ) noexcept
    {
        const double phase =
            2.0 * kPi *
            frequency *
            static_cast<double>(index) /
            kSourceRate;

        left = amplitude * std::sin(phase);
        right = -left;

        ++index;
    }
};

// Measure a sinusoidal component at a selected
// host-domain frequency using sine/cosine projection.
//
// The returned magnitude is independent of the
// unknown constant phase delay.
//
// A measurement window with many cycles reduces
// leakage from unrelated frequency components.
double measureComponent(
    double hostRate,
    double inputFrequency,
    double measurementFrequency,
    double inputAmplitude = 0.5
)
{
    lxr::PolyphaseResampler resampler;

    if (!resampler.configure(hostRate))
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    SineSource source;
    source.frequency = inputFrequency;
    source.amplitude = inputAmplitude;

    // Discard the initial FIR startup interval.
    const std::size_t warmup =
        static_cast<std::size_t>(hostRate * 0.10);

    // Analyze one second of steady-state audio.
    const std::size_t count =
        static_cast<std::size_t>(hostRate);

    double projectionSin = 0.0;
    double projectionCos = 0.0;

    for (std::size_t i = 0; i < warmup + count; ++i)
    {
        double left = 0.0;
        double right = 0.0;

        resampler.next(source, left, right);

        if (!std::isfinite(left) ||
            !std::isfinite(right))
        {
            return std::numeric_limits<double>::quiet_NaN();
        }

        if (i < warmup)
        {
            continue;
        }

        const double time =
            static_cast<double>(i - warmup) /
            hostRate;

        const double referencePhase =
            2.0 * kPi *
            measurementFrequency * time;

        projectionSin +=
            left * std::sin(referencePhase);

        projectionCos +=
            left * std::cos(referencePhase);
    }

    return 2.0 *
        std::hypot(projectionSin, projectionCos) /
        static_cast<double>(count);
}

// ---------------------------------------------------------
// 1. DC gain
// ---------------------------------------------------------

void testDcGain(double hostRate)
{
    lxr::PolyphaseResampler resampler;

    if (!resampler.configure(hostRate))
    {
        check("DC configuration", false);
        return;
    }

    auto source = [](
        double& left,
        double& right
    ) noexcept
    {
        left = 0.5;
        right = -0.25;
    };

    double maxError = 0.0;

    const std::size_t frames =
        static_cast<std::size_t>(hostRate * 0.1);

    for (std::size_t i = 0; i < frames; ++i)
    {
        double left = 0.0;
        double right = 0.0;

        resampler.next(source, left, right);

        // Exclude the zero-padded startup transient.
        if (i < 512)
        {
            continue;
        }

        const double error =
            std::max(
                std::abs(left - 0.5),
                std::abs(right + 0.25)
            );

        maxError = std::max(maxError, error);
    }

    std::printf(
        "DC %.0f Hz: max error %.9f\n",
        hostRate,
        maxError
    );

    check(
        "DC gain",
        std::isfinite(maxError) &&
        maxError <= 0.001
    );
}

// ---------------------------------------------------------
// 2. Passband response
// ---------------------------------------------------------

void testPassband(
    double hostRate,
    double frequency
)
{
    constexpr double amplitude = 0.5;

    const double measured =
        measureComponent(
            hostRate,
            frequency,
            frequency,
            amplitude
        );

    const double responseDb =
        toDb(measured / amplitude);

    std::printf(
        "PASSBAND host=%.0f input=%.0f Hz "
        "response=%.3f dB\n",
        hostRate,
        frequency,
        responseDb
    );

    check(
        "passband amplitude",
        std::isfinite(measured) &&
        std::abs(responseDb) <= 0.5
    );
}

// ---------------------------------------------------------
// 3. Alias rejection when downsampling
// ---------------------------------------------------------

void testAliasing(
    double hostRate,
    double inputFrequency,
    double minimumAttenuationDb
)
{
    // Fold the native input frequency into
    // the destination Nyquist interval.
    double aliasedFrequency =
        std::fmod(inputFrequency, hostRate);

    if (aliasedFrequency > hostRate * 0.5)
    {
        aliasedFrequency =
            hostRate - aliasedFrequency;
    }

    constexpr double inputAmplitude = 0.5;

    const double measured =
        measureComponent(
            hostRate,
            inputFrequency,
            aliasedFrequency,
            inputAmplitude
        );

    const double attenuationDb =
        toDb(measured / inputAmplitude);

    std::printf(
        "ALIAS host=%.0f input=%.0f Hz "
        "alias=%.0f Hz attenuation=%.2f dB\n",
        hostRate,
        inputFrequency,
        aliasedFrequency,
        attenuationDb
    );

    check(
        "alias rejection",
        std::isfinite(measured) &&
        attenuationDb <= -minimumAttenuationDb
    );
}

// ---------------------------------------------------------
// 4. Impulse group delay
// ---------------------------------------------------------

void testLatency(double hostRate)
{
    lxr::PolyphaseResampler resampler;

    if (!resampler.configure(hostRate))
    {
        check("latency configuration", false);
        return;
    }

    std::uint64_t sourceIndex = 0;

    auto source = [&sourceIndex](
        double& left,
        double& right
    ) noexcept
    {
        left = sourceIndex == 0 ? 1.0 : 0.0;
        right = 0.0;

        ++sourceIndex;
    };

    double largestMagnitude = 0.0;
    std::size_t peakFrame = 0;

    const std::size_t frames =
        static_cast<std::size_t>(
            std::ceil(
                128.0 * hostRate / kSourceRate
            )
        ) + 8;

    for (std::size_t frame = 0;
         frame < frames;
         ++frame)
    {
        double left = 0.0;
        double right = 0.0;

        resampler.next(source, left, right);

        const double magnitude = std::abs(left);

        if (magnitude > largestMagnitude)
        {
            largestMagnitude = magnitude;
            peakFrame = frame;
        }
    }

    const double expectedHostFrames =
        lxr::PolyphaseResampler::
            groupDelaySourceFrames() *
        hostRate / kSourceRate;

    const double measuredError =
        std::abs(
            static_cast<double>(peakFrame) -
            expectedHostFrames
        );

    std::printf(
        "LATENCY host=%.0f "
        "expected=%.3f frames "
        "peak=%zu frames error=%.3f\n",
        hostRate,
        expectedHostFrames,
        peakFrame,
        measuredError
    );

    check(
        "impulse latency",
        largestMagnitude > 0.01 &&
        measuredError <= 2.0
    );
}

// ---------------------------------------------------------
// 5. Long-run rate and finite-value stability
// ---------------------------------------------------------

void testLongRun(double hostRate)
{
    lxr::PolyphaseResampler resampler;

    if (!resampler.configure(hostRate))
    {
        check("long-run configuration", false);
        return;
    }

    std::uint64_t sourceFrames = 0;

    auto source = [&sourceFrames](
        double& left,
        double& right
    ) noexcept
    {
        const double phase =
            2.0 * kPi * 997.0 *
            static_cast<double>(sourceFrames) /
            kSourceRate;

        left = 0.25 * std::sin(phase);
        right = -left;

        ++sourceFrames;
    };

    constexpr std::size_t hostFrames = 200000;

    bool finite = true;

    for (std::size_t i = 0;
         i < hostFrames;
         ++i)
    {
        double left = 0.0;
        double right = 0.0;

        resampler.next(source, left, right);

        if (!std::isfinite(left) ||
            !std::isfinite(right))
        {
            finite = false;
            break;
        }
    }

    // Independent expected position.
    const double expectedPosition =
        static_cast<double>(hostFrames) *
        kSourceRate / hostRate;

    const double positionError =
        std::abs(
            resampler.sourcePosition() -
            expectedPosition
        );

    std::printf(
        "LONG RUN host=%.0f "
        "frames=%zu position_error=%.9f "
        "source_frames=%llu\n",
        hostRate,
        hostFrames,
        positionError,
        static_cast<unsigned long long>(
            sourceFrames
        )
    );

    check(
        "long-run stability",
        finite
    );

    check(
        "long-run rate drift",
        finite && positionError <= 0.001
    );
}

} // namespace

int main()
{
    std::puts(
        "P12b-4c FIR AUDIO QUALITY TESTS\n"
    );

    constexpr double rates[] = {
        32000.0,
        44100.0,
        48000.0,
        88200.0,
        96000.0,
        192000.0
    };

    // Test each configured host rate.
    for (double rate : rates)
    {
        std::printf(
            "\n=== HOST RATE %.0f ===\n",
            rate
        );

        testDcGain(rate);

        testPassband(rate, 1000.0);
        testPassband(rate, 8000.0);

        // 12 kHz is within the 32 kHz host
        // passband but closer to the transition band.
        if (rate == 32000.0)
        {
            testPassband(rate, 12000.0);
        }

        testLatency(rate);
        testLongRun(rate);
    }

    // 19 kHz lies above the 16 kHz Nyquist
    // frequency of the 32 kHz destination.
    // It would alias to 13 kHz without filtering.
    std::puts("\n=== DOWNSAMPLING ALIAS TEST ===");

    testAliasing(
        32000.0,
        19000.0,
        40.0
    );

    std::printf(
        "\nSUMMARY pass=%d fail=%d\n",
        gPassed,
        gFailed
    );

    return gFailed == 0 ? 0 : 1;
}
