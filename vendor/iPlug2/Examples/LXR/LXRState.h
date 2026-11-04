#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../../../dsp/RoutingStateAuthority.h"

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
  UnexpectedTrailingBytes,
  MissingRoutingModule,
  UnsupportedMandatoryTag,
  UnsupportedRoutingProfile,
  InvalidRoutingTable,
  InvalidRoutingRecord,
  DuplicateTag,
  InvalidTagOrder,
  ReservedField
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

// LXRS v2 is a bounded production container. The v1 constants and Encode()/
// Decode() API above remain immutable for old fixtures and migration tests.
inline constexpr std::uint16_t kV2FormatVersion = 2;
inline constexpr std::size_t kV2HeaderSize = 32;
inline constexpr std::size_t kV2TlvHeaderSize = 16;
inline constexpr std::size_t kV2RoutingHeaderSize = 48;
inline constexpr std::size_t kV2RouteSize = 16;
inline constexpr std::size_t kV2RoutingBodySize = 240;
inline constexpr std::size_t kV2EncodedSize = 516;
inline constexpr std::size_t kV2VST3EncodedSize = 520;
inline constexpr std::size_t kMaximumPluginStateSize =
    kMaximumStateSize - kVST3BypassSize;
inline constexpr std::uint32_t kRoutingTag = 1;
inline constexpr std::uint16_t kRequiredTlvFlag = 1;

struct PersistentDecodeResult
{
  DecodeStatus status = DecodeStatus::InvalidHeader;
  std::size_t consumed = 0;
  bool legacy = false;
  std::uint16_t version = 0;

  explicit operator bool() const noexcept { return status == DecodeStatus::Ok; }
};

inline std::uint32_t Crc32V2(const std::uint8_t* data,
                            std::size_t size) noexcept
{
  std::uint32_t crc = 0xffffffffu;
  for (std::size_t i = 0; i < size; ++i)
  {
    const std::uint8_t value = (i >= 24u && i < 28u) ? 0u : data[i];
    crc ^= value;
    for (int bit = 0; bit < 8; ++bit)
    {
      const std::uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1u) ^ (0xedb88320u & mask);
    }
  }
  return ~crc;
}

inline void WriteRoute(std::uint8_t* destination,
                       const PersistentRouteRecord& route) noexcept
{
  destination[0] = route.owner;
  destination[1] = route.source;
  PutU16LE(destination + 2, route.flags);
  PutU16LE(destination + 4, route.identityNamespace);
  PutU16LE(destination + 6, route.originalIndex);
  PutU16LE(destination + 8, route.firmwareParameter);
  destination[10] = route.valueType;
  destination[11] = route.scope;
  PutU16LE(destination + 12, route.physicalKey);
  destination[14] = route.menuVoiceHint;
  destination[15] = route.reserved;
}

inline PersistentRouteRecord ReadRoute(const std::uint8_t* source) noexcept
{
  PersistentRouteRecord route {};
  route.owner = source[0];
  route.source = source[1];
  route.flags = GetU16LE(source + 2);
  route.identityNamespace = GetU16LE(source + 4);
  route.originalIndex = GetU16LE(source + 6);
  route.firmwareParameter = GetU16LE(source + 8);
  route.valueType = source[10];
  route.scope = source[11];
  route.physicalKey = GetU16LE(source + 12);
  route.menuVoiceHint = source[14];
  route.reserved = source[15];
  return route;
}

