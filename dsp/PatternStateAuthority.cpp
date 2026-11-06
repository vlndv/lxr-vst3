#include "PatternStateAuthority.h"

#include <limits>

namespace lxr::sequencer
{

PatternStateAuthority::ReadHandle::~ReadHandle()
{
    release();
}

const PatternBank& PatternStateAuthority::ReadHandle::bank() const noexcept
{
    return *mBank; // Precondition: active() is true.
}

void PatternStateAuthority::ReadHandle::release() noexcept
{
    if (mOwner != nullptr)
        mOwner->release(*this);
}

PatternStateAuthority::WriteHandle::~WriteHandle()
{
    cancel();
}

PatternBank* PatternStateAuthority::WriteHandle::mutableCandidate() noexcept
{
    return mOwner != nullptr ? mOwner->candidate(*this) : nullptr;
}

const PatternBank& PatternStateAuthority::WriteHandle::candidate() const noexcept
{
    const PatternBank* value = mOwner != nullptr ? mOwner->candidate(*this) : nullptr;
    return *value; // Precondition: active() is true.
}

Status PatternStateAuthority::WriteHandle::setStep(
    std::uint8_t pattern, std::uint8_t track, std::uint8_t position,
    const StepData& value) noexcept
{
    PatternBank* bank = mutableCandidate();
    return bank != nullptr
        ? SetStep(*bank, pattern, track, position, value) : Status::Stale;
}

Status PatternStateAuthority::WriteHandle::setMainGate(
    std::uint8_t pattern, std::uint8_t track, std::uint8_t mainStep,
    bool active) noexcept
{
    PatternBank* bank = mutableCandidate();
    return bank != nullptr
        ? SetMainGate(*bank, pattern, track, mainStep, active) : Status::Stale;
}

Status PatternStateAuthority::WriteHandle::setTrackLength(
    std::uint8_t pattern, std::uint8_t track,
    std::uint8_t effectiveLength) noexcept
{
    PatternBank* bank = mutableCandidate();
    return bank != nullptr
        ? SetTrackLength(*bank, pattern, track, effectiveLength) : Status::Stale;
}

Status PatternStateAuthority::WriteHandle::setRotation(
    std::uint8_t pattern, std::uint8_t track, std::uint8_t rotation) noexcept
{
    PatternBank* bank = mutableCandidate();
    return bank != nullptr
        ? SetRotation(*bank, pattern, track, rotation) : Status::Stale;
}

Status PatternStateAuthority::WriteHandle::setPatternChain(
    std::uint8_t pattern, std::uint8_t repeat,
    std::uint8_t nextPattern) noexcept
{
    PatternBank* bank = mutableCandidate();
    return bank != nullptr
        ? SetPatternChain(*bank, pattern, repeat, nextPattern) : Status::Stale;
}

Status PatternStateAuthority::WriteHandle::setShuffle(
    std::uint8_t shuffle) noexcept
{
    PatternBank* bank = mutableCandidate();
    return bank != nullptr ? SetShuffle(*bank, shuffle) : Status::Stale;
}

Status PatternStateAuthority::WriteHandle::resetPattern(
    std::uint8_t pattern) noexcept
{
    PatternBank* bank = mutableCandidate();
    if (bank == nullptr)
        return Status::Stale;
    if (!IsValidPattern(pattern))
        return Status::InvalidPattern;
    ResetPattern(*bank, pattern);
    return Status::Ok;
}

Status PatternStateAuthority::WriteHandle::resetBank() noexcept
{
    PatternBank* bank = mutableCandidate();
    if (bank == nullptr)
        return Status::Stale;
    ResetBank(*bank);
    return Status::Ok;
}

Status PatternStateAuthority::WriteHandle::copyPattern(
    std::uint8_t source, std::uint8_t destination) noexcept
{
    PatternBank* bank = mutableCandidate();
    return bank != nullptr
        ? CopyPattern(*bank, source, destination) : Status::Stale;
}

Status PatternStateAuthority::WriteHandle::copyTrack(
    std::uint8_t pattern, std::uint8_t source,
    std::uint8_t destination) noexcept
{
    PatternBank* bank = mutableCandidate();
    return bank != nullptr
        ? CopyTrack(*bank, pattern, source, destination) : Status::Stale;
}

Status PatternStateAuthority::WriteHandle::clearTrack(
    std::uint8_t pattern, std::uint8_t track) noexcept
{
    PatternBank* bank = mutableCandidate();
    return bank != nullptr
        ? ClearTrack(*bank, pattern, track) : Status::Stale;
}

Status PatternStateAuthority::WriteHandle::generateEuclidean(
    std::uint8_t pattern, std::uint8_t track, std::uint8_t length,
    std::uint8_t pulses, std::uint8_t rotation) noexcept
{
    PatternBank* bank = mutableCandidate();
    return bank != nullptr
        ? GenerateEuclidean(*bank, pattern, track, length, pulses, rotation)
        : Status::Stale;
}

Status PatternStateAuthority::WriteHandle::replace(
    const PatternBank& candidateBank) noexcept
{
    PatternBank* bank = mutableCandidate();
    if (bank == nullptr)
        return Status::Stale;
    const Status validation = ValidateBank(candidateBank);
    if (validation != Status::Ok)
        return validation;
    *bank = candidateBank;
    return Status::Ok;
}

void PatternStateAuthority::WriteHandle::cancel() noexcept
{
    if (mOwner != nullptr)
        mOwner->cancel(*this);
}

PatternStateAuthority::PatternStateAuthority() noexcept
{
    (void) initialize(MakeDefaultPatternBank());
}

PatternStateAuthority::PatternStateAuthority(const PatternBank& initial) noexcept
{
    if (initialize(initial) != Status::Ok)
        (void) initialize(MakeDefaultPatternBank());
}

Status PatternStateAuthority::initialize(const PatternBank& initial) noexcept
{
    const Status validation = ValidateBank(initial);
    if (validation != Status::Ok)
        return validation;
    for (auto& slot : mSlots)
    {
        slot.bank = initial;
        slot.generation = 0u;
        slot.readers.store(0u, std::memory_order_relaxed);
        slot.state.store(SlotState::Available, std::memory_order_relaxed);
    }
    mActive.store(0u, std::memory_order_relaxed);
    mGeneration.store(0u, std::memory_order_relaxed);
    mWriter.clear(std::memory_order_relaxed);
    return Status::Ok;
}

Status PatternStateAuthority::tryAcquire(ReadHandle& handle,
                                         unsigned attempts) noexcept
{
    if (handle.active())
        return Status::Busy;
    for (unsigned attempt = 0; attempt < attempts; ++attempt)
    {
        const std::uint8_t index = mActive.load(std::memory_order_acquire);
        Slot& slot = mSlots[index];
        slot.readers.fetch_add(1u, std::memory_order_acq_rel);
        if (slot.state.load(std::memory_order_acquire) == SlotState::Available &&
            mActive.load(std::memory_order_acquire) == index)
        {
            handle.mOwner = this;
            handle.mBank = &slot.bank;
            handle.mSlot = index;
            handle.mGeneration = slot.generation;
            return Status::Ok;
        }
        slot.readers.fetch_sub(1u, std::memory_order_release);
    }
    return Status::Busy;
}

Status PatternStateAuthority::tryBeginEdit(WriteHandle& handle) noexcept
{
    if (handle.active() || mWriter.test_and_set(std::memory_order_acquire))
        return Status::Busy;

    const std::uint8_t active = mActive.load(std::memory_order_acquire);
    for (std::size_t offset = 1; offset < kSlotCount; ++offset)
    {
        const std::uint8_t index = static_cast<std::uint8_t>(
            (active + offset) % kSlotCount);
        Slot& slot = mSlots[index];
        if (slot.readers.load(std::memory_order_acquire) != 0u)
            continue;
        SlotState expected = SlotState::Available;
        if (!slot.state.compare_exchange_strong(
                expected, SlotState::Writing,
                std::memory_order_acq_rel, std::memory_order_acquire))
            continue;
        // A reader that raced the claim will see Writing and release its pin.
        // Giving up immediately keeps writer work bounded.
        if (slot.readers.load(std::memory_order_acquire) != 0u)
        {
            slot.state.store(SlotState::Available, std::memory_order_release);
            continue;
        }

        slot.bank = mSlots[active].bank;
        handle.mOwner = this;
        handle.mSlot = index;
        handle.mBaseGeneration = mSlots[active].generation;
        return Status::Ok;
    }

    mWriter.clear(std::memory_order_release);
    return Status::Busy;
}

Status PatternStateAuthority::commit(WriteHandle& handle,
                                     std::uint64_t* generation) noexcept
{
    PatternBank* bank = candidate(handle);
    if (bank == nullptr)
        return Status::Stale;
    const Status validation = ValidateBank(*bank);
    if (validation != Status::Ok)
        return validation;

    const std::uint64_t current = mGeneration.load(std::memory_order_relaxed);
    if (current == std::numeric_limits<std::uint64_t>::max())
        return Status::GenerationExhausted;
    const std::uint64_t next = current + 1u;
    Slot& slot = mSlots[handle.mSlot];
    slot.generation = next;
    slot.state.store(SlotState::Available, std::memory_order_release);
    mActive.store(handle.mSlot, std::memory_order_release);
    mGeneration.store(next, std::memory_order_release);

    handle.mOwner = nullptr;
    handle.mSlot = 0u;
    handle.mBaseGeneration = 0u;
    mWriter.clear(std::memory_order_release);
    if (generation != nullptr)
        *generation = next;
    return Status::Ok;
}

Status PatternStateAuthority::tryPublish(const PatternBank& candidateBank,
                                         std::uint64_t* generation) noexcept
{
    const Status validation = ValidateBank(candidateBank);
    if (validation != Status::Ok)
        return validation;
    WriteHandle handle;
    const Status begin = tryBeginEdit(handle);
    if (begin != Status::Ok)
        return begin;
    const Status replace = handle.replace(candidateBank);
    if (replace != Status::Ok)
        return replace;
    return commit(handle, generation);
}

void PatternStateAuthority::release(ReadHandle& handle) noexcept
{
    if (handle.mOwner != this)
        return;
    mSlots[handle.mSlot].readers.fetch_sub(1u, std::memory_order_release);
    handle.mOwner = nullptr;
    handle.mBank = nullptr;
    handle.mSlot = 0u;
    handle.mGeneration = 0u;
}

void PatternStateAuthority::cancel(WriteHandle& handle) noexcept
{
    if (handle.mOwner != this)
        return;
    mSlots[handle.mSlot].state.store(
        SlotState::Available, std::memory_order_release);
    handle.mOwner = nullptr;
    handle.mSlot = 0u;
    handle.mBaseGeneration = 0u;
    mWriter.clear(std::memory_order_release);
}

PatternBank* PatternStateAuthority::candidate(WriteHandle& handle) noexcept
{
    return handle.mOwner == this && handle.mSlot < kSlotCount
        ? &mSlots[handle.mSlot].bank : nullptr;
}

const PatternBank* PatternStateAuthority::candidate(
    const WriteHandle& handle) const noexcept
{
    return handle.mOwner == this && handle.mSlot < kSlotCount
        ? &mSlots[handle.mSlot].bank : nullptr;
}

} // namespace lxr::sequencer
