// Exact-DLL P17-E production-operation acceptance. No mock kit loader.

#include "../vendor/iPlug2/Examples/LXR/LXRKitManagementInterface.h"
#include "../dsp/KitManagement.h"

#define main P12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main

#include <cwchar>

namespace
{
namespace manage = lxr::kit::management;

void pumpMain(unsigned milliseconds = 180u)
{
    const auto deadline = GetTickCount64() + milliseconds;
    do
    {
        MSG message;
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
        Sleep(1);
    } while (GetTickCount64() < deadline);
}

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

bool writeNew(const std::wstring& path, const std::vector<std::uint8_t>& bytes)
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

std::vector<std::uint8_t> readExact(const std::wstring& path)
{
    std::vector<std::uint8_t> result;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) return result;
    LARGE_INTEGER size {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
        size.QuadPart > 65536)
    {
        CloseHandle(file);
        return result;
    }
    result.resize(static_cast<std::size_t>(size.QuadPart));
    DWORD transferred = 0;
    const bool ok = ReadFile(file, result.data(),
                             static_cast<DWORD>(result.size()), &transferred,
                             nullptr) && transferred == result.size();
    CloseHandle(file);
    if (!ok) result.clear();
    return result;
}

lxr::vst::IKitManagement* kitInterface(HostInstance& instance)
{
    lxr::vst::IKitManagement* result = nullptr;
    require("query P17-E production interface",
            instance.component->queryInterface(
                INLINE_UID_OF(lxr::vst::IKitManagement),
                reinterpret_cast<void**>(&result)) == kResultOk &&
            result != nullptr);
    return result;
}

lxr::vst::KitOperationWire importKit(lxr::vst::IKitManagement& api,
                                     const std::wstring& path)
{
    lxr::vst::KitOperationWire result;
    require("invoke production import",
            api.importSnd(
                reinterpret_cast<const Steinberg::Vst::TChar*>(path.data()),
                static_cast<Steinberg::uint32>(path.size()), &result) ==
                kResultOk);
    return result;
}

lxr::vst::KitOperationWire exportKit(lxr::vst::IKitManagement& api,
                                     const std::wstring& path)
{
    lxr::vst::KitOperationWire result;
    require("invoke production export",
            api.exportSnd(
                reinterpret_cast<const Steinberg::Vst::TChar*>(path.data()),
                static_cast<Steinberg::uint32>(path.size()), &result) ==
                kResultOk);
    return result;
}

bool status(const lxr::vst::KitOperationWire& result, manage::Status expected)
{
    return result.status == static_cast<Steinberg::uint32>(expected);
}

bool decodeProject(const std::vector<std::uint8_t>& bytes,
                   lxr::state::ProjectCandidate& candidate)
{
    return lxr::state::DecodeProject(bytes.data(), bytes.size(), candidate, true);
}

std::vector<std::uint8_t> encodeProject(
    const lxr::state::ProjectCandidate& candidate)
{
    lxr::state::ProjectBytes bytes;
    std::size_t size = 0;
    if (!lxr::state::EncodeProject(candidate, bytes, size, true)) return {};
    return {bytes.begin(), bytes.begin() + size};
}

bool unchangedSequencer(const lxr::state::ProjectCandidate& before,
                        const lxr::state::ProjectCandidate& after)
{
    lxr::sequencer::state::EncodedBank a {}, b {};
    std::array<std::uint8_t, 16> ta {}, tb {};
    return lxr::sequencer::state::Encode(before.bank, a) &&
           lxr::sequencer::state::Encode(after.bank, b) && a == b &&
           lxr::sequencer::state::EncodeTransport(before.transport, ta.data()) &&
           lxr::sequencer::state::EncodeTransport(after.transport, tb.data()) &&
           ta == tb && before.routing.bypass == after.routing.bypass;
}

