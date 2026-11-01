// Startup regressions. Firmware expectations: SlopeEg2.c and mixer.c at
// SonicPotions/LXR dee4968; no envelope or decimator equations are replaced.
#include "../dsp/Engine.h"
#include "../dsp/ParameterArray.h"
#include "../dsp/ParamMapEnv.h"
#include "../dsp/MidiParser.h"
#include "../dsp/LXRAudioAdapter.h"
#include "../vendor/iPlug2/Examples/LXR/LXRParamContract.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {
bool rendering = false;
int renderAllocations = 0;
int pass = 0, fail = 0;
void check(const char* name, bool ok) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    ok ? ++pass : ++fail;
}
struct Result { int st1 = 0, st2 = 0, nonzero = 0; };

void initial(lxr::Engine& e, lxr::ParameterArray& p, bool host = false) {
    e.init();
    p.init();
    if (host) {
        for (int id = 0; id < lxr::kNumParams; ++id)
            p.set(id, lxr::vst::kParamDescriptors[id].defaultValue, e);
    } else p.applyAllToEngine(e);
}
void legacy(lxr::Engine& e, lxr::ParameterArray& p) {
    initial(e, p);
    for (int id = 49; id <= 59; id += 2) p.set(id, 127, e);
    for (int id = 108; id <= 114; ++id) p.set(id, 0, e);
    p.set(61, 0, e); // Historical native default (host had 64).
}
Result native(lxr::Engine& e) {
    Result r;
    int16_t a[32], b[32], c[32], d[32];
    rendering = true;
    for (int block = 0; block < 100; ++block) {
        e.processBlock(a, b, c, d);
        for (int i = 0; i < 32; ++i) {
            r.st1 = std::max({r.st1, std::abs(int(a[i])), std::abs(int(b[i]))});
            r.st2 = std::max({r.st2, std::abs(int(c[i])), std::abs(int(d[i]))});
            r.nonzero += a[i] != 0 || b[i] != 0;
        }
    }
    rendering = false;
    return r;
}
}

