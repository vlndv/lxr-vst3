#pragma once

#include <array>
#include <cstddef>

namespace lxr
{

template <typename Event, std::size_t Capacity>
class FixedFifo
{
public:
    static_assert(Capacity > 0, "FixedFifo capacity must be nonzero");

    bool push(const Event& event) noexcept
    {
        if (mCount == Capacity)
            return false;
        mEvents[mWrite] = event;
        mWrite = (mWrite + 1u) % Capacity;
        ++mCount;
        return true;
    }

    Event* front() noexcept
    {
        return mCount == 0 ? nullptr : &mEvents[mRead];
    }

    void pop() noexcept
    {
        if (mCount == 0)
            return;
        mRead = (mRead + 1u) % Capacity;
        --mCount;
    }

    std::size_t size() const noexcept { return mCount; }
    bool empty() const noexcept { return mCount == 0; }
    static constexpr std::size_t capacity() noexcept { return Capacity; }

private:
    std::array<Event, Capacity> mEvents {};
    std::size_t mRead = 0;
    std::size_t mWrite = 0;
    std::size_t mCount = 0;
};

} // namespace lxr
