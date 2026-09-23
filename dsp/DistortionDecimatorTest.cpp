// FILE: dsp/DistortionDecimatorTest.cpp
#include "Distortion.h"
#include "Decimator.h"
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

int main() {
    // ========================================================================
    // Distortion tests
    // ========================================================================
    {
        lxr::Distortion dist;
        dist.init();
        dist.setShape(0);
        check("setShape(0)", dist.shape, 0.0f, 1e-5f);
    }
    {
        lxr::Distortion dist;
        dist.init();
        dist.setShape(64);
        // 64/128 = 0.5. 2*0.5 / (1-0.5) = 2.0
        check("setShape(64)", dist.shape, 2.0f, 1e-5f);
    }
    {
        lxr::Distortion dist;
        dist.init();
        dist.setShape(127);
        // 127/128 = 0.9921875. 2*0.9921875 / (1-0.9921875) = 254.0
        check("setShape(127)", dist.shape, 254.0f, 1e-5f);
    }
    {
        lxr::Distortion dist;
        dist.init();
        dist.setShape(64); // shape = 2.0
        int16_t buf[4] = {16384, -16384, 32767, -32768};
        dist.calcBlock(buf, 4);
        
        // buf[0] = 16384. x = 0.500015259. y = 3*0.500015 / (1+2*0.500015) = 0.7500114. * 32767 = 24575.6 -> 24575
        check_i("calcBlock shape=2 buf[0]", buf[0], 24575);
        // buf[1] = -16384. x = -0.500015. y = -0.7500114. * 32767 = -24575.6 -> -24575
        check_i("calcBlock shape=2 buf[1]", buf[1], -24575);
        // buf[2] = 32767. x = 1.0. y = 1.0. * 32767 = 32767
        check_i("calcBlock shape=2 buf[2]", buf[2], 32767);
        // buf[3] = -32768. x = -1.0000305. y = -1.00001017. * 32767 = -32767.33 -> -32767
        // ORIGINAL QUIRK: -32768 input becomes -32767 due to float division and truncation.
        check_i("calcBlock shape=2 buf[3] (-32768 quirk)", buf[3], -32767);
    }
    {
        lxr::Distortion dist;
        dist.init();
        dist.setShape(64); // shape = 2.0
        // x = 0.5, y = (1+2)*0.5 / (1+2*0.5) = 1.5 / 2.0 = 0.75
        check("calcSampleFloat(0.5)", dist.calcSampleFloat(0.5f), 0.75f, 1e-5f);
    }

    // ========================================================================
    // Decimator tests
    // ========================================================================
    {
        lxr::Decimator dec;
        dec.init();
        check("decimator init cnt", dec.cnt, 0.0f, 1e-5f);
        check_i("decimator init held", dec.heldSample, 0);
    }
    {
        lxr::Decimator dec;
        dec.init();
        int16_t buf[4] = {100, 200, 300, 400};
        dec.processBlock(buf, 4, 1.0f, 1.0f);
        check_i("decimator rate=1.0 buf[0]", buf[0], 100);
        check_i("decimator rate=1.0 buf[1]", buf[1], 200);
        check_i("decimator rate=1.0 buf[2]", buf[2], 300);
        check_i("decimator rate=1.0 buf[3]", buf[3], 400);
    }
    {
        lxr::Decimator dec;
        dec.init();
        int16_t buf[4] = {100, 200, 300, 400};
        dec.processBlock(buf, 4, 0.5f, 1.0f);
        check_i("decimator rate=0.5 buf[0]", buf[0], 0);
        check_i("decimator rate=0.5 buf[1]", buf[1], 200);
        check_i("decimator rate=0.5 buf[2]", buf[2], 200);
        check_i("decimator rate=0.5 buf[3]", buf[3], 400);
    }
    {
        lxr::Decimator dec;
        dec.init();
        dec.heldSample = 999;
        int16_t buf[4] = {100, 200, 300, 400};
        dec.processBlock(buf, 4, 0.0f, 1.0f);
        check_i("decimator rate=0.0 buf[0]", buf[0], 999);
        check_i("decimator rate=0.0 buf[3]", buf[3], 999);
    }
    {
        lxr::Decimator dec;
        dec.init();
        int16_t buf[4] = {100, 200, 300, 400};
        dec.processBlock(buf, 4, 0.5f, 0.5f);
        check_i("decimator rate=0.25 buf[0]", buf[0], 0);
        check_i("decimator rate=0.25 buf[2]", buf[2], 0);
        check_i("decimator rate=0.25 buf[3]", buf[3], 400);
    }

    printf("SUMMARY pass=%d fail=%d\n", pass, fail);
    return fail > 0 ? 1 : 0;
}