// Real VST3 interfaces, independent native expectations; no IParam-only oracle.
#if defined(LXR_R2_INTERNAL_PROBE)
// Match iPlug2's required SingleComponentEffect include order for its editor
// state name-clash workaround before including the external host helpers.
#include "../vendor/iPlug2/Examples/LXR/LXR.h"
#endif
#define main ExistingStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main
#include <atomic>
#include <thread>

#if defined(LXR_R2_INTERNAL_PROBE)
// This optional build uses the exact Release class layout/headers. External
// controller assertions below remain the oracle; these checks additionally
// inspect private processing projections and force the existing bounded gate.
struct LXRControllerSyncTestProbe {
    using Authority = lxr::ParameterStateAuthority<228>;
    static LXR& plugin(HostInstance& h) {
        return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(
            static_cast<SingleComponentEffect*>(h.component)));
    }
    static bool projections(HostInstance& h, int id, int intended) {
        auto& p = plugin(h);
        return p.mState.value(id) == intended && p.mParams.values[id] == intended &&
            p.GetParam(id)->Int() == intended;
    }
    static bool engine(HostInstance& h) {
        auto& p = plugin(h);
        const float drive = 0.4f + (45 / 127.f) * (45 / 127.f) * 6;
        return p.mEngine.drums[0].vol == 77 / 127.f && p.mEngine.drums[0].filter.drive == drive;
    }
    static bool hold(HostInstance& h, Authority::WriteHandle& held) {
        return plugin(h).mState.tryBeginWrite(held);
    }
    static void release(HostInstance& h, Authority::WriteHandle& held) {
        plugin(h).mState.cancelUnmodified(held);
    }
    static std::size_t queued(HostInstance& h) { return plugin(h).mDeferredMidi.size(); }
    static uint64_t overflow(HostInstance& h) { return plugin(h).mDeferredMidiOverflows.load(); }
    static lxr::kit::RuntimeLoadResult publishKit(
        HostInstance& h, const lxr::kit::CanonicalKit& kit) {
        auto& p = plugin(h);
        lxr::kit::RuntimeLoadResult result;
        lxr::kit::PatchCandidate candidate;
        result.candidate = lxr::kit::BuildPatchCandidate(kit, candidate);
        if (!result.candidate) return result;
        const auto order = p.mDeferredHostOrder.fetch_add(1u, std::memory_order_acq_rel);
        if (order == 0u) {
            result.publication.status =
                lxr::kit::PublicationStatus::GenerationExhausted;
            return result;
        }
        result.publication = lxr::kit::PublishPatchCandidate(
            candidate, {order, p.mRoutingState.generation()}, p.mState,
            p.mRoutingState);
        if (!result.publication) return result;
        for (std::size_t i = 0; i < candidate.patch.size(); ++i)
            p.GetParam(static_cast<int>(i))->Set(candidate.patch[i]);
        for (std::size_t i = 0; i < candidate.routes.size(); ++i) {
            const auto id = lxr::route_host::parameterForSlot(i);
            p.GetParam(id.value())->Set(
                lxr::route_host::encode(candidate.routes[i]).value());
        }
        p.mControllerNeedsReconcile = true;
        return result;
    }
    static lxr::kit::CanonicalKit currentKit(HostInstance& h,
                                             std::uint8_t volume,
                                             std::uint8_t snareSelector = 0u) {
        auto& p = plugin(h);
        lxr::kit::RawKit raw;
        raw.name = {{'P','1','7','D','H','O','S','T'}};
        for (std::size_t i = 0; i < raw.values.size(); ++i)
            raw.values[i] = p.mState.value(i);
        raw.values[88] = volume;
        raw.values[152] = snareSelector;
        lxr::kit::CanonicalKit kit;
        if (!lxr::kit::DecodeRawKit(raw, kit)) return {};
        return kit;
    }
    static bool runtimeKitApplied(HostInstance& h, std::uint8_t volume,
                                  std::uint16_t selector) {
        auto& p = plugin(h);
        return p.mParams.values[88u] == volume &&
            p.mAppliedRouting.routes[3].originalIndex == selector &&
            p.mAppliedKitGeneration != 0u &&
            (p.mAppliedKitNativeBoundary % 32u) == 0u;
    }
    static std::uint8_t stateValue(HostInstance& h, std::size_t index) {
        return plugin(h).mState.value(index);
    }
    static std::uint64_t patternGeneration(HostInstance& h) {
        return plugin(h).mAppliedPattern.generation();
    }
    static lxr::sequencer::state::TransportConfig audioTransport(HostInstance& h) {
        return plugin(h).mAudioTransport;
    }
    static void layout(HostInstance& h, HMODULE module) {
        auto& p = plugin(h);
        auto* base = reinterpret_cast<const unsigned char*>(&p);
        std::printf("LAYOUT vtable_rva=%llu parser_offset=%llu params_offset=%llu object_size=%llu\n",
            static_cast<unsigned long long>(*reinterpret_cast<const uintptr_t*>(base) - reinterpret_cast<uintptr_t>(module)),
            static_cast<unsigned long long>(reinterpret_cast<const unsigned char*>(&p.mMidiParser) - base),
            static_cast<unsigned long long>(reinterpret_cast<const unsigned char*>(&p.mParams) - base),
            static_cast<unsigned long long>(sizeof(p)));
    }
};
#endif

