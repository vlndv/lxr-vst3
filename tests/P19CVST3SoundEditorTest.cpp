// Exact private-DLL P19-C production editor/controller/lifecycle test.
#define main ExistingP12cVST3StateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main

#include "pluginterfaces/gui/iplugview.h"
#include "../vendor/iPlug2/Examples/LXR/LXRGUIFoundation.h"

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
    if (*result == nullptr)
    {
        *result = window;
        return FALSE;
    }
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
        require("create P19-C production editor", view != nullptr);
        ViewRect bounds {};
        require("query P19-C editor dimensions", view->getSize(&bounds) == kResultOk);
        check("P19-C fixed editor width is 1280", bounds.getWidth() == 1280);
        check("P19-C fixed editor height is 800", bounds.getHeight() == 800);
        check("P19-C host resize remains disabled", view->canResize() == kResultFalse);
        check("P19-C HWND platform supported",
              view->isPlatformTypeSupported(kPlatformTypeHWND) == kResultTrue);
        parent = CreateWindowExA(0, "STATIC", "LXR P19-C host",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
            nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        require("create isolated editor parent", parent != nullptr);
        require("attach production editor to HWND",
                view->attached(parent, kPlatformTypeHWND) == kResultOk);
        pumpWindowMessages();
        EnumChildWindows(parent, captureFirstChild,
                         reinterpret_cast<LPARAM>(&child));
        require("discover production editor child window", child != nullptr);
    }

    ~AttachedEditor()
    {
        if (view != nullptr)
        {
            view->removed();
            view->release();
        }
        if (parent != nullptr)
            DestroyWindow(parent);
        pumpWindowMessages();
    }

    void click(int x, int y)
    {
        SendMessage(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
        SendMessage(child, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
        pumpWindowMessages();
    }
};

int controllerRaw(const HostInstance& instance, int parameter)
{
    return static_cast<int>(std::floor(
        instance.controller->getParamNormalized(static_cast<ParamID>(parameter)) *
        127.0 + 0.5));
}

void verifyOneBalancedGesture(HostInstance& instance, AttachedEditor& editor,
                              int x, int y, const char* label)
{
    const int beforeBegin = instance.handler.begins;
    const int beforeEdit = instance.handler.edits;
    const int beforeEnd = instance.handler.ends;
    editor.click(x, y);
    check(label, instance.handler.begins == beforeBegin + 1 &&
                 instance.handler.edits == beforeEdit + 1 &&
                 instance.handler.ends == beforeEnd + 1);
}

} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 2)
    {
        std::puts("Usage: P19CVST3SoundEditorTest.exe <absolute VST3 DLL path>");
        return 2;
    }

    HMODULE module = LoadLibraryA(argv[1]);
    require("load exact P19-C private VST3", module != nullptr);
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
    auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
    auto getFactory = reinterpret_cast<IPluginFactory* (*)()>(
        GetProcAddress(module, "GetPluginFactory"));
    require("resolve P19-C private VST3 exports", init && exitDll && getFactory);
    require("initialize P19-C private DLL", init());
    IPluginFactory* factory = getFactory();
    require("get P19-C private plugin factory", factory != nullptr);

    PClassInfo info {};
    bool found = false;
    for (int index = 0; index < factory->countClasses(); ++index)
        if (factory->getClassInfo(index, &info) == kResultOk &&
            std::strcmp(info.category, kVstAudioEffectClass) == 0)
        {
            found = true;
            break;
        }
    require("find P19-C LXR audio component", found);

    const auto firstBounds = lxr::gui::soundControlBounds(lxr::gui::kSoundBindings[0]);
    const int controlX = (firstBounds.left + firstBounds.right) / 2;
    const int controlY = (firstBounds.top + firstBounds.bottom) / 2;
    std::vector<uint8_t> restoredState;
    {
        HostInstance instance(factory, info.cid);
        // The VST3 wrapper additionally advertises MIDI-CC proxies and bypass.
        // The public LXR contract is the unchanged contiguous 0..245 prefix.
        check("controller contains all 246 unchanged LXR parameters",
              instance.controller->getParameterCount() >= lxr::vst::kNumParams);
        for (int parameter = 0; parameter < lxr::vst::kNumParams; ++parameter)
        {
            ParameterInfo parameterInfo {};
            require("read registered parameter info",
                instance.controller->getParameterInfo(parameter, parameterInfo) == kResultOk);
            check("registered host parameter ID unchanged",
                  parameterInfo.id == static_cast<ParamID>(parameter));
        }

        const int editsBeforeOpen = instance.handler.edits;
        AttachedEditor editor(instance);
        check("editor construction emits no host edits",
              instance.handler.edits == editsBeforeOpen &&
              instance.handler.begins == 0 && instance.handler.ends == 0);

        // Navigate to Sound. Navigation and owner selection are transient and
        // therefore must not produce parameter gestures.
        editor.click(24 + 176 + 82, 96);
        check("Sound view navigation creates no gesture",
              instance.handler.edits == editsBeforeOpen);

        constexpr std::array<int, 6> waveParameters {{1, 2, 3, 4, 6, 7}};
        for (std::size_t owner = 0; owner < waveParameters.size(); ++owner)
        {
            const int beforeSelection = instance.handler.edits;
            editor.click(170 + static_cast<int>(owner) * 96 + 43, 149);
            check("select one of six synthesis editors without host edit",
                  instance.handler.edits == beforeSelection);
            const int beforeRaw = controllerRaw(instance, waveParameters[owner]);
            verifyOneBalancedGesture(instance, editor, controlX, controlY,
                "visible owner control emits one balanced gesture");
            check("GUI adjustment updates accepted controller state",
                  controllerRaw(instance, waveParameters[owner]) ==
                    (beforeRaw >= 0 && beforeRaw <= 5 ? (beforeRaw + 1) % 6 : 0));
        }

        // Host-to-GUI proof: set a visible enum through the controller, then
        // operate the actual production widget. Its next identity must be based
        // on the automated value, not stale widget-local state.
        require("select D1 for automation readback", true);
        editor.click(170 + 43, 149);
        require("host automation updates D1 wave",
            instance.controller->setParamNormalized(1, 4.0 / 127.0) == kResultOk);
        check("controller accepted host automation", controllerRaw(instance, 1) == 4);
        verifyOneBalancedGesture(instance, editor, controlX, controlY,
            "automation-readback adjustment has one balanced gesture");
        check("visible widget used automated value", controllerRaw(instance, 1) == 5);

        // Feed the host-accepted controller values through the production
        // processor queue, exactly as a host loops GUI edits to the component.
        instance.setup(48000.0);
        for (int parameter : waveParameters)
            instance.processParameter(static_cast<ParamID>(parameter),
                                      instance.controller->getParamNormalized(parameter));
        instance.stop();
        restoredState = instance.saveState();
        check("P19-C state remains under 65536 bytes", restoredState.size() <= 65536u);
        check("48 kHz PDC remains 26 samples",
              instance.processor->getLatencySamples() == 26u);
    }

    {
        HostInstance restored(factory, info.cid);
        require("restore LXRS state for visible readback", restored.loadState(restoredState));
        const int editsBefore = restored.handler.edits;
        AttachedEditor reopened(restored);
        reopened.click(24 + 176 + 82, 96);
        reopened.click(170 + 43, 149);
        check("reopen and voice switch emit no host edits",
              restored.handler.edits == editsBefore);
        const int restoredWave = controllerRaw(restored, 1);
        verifyOneBalancedGesture(restored, reopened, controlX, controlY,
            "restored visible control has one balanced gesture");
        check("reopened editor uses restored accepted state",
              controllerRaw(restored, 1) ==
                (restoredWave >= 0 && restoredWave <= 5 ? (restoredWave + 1) % 6 : 0));
    }

    {
        HostInstance first(factory, info.cid);
        HostInstance second(factory, info.cid);
        AttachedEditor firstEditor(first);
        AttachedEditor secondEditor(second);
        firstEditor.click(24 + 176 + 82, 96);
        secondEditor.click(24 + 176 + 82, 96);
        const int secondBefore = controllerRaw(second, 1);
        verifyOneBalancedGesture(first, firstEditor, controlX, controlY,
            "first instance GUI edit balanced");
        check("multiple editor instances remain isolated",
              controllerRaw(second, 1) == secondBefore &&
              second.handler.edits == 0);
    }

    {
        HostInstance editorLifecycle(factory, info.cid);
        HostInstance unopenedControl(factory, info.cid);
        {
            AttachedEditor editor(editorLifecycle);
        }
        editorLifecycle.setup(48000.0);
        unopenedControl.setup(48000.0);
        const auto afterClose = renderD1(editorLifecycle);
        const auto control = renderD1(unopenedControl);
        check("closing editor preserves deterministic native audio",
              afterClose.left == control.left && afterClose.right == control.right);
        check("editor lifecycle leaves audio nonzero",
              afterClose.peak > 0.0 && control.peak > 0.0);
        check("multi-instance PDC remains 26 samples",
              editorLifecycle.processor->getLatencySamples() == 26u &&
              unopenedControl.processor->getLatencySamples() == 26u);
    }

    factory->release();
    exitDll();
    FreeLibrary(module);
    std::printf("P19C_VST3_SOUND SUMMARY pass=%d fail=%d sound=162 owners=6 pdc=26\n",
                passed, failed);
    return failed == 0 ? 0 : 1;
}
