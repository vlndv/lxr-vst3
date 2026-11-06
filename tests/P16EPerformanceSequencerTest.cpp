#include "../dsp/Engine.h"
#include "../dsp/ParameterArray.h"
#include "../dsp/PatternBank.h"
#include "../dsp/TriggerScheduler.h"
#include "reference/sequencer/original_performance_adapter.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <thread>

namespace
{

std::atomic<std::uint64_t> gAllocations {0u};
std::uint64_t gPass = 0u;
std::uint64_t gFail = 0u;

void check(bool condition, const char* label)
{
    if (condition)
        ++gPass;
    else
    {
        ++gFail;
        std::fprintf(stderr, "FAIL: %s\n", label);
    }
}

template <class A, class B>
void equal(A actual, B expected, const char* label)
{
    check(actual == static_cast<A>(expected), label);
}

using namespace lxr;
using namespace lxr::sequencer;

StepData makeStep(std::uint8_t velocity = 100u, std::uint8_t note = 63u)
{
    StepData result = MakeDefaultStep(false);
    SetStepActive(result, true);
    SetStepVelocity(result, velocity);
    result.note = note;
    return result;
}

bool publish(TriggerScheduler& scheduler, const PatternBank& bank)
{
    return scheduler.patternAuthority().tryPublish(bank) == Status::Ok;
}

void clearPlayableData(PatternBank& bank)
{
    for (PatternData& pattern : bank.patterns)
    {
        for (std::size_t track = 0; track < kTrackCount; ++track)
        {
            pattern.mainGates[track] = 0u;
            for (StepData& step : pattern.steps[track])
                SetStepActive(step, false);
        }
    }
}

struct Runner
{
    TriggerScheduler& scheduler;
    Engine& engine;
    std::uint64_t frame = 0u;

