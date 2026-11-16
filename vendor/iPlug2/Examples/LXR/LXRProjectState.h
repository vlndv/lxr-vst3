#pragma once
#include "LXRState.h"
#include "../../../../dsp/SequencerState.h"

namespace lxr::state
{
inline constexpr std::size_t kV3MaximumStreamBytes = 65536;
inline constexpr std::size_t kV3CanonicalCoreBytes = 51061;
inline constexpr std::size_t kV3CanonicalStreamBytes = 51065;
struct ProjectCandidate
{
    std::array<std::uint8_t, kParameterCount> patch {};
    PersistentRoutingState routing = MakeDisabledRoutingState();
    sequencer::PatternBank bank = sequencer::MakeDefaultPatternBank();
    sequencer::state::TransportConfig transport {};
};
using ProjectBytes = std::array<std::uint8_t, kV3MaximumStreamBytes>;

inline bool EncodeProject(const ProjectCandidate& c, ProjectBytes& bytes,
                          std::size_t& size, bool suffix) noexcept
{
    size = 0;
    std::array<std::uint8_t, kMaximumStateSize> old {};
    std::size_t oldSize = 0;
    sequencer::state::EncodedBank bank {};
    if (!EncodeV2(c.patch, c.routing, old, oldSize, false) ||
        !sequencer::state::Encode(c.bank, bank) ||
        !sequencer::state::Validate(c.transport)) return false;
    const std::size_t core = kV3CanonicalCoreBytes + (oldSize - kV2EncodedSize);
    if (core > bytes.size() - 4u) return false;
    bytes.fill(0);
    auto* p = bytes.data();
    std::memcpy(p, "LXRS", 4); PutU16LE(p + 4, 3); PutU16LE(p + 6, 32);
    PutU32LE(p + 8, static_cast<std::uint32_t>(core));
    PutU32LE(p + 12, static_cast<std::uint32_t>(core - 32u));
    PutU16LE(p + 16, 3);
    std::size_t at = 32;
    const auto module = [&](std::uint32_t tag, const std::uint8_t* data, std::size_t n) {
        PutU32LE(p + at, tag); PutU16LE(p + at + 4, 1); PutU16LE(p + at + 6, 1);
        PutU32LE(p + at + 8, static_cast<std::uint32_t>(n));
        std::memcpy(p + at + 16, data, n); at += 16 + n;
    };
    module(1, old.data(), oldSize); module(2, bank.data(), bank.size());
    std::array<std::uint8_t, 16> transport {};
    if (!sequencer::state::EncodeTransport(c.transport, transport.data())) return false;
    module(3, transport.data(), transport.size());
    if (at != core) return false;
    PutU32LE(p + 24, Crc32V2(p, core));
    if (suffix) PutU32LE(p + core, c.routing.bypass ? 1u : 0u);
    size = core + (suffix ? 4u : 0u);
    return true;
}

inline bool DecodeProject(const std::uint8_t* p, std::size_t n,
                          ProjectCandidate& result, bool suffix) noexcept
{
    if (p == nullptr || n < 4u || n > kV3MaximumStreamBytes) return false;
    ProjectCandidate c;
    if (n < 6u || std::memcmp(p, "LXRS", 4) != 0 || GetU16LE(p + 4) != 3u)
    {
        if (!DecodePersistentState(p, n, c.patch, c.routing, suffix)) return false;
        result = c;
        return true;
    }
    if (n < 32u + (suffix ? 4u : 0u)) return false;
    const std::size_t core = GetU32LE(p + 8);
    if (core < 32u || core > kV3MaximumStreamBytes - 4u ||
        core != n - (suffix ? 4u : 0u) || GetU16LE(p + 6) != 32u ||
        GetU32LE(p + 12) != core - 32u || GetU16LE(p + 18) != 0u ||
        GetU32LE(p + 20) != 0u || GetU32LE(p + 28) != 0u ||
        GetU32LE(p + 24) != Crc32V2(p, core)) return false;
    const auto count = GetU16LE(p + 16);
    if (count != 3u) return false; // Initial profile rejects unknown optional modules too.
    std::size_t at = 32;
    for (std::uint32_t tag = 1; tag <= 3; ++tag)
    {
        if (core - at < 16u) return false;
        const auto* h = p + at;
        const std::size_t body = GetU32LE(h + 8);
        if (GetU32LE(h) != tag || GetU16LE(h + 4) != 1 ||
            GetU16LE(h + 6) != 1 || GetU32LE(h + 12) != 0 ||
            body > core - at - 16u) return false;
        const auto* data = h + 16;
        if (tag == 1)
        {
            if (body < kV2EncodedSize || std::memcmp(data, "LXRS", 4) != 0 ||
                GetU16LE(data + 4) != 2 ||
                !DecodePersistentState(data, body, c.patch, c.routing, false)) return false;
        }
        else if (tag == 2)
        {
            if (!sequencer::state::Decode(data, body, c.bank)) return false;
        }
        else if (body != 16 || !sequencer::state::DecodeTransport(data, c.transport))
            return false;
        at += 16u + body;
    }
    if (at != core) return false;
    if (suffix) c.routing.bypass = GetU32LE(p + core) != 0;
    result = c;
    return true;
}
} // namespace lxr::state
