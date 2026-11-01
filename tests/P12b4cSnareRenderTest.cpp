#include "../dsp/Engine.h"
#include "../dsp/FIRLatency.h"
#include "../dsp/LXRAudioAdapter.h"
#include "../dsp/MidiParser.h"
#include "../dsp/ParameterArray.h"
#include "../dsp/TransientTables.h"
#include "reference/snare/original_snare_harness.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
constexpr uint32_t kSeed = 0x9E3779B9u;
int passed = 0;
int failed = 0;
int renderAllocations = 0;
bool rendering = false;

void check(const char* name, bool condition) {
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", name);
    condition ? ++passed : ++failed;
}

bool loadFloatTable(const char* path, float* values, std::size_t count) {
    FILE* file = std::fopen(path, "rb");
    if (!file) return false;
    const bool ok = std::fread(values, sizeof(float), count, file) == count;
    std::fclose(file);
    return ok;
}

RefSnareConfig baseConfig() {
    RefSnareConfig c {};
    c.tonal_waveform = lxr::OSC_TRI;
    c.tonal_midi_freq = static_cast<uint16_t>((70u << 8) | 64u);
    c.noise_frequency = 12000.f;
    c.mix = 0.5f;
    c.volume = 0.8f;
    c.volume_mod = 1;
    c.filter_type = lxr::FILTER_HP;
    c.filter_frequency = 0.1f;
    c.filter_resonance = 0.2f;
    c.filter_drive = 1.f;
    c.amp_attack = 1.f;
    c.amp_decay = 0.035f;
    c.amp_slope = 0.f;
    c.amp_inv_slope = 0.f;
    c.amp_repeat = 0;
    c.pitch_decay = 0.05f;
    c.pitch_slope = 0.f;
    c.pitch_amount = 0.5f;
    c.transient_waveform = 0;
    c.transient_volume = 0.f;
    c.transient_pitch = 1.f;
    c.distortion_shape = 2;
    return c;
}

void configurePort(lxr::SnareVoice& snare, const RefSnareConfig& c) {
    snare.osc.waveform = c.tonal_waveform;
    snare.osc.midiFreq = c.tonal_midi_freq;
    snare.noiseOsc.freq = c.noise_frequency;
    snare.mix = c.mix;
    snare.vol = c.volume;
    snare.volumeMod = c.volume_mod;
    snare.filterType = c.filter_type;
    snare.filter.f = c.filter_frequency;
    snare.filter.q = c.filter_resonance;
    snare.filter.drive = c.filter_drive;
    snare.filter.recalcFreq();
    snare.oscVolEg.attack = c.amp_attack;
    snare.oscVolEg.decay = c.amp_decay;
    snare.oscVolEg.slope = c.amp_slope;
    snare.oscVolEg.invSlope = c.amp_inv_slope;
    snare.oscVolEg.repeat = c.amp_repeat;
    snare.oscPitchEg.decay = c.pitch_decay;
    snare.oscPitchEg.slope = c.pitch_slope;
    snare.egPitchModAmount = c.pitch_amount;
    snare.transGen.waveform = c.transient_waveform;
    snare.transGen.volume = c.transient_volume;
    snare.transGen.pitch = c.transient_pitch;
    snare.distortion.setShape(c.distortion_shape);
}

struct DiffResult {
    int samples = 0;
    int mismatches = 0;
    int maxDifference = 0;
    int peak = 0;
    double rms = 0;
    double differenceRms = 0;
    bool canaries = true;
    bool finite = true;
};

DiffResult runDifferential(const char* name, const RefSnareConfig& config,
                           uint8_t velocity, int blocks, int retriggerBlock,
                           const lxr::OscTables& tables, const float* noteFreq) {
    // Match ref_snare_reset's zeroed firmware-global storage before init().
    // In particular SVF_init does not initialize the previous-input field zi.
    lxr::SnareVoice port {};
    port.init();
    port.rng.state = kSeed;
    configurePort(port, config);
    ref_snare_reset(kSeed);
    ref_snare_configure(&config);
    port.trigger(velocity, 60, noteFreq);
    ref_snare_trigger(velocity, 60);

    DiffResult result;
    double energy = 0;
    double diffEnergy = 0;
    for (int block = 0; block < blocks; ++block) {
        if (block == retriggerBlock) {
            port.trigger(velocity, 60, noteFreq);
            ref_snare_trigger(velocity, 60);
        }
        std::array<int16_t, 34> actual {};
        std::array<int16_t, 34> expected {};
        actual.front() = expected.front() = 0x1234;
        actual.back() = expected.back() = static_cast<int16_t>(0xA55A);
        rendering = true;
        port.calcAsync(noteFreq);
        port.calcSyncBlock(actual.data() + 1, 32, tables);
        ref_snare_process_block(expected.data() + 1, 32);
        rendering = false;
        result.canaries &= actual.front() == 0x1234 && expected.front() == 0x1234 &&
                           actual.back() == static_cast<int16_t>(0xA55A) &&
                           expected.back() == static_cast<int16_t>(0xA55A);
        for (int i = 1; i <= 32; ++i) {
            const int a = actual[i];
            const int e = expected[i];
            const int d = std::abs(a - e);
            result.mismatches += d != 0;
            result.maxDifference = std::max(result.maxDifference, d);
            result.peak = std::max(result.peak, std::abs(a));
            energy += static_cast<double>(a) * a;
            diffEnergy += static_cast<double>(d) * d;
            result.finite &= std::isfinite(static_cast<double>(a));
            ++result.samples;
        }
    }
    result.rms = std::sqrt(energy / result.samples);
    result.differenceRms = std::sqrt(diffEnergy / result.samples);
    std::printf("DIFF case=%s samples=%d mismatches=%d max_diff=%d diff_rms=%.12g "
                "peak=%d rms=%.12g\n", name, result.samples, result.mismatches,
                result.maxDifference, result.differenceRms, result.peak, result.rms);
    return result;
}

