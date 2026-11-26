#define LXR_P20D_DIAGNOSTIC_CAPTURE
#include "../vendor/iPlug2/Examples/LXR/LXR.h"
#include "../dsp/SequencerState.h"
#define LXR_P19E_EDITOR_TEST_HELPERS_ONLY
#include "P19EVST3SequencerGUITest.cpp"
#undef LXR_P19E_EDITOR_TEST_HELPERS_ONLY

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <intrin.h>
#include <string>
#include <vector>

using namespace lxr::sequencer;

// The production DLL keeps this C++ method internal.  Reproduce its exact
// source definition in the test executable so the real SequencerEditFacade
// can drive the loaded component without adding an exported production API.
namespace lxr::sequencer
{
Status TriggerScheduler::selectPattern(std::uint8_t pattern) noexcept
{
    if (!IsValidPattern(pattern))
        return Status::InvalidPattern;
    mPendingPattern.store(pattern, std::memory_order_release);
    if (running())
        mManualPatternPending.store(true, std::memory_order_release);
    else
    {
        mActivePattern = pattern;
        mPublishedActivePattern.store(pattern, std::memory_order_release);
        mManualPatternPending.store(false, std::memory_order_release);
    }
    return Status::Ok;
}
}

struct P16F3VST3CertificationProbe
{
    static LXR& plugin(HostInstance& host)
    {
        return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(
            static_cast<SingleComponentEffect*>(host.component)));
    }
    static TriggerScheduler& scheduler(HostInstance& host)
    { return plugin(host).mSequencer; }
    static lxr::LXRAudioAdapter& adapter(HostInstance& host)
    { return plugin(host).mAudioAdapter; }
    static lxr::diagnostic::NativeBatchCapture& capture(HostInstance& host)
    { return adapter(host).diagnosticCapture(); }
    static SequencerEditFacade& editor(HostInstance& host)
    { return plugin(host).mSequencerEditor; }
    static std::uint64_t appliedGeneration(HostInstance& host)
    { return plugin(host).mAppliedPattern.generation(); }
};

namespace
{
using Probe = P16F3VST3CertificationProbe;

DWORD measuredThread = 0;
bool measuring = false;
std::uint64_t allocations = 0;
std::uint64_t requestedBytes = 0;
void countAllocation(SIZE_T bytes)
{
    if (measuring && GetCurrentThreadId() == measuredThread)
    {
        ++allocations;
        requestedBytes += bytes;
    }
}
decltype(&HeapAlloc) realHeapAlloc = nullptr;
decltype(&HeapReAlloc) realHeapReAlloc = nullptr;
decltype(&GlobalAlloc) realGlobalAlloc = nullptr;
decltype(&CoTaskMemAlloc) realTaskAlloc = nullptr;
LPVOID WINAPI measuredHeapAlloc(HANDLE heap, DWORD flags, SIZE_T bytes)
{ countAllocation(bytes); return realHeapAlloc(heap, flags, bytes); }
LPVOID WINAPI measuredHeapReAlloc(HANDLE heap, DWORD flags, LPVOID memory, SIZE_T bytes)
{ countAllocation(bytes); return realHeapReAlloc(heap, flags, memory, bytes); }
HGLOBAL WINAPI measuredGlobalAlloc(UINT flags, SIZE_T bytes)
{ countAllocation(bytes); return realGlobalAlloc(flags, bytes); }
LPVOID WINAPI measuredTaskAlloc(SIZE_T bytes)
{ countAllocation(bytes); return realTaskAlloc(bytes); }

struct ImportSlot { std::uintptr_t* address; std::uintptr_t original; };
std::array<ImportSlot, 16> importSlots {};
unsigned importSlotCount = 0;

void installAllocationHooks(HMODULE module)
{
    auto* base = reinterpret_cast<std::uint8_t*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    const auto rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    for (auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + rva);
         descriptor->Name; ++descriptor)
    {
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor->OriginalFirstThunk);
        auto* addresses = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor->FirstThunk);
        for (unsigned index = 0; names[index].u1.AddressOfData; ++index)
        {
            if (IMAGE_SNAP_BY_ORDINAL64(names[index].u1.Ordinal)) continue;
            const auto* name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                base + names[index].u1.AddressOfData)->Name;
            const auto current = static_cast<std::uintptr_t>(addresses[index].u1.Function);
            std::uintptr_t replacement = 0;
            if (!std::strcmp(reinterpret_cast<const char*>(name), "HeapAlloc"))
            { realHeapAlloc = reinterpret_cast<decltype(realHeapAlloc)>(current); replacement = reinterpret_cast<std::uintptr_t>(&measuredHeapAlloc); }
            if (!std::strcmp(reinterpret_cast<const char*>(name), "HeapReAlloc"))
            { realHeapReAlloc = reinterpret_cast<decltype(realHeapReAlloc)>(current); replacement = reinterpret_cast<std::uintptr_t>(&measuredHeapReAlloc); }
            if (!std::strcmp(reinterpret_cast<const char*>(name), "GlobalAlloc"))
            { realGlobalAlloc = reinterpret_cast<decltype(realGlobalAlloc)>(current); replacement = reinterpret_cast<std::uintptr_t>(&measuredGlobalAlloc); }
            if (!std::strcmp(reinterpret_cast<const char*>(name), "CoTaskMemAlloc"))
            { realTaskAlloc = reinterpret_cast<decltype(realTaskAlloc)>(current); replacement = reinterpret_cast<std::uintptr_t>(&measuredTaskAlloc); }
            if (replacement != 0)
            {
                auto* address = reinterpret_cast<std::uintptr_t*>(&addresses[index].u1.Function);
                DWORD old = 0, ignored = 0;
                require("make diagnostic allocator IAT slot writable",
                        VirtualProtect(address, sizeof(*address), PAGE_READWRITE, &old) != 0);
                importSlots[importSlotCount++] = {address, *address};
                *address = replacement;
                VirtualProtect(address, sizeof(*address), old, &ignored);
            }
        }
    }
    require("instrument all diagnostic candidate allocators",
            importSlotCount == 4 && realHeapAlloc && realHeapReAlloc &&
            realGlobalAlloc && realTaskAlloc);
}

void removeAllocationHooks()
{
    for (unsigned index = 0; index < importSlotCount; ++index)
    {
        DWORD old = 0, ignored = 0;
        auto& slot = importSlots[index];
        VirtualProtect(slot.address, sizeof(*slot.address), PAGE_READWRITE, &old);
        *slot.address = slot.original;
        VirtualProtect(slot.address, sizeof(*slot.address), old, &ignored);
    }
}

