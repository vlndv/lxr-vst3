#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace lxr
{

// Lock-free, bounded publication for the complete 228-slot logical parameter
// state. Every payload element is atomic: a reader that overlaps a writer does
// not race with non-atomic storage and simply rejects that attempt. Writers are
// serialized by atomic_flag, including callers from different host threads.
template <std::size_t ParameterCount>
class ParameterStateAuthority
{
public:
    using Values = std::array<std::uint8_t, ParameterCount>;

    static constexpr unsigned kDefaultSnapshotAttempts = 16;

    struct WriteHandle
    {
        WriteHandle() noexcept = default;
        WriteHandle(const WriteHandle&) = delete;
        WriteHandle& operator=(const WriteHandle&) = delete;

        bool active() const noexcept { return owner != nullptr; }
        std::uint64_t generation() const noexcept { return nextGeneration; }

    private:
        friend class ParameterStateAuthority;
        ParameterStateAuthority* owner = nullptr;
        std::uint64_t nextGeneration = 0;
    };

    ParameterStateAuthority() noexcept
    {
        for (auto& value : mValues)
            value.store(0, std::memory_order_relaxed);
        for (auto& generation : mValueGenerations)
            generation.store(0, std::memory_order_relaxed);
    }

    // Construction-time initialization only. No other thread may access this
    // object until initialize() returns.
    void initialize(const Values& values) noexcept
    {
        for (std::size_t i = 0; i < ParameterCount; ++i)
        {
            mValues[i].store(values[i], std::memory_order_relaxed);
            mValueGenerations[i].store(0, std::memory_order_relaxed);
        }
        mSequence.store(0, std::memory_order_relaxed);
        mWriter.clear(std::memory_order_relaxed);
    }

    // One attempt only. Audio-thread callers never spin or block.
    bool tryBeginWrite(WriteHandle& handle) noexcept
    {
        if (handle.active() || mWriter.test_and_set(std::memory_order_acquire))
            return false;

        const std::uint64_t stable = mSequence.load(std::memory_order_relaxed);
        // Only this class writes mSequence, and the writer flag excludes all
        // other writers, so a held writer always starts from an even value.
        if ((stable & 1u) != 0u)
        {
            mWriter.clear(std::memory_order_release);
            return false;
        }

        handle.owner = this;
        handle.nextGeneration = stable + 2u;
        mSequence.store(stable + 1u, std::memory_order_release);
        return true;
    }

    bool write(WriteHandle& handle,
               std::size_t parameter,
               std::uint8_t value) noexcept
    {
        if (handle.owner != this || parameter >= ParameterCount || value > 127u)
            return false;

        mValues[parameter].store(value, std::memory_order_relaxed);
        mValueGenerations[parameter].store(
            handle.nextGeneration, std::memory_order_relaxed);
        return true;
    }

    bool writeAll(WriteHandle& handle, const Values& values) noexcept
    {
        if (handle.owner != this)
            return false;

        for (const std::uint8_t value : values)
        {
            if (value > 127u)
                return false;
        }
        for (std::size_t i = 0; i < ParameterCount; ++i)
        {
            mValues[i].store(values[i], std::memory_order_relaxed);
            mValueGenerations[i].store(
                handle.nextGeneration, std::memory_order_relaxed);
        }
        return true;
    }

    std::uint64_t commit(WriteHandle& handle) noexcept
    {
        if (handle.owner != this)
            return 0;

        const std::uint64_t generation = handle.nextGeneration;
        mSequence.store(generation, std::memory_order_release);
        handle.owner = nullptr;
        handle.nextGeneration = 0;
        mWriter.clear(std::memory_order_release);
        return generation;
    }

    // Safe only before any write() in this transaction. This is used for
    // validated publication setup failures, never to roll back payload writes.
    void cancelUnmodified(WriteHandle& handle) noexcept
    {
        if (handle.owner != this)
            return;
        const std::uint64_t stable = handle.nextGeneration - 2u;
        mSequence.store(stable, std::memory_order_release);
        handle.owner = nullptr;
        handle.nextGeneration = 0;
        mWriter.clear(std::memory_order_release);
    }

    bool tryPublish(const Values& values,
                    std::uint64_t* generation = nullptr) noexcept
    {
        for (const std::uint8_t value : values)
        {
            if (value > 127u)
                return false;
        }

        WriteHandle handle;
        if (!tryBeginWrite(handle))
            return false;
        if (!writeAll(handle, values))
        {
            cancelUnmodified(handle);
            return false;
        }
        const std::uint64_t committed = commit(handle);
        if (generation != nullptr)
            *generation = committed;
        return committed != 0;
    }

    bool trySnapshot(Values& values,
                     std::uint64_t* generation = nullptr,
                     unsigned attempts = kDefaultSnapshotAttempts) const noexcept
    {
        for (unsigned attempt = 0; attempt < attempts; ++attempt)
        {
            const std::uint64_t before =
                mSequence.load(std::memory_order_acquire);
            if ((before & 1u) != 0u)
                continue;

            for (std::size_t i = 0; i < ParameterCount; ++i)
                values[i] = mValues[i].load(std::memory_order_relaxed);

            const std::uint64_t after =
                mSequence.load(std::memory_order_acquire);
            if (before == after && (after & 1u) == 0u)
            {
                if (generation != nullptr)
                    *generation = after;
                return true;
            }
        }
        return false;
    }

    std::uint8_t value(std::size_t parameter) const noexcept
    {
        return parameter < ParameterCount
            ? mValues[parameter].load(std::memory_order_acquire) : 0;
    }

    std::uint64_t valueGeneration(std::size_t parameter) const noexcept
    {
        return parameter < ParameterCount
            ? mValueGenerations[parameter].load(std::memory_order_acquire) : 0;
    }

    std::uint64_t generation() const noexcept
    {
        return mSequence.load(std::memory_order_acquire);
    }

private:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "LXR state generations must be lock-free on the target");

    std::array<std::atomic<std::uint8_t>, ParameterCount> mValues {};
    std::array<std::atomic<std::uint64_t>, ParameterCount> mValueGenerations {};
    std::atomic<std::uint64_t> mSequence {0};
    std::atomic_flag mWriter = ATOMIC_FLAG_INIT;
};

} // namespace lxr
