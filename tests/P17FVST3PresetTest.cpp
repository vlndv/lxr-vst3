// Exact-DLL P17-F host-state and standard VST3 preset acceptance.

#include "public.sdk/source/vst/vstpresetfile.h"
#include "../vendor/iPlug2/Examples/LXR/LXRParamContract.h"

#define LXR_EMBED_P17E_SUITE 1
#include "P17EVST3KitManagementTest.cpp"
#undef LXR_EMBED_P17E_SUITE

namespace
{
using Steinberg::Vst::PresetFile;

lxr::state::ProjectCandidate decodeState(const std::vector<std::uint8_t>& bytes)
{
    lxr::state::ProjectCandidate result;
    require("decode P17-F project state",
            lxr::state::DecodeProject(bytes.data(), bytes.size(), result, true));
    return result;
}

bool sameName(const Steinberg::uint8* wire,
              const std::array<std::uint8_t, 8>& name)
{
    return std::equal(name.begin(), name.end(), wire);
}
} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 3) return 2;
    HMODULE module = LoadLibraryA(argv[1]);
    require("load exact private VST3", module != nullptr);
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
    auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
    auto factoryFn = reinterpret_cast<IPluginFactory* (*)()>(
        GetProcAddress(module, "GetPluginFactory"));
    require("resolve exact VST3 exports", init && exitDll && factoryFn);
    require("initialize exact private VST3", init());
    auto* factory = factoryFn();
    require("get exact plugin factory", factory != nullptr);
    PClassInfo info {};
    bool found = false;
    for (int i = 0; i < factory->countClasses(); ++i)
        if (factory->getClassInfo(i, &info) == kResultOk &&
            std::strcmp(info.category, kVstAudioEffectClass) == 0)
        {
            found = true;
            break;
        }
    require("find exact LXR class", found);
    const auto classID = FUID::fromTUID(info.cid);

    const std::array<std::uint8_t, 8> name {{'P',0,0xffu,'S','E','T',0,0}};
    const auto kitPath = child(argv[2], L"p17f-name.snd");
    const auto exportPath = child(argv[2], L"p17f-export.snd");
    std::vector<std::uint8_t> historicalState;
    std::vector<std::uint8_t> acceptedState;
    AudioRender beforePreset, afterPreset;

    {
        HostInstance instance(factory, info.cid);
        auto* api = kitInterface(instance);
        historicalState = instance.saveState();
        auto historical = decodeState(historicalState);
        check("historical VST3 state remains exactly 51065 bytes",
              historicalState.size() == lxr::state::kV3CanonicalStreamBytes &&
              !historical.kitIdentityPresent);
        historical.bank.patterns[6].mainGates[4] = 0x4242u;
        historical.transport.enabled = 1u;
        historical.transport.bpm = 181u;
        historical.transport.pattern = 6u;
        historical.routing.bypass = false;
        historicalState = encodeProject(historical);
        require("seed historical pattern and transport state",
                historicalState.size() == lxr::state::kV3CanonicalStreamBytes &&
                instance.loadState(historicalState));

        const auto kitBytes = fixture(historical, name, 92u);
        require("write P17-F source kit", writeNew(kitPath, kitBytes));
        const auto imported = importKit(*api, kitPath);
        check("import publishes exact binary identity",
              status(imported, manage::Status::Success) &&
              sameName(imported.name, name));
        pumpMain();
        acceptedState = instance.saveState();
        const auto accepted = decodeState(acceptedState);
        check("identity-bearing VST3 state is exactly 51089 bytes",
              acceptedState.size() == lxr::state::kV3IdentityStreamBytes &&
              accepted.kitIdentityPresent && accepted.kitName == name);
        check("identity state preserves patch patterns transport and bypass",
              accepted.patch[88] == 92u &&
              accepted.bank.patterns[6].mainGates[4] == 0x4242u &&
              accepted.transport.enabled == 1u &&
              accepted.transport.bpm == 181u &&
              accepted.transport.pattern == 6u && !accepted.routing.bypass);
        check("import creates no host edit gestures", instance.handler.edits == 0);

        const auto exported = exportKit(*api, exportPath);
        check("export after state integration succeeds",
              status(exported, manage::Status::Success));
        check("export preserves exact eight-byte identity",
              readExact(exportPath) == kitBytes);

        instance.setup(48000.0);
        check("P17-F retains 26-sample PDC",
              instance.processor->getLatencySamples() == 26u);
        MemoryStream preset;
        require("SDK writes standard VST3 preset",
                PresetFile::savePreset(&preset, classID, instance.component,
                                       instance.controller));
        check("standard preset has VST3 container header",
              preset.getSize() > static_cast<TSize>(acceptedState.size()) &&
              std::memcmp(preset.getData(), "VST3", 4) == 0);
        beforePreset = renderD1(instance);

        instance.processParameter(88u, 17.0 / 127.0);
        instance.processParameter(lxr::vst::kSequencerRollTracksParam, 1.0);
        pumpMain();
        auto changed = decodeState(instance.saveState());
        check("pre-preset mutation took effect", changed.patch[88] == 17u);
        check("ordinary automation preserves kit identity",
              changed.kitIdentityPresent && changed.kitName == name);
        require("SDK loads standard VST3 preset",
                PresetFile::loadPreset(&preset, classID, instance.component,
                                       instance.controller));
        pumpMain();
        const auto restoredBytes = instance.saveState();
        const auto restored = decodeState(restoredBytes);
        check("host preset restores exact complete component state",
              restoredBytes == acceptedState && restored.kitName == name &&
              restored.kitIdentityPresent);
        check("host preset does not leave manual roll armed",
              instance.controller->normalizedParamToPlain(
                  lxr::vst::kSequencerRollTracksParam,
                  instance.controller->getParamNormalized(
                      lxr::vst::kSequencerRollTracksParam)) == 0.0);
        lxr::vst::KitOperationWire last;
        require("read restored kit identity", api->getKitOperation(&last) == kResultOk);
        check("controller-side operation identity follows preset restore",
              sameName(last.name, name));
        afterPreset = renderD1(instance);
        check("host preset restoration produces deterministic PCM",
              beforePreset.left == afterPreset.left &&
              beforePreset.right == afterPreset.right);

        const auto stable = instance.saveState();
        auto corrupt = stable;
        corrupt[lxr::state::kV3CanonicalCoreBytes + 16u] ^= 0x80u;
        check("corrupt identity module rejects", !instance.loadState(corrupt));
        check("corrupt identity rejection preserves complete state",
              instance.saveState() == stable);

        require("historical v3 still restores", instance.loadState(historicalState));
        pumpMain();
        const auto migrated = decodeState(instance.saveState());
        check("historical restore uses absent zero identity default",
              !migrated.kitIdentityPresent && migrated.kitName ==
                  std::array<std::uint8_t, 8>{} &&
              instance.saveState().size() == lxr::state::kV3CanonicalStreamBytes);
        require("read historical restore identity", api->getKitOperation(&last) == kResultOk);
        check("historical restore reconciles displayed raw identity",
              sameName(last.name, std::array<std::uint8_t, 8>{}));
        api->release();
    }

    {
        HostInstance first(factory, info.cid);
        HostInstance second(factory, info.cid);
        auto* firstApi = kitInterface(first);
        auto* secondApi = kitInterface(second);
        check("instance A restores identity-bearing state",
              first.loadState(acceptedState));
        check("instance B restores historical state",
              second.loadState(historicalState));
        const auto firstState = decodeState(first.saveState());
        const auto secondState = decodeState(second.saveState());
        check("two instances retain independent identity provenance",
              firstState.kitIdentityPresent && firstState.kitName == name &&
              !secondState.kitIdentityPresent && secondState.kitName ==
                  std::array<std::uint8_t, 8>{});
        firstApi->release();
        secondApi->release();
    }

    factory->release();
    require("shutdown exact private VST3", exitDll());
    FreeLibrary(module);
    std::printf("P17F VST3 PRESET SUMMARY pass=%d fail=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