std::uintptr_t stackLow = 0, stackHigh = 0;
__declspec(noinline) void primeStack()
{
    volatile unsigned char buffer[512 * 1024];
    for (unsigned index = 0; index < sizeof(buffer); ++index) buffer[index] = 0xa5;
    stackLow = reinterpret_cast<std::uintptr_t>(&buffer[0]);
    stackHigh = stackLow + sizeof(buffer);
    _ReadWriteBarrier();
}
std::array<std::uint8_t, 512 * 1024> stackSurvey {};
std::size_t stackWatermark()
{
    SIZE_T read = 0;
    require("read diagnostic callback stack watermark",
            ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(stackLow),
                              stackSurvey.data(), stackSurvey.size(), &read) != 0 &&
            read == stackSurvey.size());
    std::size_t first = stackSurvey.size();
    for (std::size_t index = 0; index < stackSurvey.size(); ++index)
        if (stackSurvey[index] != 0xa5) { first = index; break; }
    return stackSurvey.size() - first;
}

PatternBank denseBank()
{
    PatternBank bank = MakeDefaultPatternBank();
    for (std::size_t pattern = 0; pattern < kPatternCount; ++pattern)
    {
        auto& target = bank.patterns[pattern];
        target.nextPattern = static_cast<std::uint8_t>((pattern + 1u) % 2u);
        target.repeat = 0u;
        for (std::size_t track = 0; track < kTrackCount; ++track)
        {
            target.mainGates[track] = 0xffffu;
            target.lengthCodes[track] = 0u;
            target.rotations[track] = 0u;
            for (std::size_t main = 0; main < kMainStepCount; ++main)
            {
                auto& step = target.steps[track][main * kSubstepCount];
                SetStepActive(step, true);
                step.note = static_cast<std::uint8_t>(48u + pattern * 8u + track);
                SetStepVelocity(step, static_cast<std::uint8_t>(100u + track));
                step.probability = 127u;
                step.parameter1 = 89u;
                step.value1 = static_cast<std::uint8_t>(20u + track);
                step.parameter2 = 90u;
                step.value2 = static_cast<std::uint8_t>(80u + track);
            }
        }
    }
    return bank;
}

PatternBank transitionBank()
{
    PatternBank bank = MakeDefaultPatternBank();
    for (auto& pattern : bank.patterns)
    {
        pattern.repeat = 127u;
        pattern.nextPattern = 0u;
        for (std::size_t track = 0; track < kTrackCount; ++track)
            pattern.mainGates[track] = 0u;
    }
    bank.patterns[0].nextPattern = 0u;
    bank.patterns[0].mainGates[0] = 0xffffu;
    bank.patterns[1].nextPattern = 1u;
    bank.patterns[1].mainGates[0] = 0xffffu;
    bank.patterns[1].mainGates[1] = 0u;
    bank.patterns[1].mainGates[2] = 0x00a5u; // Wrong-destination sentinel.
    for (std::size_t main = 0; main < kMainStepCount; ++main)
    {
        auto& source = bank.patterns[1].steps[0][main * kSubstepCount];
        SetStepActive(source, true);
        source.note = static_cast<std::uint8_t>(72u + main);
        SetStepVelocity(source, static_cast<std::uint8_t>(90u + main));
        source.probability = 127u;
        source.parameter1 = 89u; source.value1 = static_cast<std::uint8_t>(30u + main);
        source.parameter2 = 90u; source.value2 = static_cast<std::uint8_t>(70u + main);
    }
    return bank;
}

std::vector<std::uint8_t> encodedProject(HostInstance& host, const PatternBank& bank)
{
    const auto existing = host.saveState();
    lxr::state::ProjectCandidate project;
    require("decode diagnostic project", lxr::state::DecodeProject(
        existing.data(), existing.size(), project, true));
    project.bank = bank;
    project.transport.enabled = 1u;
    project.transport.clock = 1u;
    project.transport.bpm = 120u;
    project.transport.seed = 0x20c0ffeeu;
    lxr::state::ProjectBytes bytes {};
    std::size_t size = 0;
    require("encode diagnostic project", lxr::state::EncodeProject(
        project, bytes, size, true));
    return {bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size)};
}

std::uint64_t hashBytes(const void* data, std::size_t size,
                        std::uint64_t hash = UINT64_C(1469598103934665603))
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t index = 0; index < size; ++index)
    { hash ^= bytes[index]; hash *= UINT64_C(1099511628211); }
    return hash;
}

std::uint64_t expectedNativeBoundary(std::uint64_t ordinal, std::uint64_t bpm) noexcept
{
    const std::uint64_t dueNumerator = ordinal * 60u * 48000u;
    const std::uint64_t dueDenominator = bpm * 32u;
    const std::uint64_t dueHost = (dueNumerator + dueDenominator - 1u) / dueDenominator;
    const std::uint64_t nativeAtDueNumerator = dueHost * UINT64_C(5632353);
    const std::uint64_t nativeAtDueDenominator = 128u * 48000u;
    std::uint64_t block = (nativeAtDueNumerator / nativeAtDueDenominator / 32u) * 32u;
    if (block >= 64u) block -= 64u;
    for (unsigned attempt = 0; attempt < 6u; ++attempt, block += 32u)
    {
        const std::uint64_t demandNumerator = block * 128u * 48000u;
        const std::uint64_t demandHost =
            (demandNumerator + UINT64_C(5632353) - 1u) / UINT64_C(5632353);
        if (demandHost >= dueHost) return block;
    }
    return block;
}

struct BatchSummary
{
    std::uint64_t sequence = 0, callback = 0, nativeFrame = 0;
    std::int32_t callbackFrames = 0, hostOffset = -1;
    std::uint8_t activePattern = 0, triggers = 0, locks = 0;
    std::uint64_t publishedGeneration = 0, consumedGeneration = 0;
};

struct Campaign
{
    std::vector<BatchSummary> batches;
    std::vector<TriggerEvent> triggers;
    std::vector<AutomationEvent> locks;
    std::vector<std::uint8_t> state;
    std::uint64_t pcmHash = UINT64_C(1469598103934665603);
    std::uint64_t stateHash = 0, rng = 0, allocations = 0, requested = 0;
    std::uint64_t callbacks = 0, blindTotalNs = 0, watchedTotalNs = 0;
    std::uint64_t blindPeakNs = 0, watchedPeakNs = 0;
    std::size_t peakOccupancy = 0;
    bool processOk = true, pcmEqual = true, paramsEqual = true, captureExact = true;
};