inline DecodeStatus ValidateOpaqueOptionalTlvs(
    const PersistentRoutingState& routing) noexcept
{
  if (routing.optionalTlvBytes > routing.optionalTlvs.size())
    return DecodeStatus::InvalidLength;
  std::size_t position = 0;
  std::uint32_t previousTag = kRoutingTag;
  unsigned count = 1; // The mandatory routing TLV is always first.
  while (position < routing.optionalTlvBytes)
  {
    if (++count > 16u ||
        routing.optionalTlvBytes - position < kV2TlvHeaderSize)
      return DecodeStatus::InvalidLength;
    const auto* tlv = routing.optionalTlvs.data() + position;
    const std::uint32_t tag = GetU32LE(tlv);
    const std::uint16_t flags = GetU16LE(tlv + 6);
    const std::uint32_t bodyBytes = GetU32LE(tlv + 8);
    if (tag <= previousTag)
      return tag == previousTag ? DecodeStatus::DuplicateTag
                                : DecodeStatus::InvalidTagOrder;
    if (flags != 0u || GetU32LE(tlv + 12) != 0u)
      return DecodeStatus::ReservedField;
    if (bodyBytes > routing.optionalTlvBytes - position - kV2TlvHeaderSize)
      return DecodeStatus::InvalidLength;
    previousTag = tag;
    position += kV2TlvHeaderSize + static_cast<std::size_t>(bodyBytes);
  }
  return position == routing.optionalTlvBytes
      ? DecodeStatus::Ok : DecodeStatus::InvalidLength;
}

inline bool EncodeV2(
    const std::array<std::uint8_t, kParameterCount>& parameters,
    const PersistentRoutingState& routing,
    std::array<std::uint8_t, kMaximumStateSize>& encoded,
    std::size_t& encodedSize,
    bool includeVST3BypassSuffix = false) noexcept
{
  encodedSize = 0;
  for (const auto value : parameters)
  {
    if (value > 127u)
      return false;
  }
  if (ValidatePersistentRoutes(routing.routes) != PersistentRouteStatus::Ok ||
      ValidateOpaqueOptionalTlvs(routing) != DecodeStatus::Ok)
    return false;

  const std::size_t pluginBytes = kV2EncodedSize + routing.optionalTlvBytes;
  const std::size_t totalBytes = pluginBytes +
      (includeVST3BypassSuffix ? kVST3BypassSize : 0u);
  if (pluginBytes > kMaximumPluginStateSize || totalBytes > encoded.size())
    return false;

  encoded.fill(0);
  auto* data = encoded.data();
  std::memcpy(data, kMagic.data(), kMagic.size());
  PutU16LE(data + 4, kV2FormatVersion);
  PutU16LE(data + 6, static_cast<std::uint16_t>(kV2HeaderSize));
  PutU32LE(data + 8, static_cast<std::uint32_t>(pluginBytes));
  PutU16LE(data + 12, static_cast<std::uint16_t>(kParameterCount));
  PutU16LE(data + 14, 0);
  PutU32LE(data + 16, static_cast<std::uint32_t>(kPayloadSize));
  PutU32LE(data + 20, static_cast<std::uint32_t>(pluginBytes - 260u));
  PutU32LE(data + 24, 0);
  PutU32LE(data + 28, 0);
  std::memcpy(data + kV2HeaderSize, parameters.data(), parameters.size());

  auto* tlv = data + 260;
  PutU32LE(tlv, kRoutingTag);
  PutU16LE(tlv + 4, 1);
  PutU16LE(tlv + 6, kRequiredTlvFlag);
  PutU32LE(tlv + 8, static_cast<std::uint32_t>(kV2RoutingBodySize));
  PutU32LE(tlv + 12, 0);

  auto* body = tlv + kV2TlvHeaderSize;
  PutU16LE(body, 1);
  PutU16LE(body + 2, static_cast<std::uint16_t>(kV2RouteSize));
  PutU16LE(body + 4, static_cast<std::uint16_t>(kPersistentRouteCount));
  PutU16LE(body + 6, 1);
  PutU32LE(body + 8, 1);
  std::memcpy(body + 12, kOriginalRoutingTableSha256.data(),
              kOriginalRoutingTableSha256.size());
  PutU32LE(body + 44, 0);
  for (std::size_t i = 0; i < routing.routes.size(); ++i)
    WriteRoute(body + kV2RoutingHeaderSize + i * kV2RouteSize,
               routing.routes[i]);

  if (routing.optionalTlvBytes != 0)
  {
    std::memcpy(data + kV2EncodedSize, routing.optionalTlvs.data(),
                routing.optionalTlvBytes);
  }
  PutU32LE(data + 24, Crc32V2(data, pluginBytes));
  if (includeVST3BypassSuffix)
    PutU32LE(data + pluginBytes, routing.bypass ? 1u : 0u);
  encodedSize = totalBytes;
  return true;
}

