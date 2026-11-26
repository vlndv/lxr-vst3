// Qualification in an owned, synchronous VST3 host. No remote-process reads.
#include "../vendor/iPlug2/Examples/LXR/LXR.h"
#include "IControl.h"
#define LXR_P19E_EDITOR_TEST_HELPERS_ONLY
#include "P19EVST3SequencerGUITest.cpp"
#undef LXR_P19E_EDITOR_TEST_HELPERS_ONLY
#ifdef fopen
#undef fopen
#endif

#include <chrono>

using namespace lxr::sequencer;

struct P16F3VST3CertificationProbe {
    static LXR& plugin(HostInstance& host)
    {
        return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(
            static_cast<SingleComponentEffect*>(host.component)));
    }

    static const TriggerScheduler& scheduler(HostInstance& host) { return plugin(host).mSequencer; }
    static uint64_t boundary(HostInstance& host) { return plugin(host).mAudioAdapter.nativeFrame(); }
    static uint64_t generation(HostInstance& host) { return plugin(host).mAppliedPattern.generation(); }
    static uint64_t published(HostInstance& host) { return scheduler(host).patternAuthority().generation(); }
    static bool import(HostInstance& host, const char* path, HMODULE module, uintptr_t rva)
    {
        using Import = bool (*)(LXR*, const char*);
        return reinterpret_cast<Import>(reinterpret_cast<uintptr_t>(module) + rva)(&plugin(host), path);
    }

    static bool visibleImport(HostInstance& host)
    {
        auto* graphics = plugin(host).GetUI();
        for (int i = 0; i < graphics->NControls(); ++i)
        {
            const auto* control = graphics->GetControl(i);
            const auto& rect = control->GetRECT();
            if (!control->IsHidden() && rect.L == 1125 && rect.R == 1240 &&
                rect.T == 635 && rect.B == 675)
                return true;
        }
        return false;
    }
};

