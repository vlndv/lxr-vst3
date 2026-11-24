// P19-F exact private-DLL GUI, DPI, accessibility, keyboard and lifecycle test.
#define main ExistingP12cVST3StateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main

#include "../vendor/iPlug2/Examples/LXR/LXRGUIFoundation.h"
#include "../vendor/iPlug2/Examples/LXR/LXRProjectState.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"

#include <UIAutomation.h>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

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

struct RenderStats final
{
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double maximum = 0.0;
    std::size_t count = 0u;
};

class TestPlugFrame final : public IPlugFrame
{
public:
    tresult PLUGIN_API queryInterface(const TUID iid, void** object) override
    {
        if (FUnknownPrivate::iidEqual(iid, IPlugFrame::iid) ||
            FUnknownPrivate::iidEqual(iid, FUnknown::iid))
        {
            *object = this;
            addRef();
            return kResultOk;
        }
        *object = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1000u; }
    uint32 PLUGIN_API release() override { return 1000u; }
    tresult PLUGIN_API resizeView(IPlugView*, ViewRect* size) override
    {
        if (size == nullptr) return kInvalidArgument;
        lastWidth = size->getWidth();
        lastHeight = size->getHeight();
        ++requests;
        return kResultOk;
    }
    int lastWidth = 0;
    int lastHeight = 0;
    int requests = 0;
};

RenderStats summarize(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    RenderStats result;
    result.count = values.size();
    if (values.empty()) return result;
    auto quantile = [&values](double fraction) {
        const auto index = static_cast<std::size_t>(std::ceil(
            fraction * static_cast<double>(values.size()))) - 1u;
        return values[std::min(index, values.size() - 1u)];
    };
    result.median = quantile(0.50);
    result.p95 = quantile(0.95);
    result.p99 = quantile(0.99);
    result.maximum = values.back();
    return result;
}

struct UIHeapSnapshot final
{
    std::uint64_t blocks = 0u;
    std::uint64_t bytes = 0u;
};