namespace {
void flushMain(unsigned milliseconds = 120)
{
    const auto deadline = GetTickCount64() + milliseconds;
    do {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        Sleep(1);
    } while (GetTickCount64() < deadline);
}

void cc(HostInstance& h, int number, int value)
{
    ParamID tag = kNoParamId;
    require("map exact MIDI CC", h.midiMapping->getMidiControllerAssignment(
        0, 0, static_cast<CtrlNumber>(number), tag) == kResultOk);
    std::printf("CALL process MIDI B0 %02X %02X mappedParamID=%u\n", number, value, tag);
    h.processParameter(tag, value / 127.0);
}

void nrpn(HostInstance& h, int number, int value)
{
    cc(h, 99, number >> 7); cc(h, 98, number & 127); cc(h, 6, value);
}

void ccHostFloat(HostInstance& h, int number, int value)
{
    ParamID tag = kNoParamId;
    require("map float-precision host MIDI", h.midiMapping->getMidiControllerAssignment(0, 0, number, tag) == kResultOk);
    const double normalized = static_cast<double>(static_cast<float>(value / 127.0));
    std::printf("FLOAT_MIDI cc=%d intended=%d normalized=%.17g scaled=%.17g\n", number, value, normalized, normalized*127);
    h.processParameter(tag, normalized);
}

void processBoundary(HostInstance& h)
{
    double l[1] {}, r[1] {}; double* channels[] {l, r};
    AudioBusBuffers out {}; out.numChannels = 2; out.channelBuffers64 = channels;
    ProcessData data {}; data.processMode = kOffline; data.symbolicSampleSize = kSample64;
    data.numSamples = 1; data.numOutputs = 1; data.outputs = &out;
    require("process native projection boundary", h.processor->process(data) == kResultOk);
}

struct RecordingHandler final : IComponentHandler {
    uint32 refs = 1;
    IEditController* controller = nullptr;
    HostInstance* reentrantMidi = nullptr;
    int refreshes = 0, edits = 0, begins = 0, ends = 0, reject = 0;
    bool wrongThread = false;
    DWORD mainThread = GetCurrentThreadId();
    tresult PLUGIN_API queryInterface(const TUID id, void** p) override {
        if (FUnknownPrivate::iidEqual(id, IComponentHandler::iid) ||
            FUnknownPrivate::iidEqual(id, FUnknown::iid)) {
            *p = static_cast<IComponentHandler*>(this); addRef(); return kResultOk;
        }
        *p = nullptr; return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return ++refs; }
    uint32 PLUGIN_API release() override { return --refs; }
    tresult PLUGIN_API beginEdit(ParamID id) override {
        ++begins; std::printf("CALL beginEdit id=%u\n", id); return kResultOk;
    }
    tresult PLUGIN_API performEdit(ParamID id, ParamValue v) override {
        ++edits; std::printf("CALL performEdit id=%u value=%.17g\n", id, v); return kResultOk;
    }
    tresult PLUGIN_API endEdit(ParamID id) override {
        ++ends; std::printf("CALL endEdit id=%u\n", id); return kResultOk;
    }
    tresult PLUGIN_API restartComponent(int32 flags) override {
        wrongThread |= GetCurrentThreadId() != mainThread;
        if (flags & kParamValuesChanged) {
            ++refreshes;
            std::printf("CALL restartComponent(kParamValuesChanged) read88=%.17g read128=%.17g\n",
                controller->getParamNormalized(88), controller->getParamNormalized(128));
            if (reentrantMidi) {
                auto* target = reentrantMidi; reentrantMidi = nullptr;
                cc(*target, 88, 91); // A newer generation during acknowledgement.
            }
            if (reject > 0) { --reject; return kResultFalse; }
        }
        return kResultOk;
    }
};

void agrees(HostInstance& h, int id, int intended, const char* label)
{
    std::array<uint8_t, 228> native {};
    const auto bytes = h.saveState();
    const bool decoded = decodeSaved(bytes, native);
    const double actual = h.controller->getParamNormalized(id);
    const double expected = intended / 127.0;
    std::printf("VALUE %s id=%d expected_native=%d actual_native=%u expected_normalized=%.17g actual_normalized=%.17g native_from_controller=%.17g\n",
        label, id, intended, native[id], expected, actual, actual * 127);
    check(label, decoded && native[id] == intended && std::abs(actual - expected) < 1e-12 &&
        h.controller->normalizedParamToPlain(id, actual) == intended &&
        std::abs(h.controller->plainParamToNormalized(id, intended) - expected) < 1e-12);
#if defined(LXR_R2_INTERNAL_PROBE)
    check("authority/ParameterArray/processor IParam agree", LXRControllerSyncTestProbe::projections(h, id, intended));
#endif
}
}

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 2 && argc != 3) return 2;
    std::printf("EXACT_DLL %s\n", argv[1]);
    HMODULE module = LoadLibraryA(argv[1]);
    require("load exact VST3", module != nullptr);
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
    auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
    auto factoryFn = reinterpret_cast<IPluginFactory* (*)()>(GetProcAddress(module, "GetPluginFactory"));
    require("exports available", init && exitDll && factoryFn);
    require("DLL initialization", init());
    auto* factory = factoryFn();
    PClassInfo info {};
    bool found = false;
    for (int i = 0; i < factory->countClasses(); ++i)
        if (factory->getClassInfo(i, &info) == kResultOk &&
            std::strcmp(info.category, kVstAudioEffectClass) == 0) { found = true; break; }
    require("find audio class", found);
