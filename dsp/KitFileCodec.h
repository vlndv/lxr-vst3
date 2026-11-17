#pragma once
// Original Sonic Potions LXR 0.37 Copyright 2013 Julian Schmidt.
// Derived material: LICENSE-LXR.txt. No live state or file I/O in this interface.
#include "KitContract.h"

namespace lxr::kit::snd {
inline constexpr std::size_t kSize = 236;
inline constexpr std::size_t kPayloadOffset = 8;
using Bytes = std::array<std::uint8_t, kSize>;
enum class Error : std::uint8_t {
    Ok, InvalidArgument, Truncated, Oversized, BufferTooSmall,
    InvalidField, InvalidSelector, InvalidRoute, UnsupportedRoute, NonrepresentableRoute
};
struct Result final {
    Error error = Error::Ok;
    std::uint16_t field = 0xffffu;
    std::uint8_t route = 0xffu;
    kit::Issue detail = kit::Issue::None;
    constexpr explicit operator bool() const noexcept { return error == Error::Ok; }
};

// Archival framing only: arbitrary name8 and raw228 bytes. No magic, version,
// checksum, text restriction or invented reserved-byte rule. No normalization.
Result DecodeRaw(const std::uint8_t* input, std::size_t size, RawKit& output) noexcept;
Result EncodeRaw(const RawKit&, std::uint8_t* output, std::size_t capacity) noexcept;

// Strict source-semantic/route admission, separate from raw archival decoding.
// Cross-voice/global/unreviewed native routes reject. Source-representable
// reserved bytes and external-wave identities survive; no sample is loaded.
Result DecodeCanonical(const std::uint8_t*, std::size_t, CanonicalKit&) noexcept;
Result CheckRepresentability(const CanonicalKit&) noexcept;
Result EncodeCanonical(const CanonicalKit&, std::uint8_t*, std::size_t) noexcept;

// Explicit current-route export. Scalar/name/reserved/OFF-hint provenance comes
// from the kit. Twelve ordered typed assignments override source selectors;
// enabled LFO records also supply their hint. Never export a patch candidate's
// inert compatibility selector bytes as if they were original identities.
Result CheckRepresentability(const CanonicalKit&,
    const std::array<PersistentRouteRecord, kPersistentRouteCount>&) noexcept;
Result EncodeCanonical(const CanonicalKit&,
    const std::array<PersistentRouteRecord, kPersistentRouteCount>&,
    std::uint8_t*, std::size_t) noexcept;
static_assert(kSize == kOriginalSoundBytes && kPayloadOffset == kNameBytes);
} // namespace lxr::kit::snd
