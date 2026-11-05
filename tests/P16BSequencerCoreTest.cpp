#include "../interfaces/SequencerCore.h"
#include "reference/sequencer/original_sequencer_core_adapter.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <thread>

namespace {

std::atomic<std::uint64_t> gAllocations {0};

} // namespace

void* operator new(std::size_t size)
{
    gAllocations.fetch_add(1u, std::memory_order_relaxed);
    if (void* value = std::malloc(size))
        return value;
    std::abort();
}

void* operator new[](std::size_t size)
{
    gAllocations.fetch_add(1u, std::memory_order_relaxed);
    if (void* value = std::malloc(size))
        return value;
    std::abort();
}

void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

namespace {

namespace sq = lxr::sequencer;

int gPass = 0;
int gFail = 0;

struct Metrics
{
    std::uint64_t authorityBytes = 0;
    std::uint64_t payloadBytes = 0;
    std::uint64_t allocationDelta = 0;
    std::uint64_t clockNanosecondsPerEvent = 0;
    std::uint64_t acquireNanoseconds = 0;
    std::uint64_t editNanoseconds = 0;
    std::uint64_t publicationBusy = 0;
    std::uint64_t readerSnapshots = 0;
};

Metrics gMetrics;

void check(bool condition, const char* name)
{
    if (condition)
        ++gPass;
    else
    {
        ++gFail;
        std::fprintf(stderr, "FAIL: %s\n", name);
    }
}

bool sameStep(const sq::StepData& step, const std::uint8_t expected[7])
{
    return step.volume == expected[0] && step.probability == expected[1] &&
           step.note == expected[2] && step.parameter1 == expected[3] &&
           step.value1 == expected[4] && step.parameter2 == expected[5] &&
           step.value2 == expected[6];
}

void dataModelAndDefaultTests()
{
    check(sq::kTrackCount == 7, "seven tracks");
    check(sq::kPatternCount == 8, "eight patterns");
    check(sq::kPhysicalPositionCount == 128, "128 physical positions");
    check(sq::kStoredStepCount == 7168, "7168 stored steps");
    check(sizeof(sq::StepData) == p16b_original_step_size(), "Step size matches original header");
    for (std::uint32_t field = 0; field < 7; ++field)
        check(p16b_original_step_offset(field) == field, "Step offset matches original header");

    const sq::PatternBank a = sq::MakeDefaultPatternBank();
    const sq::PatternBank b = sq::MakeDefaultPatternBank();
    check(sq::ValidateBank(a) == sq::Status::Ok, "default bank validates");
    check(std::memcmp(&a, &b, sizeof(a)) == 0, "default construction deterministic");

    for (std::uint8_t pattern = 0; pattern < sq::kPatternCount; ++pattern)
    {
        check(a.patterns[pattern].repeat == p16b_original_default_repeat(),
              "default repeat matches source fixture");
        check(a.patterns[pattern].nextPattern == p16b_original_default_next(pattern),
              "default next pattern matches source fixture");
        for (std::uint8_t track = 0; track < sq::kTrackCount; ++track)
        {
            check(a.patterns[pattern].mainGates[track] ==
                      p16b_original_default_main_gates(), "default main gates");
            check(a.patterns[pattern].lengthCodes[track] ==
                      p16b_original_default_length_code(), "default length code");
            check(a.patterns[pattern].rotations[track] ==
                      p16b_original_default_rotation(), "default rotation");
            for (std::uint8_t position = 0;
                 position < sq::kPhysicalPositionCount; ++position)
            {
                std::uint8_t expected[7] {};
                p16b_original_default_step(position, expected);
                check(sameStep(a.patterns[pattern].steps[track][position], expected),
                      "default step matches source fixture");
            }
        }
    }
    check(a.shuffle == 0u, "default shuffle zero");
    check(sq::kInitialPattern == 0u && sq::kDefaultTempo == 120u,
          "runtime defaults source backed");

    for (std::uint8_t track = 0; track < sq::kTrackCount; ++track)
    {
        const auto identity = sq::IdentityForTrack(static_cast<sq::Track>(track));
        check(identity.synthesisOwner == p16b_original_owner(track), "track owner mapping");
        check(static_cast<std::uint8_t>(identity.articulation) ==
                  p16b_original_articulation(track), "track articulation mapping");
    }
}

void validationAndEditTests()
{
    sq::PatternBank bank = sq::MakeDefaultPatternBank();
    sq::StepData before {};
    check(sq::GetStep(bank, 0, 0, 0, before) == sq::Status::Ok, "get valid step");
    check(sq::GetStep(bank, 8, 0, 0, before) == sq::Status::InvalidPattern,
          "reject pattern 8");
    check(sq::GetStep(bank, 0, 7, 0, before) == sq::Status::InvalidTrack,
          "reject track 7");
    check(sq::GetStep(bank, 0, 0, 128, before) == sq::Status::InvalidPhysicalPosition,
          "reject position 128");
    check(!sq::IsValidMainStep(16) && !sq::IsValidSubstep(8), "index boundary rejection");
    check(sq::PhysicalPosition(15, 7) == 127 && sq::MainStepOf(127) == 15 &&
              sq::SubstepOf(127) == 7, "flattened boundary mapping");

    sq::StepData invalid = before;
    invalid.probability = 128;
    check(sq::SetStep(bank, 0, 0, 0, invalid) == sq::Status::InvalidProbability,
          "reject invalid probability");
    invalid = before; invalid.note = 128;
    check(sq::SetStep(bank, 0, 0, 0, invalid) == sq::Status::InvalidNote,
          "reject invalid note");
    invalid = before; invalid.parameter1 = 228;
    check(sq::SetStep(bank, 0, 0, 0, invalid) == sq::Status::InvalidAutomationParameter,
          "reject invalid lock identity");
    invalid = before; invalid.value2 = 128;
    check(sq::SetStep(bank, 0, 0, 0, invalid) == sq::Status::InvalidAutomationValue,
          "reject invalid lock value");
    sq::StepData after {};
    (void) sq::GetStep(bank, 0, 0, 0, after);
    check(std::memcmp(&before, &after, sizeof(before)) == 0, "rejected step edits preserve data");

    check(sq::SetTrackLength(bank, 0, 0, 0) == sq::Status::InvalidTrackLength,
          "reject effective length zero");
    check(sq::SetTrackLength(bank, 0, 0, 17) == sq::Status::InvalidTrackLength,
          "reject effective length 17");
    check(sq::SetTrackLength(bank, 0, 0, 1) == sq::Status::Ok &&
              bank.patterns[0].lengthCodes[0] == 1, "minimum track length");
    check(sq::SetTrackLength(bank, 0, 0, 16) == sq::Status::Ok &&
              bank.patterns[0].lengthCodes[0] == 0, "length 16 source encoding");
    check(sq::SetRotation(bank, 0, 0, 15) == sq::Status::Ok, "maximum rotation");
    check(sq::SetRotation(bank, 0, 0, 16) == sq::Status::InvalidRotation,
          "reject rotation 16");
    check(sq::SetPatternChain(bank, 0, 127, 14) == sq::Status::Ok,
          "maximum chain metadata");
    check(sq::SetPatternChain(bank, 0, 128, 14) == sq::Status::InvalidRepeat,
          "reject repeat 128");
    check(sq::SetPatternChain(bank, 0, 0, 15) == sq::Status::InvalidNextPattern,
          "reject next code 15");
    check(sq::SetShuffle(bank, 127) == sq::Status::Ok, "maximum shuffle");
    check(sq::SetShuffle(bank, 128) == sq::Status::InvalidShuffle, "reject shuffle 128");

    sq::StepData stored = before;
    stored.note = 91;
    (void) sq::SetStep(bank, 0, 0, 23, stored);
    check(sq::SetMainGate(bank, 0, 0, 2, true) == sq::Status::Ok,
          "set main gate");
    check(sq::SetMainGate(bank, 0, 0, 16, true) == sq::Status::InvalidMainStep,
          "reject main gate 16");
    bool gate = false;
    (void) sq::GetMainGate(bank, 0, 0, 2, gate);
    sq::StepData retained {};
    (void) sq::GetStep(bank, 0, 0, 23, retained);
    check(gate && retained.note == 91, "gate mutation preserves substep data");
    (void) sq::SetMainGate(bank, 0, 0, 2, false);
    (void) sq::GetStep(bank, 0, 0, 23, retained);
    check(retained.note == 91, "gate clear preserves substep data");

    (void) sq::SetPatternChain(bank, 0, 5, 3);
    sq::ResetPattern(bank, 0);
    check(bank.patterns[0].repeat == 5 && bank.patterns[0].nextPattern == 3,
          "source reset-pattern preserves chain metadata");
    (void) sq::SetPatternChain(bank, 1, 7, 4);
    bank.patterns[0].steps[0][0].note = 42;
    (void) sq::CopyPattern(bank, 0, 1);
    check(bank.patterns[1].steps[0][0].note == 42 &&
              bank.patterns[1].repeat == 7 && bank.patterns[1].nextPattern == 4,
          "source copy-pattern excludes chain metadata");

    sq::PatternBank corrupt = sq::MakeDefaultPatternBank();
    corrupt.patterns[0].steps[0][0].probability = 128;
    check(sq::ValidateBank(corrupt) == sq::Status::InvalidProbability,
          "whole-bank validation rejects corrupt probability");
    corrupt = sq::MakeDefaultPatternBank(); corrupt.patterns[0].lengthCodes[0] = 16;
    check(sq::ValidateBank(corrupt) == sq::Status::InvalidTrackLength,
          "whole-bank validation rejects corrupt length code");
    corrupt = sq::MakeDefaultPatternBank(); corrupt.patterns[0].rotations[0] = 16;
    check(sq::ValidateBank(corrupt) == sq::Status::InvalidRotation,
          "whole-bank validation rejects corrupt rotation");
    corrupt = sq::MakeDefaultPatternBank(); corrupt.patterns[0].repeat = 128;
    check(sq::ValidateBank(corrupt) == sq::Status::InvalidRepeat,
          "whole-bank validation rejects corrupt repeat");
    corrupt = sq::MakeDefaultPatternBank(); corrupt.patterns[0].nextPattern = 15;
    check(sq::ValidateBank(corrupt) == sq::Status::InvalidNextPattern,
          "whole-bank validation rejects corrupt next code");
    corrupt = sq::MakeDefaultPatternBank(); corrupt.shuffle = 128;
    check(sq::ValidateBank(corrupt) == sq::Status::InvalidShuffle,
          "whole-bank validation rejects corrupt shuffle");
}

void rngTests()
{
    sq::DeterministicRng known(0x12345678u);
    check(known.next() == 0x87985aa5u && known.next() == 0x155b24a3u &&
              known.next() == 0x4820f4c4u && known.next() == 0x81b3ac98u,
          "xorshift32 sequence is fixed explicitly");
    sq::DeterministicRng a(0x12345678u), b(0x12345678u), c(0x87654321u);
    bool same = true;
    bool different = false;
    for (int i = 0; i < 10000; ++i)
    {
        const auto av = a.next();
        const auto bv = b.next();
        const auto cv = c.next();
        same = same && av == bv;
        different = different || av != cv;
    }
    check(same, "equal seeds reproduce sequence");
    check(different, "independent seeds diverge");
    a.reset(); b.reset(0x12345678u);
    check(a.next() == b.next(), "reset and replay deterministic");
    sq::DeterministicRng zero(0u);
    check(zero.next() == 0u && zero.next() == 0u,
          "zero seed has defined non-undefined degenerate sequence");
    check(sq::ProbabilityPasses(0, 0), "probability-zero source quirk");
    check(!sq::ProbabilityPasses(1, 0), "probability zero rejects nonzero RNG");
    check(sq::ProbabilityPasses(127, 127), "probability maximum always passes");

    sq::ClusterProbability cluster;
    cluster.reset(91);
    sq::DeterministicRng source(1);
    check(cluster.boundary(sq::Track::Drum1, false, source) == 91,
          "inactive cluster boundary reuses previous RNG");
    const std::uint8_t sampled = cluster.boundary(sq::Track::Drum1, true, source);
    check(sampled == cluster.value(sq::Track::Drum1), "active boundary latches RNG");
    check(cluster.value(sq::Track::Drum2) == 91, "probability latches are per track");
}

std::uint64_t hashEvent(std::uint64_t hash, const sq::ClockEvent& event)
{
    hash ^= event.quantizedNativeFrame + 0x9e3779b97f4a7c15ull +
            (hash << 6u) + (hash >> 2u);
    hash ^= event.ordinal + event.physicalPosition;
    return hash;
}

struct ClockDigest { std::uint64_t hash; std::uint64_t count; };

ClockDigest renderClock(const std::array<std::uint64_t, 6>& chunks,
                        std::uint64_t endFrame)
{
    sq::SequencerClock clock;
    clock.start();
    std::array<sq::ClockEvent, 64> events {};
    ClockDigest digest {1469598103934665603ull, 0u};
    std::uint64_t cursor = 0;
    std::size_t chunk = 0;
    while (cursor < endFrame)
    {
        const std::uint64_t size = chunks[chunk++ % chunks.size()];
        const std::uint64_t next = size > endFrame - cursor ? endFrame : cursor + size;
        for (;;)
        {
            std::size_t written = 0;
            const sq::Status status = clock.advanceTo(next, events.data(), events.size(), written);
            for (std::size_t i = 0; i < written; ++i)
            {
                digest.hash = hashEvent(digest.hash, events[i]);
                ++digest.count;
            }
            if (status == sq::Status::Ok)
                break;
            check(status == sq::Status::InsufficientCapacity, "clock capacity is resumable");
        }
        cursor = next;
    }
    return digest;
}

void clockTests()
{
    sq::SequencerClock clock;
    check(clock.tempo() == 120, "default tempo 120");
    check(clock.setTempo(0) == sq::Status::InvalidTempo, "reject external-sync zero as internal tempo");
    check(clock.setTempo(1) == sq::Status::Ok, "minimum internal tempo");
    check(clock.setTempo(255) == sq::Status::Ok, "maximum internal tempo");
    check(clock.setTempo(120) == sq::Status::Ok, "restore 120 BPM");
    check(std::fabs(p16b_original_internal_tick_quarter_ms(120) -
                    (125.0f / 6.0f)) < 0.00001f, "original-C clock expression");

    clock.reset();
    clock.start();
    std::array<sq::ClockEvent, 16> events {};
    std::size_t written = 0;
    check(clock.advanceTo(1, events.data(), events.size(), written) == sq::Status::Ok &&
              written == 1 && events[0].quantizedNativeFrame == 0,
          "start event immediate");
    check(clock.nextQuantizedFrame() == 704, "120 BPM second position native-block quantized");

    // Independent integer oracle: floor(i * rate * 60 / (128 * 32 * bpm)).
    sq::ClockEvent last {};
    sq::ClockEvent mainBoundary {};
    sq::ClockEvent patternBoundary {};
    for (std::uint64_t i = 1; i < 1000000u; ++i)
    {
        const std::uint64_t end = clock.nextQuantizedFrame() + 1u;
        written = 0;
        const sq::Status status = clock.advanceTo(end, &last, 1, written);
        check(status == sq::Status::Ok && written == 1 && last.ordinal == i,
              "million-position clock has no duplicate or omitted ordinal");
        if (i == 8u) mainBoundary = last;
        if (i == 128u) patternBoundary = last;
    }
    const std::uint64_t oracle =
        (999999ull * sq::kClockStepNumerator) /
        (sq::kClockDenominatorScale * 120ull);
    check(last.ordinal == 999999u && last.exactFrameFloor == oracle,
          "million-position clock has zero rational drift");
    check(mainBoundary.exactFrameFloor ==
              (8ull * sq::kClockStepNumerator) /
                  (sq::kClockDenominatorScale * 120ull),
          "one main step matches independent oracle");
    check(patternBoundary.exactFrameFloor ==
              (128ull * sq::kClockStepNumerator) /
                  (sq::kClockDenominatorScale * 120ull),
          "one complete pattern matches independent oracle");

    sq::SequencerClock slow;
    (void) slow.setTempo(1); slow.start(); written = 0;
    (void) slow.advanceTo(1, events.data(), events.size(), written);
    check(slow.nextExactFrameFloor() ==
              sq::kClockStepNumerator / sq::kClockDenominatorScale,
          "minimum-tempo interval oracle");
    sq::SequencerClock fast;
    (void) fast.setTempo(255); fast.start(); written = 0;
    (void) fast.advanceTo(1, events.data(), events.size(), written);
    check(fast.nextExactFrameFloor() ==
              sq::kClockStepNumerator / (sq::kClockDenominatorScale * 255ull),
          "maximum-tempo interval oracle");

    const auto uniform = renderClock({{4096,4096,4096,4096,4096,4096}}, 1000000);
    const auto variable = renderClock({{1,17,255,1024,31,511}}, 1000000);
    check(uniform.count == variable.count && uniform.hash == variable.hash,
          "clock timeline invariant to caller chunk sizes");

    sq::SequencerClock transport;
    transport.start();
    written = 0; (void) transport.advanceTo(10000, events.data(), events.size(), written);
    transport.stop();
    written = 99; (void) transport.advanceTo(12000, events.data(), events.size(), written);
    check(written == 0, "stop emits no positions");
    transport.resume();
    written = 0; (void) transport.advanceTo(12033, events.data(), events.size(), written);
    check(written == 1 && events[0].ordinal == 0 &&
              events[0].quantizedNativeFrame == 12000,
          "resume follows source restart semantics");
    check(transport.advanceTo(11000, events.data(), events.size(), written) ==
              sq::Status::NonMonotonicTime, "reject backwards clock advance");

    sq::SequencerClock tempoChange;
    tempoChange.start();
    written = 0; (void) tempoChange.advanceTo(1, events.data(), events.size(), written);
    (void) tempoChange.setTempo(60);
    check(tempoChange.nextQuantizedFrame() == 1376,
          "tempo change applies from defined last boundary");

    check((clock.nextQuantizedFrame() % sq::kNativeBlockFrames) == 0,
          "native timestamp is block aligned");
    const std::uint64_t exact = clock.nextExactFrameFloor();
    check(clock.nextQuantizedFrame() >= exact &&
              clock.nextQuantizedFrame() - exact <= sq::kNativeBlockFrames,
          "native quantization bounded by one block");
}

void authorityTests()
{
    sq::PatternStateAuthority authority;
    gMetrics.authorityBytes = sizeof(authority);
    gMetrics.payloadBytes = sq::PatternStateAuthority::payloadBytes();
    sq::PatternStateAuthority::ReadHandle initial;
    check(authority.tryAcquire(initial) == sq::Status::Ok,
          "acquire initial snapshot");
    check(initial.generation() == 0 && sq::ValidateBank(initial.bank()) == sq::Status::Ok,
          "initial snapshot coherent");
    initial.release();

    sq::PatternStateAuthority::WriteHandle edit;
    check(authority.tryBeginEdit(edit) == sq::Status::Ok, "begin bounded edit");
    sq::StepData changed = edit.candidate().patterns[0].steps[0][0];
    changed.note = 17;
    check(edit.setStep(0, 0, 0, changed) == sq::Status::Ok,
          "edit unpublished candidate");
    std::uint64_t generation = 0;
    check(authority.commit(edit, &generation) == sq::Status::Ok && generation == 1,
          "commit coherent generation");
    sq::PatternStateAuthority::ReadHandle current;
    (void) authority.tryAcquire(current);
    check(current.bank().patterns[0].steps[0][0].note == 17 &&
              current.generation() == 1, "reader sees complete committed edit");
    current.release();

    sq::PatternStateAuthority::WriteHandle rejected;
    (void) authority.tryBeginEdit(rejected);
    sq::StepData invalid = rejected.candidate().patterns[0].steps[0][0];
    invalid.note = 128;
    check(rejected.setStep(0, 0, 0, invalid) == sq::Status::InvalidNote,
          "authority rejects invalid edit");
    rejected.cancel();
    (void) authority.tryAcquire(current);
    check(current.bank().patterns[0].steps[0][0].note == 17,
          "rejected authority edit preserves published state");
    current.release();

    // Pin both inactive historical slots. The third is active, so publication
    // must return bounded backpressure rather than overwrite a reader.
    sq::PatternStateAuthority pins;
    sq::PatternStateAuthority::ReadHandle pin0, pin1;
    (void) pins.tryAcquire(pin0);
    sq::PatternBank candidate = sq::MakeDefaultPatternBank();
    candidate.shuffle = 1; (void) pins.tryPublish(candidate);
    check(pin0.bank().shuffle == 0, "pinned historical snapshot remains immutable");
    (void) pins.tryAcquire(pin1);
    check(pin1.bank().shuffle == 1, "new active snapshot is complete");
    candidate.shuffle = 2; (void) pins.tryPublish(candidate);
    sq::PatternStateAuthority::WriteHandle blocked;
    check(pins.tryBeginEdit(blocked) == sq::Status::Busy,
          "pinned inactive slots produce bounded backpressure");
    ++gMetrics.publicationBusy;
    pin0.release();
    check(pins.tryBeginEdit(blocked) == sq::Status::Ok,
          "publication resumes after reader release");
    blocked.cancel();
    pin1.release();

    sq::PatternStateAuthority other;
    candidate.shuffle = 9; (void) authority.tryPublish(candidate);
    (void) other.tryAcquire(current);
    check(current.bank().shuffle == 0, "multiple authorities are isolated");
    current.release();

    sq::PatternBank corrupt = sq::MakeDefaultPatternBank();
    corrupt.shuffle = 128;
    const std::uint64_t beforeGeneration = authority.generation();
    check(authority.tryPublish(corrupt) == sq::Status::InvalidShuffle &&
              authority.generation() == beforeGeneration,
          "invalid whole-bank publication cannot partially mutate authority");

    sq::PatternStateAuthority concurrent;
    std::atomic<bool> done {false};
    std::atomic<bool> torn {false};
    std::atomic<unsigned> ready {0};
    std::atomic<std::uint64_t> snapshots {0};
    auto reader = [&]() {
        ready.fetch_add(1u, std::memory_order_release);
        while (!done.load(std::memory_order_acquire))
        {
            sq::PatternStateAuthority::ReadHandle handle;
            if (concurrent.tryAcquire(handle) != sq::Status::Ok)
                continue;
            const std::uint8_t expected = handle.bank().patterns[0].steps[0][0].note;
            for (const auto& pattern : handle.bank().patterns)
                for (const auto& track : pattern.steps)
                    for (const auto& step : track)
                        if (step.note != expected) torn.store(true, std::memory_order_relaxed);
            snapshots.fetch_add(1u, std::memory_order_relaxed);
        }
    };
    std::thread r1(reader), r2(reader);
    while (ready.load(std::memory_order_acquire) != 2u)
        std::this_thread::yield();
    std::uint64_t busy = 0;
    for (std::uint8_t marker = 1; marker <= 40; ++marker)
    {
        bool published = false;
        for (unsigned attempt = 0; attempt < 10000 && !published; ++attempt)
        {
            sq::PatternStateAuthority::WriteHandle writer;
            if (concurrent.tryBeginEdit(writer) != sq::Status::Ok)
            {
                ++busy;
                std::this_thread::yield();
                continue;
            }
            for (std::uint8_t pattern = 0; pattern < sq::kPatternCount; ++pattern)
                for (std::uint8_t track = 0; track < sq::kTrackCount; ++track)
                    for (std::uint8_t position = 0;
                         position < sq::kPhysicalPositionCount; ++position)
                    {
                        sq::StepData value = writer.candidate().patterns[pattern]
                            .steps[track][position];
                        value.note = marker;
                        (void) writer.setStep(pattern, track, position, value);
                    }
            published = concurrent.commit(writer) == sq::Status::Ok;
        }
        check(published, "concurrent publication completes within bounded test attempts");
    }
    done.store(true, std::memory_order_release);
    r1.join(); r2.join();
    gMetrics.publicationBusy += busy;
    gMetrics.readerSnapshots = snapshots.load(std::memory_order_relaxed);
    check(!torn.load(std::memory_order_relaxed), "concurrent readers never observe torn bank");
    check(gMetrics.readerSnapshots > 0, "concurrent readers acquired snapshots");
}

void realTimeSafetyAndCostTests()
{
    sq::SequencerClock clock;
    sq::PatternStateAuthority authority;
    std::array<sq::ClockEvent, 2> events {};

    const std::uint64_t allocationBefore = gAllocations.load(std::memory_order_relaxed);
    clock.start();
    const auto clockStart = std::chrono::steady_clock::now();
    for (std::uint64_t i = 0; i < 100000; ++i)
    {
        std::size_t written = 0;
        (void) clock.advanceTo(clock.nextQuantizedFrame() + 1u,
                               events.data(), events.size(), written);
    }
    const auto clockEnd = std::chrono::steady_clock::now();

    const auto acquireStart = std::chrono::steady_clock::now();
    for (int i = 0; i < 100000; ++i)
    {
        sq::PatternStateAuthority::ReadHandle handle;
        (void) authority.tryAcquire(handle);
    }
    const auto acquireEnd = std::chrono::steady_clock::now();

    const auto editStart = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i)
    {
        sq::PatternStateAuthority::WriteHandle handle;
        (void) authority.tryBeginEdit(handle);
        (void) handle.setShuffle(static_cast<std::uint8_t>(i & 0x7f));
        (void) authority.commit(handle);
    }
    const auto editEnd = std::chrono::steady_clock::now();
    const std::uint64_t allocationAfter = gAllocations.load(std::memory_order_relaxed);