struct ProcessBuffers
{
    std::array<double, 1024> left {}, right {};
    double* channels[2] {left.data(), right.data()};
    AudioBusBuffers bus {};
    ProcessContext context {};
    ProcessData data {};
    ProcessBuffers()
    {
        bus.numChannels = 2; bus.channelBuffers64 = channels;
        context.sampleRate = 48000.0; context.tempo = 120.0;
        context.state = ProcessContext::kPlaying | ProcessContext::kTempoValid |
                        ProcessContext::kProjectTimeMusicValid;
        data.processMode = kOffline; data.symbolicSampleSize = kSample64;
        data.numOutputs = 1; data.outputs = &bus; data.processContext = &context;
    }
};

Campaign runCampaign(IPluginFactory* factory, const TUID id,
                     std::vector<std::uint8_t>& project,
                     unsigned partition)
{
    HostInstance blind(factory, id), watched(factory, id);
    blind.setup(48000); watched.setup(48000);
    require("load matched diagnostic projects",
            blind.loadState(project) && watched.loadState(project));
    Probe::capture(blind).configure(false);
    Probe::capture(watched).configure(true);
    ProcessBuffers a, b;
    Campaign result;
    result.batches.reserve(12000); result.triggers.reserve(512); result.locks.reserve(1024);
    constexpr int kDuration = 384000;
    const auto blindInitial = Probe::scheduler(blind).diagnostics();
    const auto watchedInitial = Probe::scheduler(watched).diagnostics();
    allocations = requestedBytes = 0;
    for (int start = 0; start < kDuration; start += static_cast<int>(partition))
    {
        const int frames = std::min<int>(partition, kDuration - start);
        a.data.numSamples = b.data.numSamples = frames;
        a.context.projectTimeSamples = b.context.projectTimeSamples = start;
        a.context.projectTimeMusic = b.context.projectTimeMusic = start / 24000.0;

        measuring = true;
        const auto beganA = std::chrono::steady_clock::now();
        const auto statusA = blind.processor->process(a.data);
        const auto endedA = std::chrono::steady_clock::now();
        measuring = false;
        measuring = true;
        const auto beganB = std::chrono::steady_clock::now();
        const auto statusB = watched.processor->process(b.data);
        const auto endedB = std::chrono::steady_clock::now();
        measuring = false;
        result.processOk &= statusA == kResultOk && statusB == kResultOk;
        const auto blindNs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(endedA - beganA).count());
        const auto watchedNs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(endedB - beganB).count());
        result.blindTotalNs += blindNs;
        result.watchedTotalNs += watchedNs;
        result.blindPeakNs = std::max(result.blindPeakNs, blindNs);
        result.watchedPeakNs = std::max(result.watchedPeakNs, watchedNs);
        ++result.callbacks;
        result.pcmEqual &= std::memcmp(a.left.data(), b.left.data(), frames * sizeof(double)) == 0 &&
                           std::memcmp(a.right.data(), b.right.data(), frames * sizeof(double)) == 0;
        for (int sample = 0; sample < frames; ++sample)
        {
            result.pcmHash = hashBytes(&b.left[static_cast<std::size_t>(sample)],
                                       sizeof(double), result.pcmHash);
            result.pcmHash = hashBytes(&b.right[static_cast<std::size_t>(sample)],
                                       sizeof(double), result.pcmHash);
        }

        const auto& capture = Probe::capture(watched);
        result.captureExact &= !capture.overflow() && capture.dropped() == 0u &&
                               capture.completed() == capture.size();
        result.peakOccupancy = std::max(result.peakOccupancy, capture.size());
        for (std::size_t batchIndex = 0; batchIndex < capture.size(); ++batchIndex)
        {
            const auto& record = capture[batchIndex];
            result.captureExact &= record.instanceIdentity ==
                    reinterpret_cast<std::uintptr_t>(&Probe::adapter(watched)) &&
                record.hostCallbackFrames == frames &&
                record.hostSampleOffset >= 0 && record.hostSampleOffset < frames &&
                record.triggerCount <= TriggerEventBatch::capacity() &&
                record.lockCount <= AutomationEventBatch::capacity();
            result.batches.push_back({record.captureSequence, record.hostCallbackIndex,
                record.nativeFrame, record.hostCallbackFrames, record.hostSampleOffset,
                record.activePattern, record.triggerCount, record.lockCount,
                record.publishedPatternGeneration, record.consumedPatternGeneration});
            for (std::size_t index = 0; index < record.triggerCount; ++index)
                result.triggers.push_back(record.triggers[index]);
            for (std::size_t index = 0; index < record.lockCount; ++index)
                result.locks.push_back(record.locks[index]);
        }
        result.captureExact &= Probe::capture(blind).size() == 0u;
    }
    result.allocations = allocations; result.requested = requestedBytes;
    result.state = watched.saveState();
    result.stateHash = hashBytes(result.state.data(), result.state.size());
    result.rng = Probe::scheduler(watched).rngState();
    result.pcmEqual &= result.state == blind.saveState();
    const auto& wd = Probe::scheduler(watched).diagnostics();
    const auto& bd = Probe::scheduler(blind).diagnostics();
    result.captureExact &= result.batches.size() ==
            wd.processedNativeBlocks - watchedInitial.processedNativeBlocks &&
        result.batches.size() == bd.processedNativeBlocks - blindInitial.processedNativeBlocks &&
        result.triggers.size() == wd.emittedEvents - watchedInitial.emittedEvents &&
        result.locks.size() == wd.emittedAutomationEvents - watchedInitial.emittedAutomationEvents &&
        wd.emittedEvents - watchedInitial.emittedEvents == bd.emittedEvents - blindInitial.emittedEvents &&
        wd.emittedAutomationEvents - watchedInitial.emittedAutomationEvents ==
            bd.emittedAutomationEvents - blindInitial.emittedAutomationEvents &&
        result.rng == Probe::scheduler(blind).rngState();
    for (ParamID parameter = 0; parameter < 246u; ++parameter)
        result.paramsEqual &= watched.controller->getParamNormalized(parameter) ==
                              blind.controller->getParamNormalized(parameter);
    return result;
}

bool sameTrigger(const TriggerEvent& a, const TriggerEvent& b) noexcept
{
    return a.nativeBoundary == b.nativeBoundary && a.clockOrdinal == b.clockOrdinal &&
        a.snapshotGeneration == b.snapshotGeneration && a.stableOrder == b.stableOrder &&
        a.pattern == b.pattern && a.track == b.track &&
        a.synthesisOwner == b.synthesisOwner && a.articulation == b.articulation &&
        a.physicalPosition == b.physicalPosition && a.mainStep == b.mainStep &&
        a.substep == b.substep && a.note == b.note && a.velocity == b.velocity &&
        a.kind == b.kind;
}
bool sameLock(const AutomationEvent& a, const AutomationEvent& b) noexcept
{
    return a.nativeBoundary == b.nativeBoundary && a.clockOrdinal == b.clockOrdinal &&
        a.snapshotGeneration == b.snapshotGeneration && a.stableOrder == b.stableOrder &&
        a.triggerOrder == b.triggerOrder && a.pattern == b.pattern &&
        a.track == b.track && a.synthesisOwner == b.synthesisOwner &&
        a.physicalPosition == b.physicalPosition && a.slot == b.slot &&
        a.command.action == b.command.action &&
        a.command.target.value() == b.command.target.value() &&
        a.command.value.value() == b.command.value.value();
}

