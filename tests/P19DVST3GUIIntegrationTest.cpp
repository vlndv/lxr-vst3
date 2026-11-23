// Exact private-DLL P19-D production editor and management integration.
#define main ExistingP12cVST3StateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main

#include "../vendor/iPlug2/Examples/LXR/LXRKitManagementInterface.h"
#include "../vendor/iPlug2/Examples/LXR/LXRMorphManagementInterface.h"
#include "../vendor/iPlug2/Examples/LXR/LXRGUIFoundation.h"
#include "../dsp/KitManagement.h"
#include "../dsp/MorphManagement.h"

#include "pluginterfaces/gui/iplugview.h"

#include <cwchar>

namespace
{
void pumpWindowMessages()
{
    MSG message {};
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
}

BOOL CALLBACK captureFirstChild(HWND window, LPARAM data)
{
    auto* result = reinterpret_cast<HWND*>(data);
    if (*result == nullptr) { *result = window; return FALSE; }
    return TRUE;
}

struct AttachedEditor
{
    IPlugView* view = nullptr;
    HWND parent = nullptr;
    HWND child = nullptr;

    explicit AttachedEditor(HostInstance& instance)
    {
        view = instance.controller->createView(ViewType::kEditor);
        require("create P19-D production editor", view != nullptr);
        ViewRect bounds {};
        require("query P19-D dimensions", view->getSize(&bounds) == kResultOk);
        check("production width is 1280", bounds.getWidth() == 1280);
        check("production height is 800", bounds.getHeight() == 800);
        check("fixed production editor cannot resize", view->canResize() == kResultFalse);
        parent = CreateWindowExA(0, "STATIC", "LXR P19-D exact host",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
            nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        require("create isolated P19-D parent", parent != nullptr);
        require("attach exact P19-D editor",
                view->attached(parent, kPlatformTypeHWND) == kResultOk);
        pumpWindowMessages();
        EnumChildWindows(parent, captureFirstChild,
                         reinterpret_cast<LPARAM>(&child));
        require("discover exact production child HWND", child != nullptr);
    }

    ~AttachedEditor()
    {
        if (view != nullptr) { view->removed(); view->release(); }
        if (parent != nullptr) DestroyWindow(parent);
        pumpWindowMessages();
    }

    void click(int x, int y)
    {
        SendMessage(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
        SendMessage(child, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
        pumpWindowMessages();
    }
};

int raw(const HostInstance& instance, int parameter, int maximum)
{
    return static_cast<int>(std::floor(
        instance.controller->getParamNormalized(static_cast<ParamID>(parameter)) *
        static_cast<double>(maximum) + 0.5));
}

void balancedClick(HostInstance& instance, AttachedEditor& editor,
                   int x, int y, const char* label)
{
    const int begins = instance.handler.begins;
    const int edits = instance.handler.edits;
    const int ends = instance.handler.ends;
    editor.click(x, y);
    check(label, instance.handler.begins == begins + 1 &&
                 instance.handler.edits == edits + 1 &&
                 instance.handler.ends == ends + 1);
}

std::wstring widen(const char* text)
{
    std::wstring result;
    while (text != nullptr && *text != 0)
        result.push_back(static_cast<unsigned char>(*text++));
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
        static_cast<DWORD>(bytes.size()), &transferred, nullptr) &&
        transferred == bytes.size() && FlushFileBuffers(file);
    return CloseHandle(file) && ok;
}

lxr::vst::IKitManagement* kitInterface(HostInstance& instance)
{
    lxr::vst::IKitManagement* api = nullptr;
    require("query exact P17 kit operation interface",
        instance.component->queryInterface(INLINE_UID_OF(lxr::vst::IKitManagement),
            reinterpret_cast<void**>(&api)) == kResultOk && api != nullptr);
    return api;
}

lxr::vst::IMorphManagement* morphInterface(HostInstance& instance)
{
    lxr::vst::IMorphManagement* api = nullptr;
    require("query exact P18 morph operation interface",
        instance.component->queryInterface(INLINE_UID_OF(lxr::vst::IMorphManagement),
            reinterpret_cast<void**>(&api)) == kResultOk && api != nullptr);
    return api;
}

bool decode(const std::vector<std::uint8_t>& state,
            lxr::state::ProjectCandidate& candidate)
{
    return lxr::state::DecodeProject(state.data(), state.size(), candidate, true);
}

std::vector<std::uint8_t> sndFixture(
    const lxr::state::ProjectCandidate& state,
    const std::array<std::uint8_t, 8>& name, std::uint8_t volume)
{
    std::vector<std::uint8_t> bytes(236u);
    std::copy(name.begin(), name.end(), bytes.begin());
    std::copy(state.patch.begin(), state.patch.end(), bytes.begin() + 8u);
    bytes[8u + 88u] = volume;
    return bytes;
}
}

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 3)
    {
        std::puts("Usage: P19DVST3GUIIntegrationTest.exe <VST3 DLL> <temp dir>");
        return 2;
    }
    HMODULE module = LoadLibraryA(argv[1]);
    require("load exact P19-D private DLL", module != nullptr);
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
    auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
    auto factoryFn = reinterpret_cast<IPluginFactory* (*)()>(
        GetProcAddress(module, "GetPluginFactory"));
    require("resolve exact P19-D exports", init && exitDll && factoryFn);
    require("initialize exact P19-D DLL", init());
    auto* factory = factoryFn();
    require("get exact P19-D factory", factory != nullptr);
    PClassInfo info {};
    bool found = false;
    for (int i = 0; i < factory->countClasses(); ++i)
        if (factory->getClassInfo(i, &info) == kResultOk &&
            std::strcmp(info.category, kVstAudioEffectClass) == 0)
        { found = true; break; }
    require("find exact LXR component", found);

