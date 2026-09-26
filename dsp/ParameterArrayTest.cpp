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
    
    printf("\nSUMMARY pass=%d fail=%d\n", pass, fail);
    return fail > 0 ? 1 : 0;
}