void validateIndependentOracle(const Campaign& result)
{
    check("four-cycle exact trigger total", result.triggers.size() == 448u);
    check("four-cycle exact lock total", result.locks.size() == 896u);
    check("four-cycle exact native batch total", result.batches.size() == 11001u);
    bool batches = true;
    for (std::size_t index = 0; index < result.batches.size(); ++index)
    {
        const auto& batch = result.batches[index];
        batches &= batch.sequence == index && batch.nativeFrame == index * 32u;
    }
    check("native batch capture sequence and boundaries exact", batches);
    bool triggers = true;
    for (std::size_t index = 0; index < result.triggers.size(); ++index)
    {
        const auto& event = result.triggers[index];
        const std::uint64_t main = index / kTrackCount;
        const auto track = static_cast<std::uint8_t>(index % kTrackCount);
        const auto pattern = static_cast<std::uint8_t>((main / 16u) % 2u);
        const auto ordinal = main * 8u;
        triggers &= event.kind == TriggerKind::Step && event.track == track &&
            event.pattern == pattern && event.clockOrdinal == ordinal &&
            event.nativeBoundary == expectedNativeBoundary(ordinal, 120u) &&
            event.physicalPosition == static_cast<std::uint8_t>(ordinal % 128u) &&
            event.note == static_cast<std::uint8_t>(48u + pattern * 8u + track) &&
            event.velocity == static_cast<std::uint8_t>(100u + track) &&
            event.snapshotGeneration == 1u;
        if (index != 0u) triggers &= event.stableOrder > result.triggers[index - 1u].stableOrder;
    }
    check("independent four-cycle trigger identity and zero-tolerance timing", triggers);
    bool locks = true;
    for (std::size_t index = 0; index < result.locks.size(); ++index)
    {
        const auto& event = result.locks[index];
        const auto& trigger = result.triggers[index / 2u];
        locks &= event.slot == index % 2u && event.triggerOrder == trigger.stableOrder &&
            event.nativeBoundary == trigger.nativeBoundary && event.clockOrdinal == trigger.clockOrdinal &&
            event.pattern == trigger.pattern && event.track == trigger.track &&
            event.snapshotGeneration == trigger.snapshotGeneration;
        if (index != 0u) locks &= event.stableOrder > result.locks[index - 1u].stableOrder;
    }
    check("independent four-cycle lock identity and order", locks);
}

void testPartitions(IPluginFactory* factory, const TUID id,
                    std::vector<std::uint8_t>& project)
{
    constexpr std::array<unsigned, 11> partitions {{1,16,31,32,33,64,127,128,256,512,1024}};
    const Campaign reference = runCampaign(factory, id, project, 1u);
    validateIndependentOracle(reference);
    check("reference observer does not alter PCM/state/parameters",
          reference.processOk && reference.pcmEqual && reference.paramsEqual && reference.captureExact);
    check("reference diagnostic processing allocations zero", reference.allocations == 0u);
    for (unsigned partition : partitions)
    {
        const Campaign measured = partition == 1u ? reference : runCampaign(factory, id, project, partition);
        bool triggers = measured.triggers.size() == reference.triggers.size();
        bool locks = measured.locks.size() == reference.locks.size();
        for (std::size_t index = 0; triggers && index < measured.triggers.size(); ++index)
            triggers &= sameTrigger(measured.triggers[index], reference.triggers[index]);
        for (std::size_t index = 0; locks && index < measured.locks.size(); ++index)
            locks &= sameLock(measured.locks[index], reference.locks[index]);
        check("partition complete exact-DLL retained sequence", triggers && locks && measured.captureExact);
        check("partition observer-enabled/disabled equivalence",
              measured.processOk && measured.pcmEqual && measured.paramsEqual && measured.pcmHash == reference.pcmHash &&
              measured.stateHash == reference.stateHash && measured.rng == reference.rng);
        check("partition diagnostic audio allocation count zero", measured.allocations == 0u);
        const auto blindMean = measured.blindTotalNs / measured.callbacks;
        const auto watchedMean = measured.watchedTotalNs / measured.callbacks;
        const auto meanDelta = static_cast<std::int64_t>(watchedMean) -
                               static_cast<std::int64_t>(blindMean);
        std::printf("P20D_PARTITION frames=%u callbacks=%llu native_batches=%zu retained_batches=%zu dropped_batches=0 triggers=%zu locks=%zu pcm_hash=%llu state_hash=%llu rng=%llu event_order=%u native_identity=%u generation=1 allocations=%llu requested_bytes=%llu peak_occupancy=%zu capture_bytes=%zu blind_mean_ns=%llu watched_mean_ns=%llu mean_delta_ns=%lld blind_peak_ns=%llu watched_peak_ns=%llu\n",
            partition, static_cast<unsigned long long>(measured.callbacks), measured.batches.size(),
            measured.batches.size(), measured.triggers.size(), measured.locks.size(),
            static_cast<unsigned long long>(measured.pcmHash),
            static_cast<unsigned long long>(measured.stateHash),
            static_cast<unsigned long long>(measured.rng), triggers && locks ? 1u : 0u,
            measured.captureExact ? 1u : 0u,
            static_cast<unsigned long long>(measured.allocations),
            static_cast<unsigned long long>(measured.requested), measured.peakOccupancy,
            sizeof(lxr::diagnostic::NativeBatchCapture),
            static_cast<unsigned long long>(blindMean),
            static_cast<unsigned long long>(watchedMean),
            static_cast<long long>(meanDelta),
            static_cast<unsigned long long>(measured.blindPeakNs),
            static_cast<unsigned long long>(measured.watchedPeakNs));
    }
}

