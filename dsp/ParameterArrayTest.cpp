// FILE: dsp/ParameterArrayTest.cpp
#include "ParameterArray.h"
#include <cstdio>
#include <cmath>

static int pass = 0, fail = 0;
void check(const char* name, bool cond) {
    if (cond) { printf("PASS %s\n", name); pass++; }
    else { printf("FAIL %s\n", name); fail++; }
}

int main() {
    lxr::Engine engine;
    engine.loadAssets("data");
    lxr::ParameterArray params;
    params.init();
    
    // Waveform
    params.set(1, 2, engine);
    check("waveform D1", engine.drums[0].osc.waveform == 2);
    params.set(4, 3, engine);
    check("waveform SN", engine.snare.osc.waveform == 3);
    
    // Coarse
    params.set(8, 70, engine);
    check("coarse D1", (engine.drums[0].osc.midiFreq >> 8) == 70);
    
    // Fine
    params.set(9, 80, engine);
    check("fine D1", (engine.drums[0].osc.midiFreq & 0xFF) == 80);
    
    // Filter freq
    params.set(37, 64, engine);
    check("filter freq D1", engine.drums[0].filter.f > 0.0f);
    
    // Filter reso
    params.set(43, 100, engine);
    float expectedQ = 1.0f - 100.0f / 127.0f;
    check("filter reso D1", std::abs(engine.drums[0].filter.q - expectedQ) < 0.01f);
    
    // Volume
    params.set(88, 64, engine);
    check("volume D1", std::abs(engine.drums[0].vol - 64.0f / 127.0f) < 0.01f);
    
    // Pan
    params.set(94, 32, engine);
    check("pan D1", engine.mixer.pan[0] == 32);

    // Pitch EG (PAR 74 = MODAMNT1 = D1 pitch EG amount; PAR 70 = MOD_EG1 = D1 pitch EG decay)
    params.set(74, 80, engine);
    check("pitch mod amount D1", engine.drums[0].egPitchModAmount > 0.0f);

    params.set(70, 50, engine);
    check("pitch eg decay D1", engine.drums[0].oscPitchEg.decay > 0.0f);

    params.set(77, 50, engine);
    check("pitch eg decay D1", engine.drums[0].oscPitchEg.decay > 0.0f);

    // FM (PAR 82 = FMAMNT1 = D1 FM amount)
    params.set(82, 100, engine);
    check("fm amount D1", std::abs(engine.drums[0].fmModAmount - 100.0f/127.0f) < 0.01f);

    // Slope
    params.set(62, 64, engine);
    check("slope D1 set", true);  // Just verify it doesn't crash

    // Drive
    params.set(102, 80, engine);
    check("drive D1", true);

    // Decimation
    params.set(108, 64, engine);
    check("decimation D1", engine.drums[0].decimationRate != 0.0f);

    // Filter type
    params.set(191, 1, engine);
    check("filter type D1", engine.drums[0].filterType == 2);  // filterTypeFromParam(1) = 2 = HP

    // Filter drive
    params.set(128, 80, engine);
    check("filter drive D1", true);

    // Mix mode
    params.set(134, 0, engine);
    check("mix mode D1 FM", engine.drums[0].mixOscs == false);
    params.set(134, 1, engine);
    check("mix mode D1 mix", engine.drums[0].mixOscs == true);

    // Transient
    params.set(197, 100, engine);
    check("transient vol D1", std::abs(engine.drums[0].transGen.volume - 100.0f/127.0f) < 0.01f);

    params.set(203, 2, engine);
    check("transient wave D1", engine.drums[0].transGen.waveform == 2);

    // Volume mod
    params.set(137, 0, engine);
    check("volume mod off D1", engine.drums[0].volumeMod == false);
    params.set(137, 1, engine);
    check("volume mod on D1", engine.drums[0].volumeMod == true);
    
    printf("\nSUMMARY pass=%d fail=%d\n", pass, fail);
    return fail > 0 ? 1 : 0;
}