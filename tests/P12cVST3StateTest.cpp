#define NOMINMAX
#include <windows.h>

#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include "../dsp/Engine.h"
#include "../vendor/iPlug2/Examples/LXR/LXRState.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

int passed = 0;
int failed = 0;

void check(const char* name, bool condition)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", name);
    condition ? ++passed : ++failed;
}

void require(const char* name, bool condition)
{
    check(name, condition);
    if (!condition)
        std::exit(1);
}

struct ComponentHandler final : IComponentHandler
{
    uint32 refs = 1;
    int edits = 0;
    tresult PLUGIN_API queryInterface(const TUID iid, void** object) override
    {
        if (FUnknownPrivate::iidEqual(iid, IComponentHandler::iid) ||
            FUnknownPrivate::iidEqual(iid, FUnknown::iid))
        {
            *object = static_cast<IComponentHandler*>(this);
            addRef();
            return kResultOk;
        }
        *object = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return ++refs; }
    uint32 PLUGIN_API release() override { return --refs; }
    tresult PLUGIN_API beginEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API performEdit(ParamID, ParamValue) override { ++edits; return kResultOk; }
    tresult PLUGIN_API endEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API restartComponent(int32) override { return kResultOk; }
};

struct HostInstance
{
    HostApplication host;
    ComponentHandler handler;
    IComponent* component = nullptr;
    IAudioProcessor* processor = nullptr;
    IEditController* controller = nullptr;
    IMidiMapping* midiMapping = nullptr;
    bool active = false;

    HostInstance(IPluginFactory* factory, const TUID cid)
    {
        require("create VST3 component", factory->createInstance(
            cid, IComponent::iid, reinterpret_cast<void**>(&component)) == kResultOk);
        require("initialize VST3 component", component->initialize(&host) == kResultOk);
        require("query audio processor", component->queryInterface(
            IAudioProcessor::iid, reinterpret_cast<void**>(&processor)) == kResultOk);
        require("query edit controller", component->queryInterface(
            IEditController::iid, reinterpret_cast<void**>(&controller)) == kResultOk);
        require("query MIDI mapping", component->queryInterface(
            IMidiMapping::iid, reinterpret_cast<void**>(&midiMapping)) == kResultOk);
        require("attach component handler", controller->setComponentHandler(&handler) == kResultOk);
        SpeakerArrangement stereo = SpeakerArr::kStereo;
        require("set stereo output", processor->setBusArrangements(nullptr, 0, &stereo, 1) == kResultOk);
        require("activate audio bus", component->activateBus(kAudio, kOutput, 0, true) == kResultOk);
        require("activate event bus", component->activateBus(kEvent, kInput, 0, true) == kResultOk);
    }

    ~HostInstance()
    {
        stop();
        controller->setComponentHandler(nullptr);
        midiMapping->release();
        controller->release();
        processor->release();
        component->terminate();
        component->release();
    }

    void setup(double sampleRate)
    {
        stop();
        ProcessSetup setupInfo {kOffline, kSample64, 256, sampleRate};
        require("setup processing rate", processor->setupProcessing(setupInfo) == kResultOk);
        require("activate component", component->setActive(true) == kResultOk);
        require("start processing", processor->setProcessing(true) == kResultOk);
        active = true;
    }

    void stop()
    {
        if (active)
        {
            processor->setProcessing(false);
            component->setActive(false);
            active = false;
        }
    }

    void setRaw(int parameter, int value)
    {
        require("set host parameter", controller->setParamNormalized(
            static_cast<ParamID>(parameter), static_cast<double>(value) / 127.0) == kResultOk);
    }

    std::array<double, 228> values() const
    {
        std::array<double, 228> result {};
        for (int i = 0; i < 228; ++i)
            result[static_cast<std::size_t>(i)] = controller->getParamNormalized(i);
        return result;
    }

    std::vector<uint8_t> saveState()
    {
        MemoryStream state;
        require("get component state", component->getState(&state) == kResultOk);
        return std::vector<uint8_t>(
            reinterpret_cast<uint8_t*>(state.getData()),
            reinterpret_cast<uint8_t*>(state.getData()) + state.getSize());
    }

