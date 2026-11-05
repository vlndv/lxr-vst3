#include "../dsp/Engine.h"
#include "../dsp/LXRAudioAdapter.h"
#include "../dsp/TriggerScheduler.h"
#include "reference/sequencer/original_trigger_scheduler_adapter.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace
{
std::atomic<std::uint64_t> gAllocations {0};
std::uint64_t gPass = 0;
std::uint64_t gFail = 0;

void check(bool value, const char* label)
{
    if (value)
        ++gPass;
    else
    {
        ++gFail;
        std::fprintf(stderr, "FAIL: %s\n", label);
    }
}
template <class A, class B>
void checkEqual(A actual, B expected, const char* label)
{
    check(actual == static_cast<A>(expected), label);
}

using namespace lxr;
using namespace lxr::sequencer;

StepData step(bool active, std::uint8_t velocity, std::uint8_t probability,
              std::uint8_t note)
{
    StepData value = MakeDefaultStep(false);
    SetStepActive(value, active);
    SetStepVelocity(value, velocity);
    value.probability = probability;
    value.note = note;
    return value;
}

bool publish(TriggerScheduler& scheduler, const PatternBank& bank)
{
    return scheduler.patternAuthority().tryPublish(bank) == Status::Ok;
}

PatternBank oneTrackBank(std::uint8_t track, std::uint8_t position,
                         std::uint8_t velocity = 100u,
                         std::uint8_t probability = 127u,
                         std::uint8_t note = 63u)
{
    PatternBank bank = MakeDefaultPatternBank();
    for (auto& pattern : bank.patterns)
    {
        for (auto& gates : pattern.mainGates)
            gates = 0u;
        for (auto& trackSteps : pattern.steps)
            for (auto& value : trackSteps)
                SetStepActive(value, false);
    }
    bank.patterns[0].mainGates[track] =
        static_cast<std::uint16_t>(1u << MainStepOf(position));
    bank.patterns[0].steps[track][position] =
        step(true, velocity, probability, note);
    return bank;
}

void copyForReference(
    const PatternData& pattern,
    std::uint8_t output[P16C_TRACKS][P16C_POSITIONS][P16C_STEP_BYTES])
{
    for (std::size_t track = 0; track < kTrackCount; ++track)
        for (std::size_t position = 0; position < kPhysicalPositionCount; ++position)
        {
            const StepData& value = pattern.steps[track][position];
            output[track][position][0] = value.volume;
            output[track][position][1] = value.probability;
            output[track][position][2] = value.note;
            output[track][position][3] = value.parameter1;
            output[track][position][4] = value.value1;
            output[track][position][5] = value.parameter2;
            output[track][position][6] = value.value2;
        }
}

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

bool samePcm(const PcmBlock& a, const PcmBlock& b)
{
    return a.l1 == b.l1 && a.r1 == b.r1 && a.l2 == b.l2 && a.r2 == b.r2;
}

bool nonzero(const PcmBlock& block)
{
    for (std::size_t i = 0; i < 32; ++i)
        if (block.l1[i] != 0 || block.r1[i] != 0 ||
            block.l2[i] != 0 || block.r2[i] != 0)
            return true;
    return false;
}

void configureEngine(Engine& engine)
{
    check(engine.loadAssets("data"), "load native assets");
    engine.setup808Kit();
}

void testBatchCapacity()
{
    TriggerEventBatch batch;
    for (std::uint8_t track = 0; track < 8u; ++track)
    {
        TriggerEvent event {};
        event.track = track;
        const bool accepted = batch.push(event);
        check(accepted == (track < 7u), "fixed batch reject-newest policy");
    }
    checkEqual(batch.size(), 7u, "fixed batch capacity seven");
    checkEqual(batch.rejected(), 1u, "fixed batch rejection diagnostic");
    for (std::size_t i = 0; i < batch.size(); ++i)
        checkEqual(batch[i].track, i, "fixed batch preserves accepted order");
}

void testSevenTrackReference()
{
    constexpr std::uint32_t seed = 0x12345678u;
    TriggerScheduler scheduler(seed);
    PatternBank bank = MakeDefaultPatternBank();
    PatternData& pattern = bank.patterns[0];
    for (std::size_t track = 0; track < kTrackCount; ++track)
    {
        pattern.mainGates[track] = 1u;
        pattern.steps[track][0] = step(true,
            static_cast<std::uint8_t>(20u + track), 127u,
            static_cast<std::uint8_t>(50u + track));
    }
    check(publish(scheduler, bank), "publish seven-track bank");
    check(scheduler.start() == Status::Ok, "start seven-track scheduler");

    Engine engine;
    check(scheduler.processNativeBlock(0u, engine) == Status::Ok,
          "process first seven-track boundary");
    const TriggerEventBatch& batch = scheduler.lastBatch();
    checkEqual(batch.size(), 7u, "seven events emitted");

    std::uint8_t raw[P16C_TRACKS][P16C_POSITIONS][P16C_STEP_BYTES] {};
    copyForReference(pattern, raw);
    P16CReferenceState reference {};
    p16c_reference_init(&reference, pattern.rotations.data(),
                        pattern.lengthCodes.data(), seed);
    std::array<P16CReferenceEvent, 7> expected {};
    std::size_t rejected = 0;
    const std::size_t expectedCount = p16c_reference_tick(
        &reference, raw, pattern.mainGates.data(), pattern.lengthCodes.data(),
        expected.data(), expected.size(), &rejected);
    checkEqual(expectedCount, batch.size(), "source-derived event count");
    checkEqual(rejected, 0u, "source-derived no overflow");
    for (std::size_t i = 0; i < batch.size(); ++i)
    {
        const TriggerEvent& actual = batch[i];
        checkEqual(actual.track, expected[i].track, "reference track order");
        checkEqual(actual.synthesisOwner, expected[i].owner, "reference owner");
        checkEqual(static_cast<std::uint8_t>(actual.articulation),
                   expected[i].articulation, "reference articulation");
        checkEqual(actual.physicalPosition, expected[i].position,
                   "reference physical position");
        checkEqual(actual.velocity, expected[i].velocity, "reference velocity");
        checkEqual(actual.note, expected[i].note, "reference note");
        checkEqual(actual.stableOrder, i, "stable first-boundary order");
    }
    checkEqual(batch[5].synthesisOwner, 5u, "closed hat owner five");
    check(batch[5].articulation == Articulation::ClosedHiHat,
          "closed hat articulation");
    check(batch[6].articulation == Articulation::OpenHiHat,
          "open hat articulation");
    checkEqual(engine.hihat.isOpen, 1u, "simultaneous hats leave open articulation");
    checkEqual(scheduler.rngState(), reference.rng_state, "reference rng state");
    checkEqual(reference.rng_calls, 7u, "one boundary RNG call per active track");
}

void testGatesSubstepsAndProbability()
{
    TriggerScheduler scheduler(1u);
    PatternBank bank = oneTrackBank(0u, 0u, 99u, 127u, 60u);
    PatternData& pattern = bank.patterns[0];
    pattern.steps[0][3] = step(true, 88u, 127u, 61u);
    pattern.steps[0][7] = step(true, 77u, 127u, 62u);
    pattern.steps[0][8] = step(true, 66u, 127u, 63u);
    pattern.mainGates[0] = 0x0001u; // Main step one remains closed.
    check(publish(scheduler, bank), "publish substep bank");
    check(scheduler.start() == Status::Ok, "start substep scheduler");
    Engine engine;
    std::array<std::uint8_t, 4> positions {};
    std::size_t count = 0;
    for (std::uint64_t frame = 0; frame < 8000u; frame += 32u)
    {
        const Status status = scheduler.processNativeBlock(frame, engine);
        check(status == Status::Ok, "substep timeline status");
        for (const auto& event : scheduler.lastBatch())
            if (count < positions.size())
                positions[count++] = event.physicalPosition;
    }
    checkEqual(count, 3u, "gate suppresses next main step");
    checkEqual(positions[0], 0u, "substep zero emitted");
    checkEqual(positions[1], 3u, "substep three emitted");
    checkEqual(positions[2], 7u, "substep seven emitted");
    check(pattern.steps[0][8].volume == bank.patterns[0].steps[0][8].volume,
          "playback does not rewrite suppressed step");

    TriggerScheduler inactiveBoundary(1u);
    PatternBank quirk = oneTrackBank(0u, 1u, 90u, 0u, 64u);
    check(publish(inactiveBoundary, quirk), "publish inactive-boundary quirk");
    check(inactiveBoundary.start() == Status::Ok, "start inactive-boundary quirk");
    const std::uint32_t initialState = inactiveBoundary.rngState();
    check(inactiveBoundary.processNativeBlock(0u, engine) == Status::Ok,
          "inactive boundary processed");
    checkEqual(inactiveBoundary.rngState(), initialState,
               "inactive boundary consumes no RNG");
    for (std::uint64_t frame = 32u; frame <= 704u; frame += 32u)
        check(inactiveBoundary.processNativeBlock(frame, engine) == Status::Ok,
              "advance to reused cluster latch");
    checkEqual(inactiveBoundary.lastBatch().size(), 1u,
               "probability zero passes reused zero latch");
    checkEqual(inactiveBoundary.rngState(), initialState,
               "cluster reuse still consumes no RNG");

    TriggerScheduler probabilityZeroPass(0u);
    PatternBank zero = oneTrackBank(0u, 0u, 100u, 0u, 63u);
    check(publish(probabilityZeroPass, zero), "publish probability zero pass");
    check(probabilityZeroPass.start() == Status::Ok, "start probability zero pass");
    check(probabilityZeroPass.processNativeBlock(0u, engine) == Status::Ok,
          "process probability zero pass");
    checkEqual(probabilityZeroPass.lastBatch().size(), 1u,
               "probability zero passes random zero");

    TriggerScheduler probabilityZeroReject(1u);
    check(publish(probabilityZeroReject, zero), "publish probability zero reject");
    check(probabilityZeroReject.start() == Status::Ok, "start probability zero reject");
    check(probabilityZeroReject.processNativeBlock(0u, engine) == Status::Ok,
          "process probability zero reject");
    checkEqual(probabilityZeroReject.lastBatch().size(), 0u,
               "probability zero rejects nonzero random");

    TriggerScheduler maximum(0xabcdef01u);
    PatternBank maxBank = oneTrackBank(0u, 0u, 100u, 127u, 63u);
    check(publish(maximum, maxBank), "publish maximum probability");
    check(maximum.start() == Status::Ok, "start maximum probability");
    check(maximum.processNativeBlock(0u, engine) == Status::Ok,
          "process maximum probability");
    checkEqual(maximum.lastBatch().size(), 1u, "probability 127 always passes");
}

void testTransportPatternsBusyAndInstances()
{
    TriggerScheduler scheduler(0x11111111u);
    PatternBank bank = MakeDefaultPatternBank();
    for (std::size_t patternIndex = 0; patternIndex < kPatternCount; ++patternIndex)
    {
        auto& pattern = bank.patterns[patternIndex];
        pattern.mainGates[0] = 1u;
        pattern.steps[0][0] = step(true, 100u, 127u,
                                   static_cast<std::uint8_t>(40u + patternIndex));
    }
    check(publish(scheduler, bank), "publish eight patterns");
    Engine engine;
    for (std::uint8_t pattern = 0; pattern < kPatternCount; ++pattern)
    {
        scheduler.reset(0u);
        check(scheduler.selectPattern(pattern) == Status::Ok, "select current pattern");
        check(scheduler.start() == Status::Ok, "start selected pattern");
        check(scheduler.processNativeBlock(0u, engine) == Status::Ok,
              "process selected pattern");
        checkEqual(scheduler.lastBatch().size(), 1u, "selected pattern event count");
        checkEqual(scheduler.lastBatch()[0].pattern, pattern, "selected pattern identity");
        checkEqual(scheduler.lastBatch()[0].note, 40u + pattern,
                   "selected pattern note");
        scheduler.stop();
    }
    check(scheduler.selectPattern(8u) == Status::InvalidPattern,
          "invalid pattern rejected");

    scheduler.reset(0u);
    check(scheduler.selectPattern(0u) == Status::Ok, "reselect pattern zero");
    check(scheduler.start() == Status::Ok, "transport start");
    check(scheduler.processNativeBlock(0u, engine) == Status::Ok, "transport first event");
    const std::uint32_t afterFirst = scheduler.rngState();
    scheduler.stop();
    check(scheduler.processNativeBlock(32u, engine) == Status::Ok,
          "stopped boundary emits nothing");
    check(scheduler.lastBatch().empty(), "stop suppresses events");
    check(scheduler.start() == Status::Ok, "source-style resume restarts");
    check(scheduler.processNativeBlock(64u, engine) == Status::Ok,
          "resume first eligible block");
    checkEqual(scheduler.lastBatch().size(), 1u, "resume emits position zero");
    check(scheduler.rngState() != afterFirst, "resume does not reseed RNG");
    scheduler.reset(0u);
    check(scheduler.start() == Status::Ok, "reset then restart");
    check(scheduler.processNativeBlock(0u, engine) == Status::Ok,
          "reset deterministic first event");
    checkEqual(scheduler.rngState(), afterFirst, "reset reconstructs RNG epoch");

    TriggerScheduler busy(9u);
    check(publish(busy, bank), "publish Busy bank");
    check(busy.start() == Status::Ok, "start Busy scheduler");
    check(busy.processNativeBlock(0u, engine, 0u) == Status::Busy,
          "zero-attempt snapshot reports Busy");
    check(busy.lastBatch().empty(), "Busy emits no event");
    checkEqual(busy.trackPosition(0u), 0u, "Busy consumes physical boundary");
    checkEqual(busy.diagnostics().snapshotBusyBoundaries, 1u,
               "Busy diagnostic bounded");

    TriggerScheduler first(0x22222222u);
    TriggerScheduler second(0x22222222u);
    check(publish(first, bank) && publish(second, bank), "publish multi-instance banks");
    check(first.start() == Status::Ok && second.start() == Status::Ok,
          "start multi-instance schedulers");
    check(first.processNativeBlock(0u, engine) == Status::Ok,
          "first instance boundary");
    Engine engine2;
    check(second.processNativeBlock(0u, engine2) == Status::Ok,
          "second instance boundary");
    checkEqual(first.rngState(), second.rngState(), "matched instance RNG");
    checkEqual(first.lastBatch().size(), second.lastBatch().size(),
               "matched instance event count");
}

void testClockTimeline()
{
    TriggerScheduler scheduler(3u);
    PatternBank bank = MakeDefaultPatternBank();
    auto& pattern = bank.patterns[0];
    pattern.mainGates[0] = 0xffffu;
    for (auto& value : pattern.steps[0])
        value = step(true, 100u, 127u, 63u);
    check(publish(scheduler, bank), "publish full timeline bank");
    check(scheduler.start() == Status::Ok, "start full timeline");
    Engine engine;
    std::uint64_t events = 0;
    std::uint8_t expectedPosition = 0;
    std::uint64_t firstBoundary = UINT64_MAX;
    std::uint64_t secondBoundary = UINT64_MAX;
    for (std::uint64_t frame = 0; frame < 90000u; frame += 32u)
    {
        check(scheduler.processNativeBlock(frame, engine) == Status::Ok,
              "full timeline status");
        for (const auto& event : scheduler.lastBatch())
        {
            if (events == 0u) firstBoundary = event.nativeBoundary;
            if (events == 1u) secondBoundary = event.nativeBoundary;
            checkEqual(event.physicalPosition, expectedPosition,
                       "full timeline physical position");
            expectedPosition = static_cast<std::uint8_t>((expectedPosition + 1u) & 0x7fu);
            ++events;
        }
    }
    checkEqual(firstBoundary, 0u, "immediate first native boundary");
    checkEqual(secondBoundary, 704u, "120 BPM second quantized boundary");
    check(events >= 128u, "one full pattern emitted");
    checkEqual(expectedPosition, static_cast<std::uint8_t>(events & 0x7fu),
               "pattern wrap has no duplicate or missing position");

    TriggerScheduler tempo(4u);
    check(publish(tempo, bank), "publish tempo bank");
    check(tempo.setTempo(255u) == Status::Ok, "maximum tempo accepted");
    check(tempo.setTempo(0u) == Status::InvalidTempo, "zero tempo rejected");
    check(tempo.start() == Status::Ok, "start maximum tempo");
    check(tempo.processNativeBlock(0u, engine) == Status::Ok,
          "maximum tempo first event");
    std::uint64_t next = UINT64_MAX;
    for (std::uint64_t frame = 32u; frame < 1000u; frame += 32u)
    {
        check(tempo.processNativeBlock(frame, engine) == Status::Ok,
              "maximum tempo timeline");
        if (!tempo.lastBatch().empty()) { next = frame; break; }
    }
    checkEqual(next, 352u, "maximum tempo remains one event per native block");

    TriggerScheduler slow(0u);
    check(publish(slow, bank), "publish minimum-tempo bank");
    check(slow.setTempo(1u) == Status::Ok, "minimum tempo accepted");
    check(slow.start() == Status::Ok, "start minimum tempo");
    check(slow.processNativeBlock(0u, engine) == Status::Ok,
          "minimum tempo immediate boundary");
    checkEqual(slow.lastBatch().size(), 1u,
               "minimum tempo emits immediate start");
    check(slow.processNativeBlock(32u, engine) == Status::Ok,
          "minimum tempo subsequent block");
    check(slow.lastBatch().empty(), "minimum tempo does not duplicate start");

    TriggerScheduler changing(0u);
    PatternBank changingBank = oneTrackBank(0u, 0u, 100u, 127u, 60u);
    changingBank.patterns[0].steps[0][1] = step(true, 101u, 127u, 61u);
    check(publish(changing, changingBank), "publish tempo-change bank");
    check(changing.start() == Status::Ok, "start tempo-change scheduler");
    check(changing.processNativeBlock(0u, engine) == Status::Ok,
          "tempo-change first boundary");
    check(changing.setTempo(60u) == Status::Ok,
          "running tempo change accepted");
    for (std::uint64_t frame = 32u; frame < 1376u; frame += 32u)
    {
        check(changing.processNativeBlock(frame, engine) == Status::Ok,
              "tempo-change pre-boundary status");
        check(changing.lastBatch().empty(),
              "tempo change has no premature or duplicate event");
    }
    check(changing.processNativeBlock(1376u, engine) == Status::Ok,
          "tempo-change reanchored boundary status");
    checkEqual(changing.lastBatch().size(), 1u,
               "tempo change emits exactly one reanchored event");
    checkEqual(changing.lastBatch()[0].physicalPosition, 1u,
               "tempo change advances exactly one position");
    checkEqual(changing.lastBatch()[0].nativeBoundary, 1376u,
               "tempo change uses clock-defined boundary");
}

void testExtendedReferenceSnapshotsAndChunking()
{
    constexpr std::uint32_t seed = 0x6d2b79f5u;
    TriggerScheduler scheduler(seed);
    PatternBank bank = MakeDefaultPatternBank();
    PatternData& pattern = bank.patterns[0];
    for (std::size_t track = 0; track < kTrackCount; ++track)
    {
        pattern.mainGates[track] = static_cast<std::uint16_t>(
            track % 2u == 0u ? 0xffffu : 0xaaaau);
        for (std::size_t position = 0; position < kPhysicalPositionCount; ++position)
            pattern.steps[track][position] = step(
                ((position + track) % 3u) != 1u,
                static_cast<std::uint8_t>((position + 17u * track) & 0x7fu),
                static_cast<std::uint8_t>((7u * position + 11u * track) & 0x7fu),
                static_cast<std::uint8_t>((31u + position + track) & 0x7fu));
    }
    check(publish(scheduler, bank), "publish extended reference bank");
    check(scheduler.start() == Status::Ok, "start extended reference scheduler");

    std::uint8_t raw[P16C_TRACKS][P16C_POSITIONS][P16C_STEP_BYTES] {};
    copyForReference(pattern, raw);
    P16CReferenceState reference {};
    p16c_reference_init(&reference, pattern.rotations.data(),
                        pattern.lengthCodes.data(), seed);
    Engine engine;
    std::uint64_t priorDue = 0;
    std::uint64_t compared = 0;
    for (std::uint64_t frame = 0; frame < 90000u && compared < 128u; frame += 32u)
    {
        check(scheduler.processNativeBlock(frame, engine) == Status::Ok,
              "extended reference timeline status");
        const std::uint64_t due = scheduler.diagnostics().duePhysicalPositions;
        if (due == priorDue)
            continue;
        priorDue = due;
        std::array<P16CReferenceEvent, 7> expected {};
        std::size_t rejected = 0;
        const std::size_t count = p16c_reference_tick(
            &reference, raw, pattern.mainGates.data(), pattern.lengthCodes.data(),
            expected.data(), expected.size(), &rejected);
        checkEqual(rejected, 0u, "extended reference has no overflow");
        checkEqual(scheduler.lastBatch().size(), count,
                   "extended reference exact event count");
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto& actual = scheduler.lastBatch()[i];
            checkEqual(actual.track, expected[i].track,
                       "extended reference track");
            checkEqual(actual.physicalPosition, expected[i].position,
                       "extended reference position");
            checkEqual(actual.velocity, expected[i].velocity,
                       "extended reference velocity");
            checkEqual(actual.note, expected[i].note,
                       "extended reference note");
            checkEqual(scheduler.probabilityLatch(actual.track),
                       expected[i].random_value,
                       "extended reference probability latch");
        }
        checkEqual(scheduler.rngState(), reference.rng_state,
                   "extended reference RNG consumption");
        ++compared;
    }
    checkEqual(compared, 128u, "extended reference full pattern compared");

    TriggerScheduler updates(0u);
    PatternBank updateBank = oneTrackBank(0u, 0u, 100u, 127u, 60u);
    updateBank.patterns[0].steps[0][1] = step(true, 101u, 127u, 61u);
    updateBank.patterns[0].steps[0][2] = step(true, 102u, 127u, 62u);
    std::uint64_t initialGeneration = 0;
    check(updates.patternAuthority().tryPublish(updateBank, &initialGeneration) == Status::Ok,
          "publish initial snapshot update bank");
    check(updates.start() == Status::Ok, "start snapshot update scheduler");
    check(updates.processNativeBlock(0u, engine) == Status::Ok,
          "snapshot update first boundary");
    checkEqual(updates.lastBatch()[0].snapshotGeneration, initialGeneration,
               "event carries initial snapshot generation");
    updateBank.patterns[0].mainGates[0] = 0u;
    std::uint64_t closedGeneration = 0;
    check(updates.patternAuthority().tryPublish(updateBank, &closedGeneration) == Status::Ok,
          "publish gate close while playing");
    for (std::uint64_t frame = 32u; frame <= 704u; frame += 32u)
        check(updates.processNativeBlock(frame, engine) == Status::Ok,
              "advance through live gate close");
    check(updates.lastBatch().empty(), "live gate close suppresses due substep");
    updateBank.patterns[0].mainGates[0] = 1u;
    std::uint64_t reopenedGeneration = 0;
    check(updates.patternAuthority().tryPublish(updateBank, &reopenedGeneration) == Status::Ok,
          "publish gate reopen while playing");
    for (std::uint64_t frame = 736u; frame <= 1376u; frame += 32u)
        check(updates.processNativeBlock(frame, engine) == Status::Ok,
              "advance through live gate reopen");
    checkEqual(updates.lastBatch().size(), 1u, "live gate reopen restores event");
    checkEqual(updates.lastBatch()[0].physicalPosition, 2u,
               "snapshot Busy/update never repeats prior position");
    checkEqual(updates.lastBatch()[0].snapshotGeneration, reopenedGeneration,
               "event carries reopened snapshot generation");
    check(reopenedGeneration > closedGeneration && closedGeneration > initialGeneration,
          "snapshot generations are monotonic");

    // Caller partitioning changes only loop grouping; every native boundary is
    // still visited once. Compare two independent timelines grouped differently.
    TriggerScheduler chunksA(seed);
    TriggerScheduler chunksB(seed);
    check(publish(chunksA, bank) && publish(chunksB, bank),
          "publish chunk-invariance banks");
    check(chunksA.start() == Status::Ok && chunksB.start() == Status::Ok,
          "start chunk-invariance schedulers");
    Engine engineA;
    Engine engineB;
    std::uint64_t frameA = 0;
    std::uint64_t frameB = 0;
    const std::array<std::uint8_t, 7> grouping {{1u, 5u, 2u, 9u, 3u, 7u, 4u}};
    std::size_t group = 0;
    for (std::size_t block = 0; block < 4096u; ++block)
    {
        check(chunksA.processNativeBlock(frameA, engineA) == Status::Ok,
              "single-block partition status");
        frameA += 32u;
    }
    std::size_t remaining = 4096u;
    while (remaining != 0u)
    {
        const std::size_t take = remaining < grouping[group % grouping.size()]
            ? remaining : grouping[group % grouping.size()];
        for (std::size_t i = 0; i < take; ++i)
        {
            check(chunksB.processNativeBlock(frameB, engineB) == Status::Ok,
                  "variable partition status");
            frameB += 32u;
        }
        remaining -= take;
        ++group;
    }
    checkEqual(chunksA.diagnostics().duePhysicalPositions,
               chunksB.diagnostics().duePhysicalPositions,
               "variable caller chunk due-count invariant");
    checkEqual(chunksA.diagnostics().emittedEvents,
               chunksB.diagnostics().emittedEvents,
               "variable caller chunk event-count invariant");
    checkEqual(chunksA.rngState(), chunksB.rngState(),
               "variable caller chunk RNG invariant");
    for (std::uint8_t track = 0; track < kTrackCount; ++track)
        checkEqual(chunksA.trackPosition(track), chunksB.trackPosition(track),
                   "variable caller chunk position invariant");
}

