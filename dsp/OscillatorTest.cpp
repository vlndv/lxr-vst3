// dsp/OscillatorTest.cpp (expected values from the ORIGINAL Oscillator.c, ARM float->uint32 semantics; see tests/reference/oscillator)
// Run from the repo root: needs data/*.bin (tools/export_data.py). Optional argument: data directory.
#include "Oscillator.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
using namespace lxr;
static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", what); if (!ok) ++g_fail; }
static void checkF(const char* n, float got, float exp) {
    bool ok = (exp == 0.f) ? std::fabs(got) <= 1e-9f : std::fabs(got - exp) <= 4.8e-7f * std::fabs(exp);
    std::printf("%s %s got=%.9g exp=%.9g\n", ok ? "PASS" : "FAIL", n, got, exp); if (!ok) ++g_fail;
}
static uint32_t fnv(uint32_t h, int16_t v) { h ^= (uint8_t)(v & 0xff); h *= 16777619u; h ^= (uint8_t)((v >> 8) & 0xff); h *= 16777619u; return h; }
static void mkmod(int16_t* m, int n, int blk) { for (int i = 0; i < n; i++) m[i] = (int16_t)((((i + blk * 32) * 4099 + 7) % 2001 - 1000) * 30); }
struct Case { const char* name; uint8_t wave; float freq, pitchMod, modNode, fmMod; uint32_t phase; float gain; bool fm;
              uint32_t hash; int o0, o1, o2, o3, o40, olast; uint32_t phaseEnd; int outputEnd; uint32_t phaseInc; int table; };
