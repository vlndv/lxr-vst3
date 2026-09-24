// FILE: dsp/MixerTest.cpp
#include "Mixer.h"
#include <cstdio>
#include <cmath>

static int pass = 0, fail = 0;

static void check(const char* name, float actual, float expected, float tol) {
    float diff = std::fabs(actual - expected);
    if (diff <= tol) { printf("PASS %s\n", name); pass++; }
    else { printf("FAIL %s: expected %g, got %g (diff %g)\n", name, expected, actual, diff); fail++; }
}

static void check_i(const char* name, int32_t actual, int32_t expected) {
    if (actual == expected) { printf("PASS %s\n", name); pass++; }
    else { printf("FAIL %s: expected %d, got %d\n", name, expected, actual); fail++; }
}

static void check_u8(const char* name, uint8_t actual, uint8_t expected) {
    if (actual == expected) { printf("PASS %s\n", name); pass++; }
    else { printf("FAIL %s: expected %u, got %u\n", name, expected, actual); fail++; }
}

int main() {
    printf("--- MIXER INIT ---\n");
    {
        lxr::Mixer mixer;
        mixer.init();
        check_u8("init routing[0]", mixer.routing[0], 0); // ROUTE_ST1
        check_u8("init pan[0]", mixer.pan[0], 64); // center
        check("init decimationRate[0]", mixer.decimationRate[0], 1.0f, 1e-5f);
        check("init decimationRate[6] (ALL)", mixer.decimationRate[6], 1.0f, 1e-5f);
        check("init mute[0]", (float)mixer.mute[0], 0.0f, 0.1f);
    }
    
    printf("--- SQRT LUT ---\n");
    {
        lxr::Mixer mixer;
        mixer.init();
        // sqrtLut[0] = sqrt(0/127) = 0
        check("sqrtLut[0]", mixer.sqrtLut[0], 0.0f, 1e-5f);
        // sqrtLut[127] = sqrt(127/127) = 1
        check("sqrtLut[127]", mixer.sqrtLut[127], 1.0f, 1e-5f);
        // sqrtLut[64] = sqrt(64/127) = 0.70986...
        check("sqrtLut[64]", mixer.sqrtLut[64], 0.70986f, 0.001f);
        // sqrtLut[32] = sqrt(32/127) = 0.50196...
        check("sqrtLut[32]", mixer.sqrtLut[32], 0.50196f, 0.001f);
    }
    
    printf("--- SATURATING ADD ---\n");
    {
        check_i("satAdd normal", lxr::saturatingAdd(100, 200), 300);
        check_i("satAdd overflow", lxr::saturatingAdd(30000, 10000), 32767);
        check_i("satAdd underflow", lxr::saturatingAdd(-30000, -10000), -32768);
        check_i("satAdd negative", lxr::saturatingAdd(-100, -200), -300);
    }
    
    printf("--- PAN ---\n");
    {
        lxr::Mixer mixer;
        mixer.init();
        int16_t buf[4] = {1000, 2000, 3000, 4000};
        int16_t* voiceBufs[6] = {buf, nullptr, nullptr, nullptr, nullptr, nullptr};
        int16_t st1L[4], st1R[4], st2L[4], st2R[4];
        
        // Pan hard left (pan=0): L=sqrt(127/127)=1.0, R=sqrt(0/127)=0.0
        mixer.setPan(0, 0);
        mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("pan left L[0]", st1L[0], 1000);
        check_i("pan left R[0]", st1R[0], 0);
    }
    {
        lxr::Mixer mixer;
        mixer.init();
        int16_t buf[4] = {1000, 2000, 3000, 4000};
        int16_t* voiceBufs[6] = {buf, nullptr, nullptr, nullptr, nullptr, nullptr};
        int16_t st1L[4], st1R[4], st2L[4], st2R[4];
        
        // Pan hard right (pan=127): L=sqrt(0/127)=0.0, R=sqrt(127/127)=1.0
        mixer.setPan(0, 127);
        mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("pan right L[0]", st1L[0], 0);
        check_i("pan right R[0]", st1R[0], 1000);
    }
    {
        lxr::Mixer mixer;
        mixer.init();
        int16_t buf[4] = {1000, 2000, 3000, 4000};
        int16_t* voiceBufs[6] = {buf, nullptr, nullptr, nullptr, nullptr, nullptr};
        int16_t st1L[4], st1R[4], st2L[4], st2R[4];
        
        // Pan center (pan=64): L=sqrt(63/127)=0.7043, R=sqrt(64/127)=0.7099
        mixer.setPan(0, 64);
        mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("pan center L[0]", st1L[0], 704);
        check_i("pan center R[0]", st1R[0], 709);
    }
    
    printf("--- ROUTING ---\n");
    {
        lxr::Mixer mixer;
        mixer.init();
        int16_t buf[4] = {1000, 2000, 3000, 4000};
        int16_t* voiceBufs[6] = {buf, nullptr, nullptr, nullptr, nullptr, nullptr};
        int16_t st1L[4], st1R[4], st2L[4], st2R[4];
        
        // Route to St2
        mixer.setRouting(0, lxr::ROUTE_ST2);
        mixer.setPan(0, 0); // hard left for easy verification
        mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("route St2 st1L[0]", st1L[0], 0); // St1 should be empty
        check_i("route St2 st2L[0]", st2L[0], 1000); // St2 should have signal
    }
    {
        lxr::Mixer mixer;
        mixer.init();
        int16_t buf[4] = {1000, 2000, 3000, 4000};
        int16_t* voiceBufs[6] = {buf, nullptr, nullptr, nullptr, nullptr, nullptr};
        int16_t st1L[4], st1R[4], st2L[4], st2R[4];
        
        // Route to St1_R only (mono right)
        mixer.setRouting(0, lxr::ROUTE_ST1_R);
        mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("route St1_R st1L[0]", st1L[0], 0);
        check_i("route St1_R st1R[0]", st1R[0], 1000);
    }
    
    printf("--- MUTE ---\n");
    {
        lxr::Mixer mixer;
        mixer.init();
        int16_t buf[4] = {1000, 2000, 3000, 4000};
        int16_t* voiceBufs[6] = {buf, nullptr, nullptr, nullptr, nullptr, nullptr};
        int16_t st1L[4], st1R[4], st2L[4], st2R[4];
        
        mixer.setMute(0, true);
        mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("mute st1L[0]", st1L[0], 0);
        check_i("mute st1R[0]", st1R[0], 0);
    }
    
    printf("--- MULTI-VOICE SUM ---\n");
    {
        lxr::Mixer mixer;
        mixer.init();
        int16_t buf0[4] = {1000, 1000, 1000, 1000};
        int16_t buf1[4] = {500, 500, 500, 500};
        int16_t* voiceBufs[6] = {buf0, buf1, nullptr, nullptr, nullptr, nullptr};
        int16_t st1L[4], st1R[4], st2L[4], st2R[4];
        
        // Both voices to St1, hard left
        mixer.setPan(0, 0);
        mixer.setPan(1, 0);
        mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("sum st1L[0]", st1L[0], 1500);
        check_i("sum st1R[0]", st1R[0], 0);
    }
    {
        lxr::Mixer mixer;
        mixer.init();
        int16_t buf0[4] = {30000, 30000, 30000, 30000};
        int16_t buf1[4] = {10000, 10000, 10000, 10000};
        int16_t* voiceBufs[6] = {buf0, buf1, nullptr, nullptr, nullptr, nullptr};
        int16_t st1L[4], st1R[4], st2L[4], st2R[4];
        
        // Saturating sum test
        mixer.setPan(0, 0);
        mixer.setPan(1, 0);
        mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("saturating sum st1L[0]", st1L[0], 32767);
    }
    
    printf("--- DECIMATION ---\n");
    {
        lxr::Mixer mixer;
        mixer.init();
        int16_t buf[4] = {100, 200, 300, 400};
        int16_t* voiceBufs[6] = {buf, nullptr, nullptr, nullptr, nullptr, nullptr};
        int16_t st1L[4], st1R[4], st2L[4], st2R[4];
        
        // Decimation rate 0.5: S&H every other sample
        mixer.setDecimationRate(0, 0.5f);
        mixer.setPan(0, 0); // hard left
        mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        // After decimation: buf = {0, 200, 200, 400} (held samples)
        // Then pan left: L = buf * 1.0
        check_i("decim st1L[0]", st1L[0], 0);
        check_i("decim st1L[1]", st1L[1], 200);
        check_i("decim st1L[2]", st1L[2], 200);
        check_i("decim st1L[3]", st1L[3], 400);
    }
    
    printf("\nSUMMARY pass=%d fail=%d\n", pass, fail);
    return fail > 0 ? 1 : 0;
}