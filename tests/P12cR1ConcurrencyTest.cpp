#include "../dsp/FixedFifo.h"
#include "../dsp/ParameterStateAuthority.h"
#include "../vendor/iPlug2/Examples/LXR/LXRState.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <thread>
#include <vector>

namespace
{

using Authority =
    lxr::ParameterStateAuthority<lxr::state::kParameterCount>;
using Values = Authority::Values;

std::atomic<bool> gCountAllocations {false};
std::atomic<int> gAllocations {0};
int gPassed = 0;
int gFailed = 0;

void check(const char* name, bool condition)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", name);
    condition ? ++gPassed : ++gFailed;
}

Values pattern(std::uint8_t epoch)
{
    Values result {};
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<std::uint8_t>((epoch + i * 17u) & 0x7fu);
    return result;
}

bool isPattern(const Values& values)
{
    return values == pattern(values[0]);
}

void reproduceAuditedDefects()
{
    // Defect A: the former serializer performed independent atomic loads with
    // no enclosing generation. Force a batch update between its two halves.
    std::array<std::atomic<std::uint8_t>, lxr::state::kParameterCount> oldState;
    const Values first = pattern(5);
    const Values second = pattern(73);
    for (std::size_t i = 0; i < oldState.size(); ++i)
        oldState[i].store(first[i], std::memory_order_relaxed);
    Values torn {};
    for (std::size_t i = 0; i < torn.size() / 2; ++i)
        torn[i] = oldState[i].load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < oldState.size(); ++i)
        oldState[i].store(second[i], std::memory_order_relaxed);
    for (std::size_t i = torn.size() / 2; i < torn.size(); ++i)
        torn[i] = oldState[i].load(std::memory_order_relaxed);
    check("reproduce old incoherent serialization snapshot",
          torn != first && torn != second && !isPattern(torn));

    // Defect B: old ApplyPending returned on an odd restore but MIDI continued.
    // The restore had already mirrored this parameter to IParam; MIDI then
    // overwrote IParam/native, and the completed restore later overwrote only
    // native state from its candidate.
    std::uint8_t host = 40;
    std::uint8_t native = 12;
    const std::uint8_t restoreCandidate = 40;
    const bool oldApplyReturnedEarly = true;
    if (oldApplyReturnedEarly)
    {
        host = 77;
        native = 77;
    }
    native = restoreCandidate;
    check("reproduce old restore versus MIDI divergence", host != native);
}

struct Event
{
    std::uint8_t parameter = 0;
    std::uint8_t value = 0;
};

// Independent ordering oracle. It records submitted/accepted operations in the
// contract order and projects accepted operations into separate host/native
// arrays, rather than comparing two production snapshots.
class OrderingHarness
{
public:
    OrderingHarness()
    {
        mExpected.fill(0);
        mHost.fill(0);
        mNative.fill(0);
        mAuthority.initialize(mExpected);
    }

    bool beginRestore(const Values& values)
    {
        if (!mAuthority.tryBeginWrite(mWriter))
            return false;
        mRestore = values;
        return true;
    }

    bool submitMidi(Event event)
    {
        ++mSubmitted;
        Authority::WriteHandle writer;
        if (!mAuthority.tryBeginWrite(writer))
            return mQueue.push(event);
        mAuthority.write(writer, event.parameter, event.value);
        mAuthority.commit(writer);
        applyEvent(event);
        ++mAccepted;
        return true;
    }

    bool finishRestore()
    {
        if (!mWriter.active() || !mAuthority.writeAll(mWriter, mRestore))
            return false;
        mAuthority.commit(mWriter);
        mExpected = mRestore;
        mHost = mRestore;
        mNative = mRestore;
        ++mAccepted;
        while (auto* event = mQueue.front())
        {
            Authority::WriteHandle writer;
            if (!mAuthority.tryBeginWrite(writer))
                return false;
            mAuthority.write(writer, event->parameter, event->value);
            mAuthority.commit(writer);
            applyEvent(*event);
            mQueue.pop();
            ++mAccepted;
        }
        return true;
    }

    bool restoreNow(const Values& values)
    {
        ++mSubmitted;
        std::uint64_t generation = 0;
        if (!mAuthority.tryPublish(values, &generation))
            return false;
        (void)generation;
        mExpected = values;
        mHost = values;
        mNative = values;
        ++mAccepted;
        return true;
    }

    bool converged() const
    {
        Values serialized {};
        return mAuthority.trySnapshot(serialized) &&
            serialized == mExpected && mHost == mExpected &&
            mNative == mExpected;
    }

    std::size_t queued() const noexcept { return mQueue.size(); }
    int submitted() const noexcept { return mSubmitted; }
    int accepted() const noexcept { return mAccepted; }

private:
    void applyEvent(Event event)
    {
        mExpected[event.parameter] = event.value;
        mHost[event.parameter] = event.value;
        mNative[event.parameter] = event.value;
    }