struct NativeStats { int peak = 0; int nonzero = 0; double rms = 0; };
NativeStats renderNative(lxr::Engine& engine, int blocks) {
    NativeStats result;
    double energy = 0;
    int samples = 0;
    std::array<int16_t, 32> l {}, r {}, l2 {}, r2 {};
    rendering = true;
    for (int block = 0; block < blocks; ++block) {
        engine.processBlock(l.data(), r.data(), l2.data(), r2.data());
        for (int i = 0; i < 32; ++i) {
            result.peak = std::max({result.peak, std::abs(int(l[i])), std::abs(int(r[i]))});
            result.nonzero += l[i] != 0 || r[i] != 0;
            energy += static_cast<double>(l[i]) * l[i] + static_cast<double>(r[i]) * r[i];
            samples += 2;
        }
    }
    rendering = false;
    result.rms = std::sqrt(energy / samples);
    return result;
}
} // namespace

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
    lxr::Engine assets;
    std::array<float, 128> noteFreq {};
    check("oscillator assets loaded", assets.loadAssets("data"));
    check("transient assets loaded", lxr::gTransientTables.load("data"));
    check("reference assets loaded", ref_snare_load_assets("data") != 0);
    check("exported MIDI frequency table loaded",
          loadFloatTable("data/midi_note_frequencies.bin", noteFreq.data(), noteFreq.size()));
    if (failed) return 1;

    struct Case { const char* name; RefSnareConfig config; uint8_t velocity; int blocks; int retrigger; };
    std::array<Case, 12> cases {};
    for (auto& c : cases) { c.config = baseConfig(); c.velocity = 110; c.blocks = 12; c.retrigger = -1; }
    cases[0].name = "tonal-only"; cases[0].config.mix = 0.f;
    cases[1].name = "noise-only"; cases[1].config.mix = 1.f;
    cases[2].name = "mixed"; cases[2].config.mix = 0.37f;
    cases[3].name = "transient"; cases[3].config.transient_waveform = 11;
    cases[3].config.transient_volume = 0.7f; cases[3].config.transient_pitch = 1.35f;
    cases[4].name = "lowpass"; cases[4].config.filter_type = lxr::FILTER_LP;
    cases[4].config.filter_frequency = 0.045f; cases[4].config.filter_resonance = 0.65f;
    cases[5].name = "naive-two-pole"; cases[5].config.filter_type = lxr::FILTER_NAIVE_2_POLE;
    cases[5].config.filter_frequency = 0.12f;
    cases[6].name = "distortion-max"; cases[6].config.distortion_shape = 127;
    cases[7].name = "velocity-zero-modulated"; cases[7].velocity = 0;
    cases[8].name = "velocity-zero-unmodulated"; cases[8].velocity = 0;
    cases[8].config.volume_mod = 0;
    cases[9].name = "repeat-trigger"; cases[9].blocks = 20; cases[9].retrigger = 8;
    cases[9].config.amp_repeat = 2; cases[9].config.amp_attack = 0.25f;
    cases[10].name = "parameter-minima";
    cases[10].config.mix = 0.f; cases[10].config.volume = 0.f;
    cases[10].config.noise_frequency = 0.f; cases[10].config.filter_frequency = 0.f;
    cases[10].config.filter_resonance = 1.f; cases[10].config.filter_drive = 0.4f;
    cases[10].config.transient_volume = 0.f; cases[10].config.distortion_shape = 0;
    cases[11].name = "parameter-maxima";
    cases[11].config.mix = 1.f; cases[11].config.volume = 1.f;
    cases[11].config.noise_frequency = 22000.f; cases[11].config.filter_frequency = 0.45f;
    cases[11].config.filter_resonance = 0.02f; cases[11].config.filter_drive = 6.4f;
    cases[11].config.transient_waveform = 13; cases[11].config.transient_volume = 1.f;
    cases[11].config.transient_pitch = 1.f + (127.f / 33.9f - 0.75f);
    cases[11].config.distortion_shape = 127;

    DiffResult mixed {};
    for (const auto& c : cases) {
        const DiffResult result = runDifferential(c.name, c.config, c.velocity,
            c.blocks, c.retrigger, assets.tables, noteFreq.data());
        check("original-C differential is bit exact", result.mismatches == 0 &&
              result.maxDifference == 0 && result.differenceRms == 0);
        check("render buffers retain canaries", result.canaries);
        check("rendered PCM is finite", result.finite);
        const bool endpointSilence = std::strcmp(c.name, "parameter-minima") == 0;
        if (std::strcmp(c.name, "velocity-zero-modulated") == 0)
            check("zero velocity mutes when velocity modulation is enabled", result.peak == 0);
        else if (endpointSilence)
            check("minimum output volume mutes the endpoint case", result.peak == 0);
        else
            check("configured snare path produces nonzero PCM", result.peak > 0);
        if (std::strcmp(c.name, "mixed") == 0) mixed = result;
    }
    std::printf("SNARE_PCM peak=%d rms=%.12g samples=%d\n", mixed.peak, mixed.rms, mixed.samples);

    // The original leaves noise pitch independent of the incoming MIDI note.
    RefSnareConfig noteCase = baseConfig();
    lxr::SnareVoice noteSnare {}; noteSnare.init(); configurePort(noteSnare, noteCase);
    noteSnare.trigger(100, 24, noteFreq.data()); const float lowNoise = noteSnare.noiseOsc.freq;
    noteSnare.trigger(100, 100, noteFreq.data());
    check("noise oscillator is not transposed by MIDI note", noteSnare.noiseOsc.freq == lowNoise &&
          lowNoise == noteCase.noise_frequency);

    // Tonal-only envelope makes decay observable without random-noise variance.
    RefSnareConfig decay = baseConfig(); decay.mix = 0; decay.tonal_waveform = lxr::OSC_SINE;
    decay.amp_decay = 0.2f;
    lxr::SnareVoice decaySnare {}; decaySnare.init(); decaySnare.rng.state = kSeed;
    configurePort(decaySnare, decay); decaySnare.trigger(127, 60, noteFreq.data());
    std::array<int16_t, 32> block {};
    double firstEnergy = 0, lastEnergy = 0;
    for (int b = 0; b < 10; ++b) {
        decaySnare.calcAsync(noteFreq.data());
        decaySnare.calcSyncBlock(block.data(), 32, assets.tables);
        double energy = 0; for (int16_t s : block) energy += static_cast<double>(s) * s;
        if (b == 0) firstEnergy = energy;
        if (b == 9) lastEnergy = energy;
    }
    check("amplitude envelope decays tonal energy to zero", firstEnergy > 0 && lastEnergy == 0);

    // Native engine, mixer routing, channel 4 MIDI and FIR integration.
    lxr::Engine engine;
    lxr::ParameterArray parameters;
    check("native engine assets loaded", engine.loadAssets("data"));
    parameters.init(); parameters.applyAllToEngine(engine);
    lxr::MidiParser midi;
    midi.processEvent({0x93, 3, 60, 110}, engine, parameters);
    const NativeStats native = renderNative(engine, 100);
    std::printf("NATIVE_SNARE peak=%d rms=%.12g nonzero_frames=%d\n",
                native.peak, native.rms, native.nonzero);
    check("MIDI channel 4 triggers native snare on ST1", native.peak > 0 && native.nonzero > 0);

    engine.init(); parameters.applyAllToEngine(engine);
    midi.processEvent({0x90, 0, 60, 110}, engine, parameters);
    check("other drum voice remains functional", renderNative(engine, 32).peak > 0);

    lxr::LXRAudioAdapter adapter;
    engine.init(); parameters.applyAllToEngine(engine);
    midi.processEvent({0x93, 3, 60, 110}, engine, parameters);
    check("48 kHz FIR configured", adapter.configure(48000.));
    double hostPeak = 0, hostEnergy = 0; bool hostFinite = true;
    rendering = true;
    for (int i = 0; i < 8192; ++i) {
        double l = 0, r = 0;
        adapter.getNextHostST1Sample(engine, l, r);
        hostFinite &= std::isfinite(l) && std::isfinite(r);
        hostPeak = std::max({hostPeak, std::abs(l), std::abs(r)});
        hostEnergy += l*l + r*r;
    }
    rendering = false;
    const double hostRms = std::sqrt(hostEnergy / (8192. * 2.));
    std::printf("FIR_SNARE rate=48000 peak=%.12g rms=%.12g latency=%u\n",
                hostPeak, hostRms, lxr::firLatency::hostSamples(48000.));
    check("FIR snare output is finite and nonzero", hostFinite && hostPeak > 0 && hostRms > 0);
    check("FIR latency remains 26 samples at 48 kHz",
          lxr::firLatency::hostSamples(48000.) == 26);
    check("no C++ allocation during measured render paths", renderAllocations == 0);

    std::printf("SUMMARY pass=%d fail=%d renderAllocations=%d\n",
                passed, failed, renderAllocations);
    return failed ? 1 : 0;
}
