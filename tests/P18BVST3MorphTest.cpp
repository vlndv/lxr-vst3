// Exact-DLL P18-B transient morph acceptance. No mock processor path.

#include "../vendor/iPlug2/Examples/LXR/LXR.h"
#include "../dsp/KitFileIO.h"
#include "../dsp/Morphing.h"

#define main P12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main

#include <cwchar>

struct P18BMorphRuntimeTestProbe
{
  static LXR& plugin(HostInstance& host)
  {
    return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(
      static_cast<SingleComponentEffect*>(host.component)));
  }

  static void idle(HostInstance& host)
  {
    auto* delegate = static_cast<iplug::IPlugAPIBase*>(&plugin(host));
    delegate->OnIdle();
  }
};

namespace
{
std::wstring widen(const char* text)
{
    std::wstring result;
    if (text == nullptr) return result;
    while (*text != 0) result.push_back(static_cast<unsigned char>(*text++));
    return result;
}

std::wstring child(const char* folder, const wchar_t* name)
{
    auto result = widen(folder);
    if (!result.empty() && result.back() != L'\\') result.push_back(L'\\');
    result += name;
    return result;
}

bool writeNew(const std::wstring& path,
              const std::vector<std::uint8_t>& bytes)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD transferred = 0;
    const bool ok = WriteFile(file, bytes.data(),
                              static_cast<DWORD>(bytes.size()), &transferred,
                              nullptr) && transferred == bytes.size() &&
                    FlushFileBuffers(file);
    return CloseHandle(file) && ok;
}

lxr::vst::IMorphManagement* morphInterface(HostInstance& instance)
{
    lxr::vst::IMorphManagement* result = nullptr;
    require("query P18-B morph interface",
            instance.component->queryInterface(
                INLINE_UID_OF(lxr::vst::IMorphManagement),
                reinterpret_cast<void**>(&result)) == kResultOk &&
            result != nullptr);
    return result;
}

lxr::vst::MorphOperationWire importTarget(
    lxr::vst::IMorphManagement& api, const std::wstring& path)
{
    lxr::vst::MorphOperationWire result;
    require("invoke production morph import",
            api.importMorphSnd(
                reinterpret_cast<const Steinberg::Vst::TChar*>(path.data()),
                static_cast<Steinberg::uint32>(path.size()), &result) ==
                kResultOk);
    return result;
}

lxr::vst::MorphOperationWire morphState(lxr::vst::IMorphManagement& api)
{
    lxr::vst::MorphOperationWire result;
    require("read production morph state",
            api.getMorphState(&result) == kResultOk);
    return result;
}

std::vector<std::uint8_t> fixture(
    const lxr::state::ProjectCandidate& source,
    const std::array<std::uint8_t, 8>& name,
    std::uint8_t volume, std::uint8_t pitch)
{
    std::vector<std::uint8_t> bytes(236u);
    std::copy(name.begin(), name.end(), bytes.begin());
    std::copy(source.patch.begin(), source.patch.end(), bytes.begin() + 8u);
    bytes[8u + 8u] = pitch;
    bytes[8u + 37u] = 19u;
    bytes[8u + 88u] = volume;
    bytes[8u + 134u] = 0u;
    bytes[8u + 191u] = 7u;
    return bytes;
}

struct RenderedMorph
{
    AudioRender audio;
    std::vector<std::uint8_t> state;
    lxr::vst::MorphOperationWire morph;
    double controllerAmount = 0.0;
};

