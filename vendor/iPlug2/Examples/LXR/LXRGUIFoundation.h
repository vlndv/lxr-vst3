#pragma once

// P19-B production GUI foundation. This file contains only fixed-size state,
// control metadata and bounded readback publication. It is deliberately
// independent of IGraphics so the UI contract can be tested without a host.

#include "LXRParamContract.h"

#include "../../../../dsp/RouteHostControls.h"
#include "../../../../dsp/PatternBank.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace lxr::gui
{

inline constexpr int kCanvasWidth = 1280;
inline constexpr int kCanvasHeight = 800;
inline constexpr int kRefreshHz = 30;

enum class PrimaryView : std::uint8_t
{
    Overview = 0,
    Sound,
    Modulation,
    Sequencer,
    KitsMorph,
    Count
};

enum class VoiceOwner : std::uint8_t
{
    Drum1 = 0,
    Drum2,
    Drum3,
    Snare,
    Cymbal,
    HiHat,
    Count
};

#include "LXRSoundEditorContract.h"
#include "LXRModulationEditorContract.h"
#include "LXRSequencerEditorContract.h"

inline constexpr std::array<const char*, 5> kViewNames {{
    "Overview", "Sound", "Modulation", "Sequencer", "Kits + Morph"
}};

inline constexpr std::array<const char*, 5> kViewControlIds {{
    "lxr.nav.overview", "lxr.nav.sound", "lxr.nav.modulation",
    "lxr.nav.sequencer", "lxr.nav.kits-morph"
}};

inline constexpr std::array<const char*, 5> kViewGroups {{
    "lxr.view.overview", "lxr.view.sound", "lxr.view.modulation",
    "lxr.view.sequencer", "lxr.view.kits-morph"
}};

inline constexpr std::array<const char*, 6> kVoiceNames {{
    "Drum 1", "Drum 2", "Drum 3", "Snare", "Cymbal", "Hi-hat"
}};

inline constexpr std::array<const char*, 6> kVoiceShortNames {{
    "D1", "D2", "D3", "SN", "CY", "HH"
}};

inline constexpr std::array<const char*, 6> kVoiceControlIds {{
    "lxr.voice.d1", "lxr.voice.d2", "lxr.voice.d3",
    "lxr.voice.sn", "lxr.voice.cy", "lxr.voice.hh"
}};

inline constexpr std::array<const char*, 6> kVoiceGroups {{
    "lxr.owner.d1", "lxr.owner.d2", "lxr.owner.d3",
    "lxr.owner.sn", "lxr.owner.cy", "lxr.owner.hh"
}};

constexpr bool valid(PrimaryView view) noexcept
{
    return static_cast<std::size_t>(view) < kViewNames.size();
}

constexpr bool valid(VoiceOwner voice) noexcept
{
    return static_cast<std::size_t>(voice) < kVoiceNames.size();
}

// Closed/open hat are sequencer articulations, not independent synthesis
// owners. P19-B intentionally exposes exactly one Hi-hat owner.
constexpr VoiceOwner synthesisOwnerForTrack(std::uint8_t track) noexcept
{
    return track < 5u ? static_cast<VoiceOwner>(track)
                      : track < 7u ? VoiceOwner::HiHat : VoiceOwner::Count;
}

class SelectionState final
{
public:
    constexpr PrimaryView view() const noexcept { return mView; }
    constexpr VoiceOwner voice() const noexcept { return mVoice; }
    constexpr std::uint64_t generation() const noexcept { return mGeneration; }

    constexpr bool select(PrimaryView view) noexcept
    {
        if (!valid(view))
            return false;
        if (mView != view)
        {
            mView = view;
            ++mGeneration;
        }
        return true;
    }

    constexpr bool select(VoiceOwner voice) noexcept
    {
        if (!valid(voice))
            return false;
        if (mVoice != voice)
        {
            mVoice = voice;
            ++mGeneration;
        }
        return true;
    }

private:
    PrimaryView mView = PrimaryView::Overview;
    VoiceOwner mVoice = VoiceOwner::Drum1;
    std::uint64_t mGeneration = 0;
};

enum class ControlKind : std::uint8_t
{
    Integer,
    Boolean,
    Enumeration,
    SourceToggle,
    TypedRoute
};

struct ParameterBinding final
{
    int parameter = -1;
    const char* controlId = nullptr;
    const char* label = nullptr;
    ControlKind kind = ControlKind::Integer;
    int minimum = 0;
    int maximum = 0;
    int defaultValue = 0;
    VoiceOwner owner = VoiceOwner::Count;
};

// The representative P19-B surface. Each host parameter has one editable GUI
// binding; persistent header values are read-only mirrors, not second bindings.
inline constexpr std::array<ParameterBinding, 36> kCoreBindings {{
    {88, "lxr.param.088", "D1 Level", ControlKind::Integer, 0, 127, 100, VoiceOwner::Drum1},
    {89, "lxr.param.089", "D2 Level", ControlKind::Integer, 0, 127, 100, VoiceOwner::Drum2},
    {90, "lxr.param.090", "D3 Level", ControlKind::Integer, 0, 127, 100, VoiceOwner::Drum3},
    {91, "lxr.param.091", "Snare Level", ControlKind::Integer, 0, 127, 100, VoiceOwner::Snare},
    {92, "lxr.param.092", "Cymbal Level", ControlKind::Integer, 0, 127, 100, VoiceOwner::Cymbal},
    {93, "lxr.param.093", "Hi-hat Level", ControlKind::Integer, 0, 127, 100, VoiceOwner::HiHat},
    {8, "lxr.param.008", "Pitch", ControlKind::Integer, 0, 127, 63, VoiceOwner::Drum1},
    {10, "lxr.param.010", "Pitch", ControlKind::Integer, 0, 127, 63, VoiceOwner::Drum2},
    {12, "lxr.param.012", "Pitch", ControlKind::Integer, 0, 127, 63, VoiceOwner::Drum3},
    {14, "lxr.param.014", "Pitch", ControlKind::Integer, 0, 127, 63, VoiceOwner::Snare},
    {16, "lxr.param.016", "Pitch", ControlKind::Integer, 0, 127, 63, VoiceOwner::Cymbal},
    {18, "lxr.param.018", "Pitch", ControlKind::Integer, 0, 127, 63, VoiceOwner::HiHat},
    {37, "lxr.param.037", "Filter Cutoff", ControlKind::Integer, 0, 127, 64, VoiceOwner::Drum1},
    {38, "lxr.param.038", "Filter Cutoff", ControlKind::Integer, 0, 127, 64, VoiceOwner::Drum2},
    {39, "lxr.param.039", "Filter Cutoff", ControlKind::Integer, 0, 127, 64, VoiceOwner::Drum3},
    {40, "lxr.param.040", "Filter Cutoff", ControlKind::Integer, 0, 127, 64, VoiceOwner::Snare},
    {41, "lxr.param.041", "Filter Cutoff", ControlKind::Integer, 0, 127, 64, VoiceOwner::Cymbal},
    {42, "lxr.param.042", "Filter Cutoff", ControlKind::Integer, 0, 127, 64, VoiceOwner::HiHat},
    // Waveform is a 7-bit source field. P19-B offers the six built-in choices
    // but preserves and displays unsupported user-SD values instead of
    // narrowing the host contract to a menu index.
    {1, "lxr.param.001", "Waveform", ControlKind::Enumeration, 0, 127, 1, VoiceOwner::Drum1},
    {2, "lxr.param.002", "Waveform", ControlKind::Enumeration, 0, 127, 1, VoiceOwner::Drum2},
    {3, "lxr.param.003", "Waveform", ControlKind::Enumeration, 0, 127, 1, VoiceOwner::Drum3},
    {4, "lxr.param.004", "Waveform", ControlKind::Enumeration, 0, 127, 1, VoiceOwner::Snare},
    {6, "lxr.param.006", "Waveform", ControlKind::Enumeration, 0, 127, 1, VoiceOwner::Cymbal},
    {7, "lxr.param.007", "Waveform", ControlKind::Enumeration, 0, 127, 1, VoiceOwner::HiHat},
    // ORIGINAL QUIRK: on/off fields retain their native 7-bit 0..127 storage.
    {137, "lxr.param.137", "Volume Modulation", ControlKind::SourceToggle, 0, 127, 1, VoiceOwner::Drum1},
    {138, "lxr.param.138", "Volume Modulation", ControlKind::SourceToggle, 0, 127, 1, VoiceOwner::Drum2},
    {139, "lxr.param.139", "Volume Modulation", ControlKind::SourceToggle, 0, 127, 1, VoiceOwner::Drum3},
    {140, "lxr.param.140", "Volume Modulation", ControlKind::SourceToggle, 0, 127, 1, VoiceOwner::Snare},
    {141, "lxr.param.141", "Volume Modulation", ControlKind::SourceToggle, 0, 127, 1, VoiceOwner::Cymbal},
    {142, "lxr.param.142", "Volume Modulation", ControlKind::SourceToggle, 0, 127, 1, VoiceOwner::HiHat},
    {228, "lxr.param.228", "Velocity Destination", ControlKind::TypedRoute, 0, 65535, 0, VoiceOwner::Drum1},
    {229, "lxr.param.229", "Velocity Destination", ControlKind::TypedRoute, 0, 65535, 0, VoiceOwner::Drum2},
    {230, "lxr.param.230", "Velocity Destination", ControlKind::TypedRoute, 0, 65535, 0, VoiceOwner::Drum3},
    {231, "lxr.param.231", "Velocity Destination", ControlKind::TypedRoute, 0, 65535, 0, VoiceOwner::Snare},
    {232, "lxr.param.232", "Velocity Destination", ControlKind::TypedRoute, 0, 65535, 0, VoiceOwner::Cymbal},
    {233, "lxr.param.233", "Velocity Destination", ControlKind::TypedRoute, 0, 65535, 0, VoiceOwner::HiHat}
}};

inline constexpr std::array<ParameterBinding, 5> kGlobalBindings {{
    {240, "lxr.param.240", "Sequencer Enable", ControlKind::Boolean, 0, 1, 0, VoiceOwner::Count},
    {241, "lxr.param.241", "Clock", ControlKind::Enumeration, 0, 1, 0, VoiceOwner::Count},
    {242, "lxr.param.242", "Internal BPM", ControlKind::Integer, 1, 255, 120, VoiceOwner::Count},
    {245, "lxr.param.245", "Morph Amount", ControlKind::Integer, 0, 255, 0, VoiceOwner::Count},
    {114, "lxr.param.114", "All Voice Decimation", ControlKind::Integer, 0, 127, 127, VoiceOwner::Count}
}};

constexpr bool sameString(const char* a, const char* b) noexcept
{
    if (a == nullptr || b == nullptr)
        return a == b;
    for (;; ++a, ++b)
    {
        if (*a != *b)
            return false;
        if (*a == '\0')
            return true;
    }
}

constexpr bool validateBindings() noexcept
{
    for (std::size_t i = 0; i < kCoreBindings.size(); ++i)
    {
        const auto& row = kCoreBindings[i];
        if (row.parameter < 0 || row.parameter >= vst::kNumParams ||
            row.controlId == nullptr || row.controlId[0] == '\0' ||
            row.label == nullptr || row.label[0] == '\0' ||
            row.minimum > row.defaultValue || row.defaultValue > row.maximum ||
            !valid(row.owner))
            return false;
        for (std::size_t j = 0; j < i; ++j)
            if (row.parameter == kCoreBindings[j].parameter ||
                sameString(row.controlId, kCoreBindings[j].controlId))
                return false;
    }
    for (std::size_t i = 0; i < kGlobalBindings.size(); ++i)
    {
        const auto& row = kGlobalBindings[i];
        if (row.parameter < 0 || row.parameter >= vst::kNumParams ||
            row.minimum > row.defaultValue || row.defaultValue > row.maximum ||
            row.owner != VoiceOwner::Count)
            return false;
        for (const auto& voice : kCoreBindings)
            if (row.parameter == voice.parameter ||
                sameString(row.controlId, voice.controlId))
                return false;
        for (std::size_t j = 0; j < i; ++j)
            if (row.parameter == kGlobalBindings[j].parameter ||
                sameString(row.controlId, kGlobalBindings[j].controlId))
                return false;
    }
    return true;
}

static_assert(validateBindings(), "P19-B GUI bindings must be unique and valid");

constexpr const ParameterBinding* bindingForParameter(int parameter) noexcept
{
    for (const auto& binding : kCoreBindings)
        if (binding.parameter == parameter)
            return &binding;
    for (const auto& binding : kGlobalBindings)
        if (binding.parameter == parameter)
            return &binding;
    return nullptr;
}

inline bool normalize(const ParameterBinding& binding, int value,
                      double& normalized) noexcept
{
    if (value < binding.minimum || value > binding.maximum ||
        binding.maximum <= binding.minimum)
        return false;
    normalized = static_cast<double>(value - binding.minimum) /
        static_cast<double>(binding.maximum - binding.minimum);
    return true;
}

inline bool denormalize(const ParameterBinding& binding, double normalized,
                        int& value) noexcept
{
    if (!std::isfinite(normalized) || normalized < 0.0 || normalized > 1.0 ||
        binding.maximum <= binding.minimum)
        return false;
    value = binding.minimum + static_cast<int>(std::floor(
        normalized * static_cast<double>(binding.maximum - binding.minimum) + 0.5));
    return value >= binding.minimum && value <= binding.maximum;
}

struct RouteOptions final
{
    struct Option final
    {
        std::uint16_t token = 0;
        const char* symbol = "OFF";
    };

    static constexpr std::size_t kCapacity =
        modulation_contract::kOriginalTargetCount + 1u;
    std::array<Option, kCapacity> values {};
    std::size_t count = 0;

    constexpr bool contains(std::uint16_t token) const noexcept
    {
        for (std::size_t i = 0; i < count; ++i)
            if (values[i].token == token)
                return true;
        return false;
    }

    constexpr std::uint16_t adjacent(std::uint16_t token, int direction) const noexcept
    {
        if (count == 0u)
            return token;
        std::size_t at = 0u;
        while (at < count && values[at].token != token)
            ++at;
        if (at == count)
            return values[0].token;
        if (direction < 0)
            return values[(at + count - 1u) % count].token;
        return values[(at + 1u) % count].token;
    }
};

constexpr RouteOptions routeOptions(route_host::RouteHostParamId id) noexcept
{
    RouteOptions result;
    if (!id.valid())
        return result;
    result.values[result.count++] = {0u, "OFF"};
    for (std::uint16_t token = 1u;
         token < modulation_contract::kOriginalTargetCount; ++token)
    {
        const auto decoded = route_host::decode(
            id, route_host::RouteToken16::fromRaw(token));
        if (decoded.status != route_host::TokenStatus::Accepted)
            continue;
        const auto* target = modulation_contract::lookup(
            modulation_contract::OriginalTargetIndex::fromRaw(token));
        if (target != nullptr && result.count < result.values.size())
            result.values[result.count++] = {token, target->symbol};
    }
    return result;
}

class RouteSelection final
{
public:
    explicit constexpr RouteSelection(route_host::RouteHostParamId id) noexcept
        : mId(id) {}

    constexpr std::uint16_t accepted() const noexcept { return mAccepted; }

    constexpr route_host::TokenStatus trySelect(std::uint32_t raw) noexcept
    {
        const auto decoded = route_host::decode(
            mId, route_host::RouteToken16::fromRaw(raw));
        if (decoded.status == route_host::TokenStatus::Off ||
            decoded.status == route_host::TokenStatus::Accepted)
            mAccepted = static_cast<std::uint16_t>(raw);
        return decoded.status;
    }

private:
    route_host::RouteHostParamId mId;
    std::uint16_t mAccepted = 0u;
};

struct AudioReadback final
{
    std::uint64_t generation = 0u;
    double hostTempoBpm = 0.0;
    std::uint8_t activePattern = 0u;
    std::uint8_t pendingPattern = 0u;
    std::uint8_t sequencerEnabled = 0u;
    std::uint8_t clockMode = 0u;
    std::uint16_t internalTempo = 120u;
    bool hostContextPresent = false;
    bool hostTempoValid = false;
    bool hostTransportValid = false;
    bool hostPlaying = false;
    bool sequencerRunning = false;
    bool reanchored = false;
    std::uint8_t hostTransition = 0u;
    std::uint8_t mutedTracks = 0u;
    std::uint8_t rollTracks = 0u;
    std::array<std::uint8_t, 7> trackPositions {};
};

class ReadbackAuthority final
{
public:
    ReadbackAuthority() noexcept = default;
    ReadbackAuthority(const ReadbackAuthority&) = delete;
    ReadbackAuthority& operator=(const ReadbackAuthority&) = delete;

    void publish(double hostTempoBpm, bool hostContextPresent,
                 bool hostTempoFlag, bool hostTransportFlag, bool hostPlaying,
                 bool sequencerRunning, std::uint8_t activePattern,
                 std::uint8_t pendingPattern, std::uint8_t sequencerEnabled,
                 std::uint8_t clockMode, std::uint16_t internalTempo) noexcept
    {
        publish(hostTempoBpm, hostContextPresent, hostTempoFlag,
                hostTransportFlag, hostPlaying, sequencerRunning,
                activePattern, pendingPattern, sequencerEnabled, clockMode,
                internalTempo, {}, 0u, 0u, 0u, false);
    }

    void publish(double hostTempoBpm, bool hostContextPresent,
                 bool hostTempoFlag, bool hostTransportFlag, bool hostPlaying,
                 bool sequencerRunning, std::uint8_t activePattern,
                 std::uint8_t pendingPattern, std::uint8_t sequencerEnabled,
                 std::uint8_t clockMode, std::uint16_t internalTempo,
                 const std::array<std::uint8_t, 7>& trackPositions,
                 std::uint8_t mutedTracks, std::uint8_t rollTracks,
                 std::uint8_t hostTransition, bool reanchored) noexcept
    {
        const bool tempoValid = hostContextPresent && hostTempoFlag &&
            std::isfinite(hostTempoBpm) && hostTempoBpm >= 1.0 &&
            hostTempoBpm <= 255.0;
        std::uint64_t tempoBits = 0u;
        const double storedTempo = tempoValid ? hostTempoBpm : 0.0;
        static_assert(sizeof(tempoBits) == sizeof(storedTempo),
                      "Readback tempo requires a 64-bit double");
        std::memcpy(&tempoBits, &storedTempo, sizeof(tempoBits));

        std::uint64_t state = activePattern & 0x7u;
        state |= static_cast<std::uint64_t>(pendingPattern & 0x7u) << 3u;
        state |= static_cast<std::uint64_t>(sequencerEnabled != 0u) << 6u;
        state |= static_cast<std::uint64_t>(clockMode != 0u) << 7u;
        state |= static_cast<std::uint64_t>(hostContextPresent) << 8u;
        state |= static_cast<std::uint64_t>(tempoValid) << 9u;
        state |= static_cast<std::uint64_t>(
            hostContextPresent && hostTransportFlag) << 10u;
        state |= static_cast<std::uint64_t>(
            hostContextPresent && hostTransportFlag && hostPlaying) << 11u;
        state |= static_cast<std::uint64_t>(sequencerRunning) << 12u;
        state |= static_cast<std::uint64_t>(internalTempo & 0xffu) << 16u;
        state |= static_cast<std::uint64_t>(hostTransition & 0x0fu) << 24u;
        state |= static_cast<std::uint64_t>(reanchored) << 28u;

        std::uint64_t performance = mutedTracks & 0x7fu;
        performance |= static_cast<std::uint64_t>(rollTracks & 0x7fu) << 7u;
        for (std::size_t track = 0u; track < trackPositions.size(); ++track)
            performance |= static_cast<std::uint64_t>(
                trackPositions[track] & 0x7fu) << (14u + track * 7u);

        const auto odd = mSequence.fetch_add(1u, std::memory_order_acq_rel) + 1u;
        mTempoBits.store(tempoBits, std::memory_order_relaxed);
        mState.store(state, std::memory_order_relaxed);
        mPerformance.store(performance, std::memory_order_relaxed);
        mSequence.store(odd + 1u, std::memory_order_release);
    }

    bool trySnapshot(AudioReadback& output, unsigned attempts = 4u) const noexcept
    {
        for (unsigned attempt = 0; attempt < attempts; ++attempt)
        {
            const auto begin = mSequence.load(std::memory_order_acquire);
            if ((begin & 1u) != 0u)
                continue;
            const auto state = mState.load(std::memory_order_relaxed);
            const auto performance = mPerformance.load(std::memory_order_relaxed);
            const auto tempoBits = mTempoBits.load(std::memory_order_relaxed);
            const auto end = mSequence.load(std::memory_order_acquire);
            if (begin != end || (end & 1u) != 0u)
                continue;

            output = {};
            output.generation = end;
            std::memcpy(&output.hostTempoBpm, &tempoBits, sizeof(tempoBits));
            output.activePattern = static_cast<std::uint8_t>(state & 0x7u);
            output.pendingPattern = static_cast<std::uint8_t>((state >> 3u) & 0x7u);
            output.sequencerEnabled = static_cast<std::uint8_t>((state >> 6u) & 1u);
            output.clockMode = static_cast<std::uint8_t>((state >> 7u) & 1u);
            output.hostContextPresent = ((state >> 8u) & 1u) != 0u;
            output.hostTempoValid = ((state >> 9u) & 1u) != 0u;
            output.hostTransportValid = ((state >> 10u) & 1u) != 0u;
            output.hostPlaying = ((state >> 11u) & 1u) != 0u;
            output.sequencerRunning = ((state >> 12u) & 1u) != 0u;
            output.internalTempo = static_cast<std::uint16_t>((state >> 16u) & 0xffu);
            output.hostTransition = static_cast<std::uint8_t>((state >> 24u) & 0x0fu);
            output.reanchored = ((state >> 28u) & 1u) != 0u;
            output.mutedTracks = static_cast<std::uint8_t>(performance & 0x7fu);
            output.rollTracks = static_cast<std::uint8_t>((performance >> 7u) & 0x7fu);
            for (std::size_t track = 0u; track < output.trackPositions.size(); ++track)
                output.trackPositions[track] = static_cast<std::uint8_t>(
                    (performance >> (14u + track * 7u)) & 0x7fu);
            return true;
        }
        return false;
    }

private:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "P19-B x64 readback atomics must be lock-free");
    mutable std::atomic<std::uint64_t> mSequence {0u};
    std::atomic<std::uint64_t> mTempoBits {0u};
    std::atomic<std::uint64_t> mState {0u};
    std::atomic<std::uint64_t> mPerformance {0u};
};

} // namespace lxr::gui