static const Case CASES[] = {
    {"sine100", 0, 100.f, 1.f, 1.f, 0.f, 0u, 1.0f, false, 1313899655u, -32767, -32763, -32753, -32736, -27553, -12334, 819896952u, 0, 9760678u, 0},
    {"sine1000st", 0, 1000.f, 1.f, 1.f, 0.f, 1023u<<20, 0.5f, false, 3778993200u, -18, 2315, 4602, 6795, -8871, -10753, 681727840u, 0, 97606776u, 0},
    {"sine10000", 0, 10000.f, 1.f, 1.f, 0.f, 12345678u, 1.0f, false, 1769332387u, -32761, -4086, 31611, 13032, -27307, -21766, 397660238u, 0, 976067776u, 0},
    {"sinePitch", 0, 200.f, 2.5f, 0.8f, 0.f, 777u, 1.0f, false, 2720158188u, -32766, -32712, -32550, -32288, 21481, -984, 3279588585u, 0, 39042712u, 0},
    {"tri55", 1, 55.f, 1.f, 1.f, 0.f, 0xffu<<20, 1.0f, false, 415933089u, -24867, -24609, -24480, -24351, -18287, -11187, 718330128u, 0, 5368372u, 4},
    {"tri440", 1, 440.f, 1.f, 1.f, 0.f, 0u, 1.0f, false, 4071012959u, -32767, -31926, -30618, -29317, 20104, -10632, 3607546320u, 0, 42946980u, 5},
    {"tri1000", 1, 1000.f, 1.f, 1.f, 0.f, 5000000u, 0.7f, false, 2080847968u, -22932, -21350, -19215, -16997, -15430, -13164, 3909001888u, 0, 97606776u, 6},
    {"saw100", 2, 100.f, 1.f, 1.f, 0.f, 0xffu<<20, 1.0f, false, 1443317227u, 28878, 28681, 28552, 28425, 22881, 16450, 1087283832u, 0, 9760678u, 4},
    {"saw3000", 2, 3000.f, 1.f, 1.f, 0.f, 0u, 1.0f, false, 616116030u, 0, 32636, 26897, 21711, -16536, -11497, 3122070400u, 0, 292820320u, 7},
    {"saw8000", 2, 8000.f, 1.f, 1.f, 0.f, 123456u, 1.0f, false, 962220496u, 0, 32694, 15175, -4308, 27507, 22649, 1167367488u, 0, 780854208u, 9},
    {"saw20000", 2, 20000.f, 1.f, 1.f, 0.f, 42u, 1.0f, false, 864968251u, 0, 9318, -17867, 24810, 29706, -32350, 770629162u, 0, 1952135552u, 10},
    {"rec220", 3, 220.f, 1.f, 1.f, 0.f, 0xffu<<20, 1.0f, false, 1961137151u, 32144, 32136, 32134, 32126, 32010, 31920, 2071160040u, 0, 21473490u, 4},
    {"rec9000", 3, 9000.f, 1.f, 1.f, 0.f, 500000u, 1.0f, false, 3549599577u, 2879, 32208, 27088, -20403, 31143, -2565, 776779296u, 0, 878460992u, 9},
    {"noise1000", 4, 1000.f, 1.f, 1.f, 0.f, 0u, 0.9f, false, 1344633589u, 0, 0, 0, 0, 0, 16150, 3904001888u, 17945, 97606776u, 0},
    {"noise12000", 4, 12000.f, 1.f, 1.f, 0.f, 0u, 1.0f, false, 804902453u, 0, 0, 0, 17945, -22034, -5666, 3898347008u, -5666, 1171281280u, 0},
    {"crash500", 5, 500.f, 1.f, 1.f, 0.f, 0u, 1.0f, false, 486648012u, 256, -1280, -768, -2560, -9216, 7936, 128108820u, 0, 1525105u, 0},
    {"crash2000", 5, 2000.f, 1.f, 1.f, 0.f, 1000000u, 1.0f, false, 3028493800u, -3072, 2560, 1536, -1536, -4864, -256, 513435532u, 0, 6100423u, 0},
    {"fmSine200", 0, 200.f, 1.f, 1.f, 0.5f, 0u, 1.0f, true, 524793002u, -32767, -32753, -32712, -32647, 27936, 23514, 1639793904u, 0, 19521356u, 0},
    {"fmSine1500", 0, 1500.f, 1.f, 1.f, 1.0f, 555u, 0.7f, true, 2459711523u, -22936, -22414, -20859, -18361, -10456, -10998, 3708519403u, 0, 146410160u, 0},
    {"fmTri100", 1, 100.f, 1.f, 1.f, 0.3f, 0xffu<<20, 1.0f, true, 1170710781u, -24771, -24471, -24170, -23870, -16276, 168, 1087283832u, 168, 9760678u, 4},
    {"fmSaw400", 2, 400.f, 1.f, 1.f, 1.0f, 0u, 1.0f, true, 202755147u, 0, 32031, 31708, 31173, -7121, -16746, 3279587808u, -16746, 39042712u, 4},
    {"fmRec2500", 3, 2500.f, 1.f, 1.f, 0.8f, 31337u, 1.0f, true, 3964456319u, 141, 30442, 31732, 31026, -31415, -31169, 3317585449u, -31169, 244016944u, 7},
    {"fmCrash800", 5, 800.f, 1.f, 1.f, 0.5f, 0u, 1.0f, true, 1745986063u, 256, 512, 1792, -1024, -5632, 1536, 204974196u, 1536, 2440169u, 0},
    {"fmNoise", 4, 3000.f, 1.f, 1.f, 0.5f, 0u, 1.0f, true, 3427047058u, 0, 0, 0, 0, 21822, 27935, 3122070400u, 27935, 292820320u, 0},
};

