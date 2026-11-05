#pragma once

#include "PatternBank.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace lxr::sequencer
{

// Three immutable fixed-size slots provide wait-free reader acquisition in the
// uncontended case and bounded writer backpressure. A slot may be overwritten
// only after it is inactive, atomically claimed, and has zero pinned readers.
class PatternStateAuthority final
{
public:
    static constexpr std::size_t kSlotCount = 3;
    static constexpr unsigned kDefaultAcquireAttempts = 8;

    class ReadHandle final
    {
    public:
        ReadHandle() noexcept = default;
        ReadHandle(const ReadHandle&) = delete;
        ReadHandle& operator=(const ReadHandle&) = delete;
        ~ReadHandle();

        bool active() const noexcept { return mOwner != nullptr; }
        const PatternBank& bank() const noexcept;
        std::uint64_t generation() const noexcept { return mGeneration; }
        void release() noexcept;

    private:
        friend class PatternStateAuthority;
        PatternStateAuthority* mOwner = nullptr;
        const PatternBank* mBank = nullptr;
        std::uint8_t mSlot = 0;
        std::uint64_t mGeneration = 0;
    };

    class WriteHandle final
    {
    public:
        WriteHandle() noexcept = default;
        WriteHandle(const WriteHandle&) = delete;
        WriteHandle& operator=(const WriteHandle&) = delete;
        ~WriteHandle();

        bool active() const noexcept { return mOwner != nullptr; }
        std::uint64_t baseGeneration() const noexcept { return mBaseGeneration; }

        Status setStep(std::uint8_t pattern, std::uint8_t track,
                       std::uint8_t position, const StepData& value) noexcept;
        Status setMainGate(std::uint8_t pattern, std::uint8_t track,
                           std::uint8_t mainStep, bool active) noexcept;
        Status setTrackLength(std::uint8_t pattern, std::uint8_t track,
                              std::uint8_t effectiveLength) noexcept;
        Status setRotation(std::uint8_t pattern, std::uint8_t track,
                           std::uint8_t rotation) noexcept;
        Status setPatternChain(std::uint8_t pattern, std::uint8_t repeat,
                               std::uint8_t nextPattern) noexcept;
        Status setShuffle(std::uint8_t shuffle) noexcept;
        Status resetPattern(std::uint8_t pattern) noexcept;
        Status resetBank() noexcept;
        Status copyPattern(std::uint8_t source, std::uint8_t destination) noexcept;
        Status replace(const PatternBank& candidate) noexcept;
        const PatternBank& candidate() const noexcept;
        void cancel() noexcept;

    private:
        friend class PatternStateAuthority;
        PatternBank* mutableCandidate() noexcept;
        PatternStateAuthority* mOwner = nullptr;
        std::uint8_t mSlot = 0;
        std::uint64_t mBaseGeneration = 0;
    };

    PatternStateAuthority() noexcept;
    explicit PatternStateAuthority(const PatternBank& initial) noexcept;
    PatternStateAuthority(const PatternStateAuthority&) = delete;
    PatternStateAuthority& operator=(const PatternStateAuthority&) = delete;

    Status initialize(const PatternBank& initial) noexcept;
    Status tryAcquire(ReadHandle& handle,
                      unsigned attempts = kDefaultAcquireAttempts) noexcept;
    Status tryBeginEdit(WriteHandle& handle) noexcept;
    Status commit(WriteHandle& handle, std::uint64_t* generation = nullptr) noexcept;
    Status tryPublish(const PatternBank& candidate,
                      std::uint64_t* generation = nullptr) noexcept;

    std::uint64_t generation() const noexcept
    {
        return mGeneration.load(std::memory_order_acquire);
    }

    static constexpr std::size_t payloadBytes() noexcept
    {
        return sizeof(PatternBank) * kSlotCount;
    }

private:
    enum class SlotState : std::uint8_t { Available = 0, Writing = 1 };

    struct Slot
    {
        PatternBank bank {};
        std::uint64_t generation = 0;
        std::atomic<std::uint32_t> readers {0};
        std::atomic<SlotState> state {SlotState::Available};
    };

    void release(ReadHandle& handle) noexcept;
    void cancel(WriteHandle& handle) noexcept;
    PatternBank* candidate(WriteHandle& handle) noexcept;
    const PatternBank* candidate(const WriteHandle& handle) const noexcept;

    std::array<Slot, kSlotCount> mSlots {};
    std::atomic<std::uint8_t> mActive {0};
    std::atomic<std::uint64_t> mGeneration {0};
    std::atomic_flag mWriter = ATOMIC_FLAG_INIT;
};

static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "Pattern generations must be lock-free on the x64 target");
static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
              "Pattern reader pins must be lock-free on the x64 target");

} // namespace lxr::sequencer