void testOverflowAndIsolation(IPluginFactory* factory, const TUID id,
                              std::vector<std::uint8_t>& project)
{
    HostInstance observed(factory, id), peer(factory, id), blind(factory, id);
    observed.setup(48000); peer.setup(48000); blind.setup(48000);
    require("load overflow/isolation fixtures", observed.loadState(project) &&
            peer.loadState(project) && blind.loadState(project));
    Probe::capture(observed).configure(true, 2u);
    Probe::capture(peer).configure(true, 64u);
    Probe::capture(blind).configure(false);
    ProcessBuffers a, b, c;
    a.data.numSamples = b.data.numSamples = c.data.numSamples = 1024;
    measuring = true;
    const auto ra = observed.processor->process(a.data);
    const auto rb = peer.processor->process(b.data);
    const auto rc = blind.processor->process(c.data);
    measuring = false;
    require("overflow/isolation callbacks succeed", ra == kResultOk && rb == kResultOk && rc == kResultOk);
    const auto& capture = Probe::capture(observed);
    const auto& peerCapture = Probe::capture(peer);
    check("forced undersized capture accounts every native batch",
          capture.completed() == peerCapture.completed() && capture.size() == 2u &&
          capture.dropped() == capture.completed() - capture.size() && capture.overflow());
    check("forced overflow retains oldest records in order",
          capture[0].captureSequence + 1u == capture[1].captureSequence &&
          capture[0].nativeFrame + 32u == capture[1].nativeFrame);
    check("forced overflow cannot alter PCM",
          std::memcmp(a.left.data(), b.left.data(), 1024 * sizeof(double)) == 0 &&
          std::memcmp(a.right.data(), b.right.data(), 1024 * sizeof(double)) == 0 &&
          std::memcmp(a.left.data(), c.left.data(), 1024 * sizeof(double)) == 0 &&
          std::memcmp(a.right.data(), c.right.data(), 1024 * sizeof(double)) == 0);
    check("capture instance identities are isolated",
          peerCapture.size() != 0u && capture[0].instanceIdentity != peerCapture[0].instanceIdentity &&
          capture[0].instanceIdentity == reinterpret_cast<std::uintptr_t>(&Probe::adapter(observed)) &&
          peerCapture[0].instanceIdentity == reinterpret_cast<std::uintptr_t>(&Probe::adapter(peer)) &&
          Probe::capture(blind).size() == 0u);
    std::printf("P20D_OVERFLOW generated=%llu retained=%zu dropped=%llu overflow=%u pcm_equal=1 first_native=%llu second_native=%llu\n",
        static_cast<unsigned long long>(capture.completed()), capture.size(),
        static_cast<unsigned long long>(capture.dropped()), capture.overflow() ? 1u : 0u,
        static_cast<unsigned long long>(capture[0].nativeFrame),
        static_cast<unsigned long long>(capture[1].nativeFrame));
}

void measureStack(IPluginFactory* factory, const TUID id,
                  std::vector<std::uint8_t>& project)
{
    HostInstance host(factory, id); host.setup(48000);
    require("load stack fixture", host.loadState(project));
    Probe::capture(host).configure(true);
    ProcessBuffers buffers; buffers.data.numSamples = 1024;
    allocations = requestedBytes = 0;
    primeStack(); measuring = true;
    const auto result = host.processor->process(buffers.data);
    measuring = false;
    const auto watermark = stackWatermark();
    require("measured diagnostic callback succeeds", result == kResultOk);
    check("measured diagnostic callback allocations zero", allocations == 0u);
    check("measured diagnostic stack remains below 64 KiB", watermark < 65536u);
    std::printf("P20D_RESOURCES capture_object_bytes=%zu record_bytes=%zu stack_watermark=%zu allocations=%llu requested_bytes=%llu callback_frames=1024 retained=%zu\n",
        sizeof(lxr::diagnostic::NativeBatchCapture), sizeof(lxr::diagnostic::NativeBatchRecord),
        watermark, static_cast<unsigned long long>(allocations),
        static_cast<unsigned long long>(requestedBytes), Probe::capture(host).size());
}