inline PersistentDecodeResult DecodePersistentState(
    const std::uint8_t* data,
    std::size_t size,
    std::array<std::uint8_t, kParameterCount>& parameters,
    PersistentRoutingState& routing,
    bool requireVST3BypassSuffix = false) noexcept
{
  if (data == nullptr || size == 0)
    return {DecodeStatus::Empty, 0, false, 0};
  if (size > kMaximumStateSize)
    return {DecodeStatus::TooLarge, 0, false, 0};

  const bool hasMagic = size >= kMagic.size() &&
      std::memcmp(data, kMagic.data(), kMagic.size()) == 0;
  if (hasMagic && size < 8u)
    return {DecodeStatus::InvalidLength, 0, false, 0};
  const std::uint16_t version = hasMagic && size >= 6u
      ? GetU16LE(data + 4) : 0u;
  if (!hasMagic || version == kFormatVersion)
  {
    std::array<std::uint8_t, kParameterCount> patchCandidate {};
    const auto old = Decode(data, size, patchCandidate,
                            requireVST3BypassSuffix);
    if (!old)
      return {old.status, 0, old.legacy, version};
    PersistentRoutingState routingCandidate = MakeDisabledRoutingState();
    if (requireVST3BypassSuffix)
      routingCandidate.bypass = GetU32LE(data + old.consumed) != 0u;
    parameters = patchCandidate;
    routing = routingCandidate;
    return {DecodeStatus::Ok, old.consumed, old.legacy,
            static_cast<std::uint16_t>(old.legacy ? 0u : kFormatVersion)};
  }
  if (version != kV2FormatVersion)
    return {DecodeStatus::UnsupportedVersion, 0, false, version};
  if (size < kV2HeaderSize)
    return {DecodeStatus::InvalidLength, 0, false, version};

  const std::uint16_t headerBytes = GetU16LE(data + 6);
  const std::uint32_t pluginBytes32 = GetU32LE(data + 8);
  const std::uint16_t parameterCount = GetU16LE(data + 12);
  const std::uint16_t flags = GetU16LE(data + 14);
  const std::uint32_t parameterBytes = GetU32LE(data + 16);
  const std::uint32_t extensionBytes = GetU32LE(data + 20);
  if (headerBytes != kV2HeaderSize || parameterCount != kParameterCount ||
      flags != 0u || parameterBytes != kPayloadSize || GetU32LE(data + 28) != 0u)
    return {DecodeStatus::InvalidHeader, 0, false, version};
  if (pluginBytes32 > kMaximumPluginStateSize)
    return {DecodeStatus::TooLarge, 0, false, version};
  if (pluginBytes32 < 260u || extensionBytes != pluginBytes32 - 260u)
    return {DecodeStatus::InvalidLength, 0, false, version};
  const std::size_t pluginBytes = pluginBytes32;
  const std::size_t expectedSize = pluginBytes +
      (requireVST3BypassSuffix ? kVST3BypassSize : 0u);
  if (size < expectedSize)
    return {DecodeStatus::InvalidLength, 0, false, version};
  if (size != expectedSize)
    return {DecodeStatus::UnexpectedTrailingBytes, 0, false, version};
  if (Crc32V2(data, pluginBytes) != GetU32LE(data + 24))
    return {DecodeStatus::InvalidChecksum, 0, false, version};

  std::array<std::uint8_t, kParameterCount> patchCandidate {};
  std::memcpy(patchCandidate.data(), data + kV2HeaderSize,
              patchCandidate.size());
  for (const auto value : patchCandidate)
  {
    if (value > 127u)
      return {DecodeStatus::InvalidParameter, 0, false, version};
  }

  PersistentRoutingState routingCandidate = MakeDisabledRoutingState();
  bool foundRouting = false;
  std::uint32_t previousTag = 0;
  unsigned tlvCount = 0;
  std::size_t optionalBytes = 0;
  std::size_t position = 260;
  while (position < pluginBytes)
  {
    if (++tlvCount > 16u || pluginBytes - position < kV2TlvHeaderSize)
      return {DecodeStatus::InvalidLength, 0, false, version};
    const auto* tlv = data + position;
    const std::uint32_t tag = GetU32LE(tlv);
    const std::uint16_t recordVersion = GetU16LE(tlv + 4);
    const std::uint16_t tlvFlags = GetU16LE(tlv + 6);
    const std::uint32_t bodyBytes32 = GetU32LE(tlv + 8);
    if (tag == 0u || (tlvFlags & ~kRequiredTlvFlag) != 0u ||
        GetU32LE(tlv + 12) != 0u)
      return {DecodeStatus::ReservedField, 0, false, version};
    if (tag <= previousTag)
      return {tag == previousTag ? DecodeStatus::DuplicateTag
                                 : DecodeStatus::InvalidTagOrder,
              0, false, version};
    if (bodyBytes32 > pluginBytes - position - kV2TlvHeaderSize)
      return {DecodeStatus::InvalidLength, 0, false, version};
    const std::size_t tlvBytes = kV2TlvHeaderSize + bodyBytes32;
    const auto* body = tlv + kV2TlvHeaderSize;
    previousTag = tag;

    if (tag == kRoutingTag)
    {
      if (foundRouting)
        return {DecodeStatus::DuplicateTag, 0, false, version};
      if (recordVersion != 1u)
        return {DecodeStatus::UnsupportedVersion, 0, false, version};
      if (tlvFlags != kRequiredTlvFlag || bodyBytes32 != kV2RoutingBodySize)
        return {DecodeStatus::InvalidLength, 0, false, version};
      if (GetU16LE(body) != 1u)
        return {DecodeStatus::UnsupportedVersion, 0, false, version};
      if (GetU16LE(body + 2) != kV2RouteSize ||
          GetU16LE(body + 4) != kPersistentRouteCount)
        return {DecodeStatus::InvalidLength, 0, false, version};
      if (GetU16LE(body + 6) != 1u)
        return {DecodeStatus::UnsupportedRoutingProfile, 0, false, version};
      if (GetU32LE(body + 8) != 1u ||
          std::memcmp(body + 12, kOriginalRoutingTableSha256.data(),
                      kOriginalRoutingTableSha256.size()) != 0)
        return {DecodeStatus::InvalidRoutingTable, 0, false, version};
      if (GetU32LE(body + 44) != 0u)
        return {DecodeStatus::ReservedField, 0, false, version};
      for (std::size_t i = 0; i < routingCandidate.routes.size(); ++i)
      {
        const auto record = ReadRoute(
            body + kV2RoutingHeaderSize + i * kV2RouteSize);
        const auto routeStatus = ValidatePersistentRoute(record);
        if ((routeStatus != PersistentRouteStatus::Ok &&
             routeStatus != PersistentRouteStatus::Disabled) ||
            static_cast<std::size_t>(record.source) * 6u + record.owner != i)
        {
          return {DecodeStatus::InvalidRoutingRecord, 0, false, version};
        }
        routingCandidate.routes[i] = record;
      }
      foundRouting = true;
    }
    else
    {
      if ((tlvFlags & kRequiredTlvFlag) != 0u)
        return {DecodeStatus::UnsupportedMandatoryTag, 0, false, version};
      if (tlvBytes > routingCandidate.optionalTlvs.size() - optionalBytes)
        return {DecodeStatus::TooLarge, 0, false, version};
      std::memcpy(routingCandidate.optionalTlvs.data() + optionalBytes,
                  tlv, tlvBytes);
      optionalBytes += tlvBytes;
    }
    position += tlvBytes;
  }
  if (!foundRouting)
    return {DecodeStatus::MissingRoutingModule, 0, false, version};
  routingCandidate.optionalTlvBytes = static_cast<std::uint16_t>(optionalBytes);
  if (requireVST3BypassSuffix)
    routingCandidate.bypass = GetU32LE(data + pluginBytes) != 0u;

  parameters = patchCandidate;
  routing = routingCandidate;
  return {DecodeStatus::Ok, pluginBytes, false, version};
}

} // namespace lxr::state