void* operator new(std::size_t n) {
    if (rendering) ++renderAllocations;
    if (void* p = std::malloc(n ? n : 1)) return p;
    std::abort();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

int main() {
    lxr::Engine e;
    lxr::ParameterArray p;
    if (!e.loadAssets("data")) { check("assets loaded", false); return 1; }
    p.init();
    for (int id = 0; id < lxr::kNumParams; ++id)
        check("native/host initial value agrees", p.get(id) == lxr::vst::kParamDescriptors[id].defaultValue);

    // Original-C reference vectors in tests/reference/mapping/expected_output.txt.
    const uint8_t attacks[] = {0, 64, 127};
    const float expected[] = {1.0f, 0.004922270774841309f, 0.0f};
    for (int i = 0; i < 3; ++i) {
        AmpEg eg;
        eg.init(); eg.setAttack(attacks[i], false); eg.trigger();
        check("firmware attack min/mid/max", eg.attack == expected[i]);
        const float gain = eg.calc();
        check("attack transition and finite gain", std::isfinite(gain) &&
              (i == 0 ? eg.state == 2 && eg.value == 1 && gain == 1 :
               i == 1 ? eg.state == 1 && eg.value == expected[i] && gain > 0 :
                        eg.state == 1 && eg.value == 0 && gain == 0));
    }
    for (int v = 0; v < 128; ++v) {
        AmpEg eg; eg.init(); eg.setAttack(v, false); eg.setDecay(v, false);
        p.set(49, v, e); p.set(50, v, e);
        check("ParameterArray/map/AmpEg consistency", e.drums[0].oscVolEg.attack == eg.attack &&
              eg.attack == lxr::egAttackStep(v) && e.drums[0].oscVolEg.decay == eg.decay &&
              eg.decay == lxr::egDecayStep(v));
    }
    for (int experiment = 0; experiment < 4; ++experiment) {
        legacy(e, p);
        if (experiment & 1) p.set(49, 64, e);
        if (experiment & 2) { p.set(108, 127, e); p.set(114, 127, e); }
        e.triggerDrum(0, 110, 60);
        const auto r = native(e);
        std::printf("Experiment %d ST1=%d ST2=%d nonzero=%d gain=%.9g\n",
                    experiment, r.st1, r.st2, r.nonzero, e.drums[0].targetGain);
        check("independent startup faults reproduced", experiment == 3 ? r.st1 > 0 : r.st1 == 0);
        check("legacy attack holds zero / midpoint rises", experiment & 1 ?
              e.drums[0].targetGain > 0 : e.drums[0].targetGain == 0);
        check("ST2 remains silent", r.st2 == 0);
    }
    for (int host = 0; host < 2; ++host) {
        initial(e, p, host != 0); e.triggerDrum(0, 110, 60);
        const auto r = native(e);
        std::printf("Startup host=%d ST1=%d ST2=%d nonzero=%d\n", host, r.st1, r.st2, r.nonzero);
        check("corrected initial patch produces ST1", r.st1 > 0 && r.nonzero > 3000 && r.st2 == 0);
    }
    for (int frozen : {108, 114}) {
        initial(e, p); p.set(frozen, 0, e); e.triggerDrum(0, 110, 60);
        check("either voice or global decimator can freeze startup", native(e).st1 == 0 && e.drums[0].targetGain > 0);
    }
    initial(e, p); p.set(215, 1, e); e.triggerDrum(0, 110, 60);
    const auto st2 = native(e);
    check("native ST2 route is independent of ST1", st2.st1 == 0 && st2.st2 > 0);

    lxr::MidiParser midi;
    for (uint8_t channel = 0; channel < 7; ++channel) {
        initial(e, p);
        // Give HH open/closed distinct decays to verify both trigger variants.
        p.set(60, 32, e); p.set(61, 96, e);
        midi.processEvent({uint8_t(0x90 | channel), channel, 60, 110}, e, p);
        const AmpEg* env[] = {&e.drums[0].oscVolEg, &e.drums[1].oscVolEg,
            &e.drums[2].oscVolEg, &e.snare.oscVolEg, &e.cymbal.oscVolEg, &e.hihat.oscVolEg};
        bool routed = true;
        for (int voice = 0; voice < 6; ++voice)
            routed &= env[voice]->state == (voice == std::min(int(channel), 5) ? 1 : 0);
        check("MIDI ch1-7 selects only intended voice", routed);
        if (channel >= 5) check("HH closed/open decay selected", e.hihat.oscVolEg.decay ==
            (channel == 5 ? e.hihat.decayClosed : e.hihat.decayOpen));
        check("MIDI startup hit audible", native(e).st1 > 0);
    }

    lxr::LXRAudioAdapter adapter;
    initial(e, p); e.triggerDrum(0, 110, 60);
    double l = 1, r = 1;
    adapter.getNextHostST1Sample(e, l, r);
    check("unready FIR silences without advancing engine", !adapter.ready() && l == 0 && r == 0 &&
          e.drums[0].oscVolEg.value == 0);
    check("initial invalid host rate rejected", !adapter.configure(0.) && !adapter.ready());
    for (double rate : {32000., 44100., 48000., 88200., 96000., 176400., 192000.}) {
        initial(e, p); e.triggerDrum(0, 110, 60);
        check("host rate configure", adapter.configure(rate));
        double peak = 0; bool finite = true;
        rendering = true;
        for (int i = 0; i < 4096; ++i) {
            adapter.getNextHostST1Sample(e, l, r);
            finite &= std::isfinite(l) && std::isfinite(r);
            peak = std::max({peak, std::abs(l), std::abs(r)});
        }
        rendering = false;
        std::printf("Adapter rate=%.0f peak=%.9g\n", rate, peak);
        check("engine/FIR finite nonzero integration", finite && peak > 0.01);
        adapter.reset();
        check("reset retains readiness", adapter.ready());
    }
    check("no C++ allocation during native/adapter rendering", renderAllocations == 0);
    std::printf("SUMMARY pass=%d fail=%d renderAllocations=%d\n", pass, fail, renderAllocations);
    return fail ? 1 : 0;
}