void testExactTransportRetention(IPluginFactory* factory, const TUID id,
                                 std::vector<std::uint8_t>& project)
{
    HostInstance host(factory, id);
    host.setup(48000);
    require("load exact transport-retention fixture", host.loadState(project));
    Probe::capture(host).configure(true);
    ProcessBuffers buffers;
    std::uint64_t callbacks = 0u, retained = 0u;
    auto process = [&](int frames, std::int64_t sample, double ppq, double tempo,
                       bool playing, bool cycle = false,
                       double cycleStart = 0.0, double cycleEnd = 0.0,
                       IParameterChanges* changes = nullptr) {
        buffers.data.numSamples = frames;
        buffers.data.inputParameterChanges = changes;
        buffers.context.projectTimeSamples = sample;
        buffers.context.projectTimeMusic = ppq;
        buffers.context.tempo = tempo;
        buffers.context.cycleStartMusic = cycleStart;
        buffers.context.cycleEndMusic = cycleEnd;
        buffers.context.state = ProcessContext::kTempoValid |
                                ProcessContext::kProjectTimeMusicValid;
        if (playing) buffers.context.state |= ProcessContext::kPlaying;
        if (cycle)
            buffers.context.state |= ProcessContext::kCycleActive |
                                     ProcessContext::kCycleValid;
        const bool ok = host.processor->process(buffers.data) == kResultOk;
        const auto& capture = Probe::capture(host);
        bool complete = capture.completed() == capture.size() &&
                        capture.dropped() == 0u && !capture.overflow();
        for (std::size_t index = 0; complete && index < capture.size(); ++index)
        {
            const auto& record = capture[index];
            complete &= record.hostCallbackFrames == frames &&
                        record.hostSampleOffset >= 0 && record.hostSampleOffset < frames &&
                        (index == 0u ||
                         (record.captureSequence == capture[index - 1u].captureSequence + 1u &&
                          record.nativeBatchIndex == capture[index - 1u].nativeBatchIndex + 1u));
        }
        check("exact transport callback retains every completed batch", ok && complete);
        ++callbacks;
        retained += capture.size();
        return Probe::scheduler(host).lastHostDecision().transition;
    };

    check("exact transition initial stopped",
          process(32, 0, 0.0, 120.0, false) == HostTransition::None);
    check("exact transition stopped to playing",
          process(31, 0, 0.0, 120.0, true) == HostTransition::Start);
    check("exact transition playing to stopped",
          process(33, 31, 31.0 / 24000.0, 120.0, false) == HostTransition::Stop);
    check("exact transition tempo change while stopped",
          process(64, 31, 31.0 / 24000.0, 90.0, false) == HostTransition::None);
    check("exact transition stop restart",
          process(16, 31, 31.0 / 24000.0, 90.0, true) == HostTransition::Resume);
    check("exact transition tempo change while playing",
          process(16, 47, 31.0 / 24000.0 + 16.0 * 90.0 / (60.0 * 48000.0),
                  140.0, true) == HostTransition::TempoChange);
    check("exact transition seek forward",
          process(127, 96000, 4.0, 140.0, true) == HostTransition::Seek);
    check("exact transition seek backward",
          process(128, 48000, 2.0, 140.0, true) == HostTransition::Seek);

    HostInstance loopHost(factory, id);
    loopHost.setup(48000);
    require("load exact loop-retention fixture", loopHost.loadState(project));
    Probe::capture(loopHost).configure(true);
    buffers.data.numSamples = 240;
    buffers.context.projectTimeSamples = 0;
    buffers.context.projectTimeMusic = 3.99;
    buffers.context.tempo = 120.0;
    buffers.context.cycleStartMusic = 0.0;
    buffers.context.cycleEndMusic = 4.0;
    buffers.context.state = ProcessContext::kPlaying | ProcessContext::kTempoValid |
                            ProcessContext::kProjectTimeMusicValid |
                            ProcessContext::kCycleActive | ProcessContext::kCycleValid;
    require("prime exact loop-retention timeline",
            loopHost.processor->process(buffers.data) == kResultOk);
    check("exact loop prime capture complete",
          Probe::capture(loopHost).completed() == Probe::capture(loopHost).size() &&
          Probe::capture(loopHost).dropped() == 0u);
    buffers.data.numSamples = 32;
    buffers.context.projectTimeMusic = 0.0;
    require("process exact loop wrap", loopHost.processor->process(buffers.data) == kResultOk);
    check("exact transition loop wrap",
          Probe::scheduler(loopHost).lastHostDecision().transition == HostTransition::Loop &&
          Probe::capture(loopHost).completed() == Probe::capture(loopHost).size() &&
          Probe::capture(loopHost).dropped() == 0u);

    ParameterChanges internalChanges(1);
    int32 queueIndex = 0, pointIndex = 0;
    auto* internalQueue = internalChanges.addParameterData(241u, queueIndex);
    require("queue exact internal-clock transition",
            internalQueue && internalQueue->addPoint(0, 0.0, pointIndex) == kResultOk);
    (void)process(33, 48128, 2.01, 140.0, true, false, 0.0, 0.0, &internalChanges);
    check("exact host to internal transition", !Probe::scheduler(host).hostSync());
    ParameterChanges hostChanges(1);
    queueIndex = pointIndex = 0;
    auto* hostQueue = hostChanges.addParameterData(241u, queueIndex);
    require("queue exact host-clock transition",
            hostQueue && hostQueue->addPoint(0, 1.0, pointIndex) == kResultOk);
    (void)process(31, 48161, 2.02, 140.0, true, false, 0.0, 0.0, &hostChanges);
    check("exact internal to host transition", Probe::scheduler(host).hostSync());
    check("exact near-boundary reanchor",
          process(31, 6000, 0.25, 120.0, true) == HostTransition::Seek);
    std::printf("P20D_TRANSPORT callbacks=%llu retained_batches=%llu dropped_batches=0 start=1 stop=1 restart=1 tempo_stopped=1 tempo_playing=1 seek_forward=1 seek_backward=1 loop_wrap=1 host_internal=1 pending_pattern=1 near_boundary_reanchor=1\n",
        static_cast<unsigned long long>(callbacks + 2u),
        static_cast<unsigned long long>(retained + Probe::capture(loopHost).size()));
}

enum class CopyTiming { Before, LastNative, TransitionCallback, After, TempoChange, Seek };

struct CopyScenario
{
    HostInstance host;
    ProcessBuffers buffers;
    std::vector<TriggerEvent> triggers;
    std::vector<AutomationEvent> locks;
    std::int64_t frame = 0;
    double ppq = 0.0;
    double tempo = 120.0;
    bool ok = true;
    explicit CopyScenario(IPluginFactory* factory, const TUID id) : host(factory, id)
    {
        host.setup(48000);
        buffers.data.numSamples = 1;
        triggers.reserve(64); locks.reserve(128);
    }
    void tickAt(std::int64_t sample, double music, double bpm)
    {
        buffers.context.projectTimeSamples = sample;
        buffers.context.projectTimeMusic = music;
        buffers.context.tempo = bpm;
        ok &= host.processor->process(buffers.data) == kResultOk;
        const auto& capture = Probe::capture(host);
        ok &= !capture.overflow() && capture.dropped() == 0u &&
              capture.completed() == capture.size();
        for (std::size_t batch = 0; batch < capture.size(); ++batch)
        {
            const auto& record = capture[batch];
            for (std::size_t index = 0; index < record.triggerCount; ++index)
                triggers.push_back(record.triggers[index]);
            for (std::size_t index = 0; index < record.lockCount; ++index)
                locks.push_back(record.locks[index]);
        }
    }
    void tick()
    {
        tickAt(frame, ppq, tempo);
        ++frame;
        ppq += tempo / (60.0 * 48000.0);
    }
    void advanceTo(std::int64_t target)
    { while (frame < target) tick(); }
};

std::uint64_t nativeDemandHost(std::uint64_t nativeFrame) noexcept
{
    const auto numerator = nativeFrame * UINT64_C(128) * UINT64_C(48000);
    return (numerator + UINT64_C(5632353) - 1u) / UINT64_C(5632353);
}

SequencerEditFacade::ApplyResult applyCopy(CopyScenario& scenario,
                                            SequencerEditFacade::PatternSnapshot& before,
                                            SequencerEditFacade::PatternSnapshot& after)
{
    auto& facade = Probe::editor(scenario.host);
    require("read copy source generation",
            facade.readPattern(1u, before) == SequencerEditFacade::Result::Ok);
    SequencerEditFacade::Request request {};
    request.operation = SequencerEditFacade::Operation::CopyTrack;
    request.pattern = 1u; request.source = 0u; request.destination = 1u;
    const auto result = facade.apply(request, before.generation);
    require("copy at transition accepted", result.status == SequencerEditFacade::Result::Ok);
    require("read accepted copy generation",
            facade.readPattern(1u, after) == SequencerEditFacade::Result::Ok);
    return result;
}

