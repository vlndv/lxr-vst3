#pragma once

// Sonic Potions LXR 0.37 derived musical data; see LICENSE-LXR.txt.
// Non-real-time codec. This is an explicit wire format, never an object dump.
#include "PatternBank.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace lxr::sequencer::state
{
inline constexpr std::size_t kHeaderBytes = 32;
inline constexpr std::size_t kPayloadBytes = 50417;
inline constexpr std::size_t kModuleBytes = kHeaderBytes + kPayloadBytes;
using EncodedBank = std::array<std::uint8_t, kModuleBytes>;

inline void Put16(std::uint8_t* p, std::uint16_t v) noexcept
{ p[0] = static_cast<std::uint8_t>(v); p[1] = static_cast<std::uint8_t>(v >> 8u); }
inline void Put32(std::uint8_t* p, std::uint32_t v) noexcept
{ Put16(p, static_cast<std::uint16_t>(v)); Put16(p + 2, static_cast<std::uint16_t>(v >> 16u)); }
inline std::uint16_t Get16(const std::uint8_t* p) noexcept
{ return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8u)); }
inline std::uint32_t Get32(const std::uint8_t* p) noexcept
{ return Get16(p) | (static_cast<std::uint32_t>(Get16(p + 2)) << 16u); }
inline std::uint32_t Crc(const std::uint8_t* p, std::size_t n,
                         std::size_t zeroOffset) noexcept
{
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < n; ++i)
    {
        crc ^= i >= zeroOffset && i - zeroOffset < 4u ? 0u : p[i];
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

inline bool Encode(const PatternBank& bank, EncodedBank& bytes) noexcept
{
    if (ValidateBank(bank) != Status::Ok) return false;
    bytes.fill(0);
    auto* p = bytes.data();
    std::memcpy(p, "LXQB", 4);
    Put16(p + 4, 1); Put16(p + 6, kHeaderBytes);
    Put32(p + 8, kModuleBytes); Put32(p + 12, kPayloadBytes);
    Put16(p + 16, 8); Put16(p + 18, 7);
    Put16(p + 24, 128); Put16(p + 26, 7);
    std::size_t at = kHeaderBytes;
    for (const auto& pattern : bank.patterns)
        for (const auto& track : pattern.steps)
            for (const auto& s : track)
            {
                p[at++] = s.volume; p[at++] = s.probability; p[at++] = s.note;
                p[at++] = s.parameter1; p[at++] = s.value1;
                p[at++] = s.parameter2; p[at++] = s.value2;
            }
    for (const auto& pattern : bank.patterns)
        for (auto mask : pattern.mainGates) { Put16(p + at, mask); at += 2; }
    for (const auto& pattern : bank.patterns)
        for (auto length : pattern.lengthCodes) p[at++] = length;
    for (const auto& pattern : bank.patterns)
        for (auto rotation : pattern.rotations) p[at++] = rotation;
    for (const auto& pattern : bank.patterns)
    { p[at++] = pattern.nextPattern; p[at++] = pattern.repeat; }
    p[at++] = bank.shuffle;
    if (at != kModuleBytes) return false;
    Put32(p + 20, Crc(p, bytes.size(), 20));
    return true;
}

inline bool Decode(const std::uint8_t* p, std::size_t n, PatternBank& bank) noexcept
{
    if (p == nullptr || n != kModuleBytes || std::memcmp(p, "LXQB", 4) != 0 ||
        Get16(p + 4) != 1 || Get16(p + 6) != kHeaderBytes ||
        Get32(p + 8) != kModuleBytes || Get32(p + 12) != kPayloadBytes ||
        Get16(p + 16) != 8 || Get16(p + 18) != 7 ||
        Get16(p + 24) != 128 || Get16(p + 26) != 7 || Get32(p + 28) != 0 ||
        Get32(p + 20) != Crc(p, n, 20)) return false;
    PatternBank candidate {};
    std::size_t at = kHeaderBytes;
    for (auto& pattern : candidate.patterns)
        for (auto& track : pattern.steps)
            for (auto& s : track)
            {
                s.volume = p[at++]; s.probability = p[at++]; s.note = p[at++];
                s.parameter1 = p[at++]; s.value1 = p[at++];
                s.parameter2 = p[at++]; s.value2 = p[at++];
            }
    for (auto& pattern : candidate.patterns)
        for (auto& mask : pattern.mainGates) { mask = Get16(p + at); at += 2; }
    for (auto& pattern : candidate.patterns)
        for (auto& length : pattern.lengthCodes) length = p[at++];
    for (auto& pattern : candidate.patterns)
        for (auto& rotation : pattern.rotations) rotation = p[at++];
    for (auto& pattern : candidate.patterns)
    { pattern.nextPattern = p[at++]; pattern.repeat = p[at++]; }
    candidate.shuffle = p[at++];
    if (at != n || ValidateBank(candidate) != Status::Ok) return false;
    bank = candidate;
    return true;
}

struct TransportConfig
{
    std::uint8_t enabled = 0;
    std::uint8_t clock = 0;
    std::uint16_t bpm = 120;
    std::uint8_t pattern = 0;
    std::uint32_t seed = 0x9e3779b9u;
};
inline bool Validate(const TransportConfig& c) noexcept
{ return c.enabled <= 1 && c.clock <= 1 && c.bpm >= 1 && c.bpm <= 255 && c.pattern < 8; }
inline bool EncodeTransport(const TransportConfig& c, std::uint8_t* p) noexcept
{
    if (!Validate(c)) return false;
    std::memset(p, 0, 16);
    p[0] = c.enabled; p[1] = c.clock; Put16(p + 2, c.bpm);
    p[4] = c.pattern; Put32(p + 8, c.seed);
    return true;
}
inline bool DecodeTransport(const std::uint8_t* p, TransportConfig& c) noexcept
{
    TransportConfig candidate;
    candidate.enabled = p[0]; candidate.clock = p[1]; candidate.bpm = Get16(p + 2);
    candidate.pattern = p[4]; candidate.seed = Get32(p + 8);
    if (p[5] != 0 || Get16(p + 6) != 0 || Get32(p + 12) != 0 || !Validate(candidate))
        return false;
    c = candidate;
    return true;
}
} // namespace lxr::sequencer::state