    bool loadState(std::vector<uint8_t>& bytes)
    {
        MemoryStream state(bytes.data(), static_cast<TSize>(bytes.size()));
        return component->setState(&state) == kResultOk;
    }

    void processParameter(ParamID id, double normalized)
    {
        ParameterChanges changes(1);
        int32 queueIndex = 0;
        auto* queue = changes.addParameterData(id, queueIndex);
        int32 pointIndex = 0;
        require("add parameter point", queue != nullptr &&
            queue->addPoint(0, normalized, pointIndex) == kResultOk);

        std::array<double, 1> left {};
        std::array<double, 1> right {};
        double* channels[] {left.data(), right.data()};
        AudioBusBuffers output {};
        output.numChannels = 2;
        output.channelBuffers64 = channels;
        ProcessData data {};
        data.processMode = kOffline;
        data.symbolicSampleSize = kSample64;
        data.numSamples = 1;
        data.numOutputs = 1;
        data.outputs = &output;
        data.inputParameterChanges = &changes;
        require("process parameter point", processor->process(data) == kResultOk);
    }
};

struct AudioRender
{
    std::vector<double> left = std::vector<double>(8192, 0.0);
    std::vector<double> right = std::vector<double>(8192, 0.0);
    double peak = 0.0;
    double rms = 0.0;
};

AudioRender renderD1(HostInstance& instance)
{
    AudioRender result;
    std::array<double, 256> left {};
    std::array<double, 256> right {};
    double* channels[] {left.data(), right.data()};
    AudioBusBuffers output {};
    output.numChannels = 2;
    output.channelBuffers64 = channels;
    EventList events;
    Event note {};
    note.type = Event::kNoteOnEvent;
    note.noteOn.channel = 0;
    note.noteOn.pitch = 60;
    note.noteOn.velocity = 110.0f / 127.0f;
    note.noteOn.noteId = -1;
    require("queue deterministic note", events.addEvent(note) == kResultOk);

    ProcessData data {};
    data.processMode = kOffline;
    data.symbolicSampleSize = kSample64;
    data.numSamples = 256;
    data.numOutputs = 1;
    data.outputs = &output;
    data.inputEvents = &events;
    double energy = 0.0;
    for (int block = 0; block < 32; ++block)
    {
        left.fill(0.0);
        right.fill(0.0);
        require("render deterministic block", instance.processor->process(data) == kResultOk);
        events.clear();
        for (int i = 0; i < 256; ++i)
        {
            const int frame = block * 256 + i;
            result.left[frame] = left[i];
            result.right[frame] = right[i];
            result.peak = std::max({result.peak, std::abs(left[i]), std::abs(right[i])});
            energy += left[i] * left[i] + right[i] * right[i];
        }
    }
    result.rms = std::sqrt(energy / (2.0 * result.left.size()));
    return result;
}

bool decodeSaved(const std::vector<uint8_t>& saved,
                 std::array<uint8_t, lxr::state::kParameterCount>& parameters)
{
    auto routing = lxr::MakeDisabledRoutingState();
    const auto decoded = lxr::state::DecodePersistentState(
        saved.data(), saved.size(), parameters, routing, true);
    return decoded && decoded.consumed >= lxr::state::kEncodedSize;
}