void testNativePcmAndMidiCoexistence()
{
    {
        Engine freshA;
        Engine freshB;
        configureEngine(freshA);
        configureEngine(freshB);
        freshA.triggerHiHat(100u, false, 61u);
        freshA.triggerHiHat(90u, true, 62u);
        freshB.triggerHiHat(100u, false, 61u);
        freshB.triggerHiHat(90u, true, 62u);
        check(samePcm(render(freshA), render(freshB)),
              "fresh matched Engine instances render identical hats");
    }

    Engine scheduled;
    Engine direct;
    configureEngine(scheduled);
    configureEngine(direct);

    TriggerScheduler stopped;
    check(stopped.processNativeBlock(0u, scheduled) == Status::Ok,
          "stopped scheduler baseline boundary");
    check(samePcm(render(scheduled), render(direct)),
          "sequencer off preserves exact native PCM");

    scheduled.init(); direct.init();
    configureEngine(scheduled); configureEngine(direct);
    TriggerScheduler d1(0u);
    PatternBank bank = oneTrackBank(0u, 0u, 110u, 127u, 60u);
    check(publish(d1, bank), "publish D1 audio bank");
    check(d1.start() == Status::Ok, "start D1 audio scheduler");
    check(d1.processNativeBlock(0u, scheduled) == Status::Ok,
          "dispatch D1 scheduler event");
    direct.triggerDrum(0u, 110u, 60u);
    const PcmBlock scheduledPcm = render(scheduled);
    const PcmBlock directPcm = render(direct);
    check(nonzero(scheduledPcm), "sequencer D1 renders nonzero PCM");
    check(samePcm(scheduledPcm, directPcm),
          "sequencer D1 PCM equals direct native trigger");

    for (std::uint8_t track = 0; track < 7u; ++track)
    {
        Engine voice;
        configureEngine(voice);
        TriggerScheduler owner(0u);
        PatternBank ownerBank = oneTrackBank(track, 0u, 120u, 127u, 63u);
        check(publish(owner, ownerBank), "publish owner audio bank");
        check(owner.start() == Status::Ok, "start owner audio scheduler");
        check(owner.processNativeBlock(0u, voice) == Status::Ok,
              "dispatch owner audio event");
        check(nonzero(render(voice)), "all logical tracks render expected nonzero PCM");
    }

    Engine hatsScheduled;
    configureEngine(hatsScheduled);
    Engine hatsDirect = hatsScheduled;
    TriggerScheduler hats(0u);
    PatternBank hatsBank = MakeDefaultPatternBank();
    for (auto& trackSteps : hatsBank.patterns[0].steps)
        for (auto& value : trackSteps)
            SetStepActive(value, false);
    hatsBank.patterns[0].mainGates[5] = 1u;
    hatsBank.patterns[0].mainGates[6] = 1u;
    hatsBank.patterns[0].steps[5][0] = step(true, 100u, 127u, 61u);
    hatsBank.patterns[0].steps[6][0] = step(true, 90u, 127u, 62u);
    check(publish(hats, hatsBank), "publish simultaneous hats");
    check(hats.start() == Status::Ok, "start simultaneous hats");
    check(hats.processNativeBlock(0u, hatsScheduled) == Status::Ok,
          "dispatch simultaneous hats");
    hatsDirect.triggerHiHat(100u, false, 61u);
    hatsDirect.triggerHiHat(90u, true, 62u);
    checkEqual(hatsScheduled.hihat.isOpen, 1u, "open hat dispatches after closed");
    check(hatsScheduled.hihat.velo == hatsDirect.hihat.velo,
          "hat velocity state matches direct order");
    check(hatsScheduled.hihat.osc.phase == hatsDirect.hihat.osc.phase,
          "hat oscillator phase matches direct order");
    check(hatsScheduled.hihat.osc.baseNote == hatsDirect.hihat.osc.baseNote,
          "hat note state matches direct order");
    check(hatsScheduled.hihat.oscVolEg.decay == hatsDirect.hihat.oscVolEg.decay,
          "hat decay state matches direct order");
    check(hatsScheduled.velocityModulators[5].modValue ==
              hatsDirect.velocityModulators[5].modValue,
          "hat velocity modulator matches direct order");
    check(hatsScheduled.hihat.rng.state == hatsDirect.hihat.rng.state,
          "hat voice RNG matches before render");
    const PcmBlock hatsDirectPcm = render(hatsDirect);
    const PcmBlock hatsScheduledPcm = render(hatsScheduled);
    if (!samePcm(hatsScheduledPcm, hatsDirectPcm))
    {
        for (std::size_t i = 0; i < 32u; ++i)
            if (hatsScheduledPcm.l1[i] != hatsDirectPcm.l1[i] ||
                hatsScheduledPcm.r1[i] != hatsDirectPcm.r1[i])
            {
                std::fprintf(stderr, "hat first diff %zu: %d/%d vs %d/%d\n", i,
                    hatsScheduledPcm.l1[i], hatsScheduledPcm.r1[i],
                    hatsDirectPcm.l1[i], hatsDirectPcm.r1[i]);
                break;
            }
    }
    check(samePcm(hatsScheduledPcm, hatsDirectPcm),
          "simultaneous hat PCM matches direct closed-then-open order");

    Engine midiScheduled;
    Engine midiDirect;
    configureEngine(midiScheduled);
    configureEngine(midiDirect);
    TriggerScheduler coexist(0u);
    PatternBank coexistBank = oneTrackBank(0u, 0u, 80u, 127u, 67u);
    check(publish(coexist, coexistBank), "publish MIDI coexistence bank");
    check(coexist.start() == Status::Ok, "start MIDI coexistence scheduler");
    // Existing external MIDI trigger occurs first; sequencer follows at the
    // pre-render native boundary.
    midiScheduled.triggerDrum(0u, 127u, 48u);
    midiDirect.triggerDrum(0u, 127u, 48u);
    check(coexist.processNativeBlock(0u, midiScheduled) == Status::Ok,
          "dispatch sequencer after simulated MIDI");
    midiDirect.triggerDrum(0u, 80u, 67u);
    check(samePcm(render(midiScheduled), render(midiDirect)),
          "MIDI then sequencer ordering exact PCM");
}

