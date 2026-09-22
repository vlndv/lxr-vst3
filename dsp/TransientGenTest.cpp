// FILE: dsp/TransientGenTest.cpp
#include "TransientGen.h"
#include "TransientTables.h"
#include <cstdio>
#include <cmath>
#include <cstring>

namespace lxr {
// Mock data for testing if real binary files are not loaded
static float mockVolTable[69] = {1.0f, 0.5f};
static int8_t mockData[12 * 2205] = {0};
} // namespace lxr

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

static void check_u32(const char* name, uint32_t actual, uint32_t expected) {
    if (actual == expected) { printf("PASS %s\n", name); pass++; }
    else { printf("FAIL %s: expected %u, got %u\n", name, expected, actual); fail++; }
}

int main() {
    // Initialize mock tables if real ones aren't loaded
    if (!lxr::gTransientTables.volumeTable) {
        lxr::gTransientTables.volumeTable = lxr::mockVolTable;
        lxr::gTransientTables.data = lxr::mockData;
        lxr::mockData[0] = 10;
        lxr::mockData[1] = 20;       // waveform 1, index 1
        lxr::mockData[2205] = 40;    // waveform 2, index 0
        lxr::mockData[2206] = 50;    // waveform 2, index 1
    }

    {
        lxr::TransientGen tg;
        tg.init();
        check("init pitch", tg.pitch, 1.0f, 1e-5f);
        check_i("init output", tg.output, 0);
        check_u32("init phase", tg.phase, 0);
        check_i("init waveform", tg.waveform, 0);
        check("init volume", tg.volume, 1.0f, 1e-5f);
    }
    {
        lxr::TransientGen tg;
        tg.init();
        tg.setWaveform(13);
        check_i("setWaveform 13", tg.waveform, 13);
        tg.setWaveform(14);
        check_i("setWaveform 14 clamps to 0", tg.waveform, 0);
    }
    {
        lxr::TransientGen tg;
        tg.init();
        tg.phase = 12345;
        tg.trigger();
        check_u32("trigger resets phase", tg.phase, 0);
    }
    {
        lxr::TransientGen tg;
        tg.init();
        int16_t buf[4] = {1, 1, 1, 1};
        tg.setWaveform(0);
        tg.calcBlock(buf, 4);
        check_i("calcBlock wf 0 buf[0]", buf[0], 0);
        tg.setWaveform(1);
        tg.calcBlock(buf, 4);
        check_i("calcBlock wf 1 buf[0]", buf[0], 0);
    }
    {
        lxr::TransientGen tg;
        tg.init();
        tg.setWaveform(2);
        tg.pitch = 1.0f;
        tg.volume = 1.0f;
        int16_t buf[4] = {0};
        tg.calcBlock(buf, 1);
        check_u32("calcBlock phase after 1 sample", tg.phase, 1048576);
        // waveform 2 reads from transientData[0][0] = mockData[0] = 10
        // 10 << 8 = 2560
        check_i("calcBlock buf[0] value", buf[0], 2560);
    }
    {
        lxr::TransientGen tg;
        tg.init();
        tg.setWaveform(2);
        tg.pitch = 1.0f;
        tg.phase = 2311061503u; // (2204 << 20) - 1
        int16_t buf[4] = {0};
        tg.calcBlock(buf, 1);
        // ORIGINAL QUIRK VERIFICATION: Float promotion phase accumulation
        // static_cast<float>(2311061503u) rounds to 2311061504.0f.
        // increment = 1.0f * 1048576.0f = 1048576.0f.
        // sum = 2311061504.0f + 1048576.0f = 2312110080.0f.
        check_u32("calcBlock phase float promotion bug replicated", tg.phase, 2312110080u);
        
        tg.phase = 2311061504u; // exactly 2204 << 20
        tg.calcBlock(buf, 1);
        check_u32("calcBlock phase stops at limit", tg.phase, 2311061504u);
    }
    {
        lxr::TransientGen tg;
        tg.init();
        tg.setWaveform(0);
        tg.volume = 1.0f;
        tg.calc();
        check_u32("calc phase after 1 sample", tg.phase, 1048576);
        check_i("calc wf 0 output", tg.output, 32512);
    }
    {
        lxr::TransientGen tg;
        tg.init();
        tg.setWaveform(1);
        tg.volume = 1.0f;
        tg.calc();
        // waveform 1 reads from transientData[0][1] = mockData[1] = 20
        // 20 << 8 = 5120
        check_i("calc wf 1 output", tg.output, 5120);
    }

    printf("SUMMARY pass=%d fail=%d\n", pass, fail);
    printf("NOTE: For full differential testing, compile original transientGenerator.c\n");
    printf("and link against this harness to compare byte-for-byte output.\n");
    return fail > 0 ? 1 : 0;
}