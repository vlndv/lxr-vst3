#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace lxr::state
{

inline constexpr std::size_t kParameterCount = 228;
inline constexpr std::size_t kActiveParameterCount = 227;
inline constexpr std::size_t kHeaderSize = 24;
inline constexpr std::size_t kPayloadSize = kParameterCount;
inline constexpr std::size_t kEncodedSize = kHeaderSize + kPayloadSize;
inline constexpr std::size_t kLegacyEncodedSize = kParameterCount * sizeof(double);
inline constexpr std::size_t kVST3BypassSize = sizeof(std::int32_t);
inline constexpr std::uint16_t kFormatVersion = 1;
inline constexpr std::size_t kMaximumStateSize = 4096;

inline constexpr std::array<std::uint8_t, 4> kMagic {{'L', 'X', 'R', 'S'}};

enum class DecodeStatus
{
  Ok,
  Empty,
  TooLarge,
  InvalidMagic,
  UnsupportedVersion,
  InvalidHeader,
  InvalidLength,
  InvalidChecksum,
  InvalidParameter,
  UnexpectedTrailingBytes
};

struct DecodeResult
{
  DecodeStatus status = DecodeStatus::InvalidHeader;
  std::size_t consumed = 0;
  bool legacy = false;

  explicit operator bool() const noexcept { return status == DecodeStatus::Ok; }
};

inline void PutU16LE(std::uint8_t* destination, std::uint16_t value) noexcept
{
  destination[0] = static_cast<std::uint8_t>(value & 0xffu);
  destination[1] = static_cast<std::uint8_t>((value >> 8u) & 0xffu);
}

inline void PutU32LE(std::uint8_t* destination, std::uint32_t value) noexcept
{
  destination[0] = static_cast<std::uint8_t>(value & 0xffu);
  destination[1] = static_cast<std::uint8_t>((value >> 8u) & 0xffu);
  destination[2] = static_cast<std::uint8_t>((value >> 16u) & 0xffu);
  destination[3] = static_cast<std::uint8_t>((value >> 24u) & 0xffu);
}

inline std::uint16_t GetU16LE(const std::uint8_t* source) noexcept
{
  return static_cast<std::uint16_t>(source[0]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(source[1]) << 8u);
}

inline std::uint32_t GetU32LE(const std::uint8_t* source) noexcept
{
  return static_cast<std::uint32_t>(source[0]) |
         (static_cast<std::uint32_t>(source[1]) << 8u) |
         (static_cast<std::uint32_t>(source[2]) << 16u) |
         (static_cast<std::uint32_t>(source[3]) << 24u);
}

inline std::uint32_t Crc32(const std::uint8_t* data, std::size_t size) noexcept
{
  std::uint32_t crc = 0xffffffffu;
  for (std::size_t i = 0; i < size; ++i)
  {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit)
    {
      const std::uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1u) ^ (0xedb88320u & mask);
    }
  }
  return ~crc;
}

inline bool Encode(const std::array<std::uint8_t, kParameterCount>& parameters,
                   std::array<std::uint8_t, kEncodedSize>& encoded) noexcept
{
  for (std::size_t i = 0; i < parameters.size(); ++i)
  {
    if (parameters[i] > 127)
      return false;
  }

  encoded.fill(0);
  std::memcpy(encoded.data(), kMagic.data(), kMagic.size());
  PutU16LE(encoded.data() + 4, kFormatVersion);
  PutU16LE(encoded.data() + 6, static_cast<std::uint16_t>(kHeaderSize));
  PutU32LE(encoded.data() + 8, static_cast<std::uint32_t>(kPayloadSize));
  PutU16LE(encoded.data() + 12, static_cast<std::uint16_t>(kParameterCount));
  PutU16LE(encoded.data() + 14, 0); // Flags reserved for future formats.
  PutU32LE(encoded.data() + 16, 0); // No extension payload in version 1.
  std::memcpy(encoded.data() + kHeaderSize, parameters.data(), parameters.size());
  PutU32LE(encoded.data() + 20,
           Crc32(encoded.data() + kHeaderSize, kPayloadSize));
  return true;
}