RenderedMorph renderAmount(IPluginFactory& factory, const TUID cid,
                           std::vector<std::uint8_t> sourceState,
                           const std::wstring& targetPath,
                           std::uint8_t amount)
{
    HostInstance instance(&factory, cid);
    require("restore source A for morph render",
            instance.loadState(sourceState));
    auto* api = morphInterface(instance);
    const auto loaded = importTarget(*api, targetPath);
    require("load target B for morph render",
            loaded.fileError == static_cast<Steinberg::uint32>(
                lxr::kit::snd::FileError::Ok) &&
            loaded.publicationStatus == static_cast<Steinberg::uint32>(
                lxr::morph::PublicationStatus::Ok));
    instance.setup(48000.0);
    instance.processParameter(
        lxr::vst::kMorphAmountParam,
        static_cast<double>(amount) / 255.0);
    RenderedMorph result;
    result.audio = renderD1(instance);
    result.state = instance.saveState();
    result.morph = morphState(*api);
    P18BMorphRuntimeTestProbe::idle(instance);
    result.controllerAmount = instance.controller->normalizedParamToPlain(
        lxr::vst::kMorphAmountParam,
        instance.controller->getParamNormalized(
            lxr::vst::kMorphAmountParam));
    api->release();
    return result;
}
} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 3) return 2;
    HMODULE module = LoadLibraryA(argv[1]);
    require("load exact private P18-B VST3", module != nullptr);
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
    auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
    auto factoryFn = reinterpret_cast<IPluginFactory* (*)()>(
        GetProcAddress(module, "GetPluginFactory"));
    require("resolve P18-B VST3 exports", init && exitDll && factoryFn);
    require("initialize P18-B VST3", init());
    auto* factory = factoryFn();
    require("get P18-B factory", factory != nullptr);
    PClassInfo info {};
    bool found = false;
    for (int i = 0; i < factory->countClasses(); ++i)
        if (factory->getClassInfo(i, &info) == kResultOk &&
            std::strcmp(info.category, kVstAudioEffectClass) == 0)
        { found = true; break; }
    require("find P18-B LXR class", found);

    lxr::state::ProjectCandidate initial;
    std::vector<std::uint8_t> sourceState;
    {
        HostInstance source(factory, info.cid);
        source.setRaw(8, 54);
        source.setRaw(37, 82);
        source.setRaw(88, 111);
        sourceState = source.saveState();
        require("decode authoritative source A",
                lxr::state::DecodeProject(sourceState.data(),
                    sourceState.size(), initial, true));
    }

    const std::array<std::uint8_t, 8> nameA {{'P','1','8','B','T','A','R','A'}};
    const std::array<std::uint8_t, 8> nameB {{'P','1','8','B','T','A','R','B'}};
    const auto bytesA = fixture(initial, nameA, 23u, 91u);
    const auto bytesB = fixture(initial, nameB, 67u, 74u);
    auto invalid = bytesA;
    invalid[8u + 1u] = 6u; // External/user sample is runtime-unsupported.
    const auto pathA = child(argv[2], L"morph-a.snd");
    const auto pathB = child(argv[2], L"morph-b.snd");
    const auto invalidPath = child(argv[2], L"morph-invalid.snd");
    require("write morph target A", writeNew(pathA, bytesA));
    require("write morph target B", writeNew(pathB, bytesB));
    require("write invalid morph target", writeNew(invalidPath, invalid));

    {
        HostInstance instance(factory, info.cid);
        require("restore source before transient import",
                instance.loadState(sourceState));
        auto* api = morphInterface(instance);
        const auto before = instance.saveState();
        const auto loaded = importTarget(*api, pathA);
        check("target load succeeds with exact name",
              loaded.fileError == 0u &&
              loaded.publicationStatus == static_cast<Steinberg::uint32>(
                  lxr::morph::PublicationStatus::Ok) &&
              loaded.targetPresent == 1u && loaded.amount == 0u &&
              std::equal(nameA.begin(), nameA.end(), loaded.name));
        check("target B is absent from LXRS persistence",
              instance.saveState() == before && before == sourceState);
        const auto accepted = morphState(*api);
        const auto rejected = importTarget(*api, invalidPath);
        const auto retained = morphState(*api);
        check("invalid runtime target rejects before mutation",
              rejected.issue == static_cast<Steinberg::uint32>(
                  lxr::kit::Issue::ExternalSample) &&
              rejected.field == 1u &&
              rejected.publicationStatus == static_cast<Steinberg::uint32>(
                  lxr::morph::PublicationStatus::InvalidCandidate));
        check("invalid target retains accepted B and generation",
              retained.targetPresent == 1u &&
              retained.generation == accepted.generation &&
              std::equal(nameA.begin(), nameA.end(), retained.name));
        api->release();
    }

    const auto amount0 = renderAmount(*factory, info.cid, sourceState, pathA, 0u);
    const auto amount128 = renderAmount(*factory, info.cid, sourceState, pathA, 128u);
    const auto amount128Repeat = renderAmount(*factory, info.cid, sourceState, pathA, 128u);
    const auto amount255 = renderAmount(*factory, info.cid, sourceState, pathA, 255u);
    check("amount automation reaches transient authority and controller",
          amount128.morph.amount == 128u &&
          amount128.controllerAmount == 128.0);
    check("morph amount and target do not alter project state",
          amount0.state == sourceState && amount128.state == sourceState &&
          amount255.state == sourceState);
    check("exact private VST3 morph PCM is deterministic",
          amount128.audio.left == amount128Repeat.audio.left &&
          amount128.audio.right == amount128Repeat.audio.right);
    check("amount 0, midpoint and 255 produce distinct PCM",
          amount0.audio.left != amount128.audio.left &&
          amount128.audio.left != amount255.audio.left);

    {
        HostInstance restored(factory, info.cid);
        auto state = amount128.state;
        require("restore LXRS state without morph session",
                restored.loadState(state));
        auto* api = morphInterface(restored);
        const auto transient = morphState(*api);
        check("new instance does not restore target or amount",
              transient.targetPresent == 0u && transient.amount == 0u &&
              transient.generation == 0u);
        api->release();
    }

    {
        HostInstance first(factory, info.cid);
        HostInstance second(factory, info.cid);
        auto* firstApi = morphInterface(first);
        auto* secondApi = morphInterface(second);
        check("two target imports succeed independently",
              importTarget(*firstApi, pathA).publicationStatus == 0u &&
              importTarget(*secondApi, pathB).publicationStatus == 0u);
        first.setup(48000.0);
        second.setup(48000.0);
        first.processParameter(lxr::vst::kMorphAmountParam, 64.0 / 255.0);
        second.processParameter(lxr::vst::kMorphAmountParam, 192.0 / 255.0);
        const auto firstState = morphState(*firstApi);
        const auto secondState = morphState(*secondApi);
        check("two live morph sessions are independent",
              firstState.amount == 64u && secondState.amount == 192u &&
              std::equal(nameA.begin(), nameA.end(), firstState.name) &&
              std::equal(nameB.begin(), nameB.end(), secondState.name));
        firstApi->release();
        secondApi->release();
    }

    factory->release();
    require("shutdown P18-B VST3", exitDll());
    FreeLibrary(module);
    std::printf("P18B VST3 SUMMARY pass=%d fail=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
