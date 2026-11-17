#pragma once
// Original LXR 0.37 kit identities Copyright 2013 Julian Schmidt.
// Derived material: LICENSE-LXR.txt (non-commercial, full source).
#include "RoutingStateAuthority.h"

namespace lxr::kit {
inline constexpr std::size_t kFieldCount = 228;
inline constexpr std::size_t kNameBytes = 8;
inline constexpr std::size_t kOwnerCount = 6;
inline constexpr std::size_t kOriginalSoundBytes = kNameBytes + kFieldCount;

// A logical source record, not an object-image disk format. No parsing here.
struct RawKit final {
    std::array<std::uint8_t, kNameBytes> name {};
    std::array<std::uint8_t, kFieldCount> values {};
};

enum class Owner : std::uint8_t { Drum1, Drum2, Drum3, Snare, Cymbal, HiHat, Global, Reserved };
enum class FieldKind : std::uint8_t { Scalar, OscillatorWave, Selector, VoiceHint, Reserved };
enum class NativeField : std::uint8_t { Scalar, Route, Opaque };

// Source selector is a full byte, not a SevenBitValue. Check before narrowing.
// This byte-only type also keeps named canonical records free of padding.
class StoredSelector final {
public:
    constexpr StoredSelector() noexcept = default;
    static constexpr StoredSelector fromRaw(std::uint64_t value) noexcept {
        return value <= 204 ? StoredSelector(static_cast<std::uint8_t>(value)) : StoredSelector();
    }
    constexpr std::uint16_t value() const noexcept { return mValue; }
    constexpr bool valid() const noexcept { return mValue <= 204; }
private:
    explicit constexpr StoredSelector(std::uint8_t value) noexcept : mValue(value) {}
    std::uint8_t mValue = 255;
};

// Named fields mirror the interleaved AVR storage identities. No MCU struct,
// bitfield, pointer target, padding or effective DSP value is serialized.
#include "KitRecords.inc"

struct CanonicalKit final {
    std::array<std::uint8_t, kNameBytes> name {};
    Drum1Kit drum1 {};
    Drum2Kit drum2 {};
    Drum3Kit drum3 {};
    SnareKit snare {};
    CymbalKit cymbal {};
    HiHatKit hiHat {}; // Closed/open decay60/61 and note226/227; one synth owner.
    GlobalKit global {}; // Only ID114; not device globals251..272.
    ReservedKit reserved {}; // Stored provenance, never modulation targets.
};

// Carries original identity even for valid rows outside the native subset.
// Hint0 is the source initialization sentinel;1..6 are menu voice identities.
struct OriginalSelector final {
    modulation_contract::SourceIdentity source {
        modulation_contract::SourceKind::Velocity, modulation_contract::Voice::Drum1};
    modulation_contract::OriginalTargetIndex index =
        modulation_contract::OriginalTargetIndex::fromRaw(0);
    std::uint8_t voiceHint = 0;
};

// Candidate only. No authority, transport, pattern bank, bypass, optional LXRS
// extensions or Engine reference. Original provenance remains in CanonicalKit.
struct PatchCandidate final {
    std::array<std::uint8_t, kNameBytes> name {};
    std::array<std::uint8_t, kFieldCount> patch {};
    std::array<PersistentRouteRecord, kPersistentRouteCount> routes {};
};
static_assert(std::is_trivially_copyable_v<RawKit> && std::is_trivially_copyable_v<CanonicalKit>);
static_assert(std::is_trivially_copyable_v<PatchCandidate>);
static_assert(sizeof(RawKit) == 236, "byte-only source storage");
static_assert(sizeof(CanonicalKit) == 236 && alignof(CanonicalKit) == 1,
              "all canonical fields initialized, no padding; still not a disk object image");
} // namespace lxr::kit