    const auto kitPath = child(argv[2], L"p19d-kit.snd");
    const auto targetPath = child(argv[2], L"p19d-target.snd");
    const auto kitExportPath = child(argv[2], L"p19d-export.snd");
    const auto morphExportPath = child(argv[2], L"p19d-morph-export.snd");
    std::vector<std::uint8_t> restoredState;
    {
        HostInstance instance(factory, info.cid);
        check("unchanged contiguous 0..245 host contract",
              instance.controller->getParameterCount() >= lxr::vst::kNumParams);
        const int editsBefore = instance.handler.edits;
        AttachedEditor editor(instance);
        check("editor open emits no gestures", instance.handler.edits == editsBefore);

        editor.click(24 + 2 * 176 + 82, 96);
        check("Modulation view navigation emits no edit",
              instance.handler.edits == editsBefore);
        const auto waveBounds = lxr::gui::modulationControlBounds(
            lxr::gui::ModulationRole::LfoWaveform);
        const auto velocityBounds = lxr::gui::modulationControlBounds(
            lxr::gui::ModulationRole::VelocityDestination);
        const auto lfoRouteBounds = lxr::gui::modulationControlBounds(
            lxr::gui::ModulationRole::LfoDestination);
        for (int owner = 0; owner < 6; ++owner)
        {
            const int before = instance.handler.edits;
            editor.click(170 + owner * 96 + 43, 149);
            check("one of six modulation lanes resolves without edit",
                  instance.handler.edits == before);
            const int parameter = 155 + owner;
            const int prior = raw(instance, parameter, 127);
            balancedClick(instance, editor,
                (waveBounds.left + waveBounds.right) / 2,
                (waveBounds.top + waveBounds.bottom) / 2,
                "LFO waveform emits one balanced gesture");
            check("LFO waveform preserves stored identity",
                  raw(instance, parameter, 127) ==
                    (prior >= 0 && prior <= 7 ? (prior + 1) % 8 : 0));
        }

        editor.click(170 + 43, 149);
        balancedClick(instance, editor,
            (velocityBounds.left + velocityBounds.right) / 2,
            (velocityBounds.top + velocityBounds.bottom) / 2,
            "velocity destination uses one balanced gesture");
        check("D1 velocity route selects original identity 2",
              raw(instance, 228, 65535) == 2);
        balancedClick(instance, editor,
            (lfoRouteBounds.left + lfoRouteBounds.right) / 2,
            (lfoRouteBounds.top + lfoRouteBounds.bottom) / 2,
            "LFO destination uses one balanced gesture");
        check("D1 LFO route selects original identity 2",
              raw(instance, 234, 65535) == 2);

        require("external automation sets eighth waveform identity",
            instance.controller->setParamNormalized(155, 7.0 / 127.0) == kResultOk);
        balancedClick(instance, editor,
            (waveBounds.left + waveBounds.right) / 2,
            (waveBounds.top + waveBounds.bottom) / 2,
            "automated LFO readback keeps balanced gesture");
        check("visible LFO used externally automated identity",
              raw(instance, 155, 127) == 0);

        editor.click(170 + 5 * 96 + 43, 149);
        require("external host selects valid high identity 203",
            instance.controller->setParamNormalized(239, 203.0 / 65535.0) == kResultOk);
        check("full-width high identity survives controller readback",
              raw(instance, 239, 65535) == 203);
        instance.processParameter(239, 203.0 / 65535.0);
        lxr::state::ProjectCandidate acceptedRoute;
        require("decode accepted high route",
                decode(instance.saveState(), acceptedRoute));
        check("routing authority accepts identity 203",
              acceptedRoute.routing.routes[11].originalIndex == 203u);
        instance.processParameter(239, 204.0 / 65535.0);
        lxr::state::ProjectCandidate rejectedRoute;
        require("decode state after invalid route",
                decode(instance.saveState(), rejectedRoute));
        check("identity 204 is rejected and prior route retained",
              rejectedRoute.routing.routes[11].originalIndex == 203u);
        balancedClick(instance, editor,
            (lfoRouteBounds.left + lfoRouteBounds.right) / 2,
            (lfoRouteBounds.top + lfoRouteBounds.bottom) / 2,
            "high route adjustment is one balanced gesture");
        check("highest route wraps to OFF by identity, not menu index",
              raw(instance, 239, 65535) == 0);

        const int editsBeforeKits = instance.handler.edits;
        editor.click(24 + 4 * 176 + 82, 96);
        check("Kits plus Morph view navigation emits no edit",
              instance.handler.edits == editsBeforeKits);
        balancedClick(instance, editor, 725, 440,
                      "Morph Amount 245 remains host-bound");

        lxr::state::ProjectCandidate source;
        require("decode exact pre-operation project", decode(instance.saveState(), source));
        const auto kitBytes = sndFixture(source,
            {{'P','1','9','D',0x01u,0xffu,0,0}}, 21u);
        const auto targetBytes = sndFixture(source,
            {{'M','O','R','P','H',0,0,0}}, 103u);
        require("write exact kit fixture", writeNew(kitPath, kitBytes));
        require("write exact morph fixture", writeNew(targetPath, targetBytes));

        auto* kits = kitInterface(instance);
        lxr::vst::KitOperationWire kitResult;
        require("Kit Import invokes production operation",
            kits->importSnd(reinterpret_cast<const Steinberg::Vst::TChar*>(kitPath.data()),
                static_cast<Steinberg::uint32>(kitPath.size()), &kitResult) == kResultOk);
        check("Kit Import typed status is success",
              kitResult.status == static_cast<Steinberg::uint32>(
                lxr::kit::management::Status::Success));
        require("Kit Export invokes production operation",
            kits->exportSnd(reinterpret_cast<const Steinberg::Vst::TChar*>(kitExportPath.data()),
                static_cast<Steinberg::uint32>(kitExportPath.size()), &kitResult) == kResultOk);
        check("Kit Export typed status is success",
              kitResult.status == static_cast<Steinberg::uint32>(
                lxr::kit::management::Status::Success));

        auto* morph = morphInterface(instance);
        lxr::vst::MorphOperationWire morphResult;
        require("Morph Target load invokes production operation",
            morph->importMorphSnd(
                reinterpret_cast<const Steinberg::Vst::TChar*>(targetPath.data()),
                static_cast<Steinberg::uint32>(targetPath.size()), &morphResult) == kResultOk);
        check("loaded target status and presence are coherent",
              morphResult.status == static_cast<Steinberg::uint32>(
                lxr::morph::management::Status::Success) && morphResult.targetPresent != 0u);
        instance.processParameter(245, 255.0 / 255.0);
        require("read production morph state", morph->getMorphState(&morphResult) == kResultOk);
        check("Morph Amount 245 is authoritative", morphResult.amount == 255u);
        require("MorphSnd export invokes production operation",
            morph->exportMorphSnd(
                reinterpret_cast<const Steinberg::Vst::TChar*>(morphExportPath.data()),
                static_cast<Steinberg::uint32>(morphExportPath.size()), &morphResult) == kResultOk);
        check("MorphSnd typed status is success",
              morphResult.status == static_cast<Steinberg::uint32>(
                lxr::morph::management::Status::Success));
        require("Morph target clear invokes production operation",
                morph->clearMorphTarget(&morphResult) == kResultOk);
        check("clear removes target but retains amount",
              morphResult.targetPresent == 0u && morphResult.amount == 255u);
        kits->release();
        morph->release();

        restoredState = instance.saveState();
        check("LXRS remains within 65536 bytes", restoredState.size() <= 65536u);
        instance.setup(48000.0);
        check("PDC remains 26 at 48 kHz",
              instance.processor->getLatencySamples() == 26u);
        instance.stop();
    }

