// Exact private-DLL P19-E production graphical-sequencer integration.
#define main ExistingP12cVST3StateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main

#include "../vendor/iPlug2/Examples/LXR/LXRGUIFoundation.h"
#include "../vendor/iPlug2/Examples/LXR/LXRProjectState.h"
#include "pluginterfaces/gui/iplugview.h"

namespace
{
void pumpWindowMessages()
{
    MSG message {};
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
    { TranslateMessage(&message); DispatchMessage(&message); }
}

BOOL CALLBACK captureFirstChild(HWND window, LPARAM data)
{
    auto* result = reinterpret_cast<HWND*>(data);
    if (*result == nullptr) { *result = window; return FALSE; }
    return TRUE;
}

struct AttachedEditor final
{
    IPlugView* view = nullptr;
    HWND parent = nullptr;
    HWND child = nullptr;
    explicit AttachedEditor(HostInstance& instance)
    {
        view = instance.controller->createView(ViewType::kEditor);
        require("create P19-E production editor", view != nullptr);
        ViewRect bounds {};
        require("query P19-E editor size", view->getSize(&bounds) == kResultOk);
        check("P19-E width is 1280", bounds.getWidth() == 1280);
        check("P19-E height is 800", bounds.getHeight() == 800);
        check("P19-E editor remains fixed", view->canResize() == kResultFalse);
        parent = CreateWindowExA(0, "STATIC", "LXR P19-E exact host",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
            nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        require("create P19-E parent", parent != nullptr);
        require("attach P19-E editor", view->attached(parent, kPlatformTypeHWND) == kResultOk);
        pumpWindowMessages();
        EnumChildWindows(parent, captureFirstChild, reinterpret_cast<LPARAM>(&child));
        require("discover P19-E child", child != nullptr);
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
    void press(int x, int y, bool down)
    {
        SendMessage(child, down ? WM_LBUTTONDOWN : WM_LBUTTONUP,
                    down ? MK_LBUTTON : 0, MAKELPARAM(x, y));
        pumpWindowMessages();
    }
};

bool decode(const std::vector<std::uint8_t>& state,
            lxr::state::ProjectCandidate& candidate)
{
    return lxr::state::DecodeProject(state.data(), state.size(), candidate, true);
}

std::uint8_t raw(const HostInstance& instance, int parameter, int maximum)
{
    return static_cast<std::uint8_t>(std::floor(
        instance.controller->getParamNormalized(static_cast<ParamID>(parameter)) *
        maximum + 0.5));
}
}

#ifndef LXR_P19E_EDITOR_TEST_HELPERS_ONLY
int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 2)
    {
        std::puts("Usage: P19EVST3SequencerGUITest.exe <VST3 DLL>");
        return 2;
    }
    HMODULE module = LoadLibraryA(argv[1]);
    require("load exact P19-E DLL", module != nullptr);
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
    auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
    auto factoryFn = reinterpret_cast<IPluginFactory* (*)()>(
        GetProcAddress(module, "GetPluginFactory"));
    require("resolve P19-E exports", init && exitDll && factoryFn);
    require("initialize P19-E DLL", init());
    auto* factory = factoryFn();
    require("get P19-E factory", factory != nullptr);
    PClassInfo info {};
    bool found = false;
    for (int i = 0; i < factory->countClasses(); ++i)
        if (factory->getClassInfo(i, &info) == kResultOk &&
            std::strcmp(info.category, kVstAudioEffectClass) == 0)
        { found = true; break; }
    require("find exact P19-E component", found);