    void untilDue(std::uint64_t target)
    {
        while (scheduler.diagnostics().duePhysicalPositions < target &&
               frame < UINT64_C(100000000))
        {
            (void) scheduler.processNativeBlock(frame, engine);
            frame += kNativeBlockFrames;
        }
    }
};

struct PcmBlock
{
    std::array<std::int16_t, 32> l1 {};
    std::array<std::int16_t, 32> r1 {};
    std::array<std::int16_t, 32> l2 {};
    std::array<std::int16_t, 32> r2 {};
};

PcmBlock render(Engine& engine)
{
    PcmBlock result;
    engine.processBlock(result.l1.data(), result.r1.data(),
                        result.l2.data(), result.r2.data());
    return result;
}

bool same(const PcmBlock& left, const PcmBlock& right)
{
    return left.l1 == right.l1 && left.r1 == right.r1 &&
           left.l2 == right.l2 && left.r2 == right.r2;
}

bool nonzero(const PcmBlock& block)
{
    for (std::size_t i = 0; i < block.l1.size(); ++i)
        if (block.l1[i] != 0 || block.r1[i] != 0 ||
            block.l2[i] != 0 || block.r2[i] != 0)
            return true;
    return false;
}

void configure(Engine& engine)
{
    check(engine.loadAssets("data"), "load assets");
    engine.setup808Kit();
}

void testEuclideanCopyAndClear()
{
    PatternBank bank = MakeDefaultPatternBank();
    StepData preserved = makeStep(77u, 44u);
    preserved.parameter1 = 8u;
    preserved.value1 = 91u;
    bank.patterns[0].steps[2][37] = preserved;
    bank.patterns[0].rotations[2] = 9u;

    for (std::uint8_t length = 1u; length <= 16u; ++length)
        for (std::uint8_t pulses = 1u; pulses <= length; ++pulses)
            for (std::uint8_t rotation = 0u; rotation <= length; ++rotation)
            {
                const Status status = GenerateEuclidean(
                    bank, 0u, 2u, length, pulses, rotation);
                check(status == Status::Ok, "Euclidean valid input");
                equal(bank.patterns[0].mainGates[2],
                      p16e_original_euclidean(length, pulses, rotation),
                      "unchanged original-C Euclidean gate word");
                equal(bank.patterns[0].lengthCodes[2],
                      length == 16u ? 0u : length,
                      "Euclidean length encoding");
                equal(bank.patterns[0].rotations[2], 9u,
                      "Euclidean preserves track rotation field");
                check(bank.patterns[0].steps[2][37].value1 == 91u,
                      "Euclidean preserves physical step records");
            }
    check(GenerateEuclidean(bank, 8u, 0u, 16u, 4u, 0u) ==
              Status::InvalidPattern, "Euclidean rejects pattern");
    check(GenerateEuclidean(bank, 0u, 7u, 16u, 4u, 0u) ==
              Status::InvalidTrack, "Euclidean rejects track");
    check(GenerateEuclidean(bank, 0u, 0u, 0u, 0u, 0u) == Status::Ok &&
          bank.patterns[0].mainGates[0] ==
              p16e_original_euclidean(1u, 1u, 0u),
          "Euclidean coerces zero length and pulses to one");
    check(GenerateEuclidean(bank, 0u, 0u, 8u, 9u, 0u) == Status::Ok &&
          bank.patterns[0].mainGates[0] ==
              p16e_original_euclidean(8u, 8u, 0u),
          "Euclidean clamps excess pulses to length");
    check(GenerateEuclidean(bank, 0u, 0u, 8u, 3u, 9u) ==
              Status::InvalidRotation, "Euclidean rejects excess rotation");

    PatternData& pattern = bank.patterns[0];
    pattern.repeat = 11u;
    pattern.nextPattern = 6u;
    pattern.mainGates[1] = 0x55aau;
    pattern.lengthCodes[1] = 5u;
    pattern.rotations[1] = 4u;
    pattern.steps[1][9] = makeStep(31u, 72u);
    check(CopyTrack(bank, 0u, 1u, 4u) == Status::Ok, "copy track");
    check(std::memcmp(pattern.steps[4].data(), pattern.steps[1].data(),
                      sizeof(pattern.steps[1])) == 0,
          "copy all step bytes");
    equal(pattern.mainGates[4], 0x55aau, "copy track gates");
    equal(pattern.lengthCodes[4], 5u, "copy track length");
    equal(pattern.rotations[4], 4u, "copy track rotation");
    check(ClearTrack(bank, 0u, 4u) == Status::Ok, "clear track");
    equal(pattern.mainGates[4], 0u, "clear track gates");
    equal(pattern.lengthCodes[4], 0u, "clear track length");
    equal(pattern.rotations[4], 0u, "clear track rotation");
    check(IsStepActive(pattern.steps[4][0]) &&
          !IsStepActive(pattern.steps[4][1]), "clear canonical step activity");
    equal(pattern.repeat, 11u, "track clear preserves repeat");
    equal(pattern.nextPattern, 6u, "track clear preserves next");

    bank.patterns[1].repeat = 7u;
    bank.patterns[1].nextPattern = 3u;
    check(CopyPattern(bank, 0u, 1u) == Status::Ok, "copy pattern");
    equal(bank.patterns[1].repeat, 7u, "pattern copy preserves destination repeat");
    equal(bank.patterns[1].nextPattern, 3u, "pattern copy preserves destination next");
    ResetPattern(bank, 1u);
    equal(bank.patterns[1].repeat, 7u, "pattern clear preserves repeat");
    equal(bank.patterns[1].nextPattern, 3u, "pattern clear preserves next");

    PatternStateAuthority authority(bank);
    PatternStateAuthority::WriteHandle edit;
    check(authority.tryBeginEdit(edit) == Status::Ok, "begin coherent edit");
    check(edit.generateEuclidean(0u, 0u, 13u, 5u, 2u) == Status::Ok,
          "candidate Euclidean edit");
    check(edit.copyTrack(0u, 0u, 1u) == Status::Ok, "candidate copy track");
    check(edit.clearTrack(0u, 1u) == Status::Ok, "candidate clear track");
    check(authority.commit(edit) == Status::Ok, "publish coherent edit");
}

void testClockShuffle()
{
    for (std::uint8_t shuffle : {std::uint8_t{0u}, std::uint8_t{1u},
                                 std::uint8_t{64u}, std::uint8_t{127u}})
    {
        for (std::uint16_t bpm : {std::uint16_t{1u}, std::uint16_t{120u},
                                  std::uint16_t{255u}})
        {
            SequencerClock clock;
            check(clock.setTempo(bpm) == Status::Ok, "shuffle tempo");
            check(clock.setShuffle(shuffle) == Status::Ok, "shuffle amount");
            clock.start();
            std::array<ClockEvent, 64> events {};
            std::size_t written = 0u;
            check(clock.advanceTo(UINT64_C(100000000), events.data(),
                                  events.size(), written) == Status::InsufficientCapacity,
                  "bounded clock capacity");
            equal(written, events.size(), "clock filled expected events");
            std::uint64_t previous = 0u;
            for (std::size_t i = 0; i < written; ++i)
            {
                const std::uint64_t expected = p16e_original_shuffle_boundary(
                    i, bpm, shuffle, previous, i != 0u ? 1u : 0u);
                equal(events[i].quantizedNativeFrame, expected,
                      "source-derived shuffle boundary");
                check(i == 0u || events[i].quantizedNativeFrame >= previous + 32u,
                      "shuffle prevents inversion and duplicate dispatch");
                previous = events[i].quantizedNativeFrame;
            }
        }
    }
    SequencerClock live;
    live.start();
    ClockEvent first {};
    std::size_t written = 0u;
    check(live.advanceTo(1u, &first, 1u, written) == Status::Ok && written == 1u,
          "live shuffle first event");
    check(live.setShuffle(127u) == Status::Ok, "live shuffle on");
    const std::uint64_t next = live.nextQuantizedFrame();
    check(live.setShuffle(0u) == Status::Ok, "live shuffle off");
    check(live.nextQuantizedFrame() >= first.quantizedNativeFrame + 32u,
          "live shuffle remains monotonic");
    check(next >= first.quantizedNativeFrame + 32u,
          "live shuffle delayed boundary valid");
}

void testChainsLengthsAndRng()
{
    PatternBank bank = MakeDefaultPatternBank();
    clearPlayableData(bank);
    bank.patterns[0].lengthCodes[0] = 1u;
    bank.patterns[0].repeat = 2u;
    bank.patterns[0].nextPattern = 1u;
    bank.patterns[1].lengthCodes[0] = 1u;
    bank.patterns[1].repeat = 0u;
    bank.patterns[1].nextPattern = 1u;
    TriggerScheduler fixed(0x12345678u);
    check(publish(fixed, bank), "publish fixed chain");
    check(fixed.setTempo(255u) == Status::Ok, "chain tempo");
    check(fixed.start() == Status::Ok, "start fixed chain");
    Engine engine;
    Runner runner {fixed, engine};
    runner.untilDue(24u);
    equal(fixed.selectedPattern(), 0u, "repeat holds through first two bars");
    runner.untilDue(25u);
    equal(fixed.selectedPattern(), 1u, "fixed transition on third track0 wrap");
    equal(fixed.barCounter(), 3u, "absolute bar counter");

    PatternBank polymeter = bank;
    polymeter.patterns[0].repeat = 0u;
    polymeter.patterns[0].nextPattern = 1u;
    polymeter.patterns[0].lengthCodes[0] = 2u;
    polymeter.patterns[0].lengthCodes[1] = 1u;
    TriggerScheduler independent(9u);
    check(publish(independent, polymeter), "publish polymeter");
    check(independent.setTempo(255u) == Status::Ok, "polymeter tempo");
    check(independent.start() == Status::Ok, "start polymeter");
    Runner polyRunner {independent, engine};
    polyRunner.untilDue(9u);
    equal(independent.trackPosition(1u), 0u, "short track wraps independently");
    equal(independent.selectedPattern(), 0u, "other-track wrap cannot transition");
    polyRunner.untilDue(17u);
    equal(independent.selectedPattern(), 1u, "track0 wrap transitions polymeter");

    constexpr std::uint32_t seed = 0x2468ace1u;
    PatternBank randomBank = MakeDefaultPatternBank();
    clearPlayableData(randomBank);
    randomBank.patterns[0].lengthCodes[0] = 1u;
    randomBank.patterns[0].nextPattern = 14u;
    TriggerScheduler random(seed);
    check(publish(random, randomBank), "publish random chain");
    check(random.setTempo(255u) == Status::Ok, "random tempo");
    check(random.start() == Status::Ok, "start random chain");
    Runner randomRunner {random, engine};
    randomRunner.untilDue(9u);
    P16EChainState reference {0u, 0u, 0u, seed, 0u};
    p16e_original_chain_wrap(&reference, 0u, 14u, 0u);
    equal(random.selectedPattern(), reference.active, "random-next original-C selection");
    equal(random.rngState(), reference.rng, "random-next consumes one full RNG value");
    equal(random.diagnostics().randomPatternSelections, 1u,
          "random-next diagnostic");

    TriggerScheduler manual(1u);
    check(publish(manual, randomBank), "publish manual bank");
    check(manual.selectPattern(3u) == Status::Ok, "stopped manual select");
    equal(manual.selectedPattern(), 3u, "stopped selection immediate");
    check(manual.start() == Status::Ok, "start manual selection");
    check(manual.selectPattern(2u) == Status::Ok, "running manual select queued");
    equal(manual.selectedPattern(), 3u, "running selection waits for boundary");

    PatternBank restartBank = MakeDefaultPatternBank();
    clearPlayableData(restartBank);
    restartBank.patterns[0].rotations[0] = 5u;
    TriggerScheduler restart(4u);
    check(publish(restart, restartBank), "publish restart rotation");
    check(restart.start() == Status::Ok, "start rotated pattern");
    Runner restartRunner {restart, engine};
    restartRunner.untilDue(1u);
    equal(restart.trackPosition(0u), 40u, "start honors stored rotation");
    restart.stop();
    check(restart.start() == Status::Ok, "restart after stop");
    restartRunner.frame += 32u;
    restartRunner.untilDue(2u);
    equal(restart.trackPosition(0u), 0u, "stop resets active runtime rotation");
}

void testMuteRollLocksAndPcm()
{
    PatternBank bank = MakeDefaultPatternBank();
    clearPlayableData(bank);
    for (std::size_t track = 0; track < kTrackCount; ++track)
    {
        bank.patterns[0].mainGates[track] = 1u;
        bank.patterns[0].steps[track][0] = makeStep(
            static_cast<std::uint8_t>(90u + track),
            static_cast<std::uint8_t>(55u + track));
    }
    bank.patterns[0].steps[0][0].parameter1 = 8u;
    bank.patterns[0].steps[0][0].value1 = 96u;
    bank.patterns[0].steps[0][0].parameter2 = 8u;
    bank.patterns[0].steps[0][0].value2 = 32u;
    bank.patterns[0].steps[0][1].parameter1 = 8u;
    bank.patterns[0].steps[0][1].value1 = 55u;

    TriggerScheduler muted(0x11111111u);
    check(publish(muted, bank), "publish mute bank");
    for (std::uint8_t track = 0u; track < 7u; ++track)
        check(muted.setTrackMuted(track, true) == Status::Ok, "mute each track");
    const std::uint32_t rngBefore = muted.rngState();
    check(muted.start() == Status::Ok, "start muted scheduler");
    Engine mutedEngine;
    ParameterArray mutedParameters;
    mutedParameters.init();
    mutedParameters.applyAllToEngine(mutedEngine);
    check(muted.processNativeBlock(0u, mutedEngine) == Status::Ok,
          "process muted boundary");
    equal(muted.lastBatch().size(), 0u, "mute suppresses ordinary triggers");
    equal(muted.rngState(), rngBefore, "mute suppresses probability RNG");
    equal(muted.trackPosition(0u), 0u, "mute preserves progression");

    check(muted.setRollRate(13u) == Status::Ok, "set 1/128 roll");
    check(muted.setRoll(0u, true) == Status::Ok, "activate muted roll");
    Runner mutedRunner {muted, mutedEngine, 32u};
    mutedRunner.untilDue(2u);
    check(muted.lastBatch().size() == 1u &&
          muted.lastBatch()[0].kind == TriggerKind::PeriodicRoll,
          "roll ignores mute gate and probability");
    check(mutedEngine.sequencerAutomationDestination(0u, 0u).valid(),
          "roll applies current-step automation before trigger");
    check(muted.setRoll(0u, false) == Status::Ok, "release periodic roll");
    muted.unmuteAll();
    check(!muted.trackMuted(0u), "unmute all");

    for (std::uint8_t selector = 0u; selector < 14u; ++selector)
        check(muted.setRollRate(selector) == Status::Ok, "all roll selectors valid");
    check(muted.setRollRate(14u) == Status::InvalidRollRate,
          "invalid roll selector rejected");

    TriggerScheduler oneShot(2u);
    check(publish(oneShot, bank), "publish one-shot bank");
    check(oneShot.setRollRate(0u) == Status::Ok, "one-shot rate");
    check(oneShot.setRoll(3u, true) == Status::Ok, "queue one-shot");
    Engine oneShotEngine;
    check(oneShot.processNativeBlock(0u, oneShotEngine) == Status::Ok,
          "one-shot while transport stopped");
    check(oneShot.lastBatch().size() == 1u &&
          oneShot.lastBatch()[0].kind == TriggerKind::OneShotRoll &&
          oneShot.lastBatch()[0].velocity == 100u &&
          oneShot.lastBatch()[0].note == 63u,
          "one-shot default note and volume");
    check(oneShot.setRoll(3u, false) == Status::Ok, "one-shot release");

    Engine patternA;
    Engine patternB;
    configure(patternA);
    configure(patternB);
    TriggerScheduler schedulerA(0u);
    TriggerScheduler schedulerB(0u);
    PatternBank bankB = bank;
    bankB.patterns[0].steps[0][0].note = 96u;
    check(publish(schedulerA, bank), "publish audio A");
    check(publish(schedulerB, bankB), "publish audio B");
    check(schedulerA.start() == Status::Ok && schedulerB.start() == Status::Ok,
          "start audio schedulers");
    check(schedulerA.processNativeBlock(0u, patternA) == Status::Ok &&
          schedulerB.processNativeBlock(0u, patternB) == Status::Ok,
          "dispatch audio patterns");
    const PcmBlock pcmA = render(patternA);
    const PcmBlock pcmB = render(patternB);
    check(nonzero(pcmA) && nonzero(pcmB), "patterns render nonzero native PCM");
    check(!same(pcmA, pcmB), "pattern A and B native PCM differ");

    Engine midiA;
    Engine midiB;
    configure(midiA);
    configure(midiB);
    TriggerScheduler stopped;
    midiA.triggerDrum(0u, 111u, 62u);
    midiB.triggerDrum(0u, 111u, 62u);
    check(stopped.processNativeBlock(0u, midiA) == Status::Ok,
          "stopped sequencer MIDI boundary");
    check(same(render(midiA), render(midiB)),
          "MIDI-only PCM bit-identical with sequencer stopped");
}

void testPublicationAndAllocation()
{
    TriggerScheduler scheduler(3u);
    PatternBank bank = MakeDefaultPatternBank();
    clearPlayableData(bank);
    bank.patterns[0].mainGates[0] = 1u;
    bank.patterns[0].steps[0][0] = makeStep(100u, 60u);
    check(publish(scheduler, bank), "publish concurrency base");

    std::atomic<unsigned> stage {0u};
    std::thread writer([&]() {
        PatternStateAuthority::WriteHandle edit;
        if (scheduler.patternAuthority().tryBeginEdit(edit) != Status::Ok)
        {
            stage.store(99u, std::memory_order_release);
            return;
        }
        StepData changed = makeStep(81u, 91u);
        (void) edit.setStep(0u, 0u, 0u, changed);
        stage.store(1u, std::memory_order_release);
        while (stage.load(std::memory_order_acquire) == 1u)
            std::this_thread::yield();
        if (scheduler.patternAuthority().commit(edit) != Status::Ok)
            stage.store(99u, std::memory_order_release);
        else
            stage.store(3u, std::memory_order_release);
    });
    while (stage.load(std::memory_order_acquire) == 0u)
        std::this_thread::yield();
    check(stage.load(std::memory_order_acquire) == 1u,
          "writer candidate held unpublished");
    check(scheduler.start() == Status::Ok, "start while candidate unpublished");
    Engine engine;
    check(scheduler.processNativeBlock(0u, engine) == Status::Ok,
          "reader observes complete old generation");
    equal(scheduler.lastBatch()[0].note, 60u, "unpublished edit invisible");
    stage.store(2u, std::memory_order_release);
    writer.join();
    check(stage.load(std::memory_order_acquire) == 3u, "candidate committed");

    TriggerScheduler allocation(4u);
    check(publish(allocation, bank), "publish allocation bank");
    check(allocation.setRollRate(13u) == Status::Ok, "allocation roll rate");
    for (std::uint8_t track = 0u; track < 7u; ++track)
        (void) allocation.setRoll(track, true);
    check(allocation.start() == Status::Ok, "start allocation scheduler");
    const std::uint64_t before = gAllocations.load(std::memory_order_relaxed);
    for (std::uint64_t frame = 0u; frame < 250000u; frame += 32u)
        (void) allocation.processNativeBlock(frame, engine);
    const std::uint64_t after = gAllocations.load(std::memory_order_relaxed);
    equal(after - before, 0u, "performance scheduler audio path allocates zero");
    equal(allocation.diagnostics().rejectedOverflowEvents, 0u,
          "derived event capacity never overflows");
    equal(allocation.diagnostics().rejectedAutomationOverflowEvents, 0u,
          "derived automation capacity never overflows");
}

} // namespace

void* operator new(std::size_t size)
{
    gAllocations.fetch_add(1u, std::memory_order_relaxed);
    if (void* value = std::malloc(size)) return value;
    std::abort();
}
void* operator new[](std::size_t size)
{
    gAllocations.fetch_add(1u, std::memory_order_relaxed);
    if (void* value = std::malloc(size)) return value;
    std::abort();
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

int main()
{
    testEuclideanCopyAndClear();
    testClockShuffle();
    testChainsLengthsAndRng();
    testMuteRollLocksAndPcm();
    testPublicationAndAllocation();
    std::printf("{\"pass\":%llu,\"fail\":%llu,\"allocations\":%llu}\n",
        static_cast<unsigned long long>(gPass),
        static_cast<unsigned long long>(gFail),
        static_cast<unsigned long long>(gAllocations.load()));
    return gFail == 0u ? 0 : 1;
}
