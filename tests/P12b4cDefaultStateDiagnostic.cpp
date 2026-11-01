
//
// FILE: tests/P12b4cDefaultStateDiagnostic.cpp
//
// P12b-4c — Default Engine State Diagnostic
//
// PURPOSE:
// Determine whether LXR produces native audio using
// its default parameter state, without applying
// the custom 808 test kit.
//
// This test bypasses:
//   - REAPER
//   - iPlug2
//   - Polyphase FIR resampler
//
// It tests:
//   ParameterArray -> Engine -> Mixer -> ST1/ST2
//

#include "../dsp/Engine.h"
#include "../dsp/ParameterArray.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv)
{
    // -----------------------------------------------------
    // STEP 1 — Initialize the native synthesis engine
    // -----------------------------------------------------

    lxr::Engine engine;

    // Load oscillator tables and sample assets.
    if (!engine.loadAssets("data"))
    {
        std::puts("FAIL: Engine assets unavailable");
        return 1;
    }

    std::puts("PASS: Engine assets loaded");

    // -----------------------------------------------------
    // STEP 2 — Initialize default parameters
    // -----------------------------------------------------

    lxr::ParameterArray params;

    params.init();

    // Reproduce the native default parameter values.
    //
    // Unlike the previous native audio diagnostic,
    // do NOT call engine.setup808Kit().
    //
    // Apply every stored default parameter to the Engine.

    for (int id = 0; id < lxr::kNumParams; ++id)
    {
        const uint8_t parameterId =
            static_cast<uint8_t>(id);

        const uint8_t defaultValue =
            params.get(parameterId);

        params.set(
            parameterId,
            defaultValue,
            engine
        );
    }

    std::puts("PASS: Default parameters applied");

    // Controlled experiments: change only the named raw parameters.
    for (int arg = 1; arg < argc; ++arg)
    {
        if (std::strcmp(argv[arg], "--attack64") == 0)
            params.set(49, 64, engine);
        else if (std::strcmp(argv[arg], "--full-rate") == 0)
        {
            params.set(108, 127, engine);
            params.set(114, 127, engine);
        }
        else if (std::strcmp(argv[arg], "--legacy") == 0)
        {
            params.set(49, 127, engine);
            params.set(108, 0, engine);
            params.set(114, 0, engine);
        }
        else
            return 2;
    }

    std::printf("D1 raw attack/decay: %u/%u; steps: %.9g/%.9g\n"
                "D1 decimator voice/global: %.9g/%.9g\n",
                params.get(49), params.get(50),
                engine.drums[0].oscVolEg.attack, engine.drums[0].oscVolEg.decay,
                engine.mixer.decimationRate[0], engine.mixer.decimationRate[6]);

    // -----------------------------------------------------
    // STEP 3 — Inspect initial D1 state
    // -----------------------------------------------------

    std::printf(
        "D1 waveform: %u\n",
        static_cast<unsigned>(
            engine.drums[0].osc.waveform
        )
    );

    std::printf(
        "D1 volume: %.6f\n",
        static_cast<double>(
            engine.drums[0].vol
        )
    );

    std::printf(
        "D1 routing: %u\n",
        static_cast<unsigned>(
            engine.mixer.routing[0]
        )
    );

    // -----------------------------------------------------
    // STEP 4 — Trigger a native D1 drum hit
    // -----------------------------------------------------

    // MIDI channel 1 maps to D1.
    //
    // Note: 60
    // Velocity: 110
    //
    // Trigger the native Engine directly.

    engine.triggerDrum(
        0,
        110,
        60
    );

    std::puts("PASS: D1 trigger executed");
    std::printf("D1 triggered envelope state/value: %d/%.9g\n",
                engine.drums[0].oscVolEg.state, engine.drums[0].oscVolEg.value);

    // -----------------------------------------------------
    // STEP 5 — Prepare the native output buffers
    // -----------------------------------------------------

    constexpr std::size_t kBlockSize = 32;

    constexpr int kBlocksToRender = 100;

    std::array<int16_t, kBlockSize> st1L {};
    std::array<int16_t, kBlockSize> st1R {};

    std::array<int16_t, kBlockSize> st2L {};
    std::array<int16_t, kBlockSize> st2R {};

    // Store absolute PCM peak amplitudes.
    int peakST1 = 0;
    int peakST2 = 0;

    // Count nonzero frames independently.
    std::uint64_t nonzeroST1 = 0;
    std::uint64_t nonzeroST2 = 0;

    // -----------------------------------------------------
    // STEP 6 — Render native Engine audio
    // -----------------------------------------------------

    for (int block = 0;
         block < kBlocksToRender;
         ++block)
    {
        // Render one complete native LXR DSP block.
        engine.processBlock(
            st1L.data(),
            st1R.data(),
            st2L.data(),
            st2R.data()
        );

        // Inspect each native output frame.
        for (std::size_t i = 0;
             i < kBlockSize;
             ++i)
        {
            const int left1 =
                std::abs(static_cast<int>(st1L[i]));

            const int right1 =
                std::abs(static_cast<int>(st1R[i]));

            const int left2 =
                std::abs(static_cast<int>(st2L[i]));

            const int right2 =
                std::abs(static_cast<int>(st2R[i]));

            // Track peak amplitude on ST1.
            peakST1 = std::max(
                peakST1,
                std::max(left1, right1)
            );

            // Track peak amplitude on ST2.
            peakST2 = std::max(
                peakST2,
                std::max(left2, right2)
            );

            // Count frames containing nonzero audio.
            if (left1 != 0 || right1 != 0)
            {
                ++nonzeroST1;
            }

            if (left2 != 0 || right2 != 0)
            {
                ++nonzeroST2;
            }
        }
    }

    // -----------------------------------------------------
    // STEP 7 — Print the measured results
    // -----------------------------------------------------

    std::printf("D1 final envelope state/value: %d/%.9g; gain: %.9g; "
                "velocity: %.9g; oscillator increment: %u\n",
                engine.drums[0].oscVolEg.state, engine.drums[0].oscVolEg.value,
                engine.drums[0].targetGain, engine.drums[0].velo,
                engine.drums[0].osc.phaseInc);
    std::printf(
        "\n=== DEFAULT ENGINE AUDIO RESULTS ===\n"
    );

    std::printf(
        "Default ST1 peak: %d\n",
        peakST1
    );

    std::printf(
        "Default ST2 peak: %d\n",
        peakST2
    );

    std::printf(
        "Nonzero ST1 frames: %llu\n",
        static_cast<unsigned long long>(
            nonzeroST1
        )
    );

    std::printf(
        "Nonzero ST2 frames: %llu\n",
        static_cast<unsigned long long>(
            nonzeroST2
        )
    );

    // -----------------------------------------------------
    // STEP 8 — Evaluate the default native output
    // -----------------------------------------------------

    if (peakST1 == 0 && peakST2 == 0)
    {
        std::puts(
            "FAIL: Default engine produces silence"
        );

        return 1;
    }

    if (peakST1 == 0 && peakST2 > 0)
    {
        std::puts(
            "FAIL: Audio is routed to ST2 only"
        );

        return 1;
    }

    std::puts(
        "PASS: Default engine produces ST1 audio"
    );

    return 0;
}