    std::vector<std::uint8_t> saved;
    {
        HostInstance instance(factory, info.cid);
        check("unchanged 246 host parameter prefix",
              instance.controller->getParameterCount() >= 246);
        const int editsAtOpen = instance.handler.edits;
        AttachedEditor editor(instance);
        check("editor open creates no automation", instance.handler.edits == editsAtOpen);
        editor.click(24 + 3 * 176 + 82, 96); // Sequencer navigation.
        check("sequencer navigation is non-automating", instance.handler.edits == editsAtOpen);

        // Track 0 / main step 0: gate on, physical substep 4 on, gate off.
        editor.click(148, 269);
        editor.click(1206, 272);
        editor.click(148, 269);
        lxr::state::ProjectCandidate candidate;
        require("decode exact GUI edit", decode(instance.saveState(), candidate));
        check("main gate can be turned off", (candidate.bank.patterns[0].mainGates[0] & 1u) == 0u);
        check("gate off retains substep four",
              lxr::sequencer::IsStepActive(candidate.bank.patterns[0].steps[0][3]));

        // Select track 1 / step 2 and edit its selected boundary record.
        editor.click(126 + 2 * 48 + 22, 250 + 45 + 19);
        editor.click(1035, 270); // explicitly select physical substep zero
        editor.click(1072, 382); // note +1
        editor.click(1152, 382); // velocity +1
        editor.click(1170, 382); // probability -1
        editor.click(1008, 430); // first safe lock target below sentinel
        editor.click(1185, 430); // lock value +1
        editor.click(1008, 550); // length 16 -> 15
        editor.click(1117, 550); // rotation 0 -> 1
        require("decode step detail edit", decode(instance.saveState(), candidate));
        const auto& edited = candidate.bank.patterns[0].steps[1][16];
        check("selected step note edited exactly", edited.note == 64u);
        check("selected step velocity edited exactly",
              lxr::sequencer::StepVelocity(edited) == 101u);
        check("selected step probability edited exactly", edited.probability == 126u);
        check("lock target uses supported original selector",
              static_cast<bool>(lxr::sequencer::DecodeAutomationSlot(
                  edited.parameter1, edited.value1)));
        check("lock value edited", edited.value1 == 1u);
        check("track length edited", candidate.bank.patterns[0].lengthCodes[1] == 15u);
        check("track rotation edited", candidate.bank.patterns[0].rotations[1] == 1u);

        editor.click(1055, 604); // shuffle +1
        editor.click(1062, 655); // source Euclidean generator
        require("decode management edit", decode(instance.saveState(), candidate));
        check("shuffle enters persistent bank", candidate.bank.shuffle == 1u);
        check("Euclidean sets source length", candidate.bank.patterns[0].lengthCodes[1] == 0u);

        // Copy selected pattern 1 to explicit destination pattern 2.
        editor.click(160, 604);
        require("decode pattern copy", decode(instance.saveState(), candidate));
        check("pattern copy reaches destination authority",
              candidate.bank.patterns[1].mainGates[1] ==
              candidate.bank.patterns[0].mainGates[1]);

        // Select pattern two; stopped selection is persisted as configured start.
        editor.click(112, 210);
        require("decode selected pattern", decode(instance.saveState(), candidate));
        check("selected pattern persists through transport module",
              candidate.transport.pattern == 1u);

        // Mute is transient and roll pads use the existing automatable mask.
        editor.click(926, 314);
        const int begins = instance.handler.begins;
        const int ends = instance.handler.ends;
        editor.press(970, 314, true);
        check("roll press updates existing host parameter",
              raw(instance, lxr::vst::kSequencerRollTracksParam, 127) == 2u);
        editor.press(970, 314, false);
        check("roll release clears existing host parameter",
              raw(instance, lxr::vst::kSequencerRollTracksParam, 127) == 0u);
        const int beginDelta = instance.handler.begins - begins;
        const int endDelta = instance.handler.ends - ends;
        std::printf("ROLL_GESTURES begin=%d end=%d\n", beginDelta, endDelta);
        check("roll press/release emits balanced gestures",
              beginDelta > 0 && beginDelta == endDelta);
        saved = instance.saveState();
    }

    {
        HostInstance restored(factory, info.cid);
        require("restore P19-E project", restored.loadState(saved));
        lxr::state::ProjectCandidate candidate;
        require("decode reopened project", decode(restored.saveState(), candidate));
        check("reopen preserves selected pattern", candidate.transport.pattern == 1u);
        check("reopen preserves substep while main gate is off",
              lxr::sequencer::IsStepActive(candidate.bank.patterns[0].steps[0][3]) &&
              (candidate.bank.patterns[0].mainGates[0] & 1u) == 0u);
        check("reopen does not serialize held roll", raw(restored,
              lxr::vst::kSequencerRollTracksParam, 127) == 0u);
        AttachedEditor editor(restored);
        editor.click(24 + 3 * 176 + 82, 96);
        check("reopened editor does not mutate accepted state",
              restored.saveState() == saved);
    }

    {
        HostInstance first(factory, info.cid);
        HostInstance second(factory, info.cid);
        AttachedEditor firstEditor(first);
        firstEditor.click(24 + 3 * 176 + 82, 96);
        firstEditor.click(148, 269);
        lxr::state::ProjectCandidate a, b;
        require("decode first instance", decode(first.saveState(), a));
        require("decode second instance", decode(second.saveState(), b));
        check("two pattern authorities are independent",
              a.bank.patterns[0].mainGates[0] != b.bank.patterns[0].mainGates[0]);
    }

    factory->release();
    require("exit P19-E DLL", exitDll());
    FreeLibrary(module);
    std::printf("P19E_VST3_SEQUENCER_GUI SUMMARY pass=%d fail=%d tracks=7 patterns=8 pdc=26\n",
                passed, failed);
    return failed == 0 ? 0 : 1;
}
#endif