namespace {
using Probe = P16F3VST3CertificationProbe;
struct Record {
    uint64_t hostFrame;
    TriggerEvent event;
};

// The original one-frame qualification advances by at most one native block.
// Storage belongs to the test host and is written only after process returns;
// partitionMatrix() separately exposes the consequence for longer callbacks.
template <typename T, std::size_t Capacity>
struct Capture {
    std::array<T, Capacity> records {};
    std::size_t size = 0, dropped = 0;
    void append(const T& record)
    {
        if (size < records.size()) records[size++] = record;
        else ++dropped;
    }
};

struct Render {
    AudioBusBuffers bus {};
    ProcessContext context {};
    ProcessData data {};
    double samples[2] {};
    double* channels[2] {&samples[0], &samples[1]};
    explicit Render(unsigned bpm)
    {
        bus.numChannels = 2;
        bus.channelBuffers64 = channels;
        context.state = ProcessContext::kPlaying | ProcessContext::kTempoValid |
                        ProcessContext::kProjectTimeMusicValid;
        context.sampleRate = 48000;
        context.tempo = bpm;
        data.processMode = kOffline;
        data.symbolicSampleSize = kSample64;
        data.numSamples = 1;
        data.numOutputs = 1;
        data.outputs = &bus;
        data.processContext = &context;
    }
    bool tick(HostInstance& instance, int64_t frame)
    {
        context.projectTimeSamples = frame;
        context.projectTimeMusic = frame * context.tempo / 2880000.0;
        return instance.processor->process(data) == kResultOk;
    }
};

struct PartitionResult
{
    std::vector<double> pcm;
    Capture<Record, 256> triggers;
    Capture<AutomationEvent, 512> locks;
    std::vector<uint8_t> state;
    TriggerSchedulerDiagnostics diagnostics {};
    uint32_t rng = 0;
    uint64_t callbacks = 0;
    uint64_t peakCallbackNanoseconds = 0;
    bool processOk = true;
};

PartitionResult runPartition(IPluginFactory* factory, const TUID id,
                             std::vector<uint8_t>& saved, unsigned frames)
{
    HostInstance host(factory, id);
    host.setup(48000);
    require("restore partition fixture", host.loadState(saved));
    PartitionResult result;
    constexpr int64_t kDuration = 96000; // One complete four-beat pattern.
    result.pcm.resize(static_cast<std::size_t>(kDuration) * 2u);
    std::array<double, 1024> left {};
    std::array<double, 1024> right {};
    double* channels[2] {left.data(), right.data()};
    AudioBusBuffers bus {};
    bus.numChannels = 2;
    bus.channelBuffers64 = channels;
    ProcessContext context {};
    context.state = ProcessContext::kPlaying | ProcessContext::kTempoValid |
                    ProcessContext::kProjectTimeMusicValid;
    context.sampleRate = 48000;
    context.tempo = 120.0;
    ProcessData data {};
    data.processMode = kOffline;
    data.symbolicSampleSize = kSample64;
    data.numOutputs = 1;
    data.outputs = &bus;
    data.processContext = &context;
    const auto initial = Probe::scheduler(host).diagnostics();
    uint64_t previousNative = Probe::boundary(host);
    for (int64_t start = 0; start < kDuration; start += frames)
    {
        const int count = static_cast<int>(std::min<int64_t>(frames, kDuration - start));
        data.numSamples = count;
        context.projectTimeSamples = start;
        context.projectTimeMusic = start * context.tempo / 2880000.0;
        const auto began = std::chrono::steady_clock::now();
        result.processOk &= host.processor->process(data) == kResultOk;
        const auto elapsed = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - began).count());
        result.peakCallbackNanoseconds = std::max(result.peakCallbackNanoseconds, elapsed);
        ++result.callbacks;
        for (int i = 0; i < count; ++i)
        {
            const auto output = static_cast<std::size_t>(start + i) * 2u;
            result.pcm[output] = left[static_cast<std::size_t>(i)];
            result.pcm[output + 1u] = right[static_cast<std::size_t>(i)];
        }
        const auto native = Probe::boundary(host);
        if (native != previousNative)
        {
            // This is deliberately the existing test-host-only consumer. For
            // callbacks spanning multiple native blocks it can see only the
            // scheduler's final batch; the emitted diagnostics below expose
            // any records that were not retainable instead of hiding loss.
            for (const auto& event : Probe::scheduler(host).lastBatch())
                result.triggers.append({static_cast<uint64_t>(start), event});
            for (const auto& event : Probe::scheduler(host).lastAutomationBatch())
                result.locks.append(event);
            previousNative = native;
        }
    }
    result.state = host.saveState();
    result.rng = Probe::scheduler(host).rngState();
    result.diagnostics = Probe::scheduler(host).diagnostics();
    result.diagnostics.emittedEvents -= initial.emittedEvents;
    result.diagnostics.emittedAutomationEvents -= initial.emittedAutomationEvents;
    result.diagnostics.processedNativeBlocks -= initial.processedNativeBlocks;
    return result;
}