    gMetrics.allocationDelta = allocationAfter - allocationBefore;
    gMetrics.clockNanosecondsPerEvent = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(clockEnd - clockStart).count() / 100000);
    gMetrics.acquireNanoseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(acquireEnd - acquireStart).count() / 100000);
    gMetrics.editNanoseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(editEnd - editStart).count() / 100);
    check(gMetrics.allocationDelta == 0, "clock and authority operations allocate zero heap blocks");
}

} // namespace

int main()
{
    dataModelAndDefaultTests();
    validationAndEditTests();
    rngTests();
    clockTests();
    authorityTests();
    realTimeSafetyAndCostTests();
    std::printf(
        "{\"pass\":%d,\"fail\":%d,\"pattern_bank_bytes\":%zu,"
        "\"authority_bytes\":%llu,\"slot_payload_bytes\":%llu,"
        "\"allocation_delta\":%llu,\"clock_ns_per_event\":%llu,"
        "\"acquire_ns\":%llu,\"edit_ns\":%llu,"
        "\"publication_busy\":%llu,\"reader_snapshots\":%llu}\n",
        gPass, gFail, sizeof(sq::PatternBank),
        static_cast<unsigned long long>(gMetrics.authorityBytes),
        static_cast<unsigned long long>(gMetrics.payloadBytes),
        static_cast<unsigned long long>(gMetrics.allocationDelta),
        static_cast<unsigned long long>(gMetrics.clockNanosecondsPerEvent),
        static_cast<unsigned long long>(gMetrics.acquireNanoseconds),
        static_cast<unsigned long long>(gMetrics.editNanoseconds),
        static_cast<unsigned long long>(gMetrics.publicationBusy),
        static_cast<unsigned long long>(gMetrics.readerSnapshots));
    return gFail == 0 ? 0 : 1;
}