    {
        HostInstance restored(factory, info.cid);
        require("restore exact P19-D project", restored.loadState(restoredState));
        auto* morph = morphInterface(restored);
        lxr::vst::MorphOperationWire state;
        require("query morph state after restore", morph->getMorphState(&state) == kResultOk);
        check("project restore does not fabricate target", state.targetPresent == 0u);
        restored.processParameter(245, 127.0 / 255.0);
        require("query host-restored amount", morph->getMorphState(&state) == kResultOk);
        check("host-restored amount does not fabricate target",
              state.amount == 127u && state.targetPresent == 0u);
        const int edits = restored.handler.edits;
        AttachedEditor reopened(restored);
        reopened.click(24 + 2 * 176 + 82, 96);
        check("reopen reconstructs without unsolicited gestures",
              restored.handler.edits == edits);
        morph->release();
    }

    {
        HostInstance first(factory, info.cid);
        HostInstance second(factory, info.cid);
        AttachedEditor firstEditor(first);
        AttachedEditor secondEditor(second);
        firstEditor.click(24 + 2 * 176 + 82, 96);
        secondEditor.click(24 + 2 * 176 + 82, 96);
        const auto bounds = lxr::gui::modulationControlBounds(
            lxr::gui::ModulationRole::LfoWaveform);
        balancedClick(first, firstEditor, (bounds.left + bounds.right) / 2,
                      (bounds.top + bounds.bottom) / 2,
                      "first instance modulation edit is balanced");
        check("multiple P19-D editors remain isolated",
              second.handler.edits == 0 && raw(second, 155, 127) == 0);
    }

    {
        HostInstance lifecycle(factory, info.cid);
        HostInstance unopened(factory, info.cid);
        { AttachedEditor editor(lifecycle); }
        lifecycle.setup(48000.0);
        unopened.setup(48000.0);
        const auto afterClose = renderD1(lifecycle);
        const auto control = renderD1(unopened);
        check("editor closure preserves exact audio",
              afterClose.left == control.left && afterClose.right == control.right);
        check("editor closure leaves audio active",
              afterClose.peak > 0.0 && control.peak > 0.0);
    }

    factory->release();
    exitDll();
    FreeLibrary(module);
    std::printf("P19D_VST3_GUI SUMMARY pass=%d fail=%d modulation=54 owners=6 pdc=26\n",
                passed, failed);
    return failed == 0 ? 0 : 1;
}