void partitionMatrix(IPluginFactory* factory, const TUID id,
                     std::vector<uint8_t>& saved, const char* scenario)
{
    const std::array<unsigned, 11> sizes {{1,16,31,32,33,64,127,128,256,512,1024}};
    const auto reference = runPartition(factory, id, saved, sizes[0]);
    check("partition reference completes and retains complete dense trace",
          reference.processOk && reference.triggers.dropped == 0 &&
          reference.locks.dropped == 0 &&
          reference.triggers.size == reference.diagnostics.emittedEvents &&
          reference.locks.size == reference.diagnostics.emittedAutomationEvents);
    for (const unsigned frames : sizes)
    {
        const auto measured = frames == 1u ? reference : runPartition(factory, id, saved, frames);
        bool subsequence = true;
        std::size_t triggerCursor = 0u;
        for (std::size_t i = 0; i < measured.triggers.size; ++i)
        {
            const auto order = measured.triggers.records[i].event.stableOrder;
            while (triggerCursor < reference.triggers.size &&
                   reference.triggers.records[triggerCursor].event.stableOrder != order)
                ++triggerCursor;
            subsequence &= triggerCursor < reference.triggers.size;
            if (triggerCursor < reference.triggers.size)
            {
                const auto& expected = reference.triggers.records[triggerCursor].event;
                const auto& actual = measured.triggers.records[i].event;
                subsequence &= actual.nativeBoundary == expected.nativeBoundary &&
                    actual.pattern == expected.pattern && actual.track == expected.track &&
                    actual.physicalPosition == expected.physicalPosition &&
                    actual.snapshotGeneration == expected.snapshotGeneration;
                ++triggerCursor;
            }
        }
        const bool pcm = measured.pcm == reference.pcm;
        const bool state = measured.state == reference.state;
        const bool rng = measured.rng == reference.rng;
        const bool counts = measured.diagnostics.emittedEvents ==
                reference.diagnostics.emittedEvents &&
            measured.diagnostics.emittedAutomationEvents ==
                reference.diagnostics.emittedAutomationEvents;
        const auto missingTriggers = measured.diagnostics.emittedEvents -
            static_cast<uint64_t>(measured.triggers.size);
        const auto missingLocks = measured.diagnostics.emittedAutomationEvents -
            static_cast<uint64_t>(measured.locks.size);
        check("callback partition preserves exact PCM, musical state, RNG and emitted counts",
              measured.processOk && pcm && state && rng && counts);
        check("retained post-callback records are an ordered source-trace subsequence",
              subsequence && measured.triggers.dropped == 0 && measured.locks.dropped == 0);
        std::printf("PARTITION scenario=%s frames=%u callbacks=%llu native_blocks=%llu emitted_triggers=%llu retained_triggers=%zu missing_triggers=%llu emitted_locks=%llu retained_locks=%zu missing_locks=%llu pcm=%u state=%u rng=%u overflow=%zu peak_callback_ns=%llu\n",
                    scenario, frames, static_cast<unsigned long long>(measured.callbacks),
                    static_cast<unsigned long long>(measured.diagnostics.processedNativeBlocks),
                    static_cast<unsigned long long>(measured.diagnostics.emittedEvents),
                    measured.triggers.size, static_cast<unsigned long long>(missingTriggers),
                    static_cast<unsigned long long>(measured.diagnostics.emittedAutomationEvents),
                    measured.locks.size, static_cast<unsigned long long>(missingLocks),
                    pcm ? 1u : 0u, state ? 1u : 0u, rng ? 1u : 0u,
                    measured.triggers.dropped + measured.locks.dropped,
                    static_cast<unsigned long long>(measured.peakCallbackNanoseconds));
    }
}

void liveCopy(IPluginFactory* factory, const TUID id, std::vector<uint8_t>& saved)
{
    HostInstance host(factory, id), peer(factory, id);
    host.setup(48000);
    peer.setup(48000);
    require("restore live-copy source", host.loadState(saved) && peer.loadState(saved));
    const auto peerBefore = peer.saveState();
    AttachedEditor editor(host);
    editor.click(634, 96);
    Render render(120);
    uint64_t published = 0u;
    unsigned before = 0u, after = 0u;
    bool exact = true;
    uint64_t nextNative = 0u;
    for (int64_t frame = 0; frame < 13500; ++frame)
    {
        if (frame == 3000)
        {
            const auto prior = Probe::published(host);
            editor.click(485, 604);
            published = Probe::published(host);
            check("running copy publishes exactly one new generation", published == prior + 1u);
        }
        if (!render.tick(host, frame)) require("live copy VST3 process succeeds", false);
        if (Probe::boundary(host) == nextNative) continue;
        nextNative = Probe::boundary(host);
        for (const auto& event : Probe::scheduler(host).lastBatch())
        {
            if (frame < 3000) { ++before; exact &= event.track == 0u; }
            else
            {
                ++after;
                exact &= (event.track == 0u || event.track == 1u) &&
                         event.snapshotGeneration == published;
            }
        }
    }
    check("running copy is consumed at a native boundary with correct destination",
          exact && before == 1u && after == 4u && Probe::generation(host) == published);
    check("running copy remains instance-owned", peer.saveState() == peerBefore);
    std::printf("LIVE_COPY before=%u after=%u published=%llu consumed=%llu destination_track=1 peer_unchanged=1\n",
                before, after, static_cast<unsigned long long>(published),
                static_cast<unsigned long long>(Probe::generation(host)));
}

