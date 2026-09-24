// FILE: dsp/LfoModulationTest.cpp
#include "Lfo.h"
#include "Modulation.h"
#include <cstdio>
#include <cmath>

static int pass = 0, fail = 0;

static void check(const char* name, float actual, float expected, float tol) {
    float diff = std::fabs(actual - expected);
    if (diff <= tol) { printf("PASS %s\n", name); pass++; }
    else { printf("FAIL %s: expected %g, got %g (diff %g)\n", name, expected, actual, diff); fail++; }
}

static void check_u32(const char* name, uint32_t actual, uint32_t expected) {
    if (actual == expected) { printf("PASS %s\n", name); pass++; }
    else { printf("FAIL %s: expected %u, got %u\n", name, expected, actual); fail++; }
}

static void check_u8(const char* name, uint8_t actual, uint8_t expected) {
    if (actual == expected) { printf("PASS %s\n", name); pass++; }
    else { printf("FAIL %s: expected %u, got %u\n", name, expected, actual); fail++; }
}

int main() {
    // LFO tests
    printf("--- LFO ---\n");
    {
        lxr::Lfo lfo;
        lfo.init();
        check("lfo init phase", (float)lfo.phase, 0.0f, 1e-5f);
        check_u8("lfo init waveform", lfo.waveform, 0);
        check_u8("lfo init retrigger", lfo.retrigger, 0);
    }
    {
        // Test frequency mapping: ((v+1)/128)^3 x 200 Hz
        // v=0: (1/128)^3 * 200 = 0.0000953674 Hz
        check("lfo freq v=0", lxr::lfoFrequencyFromMidi(0), 0.0000953674f, 1e-7f);
        // v=64: (65/128)^3 * 200 = 26.1903 Hz
        check("lfo freq v=64", lxr::lfoFrequencyFromMidi(64), 26.1903f, 0.01f);
        // v=127: (128/128)^3 * 200 = 200 Hz
        check("lfo freq v=127", lxr::lfoFrequencyFromMidi(127), 200.0f, 0.01f);
    }
    {
        // Test phase offset mapping: v/127.f x 0xffffffff
        check_u32("lfo offset v=0", lxr::lfoPhaseOffsetFromMidi(0), 0u);
        // v=64: 64/127 * 2^32 = 2164392960
        check_u32("lfo offset v=64", lxr::lfoPhaseOffsetFromMidi(64), 2164392960u);
        // ORIGINAL QUIRK: v=127 saturates to 0xFFFFFFFF
        check_u32("lfo offset v=127 saturated", lxr::lfoPhaseOffsetFromMidi(127), 0xFFFFFFFFu);
    }
    {
        // Test LFO waveforms
        lxr::Lfo lfo;
        lfo.init();
        lfo.setRate(1.0f);  // 1 Hz
        lfo.waveform = lxr::LFO_SAW_UP;
        
        // Tick through one cycle
        for (int i = 0; i < 44003; i++) {  // ~1 second at 44kHz
            lfo.tick();
        }
        // After one full cycle, output should be near 1.0 (end of ramp)
        check("lfo saw up after 1 cycle", lfo.output, 1.0f, 0.01f);
    }
    {
        // Test retrigger
        lxr::Lfo lfo;
        lfo.init();
        lfo.retrigger = 1;  // retrigger on voice 1
        lfo.phaseOffset = 0x80000000u;
        lfo.phase = 0;
        
        lfo.trigger(1);  // should reset phase
        check_u32("lfo retrigger phase reset", lfo.phase, 0x80000000u);
        
        lfo.trigger(2);  // should NOT reset
        check_u32("lfo no retrigger", lfo.phase, 0x80000000u);
    }
    
    // Modulation tests
    printf("--- MODULATION ---\n");
    {
        lxr::ModTarget target;
        target.baseValue = 100.0f;
        target.reset();
        check("mod target reset", target.multiplier, 1.0f, 1e-5f);
        check("mod target getValue", target.getValue(), 100.0f, 1e-5f);
    }
    {
        lxr::ModTarget target;
        target.baseValue = 100.0f;
        target.reset();
        // Apply modulation: mod=0.5, amount=0.5
        // multiplier *= (1 + 0.5 * (0.5 - 1)) = 1 + 0.5 * (-0.5) = 0.75
        target.applyModulation(0.5f, 0.5f);
        check("mod target apply", target.multiplier, 0.75f, 1e-5f);
        check("mod target final value", target.getValue(), 75.0f, 1e-5f);
    }
    {
        lxr::VelocityModulator velMod;
        velMod.init();
        velMod.updateVelocity(64);
        check("vel mod value", velMod.modValue, 64.0f / 127.0f, 1e-5f);
    }
    {
        lxr::ModulationSystem modSys;
        modSys.init();
        modSys.updateVelocity(0, 100);
        check("mod sys velocity", modSys.velocityMods[0].modValue, 100.0f / 127.0f, 1e-5f);
    }
    
    printf("\nSUMMARY pass=%d fail=%d\n", pass, fail);
    return fail > 0 ? 1 : 0;
}