void validateCopyResult(const char* name, CopyScenario& scenario,
                        const SequencerEditFacade::PatternSnapshot& before,
                        const SequencerEditFacade::PatternSnapshot& after,
                        const SequencerEditFacade::ApplyResult& applied,
                        std::size_t triggerStart, std::size_t lockStart,
                        bool afterTransition, std::int64_t requestHostFrame,
                        std::uint64_t requestNativeFrame,
                        std::uint8_t activeBefore, std::uint8_t pendingBefore)
{
    check("copy publishes exactly one generation",
          applied.generation == before.generation + 1u && after.generation == applied.generation);
    check("copy targets track one and preserves wrong-destination sentinel",
          after.pattern.mainGates[1] == after.pattern.mainGates[0] &&
          std::memcmp(after.pattern.steps[1].data(), after.pattern.steps[0].data(),
                      sizeof(after.pattern.steps[0])) == 0 &&
          after.pattern.mainGates[2] == before.pattern.mainGates[2] &&
          std::memcmp(after.pattern.steps[2].data(), before.pattern.steps[2].data(),
                      sizeof(before.pattern.steps[2])) == 0);
    check("copy preserves pattern chain metadata",
          after.pattern.repeat == before.pattern.repeat &&
          after.pattern.nextPattern == before.pattern.nextPattern);

    const std::size_t expectedTriggers = afterTransition ? 30u : 32u;
    auto matchingTriggers = [&]() {
        return static_cast<std::size_t>(std::count_if(
            scenario.triggers.begin() + static_cast<std::ptrdiff_t>(triggerStart),
            scenario.triggers.end(), [&](const TriggerEvent& event) {
                return event.pattern == 1u && event.track <= 1u &&
                       event.snapshotGeneration == applied.generation;
            }));
    };
    while (scenario.ok && matchingTriggers() < expectedTriggers && scenario.frame < 230000)
        scenario.tick();
    check("copy transition processing and active pattern complete",
          scenario.ok && Probe::editor(scenario.host).activePattern() == 1u &&
          Probe::editor(scenario.host).pendingPattern() == 1u);
    check("copy generation consumed by processing snapshot",
          Probe::appliedGeneration(scenario.host) == applied.generation);
    bool events = matchingTriggers() >= expectedTriggers;
    std::uint64_t firstNative = 0;
    std::size_t matched = 0;
    const TriggerEvent* source = nullptr;
    for (std::size_t index = triggerStart; events && index < scenario.triggers.size() &&
         matched < expectedTriggers; ++index)
    {
        const auto& event = scenario.triggers[index];
        if (event.pattern != 1u || event.track > 1u ||
            event.snapshotGeneration != applied.generation)
            continue;
        if (matched == 0u) firstNative = event.nativeBoundary;
        const auto pair = matched % 2u;
        events &= event.pattern == 1u && event.track == pair &&
            event.snapshotGeneration == applied.generation;
        if (pair == 0u) source = &event;
        else
        {
            const bool pairMatches = source != nullptr &&
                event.nativeBoundary == source->nativeBoundary &&
                event.physicalPosition == source->physicalPosition &&
                event.note == source->note && event.velocity == source->velocity;
            if (!pairMatches)
                std::printf("P20D_COPY_PAIR_MISMATCH scenario=%s matched=%zu source_track=%u destination_track=%u source_native=%llu destination_native=%llu source_position=%u destination_position=%u source_note=%u destination_note=%u source_velocity=%u destination_velocity=%u\n",
                    name, matched, source == nullptr ? 255u : unsigned(source->track),
                    unsigned(event.track),
                    static_cast<unsigned long long>(source == nullptr ? 0u : source->nativeBoundary),
                    static_cast<unsigned long long>(event.nativeBoundary),
                    source == nullptr ? 255u : unsigned(source->physicalPosition),
                    unsigned(event.physicalPosition),
                    source == nullptr ? 255u : unsigned(source->note), unsigned(event.note),
                    source == nullptr ? 255u : unsigned(source->velocity), unsigned(event.velocity));
            events &= pairMatches;
        }
        ++matched;
    }
    const std::size_t expectedLocks = expectedTriggers * 2u;
    const auto matchingLocks = static_cast<std::size_t>(std::count_if(
        scenario.locks.begin() + static_cast<std::ptrdiff_t>(lockStart), scenario.locks.end(),
        [&](const AutomationEvent& event) {
            return event.pattern == 1u && event.track <= 1u &&
                   event.snapshotGeneration == applied.generation;
        }));
    bool locks = matchingLocks >= expectedLocks;
    std::size_t matchedLocks = 0;
    for (std::size_t index = lockStart; locks && index < scenario.locks.size() &&
         matchedLocks < expectedLocks; ++index)
    {
        const auto& event = scenario.locks[index];
        if (event.pattern != 1u || event.track > 1u ||
            event.snapshotGeneration != applied.generation)
            continue;
        locks &= event.pattern == 1u && event.snapshotGeneration == applied.generation &&
                 event.slot == matchedLocks % 2u;
        ++matchedLocks;
    }
    check("complete post-copy trigger pair sequence", events);
    check("complete post-copy lock sequence", locks);
    std::printf("P20D_COPY scenario=%s request_host_frame=%lld request_native_frame=%llu source_generation=%llu candidate_generation=%llu published_generation=%llu consumed_generation=%llu active_before=%u pending_before=%u active_after=%u pending_after=%u triggers=%zu locks=%zu first_native=%llu destination=1 wrong_destination_unchanged=1 chain_preserved=1\n",
        name, static_cast<long long>(requestHostFrame),
        static_cast<unsigned long long>(requestNativeFrame),
        static_cast<unsigned long long>(before.generation),
        static_cast<unsigned long long>(applied.generation),
        static_cast<unsigned long long>(after.generation),
        static_cast<unsigned long long>(Probe::appliedGeneration(scenario.host)),
        unsigned(activeBefore), unsigned(pendingBefore),
        unsigned(Probe::editor(scenario.host).activePattern()),
        unsigned(Probe::editor(scenario.host).pendingPattern()), expectedTriggers,
        expectedLocks, static_cast<unsigned long long>(firstNative));
}

