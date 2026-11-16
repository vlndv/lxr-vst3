#pragma once
#include <cstdint>

namespace lxr::sequencer
{
// Plugin adaptation of firmware front-panel press/release messages. This is
// transient performance state and is deliberately absent from frozen LXRS v3.
struct RollHostControls
{
    std::uint8_t rate = 8;
    std::uint8_t tracks = 0;
    std::uint64_t pressTicket = 0;
};
constexpr bool ValidRollRate(unsigned value) noexcept { return value < 14u; }
constexpr bool ValidRollTracks(unsigned value) noexcept { return value < 128u; }
} // namespace lxr::sequencer