inline bool HasAllowedSize(std::size_t available,
                           std::size_t stateSize,
                           bool allowVST3BypassSuffix) noexcept
{
  if (available == stateSize)
    return true;
  return allowVST3BypassSuffix && available == stateSize + kVST3BypassSize;
}

inline DecodeResult DecodeLegacy(const std::uint8_t* data,
                                 std::size_t size,
                                 std::array<std::uint8_t, kParameterCount>& parameters,
                                 bool allowVST3BypassSuffix) noexcept
{
  if (!HasAllowedSize(size, kLegacyEncodedSize, allowVST3BypassSuffix))
    return {DecodeStatus::InvalidMagic, 0, false};

  std::array<std::uint8_t, kParameterCount> candidate {};
  for (std::size_t i = 0; i < candidate.size(); ++i)
  {
    double value = 0.0;
    std::memcpy(&value, data + i * sizeof(double), sizeof(double));
    if (!std::isfinite(value) || value < 0.0 || value > 127.0 ||
        std::floor(value) != value)
    {
      return {DecodeStatus::InvalidParameter, 0, true};
    }
    candidate[i] = static_cast<std::uint8_t>(value);
  }

  parameters = candidate;
  return {DecodeStatus::Ok, kLegacyEncodedSize, true};
}

inline DecodeResult Decode(const std::uint8_t* data,
                           std::size_t size,
                           std::array<std::uint8_t, kParameterCount>& parameters,
                           bool allowVST3BypassSuffix = false) noexcept
{
  if (data == nullptr || size == 0)
    return {DecodeStatus::Empty, 0, false};
  if (size > kMaximumStateSize)
    return {DecodeStatus::TooLarge, 0, false};

  if (size < kMagic.size() ||
      std::memcmp(data, kMagic.data(), kMagic.size()) != 0)
  {
    return DecodeLegacy(data, size, parameters, allowVST3BypassSuffix);
  }

  if (size < kHeaderSize)
    return {DecodeStatus::InvalidLength, 0, false};

  const std::uint16_t version = GetU16LE(data + 4);
  if (version != kFormatVersion)
    return {DecodeStatus::UnsupportedVersion, 0, false};

  const std::uint16_t headerSize = GetU16LE(data + 6);
  const std::uint32_t payloadSize = GetU32LE(data + 8);
  const std::uint16_t parameterCount = GetU16LE(data + 12);
  const std::uint16_t flags = GetU16LE(data + 14);
  const std::uint32_t extensionSize = GetU32LE(data + 16);
  const std::uint32_t expectedCrc = GetU32LE(data + 20);

  if (headerSize != kHeaderSize || payloadSize != kPayloadSize ||
      parameterCount != kParameterCount || flags != 0 || extensionSize != 0)
  {
    return {DecodeStatus::InvalidHeader, 0, false};
  }

  const std::size_t stateSize = static_cast<std::size_t>(headerSize) +
                                static_cast<std::size_t>(payloadSize) +
                                static_cast<std::size_t>(extensionSize);
  if (stateSize != kEncodedSize || size < stateSize)
    return {DecodeStatus::InvalidLength, 0, false};
  if (!HasAllowedSize(size, stateSize, allowVST3BypassSuffix))
    return {DecodeStatus::UnexpectedTrailingBytes, 0, false};

  const std::uint8_t* payload = data + headerSize;
  if (Crc32(payload, payloadSize) != expectedCrc)
    return {DecodeStatus::InvalidChecksum, 0, false};

  std::array<std::uint8_t, kParameterCount> candidate {};
  std::memcpy(candidate.data(), payload, candidate.size());
  for (std::size_t i = 0; i < candidate.size(); ++i)
  {
    if (candidate[i] > 127)
      return {DecodeStatus::InvalidParameter, 0, false};
  }

  parameters = candidate;
  return {DecodeStatus::Ok, stateSize, false};
}

} // namespace lxr::state
