#include "../dsp/Engine.h"
#include "../dsp/ParameterArray.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

int main()
{
    lxr::Engine engine;

    if (!engine.loadAssets("data"))
    {
        std::puts("FAIL: Assets unavailable");
        return 1;
    }

    lxr::ParameterArray params;
    params.init();

    // Reproduce the plugin's default parameter
    // reconstruction without an 808 test preset.
    for (int id = 0; id < lxr::kNumParams; ++id)
    {
        params.set(
            static_cast<uint8_t>(id),
            params.get(static_cast<uint8_t>(id)),
            engine
        );
    }

    // MIDI channel 1, note 60, velocity 110.
    engine.triggerDrum(0, 110, 60);

    std::array<int16_t, 32> st1L {};
    std::array<int16_t, 32> st1R {};
    std::array<int16_t, 32> st2L {};
    std::array<int16_t, 32> st2R {};

    int peakST1 = 0;
    int peakST2 = 0;

    for (int block = 0; block < 100; ++block)
    {
        engine.processBlock(
            st1L.data(),
            st1R.data(),
            st2L.data(),
            st2R.data()
        );

        for (int i = 0; i < 32; ++i)
        {
            peakST1 = std::max(
                peakST1,
                std::abs(static_cast<int>(st1L[i]))
            );
            peakST1 = std::max(
                peakST1,
                std::abs(static_cast<int>(st1R[i]))
            );
            peakST2 = std::max(
                peakST2,
                std::abs(static_cast<int>(st2L[i]))
            );
            peakST2 = std::max(
                peakST2,
                std::abs(static_cast<int>(st2R[i]))
            );
        }
    }

    std::printf(
        "Default ST1 peak: %d\n"
        "Default ST2 peak: %d\n",
        peakST1,
        peakST2
    );

    if (peakST1 == 0)
    {
        std::puts(
            "FAIL: Default plugin parameter state "
            "produces no ST1 audio"
        );
        return 1;
    }

    std::puts(
        "PASS: Default engine state produces ST1 audio"
    );
    return 0;
}