void run(IPluginFactory* factory, const TUID id, std::vector<uint8_t>& saved,
         unsigned bpm, unsigned expectedTracks, unsigned beats, const char* label)
{
    HostInstance watched(factory, id), blind(factory, id);
    watched.setup(48000);
    blind.setup(48000);
    require("restore matched state", watched.loadState(saved) && blind.loadState(saved));
    Render a(bpm), b(bpm);
    Capture<Record, 256> triggers;
    Capture<AutomationEvent, 256> locks;
    bool samePcm = true, precision = true, oneBoundary = true, audible = false;
    bool order = true;
    uint64_t previousNative = 0, previousHost = 0;
    const auto& scheduler = Probe::scheduler(watched);
    const auto initialDiagnostics = scheduler.diagnostics();
    const int64_t end = 2880000 * beats / bpm;
    for (int64_t frame = 0; frame < end; ++frame)
    {
        if (!a.tick(watched, frame) || !b.tick(blind, frame))
            require("both VST3 process calls succeed", false);
        samePcm &= std::memcmp(a.samples, b.samples, sizeof(a.samples)) == 0;
        audible |= a.samples[0] != 0.0 || a.samples[1] != 0.0;
        const auto native = Probe::boundary(watched);
        if (native == previousNative) continue;
        oneBoundary &= native == previousNative + 32u;
        // The host sample that first needs this native index uses the fixed
        // source rate 5632353/128 at 48 kHz. None of the tested boundaries
        // approach a nonzero exact-integer floating-point tie.
        precision &= static_cast<uint64_t>(frame) ==
            (previousNative * 6144000u + 5632352u) / 5632353u;
        for (const auto& e : scheduler.lastBatch())
        {
            const auto right = static_cast<uint64_t>(frame) * bpm * 32u;
            const auto due = e.clockOrdinal * 2880000u;
            precision &= (e.nativeBoundary == previousNative && due <= right &&
                (frame == 0 || due > previousHost * bpm * 32u));
            order &= e.kind == TriggerKind::Step && e.pattern == 0 &&
                     e.track < expectedTracks && e.physicalPosition == e.clockOrdinal &&
                     e.physicalPosition % 8u == 0u &&
                     e.snapshotGeneration == Probe::generation(watched);
            if (triggers.size)
                order &= e.stableOrder > triggers.records[triggers.size - 1].event.stableOrder;
            triggers.append({static_cast<uint64_t>(frame), e});
        }
        for (const auto& e : scheduler.lastAutomationBatch())
        {
            order &= e.nativeBoundary == previousNative && e.slot < 2u &&
                     e.snapshotGeneration == Probe::generation(watched);
            locks.append(e);
        }
        previousHost = frame;
        previousNative = native;
    }
    const auto& d = scheduler.diagnostics();
    check("at most one native block per callback, actual delta at every refill", oneBoundary);
    check("trigger occurs on first native boundary past rational due frame", precision);
    check("exact successive stable order and applied snapshot", order);
    check("no capture drops and all dispatched records retained",
          triggers.dropped == 0 && locks.dropped == 0 &&
          triggers.size == d.emittedEvents - initialDiagnostics.emittedEvents &&
          locks.size == d.emittedAutomationEvents - initialDiagnostics.emittedAutomationEvents);
    check("no scheduler overflow or lock application failure",
          d.rejectedOverflowEvents == 0 && d.rejectedAutomationOverflowEvents == 0 &&
          d.engineAutomationApplicationFailures == 0);
    check("independent count: four main boundaries per beat per track",
          triggers.size == 4u * beats * expectedTracks && locks.size == 2u * triggers.size);
    for (std::size_t i = 0; i < triggers.size; ++i)
    {
        const auto& r = triggers.records[i];
        check("independent track and physical-position sequence",
              expectedTracks && r.event.track == i % expectedTracks &&
              r.event.physicalPosition == (i / expectedTracks) * 8u);
        std::printf("EVENT %s %u host=%llu native=%llu generation=%llu order=%llu track=%u pos=%u note=%u velocity=%u\n",
                    label, bpm, static_cast<unsigned long long>(r.hostFrame),
                    static_cast<unsigned long long>(r.event.nativeBoundary),
                    static_cast<unsigned long long>(r.event.snapshotGeneration),
                    static_cast<unsigned long long>(r.event.stableOrder), unsigned(r.event.track),
                    unsigned(r.event.physicalPosition), unsigned(r.event.note), unsigned(r.event.velocity));
    }
    check("capture-on and capture-off PCM, RNG, trigger count and state identical",
          samePcm && watched.saveState() == blind.saveState() &&
          scheduler.rngState() == Probe::scheduler(blind).rngState() &&
          d.emittedEvents == Probe::scheduler(blind).diagnostics().emittedEvents);
    check("audible PCM follows active bank", audible == (expectedTracks != 0));
    check("PDC 26 at 48 kHz", watched.processor->getLatencySamples() == 26);
    std::printf("CAPTURE %s bpm=%u callbacks=%lld triggers=%zu locks=%zu drops=%zu native_blocks=%llu\n",
                label, bpm, static_cast<long long>(end), triggers.size, locks.size,
                triggers.dropped + locks.dropped, static_cast<unsigned long long>(d.processedNativeBlocks));
}