std::vector<uint8_t> makeLegacy(const std::array<uint8_t, 228>& parameters)
{
    std::vector<uint8_t> bytes(lxr::state::kLegacyEncodedSize + lxr::state::kVST3BypassSize, 0);
    for (std::size_t i = 0; i < parameters.size(); ++i)
    {
        const double value = parameters[i];
        std::memcpy(bytes.data() + i * sizeof(double), &value, sizeof(double));
    }
    return bytes;
}

} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    if (argc != 2)
    {
        std::puts("Usage: P12cVST3StateTest.exe <absolute VST3 DLL path>");
        return 2;
    }

    HMODULE module = LoadLibraryA(argv[1]);
    require("load installed VST3", module != nullptr);
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
    auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
    auto getFactory = reinterpret_cast<IPluginFactory* (*)()>(GetProcAddress(module, "GetPluginFactory"));
    require("resolve VST3 exports", init && exitDll && getFactory);
    require("initialize DLL", init());
    IPluginFactory* factory = getFactory();
    require("get plugin factory", factory != nullptr);

    PClassInfo info {};
    bool found = false;
    for (int i = 0; i < factory->countClasses(); ++i)
    {
        if (factory->getClassInfo(i, &info) == kResultOk &&
            std::strcmp(info.category, kVstAudioEffectClass) == 0)
        {
            found = true;
            break;
        }
    }
    require("find LXR audio class", found);

    std::vector<uint8_t> saved;
    AudioRender originalAudio;
    std::array<uint8_t, 228> originalParameters {};
    {
        HostInstance original(factory, info.cid);
        original.setRaw(0, 127);
        check("reserved slot 0 accepts a bounded host value",
              original.controller->getParamNormalized(0) == 1.0);
        const std::array<std::pair<int, int>, 23> edits {{
            {1, 0}, {8, 57}, {9, 71}, {20, 2}, {37, 72}, {43, 38},
            {49, 0}, {50, 49}, {62, 31}, {70, 27}, {74, 19}, {78, 54},
            {82, 33}, {83, 68}, {88, 109}, {94, 46}, {102, 41},
            {108, 127}, {114, 127}, {115, 37}, {121, 0}, {137, 1}, {191, 7}
        }};
        for (const auto& edit : edits)
            original.setRaw(edit.first, edit.second);

        saved = original.saveState();
        const auto savedAgain = original.saveState();
        check("VST3 state is 516-byte LXRS v2 plus bypass",
              saved.size() == lxr::state::kV2VST3EncodedSize);
        check("repeated VST3 serialization is deterministic", saved == savedAgain);
        check("decode actual VST3 state", decodeSaved(saved, originalParameters));
        check("all edited parameters encoded", originalParameters[8] == 57 &&
              originalParameters[37] == 72 && originalParameters[88] == 109 &&
              originalParameters[191] == 7);
        original.setup(48000.0);
        originalAudio = renderD1(original);
        check("pre-save deterministic audio nonzero", originalAudio.peak > 0.0);
    }

    {
        HostInstance restored(factory, info.cid);
        require("restore versioned VST3 state", restored.loadState(saved));
        std::array<uint8_t, 228> restoredParameters {};
        const auto resaved = restored.saveState();
        check("restored parameters reserialize identically", resaved == saved);
        check("decode reserialized state", decodeSaved(resaved, restoredParameters));
        check("all 228 slots preserved", restoredParameters == originalParameters);
        restored.setup(48000.0);
        const AudioRender restoredAudio = renderD1(restored);
        check("restored deterministic PCM exact",
              restoredAudio.left == originalAudio.left && restoredAudio.right == originalAudio.right);
        check("restored peak and RMS exact",
              restoredAudio.peak == originalAudio.peak && restoredAudio.rms == originalAudio.rms);
        check("restored FIR latency at 48 kHz", restored.processor->getLatencySamples() == 26);
        restored.stop();
        restored.setup(96000.0);
        check("restored FIR reconfigures at 96 kHz", restored.processor->getLatencySamples() == 51);
    }

    {
        HostInstance protectedInstance(factory, info.cid);
        protectedInstance.setRaw(88, 43);
        protectedInstance.setRaw(105, 91);
        const auto before = protectedInstance.values();

        auto rejectWithoutMutation = [&](const char* name, std::vector<uint8_t> corrupt) {
            const bool rejected = !protectedInstance.loadState(corrupt);
            check(name, rejected && protectedInstance.values() == before);
        };

        rejectWithoutMutation("empty state rejected without mutation", {});
        rejectWithoutMutation("truncated state rejected without mutation",
                              std::vector<uint8_t>(saved.begin(), saved.end() - 5));
        std::vector<uint8_t> oversized(lxr::state::kMaximumStateSize + 1, 0);
        rejectWithoutMutation("oversized state rejected without mutation", oversized);
        auto corrupt = saved;
        corrupt[0] = 'X';
        rejectWithoutMutation("invalid magic rejected without mutation", corrupt);
        corrupt = saved;
        lxr::state::PutU16LE(corrupt.data() + 4, 99);
        rejectWithoutMutation("future version rejected without mutation", corrupt);
        corrupt = saved;
        lxr::state::PutU32LE(corrupt.data() + 8, 227);
        rejectWithoutMutation("corrupt length rejected without mutation", corrupt);
        corrupt = saved;
        corrupt[40] ^= 0x20;
        rejectWithoutMutation("corrupt checksum rejected without mutation", corrupt);
        corrupt = saved;
        corrupt.insert(corrupt.end() - 4, 0x55);
        rejectWithoutMutation("unexpected trailing byte rejected without mutation", corrupt);
    }

    {
        HostInstance legacy(factory, info.cid);
        auto legacyBytes = makeLegacy(originalParameters);
        require("restore legacy 228-double state", legacy.loadState(legacyBytes));
        std::array<uint8_t, 228> migrated {};
        const auto migratedState = legacy.saveState();
        check("legacy state migrates to version 2", decodeSaved(migratedState, migrated));
        check("legacy migration preserves all parameters", migrated == originalParameters);
    }

    {
        HostInstance midi(factory, info.cid);
        midi.setup(48000.0);
        ParamID ccTag = kNoParamId;
        require("map MIDI channel 1 CC 88", midi.midiMapping->getMidiControllerAssignment(
            0, 0, 88, ccTag) == kResultOk);
        midi.processParameter(ccTag, 77.0 / 127.0);
        ParamID cc99 = kNoParamId, cc98 = kNoParamId, cc6 = kNoParamId;
        require("map NRPN selector CCs",
                midi.midiMapping->getMidiControllerAssignment(0, 0, 99, cc99) == kResultOk &&
                midi.midiMapping->getMidiControllerAssignment(0, 0, 98, cc98) == kResultOk &&
                midi.midiMapping->getMidiControllerAssignment(0, 0, 6, cc6) == kResultOk);
        midi.processParameter(cc99, 0.0);
        midi.processParameter(cc98, 1.0 / 127.0);
        midi.processParameter(cc6, 45.0 / 127.0);
        std::array<uint8_t, 228> midiState {};
        const auto bytes = midi.saveState();
        check("decode MIDI-edited state", decodeSaved(bytes, midiState));
        check("MIDI CC update reaches serialized host parameter", midiState[88] == 77);
        check("MIDI NRPN update reaches serialized host parameter", midiState[128] == 45);
    }

    {
        HostInstance first(factory, info.cid);
        HostInstance second(factory, info.cid);
        first.setRaw(1, 0); first.setRaw(8, 48); first.setRaw(88, 120);
        second.setRaw(1, 2); second.setRaw(8, 75); second.setRaw(88, 64);
        auto firstState = first.saveState();
        auto secondState = second.saveState();
        check("multiple instance states differ", firstState != secondState);

        HostInstance firstReloaded(factory, info.cid);
        HostInstance secondReloaded(factory, info.cid);
        require("reload first independent instance", firstReloaded.loadState(firstState));
        require("reload second independent instance", secondReloaded.loadState(secondState));
        check("first independent instance exact", firstReloaded.saveState() == firstState);
        check("second independent instance exact", secondReloaded.saveState() == secondState);
        firstReloaded.setup(48000.0);
        secondReloaded.setup(48000.0);
        const auto firstAudio = renderD1(firstReloaded);
        const auto secondAudio = renderD1(secondReloaded);
        check("differently configured instances render differently",
              firstAudio.left != secondAudio.left || firstAudio.right != secondAudio.right);
        check("both independent instances audible", firstAudio.peak > 0.0 && secondAudio.peak > 0.0);
    }

    std::printf("VST3_STATE bytes=%zu format=%u params=%zu peak=%.12f rms=%.12f\n",
                saved.size(), static_cast<unsigned>(lxr::state::kV2FormatVersion),
                lxr::state::kActiveParameterCount, originalAudio.peak, originalAudio.rms);
    factory->release();
    exitDll();
    FreeLibrary(module);
    std::printf("SUMMARY pass=%d fail=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