struct FreqCase { uint8_t wave; float freq; uint32_t phaseInc; int table; };
static const FreqCase FREQ[] = {
    {0, 0.0f, 0u, 0},
    {0, 20.0f, 1952135u, 0},
    {0, 100.0f, 9760678u, 0},
    {0, 439.899994f, 42937220u, 0},
    {0, 440.0f, 42946980u, 0},
    {0, 879.900024f, 85884208u, 0},
    {0, 880.0f, 85893960u, 0},
    {0, 1760.0f, 171787920u, 0},
    {0, 3520.0f, 343575840u, 0},
    {0, 7040.0f, 687151680u, 0},
    {0, 14080.0f, 1374303360u, 0},
    {0, 20000.0f, 1952135552u, 0},
    {0, 30000.0f, 2928203264u, 0},
    {0, 44000.0f, 4294698240u, 0},
    {2, 0.0f, 0u, 4},
    {2, 20.0f, 1952135u, 4},
    {2, 100.0f, 9760678u, 4},
    {2, 439.899994f, 42937220u, 4},
    {2, 440.0f, 42946980u, 5},
    {2, 879.900024f, 85884208u, 5},
    {2, 880.0f, 85893960u, 6},
    {2, 1760.0f, 171787920u, 7},
    {2, 3520.0f, 343575840u, 8},
    {2, 7040.0f, 687151680u, 9},
    {2, 14080.0f, 1374303360u, 10},
    {2, 20000.0f, 1952135552u, 10},
    {2, 30000.0f, 2928203264u, 10},
    {2, 44000.0f, 4294698240u, 10},
    {5, 0.0f, 0u, 0},
    {5, 20.0f, 61004u, 0},
    {5, 100.0f, 305021u, 0},
    {5, 439.899994f, 1341788u, 0},
    {5, 440.0f, 1342093u, 0},
    {5, 879.900024f, 2683881u, 0},
    {5, 880.0f, 2684186u, 0},
    {5, 1760.0f, 5368372u, 0},
    {5, 3520.0f, 10736745u, 0},
    {5, 7040.0f, 21473490u, 0},
    {5, 14080.0f, 42946980u, 0},
    {5, 20000.0f, 61004236u, 0},
    {5, 30000.0f, 91506352u, 0},
    {5, 44000.0f, 134209320u, 0},
};

struct BaseCase { uint16_t midiFreq; uint8_t base; float freq; };
static const BaseCase BASE[] = {
    {16192, 63, 311.199829f},
    {0, 63, 7.93272018f},
    {32639, 63, 12916.8008f},
    {25600, 90, 12170.9053f},
    {2687, 40, 8.41887856f},
    {17953, 80, 1226.73572f},
};