void liveClear(IPluginFactory* factory, const TUID id, std::vector<uint8_t>& saved)
{
    HostInstance host(factory, id);
    host.setup(48000);
    require("restore copied tracks", host.loadState(saved));
    AttachedEditor editor(host);
    editor.click(634, 96);
    Render render(120);
    unsigned original = 0, afterTrack = 0, afterPattern = 0;
    bool valid = true;
    uint64_t applied = 0, publishTrack = 0, publishPattern = 0, nextNative = 0;
    for (int64_t frame = 0; frame < 13500; ++frame)
    {
        if (frame == 3000)
        {
            const auto before = Probe::published(host);
            editor.click(595, 604);
            check("clear-track first click preserves generation", Probe::published(host) == before);
            editor.click(595, 604);
            publishTrack = Probe::published(host);
            check("clear-track publishes once while transport running", publishTrack == before + 1);
        }
        if (frame == 9000)
        {
            const auto before = Probe::published(host);
            editor.click(277, 604);
            check("clear-pattern first click preserves generation", Probe::published(host) == before);
            editor.click(277, 604);
            publishPattern = Probe::published(host);
            check("clear-pattern publishes once while transport running", publishPattern == before + 1);
        }
        if (!render.tick(host, frame)) require("live clear VST3 process succeeds", false);
        if (Probe::boundary(host) == nextNative) continue;
        nextNative = Probe::boundary(host);
        const auto& events = Probe::scheduler(host).lastBatch();
        if (frame < 3000) original += static_cast<unsigned>(events.size());
        else if (frame < 9000)
        {
            afterTrack += static_cast<unsigned>(events.size());
            for (const auto& e : events)
                valid &= e.track == 1u && e.physicalPosition == 8u && e.snapshotGeneration == publishTrack;
        }
        else
        {
            afterPattern += static_cast<unsigned>(events.size());
            applied = Probe::generation(host);
        }
    }
    check("live clear reaches the native scheduler with the expected event consequences",
          valid && original == 2 && afterTrack == 1 && afterPattern == 0 && applied == publishPattern);
    std::printf("LIVE_CLEAR original=%u after_track=%u after_pattern=%u track_generation=%llu pattern_generation=%llu applied=%llu\n",
                original, afterTrack, afterPattern, static_cast<unsigned long long>(publishTrack),
                static_cast<unsigned long long>(publishPattern), static_cast<unsigned long long>(applied));
}

void writeBank(const std::string& path, const std::vector<uint8_t>& bytes)
{
    auto* file = (std::fopen)(path.c_str(), "wb");
    require("create disposable bank fixture", file != nullptr);
    require("write fixture", std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size());
    require("close fixture", std::fclose(file) == 0);
}

void checksum(std::vector<uint8_t>& bytes)
{
    state::Put32(bytes.data() + 20, state::Crc(bytes.data(), bytes.size(), 20));
}
}

