#include "../dsp/Engine.h"
#include "../dsp/HostTransport.h"
#include "../dsp/LXRAudioAdapter.h"
#include "../dsp/ParameterArray.h"
#include "../dsp/PatternBank.h"
#include "../dsp/TriggerScheduler.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace
{
using namespace lxr;
using namespace lxr::sequencer;

std::uint64_t gPass = 0u;
std::uint64_t gFail = 0u;
std::uint64_t gAllocations = 0u;
bool gMeasureAllocations = false;

void* allocate(std::size_t size)
{
    if (gMeasureAllocations) ++gAllocations;
    if (void* value = std::malloc(size == 0u ? 1u : size)) return value;
    std::abort();
}

void check(bool value, const char* label)
{
    if (value) ++gPass;
    else { ++gFail; std::fprintf(stderr, "FAIL: %s\n", label); }
}

HostTransportContext hostContext(std::int64_t sample, double ppq, double bpm,
                                 std::int32_t frames, bool playing = true)
{
    HostTransportContext value {};
    value.contextPresent = true;
    value.validFields = HostSamplePosition | HostPpqPosition | HostTempo |
                        HostTransportState;
    value.projectSamplePosition = sample;
    value.projectPpqPosition = ppq;
    value.tempoBpm = bpm;
    value.sampleRate = 48000.0;
    value.callbackFrames = frames;
    value.playing = playing;
    value.offline = true;
    return value;
}

template<typename T, std::size_t Capacity>
struct FixedCapture
{
    std::array<T, Capacity> values {};
    std::size_t size = 0u;
    std::size_t dropped = 0u;
    std::size_t peak = 0u;
    void append(const T& value) noexcept
    {
        if (size < values.size()) values[size++] = value;
        else ++dropped;
        if (size > peak) peak = size;
    }
};

struct Trace
{
    FixedCapture<TriggerEvent, 1024> triggers;
    FixedCapture<AutomationEvent, 2048> locks;
    std::uint64_t pcmHash = UINT64_C(1469598103934665603);
    std::uint64_t callbacks = 0u;
    std::uint64_t nativeBlocks = 0u;
    std::uint64_t generatedTriggers = 0u;
    std::uint64_t generatedLocks = 0u;
    std::uint64_t rng = 0u;
    std::uint64_t generation = 0u;
    std::uint64_t allocationDelta = 0u;
    bool nonzero = false;
    bool exactOracle = true;
    bool ordered = true;
};

void hashDouble(Trace& trace, double value)
{
    std::uint64_t bits = 0u;
    std::memcpy(&bits, &value, sizeof(bits));
    for (unsigned byte = 0u; byte < 8u; ++byte)
    {
        trace.pcmHash ^= (bits >> (byte * 8u)) & 0xffu;
        trace.pcmHash *= UINT64_C(1099511628211);
    }
    trace.nonzero = trace.nonzero || value != 0.0;
}

std::uint64_t expectedNativeBoundary(std::uint64_t ordinal,
                                     std::uint64_t bpm) noexcept
{
    // Independent timing oracle. The host timeline admits the event on the
    // first integer host sample at or after its rational 32-PPQ due time. The
    // causal resampler renders a native block when floor(host * Fs/Fh) first
    // demands a sample in that block. Therefore select the first block whose
    // independently computed demand host sample is not earlier than the due
    // host sample. This deliberately handles exact near-boundary cases such as
    // ordinal 280; blindly rounding musical time to a native block is wrong.
    const std::uint64_t dueNumerator = ordinal * 60u * 48000u;
    const std::uint64_t dueDenominator = bpm * 32u;
    const std::uint64_t dueHost =
        (dueNumerator + dueDenominator - 1u) / dueDenominator;
    const std::uint64_t nativeAtDueNumerator = dueHost * UINT64_C(5632353);
    const std::uint64_t nativeAtDueDenominator = 128u * 48000u;
    std::uint64_t block = (nativeAtDueNumerator / nativeAtDueDenominator / 32u) * 32u;
    if (block >= 64u) block -= 64u;
    for (unsigned attempt = 0u; attempt < 6u; ++attempt, block += 32u)
    {
        const std::uint64_t demandNumerator = block * 128u * 48000u;
        const std::uint64_t demandHost =
            (demandNumerator + UINT64_C(5632353) - 1u) / UINT64_C(5632353);
        if (demandHost >= dueHost) return block;
    }
    return block;
}

PatternBank denseBank()
{
    PatternBank bank = MakeDefaultPatternBank();
    for (std::size_t pattern = 0u; pattern < kPatternCount; ++pattern)
    {
        auto& target = bank.patterns[pattern];
        target.nextPattern = static_cast<std::uint8_t>((pattern + 1u) % 2u);
        target.repeat = 0u;
        for (std::size_t track = 0u; track < kTrackCount; ++track)
        {
            target.mainGates[track] = 0xffffu;
            target.lengthCodes[track] = 0u;
            target.rotations[track] = 0u;
            for (std::size_t main = 0u; main < kMainStepCount; ++main)
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

Trace render(unsigned partition, int totalSamples = 384000)
{
    Engine engine;
    LXRAudioAdapter adapter;
    TriggerScheduler scheduler(0x20c0ffeeu);
    ParameterArray parameters;
    Trace trace;
    check(engine.loadAssets("data"), "load retained-trace assets");
    engine.setup808Kit();
    parameters.init();
    parameters.applyAllToEngine(engine);
    check(adapter.configure(48000.0), "configure retained-trace adapter");
    check(scheduler.patternAuthority().tryPublish(denseBank()) == Status::Ok,
          "publish dense retained-trace bank");
    const auto initial = scheduler.diagnostics();
    std::uint64_t previousNative = adapter.nativeFrame();
    std::uint64_t priorOrder = 0u;
    bool haveOrder = false;
    const auto allocationStart = gAllocations;
    gMeasureAllocations = true;
    for (int absolute = 0; absolute < totalSamples; absolute += static_cast<int>(partition))
    {
        const int frames = std::min(static_cast<int>(partition), totalSamples - absolute);
        const double ppq = static_cast<double>(absolute) * 120.0 / (60.0 * 48000.0);
        const auto decision = scheduler.beginHostCallback(
            hostContext(absolute, ppq, 120.0, frames), adapter.nativeFrame(), engine);
        trace.ordered &= decision.usable;
        ++trace.callbacks;
        for (int offset = 0; offset < frames; ++offset)
        {
            double left = 0.0, right = 0.0;
            adapter.getNextHostST1Sample(engine, left, right, &scheduler, offset);
            hashDouble(trace, left);
            hashDouble(trace, right);
            const auto native = adapter.nativeFrame();
            if (native == previousNative) continue;
            trace.ordered &= native == previousNative + 32u;
            for (const auto& event : scheduler.lastBatch())
            {
                const auto expected = expectedNativeBoundary(event.clockOrdinal, 120u);
                if (trace.exactOracle && event.nativeBoundary != expected)
                    std::fprintf(stderr,
                        "ORACLE trigger ordinal=%llu native=%llu expected=%llu pattern=%u track=%u position=%u\n",
                        static_cast<unsigned long long>(event.clockOrdinal),
                        static_cast<unsigned long long>(event.nativeBoundary),
                        static_cast<unsigned long long>(expected), unsigned(event.pattern),
                        unsigned(event.track), unsigned(event.physicalPosition));
                trace.exactOracle &= event.nativeBoundary == expected;
                trace.ordered &= !haveOrder || event.stableOrder > priorOrder;
                priorOrder = event.stableOrder;
                haveOrder = true;
                trace.triggers.append(event);
            }
            for (const auto& event : scheduler.lastAutomationBatch())
            {
                const auto expected = expectedNativeBoundary(event.clockOrdinal, 120u);
                if (trace.exactOracle && event.nativeBoundary != expected)
                    std::fprintf(stderr,
                        "ORACLE lock ordinal=%llu native=%llu expected=%llu pattern=%u track=%u position=%u\n",
                        static_cast<unsigned long long>(event.clockOrdinal),
                        static_cast<unsigned long long>(event.nativeBoundary),
                        static_cast<unsigned long long>(expected), unsigned(event.pattern),
                        unsigned(event.track), unsigned(event.physicalPosition));
                trace.exactOracle &= event.nativeBoundary == expected;
                trace.locks.append(event);
            }
            previousNative = native;
        }
    }
    gMeasureAllocations = false;
    trace.allocationDelta = gAllocations - allocationStart;
    const auto final = scheduler.diagnostics();
    trace.nativeBlocks = final.processedNativeBlocks - initial.processedNativeBlocks;
    trace.generatedTriggers = final.emittedEvents - initial.emittedEvents;
    trace.generatedLocks = final.emittedAutomationEvents - initial.emittedAutomationEvents;
    trace.rng = scheduler.rngState();
    trace.generation = scheduler.patternAuthority().generation();
    return trace;
}

bool sameTrigger(const TriggerEvent& a, const TriggerEvent& b) noexcept
{
    return a.nativeBoundary == b.nativeBoundary && a.clockOrdinal == b.clockOrdinal &&
        a.snapshotGeneration == b.snapshotGeneration && a.stableOrder == b.stableOrder &&
        a.pattern == b.pattern && a.track == b.track &&
        a.physicalPosition == b.physicalPosition && a.note == b.note &&
        a.velocity == b.velocity && a.kind == b.kind;
}

bool sameLock(const AutomationEvent& a, const AutomationEvent& b) noexcept
{
    return a.nativeBoundary == b.nativeBoundary && a.clockOrdinal == b.clockOrdinal &&
        a.snapshotGeneration == b.snapshotGeneration && a.stableOrder == b.stableOrder &&
        a.triggerOrder == b.triggerOrder && a.pattern == b.pattern &&
        a.track == b.track && a.physicalPosition == b.physicalPosition &&
        a.slot == b.slot && a.command.action == b.command.action &&
        a.command.target.value() == b.command.target.value() &&
        a.command.value.value() == b.command.value.value();
}

void testPartitions()
{
    constexpr std::array<unsigned, 11> partitions {{1,16,31,32,33,64,127,128,256,512,1024}};
    const Trace reference = render(partitions[0]);
    check(reference.generatedTriggers == reference.triggers.size &&
          reference.generatedLocks == reference.locks.size,
          "one-frame trace retains every generated record");
    check(reference.triggers.dropped == 0u && reference.locks.dropped == 0u,
          "qualification capacity does not overflow");
    check(reference.exactOracle && reference.ordered,
          "independent zero-tolerance native timing and order oracle");
    check(reference.allocationDelta == 0u, "qualification render allocations are zero");
    for (const unsigned partition : partitions)
    {
        const Trace value = partition == 1u ? reference : render(partition);
        bool triggers = value.triggers.size == reference.triggers.size;
        bool locks = value.locks.size == reference.locks.size;
        for (std::size_t i = 0u; triggers && i < value.triggers.size; ++i)
            triggers &= sameTrigger(value.triggers.values[i], reference.triggers.values[i]);
        for (std::size_t i = 0u; locks && i < value.locks.size; ++i)
            locks &= sameLock(value.locks.values[i], reference.locks.values[i]);
        check(value.pcmHash == reference.pcmHash && value.nonzero,
              "partition exact PCM identity");
        check(triggers && locks && value.generatedTriggers == reference.generatedTriggers &&
              value.generatedLocks == reference.generatedLocks,
              "partition complete trigger and lock identity");
        check(value.rng == reference.rng && value.generation == reference.generation,
              "partition RNG and pattern generation identity");
        check(value.exactOracle && value.ordered && value.allocationDelta == 0u,
              "partition timing/order/allocation qualification");
        std::printf("NATIVE_PARTITION frames=%u callbacks=%llu duration_frames=384000 native_blocks=%llu generated_triggers=%llu retained_triggers=%zu generated_locks=%llu retained_locks=%zu dropped=%zu peak_occupancy=%zu drain_frequency=per_native_block pcm_hash=%llu rng=%llu generation=%llu tolerance_native_frames=0 allocations=%llu\n",
                    partition, static_cast<unsigned long long>(value.callbacks),
                    static_cast<unsigned long long>(value.nativeBlocks),
                    static_cast<unsigned long long>(value.generatedTriggers), value.triggers.size,
                    static_cast<unsigned long long>(value.generatedLocks), value.locks.size,
                    value.triggers.dropped + value.locks.dropped,
                    value.triggers.peak + value.locks.peak,
                    static_cast<unsigned long long>(value.pcmHash),
                    static_cast<unsigned long long>(value.rng),
                    static_cast<unsigned long long>(value.generation),
                    static_cast<unsigned long long>(value.allocationDelta));
    }

    FixedCapture<unsigned, 2> overflow;
    for (unsigned value = 0u; value < 5u; ++value) overflow.append(value);
    check(overflow.size == 2u && overflow.dropped == 3u &&
          overflow.values[0] == 0u && overflow.values[1] == 1u,
          "forced overflow drops new records and accounts exactly");
    std::printf("FORCED_OVERFLOW generated=5 retained=%zu dropped=%zu peak=%zu\n",
                overflow.size, overflow.dropped, overflow.peak);
}

void transition(const char* name, HostTransition actual,
                HostTransition expected, bool reanchor)
{
    check(actual == expected, name);
    std::printf("TRANSITION name=%s observed=%u expected=%u reanchor=%u\n",
                name, static_cast<unsigned>(actual), static_cast<unsigned>(expected),
                reanchor ? 1u : 0u);
}

void testTransitions()
{
    HostTransportTimeline timeline;
    auto stopped = timeline.beginCallback(hostContext(0, 0.0, 120.0, 32, false), 0u);
    transition("initial_stopped", stopped.transition, HostTransition::None, stopped.reanchor);
    auto started = timeline.beginCallback(hostContext(0, 0.0, 120.0, 31, true), 0u);
    transition("stopped_to_playing", started.transition, HostTransition::Start, started.reanchor);
    auto stop = timeline.beginCallback(hostContext(31, 31.0 / 24000.0, 120.0, 33, false), 0u);
    transition("playing_to_stopped", stop.transition, HostTransition::Stop, stop.reanchor);
    auto tempoStopped = timeline.beginCallback(hostContext(31, 31.0 / 24000.0, 90.0, 64, false), 0u);
    transition("tempo_change_stopped", tempoStopped.transition, HostTransition::None, tempoStopped.reanchor);
    auto resumed = timeline.beginCallback(hostContext(31, 31.0 / 24000.0, 90.0, 16, true), 0u);
    transition("stop_restart", resumed.transition, HostTransition::Resume, resumed.reanchor);
    auto tempoPlaying = timeline.beginCallback(hostContext(47,
        31.0 / 24000.0 + 16.0 * 90.0 / (60.0 * 48000.0), 140.0, 16, true), 0u);
    transition("tempo_change_playing", tempoPlaying.transition,
               HostTransition::TempoChange, tempoPlaying.reanchor);
    auto forward = timeline.beginCallback(hostContext(96000, 4.0, 140.0, 127, true), 0u);
    transition("seek_forward", forward.transition, HostTransition::Seek, forward.reanchor);
    auto backward = timeline.beginCallback(hostContext(48000, 2.0, 140.0, 128, true), 0u);
    transition("seek_backward", backward.transition, HostTransition::Seek, backward.reanchor);

    HostTransportTimeline loop;
    auto nearEnd = hostContext(0, 3.99, 120.0, 240, true);
    nearEnd.validFields |= HostCycle;
    nearEnd.cycleActive = true;
    nearEnd.cycleStartPpq = 0.0;
    nearEnd.cycleEndPpq = 4.0;
    (void)loop.beginCallback(nearEnd, 0u);
    auto wrapped = hostContext(0, 0.0, 120.0, 32, true);
    wrapped.validFields |= HostCycle;
    wrapped.cycleActive = true;
    wrapped.cycleStartPpq = 0.0;
    wrapped.cycleEndPpq = 4.0;
    const auto looped = loop.beginCallback(wrapped, 0u);
    transition("loop_wrap", looped.transition, HostTransition::Loop, looped.reanchor);

    Engine engine;
    ParameterArray parameters;
    TriggerScheduler scheduler;
    parameters.init();
    parameters.applyAllToEngine(engine);
    check(scheduler.patternAuthority().tryPublish(denseBank()) == Status::Ok,
          "publish transition bank");
    (void)scheduler.beginHostCallback(hostContext(0, 0.0, 120.0, 64, true), 0u, engine);
    check(scheduler.hostSync(), "host clock transition enters host sync");
    check(scheduler.start() == Status::Ok && !scheduler.hostSync(),
          "host to internal clock transition resets host epoch");
    check(scheduler.selectPattern(1u) == Status::Ok &&
          scheduler.pendingPattern() == 1u && scheduler.selectedPattern() == 0u,
          "running selection remains pending until pattern boundary");
    for (std::uint64_t native = 0u; native < 200000u &&
         scheduler.selectedPattern() == 0u; native += 32u)
        (void)scheduler.processNativeBlock(native, engine);
    check(scheduler.selectedPattern() == 1u &&
          scheduler.diagnostics().patternTransitions != 0u,
          "pending pattern activates on source-defined boundary");
    std::printf("TRANSITION name=pattern_transition observed=%u pending=%u transitions=%llu\n",
                unsigned(scheduler.selectedPattern()), unsigned(scheduler.pendingPattern()),
                static_cast<unsigned long long>(scheduler.diagnostics().patternTransitions));

    HostTransportTimeline nearBoundary;
    auto first = nearBoundary.beginCallback(hostContext(0, 0.0, 120.0, 31, true), 0u);
    auto second = nearBoundary.beginCallback(hostContext(31, 31.0 / 24000.0, 120.0, 33, true), 0u);
    auto reanchor = nearBoundary.beginCallback(hostContext(6000, 0.25, 120.0, 31, true), 0u);
    check(first.transition == HostTransition::Start && second.transition == HostTransition::None &&
          reanchor.transition == HostTransition::Seek && reanchor.reanchor,
          "repeated transport changes near native boundaries are deterministic");
    std::printf("TRANSITION name=near_native_boundary observed=%u expected=%u reanchor=%u\n",
                static_cast<unsigned>(reanchor.transition),
                static_cast<unsigned>(HostTransition::Seek), reanchor.reanchor ? 1u : 0u);
}
}

void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

int main()
{
    testPartitions();
    testTransitions();
    std::printf("SUMMARY pass=%llu fail=%llu processing_allocations=%llu trace_stack_bytes=%zu\n",
                static_cast<unsigned long long>(gPass),
                static_cast<unsigned long long>(gFail),
                static_cast<unsigned long long>(gAllocations), sizeof(Trace));
    return gFail == 0u ? 0 : 1;
}