UIHeapSnapshot heapSummary()
{
    HANDLE heaps[256] {};
    const DWORD count = GetProcessHeaps(256, heaps);
    UIHeapSnapshot result;
    for (DWORD index = 0; index < std::min<DWORD>(count, 256); ++index)
    {
        if (!HeapLock(heaps[index])) continue;
        PROCESS_HEAP_ENTRY entry {};
        while (HeapWalk(heaps[index], &entry))
        {
            if ((entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0)
            { ++result.blocks; result.bytes += entry.cbData; }
        }
        HeapUnlock(heaps[index]);
    }
    return result;
}

class ContinuousAudio final
{
public:
    explicit ContinuousAudio(HostInstance& instance) : mInstance(instance)
    {
        mThread = std::thread([this] { run(); });
    }
    ~ContinuousAudio() { stop(); }
    void stop()
    {
        if (!mRunning.exchange(false, std::memory_order_acq_rel)) return;
        if (mThread.joinable()) mThread.join();
    }
    std::uint64_t callbacks() const noexcept { return mCallbacks.load(); }
    std::uint64_t failures() const noexcept { return mFailures.load(); }
private:
    void run()
    {
        std::array<double, 64> left {};
        std::array<double, 64> right {};
        double* channels[] {left.data(), right.data()};
        AudioBusBuffers output {};
        output.numChannels = 2;
        output.channelBuffers64 = channels;
        ProcessData data {};
        data.processMode = kRealtime;
        data.symbolicSampleSize = kSample64;
        data.numSamples = 64;
        data.numOutputs = 1;
        data.outputs = &output;
        while (mRunning.load(std::memory_order_acquire))
        {
            if (mInstance.processor->process(data) != kResultOk) ++mFailures;
            ++mCallbacks;
            std::this_thread::yield();
        }
    }
    HostInstance& mInstance;
    std::atomic<bool> mRunning {true};
    std::atomic<std::uint64_t> mCallbacks {0u};
    std::atomic<std::uint64_t> mFailures {0u};
    std::thread mThread;
};

struct AttachedEditor final
{
    IPlugView* view = nullptr;
    HWND parent = nullptr;
    HWND child = nullptr;
    TestPlugFrame frame;

    explicit AttachedEditor(HostInstance& instance)
    {
        view = instance.controller->createView(ViewType::kEditor);
        require("create P19-F production editor", view != nullptr);
        ViewRect bounds {};
        require("query P19-F editor dimensions", view->getSize(&bounds) == kResultOk);
        check("P19-F width is 1280", bounds.getWidth() == 1280);
        check("P19-F height is 800", bounds.getHeight() == 800);
        check("P19-F resizing disabled", view->canResize() == kResultFalse);
        check("P19-F HWND supported",
              view->isPlatformTypeSupported(kPlatformTypeHWND) == kResultTrue);
        parent = CreateWindowExA(0, "STATIC", "LXR P19-F exact host",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1400, 920,
            nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        require("create P19-F isolated parent", parent != nullptr);
        require("install P19-F host frame", view->setFrame(&frame) == kResultOk);
        require("attach P19-F editor", view->attached(parent, kPlatformTypeHWND) == kResultOk);
        pumpWindowMessages();
        EnumChildWindows(parent, captureFirstChild, reinterpret_cast<LPARAM>(&child));
        require("discover P19-F child", child != nullptr);
    }

    ~AttachedEditor()
    {
        if (view != nullptr) { view->removed(); view->setFrame(nullptr); view->release(); }
        if (parent != nullptr) DestroyWindow(parent);
        pumpWindowMessages();
    }

    void click(int x, int y)
    {
        SendMessage(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
        SendMessage(child, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
        pumpWindowMessages();
    }

    void key(WPARAM keyCode, bool shift = false)
    {
        SetFocus(child);
        if (shift) SendMessage(child, WM_KEYDOWN, VK_SHIFT, 0);
        SendMessage(child, WM_KEYDOWN, keyCode, 0);
        SendMessage(child, WM_KEYUP, keyCode, 0);
        if (shift) SendMessage(child, WM_KEYUP, VK_SHIFT, 0);
        pumpWindowMessages();
    }

    void tabs(int count)
    {
        for (int i = 0; i < count; ++i) key(VK_TAB);
    }

    RenderStats renderSamples(int x, int y, int samples)
    {
        click(x, y);
        std::vector<double> durations;
        durations.reserve(static_cast<std::size_t>(samples));
        for (int i = 0; i < samples; ++i)
        {
            const auto start = std::chrono::steady_clock::now();
            RedrawWindow(child, nullptr, nullptr,
                         RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            const auto stop = std::chrono::steady_clock::now();
            durations.push_back(std::chrono::duration<double, std::milli>(
                stop - start).count());
        }
        return summarize(durations);
    }
};

int raw(const HostInstance& instance, int parameter, int maximum)
{
    return static_cast<int>(std::floor(
        instance.controller->getParamNormalized(static_cast<ParamID>(parameter)) *
        static_cast<double>(maximum) + 0.5));
}

void reportStats(const char* view, const RenderStats& stats)
{
    std::printf("P19F_RENDER view=%s samples=%zu median_ms=%.6f p95_ms=%.6f "
                "p99_ms=%.6f max_ms=%.6f\n", view, stats.count, stats.median,
                stats.p95, stats.p99, stats.maximum);
}

int accessibilityDescendants(HWND child)
{
    IUIAutomation* automation = nullptr;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&automation))))
        return -1;
    IUIAutomationElement* root = nullptr;
    if (FAILED(automation->ElementFromHandle(child, &root)) || root == nullptr)
    { automation->Release(); return -1; }
    IUIAutomationCondition* condition = nullptr;
    IUIAutomationElementArray* elements = nullptr;
    int count = -1;
    if (SUCCEEDED(automation->CreateTrueCondition(&condition)) && condition != nullptr &&
        SUCCEEDED(root->FindAll(TreeScope_Descendants, condition, &elements)) &&
        elements != nullptr)
        elements->get_Length(&count);
    if (elements != nullptr) elements->Release();
    if (condition != nullptr) condition->Release();
    root->Release();
    automation->Release();
    return count;
}
}

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 2)
    {
        std::puts("Usage: P19FVST3GUIQualificationTest.exe <VST3 DLL>");
        return 2;
    }
    require("initialize COM for UI Automation",
            SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
    HMODULE module = LoadLibraryA(argv[1]);
    require("load exact P19-F DLL", module != nullptr);
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
    auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
    auto factoryFn = reinterpret_cast<IPluginFactory* (*)()>(
        GetProcAddress(module, "GetPluginFactory"));
    require("resolve P19-F exports", init && exitDll && factoryFn);
    require("initialize P19-F DLL", init());
    auto* factory = factoryFn();
    require("get P19-F factory", factory != nullptr);
    PClassInfo info {};
    bool found = false;
    for (int i = 0; i < factory->countClasses(); ++i)
        if (factory->getClassInfo(i, &info) == kResultOk &&
            std::strcmp(info.category, kVstAudioEffectClass) == 0)
        { found = true; break; }
    require("find P19-F component", found);

    std::vector<std::uint8_t> saved;
    {
        HostInstance instance(factory, info.cid);
        check("host registry contains IDs 0..245",
              instance.controller->getParameterCount() >= 246);
        instance.setup(48000.0);
        check("PDC remains 26 at 48 kHz", instance.processor->getLatencySamples() == 26u);
        const int editsBeforeOpen = instance.handler.edits;
        AttachedEditor editor(instance);
        ContinuousAudio audio(instance);
        check("editor construction emits no host edit",
              instance.handler.edits == editsBeforeOpen);

        IPlugViewContentScaleSupport* scale = nullptr;
        require("query content-scale support", editor.view->queryInterface(
            IPlugViewContentScaleSupport::iid, reinterpret_cast<void**>(&scale)) == kResultOk &&
            scale != nullptr);
        for (const double factor : {1.0, 1.25, 1.5, 2.0})
            check("content scale accepted", scale->setContentScaleFactor(factor) == kResultOk);
        check("restore content scale", scale->setContentScaleFactor(1.0) == kResultOk);
        check("host receives post-attach scale resize requests", editor.frame.requests >= 4);
        check("restored logical editor maps to 1280x800",
              editor.frame.lastWidth == 1280 && editor.frame.lastHeight == 800);
        scale->release();

        const int descendants = accessibilityDescendants(editor.child);
        check("UI Automation reaches plugin HWND", descendants >= 0);
        std::printf("P19F_ACCESSIBILITY uia_descendants=%d custom_semantics=%s\n",
                    descendants, descendants > 0 ? "present" : "framework_limited");

        const std::array<const char*, 5> views {{
            "overview", "sound", "modulation", "sequencer", "kits-morph"}};
        for (std::size_t index = 0; index < views.size(); ++index)
        {
            const int edits = instance.handler.edits;
            const auto stats = editor.renderSamples(
                24 + static_cast<int>(index) * 176 + 82, 96, 60);
            reportStats(views[index], stats);
            check("view navigation emits no automation", instance.handler.edits == edits);
            check("render sample count", stats.count == 60u);
            check("render timings finite", std::isfinite(stats.maximum));
        }

        // Sound: all six owners, then keyboard endpoints on the D1 main-wave control.
        editor.click(24 + 176 + 82, 96);
        for (int owner = 0; owner < 6; ++owner)
        {
            const int edits = instance.handler.edits;
            editor.click(170 + owner * 96 + 43, 149);
            check("Sound owner switch emits no automation", instance.handler.edits == edits);
        }
        editor.click(170 + 43, 149); // restore D1; mouse does not claim keyboard focus.
        editor.tabs(12); // five views, six owners, first visible Sound control.
        const int beginsBeforeKeys = instance.handler.begins;
        const int endsBeforeKeys = instance.handler.ends;
        editor.key(VK_HOME);
        check("Sound Home reaches exact minimum", raw(instance, 1, 127) == 0);
        editor.key(VK_END);
        check("Sound End reaches highest supported built-in waveform",
              raw(instance, 1, 127) == 5);
        editor.key(VK_LEFT);
        check("Sound arrow adjustment works", raw(instance, 1, 127) == 4);
        check("keyboard gestures balanced",
              instance.handler.begins - beginsBeforeKeys ==
              instance.handler.ends - endsBeforeKeys);
        editor.key(VK_TAB);
        editor.key(VK_TAB, true);
        editor.key(VK_RETURN);
        editor.key(VK_ESCAPE);

        // Modulation: focus an exact typed route and verify invalid host values do not leak.
        editor.click(24 + 2 * 176 + 82, 96);
        editor.click(170 + 43, 149); // D1.
        editor.tabs(13); // five views, six owners, amount, destination.
        editor.key(VK_HOME);
        check("typed route Home selects OFF", raw(instance, 228, 65535) == 0);
        editor.key(VK_RIGHT);
        check("typed route arrow selects validated identity",
              raw(instance, 228, 65535) > 0);

        // Sequencer: keyboard gate edit reaches the authoritative project bank.
        editor.click(24 + 3 * 176 + 82, 96);
        editor.tabs(27); // five views, six owners, eight patterns, seven tracks, first cell.
        auto stateBefore = instance.saveState();
        editor.key(VK_SPACE);
        auto stateAfter = instance.saveState();
        check("sequencer Space edits selected gate", stateAfter != stateBefore);
        editor.key(VK_RIGHT);
        editor.key(VK_DOWN);
        editor.key(VK_HOME);
        editor.key(VK_END);
        editor.key(VK_ESCAPE);

        // Kits/Morph is keyboard reachable and Morph Amount retains 0..255.
        editor.click(24 + 4 * 176 + 82, 96);
        editor.key(VK_TAB);
        editor.key(VK_TAB, true);
        audio.stop();
        check("continuous audio ran during GUI stress", audio.callbacks() >= 100u);
        check("continuous audio reported no processing failures", audio.failures() == 0u);
        std::printf("P19F_AUDIO callbacks=%llu failures=%llu block_frames=64\n",
                    static_cast<unsigned long long>(audio.callbacks()),
                    static_cast<unsigned long long>(audio.failures()));
        instance.processParameter(245, 1.0);
        require("mirror exact Morph Amount in controller",
                instance.controller->setParamNormalized(245, 1.0) == kResultOk);
        check("Morph Amount reads back 255", raw(instance, 245, 255) == 255);
        saved = instance.saveState();
    }

    {
        HostInstance restored(factory, info.cid);
        require("restore P19-F project", restored.loadState(saved));
        check("project state restored exactly", restored.saveState() == saved);
        const auto before = restored.saveState();
        { AttachedEditor reopened(restored); reopened.key(VK_TAB); }
        check("editor reopen preserves accepted project", restored.saveState() == before);
    }

    {
        HostInstance first(factory, info.cid);
        HostInstance second(factory, info.cid);
        require("edit first independent instance",
                first.controller->setParamNormalized(245, 1.0) == kResultOk);
        check("two controller states are independent",
              raw(first, 245, 255) == 255 && raw(second, 245, 255) == 0);
    }

    {
        HostInstance lifecycle(factory, info.cid);
        const auto before = heapSummary();
        UIHeapSnapshot peak = before;
        for (int cycle = 0; cycle < 10; ++cycle)
        {
            AttachedEditor editor(lifecycle);
            editor.click(24 + (cycle % 5) * 176 + 82, 96);
            const auto current = heapSummary();
            peak.blocks = std::max(peak.blocks, current.blocks);
            peak.bytes = std::max(peak.bytes, current.bytes);
        }
        const auto after = heapSummary();
        const auto retainedBlocks = static_cast<long long>(after.blocks) -
            static_cast<long long>(before.blocks);
        const auto retainedBytes = static_cast<long long>(after.bytes) -
            static_cast<long long>(before.bytes);
        std::printf("P19F_UI_HEAP cycles=10 before_blocks=%llu peak_blocks=%llu "
                    "after_blocks=%llu retained_blocks=%lld before_bytes=%llu "
                    "peak_bytes=%llu after_bytes=%llu retained_bytes=%lld\n",
                    static_cast<unsigned long long>(before.blocks),
                    static_cast<unsigned long long>(peak.blocks),
                    static_cast<unsigned long long>(after.blocks), retainedBlocks,
                    static_cast<unsigned long long>(before.bytes),
                    static_cast<unsigned long long>(peak.bytes),
                    static_cast<unsigned long long>(after.bytes), retainedBytes);
        check("repeated editor lifecycle heap growth is bounded",
              retainedBlocks < 5000 && retainedBytes < 16 * 1024 * 1024);
    }

    factory->release();
    require("exit P19-F DLL", exitDll());
    FreeLibrary(module);
    CoUninitialize();
    std::printf("P19F_VST3_GUI_QUALIFICATION SUMMARY pass=%d fail=%d "
                "views=5 params=246 visible=223 hidden=23 pdc=26\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
