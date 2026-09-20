// dsp/ParamMapEnvTest.cpp (expected values from the original C, gcc -O0 -ffp-contract=off)
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "ParamMapEnv.h"
using namespace lxr;
static int g_fail = 0;
static const uint8_t V[] = {0, 1, 2, 10, 32, 63, 64, 65, 96, 126, 127};
static const int N = sizeof(V);
static void chk(const char* name, int v, float got, float exp) {
    bool ok;
    if (std::isinf(exp)) ok = std::isinf(got) && ((got > 0) == (exp > 0));
    else if (exp == 0.f) ok = std::fabs(got) <= 1e-9f;
    else ok = std::fabs(got - exp) <= 4.8e-7f * std::fabs(exp);
    std::printf("%s %s v=%d got=%.9g exp=%.9g\n", ok ? "PASS" : "FAIL", name, v, got, exp);
    if (!ok) ++g_fail;
}
static const float EXP_ATT[] = {1.0f, 0.387692094f, 0.239005625f, 0.0555291176f, 0.0146990418f, 0.00507897139f, 0.00492227077f, 0.00477033854f, 0.00162005424f, 3.98755074e-05f, 0.0f};
static const float EXP_DEC[] = {1.0f, 0.0592934489f, 0.0303173661f, 0.00581884384f, 0.00148284435f, 0.000507950783f, 0.000492155552f, 0.000476896763f, 0.000161528587f, 3.9935112e-06f, 0.0f};
static const float EXP_SLOPE[] = {-0.999749899f, -0.991813421f, -0.983749986f, -0.914281666f, -0.662936211f, -0.0156172402f, 0.0158650074f, 0.0483622961f, 2.09462738f, 121.151344f, 3997.81299f};
static const float EXP_INVSLOPE[] = {3997.81299f, 121.151344f, 60.5384293f, 10.6661177f, 1.96679735f, 0.0158650074f, -0.0156172402f, -0.0461312793f, -0.676859319f, -0.991813421f, -0.999749899f};
static const float EXP_PDECAY[] = {1.0f, 0.387692094f, 0.239005625f, 0.0555291176f, 0.0146990418f, 0.00507897139f, 0.00492227077f, 0.00477033854f, 0.00162005424f, 3.98755074e-05f, 0.0f};
static const float EXP_PSLOPE[] = {-1.0f, -0.992063522f, -0.984000027f, -0.91452992f, -0.66315788f, -0.0156249925f, 0.0158730075f, 0.048387073f, 2.0967741f, 125.000061f, INFINITY};
static const float EXP_PAMT[] = {0.0f, 0.00198400393f, 0.00793601573f, 0.198400393f, 2.03162003f, 7.87451172f, 8.1264801f, 8.38241673f, 18.2845802f, 31.4980469f, 32.0f};
int main() {
    for (int i = 0; i < N; ++i) {
        const uint8_t v = V[i];
        chk("egAttackStep", v, egAttackStep(v), EXP_ATT[i]);
        chk("egDecayStep", v, egDecayStep(v), EXP_DEC[i]);
        AmpSlope s = ampEgSlope(v);
        chk("ampEgSlope.slope", v, s.slope, EXP_SLOPE[i]);
        chk("ampEgSlope.invSlope", v, s.invSlope, EXP_INVSLOPE[i]);
        chk("pitchEgDecayStep", v, pitchEgDecayStep(v), EXP_PDECAY[i]);
        chk("pitchEgSlope", v, pitchEgSlope(v), EXP_PSLOPE[i]);
        chk("pitchModAmount", v, pitchModAmount(v), EXP_PAMT[i]);
    }
    // amp decay at v=64 lasts about 1.48 s at the engine tick rate (44002.7573529412/32 Hz)
    const float secs = 1.f / (egDecayStep(64) * (44002.7573529412f / 32.f));
    std::printf("%s decay v=64 full ramp %.3f s (exp 1.478)\n", std::fabs(secs - 1.478f) < 0.01f ? "PASS" : "FAIL", secs);
    if (std::fabs(secs - 1.478f) >= 0.01f) ++g_fail;
    std::printf("SUMMARY fail=%d\n", g_fail);
    return g_fail ? 1 : 0;
}
