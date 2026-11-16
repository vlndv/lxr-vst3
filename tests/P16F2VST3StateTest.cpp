// Exact production binary: project state loader and editor's production import.
#include "../vendor/iPlug2/Examples/LXR/LXR.h"
#define main ExistingP12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main
#include <thread>
#include <atomic>
#include <chrono>
#ifdef fopen
#undef fopen
#endif

struct P16F2VST3StateTestProbe
{
    static LXR& plugin(HostInstance& h)
    { return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(static_cast<SingleComponentEffect*>(h.component))); }
    static bool import(HostInstance& h, const char* path)
    {
        // Test fixture preparation; only the production setState publishes.
        // The actual file-dialog ingress is certified separately in REAPER.
        auto* file = (std::fopen)(path, "rb"); if (!file) return false;
        lxr::sequencer::state::EncodedBank bankBytes;
        const auto n = std::fread(bankBytes.data(), 1, bankBytes.size(), file);
        const bool exact = n == bankBytes.size() && std::fgetc(file) == EOF;
        std::fclose(file);
        auto saved = h.saveState(); lxr::state::ProjectCandidate c;
        if (!exact || !lxr::state::DecodeProject(saved.data(), saved.size(), c, true) ||
            !lxr::sequencer::state::Decode(bankBytes.data(), bankBytes.size(), c.bank)) return false;
        lxr::state::ProjectBytes bytes; std::size_t size = 0;
        if (!lxr::state::EncodeProject(c, bytes, size, true)) return false;
        std::vector<std::uint8_t> state(bytes.begin(), bytes.begin() + size);
        return h.loadState(state);
    }
    static bool hold(HostInstance& h, lxr::RoutingStateAuthority::Guard& g)
    { return plugin(h).mRoutingState.tryAcquire(g); }
    static void release(HostInstance& h, lxr::RoutingStateAuthority::Guard& g)
    { plugin(h).mRoutingState.release(g); }
    static bool coherent(HostInstance& h, const lxr::state::ProjectCandidate& c)
    {
        auto& p = plugin(h);
        bool same = std::equal(c.patch.begin(), c.patch.end(), p.mParams.values) &&
            p.mAudioTransport.enabled == c.transport.enabled &&
            p.mAudioTransport.clock == c.transport.clock &&
            p.mAudioTransport.bpm == c.transport.bpm &&
            p.mAudioTransport.pattern == c.transport.pattern &&
            p.mAudioTransport.seed == c.transport.seed;
        const auto& bank = p.mAppliedPattern.bank();
        for (std::size_t i = 0; i < 8; ++i)
        {
            same &= bank.patterns[i].mainGates == c.bank.patterns[i].mainGates &&
                bank.patterns[i].lengthCodes == c.bank.patterns[i].lengthCodes &&
                bank.patterns[i].rotations == c.bank.patterns[i].rotations;
            for (std::size_t t = 0; t < 7; ++t)
                for (std::size_t s = 0; s < 128; ++s)
                {
                    const auto& a = bank.patterns[i].steps[t][s];
                    const auto& b = c.bank.patterns[i].steps[t][s];
                    same &= a.volume == b.volume && a.probability == b.probability &&
                        a.note == b.note && a.parameter1 == b.parameter1 && a.value1 == b.value1 &&
                        a.parameter2 == b.parameter2 && a.value2 == b.value2;
                }
        }
        for (std::size_t i = 0; i < 12; ++i)
            same &= p.mAppliedRouting.routes[i].originalIndex == c.routing.routes[i].originalIndex;
        return same && bank.shuffle == c.bank.shuffle;
    }
    static bool pin(HostInstance& h, lxr::sequencer::PatternStateAuthority::ReadHandle& pin)
    { return plugin(h).mSequencer.patternAuthority().tryAcquire(pin) == lxr::sequencer::Status::Ok; }
    static bool reserve(HostInstance& h, lxr::sequencer::PatternStateAuthority::WriteHandle& writer)
    { return plugin(h).mSequencer.patternAuthority().tryBeginEdit(writer) == lxr::sequencer::Status::Ok; }
    static const lxr::sequencer::TriggerScheduler& sequencer(HostInstance& h)
    { return plugin(h).mSequencer; }
};