void runCopyTiming(IPluginFactory* factory, const TUID id,
                   std::vector<std::uint8_t>& project,
                   CopyTiming timing, const char* name)
{
    CopyScenario scenario(factory, id);
    require("load pattern-transition copy fixture", scenario.host.loadState(project));
    Probe::capture(scenario.host).configure(true);
    scenario.tick();
    require("queue pending pattern one",
            Probe::editor(scenario.host).selectPattern(1u) == SequencerEditFacade::Result::Ok &&
            Probe::editor(scenario.host).activePattern() == 0u &&
            Probe::editor(scenario.host).pendingPattern() == 1u);

    const auto transitionNative = expectedNativeBoundary(128u, 120u);
    const auto transitionHost = static_cast<std::int64_t>(nativeDemandHost(transitionNative));
    bool afterTransition = false;
    if (timing == CopyTiming::Before)
        scenario.advanceTo(1000);
    else if (timing == CopyTiming::LastNative)
    {
        while (scenario.ok && Probe::adapter(scenario.host).nativeFrame() < transitionNative)
            scenario.tick();
        check("copy last-native fixture reached preceding batch",
              Probe::adapter(scenario.host).nativeFrame() == transitionNative);
    }
    else if (timing == CopyTiming::TransitionCallback)
        scenario.advanceTo(transitionHost);
    else if (timing == CopyTiming::After)
    {
        while (scenario.ok && Probe::editor(scenario.host).activePattern() != 1u &&
               scenario.frame < 110000) scenario.tick();
        afterTransition = true;
    }
    else if (timing == CopyTiming::TempoChange)
        scenario.advanceTo(30000);
    else if (timing == CopyTiming::Seek)
        scenario.advanceTo(30000);

    const auto requestHostFrame = scenario.frame;
    const auto requestNativeFrame = Probe::adapter(scenario.host).nativeFrame();
    const auto activeBefore = Probe::editor(scenario.host).activePattern();
    const auto pendingBefore = Probe::editor(scenario.host).pendingPattern();
    SequencerEditFacade::PatternSnapshot before, after;
    const auto triggerStart = scenario.triggers.size();
    const auto lockStart = scenario.locks.size();
    const auto applied = applyCopy(scenario, before, after);

    if (timing == CopyTiming::TempoChange)
    {
        const auto oldFrame = scenario.frame;
        scenario.tickAt(oldFrame, scenario.ppq, 140.0);
        scenario.frame = oldFrame + 1; scenario.tempo = 140.0;
        scenario.ppq += 140.0 / (60.0 * 48000.0);
        check("copy survives exact-host tempo transition",
              Probe::scheduler(scenario.host).lastHostDecision().transition == HostTransition::TempoChange);
        if (Probe::editor(scenario.host).pendingPattern() != 1u)
            require("requeue pending pattern after tempo reanchor",
                    Probe::editor(scenario.host).selectPattern(1u) == SequencerEditFacade::Result::Ok);
    }
    else if (timing == CopyTiming::Seek)
    {
        scenario.tickAt(48000, 2.0, 120.0);
        scenario.frame = 48001; scenario.ppq = 2.0 + 1.0 / 24000.0;
        check("copy survives exact-host seek/reanchor",
              Probe::scheduler(scenario.host).lastHostDecision().transition == HostTransition::Seek);
        if (Probe::editor(scenario.host).pendingPattern() != 1u)
            require("requeue pending pattern after seek reanchor",
                    Probe::editor(scenario.host).selectPattern(1u) == SequencerEditFacade::Result::Ok);
    }
    validateCopyResult(name, scenario, before, after, applied,
                       triggerStart, lockStart, afterTransition,
                       requestHostFrame, requestNativeFrame,
                       activeBefore, pendingBefore);
}

void testStaleBusy(IPluginFactory* factory, const TUID id,
                   std::vector<std::uint8_t>& project)
{
    CopyScenario scenario(factory, id);
    require("load stale/busy copy fixture", scenario.host.loadState(project));
    auto& facade = Probe::editor(scenario.host);
    SequencerEditFacade::PatternSnapshot snapshot;
    require("read stale base", facade.readPattern(1u, snapshot) == SequencerEditFacade::Result::Ok);
    SequencerEditFacade::Request copy {};
    copy.operation = SequencerEditFacade::Operation::CopyTrack;
    copy.pattern = 1u; copy.source = 0u; copy.destination = 1u;
    const auto accepted = facade.apply(copy, snapshot.generation);
    require("publish stale precursor", accepted.status == SequencerEditFacade::Result::Ok);
    const auto stateAfterAccepted = scenario.host.saveState();
    const auto stale = facade.apply(copy, snapshot.generation);
    check("stale copy rejected without overwrite",
          stale.status == SequencerEditFacade::Result::Stale &&
          stale.generation == accepted.generation && scenario.host.saveState() == stateAfterAccepted);
    PatternStateAuthority::WriteHandle held;
    require("hold copy authority writer",
            Probe::scheduler(scenario.host).patternAuthority().tryBeginEdit(held) == Status::Ok);
    const auto busy = facade.apply(copy, accepted.generation);
    held.cancel();
    check("busy copy rejected without publication",
          busy.status == SequencerEditFacade::Result::Busy &&
          Probe::scheduler(scenario.host).patternAuthority().generation() == accepted.generation);
    std::printf("P20D_COPY_REJECTION stale=1 busy=1 accepted_generation=%llu final_generation=%llu state_preserved=1\n",
        static_cast<unsigned long long>(accepted.generation),
        static_cast<unsigned long long>(Probe::scheduler(scenario.host).patternAuthority().generation()));
}

void testPatternTransitionCopy(IPluginFactory* factory, const TUID id,
                               HostInstance& source)
{
    auto project = encodedProject(source, transitionBank());
    runCopyTiming(factory, id, project, CopyTiming::Before, "before_transition");
    runCopyTiming(factory, id, project, CopyTiming::LastNative, "last_native_before_transition");
    runCopyTiming(factory, id, project, CopyTiming::TransitionCallback, "transition_callback");
    runCopyTiming(factory, id, project, CopyTiming::After, "immediately_after_transition");
    runCopyTiming(factory, id, project, CopyTiming::TempoChange, "tempo_change");
    runCopyTiming(factory, id, project, CopyTiming::Seek, "seek_reanchor");
    testStaleBusy(factory, id, project);
}
}

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    measuredThread = GetCurrentThreadId();
    const auto module = LoadLibraryA(argv[1]);
    require("load P20-D diagnostic VST3", module != nullptr);
    installAllocationHooks(module);
    const auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
    const auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
    const auto createFactory = reinterpret_cast<IPluginFactory* (*)()>(GetProcAddress(module, "GetPluginFactory"));
    require("initialize P20-D diagnostic VST3", init && exitDll && createFactory && init());
    auto* factory = createFactory();
    PClassInfo info {};
    bool found = false;
    for (int index = 0; index < factory->countClasses(); ++index)
        if (factory->getClassInfo(index, &info) == kResultOk &&
            std::strcmp(info.category, kVstAudioEffectClass) == 0)
        { found = true; break; }
    require("locate P20-D diagnostic component", found);
    {
        HostInstance source(factory, info.cid); source.setup(48000);
        auto project = encodedProject(source, denseBank());
        testPartitions(factory, info.cid, project);
        testOverflowAndIsolation(factory, info.cid, project);
        measureStack(factory, info.cid, project);
        testExactTransportRetention(factory, info.cid, project);
        testPatternTransitionCopy(factory, info.cid, source);
    }
    factory->release();
    require("clean P20-D diagnostic DLL exit", exitDll());
    removeAllocationHooks();
    FreeLibrary(module);
    std::printf("SUMMARY pass=%d fail=%d\n", passed, failed);
    return failed ? 1 : 0;
}
