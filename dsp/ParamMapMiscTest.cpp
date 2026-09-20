// dsp/ParamMapMiscTest.cpp (expected values from the original C, gcc -O0 -ffp-contract=off)
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "ParamMapMisc.h"
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
static const float EXP_CUTOFF[] = {0.0f, 0.000417931995f, 0.000842193258f, 0.00448250398f, 0.0174359232f, 0.0493016019f, 0.0507990122f, 0.0523397923f, 0.14026013f, 0.869072974f, 1.0f};
static const float EXP_DECIM[] = {0.0f, 0.00139898481f, 0.0028163502f, 0.0148628596f, 0.0561220758f, 0.148037747f, 0.152054384f, 0.156161055f, 0.353436947f, 0.956973433f, 1.0f};
static const float EXP_DIST[] = {0.0f, 0.0157480314f, 0.0317460336f, 0.169491529f, 0.666666687f, 1.93846154f, 2.0f, 2.06349206f, 6.0f, 126.0f, 254.0f};
static const float EXP_NOISEF[] = {0.0f, 173.228348f, 346.456696f, 1732.28345f, 5543.30713f, 10913.3857f, 11086.6143f, 11259.8428f, 16629.9219f, 21826.7715f, 22000.0f};
static const float EXP_LFOHZ[] = {9.53674316e-05f, 0.000762939453f, 0.00257492065f, 0.126934052f, 3.42721939f, 25.0f, 26.1902809f, 27.4177551f, 87.0392761f, 195.34903f, 200.0f};
static const float EXP_TRPITCH[] = {0.25f, 0.279498518f, 0.308997035f, 0.544985235f, 1.1939528f, 2.10840702f, 2.1379056f, 2.16740417f, 3.0818584f, 3.96681404f, 3.99631262f};
static const float EXP_DETUNE[] = {0.970268428f, 0.970736623f, 0.971204877f, 0.974950552f, 0.985251248f, 0.999765873f, 1.00023413f, 1.00070226f, 1.01521695f, 1.02926338f, 1.02973151f};
static const uint32_t EXP_PHASEOFF[] = {0u, 33818640u, 67637280u, 338186400u, 1082196480u, 2130574336u, 2164392960u, 2198211584u, 3246589440u, 4261148672u, 4294967295u}; // v=127: saturated (see CONSTRAINTS)
struct OscCase { uint16_t mf; uint8_t base; float exp; };
static const OscCase OSC_CASES[] = {{0x3f40, 63, 1063.2489f}, {0x0000, 63, 970.268433f}, {0x7f7f, 63, 1160.50745f}, {0x6400, 90, 1093.49255f}, {0x0a7f, 40, 1029.73157f}, {0x7f00, 90, 1093.49255f}, {0x0040, 40, 1000.23413f}};
static const int TW[][2] = {{0, 0}, {1, 1}, {12, 12}, {13, 13}, {14, 0}, {15, 0}, {127, 0}};
int main() {
    for (int i = 0; i < N; ++i) {
        const uint8_t v = V[i];
        chk("cutoffShape", v, cutoffShape(v), EXP_CUTOFF[i]);
        chk("decimationRate", v, decimationRate(v), EXP_DECIM[i]);
        chk("distortionShape", v, distortionShape(v), EXP_DIST[i]);
        chk("noiseFrequencyHz", v, noiseFrequencyHz(v), EXP_NOISEF[i]);
        chk("lfoFrequencyHz", v, lfoFrequencyHz(v), EXP_LFOHZ[i]);
        chk("transientPitch", v, transientPitch(v), EXP_TRPITCH[i]);
        chk("fineDetune", v, fineDetune(v), EXP_DETUNE[i]);
        chk("unitFromParam", v, unitFromParam(v), v / 127.f);
        uint32_t po = lfoPhaseOffset(v);
        bool ok = po == EXP_PHASEOFF[i];
        std::printf("%s lfoPhaseOffset v=%d got=%u exp=%u\n", ok ? "PASS" : "FAIL", v, po, EXP_PHASEOFF[i]);
        if (!ok) ++g_fail;
    }
    for (int k = 0; k < (int)(sizeof(TW) / sizeof(TW[0])); ++k) {
        int got = transientWaveform((uint8_t)TW[k][0]);
        bool ok = got == TW[k][1];
        std::printf("%s transientWaveform v=%d got=%d exp=%d\n", ok ? "PASS" : "FAIL", TW[k][0], got, TW[k][1]);
        if (!ok) ++g_fail;
    }
    for (int v = 0; v < 8; ++v) {
        bool ok = filterTypeFromParam((uint8_t)v) == v + 1;
        std::printf("%s filterTypeFromParam v=%d\n", ok ? "PASS" : "FAIL", v);
        if (!ok) ++g_fail;
    }
    float table[128];
    for (int i = 0; i < 128; ++i) table[i] = 1000.f + i;   // synthetic note table: tests indexing and clamping only
    for (const OscCase& c : OSC_CASES) {
        float got = oscFrequencyHz(c.mf, c.base, table);
        char n[48]; std::snprintf(n, sizeof n, "oscFrequencyHz base=%d", c.base);
        chk(n, c.mf, got, c.exp);
    }
    std::printf("SUMMARY fail=%d\n", g_fail);
    return g_fail ? 1 : 0;
}
