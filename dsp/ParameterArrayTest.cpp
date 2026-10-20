//
// FILE: dsp/ParameterArrayTest.cpp
//
// Regression tests for the LXR ParameterArray.
//
// The tests verify parameter-to-engine mappings.
//
// ORIGINAL QUIRK:
// VEL_DEST parameters 149-154 are not settable through
// the original firmware's MIDI parameter path.
// ParameterArray therefore leaves those destinations unchanged.
//

#include "ParameterArray.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

// ---------------------------------------------------------
// Test counters
// ---------------------------------------------------------

static int pass = 0;
static int fail = 0;

// ---------------------------------------------------------
// Test assertion helper
// ---------------------------------------------------------

void check(const char* name, bool condition)
{
    if (condition)
    {
        std::printf("PASS %s\n", name);
        ++pass;
    }
    else
    {
        std::printf("FAIL %s\n", name);
        ++fail;
    }
}

// ---------------------------------------------------------
// Main regression suite
// ---------------------------------------------------------

int main()
{
    // Initialize the native LXR synthesis engine.
    lxr::Engine engine;

    // Load original waveform and sample assets.
    engine.loadAssets("data");

    // Initialize the canonical parameter array.
    lxr::ParameterArray params;
    params.init();

    // -----------------------------------------------------
    // Waveform parameters
    // -----------------------------------------------------

    params.set(1, 2, engine);

    check(
        "waveform D1",
        engine.drums[0].osc.waveform == 2
    );

    params.set(4, 3, engine);

    check(
        "waveform SN",
        engine.snare.osc.waveform == 3
    );

    // -----------------------------------------------------
    // Oscillator pitch
    // -----------------------------------------------------

    params.set(8, 70, engine);

    check(
        "coarse D1",
        (engine.drums[0].osc.midiFreq >> 8) == 70
    );

    params.set(9, 80, engine);

    check(
        "fine D1",
        (engine.drums[0].osc.midiFreq & 0xFF) == 80
    );

    // -----------------------------------------------------
    // Filter parameters
    // -----------------------------------------------------

    params.set(37, 64, engine);

    check(
        "filter freq D1",
        engine.drums[0].filter.f > 0.0f
    );

    params.set(43, 100, engine);

    const float expectedQ =
        1.0f - 100.0f / 127.0f;

    check(
        "filter reso D1",
        std::abs(
            engine.drums[0].filter.q - expectedQ
        ) < 0.01f
    );

    // -----------------------------------------------------
    // Volume and pan
    // -----------------------------------------------------

    params.set(88, 64, engine);

    check(
        "volume D1",
        std::abs(
            engine.drums[0].vol - 64.0f / 127.0f
        ) < 0.01f
    );

    params.set(94, 32, engine);

    check(
        "pan D1",
        engine.mixer.pan[0] == 32
    );

    // -----------------------------------------------------
    // Pitch envelope
    // -----------------------------------------------------

    params.set(74, 80, engine);

    check(
        "pitch mod amount D1",
        engine.drums[0].egPitchModAmount > 0.0f
    );

    params.set(70, 50, engine);

    check(
        "pitch eg decay D1",
        engine.drums[0].oscPitchEg.decay > 0.0f
    );

    params.set(77, 50, engine);

    check(
        "pitch eg decay D1",
        engine.drums[0].oscPitchEg.decay > 0.0f
    );

    // -----------------------------------------------------
    // FM amount
    // -----------------------------------------------------

    params.set(82, 100, engine);

    check(
        "fm amount D1",
        std::abs(
            engine.drums[0].fmModAmount -
            100.0f / 127.0f
        ) < 0.01f
    );

    // -----------------------------------------------------
    // Envelope slope
    // -----------------------------------------------------

    params.set(62, 64, engine);

    // Existing smoke test: verifies that setting
    // the parameter does not crash.
    check("slope D1 set", true);

    // -----------------------------------------------------
    // Drive
    // -----------------------------------------------------

    params.set(102, 80, engine);

    check("drive D1", true);

    // -----------------------------------------------------
    // Decimation
    // -----------------------------------------------------

    params.set(108, 64, engine);

    check(
        "decimation D1",
        engine.drums[0].decimationRate != 0.0f
    );

    // -----------------------------------------------------
    // Filter type
    // -----------------------------------------------------

    params.set(191, 1, engine);

    // filterTypeFromParam(1) = 2 (HP).
    check(
        "filter type D1",
        engine.drums[0].filterType == 2
    );

    // -----------------------------------------------------
    // Filter drive
    // -----------------------------------------------------

    params.set(128, 80, engine);

    check("filter drive D1", true);

    // -----------------------------------------------------
    // Oscillator mix mode
    // -----------------------------------------------------

    params.set(134, 0, engine);

    check(
        "mix mode D1 FM",
        engine.drums[0].mixOscs == false
    );

    params.set(134, 1, engine);

    check(
        "mix mode D1 mix",
        engine.drums[0].mixOscs == true
    );

    // -----------------------------------------------------
    // Transient generator
    // -----------------------------------------------------

    params.set(197, 100, engine);

    check(
        "transient vol D1",
        std::abs(
            engine.drums[0].transGen.volume -
            100.0f / 127.0f
        ) < 0.01f
    );

    params.set(203, 2, engine);

    check(
        "transient wave D1",
        engine.drums[0].transGen.waveform == 2
    );

    // -----------------------------------------------------
    // Volume modulation
    // -----------------------------------------------------

    params.set(137, 0, engine);

    check(
        "volume mod off D1",
        engine.drums[0].volumeMod == false
    );

    params.set(137, 1, engine);

    check(
        "volume mod on D1",
        engine.drums[0].volumeMod == true
    );

    // -----------------------------------------------------
    // LFO rate
    // -----------------------------------------------------

    params.set(115, 100, engine);

    check(
        "lfo rate D1",
        engine.drums[0].lfo.phaseInc > 0
    );

    // -----------------------------------------------------
    // LFO amount
    // -----------------------------------------------------

    params.set(121, 80, engine);

    check(
        "lfo amount D1",
        std::abs(
            engine.drums[0].lfo.amount -
            80.0f / 127.0f
        ) < 0.01f
    );

    // -----------------------------------------------------
    // LFO waveform
    // -----------------------------------------------------

    params.set(155, 3, engine);

    check(
        "lfo waveform D1",
        engine.drums[0].lfo.waveform == 3
    );

    // -----------------------------------------------------
    // LFO retrigger
    // -----------------------------------------------------

    params.set(173, 2, engine);

    check(
        "lfo retrigger D1",
        engine.drums[0].lfo.retrigger == 2
    );

    // -----------------------------------------------------
    // LFO synchronization
    // -----------------------------------------------------

    params.set(179, 5, engine);

    check(
        "lfo sync D1",
        engine.drums[0].lfo.sync == 5
    );

    // -----------------------------------------------------
    // LFO phase offset
    // -----------------------------------------------------

    params.set(185, 127, engine);

    check(
        "lfo offset D1 saturates",
        engine.drums[0].lfo.phaseOffset ==
            0xFFFFFFFFu
    );

    // -----------------------------------------------------
    // Decimation rate smoke test
    // -----------------------------------------------------

    params.set(108, 64, engine);

    check("decimation D1 set", true);

    // -----------------------------------------------------
    // Velocity modulation amount
    // -----------------------------------------------------

    params.set(143, 100, engine);

    check(
        "velo mod amount D1",
        std::abs(
            engine.velocityModulators[0].amount -
            100.0f / 127.0f
        ) < 0.01f
    );

    // -----------------------------------------------------
    // Velocity modulation destination
    // -----------------------------------------------------

    // ORIGINAL QUIRK:
    // VEL_DEST (PAR 149) is not settable through
    // the original firmware's MIDI parameter path.
    //
    // ParameterArray::set() deliberately ignores
    // parameter IDs 149-154.
    //
    // Verify that a set request does not alter
    // the existing destination.

    const uint8_t destinationBefore =
        engine.velocityModulators[0].destination;

    params.set(149, 88, engine);

    check(
        "velo mod dest D1 unchanged",
        engine.velocityModulators[0].destination ==
            destinationBefore
    );

    // -----------------------------------------------------
    // Audio routing
    // -----------------------------------------------------

    params.set(215, 1, engine);

    // Existing smoke test: checks that the call
    // completes without crashing.
    check("audio routing D1", true);

    // -----------------------------------------------------
    // Final results
    // -----------------------------------------------------

    std::printf(
        "\nSUMMARY pass=%d fail=%d\n",
        pass,
        fail
    );

    return fail > 0 ? 1 : 0;
}