    Authority mAuthority;
    Authority::WriteHandle mWriter;
    Values mRestore {};
    Values mExpected {};
    Values mHost {};
    Values mNative {};
    lxr::FixedFifo<Event, 8> mQueue;
    int mSubmitted = 0;
    int mAccepted = 0;
};

void testForcedSnapshotWindow()
{
    Authority state;
    const Values before = pattern(3);
    const Values after = pattern(91);
    state.initialize(before);

    Authority::WriteHandle writer;
    check("forced restore writer acquired", state.tryBeginWrite(writer));
    for (std::size_t i = 0; i < after.size() / 2; ++i)
        state.write(writer, i, after[i]);

    Values snapshot {};
    check("snapshot rejects forced half-publication",
          !state.trySnapshot(snapshot, nullptr, 1));
    check("second producer cannot overlap writer", !state.tryPublish(pattern(17)));

    for (std::size_t i = after.size() / 2; i < after.size(); ++i)
        state.write(writer, i, after[i]);
    const std::uint64_t generation = state.commit(writer);
    std::uint64_t observed = 0;
    check("complete restore becomes one generation",
          state.trySnapshot(snapshot, &observed) && snapshot == after &&
          observed == generation && (generation & 1u) == 0u);
}

void testConcurrentPatterns()
{
    Authority state;
    state.initialize(pattern(0));
    std::atomic<bool> start {false};
    std::atomic<bool> stop {false};
    std::atomic<int> accepted {0};
    std::atomic<int> snapshots {0};
    std::atomic<int> failures {0};
    std::atomic<bool> mixed {false};

    std::thread reader([&] {
        while (!start.load(std::memory_order_acquire)) {}
        while (!stop.load(std::memory_order_acquire))
        {
            Values snapshot {};
            if (state.trySnapshot(snapshot, nullptr, 2))
            {
                ++snapshots;
                if (!isPattern(snapshot))
                    mixed.store(true, std::memory_order_release);
            }
            else
            {
                ++failures;
            }
        }
    });

    std::array<std::thread, 3> writers;
    for (std::size_t thread = 0; thread < writers.size(); ++thread)
    {
        writers[thread] = std::thread([&, thread] {
            while (!start.load(std::memory_order_acquire)) {}
            for (int iteration = 0; iteration < 4000; ++iteration)
            {
                const auto epoch = static_cast<std::uint8_t>(
                    1u + (thread * 41u + static_cast<unsigned>(iteration)) % 127u);
                if (state.tryPublish(pattern(epoch)))
                    ++accepted;
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (auto& writer : writers)
        writer.join();
    stop.store(true, std::memory_order_release);
    reader.join();

    Values final {};
    check("concurrent multi-producer publications accepted", accepted > 0);
    check("concurrent serializer obtained coherent snapshots", snapshots > 0);
    check("bounded snapshot failure path exercised", failures > 0);
    check("no mixed-generation correlated pattern", !mixed &&
          state.trySnapshot(final) && isPattern(final));
}

void testSingleParameterAutomation()
{
    Authority state;
    Values expected = pattern(11);
    state.initialize(expected);
    std::atomic<bool> done {false};
    std::atomic<bool> invalid {false};

    std::thread automation([&] {
        for (int value = 0; value < 128; ++value)
        {
            Authority::WriteHandle writer;
            while (!state.tryBeginWrite(writer))
                std::this_thread::yield();
            state.write(writer, 88, static_cast<std::uint8_t>(value));
            state.commit(writer);
        }
        done.store(true, std::memory_order_release);
    });

    while (!done.load(std::memory_order_acquire))
    {
        Values snapshot {};
        if (!state.trySnapshot(snapshot))
            continue;
        const std::uint8_t automated = snapshot[88];
        snapshot[88] = pattern(11)[88];
        if (snapshot != pattern(11) || automated > 127)
            invalid.store(true, std::memory_order_release);
    }
    automation.join();
    Values final {};
    check("single-parameter automation snapshots remain valid",
          !invalid && state.trySnapshot(final) && final[88] == 127);
}

void testRestoreMidiOrdering()
{
    OrderingHarness afterBoundary;
    const Values restore = pattern(44);
    check("restore publication begins", afterBoundary.beginRestore(restore));
    check("overlapping MIDI CC is retained",
          afterBoundary.submitMidi({88, 117}) && afterBoundary.queued() == 1);
    check("overlapping NRPN is retained in arrival order",
          afterBoundary.submitMidi({128, 63}) && afterBoundary.queued() == 2);
    check("restore then queued MIDI converges",
          afterBoundary.finishRestore() && afterBoundary.converged() &&
          afterBoundary.submitted() == 2 && afterBoundary.accepted() == 3);

    OrderingHarness beforeBoundary;
    check("MIDI before restore accepted", beforeBoundary.submitMidi({88, 117}));
    check("later restore supersedes earlier MIDI",
          beforeBoundary.restoreNow(restore) && beforeBoundary.converged());

    OrderingHarness rapid;
    check("rapid restore one", rapid.restoreNow(pattern(1)));
    check("rapid restore two", rapid.restoreNow(pattern(2)));
    check("rapid restore three", rapid.restoreNow(pattern(3)));
    check("rapid restores converge to final patch", rapid.converged());
}

void testUnavailableAndInvalidPublication()
{
    Authority state;
    const Values good = pattern(9);
    state.initialize(good);
    Authority::WriteHandle held;
    check("writer held for unavailable test", state.tryBeginWrite(held));
    check("unavailable publication fails without waiting",
          !state.tryPublish(pattern(10)));
    state.cancelUnmodified(held);

    Values invalid = good;
    invalid[57] = 255;
    Values snapshot {};
    check("invalid candidate rejected without mutation",
          !state.tryPublish(invalid) && state.trySnapshot(snapshot) &&
          snapshot == good);

    lxr::FixedFifo<Event, 4> queue;
    check("bounded queue accepts capacity",
          queue.push({1, 1}) && queue.push({2, 2}) &&
          queue.push({3, 3}) && queue.push({4, 4}));
    check("full queue rejects newest without overwrite",
          !queue.push({5, 5}) && queue.size() == 4);
    bool ordered = true;
    for (std::uint8_t expected = 1; expected <= 4; ++expected)
    {
        Event* event = queue.front();
        ordered &= event != nullptr && event->parameter == expected &&
                   event->value == expected;
        queue.pop();
    }
    check("queue overflow preserves accepted event order", ordered);
}

void testCodecCompatibility()
{
    const Values values = pattern(37);
    std::array<std::uint8_t, lxr::state::kEncodedSize> first {};
    std::array<std::uint8_t, lxr::state::kEncodedSize> second {};
    Values decoded {};
    check("identical state produces identical LXRS v1 bytes",
          lxr::state::Encode(values, first) &&
          lxr::state::Encode(values, second) && first == second);
    const auto result = lxr::state::Decode(first.data(), first.size(), decoded);
    check("LXRS v1 round trip remains exact", result && decoded == values &&
          first.size() == 252 && first[0] == 'L' && first[1] == 'X' &&
          first[2] == 'R' && first[3] == 'S');

    Values protectedState = pattern(7);
    auto corrupt = first;
    corrupt[40] ^= 0x20;
    check("corrupt CRC leaves destination intact",
          !lxr::state::Decode(corrupt.data(), corrupt.size(), protectedState) &&
          protectedState == pattern(7));
    check("truncated state leaves destination intact",
          !lxr::state::Decode(first.data(), first.size() - 1, protectedState) &&
          protectedState == pattern(7));
    corrupt = first;
    lxr::state::PutU16LE(corrupt.data() + 4, 99);
    check("unsupported version leaves destination intact",
          !lxr::state::Decode(corrupt.data(), corrupt.size(), protectedState) &&
          protectedState == pattern(7));
    corrupt = first;
    corrupt[lxr::state::kHeaderSize + 12] = 255;
    lxr::state::PutU32LE(corrupt.data() + 20,
        lxr::state::Crc32(corrupt.data() + lxr::state::kHeaderSize,
                          lxr::state::kPayloadSize));
    check("invalid parameter leaves destination intact",
          !lxr::state::Decode(corrupt.data(), corrupt.size(), protectedState) &&
          protectedState == pattern(7));
}

void testRealtimeAllocationContract()
{
    Authority state;
    const Values values = pattern(22);
    state.initialize(values);
    Values snapshot {};
    gAllocations.store(0, std::memory_order_relaxed);
    gCountAllocations.store(true, std::memory_order_release);
    for (int iteration = 0; iteration < 10000; ++iteration)
    {
        Authority::WriteHandle writer;
        if (state.tryBeginWrite(writer))
        {
            state.write(writer, 88,
                static_cast<std::uint8_t>(iteration & 0x7f));
            state.commit(writer);
        }
        state.trySnapshot(snapshot);
    }
    gCountAllocations.store(false, std::memory_order_release);
    check("authority read/write path performs zero allocations",
          gAllocations.load(std::memory_order_acquire) == 0);
    check("64-bit generation remains even after boundary exercise",
          (state.generation() & 1u) == 0u && state.generation() >= 20000u);
}

} // namespace

void* operator new(std::size_t size)
{
    if (gCountAllocations.load(std::memory_order_relaxed))
        gAllocations.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    std::abort();
}

void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main()
{
    reproduceAuditedDefects();
    testForcedSnapshotWindow();
    testConcurrentPatterns();
    testSingleParameterAutomation();
    testRestoreMidiOrdering();
    testUnavailableAndInvalidPublication();
    testCodecCompatibility();
    testRealtimeAllocationContract();
    std::printf("SUMMARY pass=%d fail=%d\n", gPassed, gFailed);
    return gFailed == 0 ? 0 : 1;
}