std::vector<std::uint8_t> fixture(
    const lxr::state::ProjectCandidate& initial,
    const std::array<std::uint8_t, 8>& name, std::uint8_t volume)
{
    std::vector<std::uint8_t> result(236u);
    std::copy(name.begin(), name.end(), result.begin());
    std::copy(initial.patch.begin(), initial.patch.end(), result.begin() + 8);
    result[8u + 88u] = volume;
    return result;
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

    lxr::state::ProjectCandidate initial;
    {
        HostInstance instance(factory, info.cid);
        require("decode initial LXRS v3", decodeProject(instance.saveState(), initial));
    }
    const std::array<std::uint8_t, 8> nameA {{'P','1','7','E',0x01u,0xffu,0,0}};
    const std::array<std::uint8_t, 8> nameB {{'P','1','7','E','L','O','U','D'}};
    const auto bytesA = fixture(initial, nameA, 21u);
    const auto bytesB = fixture(initial, nameB, 119u);
    const auto pathA = child(argv[2], L"kit-a.snd");
    const auto pathB = child(argv[2], L"kit-b.snd");
    const auto invalidPath = child(argv[2], L"invalid.snd");
    const auto exportPath = child(argv[2], L"exported.snd");
    require("write exact kit A fixture", writeNew(pathA, bytesA));
    require("write exact kit B fixture", writeNew(pathB, bytesB));
    require("write invalid-size fixture", writeNew(invalidPath, {0, 1, 2}));

    std::vector<std::uint8_t> acceptedState;
    AudioRender audioA, audioB;
    {
        HostInstance instance(factory, info.cid);
        auto* api = kitInterface(instance);
        lxr::state::ProjectCandidate before, after;
        require("decode pre-import project", decodeProject(instance.saveState(), before));
        before.bank.patterns[3].mainGates[2] = 0x5678u;
        before.transport.enabled = 1u;
        before.transport.bpm = 177u;
        before.transport.pattern = 3u;
        auto seededState = encodeProject(before);
        require("seed nondefault sequencer and transport state",
                !seededState.empty() && instance.loadState(seededState));
        require("read seeded project before import",
                decodeProject(instance.saveState(), before));
        const auto imported = importKit(*api, pathA);
        check("valid import reports publication success",
              status(imported, manage::Status::Success) &&
              imported.published == 1u);
        check("binary name bytes preserved by production import",
              std::equal(nameA.begin(), nameA.end(), imported.name));
        pumpMain();
        acceptedState = instance.saveState();
        require("decode accepted project", decodeProject(acceptedState, after));
        check("all 228 imported values reach processor state",
              after.patch[88] == 21u &&
              std::equal(after.patch.begin(), after.patch.end(),
                         bytesA.begin() + 8));
        bool controllerPatch = true;
        for (Steinberg::Vst::ParamID id = 0; id < 228u; ++id)
            controllerPatch = controllerPatch &&
                instance.controller->normalizedParamToPlain(
                    id, instance.controller->getParamNormalized(id)) ==
                    after.patch[id];
        check("all 228 controller values reconcile", controllerPatch);
        bool controllerRoutes = true;
        for (Steinberg::Vst::ParamID slot = 0; slot < 12u; ++slot)
        {
            const auto id = 228u + slot;
            const auto field = 149u + (slot >= 6u ? 18u : 0u) + slot % 6u;
            controllerRoutes = controllerRoutes &&
                instance.controller->normalizedParamToPlain(
                    id, instance.controller->getParamNormalized(id)) ==
                    bytesA[8u + field];
        }
        check("all twelve route controls reconcile", controllerRoutes);
        check("kit import preserves patterns transport and bypass",
              unchangedSequencer(before, after));
        check("import sends no host edit gestures", instance.handler.edits == 0);

        const auto exported = exportKit(*api, exportPath);
        check("valid export reports success and publication",
              status(exported, manage::Status::Success) &&
              exported.published == 1u);
        check("production export is exact 236-byte round trip",
              readExact(exportPath) == bytesA);
        const auto existing = exportKit(*api, exportPath);
        check("existing export destination rejects explicitly",
              status(existing, manage::Status::ExistingDestination));

        const auto stateBeforeBad = instance.saveState();
        const auto rejected = importKit(*api, invalidPath);
        check("invalid-size import rejects explicitly",
              status(rejected, manage::Status::InvalidFileSize));
        check("rejected import preserves complete LXRS state",
              instance.saveState() == stateBeforeBad);
        lxr::vst::KitOperationWire last;
        require("read last production result",
                api->getKitOperation(&last) == kResultOk);
        check("last operation records rejection", last.status == rejected.status);

        instance.setup(48000.0);
        check("imported instance retains 26-sample PDC",
              instance.processor->getLatencySamples() == 26u);
        audioA = renderD1(instance);
        instance.processParameter(88u, 77.0 / 127.0);
        pumpMain();
        lxr::state::ProjectCandidate automated;
        require("decode post-import host automation",
                decodeProject(instance.saveState(), automated));
        check("host automation after import remains authoritative",
              automated.patch[88] == 77u &&
              instance.controller->normalizedParamToPlain(
                  88u, instance.controller->getParamNormalized(88u)) == 77.0);
        api->release();
    }

    {
        HostInstance restored(factory, info.cid);
        require("restore imported LXRS v3 musical state",
                restored.loadState(acceptedState));
        check("save/reopen preserves canonical 51,065-byte musical state",
              restored.saveState() == acceptedState &&
              acceptedState.size() == lxr::state::kV3CanonicalStreamBytes);
    }

    {
        HostInstance instance(factory, info.cid);
        auto* api = kitInterface(instance);
        const auto imported = importKit(*api, pathB);
        check("second distinct kit imports", status(imported, manage::Status::Success));
        pumpMain();
        lxr::state::ProjectCandidate second;
        require("decode second imported state", decodeProject(instance.saveState(), second));
        check("second instance owns independent kit", second.patch[88] == 119u);
        instance.setup(48000.0);
        audioB = renderD1(instance);
        api->release();
    }
    check("distinct valid kits produce different actual VST3 PCM",
          audioA.left != audioB.left || audioA.right != audioB.right);

    {
        HostInstance first(factory, info.cid);
        HostInstance second(factory, info.cid);
        auto* firstInterface = kitInterface(first);
        auto* secondInterface = kitInterface(second);
        check("simultaneous instance A import",
              status(importKit(*firstInterface, pathA), manage::Status::Success));
        check("simultaneous instance B import",
              status(importKit(*secondInterface, pathB), manage::Status::Success));
        lxr::state::ProjectCandidate firstState, secondState;
        require("decode simultaneous independent states",
                decodeProject(first.saveState(), firstState) &&
                decodeProject(second.saveState(), secondState));
        check("two live instances retain independent kits",
              firstState.patch[88] == 21u && secondState.patch[88] == 119u);
        firstInterface->release();
        secondInterface->release();
    }

    factory->release();
    require("shutdown exact private VST3", exitDll());
    FreeLibrary(module);
    std::printf("P17E VST3 SUMMARY pass=%d fail=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
