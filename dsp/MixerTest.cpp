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
        lxr::Mixer mixer; mixer.init();
        check_u8("init routing[0]", mixer.routing[0], 0);
        check_u8("init pan[0]", mixer.pan[0], 64);
        check("init decimationRate[0]", mixer.decimationRate[0], 1.0f, 1e-5f);
        check("init decimationRate[6] (ALL)", mixer.decimationRate[6], 1.0f, 1e-5f);
    }
    printf("--- SQRT LUT ---\n");
    {
        lxr::Mixer mixer; mixer.init();
        check("sqrtLut[0]", mixer.sqrtLut[0], 0.0f, 1e-5f);
        check("sqrtLut[127]", mixer.sqrtLut[127], 1.0f, 1e-5f);
        check("sqrtLut[64]", mixer.sqrtLut[64], 0.70986f, 0.001f);
        check("sqrtLut[32]", mixer.sqrtLut[32], 0.50196f, 0.001f);
    }
    printf("--- SATURATING ADD ---\n");
    {
        check_i("satAdd normal", lxr::saturatingAdd(100, 200), 300);
        check_i("satAdd overflow", lxr::saturatingAdd(30000, 10000), 32767);
        check_i("satAdd underflow", lxr::saturatingAdd(-30000, -10000), -32768);
    }
    printf("--- PAN ---\n");
    {
        lxr::Mixer mixer; mixer.init();
        int16_t buf[4] = {1000, 2000, 3000, 4000};
        int16_t* voiceBufs[6] = {buf, nullptr, nullptr, nullptr, nullptr, nullptr};
        int16_t st1L[4], st1R[4], st2L[4], st2R[4];
        mixer.setPan(0, 0); mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("pan left L[0]", st1L[0], 1000); check_i("pan left R[0]", st1R[0], 0);
        mixer.setPan(0, 127); mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("pan right L[0]", st1L[0], 0); check_i("pan right R[0]", st1R[0], 1000);
        mixer.setPan(0, 64); mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("pan center L[0]", st1L[0], 704); check_i("pan center R[0]", st1R[0], 709);
    }
    printf("--- ROUTING ---\n");
    {
        lxr::Mixer mixer; mixer.init();
        int16_t buf[4] = {1000, 2000, 3000, 4000};
        int16_t* voiceBufs[6] = {buf, nullptr, nullptr, nullptr, nullptr, nullptr};
        int16_t st1L[4], st1R[4], st2L[4], st2R[4];
        mixer.setRouting(0, lxr::ROUTE_ST2); mixer.setPan(0, 0);
        mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("route St2 st1L[0]", st1L[0], 0); check_i("route St2 st2L[0]", st2L[0], 1000);
        mixer.setRouting(0, lxr::ROUTE_ST1_R);
        mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("route St1_R st1L[0]", st1L[0], 0); check_i("route St1_R st1R[0]", st1R[0], 1000);
    }
    printf("--- MULTI-VOICE SUM ---\n");
    {
        lxr::Mixer mixer; mixer.init();
        int16_t buf0[4] = {1000, 1000, 1000, 1000};
        int16_t buf1[4] = {500, 500, 500, 500};
        int16_t* voiceBufs[6] = {buf0, buf1, nullptr, nullptr, nullptr, nullptr};
        int16_t st1L[4], st1R[4], st2L[4], st2R[4];
        mixer.setPan(0, 0); mixer.setPan(1, 0);
        mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("sum st1L[0]", st1L[0], 1500); check_i("sum st1R[0]", st1R[0], 0);
        buf0[0]=30000; buf1[0]=10000;
        mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("saturating sum st1L[0]", st1L[0], 32767);
    }
    printf("--- DECIMATION ---\n");
    {
        lxr::Mixer mixer; mixer.init();
        int16_t buf[4] = {100, 200, 300, 400};
        int16_t* voiceBufs[6] = {buf, nullptr, nullptr, nullptr, nullptr, nullptr};
        int16_t st1L[4], st1R[4], st2L[4], st2R[4];
        mixer.setDecimationRate(0, 0.5f); mixer.setPan(0, 0);
        mixer.processBlock(voiceBufs, st1L, st1R, st2L, st2R, 4);
        check_i("decim st1L[0]", st1L[0], 0); check_i("decim st1L[1]", st1L[1], 200);
        check_i("decim st1L[2]", st1L[2], 200); check_i("decim st1L[3]", st1L[3], 400);
    }
    printf("\nSUMMARY pass=%d fail=%d\n", pass, fail);
    return fail > 0 ? 1 : 0;
}