namespace {
using Candidate = lxr::state::ProjectCandidate;
std::int64_t maximumBoundaryNs = 0;
std::vector<std::uint8_t> wire(const Candidate& c)
{
    lxr::state::ProjectBytes b;
    std::size_t n = 0;
    require("encode production project", lxr::state::EncodeProject(c, b, n, true));
    return {b.begin(), b.begin() + n};
}
Candidate state(HostInstance& h)
{
    auto b = h.saveState(); Candidate c;
    require("decode production project", lxr::state::DecodeProject(b.data(), b.size(), c, true));
    return c;
}
void boundary(HostInstance& h, std::int64_t position = 0, int n = 128,
              double bpm = 120, bool playing = true)
{
    std::array<double, 512> l {}, r {}; double* channels[] {l.data(), r.data()};
    AudioBusBuffers output {}; output.numChannels = 2; output.channelBuffers64 = channels;
    ProcessContext context {}; context.sampleRate = 48000;
    context.state = ProcessContext::kTempoValid | ProcessContext::kProjectTimeMusicValid |
        (playing ? ProcessContext::kPlaying : 0);
    context.tempo = bpm; context.projectTimeSamples = position;
    context.projectTimeMusic = position * bpm / (48000.0 * 60.0);
    ProcessData d {}; d.processMode = kOffline; d.symbolicSampleSize = kSample64;
    d.numSamples = n; d.numOutputs = 1; d.outputs = &output; d.processContext = &context;
    const auto started = std::chrono::steady_clock::now();
    const auto result = h.processor->process(d);
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - started).count();
    maximumBoundaryNs = std::max(maximumBoundaryNs, static_cast<std::int64_t>(elapsed));
    require("process production state boundary", result == kResultOk);
}
struct PCM { std::vector<double> samples; double peak = 0; };
PCM render(HostInstance& h, int blockSize = 127, double bpm = 120)
{
    PCM pcm; pcm.samples.reserve(192000);
    std::array<double, 512> l {}, r {}; double* channels[] {l.data(), r.data()};
    AudioBusBuffers output {}; output.numChannels = 2; output.channelBuffers64 = channels;
    for (int at = 0; at < 96000;)
    {
        const int n = std::min(blockSize, 96000-at);
        ProcessContext c {}; c.sampleRate = 48000; c.tempo = bpm;
        c.state = ProcessContext::kTempoValid | ProcessContext::kProjectTimeMusicValid | ProcessContext::kPlaying;
        c.projectTimeSamples = at; c.projectTimeMusic = at * bpm / (48000.0 * 60.0);
        ProcessData d {}; d.processMode = kOffline; d.symbolicSampleSize = kSample64;
        d.numSamples = n; d.numOutputs = 1; d.outputs = &output; d.processContext = &c;
        require("render production sequence", h.processor->process(d) == kResultOk);
        for (int i = 0; i < n; ++i)
        {
            pcm.samples.push_back(l[i]); pcm.samples.push_back(r[i]);
            pcm.peak = std::max({pcm.peak, std::abs(l[i]), std::abs(r[i])});
        }
        at += n;
    }
    return pcm;
}
}

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    auto module = LoadLibraryA(argv[1]); require("load exact private module", module != nullptr);
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
    auto end = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
    auto factoryFn = reinterpret_cast<IPluginFactory* (*)()>(GetProcAddress(module, "GetPluginFactory"));
    require("production module exports", init && end && factoryFn && init());
    auto* factory = factoryFn(); PClassInfo info {}; bool found = false;
    for (int i = 0; i < factory->countClasses(); ++i)
        if (factory->getClassInfo(i, &info) == kResultOk && std::strcmp(info.category, kVstAudioEffectClass) == 0)
        { found = true; break; }
    require("production class found", found);
    {
        HostInstance h(factory, info.cid); h.setup(48000,512);
        auto initial = state(h);
        check("default empty bank silent without MIDI", render(h).peak == 0);
        require("production project loader accepts fixture bank", P16F2VST3StateTestProbe::import(h, argv[2]));
        auto c = state(h);
        check("imported nonempty gates observable", c.bank.patterns[0].mainGates[0] == 0x1111);
        check("import retains patch routes transport", c.patch == initial.patch && c.transport.enabled == 0);
        c.transport.enabled = 1; c.transport.clock = 1;
        auto bytes = wire(c); require("production loader accepts bank and config", h.loadState(bytes));
        auto first = render(h);
        check("production imported sequence audible", first.peak > 0);
        check("production imported sequence finite", std::all_of(first.samples.begin(), first.samples.end(), [](double x){return std::isfinite(x);}));
        check("all domains coherent after render", P16F2VST3StateTestProbe::coherent(h, c));
        require("reload matched seed and timeline", h.loadState(bytes));
        check("save reload exact PCM", render(h).samples == first.samples);
        require("reload variable block test", h.loadState(bytes));
        check("callback sizes 127 and 512 exact PCM", render(h, 512).samples == first.samples);
        const auto saved = h.saveState();
        auto corrupt = saved; corrupt[1000] ^= 1;
        check("production corrupt stream rejected", !h.loadState(corrupt));
        check("production rejection preserves full accepted state", h.saveState() == saved);
        const auto beforeImportReject = h.saveState();
        check("fixture missing file rejects", !P16F2VST3StateTestProbe::import(h, "missing-p16f2-file.lxqb"));
        check("failed import preserves accepted project", h.saveState() == beforeImportReject);
        HostInstance second(factory, info.cid); second.setup(48000);
        check("second default instance silent", render(second).peak == 0);
        check("second bank remains default", state(second).bank.patterns[0].mainGates[0] == 0);
        check("48 kHz PDC 26", h.processor->getLatencySamples() == 26);
        auto next = c; next.patch[88] = 23; next.transport.bpm = 60;
        next.bank.patterns[0].mainGates[0] = 0x0101;
        const auto nextBytes = wire(next); auto mutableNext = nextBytes;
        lxr::RoutingStateAuthority::Guard held;
        require("hold cross authority barrier", P16F2VST3StateTestProbe::hold(h, held));
        std::atomic<bool> returned {false}; bool accepted = true;
        std::thread restore([&] { accepted = h.loadState(mutableNext); returned.store(true); });
        restore.join();
        check("contended restore returns Busy without partial commit", returned && !accepted);
        boundary(h);
        check("audio retains old coherent tuple behind barrier", P16F2VST3StateTestProbe::coherent(h, c));
        P16F2VST3StateTestProbe::release(h, held);
        check("Busy rejection preserves full saved state", h.saveState() == saved);
        require("commit complete new tuple", h.loadState(mutableNext));
        check("accepted publication leaves prior audio tuple until boundary", P16F2VST3StateTestProbe::coherent(h, c));
        boundary(h, 0, 128, 60, false);
        check("one boundary selects complete new tuple while stopped", P16F2VST3StateTestProbe::coherent(h, next));
        check("controller transport readback restored", h.controller->normalizedParamToPlain(242,
            h.controller->getParamNormalized(242)) == 60);
        lxr::sequencer::PatternStateAuthority::ReadHandle pinA, pinB;
        require("pin accepted bank", P16F2VST3StateTestProbe::pin(h, pinA));
        require("publish bank B without processing", h.loadState(bytes));
        require("pin published B", P16F2VST3StateTestProbe::pin(h, pinB));
        require("publish bank C using third slot", h.loadState(mutableNext));
        const auto pinnedState = h.saveState();
        check("pinned inactive banks cause bounded Busy", !h.loadState(bytes));
        check("Busy never overwrites a pinned bank", h.saveState() == pinnedState);
        pinA.release(); pinB.release(); boundary(h, 0, 128, 60);
        require("publication succeeds after pins release", h.loadState(bytes));
        boundary(h);
        check("released retry coherent", P16F2VST3StateTestProbe::coherent(h, c));
        check("restoration emits no unsolicited edits", h.handler.edits == 0);
        const auto priorWriterContention = h.saveState();
        lxr::sequencer::PatternStateAuthority::WriteHandle reserved;
        require("reserve explicit concurrent pattern writer barrier", P16F2VST3StateTestProbe::reserve(h, reserved));
        check("concurrent pattern writer causes bounded restore Busy", !h.loadState(mutableNext));
        check("writer contention preserves all domains", h.saveState() == priorWriterContention);
        reserved.cancel();
    }
    factory->release(); end(); FreeLibrary(module);
    std::printf("SUMMARY pass=%d fail=%d plugin_bytes=%zu maximum_restore_callback_ns=%lld\n",
        passed, failed, sizeof(LXR), static_cast<long long>(maximumBoundaryNs));
    return failed ? 1 : 0;
}