#if defined(LXR_R2_INTERNAL_PROBE)
    if (argc == 3 && std::strcmp(argv[2], "--layout") == 0) {
        { HostInstance h(factory, info.cid); LXRControllerSyncTestProbe::layout(h, module); }
        factory->release(); exitDll(); FreeLibrary(module); return 0;
    }
#endif
    {
        RecordingHandler handler; // Outlives the attached component.
        HostInstance h(factory, info.cid);
        handler.controller = h.controller;
        require("attach recording host", h.controller->setComponentHandler(&handler) == kResultOk);
        FUnknown *processorIdentity = nullptr, *controllerIdentity = nullptr;
        h.component->queryInterface(FUnknown::iid, reinterpret_cast<void**>(&processorIdentity));
        h.controller->queryInterface(FUnknown::iid, reinterpret_cast<void**>(&controllerIdentity));
        check("single component/controller canonical identity", processorIdentity == controllerIdentity);
        processorIdentity->release(); controllerIdentity->release();
        h.setRaw(88, 100); h.setRaw(128, 0); h.setup(48000);
        flushMain();
        cc(h, 88, 77); nrpn(h, 1, 45);
        std::array<uint8_t, 228> immediate {};
        check("MIDI accepted before idle", decodeSaved(h.saveState(), immediate) && immediate[88] == 77 && immediate[128] == 45);
        check("controller waits for main boundary", std::abs(h.controller->getParamNormalized(88) * 127 - 100) < 1e-9);
        flushMain();
        agrees(h, 88, 77, "CC88 converges to 77");
        agrees(h, 128, 45, "NRPN1 converges to 45");
#if defined(LXR_R2_INTERNAL_PROBE)
        check("Engine applies original volume and drive mappings", LXRControllerSyncTestProbe::engine(h));
#endif
        const auto midiState = h.saveState();
        h.setRaw(128, 0); processBoundary(h);
        ccHostFloat(h, 99, 0); ccHostFloat(h, 98, 1); ccHostFloat(h, 6, 45);
        flushMain(); agrees(h, 128, 45, "float-precision host NRPN preserves selector 1");
        bool allWireValues = true;
        for (int v = 0; v < 128; ++v) {
            ccHostFloat(h, 88, v);
            std::array<uint8_t, 228> values {};
            allWireValues &= decodeSaved(h.saveState(), values) && values[88] == v;
        }
        check("all 128 float-precision MIDI wire values preserved", allWireValues);
        cc(h, 88, 77); flushMain();
        auto stableRefreshes = handler.refreshes;
        flushMain(220);
        check("no repeated notification loop", handler.refreshes == stableRefreshes);
        check("MIDI creates no DAW automation gestures", handler.edits == 0 && handler.begins == 0 && handler.ends == 0);

        for (int v = 1; v <= 32; ++v) cc(h, 88, v);
        for (int id = 89; id <= 93; ++id) cc(h, id, id - 20);
        for (int n = 1; n <= 6; ++n) nrpn(h, n, 40 + n);
        flushMain();
        agrees(h, 88, 32, "same parameter coalesces to newest MIDI");
        for (int id = 89; id <= 93; ++id) agrees(h, id, id - 20, "distinct rapid CC ID");
        for (int n = 1; n <= 6; ++n) agrees(h, 127 + n, 40 + n, "distinct rapid NRPN ID");

        cc(h, 88, 13); h.setRaw(88, 97); processBoundary(h); flushMain();
        agrees(h, 88, 97, "later host automation suppresses stale MIDI");
        cc(h, 88, 22);
        auto restore = midiState;
        require("restore supersedes pending MIDI", h.loadState(restore));
        processBoundary(h); flushMain(); agrees(h, 88, 77, "restore wins older notification");
        cc(h, 88, 24); flushMain(); agrees(h, 88, 24, "MIDI after restore wins");

        handler.reject = 1;
        stableRefreshes = handler.refreshes;
        cc(h, 88, 62); flushMain(220);
        check("failed host acknowledgement retried", handler.refreshes >= stableRefreshes + 2);
        agrees(h, 88, 62, "retry retains intended value");
        handler.reentrantMidi = &h;
        cc(h, 88, 61); flushMain(220);
        agrees(h, 88, 91, "new generation during host callback is retained");

        require("disconnect host", h.controller->setComponentHandler(nullptr) == kResultOk);
        cc(h, 88, 54); nrpn(h, 1, 56); flushMain();
        require("reconnect host", h.controller->setComponentHandler(&handler) == kResultOk);
        stableRefreshes = handler.refreshes; flushMain();
        check("reconnect triggers reconciliation", handler.refreshes == stableRefreshes + 1);
        agrees(h, 88, 54, "disconnected CC retained"); agrees(h, 128, 56, "disconnected NRPN retained");
        std::array<uint8_t, 228> all {};
        bool full = decodeSaved(h.saveState(), all);
        for (int id = 0; id < 228; ++id)
            full &= std::abs(h.controller->getParamNormalized(id) - all[id] / 127.0) < 1e-12;
        check("reconnect full 228 slot agreement", full);

        auto corrupt = midiState; corrupt[40] ^= 0x20;
        require("invalid state rejected", !h.loadState(corrupt));
        cc(h, 88, 17); flushMain(); agrees(h, 88, 17, "MIDI after invalid restore");
        auto legacy = makeLegacy(all);
        require("legacy restore accepted", h.loadState(legacy));
        cc(h, 88, 18); flushMain(); agrees(h, 88, 18, "MIDI after legacy migration");

        // Concurrent real processing vs main-thread serialization/restore/UI.
        // All controller cache access stays on this main thread.
        std::atomic<bool> run {true};
        std::atomic<int> processed {0}, processFailures {0};
        ParamID cc88 = kNoParamId;
        h.midiMapping->getMidiControllerAssignment(0, 0, 88, cc88);
        std::thread audio([&] {
            ParameterChanges changes(1);
            int32 qi = 0, pi = 0;
            auto* queue = changes.addParameterData(cc88, qi);
            queue->addPoint(0, 73.0 / 127.0, pi);
            double left[1] {}, right[1] {}; double* channels[] {left, right};
            AudioBusBuffers output {}; output.numChannels = 2; output.channelBuffers64 = channels;
            ProcessData data {}; data.processMode = kOffline; data.symbolicSampleSize = kSample64;
            data.numSamples = 1; data.numOutputs = 1; data.outputs = &output; data.inputParameterChanges = &changes;
            while (run.load()) {
                if (h.processor->process(data) != kResultOk) ++processFailures;
                ++processed;
                std::this_thread::yield();
            }
        });
        int snapshots = 0, incoherent = 0, restores = 0;
        for (int iteration = 0; iteration < 80; ++iteration) {
            MemoryStream stream;
            if (h.component->getState(&stream) == kResultOk) {
                std::array<uint8_t, 228> values {};
                lxr::state::ProjectCandidate candidate;
                auto result = lxr::state::DecodeProject(
                    reinterpret_cast<const uint8_t*>(stream.getData()),
                    stream.getSize(), candidate, true);
                values = candidate.patch;
                if (!result) ++incoherent;
                ++snapshots;
            }
            auto state = midiState;
            if (h.loadState(state)) ++restores;
            h.setRaw(89, iteration & 127);
            flushMain(2);
        }
        run.store(false); audio.join();
        check("real concurrent process/save/restore/UI execute", processed > 0 && snapshots > 0 && restores > 0 && processFailures == 0 && incoherent == 0);
        cc(h, 88, 77); nrpn(h, 1, 45); flushMain(220);
        agrees(h, 88, 77, "concurrent processing stabilizes CC");
        agrees(h, 128, 45, "concurrent processing stabilizes NRPN");
        check("all refresh callbacks on main thread", !handler.wrongThread);
        check("concurrent MIDI creates no edits", handler.edits == 0);
        auto finalState = h.saveState();
#if defined(LXR_R2_INTERNAL_PROBE)
        // Force an unavailable writer throughout actual process() calls. The
        // VST3 MIDI mapping, plugin FIFO, real restore and replay all execute.
        using Probe = LXRControllerSyncTestProbe;
        Probe::Authority::WriteHandle held;
        require("hold production authority gate", Probe::hold(h, held));
        cc(h, 88, 21); nrpn(h, 1, 22); cc(h, 88, 77); nrpn(h, 1, 45);
        check("ordered CC and NRPN retained while writer held", Probe::queued(h) == 8);
        flushMain();
        check("busy idle does not consume deferred MIDI", Probe::queued(h) == 8);
        Probe::release(h, held);
        require("real restore before FIFO replay", h.loadState(restore));
        // An ordinary processor parameter point supplies the replay boundary.
        h.processParameter(89, 70 / 127.0); flushMain();
        check("deferred FIFO completely replays", Probe::queued(h) == 0);
        agrees(h, 88, 77, "FIFO final CC after restore");
        agrees(h, 128, 45, "FIFO final NRPN after restore");
        check("FIFO replay updates Engine", Probe::engine(h));

        require("hold gate for fail-closed overflow", Probe::hold(h, held));
        ParamID burstTag = kNoParamId;
        h.midiMapping->getMidiControllerAssignment(0, 0, 88, burstTag);
        ParameterChanges burstChanges(1);
        int32 qi = 0, pi = 0;
        auto* burstQueue = burstChanges.addParameterData(burstTag, qi);
        burstQueue->addPoint(0, 77 / 127.0, pi);
        ProcessData burst {}; burst.processMode = kOffline; burst.symbolicSampleSize = kSample64;
        double l[1] {}, r[1] {}; double* ch[] {l, r};
        AudioBusBuffers out {}; out.numChannels = 2; out.channelBuffers64 = ch;
        burst.numSamples = 1; burst.numOutputs = 1; burst.outputs = &out; burst.inputParameterChanges = &burstChanges;
        const auto overflows = Probe::overflow(h);
        bool processedAll = true;
        for (int event = 0; event < 4097; ++event)
            processedAll &= h.processor->process(burst) == kResultOk;
        check("4096-event capacity and newest-event rejection", processedAll && Probe::queued(h) == 4096 && Probe::overflow(h) == overflows + 1);
        Probe::release(h, held);
        h.processParameter(89, 70 / 127.0); flushMain();
        check("accepted FIFO survives overflow", Probe::queued(h) == 0);
        agrees(h, 88, 77, "overflow retains accepted final MIDI");
        finalState = h.saveState();

        // P17-D: exercise the exact processor object's kit authorities,
        // processing-boundary projection and controller cache. This probe uses
        // the same bounded primitive as LoadRuntimeKit; no file or GUI path is
        // introduced here.
        auto beforeKitState = h.saveState();
        const auto patternGeneration = Probe::patternGeneration(h);
        const auto transportBefore = Probe::audioTransport(h);
        const int editsBeforeKit = handler.edits;
        auto kit = Probe::currentKit(h, 96u, 132u);
        const auto acceptedKit = Probe::publishKit(h, kit);
        require("runtime whole kit accepted", static_cast<bool>(acceptedKit));
        processBoundary(h); flushMain();
        check("runtime kit applied at native boundary",
              Probe::runtimeKitApplied(h, 96u, 132u));
        check("runtime kit preserves pattern pin",
              Probe::patternGeneration(h) == patternGeneration);
        const auto transportAfter = Probe::audioTransport(h);
        check("runtime kit preserves transport configuration",
              transportAfter.enabled == transportBefore.enabled &&
              transportAfter.clock == transportBefore.clock &&
              transportAfter.bpm == transportBefore.bpm &&
              transportAfter.pattern == transportBefore.pattern &&
              transportAfter.seed == transportBefore.seed);
        check("runtime kit sends no edit gestures", handler.edits == editsBeforeKit);
        check("runtime kit controller patch readback",
              std::abs(h.controller->getParamNormalized(88) - 96.0 / 127.0) < 1.e-12);
        check("runtime kit controller high route readback",
              h.controller->normalizedParamToPlain(231,
                  h.controller->getParamNormalized(231)) == 132.0);
        auto invalidRaw = lxr::kit::RawKit{};
        invalidRaw.name = {{'I','N','V','A','L','I','D','!'}};
        for (std::size_t i = 0; i < invalidRaw.values.size(); ++i)
            invalidRaw.values[i] = Probe::stateValue(h, i);
        invalidRaw.values[1] = 6u; // Source-valid external sample, runtime unsupported.
        lxr::kit::CanonicalKit invalidKit;
        require("decode source-valid unsupported kit",
                static_cast<bool>(lxr::kit::DecodeRawKit(invalidRaw, invalidKit)));
        const auto beforeRejected = h.saveState();
        check("unsupported runtime kit rejected",
              !Probe::publishKit(h, invalidKit));
        check("rejected kit leaves processor state unchanged",
              h.saveState() == beforeRejected);

        kit = Probe::currentKit(h, 77u, 132u);
        require("successive runtime kit accepted",
                static_cast<bool>(Probe::publishKit(h, kit)));
        processBoundary(h); flushMain();
        agrees(h, 88, 77, "successive kit controller readback");
        require("complete restore after runtime kit", h.loadState(beforeKitState));
        processBoundary(h); flushMain();
        check("complete restore supersedes runtime kit",
              h.saveState() == beforeKitState);
        finalState = h.saveState();
#endif
        {
            HostInstance second(factory, info.cid);
            require("MIDI serialize then recreate restore", second.loadState(finalState));
            second.setup(48000); flushMain();
            agrees(second, 88, 77, "recreated controller CC"); agrees(second, 128, 45, "recreated controller NRPN");
            cc(second, 88, 19); flushMain();
            agrees(second, 88, 19, "second instance independent MIDI");
            agrees(h, 88, 77, "first instance remains independent");
            cc(second, 88, 20); // Destroy with an unflushed notification.
        }
        flushMain(); check("pending controller destruction safe", true);
        stableRefreshes = handler.refreshes; flushMain(220);
        check("stabilized controller has no notification loop", handler.refreshes == stableRefreshes);
    }
    factory->release(); exitDll(); FreeLibrary(module);
    std::printf("SUMMARY pass=%d fail=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
