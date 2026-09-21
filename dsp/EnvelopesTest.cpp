// FILE: dsp/EnvelopesTest.cpp
#include "Envelopes.h"
#include <cstdio>
#include <cmath>
#include <cfloat>

static int pass = 0, fail = 0, warn = 0;

static void check(const char* name, float actual, float expected, float tol) {
    if (std::isnan(expected)) {
        if (std::isnan(actual)) { printf("PASS %s\n", name); pass++; }
        else { printf("FAIL %s: expected NaN, got %g\n", name, actual); fail++; }
    } else if (std::isinf(expected)) {
        if (std::isinf(actual) && (expected > 0) == (actual > 0)) { printf("PASS %s\n", name); pass++; }
        else { printf("FAIL %s: expected %g, got %g\n", name, expected, actual); fail++; }
    } else {
        float diff = std::fabs(actual - expected);
        if (diff <= tol) { printf("PASS %s\n", name); pass++; }
        else if (diff <= tol * 10) { printf("WARN %s: expected %.8g, got %.8g (diff %g)\n", name, expected, actual, diff); warn++; }
        else { printf("FAIL %s: expected %.8g, got %.8g (diff %g)\n", name, expected, actual, diff); fail++; }
    }
}

int main() {
    // ========================================================================
    // AmpEg mapping tests
    // ========================================================================
    {
        AmpEg eg;
        eg.init();
        eg.setAttack(127, false);
        check("AmpEg attack(127) == 0", eg.attack, 0.0f, 1e-7f);
    }
    {
        AmpEg eg;
        eg.init();
        eg.setAttack(0, false);
        check("AmpEg attack(0) == 1", eg.attack, 1.0f, 1e-5f);
    }
    {
        AmpEg eg;
        eg.init();
        eg.setAttack(64, false);
        check("AmpEg attack(64) ~ 0.00492", eg.attack, 0.004920f, 1e-5f);
    }
    {
        AmpEg eg;
        eg.init();
        eg.setAttack(1, false);
        check("AmpEg attack(1) ~ 0.3877", eg.attack, 0.38769209f, 1e-4f);
    }
    {
        AmpEg eg;
        eg.init();
        eg.setDecay(127, false);
        check("AmpEg decay(127) == 0", eg.decay, 0.0f, 1e-7f);
    }
    {
        AmpEg eg;
        eg.init();
        eg.setDecay(64, false);
        check("AmpEg decay(64) ~ 0.000492", eg.decay, 0.000492f, 1e-5f);
    }
    {
        AmpEg eg;
        eg.init();
        eg.setDecay(1, false);
        check("AmpEg decay(1) ~ 0.0593", eg.decay, 0.059293449f, 1e-4f);
    }
    {
        AmpEg eg;
        eg.init();
        eg.setSlope(64);
        // amount = (64/127 - 0.5)*1.999 = 0.007870...
        // slope = 2*0.007870/(1-0.007870) ~ 0.015864
        check("AmpEg slope(64) ~ 0.01586", eg.slope, 0.015864f, 1e-4f);
        check("AmpEg invSlope(64) ~ -0.01562", eg.invSlope, -0.015617f, 1e-4f);
    }
    {
        AmpEg eg;
        eg.init();
        eg.setSlope(0);
        // amount = -0.9995; slope = -1.999/1.9995 ~ -0.99975
        check("AmpEg slope(0) ~ -0.99975", eg.slope, -0.999750f, 1e-3f);
    }
    {
        AmpEg eg;
        eg.init();
        eg.setSlope(127);
        // amount = 0.9995; slope = 1.999/0.0005 = 3998.0
        check("AmpEg slope(127) ~ 3998", eg.slope, 3998.0f, 1.0f);
    }
    {
        AmpEg eg;
        eg.init();
        eg.setAttack(64, true); // sync divides by 16
        float expected = 0.004920f / 16.0f;
        check("AmpEg attack(64,sync) ~ 0.000308", eg.attack, expected, 1e-5f);
    }

    // ========================================================================
    // AmpEg state machine tests
    // ========================================================================
    {
        AmpEg eg;
        eg.init();
        check("AmpEg stopped calc == 0", eg.calc(), 0.0f, 1e-7f);
    }
    {
        AmpEg eg;
        eg.init();
        eg.setAttack(127, false); // attack step = 0
        eg.setDecay(64, false);
        eg.setSlope(64);
        eg.trigger();
        // state = A, attack = 0, so value stays 0, output = warp(0, invSlope) = 0
        float v = eg.calc();
        check("AmpEg attack=0 first calc ~ 0", v, 0.0f, 1e-5f);
    }
    {
        AmpEg eg;
        eg.init();
        eg.setAttack(0, false); // attack step = 1.0 (instant)
        eg.setDecay(64, false);
        eg.setSlope(64);
        eg.trigger();
        // state = A, value += 1.0 -> 1.0, transitions to D
        float v = eg.calc();
        check("AmpEg instant attack -> D", v, 1.0f, 1e-5f);
        check("AmpEg state after instant attack", (float)eg.state, 2.0f, 0.5f); // EG_D = 2
    }
    {
        AmpEg eg;
        eg.init();
        eg.setRepeat(2);
        eg.setAttack(0, false); // attack step = 1.0 (instant decay in repeat)
        eg.setDecay(64, false);
        eg.setSlope(64);
        eg.trigger();
        check("AmpEg repeat trigger state", (float)eg.state, 3.0f, 0.5f); // EG_REPEAT = 3
        check("AmpEg repeat trigger value", eg.value, 1.0f, 1e-7f);
    }

    // ========================================================================
    // PitchDecayEg tests
    // ========================================================================
    {
        PitchDecayEg eg;
        eg.init();
        eg.setDecay(64);
        check("PitchEg decay(64) ~ 0.00492", eg.decay, 0.004920f, 1e-5f);
    }
    {
        PitchDecayEg eg;
        eg.init();
        eg.setDecay(127);
        check("PitchEg decay(127) == 0", eg.decay, 0.0f, 1e-7f);
    }
    {
        PitchDecayEg eg;
        eg.init();
        eg.setSlope(127);
        // ORIGINAL QUIRK: amount = 1.0, slope = 2/0 = +inf
        check("PitchEg slope(127) == +inf", eg.slope, INFINITY, 0.0f);
    }
    {
        PitchDecayEg eg;
        eg.init();
        eg.setSlope(64);
        // amount = (64/127-0.5)*2 ~ 0.007874; slope ~ 0.015873
        check("PitchEg slope(64) ~ 0.01587", eg.slope, 0.015873f, 1e-4f);
    }
    {
        PitchDecayEg eg;
        eg.init();
        eg.setDecay(64);
        eg.setSlope(64);
        eg.trigger();
        check("PitchEg trigger value", eg.value, 1.0f, 1e-7f);
        float v = eg.calc();
        // value = 1.0 - 0.00492 = 0.99508; warp(0.99508, 0.01587) ~ 0.99516
        check("PitchEg first calc ~ 0.995", v, 0.995f, 1e-2f);
    }

    // ========================================================================
    // SnapEg tests
    // ========================================================================
    {
        SnapEg eg;
        eg.init();
        check("SnapEg init value", eg.value, 0.0f, 1e-7f);
    }
    {
        SnapEg eg;
        eg.init();
        eg.trigger();
        check("SnapEg trigger value", eg.value, 1.0f, 1e-7f);
        float v = eg.calc(1.0f);
        check("SnapEg first calc == 24", v, 24.0f, 1e-5f);
        check("SnapEg value after calc", eg.value, 0.8f, 1e-5f);
    }
    {
        SnapEg eg;
        eg.init();
        eg.trigger();
        eg.calc(1.0f); // value -> 0.8
        float v = eg.calc(1.0f);
        // ret = 0.8*0.8*24 = 15.36; value -> 0.6
        check("SnapEg second calc ~ 15.36", v, 15.36f, 1e-3f);
        check("SnapEg value after 2nd", eg.value, 0.6f, 1e-5f);
    }
    {
        SnapEg eg;
        eg.init();
        eg.trigger();
        for (int i = 0; i < 5; i++) eg.calc(1.0f); // value = 1 - 5*0.2 = 0
        float v = eg.calc(1.0f);
        check("SnapEg after expiry == 0", v, 0.0f, 1e-7f);
    }

    // ========================================================================
    printf("SUMMARY pass=%d fail=%d warn=%d\n", pass, fail, warn);
    return fail > 0 ? 1 : 0;
}