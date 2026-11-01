// Windows installed-binary integration test. Uses the local Steinberg SDK as
// a small offline host; no test hooks or instrumentation enter the plugin.
#define NOMINMAX
#include <windows.h>
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/common/memorystream.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../dsp/Engine.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
int pass = 0, fail = 0;
void check(const char* name, bool ok) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    ok ? ++pass : ++fail;
}
void require(const char* name, bool ok) {
    check(name, ok);
    if (!ok) std::exit(1);
}
// Actual framework notification receiver, independent of the latency helper.
struct LatencyHandler final : IComponentHandler {
    uint32 refs = 1;
    int changes = 0;
    uint32 observed = 0;
    IAudioProcessor* processor = nullptr;
    tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override {
        if (FUnknownPrivate::iidEqual(iid, IComponentHandler::iid) ||
            FUnknownPrivate::iidEqual(iid, FUnknown::iid)) {
            *obj = static_cast<IComponentHandler*>(this); addRef(); return kResultOk;
        }
        *obj = nullptr; return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return ++refs; }
    uint32 PLUGIN_API release() override { return --refs; }
    tresult PLUGIN_API beginEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API performEdit(ParamID, ParamValue) override { return kResultOk; }
    tresult PLUGIN_API endEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API restartComponent(int32 flags) override {
        if (flags & kLatencyChanged) { ++changes; observed = processor->getLatencySamples(); }
        return kResultOk;
    }
};
struct HostInstance {
    HostApplication host;
    LatencyHandler handler;
    IComponent* component = nullptr;
    IAudioProcessor* processor = nullptr;
    IEditController* controller = nullptr;
    HostInstance(IPluginFactory* factory, const TUID cid) {
        require("create actual VST3 component", factory->createInstance(cid, IComponent::iid,
            reinterpret_cast<void**>(&component)) == kResultOk);
        require("initialize VST3", component->initialize(&host) == kResultOk);
        require("query audio processor", component->queryInterface(IAudioProcessor::iid,
            reinterpret_cast<void**>(&processor)) == kResultOk);
        require("query controller", component->queryInterface(IEditController::iid,
            reinterpret_cast<void**>(&controller)) == kResultOk);
        handler.processor = processor;
        require("attach VST3 component handler", controller->setComponentHandler(&handler) == kResultOk);
        SpeakerArrangement stereo = SpeakerArr::kStereo;
        require("set stereo arrangement", processor->setBusArrangements(nullptr, 0, &stereo, 1) == kResultOk);
        require("activate audio output", component->activateBus(kAudio, kOutput, 0, true) == kResultOk);
        require("activate MIDI input", component->activateBus(kEvent, kInput, 0, true) == kResultOk);
    }
    ~HostInstance() {
        processor->setProcessing(false);
        component->setActive(false);
        controller->setComponentHandler(nullptr);
        controller->release(); processor->release();
        component->terminate(); component->release();
    }
    void setup(double rate) {
        const uint32 previous = processor->getLatencySamples();
        const int changes = handler.changes;
        ProcessSetup setup {kOffline, kSample64, 256, rate};
        require("setup actual host sample rate", processor->setupProcessing(setup) == kResultOk);
        const uint32 expected = rate >= 32000 && rate <= 192000
            ? static_cast<uint32>(std::floor((47.0 / 2.0) * rate /
                static_cast<double>(lxr::kEngineSampleRate) + .5)) : 0;
        check("actual VST3 integer latency", processor->getLatencySamples() == expected);
        check("latency notification only on change", handler.changes == changes + (previous != expected ? 1 : 0));
        if (previous != expected) check("notification sees updated latency", handler.observed == expected);
        require("activate processor", component->setActive(true) == kResultOk);
        require("start processing", processor->setProcessing(true) == kResultOk);
        check("activation preserves latency", processor->getLatencySamples() == expected);
    }
    double render(int channel, bool expectAudio, bool trigger = true) {
        const int changes = handler.changes;
        std::array<double, 256> l {}, r {};
        double* channels[] = {l.data(), r.data()};
        AudioBusBuffers output {};
        output.numChannels = 2; output.channelBuffers64 = channels;
        EventList events;
        Event on {};
        on.type = Event::kNoteOnEvent;
        on.noteOn.channel = static_cast<int16>(channel);
        on.noteOn.pitch = 60; on.noteOn.velocity = 110.f/127.f; on.noteOn.noteId = -1;
        if (trigger) require("queue MIDI note", events.addEvent(on) == kResultOk);
        ProcessData data {};
        data.processMode = kOffline; data.symbolicSampleSize = kSample64;
        data.numSamples = 256; data.numOutputs = 1; data.outputs = &output;
        data.inputEvents = &events;
        double peak = 0; bool finite = true;
        for (int block = 0; block < 32; ++block) {
            l.fill(123.); r.fill(123.);
            require("process installed plugin", processor->process(data) == kResultOk);
            events.clear();
            for (int i = 0; i < 256; ++i) {
                finite &= std::isfinite(l[i]) && std::isfinite(r[i]);
                peak = std::max({peak, std::abs(l[i]), std::abs(r[i])});
            }
        }
        check("finite plugin output", finite);
        check("no latency notification from process", handler.changes == changes);
        check(expectAudio ? "nonzero plugin output" : "intentional silent state preserved",
              expectAudio ? peak > 0.001 && peak < 2 : peak == 0);
        return peak;
    }
};
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) { std::puts("Usage: P12b4cVST3RuntimeTest.exe <absolute VST3 DLL path> [--missing-assets]"); return 2; }
    const bool assetsExpected = argc == 2;
    if (!assetsExpected && std::strcmp(argv[2], "--missing-assets") != 0) return 2;
    HMODULE module = LoadLibraryA(argv[1]);
    require("load installed VST3 binary", module != nullptr);
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
    auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
    auto getFactory = reinterpret_cast<IPluginFactory* (*)()>(GetProcAddress(module, "GetPluginFactory"));
    require("module exports", init && exitDll && getFactory);
    require("module init", init());
    IPluginFactory* factory = getFactory();
    require("get factory", factory != nullptr);
    PClassInfo info {}; bool found = false;
    for (int i = 0; i < factory->countClasses(); ++i) {
        if (factory->getClassInfo(i, &info) == kResultOk && std::strcmp(info.category, kVstAudioEffectClass) == 0) {
            found = true; break;
        }
    }
    require("find audio component", found);
    {
        HostInstance instance(factory, info.cid);
        const int ids[] = {49, 50, 88, 137, 191, 215, 1, 108, 114};
        for (int id : ids) std::printf("Host default PAR %d = %.0f\n", id,
            instance.controller->getParamNormalized(id) * 127.);
        for (double rate : {44100., 48000., 96000., 32000., 88200., 192000., 48000.}) {
            instance.setup(rate);
            const double peak = instance.render(0, assetsExpected);
            std::printf("PLUGIN rate=%.0f channel=1 peak=%.9g latency=%u\n", rate, peak,
                        instance.processor->getLatencySamples());
            instance.processor->setProcessing(false); instance.component->setActive(false);
            const int changes = instance.handler.changes;
            instance.setup(rate);
            instance.render(0, false, false);
            check("same-rate reset has no restart request", instance.handler.changes == changes);
            instance.processor->setProcessing(false); instance.component->setActive(false);
        }
        for (double rate : {22050., 384000.}) {
            instance.setup(rate);
            instance.render(0, false);
            instance.processor->setProcessing(false); instance.component->setActive(false);
        }
        instance.setup(48000.);
        instance.render(0, assetsExpected);
        instance.processor->setProcessing(false); instance.component->setActive(false);
        // Save/reopen an audible patch as well as the silent patch below.
        MemoryStream audibleState;
        require("save audible patch", instance.component->getState(&audibleState) == kResultOk);
        audibleState.seek(0, IBStream::kIBSeekSet, nullptr);
        {
            HostInstance reopened(factory, info.cid);
            require("reopen audible patch", reopened.component->setState(&audibleState) == kResultOk);
            bool unchanged = true;
            for (int id = 0; id < 228; ++id)
                unchanged &= instance.controller->getParamNormalized(id) == reopened.controller->getParamNormalized(id);
            check("audible patch all values preserved", unchanged);
            reopened.setup(96000.); reopened.render(0, assetsExpected);
        }
        // Muting D1 makes lost channel nibbles observable at the wrapper boundary.
        require("mute D1 for channel isolation", instance.controller->setParamNormalized(88, 0.) == kResultOk);
        for (int channel : {0, 1, 2, 3, 4, 5, 6}) {
            instance.setup(48000.);
            const double peak = instance.render(channel, assetsExpected && channel != 0);
            std::printf("CHANNEL channel=%d D1-muted peak=%.9g\n", channel + 1, peak);
            instance.processor->setProcessing(false); instance.component->setActive(false);
        }
        require("restore D1 volume", instance.controller->setParamNormalized(88, 100./127.) == kResultOk);
        // Restore a deliberately silent patch into a fresh instance. No migration
        // may turn attack 127 or decimation 0 into an audible default.
        require("set deliberate attack 127", instance.controller->setParamNormalized(49, 1.) == kResultOk);
        require("set deliberate decimation 0", instance.controller->setParamNormalized(108, 0.) == kResultOk);
        std::array<double, 228> savedValues {};
        for (int id = 0; id < 228; ++id) savedValues[id] = instance.controller->getParamNormalized(id);
        MemoryStream state;
        require("save user parameter state", instance.component->getState(&state) == kResultOk);
        state.seek(0, IBStream::kIBSeekSet, nullptr);
        HostInstance restored(factory, info.cid);
        require("restore user parameter state", restored.component->setState(&state) == kResultOk);
        check("restored attack/decimator unchanged", restored.controller->getParamNormalized(49) == 1. &&
              restored.controller->getParamNormalized(108) == 0.);
        bool unchanged = true;
        for (int id = 0; id < 228; ++id)
            unchanged &= savedValues[id] == restored.controller->getParamNormalized(id);
        check("all 228 restored values preserved", unchanged);
        restored.setup(48000.);
        std::printf("RESTORED peak=%.9g\n", restored.render(0, false));
    }
    factory->release(); exitDll(); FreeLibrary(module);
    std::printf("SUMMARY pass=%d fail=%d\n", pass, fail);
    return fail ? 1 : 0;
}