int main(int argc, char** argv)
{
    if (argc != 4 && argc != 2) return 2;
    const auto module = LoadLibraryA(argv[1]);
    require("load exact VST3", module != nullptr);
    const auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
    const auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
    const auto createFactory = reinterpret_cast<IPluginFactory* (*)()>(GetProcAddress(module, "GetPluginFactory"));
    require("initialize exact VST3", init && exitDll && createFactory && init());
    auto* factory = createFactory();
    PClassInfo info {};
    bool found = false;
    for (int i = 0; i < factory->countClasses(); ++i)
        if (factory->getClassInfo(i, &info) == kResultOk &&
            std::strcmp(info.category, kVstAudioEffectClass) == 0)
        { found = true; break; }
    require("locate exact audio component", found);
    {
        HostInstance h(factory, info.cid), independent(factory, info.cid);
        h.setup(48000);
        const auto untouched = independent.saveState();
        AttachedEditor editor(h);
        check("Import hidden outside Sequencer", !Probe::visibleImport(h));
        editor.click(634, 96);
        check("Sequencer has reachable Import bank hit target", Probe::visibleImport(h));
        if (argc == 4)
        {
            const auto original = h.saveState();
            const char magic[] = "LXQB";
            const auto begin = std::search(original.begin(), original.end(), magic, magic + 4);
            require("default state has complete LXQB", original.end() - begin >= state::kModuleBytes);
            std::vector<uint8_t> bank(begin, begin + state::kModuleBytes);
            state::Put16(bank.data() + 32 + 8 * 7 * 128 * 7, 0x000f);
            checksum(bank);
            const std::string dir = argv[2];
            const auto rva = std::strtoull(argv[3], nullptr, 10);
            const auto path = dir + "/valid.lxqb";
            writeBank(path, bank);
            require("call existing production importer", rva && Probe::import(h, path.c_str(), module, rva));
            const auto imported = h.saveState();
            lxr::state::ProjectCandidate parsed;
            require("decode imported LXRS", decode(imported, parsed));
            check("distinct expected imported gates and untouched peer",
                  parsed.bank.patterns[0].mainGates[0] == 15u && independent.saveState() == untouched);
            for (unsigned test = 0; test < 8; ++test)
            {
                auto invalid = bank;
                switch (test)
                {
                case 0: invalid.clear(); break;
                case 1: invalid.pop_back(); break;
                case 2: invalid.push_back(0); break;
                case 3: invalid[20] ^= 1; break;
                case 4: invalid[0] = 'K'; checksum(invalid); break;
                case 5: invalid[4] = 2; checksum(invalid); break;
                case 6: invalid[33] = 255; checksum(invalid); break;
                case 7: invalid[28] = 1; checksum(invalid); break;
                }
                const auto bad = dir + "/rejected-" + std::to_string(test) + ".lxqb";
                writeBank(bad, invalid);
                check("invalid import is rejected atomically",
                      !Probe::import(h, bad.c_str(), module, rva) && h.saveState() == imported);
            }
            h.processParameter(240, 1.0); // Enable
            h.processParameter(241, 1.0); // Host Sync
            auto saved = h.saveState();
            for (unsigned bpm : {60u, 90u, 120u, 140u})
                run(factory, info.cid, saved, bpm, 1, 1, "import");
            // Generate through the real editor on track zero, then copy that
            // track to track one. The first beat has the 16-pulse fixture.
            for (unsigned i = 0; i < 16; ++i) editor.click(1173, 604);
            editor.click(1062, 655);
            saved = h.saveState();
            require("decode generated bank", decode(saved, parsed));
            check("Euclidean 16/16 gate oracle", parsed.bank.patterns[0].mainGates[0] == 0xffff);
            run(factory, info.cid, saved, 120, 1, 4, "euclidean");
            partitionMatrix(factory, info.cid, saved, "euclidean");
            liveCopy(factory, info.cid, saved);
            // The editor defaults its explicit copy destination to track 1.
            editor.click(485, 604);
            saved = h.saveState();
            run(factory, info.cid, saved, 120, 2, 4, "copy");
            partitionMatrix(factory, info.cid, saved, "copy");
            liveClear(factory, info.cid, saved);
            editor.click(595, 604);
            editor.click(595, 604); // Confirm clear of source track.
            editor.click(277, 604);
            editor.click(277, 604); // Confirm clear of complete pattern.
            saved = h.saveState();
            run(factory, info.cid, saved, 120, 0, 1, "clear");
            Capture<unsigned, 2> overflow;
            for (unsigned i = 0; i < 4; ++i) overflow.append(i);
            check("bounded capture drops new records and preserves unread order",
                  overflow.size == 2 && overflow.dropped == 2 && overflow.records[0] == 0 && overflow.records[1] == 1);
        }
        check("peer state stays independent across editor operations", independent.saveState() == untouched);
    }
    factory->release();
    require("clean DLL exit", exitDll());
    FreeLibrary(module);
    std::printf("SUMMARY pass=%d fail=%d\n", passed, failed);
    return failed ? 1 : 0;
}
