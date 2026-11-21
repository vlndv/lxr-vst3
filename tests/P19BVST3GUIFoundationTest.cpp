// Exact private-DLL editor lifecycle and fixed-size contract. The reusable
// offline host remains independent of the production GUI implementation.
#define main ExistingP12cVST3StateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main

#include "pluginterfaces/gui/iplugview.h"

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 2)
    {
        std::puts("Usage: P19BVST3GUIFoundationTest.exe <absolute VST3 DLL path>");
        return 2;
    }

    HMODULE module = LoadLibraryA(argv[1]);
    require("load exact private VST3", module != nullptr);
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
    auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
    auto getFactory = reinterpret_cast<IPluginFactory* (*)()>(
        GetProcAddress(module, "GetPluginFactory"));
    require("resolve private VST3 exports", init && exitDll && getFactory);
    require("initialize private DLL", init());
    IPluginFactory* factory = getFactory();
    require("get private plugin factory", factory != nullptr);

    PClassInfo info {};
    bool found = false;
    for (int index = 0; index < factory->countClasses(); ++index)
    {
        if (factory->getClassInfo(index, &info) == kResultOk &&
            std::strcmp(info.category, kVstAudioEffectClass) == 0)
        {
            found = true;
            break;
        }
    }
    require("find LXR audio component", found);

    {
        HostInstance editorInstance(factory, info.cid);
        HostInstance controlInstance(factory, info.cid);
        const int editsBefore = editorInstance.handler.edits;
        IPlugView* view = editorInstance.controller->createView(ViewType::kEditor);
        require("create production editor view", view != nullptr);
        ViewRect bounds {};
        require("query editor dimensions", view->getSize(&bounds) == kResultOk);
        check("fixed editor width is 1280", bounds.getWidth() == 1280);
        check("fixed editor height is 800", bounds.getHeight() == 800);
        check("host resize remains disabled", view->canResize() == kResultFalse);
        // The pinned wrapper's advisory checkSizeConstraint() returns success
        // even for non-resizable views. VST3 hosts must honor canResize(); do
        // not treat that advisory return as proof of enabled host resizing.
        view->release();
        check("editor lifecycle emitted no host automation",
              editorInstance.handler.edits == editsBefore);

        editorInstance.setup(48000.0);
        controlInstance.setup(48000.0);
        const auto afterEditorClose = renderD1(editorInstance);
        const auto unopenedControl = renderD1(controlInstance);
        check("editor close preserves bit-identical audio",
              afterEditorClose.left == unopenedControl.left &&
              afterEditorClose.right == unopenedControl.right);
        check("editor and control instances remain audible",
              afterEditorClose.peak > 0.0 && unopenedControl.peak > 0.0);
        check("48 kHz PDC remains 26 samples",
              editorInstance.processor->getLatencySamples() == 26u &&
              controlInstance.processor->getLatencySamples() == 26u);
    }

    factory->release();
    exitDll();
    FreeLibrary(module);
    std::printf("P19B_VST3_GUI SUMMARY pass=%d fail=%d width=1280 height=800 pdc=26\n",
                passed, failed);
    return failed == 0 ? 0 : 1;
}
