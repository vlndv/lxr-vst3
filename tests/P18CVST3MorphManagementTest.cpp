// Exact-DLL P18-C morph-management acceptance; no mock processor path.

#include "../vendor/iPlug2/Examples/LXR/LXR.h"
#include "../dsp/KitFileIO.h"
#include "../dsp/Morphing.h"

#define main P12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main

#include <fstream>
#include <cwchar>

struct P18CMorphManagementTestProbe
{
    static LXR& plugin(HostInstance& host)
    {
        return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(
          static_cast<SingleComponentEffect*>(host.component)));
    }

    static void hold(
        HostInstance& host,
        lxr::morph::StateAuthority::WriteHandle& result)
    {
        require("hold production morph authority",
                plugin(host).mMorphState.tryBeginWrite(result));
    }

    static void release(
        HostInstance& host,
        lxr::morph::StateAuthority::WriteHandle& handle)
    {
        plugin(host).mMorphState.cancelUnmodified(handle);
    }

};

namespace
{
std::wstring widenC(const char* text)
{
    std::wstring result;
    if (text == nullptr) return result;
    while (*text != 0) result.push_back(static_cast<unsigned char>(*text++));
    return result;
}

std::wstring child(const char* folder, const wchar_t* name)
{
    auto result = widenC(folder);
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
    require("query P18-C morph interface",
            instance.component->queryInterface(
                INLINE_UID_OF(lxr::vst::IMorphManagement),
                reinterpret_cast<void**>(&result)) == kResultOk && result);
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
    require("read production morph state", api.getMorphState(&result) == kResultOk);
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

std::vector<std::uint8_t> readExactC(const std::wstring& path)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER size {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
        size.QuadPart > 4096)
    {
        CloseHandle(file);
        return {};
    }
    std::vector<std::uint8_t> result(static_cast<std::size_t>(size.QuadPart));
    DWORD transferred = 0;
    const bool ok = ReadFile(file, result.data(),
        static_cast<DWORD>(result.size()), &transferred, nullptr) &&
        transferred == result.size();
    CloseHandle(file);
    if (!ok) result.clear();
    return result;
}

lxr::vst::MorphOperationWire clearTarget(lxr::vst::IMorphManagement& api)
{
    lxr::vst::MorphOperationWire result;
    require("invoke production target clear",
            api.clearMorphTarget(&result) == kResultOk);
    return result;
}

lxr::vst::MorphOperationWire exportMorph(
    lxr::vst::IMorphManagement& api, const std::wstring& path)
{
    lxr::vst::MorphOperationWire result;
    require("invoke production MorphSnd export",
            api.exportMorphSnd(
                reinterpret_cast<const Steinberg::Vst::TChar*>(path.data()),
                static_cast<Steinberg::uint32>(path.size()), &result) ==
                kResultOk);
    return result;
}

lxr::vst::MorphOperationWire cancelLoad(lxr::vst::IMorphManagement& api)
{
    lxr::vst::MorphOperationWire result;
    require("invoke production morph cancellation",
            api.cancelMorphOperation(static_cast<Steinberg::uint32>(
                lxr::morph::management::Action::LoadTarget), &result) ==
                kResultOk);
    return result;
}

lxr::vst::IKitManagement* kitInterfaceC(HostInstance& instance)
{
    lxr::vst::IKitManagement* result = nullptr;
    require("query production kit interface",
            instance.component->queryInterface(
                INLINE_UID_OF(lxr::vst::IKitManagement),
                reinterpret_cast<void**>(&result)) == kResultOk && result);
    return result;
}

lxr::vst::KitOperationWire importSourceC(
    lxr::vst::IKitManagement& api, const std::wstring& path)
{
    lxr::vst::KitOperationWire result;
    require("invoke production source-kit load",
            api.importSnd(
                reinterpret_cast<const Steinberg::Vst::TChar*>(path.data()),
                static_cast<Steinberg::uint32>(path.size()), &result) ==
                kResultOk);
    return result;
}

lxr::vst::KitOperationWire exportSourceC(
    lxr::vst::IKitManagement& api, const std::wstring& path)
{
    lxr::vst::KitOperationWire result;
    require("invoke production whole-kit export",
            api.exportSnd(
                reinterpret_cast<const Steinberg::Vst::TChar*>(path.data()),
                static_cast<Steinberg::uint32>(path.size()), &result) ==
                kResultOk);
    return result;
}

std::uint8_t firmwareByte(std::uint8_t a, std::uint8_t b,
                          std::uint8_t amount)
{
    const std::int32_t fixed = static_cast<std::int32_t>(a) * 256 +
        (static_cast<std::int32_t>(b) - static_cast<std::int32_t>(a)) *
        amount;
    return static_cast<std::uint8_t>(
        fixed / 256 + ((fixed & 255) >= 127 ? 1 : 0));
}
} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 3) return 2;

    HMODULE module = LoadLibraryA(argv[1]);
    require("load exact private P18-C VST3", module != nullptr);
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
    auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
    auto factoryFn = reinterpret_cast<IPluginFactory* (*)()>(
        GetProcAddress(module, "GetPluginFactory"));
    require("resolve P18-C VST3 exports", init && exitDll && factoryFn);
    require("initialize P18-C VST3", init());
    auto* factory = factoryFn();
    require("get P18-C factory", factory != nullptr);
    PClassInfo info {};
    bool found = false;
    for (int i = 0; i < factory->countClasses(); ++i)
        if (factory->getClassInfo(i, &info) == kResultOk &&
            std::strcmp(info.category, kVstAudioEffectClass) == 0)
        { found = true; break; }
    require("find P18-C LXR class", found);

    lxr::state::ProjectCandidate sourceCandidate;
    std::vector<std::uint8_t> sourceState;
    {
        HostInstance source(factory, info.cid);
        source.setRaw(8, 21);
        source.setRaw(37, 89);
        source.setRaw(88, 113);
        sourceState = source.saveState();
        require("decode P18-C source A", lxr::state::DecodeProject(
            sourceState.data(), sourceState.size(), sourceCandidate, true));
    }

    const std::array<std::uint8_t, 8> binaryName
        {{'M', 0x01u, 'R', 0u, 'P', 0u, 0u, 0u}};
    const std::array<std::uint8_t, 8> replacementName
        {{'R','E','P','L','A','C','E','D'}};
    const auto targetBytes = fixture(sourceCandidate, binaryName, 31u, 97u);
    const auto replacementBytes = fixture(
        sourceCandidate, replacementName, 73u, 41u);
    auto invalidBytes = targetBytes;
    invalidBytes[9u] = 6u;
    const auto targetPath = child(argv[2], L"p18c-target.snd");
    const auto replacementPath = child(argv[2], L"p18c-replacement.snd");
    const auto invalidPath = child(argv[2], L"p18c-invalid.snd");
    const auto exportPath = child(argv[2], L"p18c-export.snd");
    const auto secondExportPath = child(argv[2], L"p18c-export-second.snd");
    const auto roundTripPath = child(argv[2], L"p18c-roundtrip.snd");
    require("write P18-C target", writeNew(targetPath, targetBytes));
    require("write P18-C replacement", writeNew(replacementPath, replacementBytes));
    require("write P18-C invalid target", writeNew(invalidPath, invalidBytes));

    {
        HostInstance instance(factory, info.cid);
        require("restore P18-C source", instance.loadState(sourceState));
        auto* morphApi = morphInterface(instance);
        auto* kitApi = kitInterfaceC(instance);
        const auto before = instance.saveState();

        const auto loaded = importTarget(*morphApi, targetPath);
        check("production target load exposes binary identity",
              loaded.status == static_cast<Steinberg::uint32>(
                  lxr::morph::management::Status::Success) &&
              loaded.targetPresent == 1u &&
              std::equal(binaryName.begin(), binaryName.end(), loaded.name));
        check("target selection preserves authoritative source A",
              instance.saveState() == before);
        const auto cancelled = cancelLoad(*morphApi);
        check("dialog cancellation is explicit and retains accepted target",
              cancelled.status == static_cast<Steinberg::uint32>(
                  lxr::morph::management::Status::Cancelled) &&
              cancelled.targetPresent == 1u);

        const auto failed = importTarget(*morphApi, invalidPath);
        const auto retained = morphState(*morphApi);
        check("invalid target reports typed error and retains prior target",
              failed.status == static_cast<Steinberg::uint32>(
                  lxr::morph::management::Status::UnsupportedField) &&
              std::equal(binaryName.begin(), binaryName.end(), retained.name));

        const auto replaced = importTarget(*morphApi, replacementPath);
        check("validated replacement is atomic",
              replaced.targetPresent == 1u &&
              std::equal(replacementName.begin(), replacementName.end(),
                         replaced.name));
        require("restore binary target for export",
                importTarget(*morphApi, targetPath).status ==
                static_cast<Steinberg::uint32>(
                    lxr::morph::management::Status::Success));

        instance.setup(48000.0);
        for (unsigned amount = 0; amount < 256u; ++amount)
            instance.processParameter(lxr::vst::kMorphAmountParam,
                                      static_cast<double>(amount) / 255.0);
        check("rapid amount automation publishes coherent final value",
              morphState(*morphApi).amount == 255u);
        instance.processParameter(lxr::vst::kMorphAmountParam, 127.0 / 255.0);
        const auto exported = exportMorph(*morphApi, exportPath);
        check("production MorphSnd export publishes exact-size file",
              exported.status == static_cast<Steinberg::uint32>(
                  lxr::morph::management::Status::Success) &&
              exported.published == 1u && readExactC(exportPath).size() == 236u);
        const auto actual = readExactC(exportPath);
        auto expected = targetBytes;
        for (std::size_t i = 0; i < 228u; ++i)
            expected[8u + i] = firmwareByte(
                sourceCandidate.patch[i], targetBytes[8u + i], 127u);
        for (std::size_t i = 149u; i <= 154u; ++i)
            expected[8u + i] = sourceCandidate.patch[i];
        for (std::size_t i = 161u; i <= 172u; ++i)
            expected[8u + i] = sourceCandidate.patch[i];
        check("exact-DLL export matches coherent firmware projection",
              actual == expected);
        check("existing MorphSnd destination is never overwritten",
              exportMorph(*morphApi, exportPath).status ==
              static_cast<Steinberg::uint32>(
                  lxr::morph::management::Status::ExistingDestination) &&
              readExactC(exportPath) == actual);

        const auto sourceLoaded = importSourceC(*kitApi, replacementPath);
        const auto afterSourceLoad = morphState(*morphApi);
        check("source-kit load succeeds while B remains loaded",
              sourceLoaded.status == static_cast<Steinberg::uint32>(
                  lxr::kit::management::Status::Success) &&
              afterSourceLoad.targetPresent == 1u &&
              std::equal(binaryName.begin(), binaryName.end(),
                         afterSourceLoad.name));

        instance.processParameter(8u, 55.0 / 127.0);
        check("host source edit is accepted without losing target",
              morphState(*morphApi).targetPresent == 1u);
        check("updated source can be exported coherently",
              exportMorph(*morphApi, secondExportPath).status ==
              static_cast<Steinberg::uint32>(
                  lxr::morph::management::Status::Success));

        lxr::morph::StateAuthority::WriteHandle held;
        P18CMorphManagementTestProbe::hold(instance, held);
        const auto busy = clearTarget(*morphApi);
        check("busy clear rejects without blocking or mutation",
              busy.status == static_cast<Steinberg::uint32>(
                  lxr::morph::management::Status::PublicationBusy) &&
              morphState(*morphApi).targetPresent == 1u);
        P18CMorphManagementTestProbe::release(instance, held);

        const auto cleared = clearTarget(*morphApi);
        check("clear removes target but retains amount 127",
              cleared.status == static_cast<Steinberg::uint32>(
                  lxr::morph::management::Status::Success) &&
              cleared.targetPresent == 0u && cleared.amount == 127u);
        const auto stateAfterClear = instance.saveState();
        lxr::state::ProjectCandidate decodedAfterClear;
        check("clear leaves source project state representable",
              lxr::state::DecodeProject(stateAfterClear.data(),
                  stateAfterClear.size(), decodedAfterClear, true) &&
              (stateAfterClear.size() == 51065u ||
               stateAfterClear.size() == 51089u));
        check("export without target rejects explicitly",
              exportMorph(*morphApi, child(argv[2], L"no-target.snd")).status ==
              static_cast<Steinberg::uint32>(
                  lxr::morph::management::Status::NoTarget));
        check("configured exact DLL reports 26-sample PDC",
              instance.processor->getLatencySamples() == 26u);
        kitApi->release();
        morphApi->release();
    }

    {
        HostInstance roundTrip(factory, info.cid);
        auto* api = kitInterfaceC(roundTrip);
        check("MorphSnd imports through the production whole-kit path",
              importSourceC(*api, exportPath).status ==
              static_cast<Steinberg::uint32>(
                  lxr::kit::management::Status::Success));
        check("imported MorphSnd re-exports byte-identically",
              exportSourceC(*api, roundTripPath).status ==
                  static_cast<Steinberg::uint32>(
                      lxr::kit::management::Status::Success) &&
              readExactC(roundTripPath) == readExactC(exportPath));
        api->release();
    }

    for (const auto amount : std::array<std::uint8_t, 3>{{0u, 127u, 255u}})
    {
        HostInstance instance(factory, info.cid);
        auto* api = morphInterface(instance);
        require("load target for clear endpoint",
                importTarget(*api, targetPath).targetPresent == 1u);
        instance.setup(48000.0);
        instance.processParameter(lxr::vst::kMorphAmountParam,
                                  static_cast<double>(amount) / 255.0);
        (void) renderD1(instance);
        const auto cleared = clearTarget(*api);
        check("clear endpoint keeps amount and removes active target",
              cleared.amount == amount && cleared.targetPresent == 0u);
        api->release();
    }

    {
        HostInstance restored(factory, info.cid);
        auto state = sourceState;
        require("restore project without transient target",
                restored.loadState(state));
        auto* api = morphInterface(restored);
        const auto empty = morphState(*api);
        check("project reopen cannot claim an unpersisted target",
              empty.targetPresent == 0u && empty.amount == 0u);
        api->release();
    }

    factory->release();
    require("shutdown P18-C VST3", exitDll());
    FreeLibrary(module);
    std::printf("P18C VST3 SUMMARY pass=%d fail=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
