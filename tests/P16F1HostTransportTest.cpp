#include "../dsp/Engine.h"
#include "../dsp/HostTransport.h"
#include "../dsp/LXRAudioAdapter.h"
#include "../dsp/ParameterArray.h"
#include "../dsp/PatternBank.h"
#include "../dsp/TriggerScheduler.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <type_traits>

namespace
{
std::uint64_t gAllocations = 0u;
bool gMeasureAllocations = false;

void* CountAllocation(std::size_t size)
{
    if (gMeasureAllocations)
        ++gAllocations;
    if (void* value = std::malloc(size == 0u ? 1u : size))
        return value;
    std::abort();
}
}

void* operator new(std::size_t size) { return CountAllocation(size); }
void* operator new[](std::size_t size) { return CountAllocation(size); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

namespace
{
using namespace lxr;
using namespace lxr::sequencer;

std::uint64_t gPass = 0u;
std::uint64_t gFail = 0u;

void check(bool condition, const char* label)
{
    if (condition)
        ++gPass;
    else
    {
        ++gFail;
        std::cerr << "FAIL: " << label << '\n';
    }
}

template<class A, class B>
void equal(const A& actual, const B& expected, const char* label)
{
    const A converted = static_cast<A>(expected);
    if (actual == converted)
        ++gPass;
    else
    {
        ++gFail;
        std::cerr << "FAIL: " << label;
        if constexpr (std::is_integral_v<A>)
            std::cerr << " actual=" << actual << " expected=" << converted;
        std::cerr << '\n';
    }
}

HostTransportContext context(std::int64_t sample, double ppq, double bpm,
                             std::int32_t frames, bool playing = true)
{
    HostTransportContext result {};
    result.contextPresent = true;
    result.validFields = HostSamplePosition | HostPpqPosition | HostTempo |
                         HostTransportState;
    result.projectSamplePosition = sample;
    result.projectPpqPosition = ppq;
    result.tempoBpm = bpm;
    result.sampleRate = 48000.0;
    result.callbackFrames = frames;
    result.playing = playing;
    return result;
}

struct Trace
{
    std::array<std::int64_t, 64> ordinals {};
    std::array<std::int64_t, 64> samples {};
    std::size_t size = 0u;
};

Trace renderTrace(const std::array<int, 8>& partitions, int totalSamples,
                  double bpm, std::uint8_t shuffle = 0u)
{
    HostTransportTimeline timeline;
    Trace trace;
    int absoluteSample = 0;
    std::size_t partition = 0u;
    while (absoluteSample < totalSamples)
    {
        int frames = partitions[partition++ % partitions.size()];
        if (frames <= 0)
            frames = 1;
        if (absoluteSample + frames > totalSamples)
            frames = totalSamples - absoluteSample;
        const double ppq = static_cast<double>(absoluteSample) * bpm /
                           (60.0 * 48000.0);
        const auto decision = timeline.beginCallback(
            context(absoluteSample, ppq, bpm, frames), shuffle);
        check(decision.usable, "trace callback usable");
        for (int offset = 0; offset < frames; ++offset)
        {
            std::int64_t musical = 0;
            std::uint64_t event = 0u;
            if (timeline.popDueBoundary(offset, shuffle, musical, event))
            {
                check(trace.size < trace.ordinals.size(), "trace capacity");
                if (trace.size < trace.ordinals.size())
                {
                    trace.ordinals[trace.size] = musical;
                    trace.samples[trace.size] = absoluteSample + offset;
                    ++trace.size;
                }
            }
        }
        absoluteSample += frames;
    }
    return trace;
}

void testArithmeticAndGrouping()
{
    HostTransportTimeline endpoint;
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto minimum = std::numeric_limits<std::int64_t>::min();
    check(endpoint.beginCallback(context(maximum - 1, 0, 120, 1), 0).usable,
          "signed sample endpoint starts");
    check(endpoint.beginCallback(context(maximum, 2.0 / 48000, 120, 1), 0)
              .transition == HostTransition::None,
          "representable maximum sample continuity");
    const auto wrapped = endpoint.beginCallback(
        context(minimum, 4.0 / 48000, 120, 1), 0);
    check(wrapped.usable && wrapped.transition == HostTransition::Seek,
          "unrepresentable sample endpoint is bounded seek without signed wrap");
    std::int64_t value = 0;
    check(HostTransportTimeline::ppqToQ32(1.0, value), "one PPQ accepted");
    equal(value, HostTransportTimeline::kPpqOne, "one PPQ Q32");
    check(!HostTransportTimeline::ppqToQ32(1000001.0, value),
          "overflow-range PPQ rejected");
    equal(HostTransportTimeline::boundaryPpqQ32(32, 0u),
          HostTransportTimeline::kPpqOne, "32 positions per quarter");
    equal(HostTransportTimeline::firstBoundaryAtOrAfter(
              -HostTransportTimeline::kPpqOne / 64, 0u),
          0, "negative preroll anchors at next boundary");
    equal(HostTransportTimeline::boundaryPpqQ32(16, 127u),
          HostTransportTimeline::kPpqOne / 2, "shuffle cycle has no drift");
    check(HostTransportTimeline::boundaryPpqQ32(8, 127u) >
          HostTransportTimeline::boundaryPpqQ32(8, 0u),
          "shuffle delays phase peak");

    const std::array<int, 8> single {{1,1,1,1,1,1,1,1}};
    const std::array<int, 8> variable {{127,1,512,32,64,256,17,3}};
    const Trace a = renderTrace(single, 6000, 120.0);
    const Trace b = renderTrace(variable, 6000, 120.0);
    equal(a.size, 8u, "120 BPM event count");
    equal(a.size, b.size, "partition invariant count");
    for (std::size_t i = 0; i < a.size; ++i)
    {
        equal(a.ordinals[i], static_cast<std::int64_t>(i),
              "ordinal sequence");
        equal(a.ordinals[i], b.ordinals[i], "partition invariant ordinal");
        equal(a.samples[i], b.samples[i], "partition invariant sample");
        equal(a.samples[i], static_cast<std::int64_t>(i * 750u),
              "independent 120 BPM sample expectation");
    }
    const Trace slow = renderTrace(variable, 6000, 60.0);
    equal(slow.size, 4u, "60 BPM event count");
    for (std::size_t i = 0; i < slow.size; ++i)
        equal(slow.samples[i], static_cast<std::int64_t>(i * 1500u),
              "independent 60 BPM sample expectation");
}

void testTransitionsAndFallbacks()
{
    HostTransportTimeline timeline;
    auto first = timeline.beginCallback(context(0, 0.0, 120.0, 512), 0u);
    equal(first.transition, HostTransition::Start, "first play starts");
    check(first.reanchor, "first play reanchors");
    std::int64_t ordinal = -1;
    std::uint64_t event = 99u;
    check(timeline.popDueBoundary(0, 0u, ordinal, event), "start boundary due");
    equal(ordinal, 0, "start ordinal zero");
    check(!timeline.popDueBoundary(1, 0u, ordinal, event), "no duplicate start");

    const double endPpq = 512.0 / 24000.0;
    auto stopped = timeline.beginCallback(context(512, endPpq, 120.0, 64, false), 0u);
    equal(stopped.transition, HostTransition::Stop, "play to stop");
    check(!timeline.popDueBoundary(0, 0u, ordinal, event), "stop emits nothing");
    auto resumed = timeline.beginCallback(context(512, endPpq, 120.0, 512, true), 0u);
    equal(resumed.transition, HostTransition::Resume, "same-position resume");
    check(!resumed.reanchor, "resume preserves epoch");

    auto stoppedSeek = timeline.beginCallback(
        context(96000, 2.0, 120.0, 64, false), 0u);
    equal(stoppedSeek.transition, HostTransition::Seek,
          "reposition while stopped detected");
    check(stoppedSeek.reanchor && !stoppedSeek.playing,
          "stopped seek reanchors without running");
    auto startAtStoppedSeek = timeline.beginCallback(
        context(96000, 2.0, 120.0, 512, true), 0u);
    equal(startAtStoppedSeek.transition, HostTransition::Resume,
          "play after stopped seek uses preanchored phase");

    auto seeked = timeline.beginCallback(context(192000, 2.5, 120.0, 512), 0u);
    equal(seeked.transition, HostTransition::Seek, "forward seek detected");
    check(seeked.reanchor, "seek reanchors");
    equal(seeked.nextTimelineOrdinal, 80, "seek absolute phase");

    HostTransportTimeline loopTimeline;
    auto nearEnd = context(0, 3.9, 120.0, 2400);
    nearEnd.validFields |= HostCycle;
    nearEnd.cycleActive = true;
    nearEnd.cycleStartPpq = 0.0;
    nearEnd.cycleEndPpq = 4.0;
    (void)loopTimeline.beginCallback(nearEnd, 0u);
    auto wrappedContext = context(0, 0.0, 120.0, 512);
    wrappedContext.validFields |= HostCycle;
    wrappedContext.cycleActive = true;
    wrappedContext.cycleStartPpq = 0.0;
    wrappedContext.cycleEndPpq = 4.0;
    const auto wrapped = loopTimeline.beginCallback(wrappedContext, 0u);
    equal(wrapped.transition, HostTransition::Loop, "loop wrap detected");
    check(wrapped.reanchor, "loop reanchors deterministic epoch");

    HostTransportTimeline tempoTimeline;
    (void)tempoTimeline.beginCallback(context(0, 0.0, 120.0, 480), 0u);
    auto changed = tempoTimeline.beginCallback(context(480, 0.02, 60.0, 480), 0u);
    equal(changed.transition, HostTransition::TempoChange,
          "callback-boundary tempo change");
    check(!changed.reanchor, "tempo change retains PPQ phase");

    for (double newTempo : {60.0, 240.0})
    {
        HostTransportTimeline insideMarker;
        (void)insideMarker.beginCallback(context(0,0.0,120.0,480),0u);
        // First half of preceding callback at120, second at the new BPM.
        const double integrated = 240.0 / 24000.0 +
            240.0 * newTempo / (48000.0 * 60.0);
        const auto correction=insideMarker.beginCallback(
            context(480,integrated,newTempo,480),0u);
        equal(correction.transition,HostTransition::TempoChange,
              "intra-callback marker correction is not a seek");
        check(!correction.reanchor,"tempo integral correction preserves history");
        const auto relocated=insideMarker.beginCallback(
            context(960,integrated+4.0,newTempo==60.0?120.0:60.0,480),0u);
        equal(relocated.transition,HostTransition::Seek,
              "tempo change outside bounded integral still seeks");
    }

    HostTransportTimeline fallback;
    (void)fallback.beginCallback(context(0, 0.0, 120.0, 64), 0u);
    auto noPpq = context(64, 64.0 / 24000.0, 120.0, 64);
    noPpq.validFields &= ~HostPpqPosition;
    const auto ppqFallback = fallback.beginCallback(noPpq, 0u);
    check(ppqFallback.usable && ppqFallback.usedPpqFallback,
          "continuous sample fallback for missing PPQ");
    auto noTempo = context(128, 128.0 / 24000.0, 120.0, 64);
    noTempo.validFields &= ~HostTempo;
    const auto tempoFallback = fallback.beginCallback(noTempo, 0u);
    check(tempoFallback.usable && tempoFallback.usedTempoFallback,
          "previous validated tempo fallback");
    auto noSample = context(192, 192.0 / 24000.0, 120.0, 64);
    noSample.validFields &= ~HostSamplePosition;
    check(fallback.beginCallback(noSample, 0u).usable,
          "PPQ-authoritative callback without sample position");

    HostTransportTimeline invalid;
    auto bad = context(0, 0.0, 0.0, 64);
    bad.validFields &= ~HostTempo;
    equal(invalid.beginCallback(bad, 0u).transition,
          HostTransition::InvalidContext, "missing initial tempo rejected");
    bad = context(0, 0.0, 120.0, 64);
    bad.contextPresent = false;
    equal(invalid.beginCallback(bad, 0u).transition,
          HostTransition::InvalidContext, "missing context is not zero");
}

StepData activeStep(std::uint8_t note = 63u)
{
    StepData step = MakeDefaultStep(false);
    SetStepActive(step, true);
    step.note = note;
    return step;
}

void testSchedulerPhaseLocksAndAllocation()
{
    PatternBank bank = MakeDefaultPatternBank();
    for (auto& pattern : bank.patterns)
        for (auto& gate : pattern.mainGates)
            gate = 0u;
    bank.patterns[0].mainGates[0] = 1u;
    bank.patterns[0].mainGates[0] |= static_cast<std::uint16_t>(1u << 10u);
    bank.patterns[0].steps[0][0] = activeStep(60u);
    bank.patterns[0].steps[0][80] = activeStep(70u);
    bank.patterns[0].steps[0][80].parameter1 = 89u; // VOL1/PAR 88.
    bank.patterns[0].steps[0][80].value1 = 32u;

    TriggerScheduler scheduler(0x12345678u);
    check(scheduler.patternAuthority().tryPublish(bank) == Status::Ok,
          "publish host pattern");
    Engine engine;
    ParameterArray parameters;
    parameters.init();
    parameters.applyAllToEngine(engine);

    auto atNonzero = context(120000, 2.5, 120.0, 512);
    const auto decision = scheduler.beginHostCallback(atNonzero, 0u, engine);
    equal(decision.nextTimelineOrdinal, 80, "scheduler nonzero phase anchor");
    gAllocations = 0u;
    gMeasureAllocations = true;
    const auto status = scheduler.processNativeBlock(0u, engine,
        PatternStateAuthority::kDefaultAcquireAttempts, 0);
    gMeasureAllocations = false;
    equal(status, Status::Ok, "host scheduler block status");
    equal(gAllocations, 0u, "host scheduler processing allocations");
    equal(scheduler.lastBatch().size(), 1u, "nonzero phase trigger count");
    equal(scheduler.lastBatch()[0].physicalPosition, 80u,
          "nonzero phase physical position");
    equal(scheduler.lastBatch()[0].note, 70u, "nonzero phase note");
    check(engine.sequencerAutomationDestination(0u, 0u).valid(),
          "parameter lock applied before host trigger");

    const double stoppedPpq = 2.5 + 512.0 / 24000.0;
    const auto stopped = scheduler.beginHostCallback(
        context(120512, stoppedPpq, 120.0, 64, false), 32u, engine);
    equal(stopped.transition, HostTransition::Stop, "scheduler host stop");
    check(!engine.sequencerAutomationDestination(0u, 0u).valid(),
          "host stop clears temporary lock");
    check(scheduler.lastBatch().empty(), "host stop clears pending batch");

    const auto resumed = scheduler.beginHostCallback(
        context(120512, stoppedPpq, 120.0, 512, true), 32u, engine);
    equal(resumed.transition, HostTransition::Resume, "scheduler resume");
    check(!resumed.reanchor, "scheduler resume does not restart");
}

struct RenderDigest
{
    std::uint64_t hash = UINT64_C(1469598103934665603);
    std::uint64_t events = 0u;
    TriggerSchedulerDiagnostics diagnostics {};
    bool nonzero = false;
};

void hashSample(RenderDigest& result, double left, double right)
{
    std::uint64_t bits[2] {};
    std::memcpy(&bits[0], &left, sizeof(left));
    std::memcpy(&bits[1], &right, sizeof(right));
    for (const std::uint64_t value : bits)
        for (unsigned byte = 0u; byte < 8u; ++byte)
        {
            result.hash ^= (value >> (byte * 8u)) & 0xffu;
            result.hash *= UINT64_C(1099511628211);
        }
    result.nonzero = result.nonzero || left != 0.0 || right != 0.0;
}

PatternBank audibleBank(bool locked = false, bool randomChain = false)
{
    PatternBank bank = MakeDefaultPatternBank();
    for (std::size_t pattern = 0; pattern < kPatternCount; ++pattern)
    {
        bank.patterns[pattern].mainGates[0] = 0xffffu;
        for (std::size_t position = 0; position < kPhysicalPositionCount;
             ++position)
        {
            auto& step = bank.patterns[pattern].steps[0][position];
            SetStepActive(step, true);
            step.note = static_cast<std::uint8_t>(48u + pattern * 4u);
            step.probability = 127u;
        }
        if (randomChain)
        {
            bank.patterns[pattern].lengthCodes[0] = 1u;
            bank.patterns[pattern].nextPattern = pattern == 0u ? 8u
                                                               : pattern;
        }
    }
    if (locked)
    {
        bank.patterns[0].steps[0][0].parameter1 = 89u; // D1 volume.
        bank.patterns[0].steps[0][0].value1 = 16u;
    }
    return bank;
}

bool prepareRender(Engine& engine, LXRAudioAdapter& adapter,
                   TriggerScheduler& scheduler, const PatternBank& bank)
{
    if (!engine.loadAssets("data"))
        return false;
    engine.setup808Kit();
    ParameterArray parameters;
    parameters.init();
    parameters.applyAllToEngine(engine);
    return adapter.configure(48000.0) &&
           scheduler.patternAuthority().tryPublish(bank) == Status::Ok;
}

RenderDigest renderConstantHost(const std::array<int, 8>& partitions,
                                int totalSamples, double bpm,
                                std::uint8_t shuffle = 0u,
                                bool locked = false,
                                std::uint32_t seed = 0x12345678u,
                                bool randomChain = false)
{
    Engine engine;
    LXRAudioAdapter adapter;
    TriggerScheduler scheduler(seed);
    RenderDigest result;
    check(prepareRender(engine, adapter, scheduler,
                        audibleBank(locked, randomChain)),
          "prepare host PCM render");
    check(scheduler.setShuffle(shuffle) == Status::Ok,
          "set host PCM shuffle");
    int absoluteSample = 0;
    std::size_t partition = 0u;
    while (absoluteSample < totalSamples)
    {
        int frames = partitions[partition++ % partitions.size()];
        if (frames <= 0)
            frames = 1;
        if (absoluteSample + frames > totalSamples)
            frames = totalSamples - absoluteSample;
        auto host = context(absoluteSample,
            static_cast<double>(absoluteSample) * bpm / (60.0 * 48000.0),
            bpm, frames);
        host.offline = true;
        (void)scheduler.beginHostCallback(host, adapter.nativeFrame(), engine);
        for (int offset = 0; offset < frames; ++offset)
        {
            double left = 0.0;
            double right = 0.0;
            adapter.getNextHostST1Sample(
                engine, left, right, &scheduler, offset);
            hashSample(result, left, right);
        }
        absoluteSample += frames;
    }
    result.diagnostics = scheduler.diagnostics();
    result.events = result.diagnostics.emittedEvents;
    return result;
}

RenderDigest renderInternal(int totalSamples)
{
    Engine engine;
    LXRAudioAdapter adapter;
    TriggerScheduler scheduler(0x12345678u);
    RenderDigest result;
    check(prepareRender(engine, adapter, scheduler, audibleBank()),
          "prepare internal PCM render");
    check(scheduler.setTempo(120u) == Status::Ok &&
          scheduler.start() == Status::Ok, "start internal PCM render");
    for (int sample = 0; sample < totalSamples; ++sample)
    {
        double left = 0.0;
        double right = 0.0;
        adapter.getNextHostST1Sample(engine, left, right, &scheduler, -1);
        hashSample(result, left, right);
    }
    result.diagnostics = scheduler.diagnostics();
    result.events = result.diagnostics.emittedEvents;
    return result;
}

RenderDigest renderTransportScenario()
{
    Engine engine;
    LXRAudioAdapter adapter;
    TriggerScheduler scheduler(0x55aa11eeu);
    RenderDigest result;
    check(prepareRender(engine, adapter, scheduler, audibleBank()),
          "prepare transport scenario");
    std::int64_t projectSample = 0;
    double ppq = 0.0;
    double bpm = 120.0;
    bool playing = true;
    for (int outputSample = 0; outputSample < 8192; outputSample += 64)
    {
        if (outputSample == 2048)
            playing = false;
        else if (outputSample == 2560)
            playing = true;
        else if (outputSample == 4096)
        {
            projectSample = 96000;
            ppq = 4.0;
        }
        else if (outputSample == 6144)
        {
            projectSample = 0;
            ppq = 0.0;
        }
        else if (outputSample == 7168)
            bpm = 60.0;

        auto host = context(projectSample, ppq, bpm, 64, playing);
        host.offline = true;
        if (outputSample == 6144)
        {
            host.validFields |= HostCycle;
            host.cycleActive = true;
            host.cycleStartPpq = 0.0;
            host.cycleEndPpq = 4.0;
        }
        (void)scheduler.beginHostCallback(host, adapter.nativeFrame(), engine);
        for (int offset = 0; offset < 64; ++offset)
        {
            double left = 0.0;
            double right = 0.0;
            adapter.getNextHostST1Sample(
                engine, left, right, &scheduler, offset);
            hashSample(result, left, right);
        }
        if (playing)
        {
            projectSample += 64;
            ppq += 64.0 * bpm / (60.0 * 48000.0);
        }
    }
    result.diagnostics = scheduler.diagnostics();
    result.events = result.diagnostics.emittedEvents;
    return result;
}

void testNativePcmFixtures()
{
    const std::array<int, 8> one {{1,1,1,1,1,1,1,1}};
    const std::array<int, 8> variable {{127,1,512,32,64,256,17,3}};
    const auto internalA = renderInternal(4096);
    const auto internalB = renderInternal(4096);
    check(internalA.nonzero && internalA.hash == internalB.hash,
          "internal-clock PCM baseline deterministic");

    const auto host120A = renderConstantHost(one, 4096, 120.0);
    const auto host120B = renderConstantHost(variable, 4096, 120.0);
    check(host120A.nonzero && host120A.events != 0u,
          "host-sync 120 BPM audible step PCM");
    check(host120A.hash == host120B.hash &&
          host120A.events == host120B.events,
          "variable callback grouping preserves exact PCM and events");
    const auto host60 = renderConstantHost(variable, 4096, 60.0);
    check(host60.nonzero && host60.events < host120A.events &&
          host60.hash != host120A.hash,
          "60 versus 120 BPM PCM spacing differs");

    const auto transportA = renderTransportScenario();
    const auto transportB = renderTransportScenario();
    check(transportA.nonzero && transportA.hash == transportB.hash,
          "start stop resume seek loop tempo PCM repeatable");
    check(transportA.diagnostics.hostStarts == 1u &&
          transportA.diagnostics.hostStops == 1u &&
          transportA.diagnostics.hostResumes == 1u &&
          transportA.diagnostics.hostSeeks >= 1u &&
          transportA.diagnostics.hostLoops == 1u &&
          transportA.diagnostics.hostTempoChanges == 1u,
          "transport scenario exercised every transition");

    const auto straight = renderConstantHost(variable, 4096, 120.0, 0u);
    const auto shuffled = renderConstantHost(variable, 4096, 120.0, 127u);
    check(straight.nonzero && shuffled.nonzero &&
          straight.hash != shuffled.hash,
          "shuffle changes audible timing without losing playback");

    const auto unlocked = renderConstantHost(variable, 4096, 120.0);
    const auto locked = renderConstantHost(variable, 4096, 120.0, 0u, true);
    check(unlocked.nonzero && locked.nonzero && unlocked.hash != locked.hash,
          "parameter lock audibly changes a valid target");

    const auto randomA = renderConstantHost(
        variable, 20000, 120.0, 0u, false, 0x77u, true);
    const auto randomB = renderConstantHost(
        one, 20000, 120.0, 0u, false, 0x77u, true);
    check(randomA.nonzero && randomA.hash == randomB.hash &&
          randomA.diagnostics.randomPatternSelections != 0u,
          "random-next PCM deterministic across callback grouping");

    Engine midiA;
    Engine midiB;
    LXRAudioAdapter adapterA;
    LXRAudioAdapter adapterB;
    TriggerScheduler disabled;
    check(midiA.loadAssets("data") && midiB.loadAssets("data") &&
          adapterA.configure(48000.0) && adapterB.configure(48000.0),
          "prepare MIDI-only PCM");
    midiA.setup808Kit();
    midiB.setup808Kit();
    midiA.triggerDrum(0u, 111u, 62u);
    midiB.triggerDrum(0u, 111u, 62u);
    RenderDigest midiWithDisabled;
    RenderDigest midiWithoutScheduler;
    for (int sample = 0; sample < 2048; ++sample)
    {
        double leftA = 0.0, rightA = 0.0;
        double leftB = 0.0, rightB = 0.0;
        adapterA.getNextHostST1Sample(
            midiA, leftA, rightA, &disabled, -1);
        adapterB.getNextHostST1Sample(
            midiB, leftB, rightB, nullptr, -1);
        hashSample(midiWithDisabled, leftA, rightA);
        hashSample(midiWithoutScheduler, leftB, rightB);
    }
    check(midiWithDisabled.nonzero &&
          midiWithDisabled.hash == midiWithoutScheduler.hash,
          "external MIDI-only PCM identical with sequencing disabled");

    const auto isolatedA = renderConstantHost(variable, 4096, 120.0,
                                               0u, false, 0x111u);
    (void)renderConstantHost(variable, 4096, 90.0,
                            127u, true, 0x999u);
    const auto isolatedB = renderConstantHost(variable, 4096, 120.0,
                                               0u, false, 0x111u);
    check(isolatedA.hash == isolatedB.hash &&
          isolatedA.events == isolatedB.events,
          "separate render instances remain isolated");
}

} // namespace

int main()
{
    testArithmeticAndGrouping();
    testTransitionsAndFallbacks();
    testSchedulerPhaseLocksAndAllocation();
    testNativePcmFixtures();
    std::cout << "{\"pass\":" << gPass << ",\"fail\":" << gFail
              << ",\"processing_allocations\":" << gAllocations << "}\n";
    return gFail == 0u ? 0 : 1;
}
