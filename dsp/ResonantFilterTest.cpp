// dsp/ResonantFilterTest.cpp  (expected values from the original C, gcc -O0 -ffp-contract=off)
#include "ResonantFilter.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace lxr;
static int g_fail = 0, g_warn = 0;
static void chkF(const char* n, float got, float exp) {
    float tol = 1e-4f * (std::fabs(exp) > 1.f ? std::fabs(exp) : 1.f);
    bool ok = std::fabs(got - exp) <= tol;
    std::printf("%s %s got=%.9g exp=%.9g\n", ok ? "PASS" : "FAIL", n, got, exp);
    if (!ok) ++g_fail;
}
static void chkI(const char* n, int got, int exp) {
    int d = std::abs(got - exp);
    const char* r = d == 0 ? "PASS" : (d <= 2 ? "WARN" : "FAIL");
    if (d > 2) ++g_fail; else if (d) ++g_warn;
    std::printf("%s %s got=%d exp=%d\n", r, n, got, exp);
}
static void chkSum(const char* n, long got, long exp) {
    bool ok = std::labs(got - exp) <= 64;
    std::printf("%s %s got=%ld exp=%ld\n", ok ? "PASS" : "FAIL", n, got, exp);
    if (!ok) ++g_fail;
}
static void mkin(int16_t* b) { for (int i = 0; i < 32; ++i) b[i] = (int16_t)(((i * 7919 + 13) % 2001 - 1000) * 20); }
struct Exp { int o0, o1, o15, o31; long sum; };
static void runCfg(const char* cfg, float fv, float reso, uint8_t drv, const Exp* E,
                   const float* st1, const float* st7, const float* st8) {
    for (int type = 1; type <= 8; ++type) {
        ResonantFilter fl; std::memset(&fl, 0, sizeof fl); fl.reset();
        fl.directSetFilterValue(fv); fl.setReso(reso); fl.setDrive(drv);
        int16_t in[32], b1[32], b2[32]; mkin(in); std::memcpy(b1, in, 64); std::memcpy(b2, in, 64);
        fl.calcBlockZDF((uint8_t)type, b1, 32);
        float s1a = fl.s1, s2a = fl.s2, zia = fl.zi, aa = fl.a, ba = fl.b;
        fl.calcBlockZDF((uint8_t)type, b2, 32);
        char n[64]; const Exp& e = E[type - 1]; long sum = 0; for (int i = 0; i < 32; ++i) sum += b2[i];
        std::snprintf(n, sizeof n, "%s type%d out[0]", cfg, type);  chkI(n, b2[0], e.o0);
        std::snprintf(n, sizeof n, "%s type%d out[1]", cfg, type);  chkI(n, b2[1], e.o1);
        std::snprintf(n, sizeof n, "%s type%d out[15]", cfg, type); chkI(n, b2[15], e.o15);
        std::snprintf(n, sizeof n, "%s type%d out[31]", cfg, type); chkI(n, b2[31], e.o31);
        std::snprintf(n, sizeof n, "%s type%d sum", cfg, type);     chkSum(n, sum, e.sum);
        if (type == 1) {
            const char* nm[6] = {"s1 blk1","s2 blk1","zi blk1","s1 blk2","s2 blk2","zi blk2"};
            float got[6] = {s1a, s2a, zia, fl.s1, fl.s2, fl.zi};
            for (int k = 0; k < 6; ++k) { std::snprintf(n, sizeof n, "%s type1 %s", cfg, nm[k]); chkF(n, got[k], st1[k]); }
        }
        if (type == 7) {
            const char* nm[4] = {"a blk1","b blk1","a blk2","b blk2"};
            float got[4] = {aa, ba, fl.a, fl.b};
            for (int k = 0; k < 4; ++k) { std::snprintf(n, sizeof n, "%s type7 %s", cfg, nm[k]); chkF(n, got[k], st7[k]); }
        }
        if (type == 8) {
            bool same = std::memcmp(b2, in, 64) == 0;
            std::printf("%s %s type8 buffer unchanged\n", same ? "PASS" : "FAIL", cfg); if (!same) ++g_fail;
            const char* nm[3] = {"s1","s2","zi"}; float got[3] = {fl.s1, fl.s2, fl.zi};
            for (int k = 0; k < 3; ++k) { std::snprintf(n, sizeof n, "%s type8 %s after blk2", cfg, nm[k]); chkF(n, got[k], st8[k]); }
        }
    }
}
int main() {
    chkF("fastTan(0.5)", fastTan(0.5f), 0.546296299f);
    chkF("fastTan(1.0)", fastTan(1.0f), 1.55555558f);
    chkF("tanhXdX(1.0)", tanhXdX(1.0f), 0.761594176f);
    chkF("softClipTwo(1.0)", softClipTwo(1.0f), 0.924234331f);
    chkF("softClipTwo(-2.5)", softClipTwo(-2.5f), -1.69656801f);
    { ResonantFilter fl; std::memset(&fl, 0, sizeof fl);
      fl.setDrive(0);   chkF("setDrive(0)", fl.drive, 0.400000006f);
      fl.setDrive(64);  chkF("setDrive(64)", fl.drive, 1.923715f);
      fl.setDrive(127); chkF("setDrive(127)", fl.drive, 6.4000001f);
      fl.setReso(0.f);   chkF("setReso(0)", fl.q, 1.0f);
      fl.setReso(0.5f);  chkF("setReso(0.5)", fl.q, 0.5f);
      fl.setReso(0.95f); chkF("setReso(0.95)", fl.q, 0.0199999996f);
      fl.directSetFilterValue(0.5f); chkF("direct(0.5) f", fl.f, 0.224999994f); chkF("direct(0.5) g", fl.g, 0.853991151f);
      fl.directSetFilterValue(1.0f); chkF("direct(1.0) f", fl.f, 0.449999988f); chkF("direct(1.0) g", fl.g, 6.10959816f);
      fl.init(); chkF("init f", fl.f, 0.112499997f); chkF("init g", fl.g, 0.368918985f); chkF("init q", fl.q, 0.899999976f); chkF("init drive", fl.drive, 0.5f);
      chkF("init s1", fl.s1, 0.f); chkF("init s2", fl.s2, 0.f); }
    { static const Exp E[8] = {{570,-12159,-7734,15494,112027L}, {-15626,32767,23,-444,3556L}, {-15109,2238,-1660,-1888,4711L}, {-15109,2238,-1660,-1888,4711L}, {-15106,26518,-7135,14585,133871L}, {16147,-32768,-7183,15474,111654L}, {3897,-199,-9334,19044,99708L}, {-19740,18580,-5220,7600,85900L}};
      static const float st1[6] = {-0.0784097761f, 0.46384269f, 0.438929796f, -0.0784097239f, 0.46384263f, 0.438929796f}; static const float st7[4] = {0.505028367f, 0.658275604f, 0.505154192f, 0.658368826f}; static const float st8[3] = {-0.157155514f, -1.25570238f, -1.04454124f};
      runCfg("C1", 0.5f, 0.5f, 64, E, st1, st7, st8); }
    { static const Exp E[8] = {{-18742,6509,-8738,13435,125667L}, {526,2785,362,-1682,-1088L}, {-5977,9986,-518,778,-327L}, {-11955,19972,-1037,1557,-657L}, {-18259,8784,-7758,11139,134968L}, {-19312,3214,-8483,14505,115688L}, {-28703,7907,-10245,11645,92243L}, {-19740,18580,-5220,7600,85900L}};
      static const float st1[6] = {-0.319866836f, 0.603727698f, 0.438929796f, -0.319862872f, 0.603718877f, 0.438929796f}; static const float st7[4] = {0.402049124f, 0.402584493f, 0.402049124f, 0.402584493f}; static const float st8[3] = {0.360988557f, -0.873997808f, -1.04454124f};
      runCfg("C3", 1.0f, 0.5f, 64, E, st1, st7, st8); }
    { int ok = 1;
      for (int type = 1; type <= 8; ++type) { ResonantFilter fl; std::memset(&fl, 0, sizeof fl); fl.reset();
        fl.directSetFilterValue(0.5f); fl.setReso(0.5f); fl.setDrive(64);
        int16_t z[32]; std::memset(z, 0, 64); fl.calcBlockZDF((uint8_t)type, z, 32); fl.calcBlockZDF((uint8_t)type, z, 32);
        for (int i = 0; i < 32; ++i) if (z[i]) ok = 0; }
      std::printf("%s silence all types\n", ok ? "PASS" : "FAIL"); if (!ok) ++g_fail; }
    std::printf("SUMMARY fail=%d warn=%d\n", g_fail, g_warn);
    return g_fail ? 1 : 0;
}