int main(int argc, char** argv) {
    OscTableStore store;
    if (!store.load(argc > 1 ? argv[1] : "data")) { std::printf("FAIL cannot load data/*.bin (run tools/export_data.py first)\nSUMMARY fail=1\n"); return 2; }
    // The original reads one element past the last row of sawTable/triTable/recTable and past crashSample; these are the values the
    // reference build (host gcc) happens to read there. On the hardware the value is unknown (UNSURE), the port default is 0.
    store.setGuards(0, -31616, -32767, 1);
    const OscTables& t = store.tables();
    check(t.sine[0] == -32767 && t.sine[2048] == 32766 && t.sine[4096] == -32767, "sine table loaded (-cos shape)");
    check(t.crash[0] != 0 || t.crash[1] != 0 || t.crash[2] != 0, "crash sample loaded");
    check(std::fabs(t.noteFreq[69] - 440.f) < 0.01f, "note table: note 69 = 440 Hz");
    for (const Case& c : CASES) {
        OscInfo o; o.waveform = c.wave; o.freq = c.freq; o.pitchMod = c.pitchMod; o.modNodeValue = c.modNode; o.fmMod = c.fmMod; o.phase = c.phase;
        osc_setFreq(&o);
        OscRng rng;
        static const int sizes[3] = {32, 32, 20};
        int16_t all[84]; int n = 0;
        for (int b = 0; b < 3; b++) {
            int16_t buf[64], mod[64]; mkmod(mod, sizes[b], b); std::memset(buf, 0x55, sizeof buf);
            if (c.fm) calcNextOscSampleFmBlock(&o, mod, buf, (uint8_t)sizes[b], c.gain, t, rng);
            else      calcNextOscSampleBlock(&o, buf, (uint8_t)sizes[b], c.gain, t, rng);
            for (int i = 0; i < sizes[b]; i++) all[n++] = buf[i];
        }
        uint32_t h = 2166136261u; for (int i = 0; i < n; i++) h = fnv(h, all[i]);
        bool ok = h == c.hash && all[0] == c.o0 && all[1] == c.o1 && all[2] == c.o2 && all[3] == c.o3 && all[40] == c.o40 && all[n - 1] == c.olast &&
                  o.phase == c.phaseEnd && o.output == c.outputEnd && o.phaseInc == c.phaseInc && o.tableOffset == c.table;
        std::printf("%s render %s (hash %u, expected %u)\n", ok ? "PASS" : "FAIL", c.name, h, c.hash); if (!ok) ++g_fail;
    }
    for (const FreqCase& f : FREQ) {
        OscInfo o; o.waveform = f.wave; o.freq = f.freq; o.pitchMod = 1.f; o.modNodeValue = 1.f; osc_setFreq(&o);
        char n[64]; std::snprintf(n, sizeof n, "setFreq wave=%d f=%g", f.wave, f.freq);
        check(o.phaseInc == f.phaseInc && (f.wave != 2 || o.tableOffset == f.table), n);
    }
    for (const BaseCase& b : BASE) {
        OscInfo o; o.midiFreq = b.midiFreq; osc_setBaseNote(&o, b.base, t.noteFreq);
        char n[64]; std::snprintf(n, sizeof n, "setBaseNote midiFreq=%u base=%d", b.midiFreq, b.base);
        checkF(n, o.freq, b.freq); check(o.baseNote == b.base, "baseNote stored");
        OscInfo p; p.midiFreq = b.midiFreq; p.baseNote = b.base; osc_recalcFreq(&p, t.noteFreq); checkF("recalcFreq equals setBaseNote", p.freq, o.freq);
    }
    // ---- port decisions (ARM VCVT.U32.F32 semantics, out-of-scope waveforms)
    check(floatToU32Sat(-1.f) == 0 && floatToU32Sat(-1e9f) == 0, "floatToU32Sat: negative -> 0");
    check(floatToU32Sat(std::nanf("")) == 0, "floatToU32Sat: NaN -> 0");
    check(floatToU32Sat(5.0e9f) == 0xFFFFFFFFu && floatToU32Sat(4294967296.0f) == 0xFFFFFFFFu, "floatToU32Sat: too large -> 0xFFFFFFFF");
    check(floatToU32Sat(3.9f) == 3u && floatToU32Sat(0.f) == 0u, "floatToU32Sat: truncates toward zero");
    check(freq2PhaseIncr(100000.f) == 0xFFFFFFFFu, "sine phase increment saturates at 100 kHz");
    check(freqToTableIndex(0.f) == 4 && freqToTableIndex(439.f) == 4 && freqToTableIndex(440.f) == 5, "freqToTableIndex: below 440 Hz wraps to table 4");
    {   // negative modulator values do not move the phase (half-wave FM): all-negative modulator == no modulator
        OscInfo a, b; a.waveform = b.waveform = OSC_SINE; a.freq = b.freq = 300.f; a.fmMod = 1.f; b.fmMod = 1.f; osc_setFreq(&a); osc_setFreq(&b);
        int16_t mod[32], x[32], y[32]; for (int i = 0; i < 32; i++) mod[i] = (int16_t)(-1000 - 900 * i);
        OscRng r1, r2; calcNextOscSampleFmBlock(&a, mod, x, 32, 1.f, t, r1);
        int16_t zero[32]; std::memset(zero, 0, sizeof zero); calcNextOscSampleFmBlock(&b, zero, y, 32, 1.f, t, r2);
        check(std::memcmp(x, y, sizeof x) == 0, "FM: negative modulator values act as 0 (half-wave FM)");
    }
    {   // user samples are not ported: silence
        OscInfo o; o.waveform = 6; o.freq = 100.f; osc_setFreq(&o); int16_t buf[32]; std::memset(buf, 0x55, sizeof buf); OscRng r;
        calcNextOscSampleBlock(&o, buf, 32, 1.f, t, r);
        bool z = true; for (int i = 0; i < 32; i++) z = z && buf[i] == 0; check(z, "waveform >= 6 (user sample) renders silence");
    }
    {   // random generator is deterministic
        OscRng a, b; bool same = true; for (int i = 0; i < 100; i++) same = same && a.next() == b.next(); check(same, "OscRng deterministic");
    }
    std::printf("SUMMARY fail=%d\n", g_fail);
    return g_fail ? 1 : 0;
}