void testAdapterBoundaryAndAllocations()
{
    Engine engine;
    configureEngine(engine);
    TriggerScheduler scheduler(0u);
    PatternBank bank = oneTrackBank(0u, 0u, 100u, 127u, 60u);
    check(publish(scheduler, bank), "publish adapter bank");
    check(scheduler.start() == Status::Ok, "start adapter scheduler");
    LXRAudioAdapter adapter;
    adapter.reset();
    std::int16_t left = 0;
    std::int16_t right = 0;
    adapter.getNextST1Sample(engine, left, right, &scheduler);
    checkEqual(scheduler.lastBatch().size(), 1u,
               "adapter dispatches before first native block");
    checkEqual(scheduler.lastBatch()[0].nativeBoundary, 0u,
               "adapter first boundary identity");
    for (std::size_t i = 1; i < 32u; ++i)
        adapter.getNextST1Sample(engine, left, right, &scheduler);
    checkEqual(adapter.nativeFrame(), 32u, "adapter advances exact native block");

    TriggerScheduler allocation(0u);
    check(publish(allocation, bank), "publish allocation bank");
    check(allocation.start() == Status::Ok, "start allocation scheduler");
    Engine allocationEngine;
    const std::uint64_t before = gAllocations.load(std::memory_order_relaxed);
    for (std::uint64_t frame = 0; frame < 200000u; frame += 32u)
        (void) allocation.processNativeBlock(frame, allocationEngine);
    const std::uint64_t after = gAllocations.load(std::memory_order_relaxed);
    checkEqual(after - before, 0u, "scheduler processing allocates zero blocks");
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
    testBatchCapacity();
    testSevenTrackReference();
    testGatesSubstepsAndProbability();
    testTransportPatternsBusyAndInstances();
    testClockTimeline();
    testExtendedReferenceSnapshotsAndChunking();
    testNativePcmAndMidiCoexistence();
    testAdapterBoundaryAndAllocations();
    std::printf("{\"pass\":%llu,\"fail\":%llu,\"allocations\":%llu}\n",
        static_cast<unsigned long long>(gPass),
        static_cast<unsigned long long>(gFail),
        static_cast<unsigned long long>(gAllocations.load()));
    return gFail == 0u ? 0 : 1;
}
