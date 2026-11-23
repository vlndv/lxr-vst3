#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Included by LXRGUIFoundation.h inside namespace lxr::gui.
namespace sequencer_editor
{

inline constexpr int kSequencerCanvasWidth = 1280;
inline constexpr int kSequencerCanvasHeight = 800;

inline constexpr std::array<const char*, 7> kTrackNames {{
    "D1", "D2", "D3", "SN", "CY", "CH", "OH"
}};
inline constexpr std::array<const char*, 7> kTrackControlIds {{
    "lxr.seq.track.d1", "lxr.seq.track.d2", "lxr.seq.track.d3",
    "lxr.seq.track.sn", "lxr.seq.track.cy", "lxr.seq.track.ch",
    "lxr.seq.track.oh"
}};

struct Bounds final
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    constexpr int width() const noexcept { return right - left; }
    constexpr int height() const noexcept { return bottom - top; }
    constexpr bool valid() const noexcept
    {
        return left >= 0 && top >= 0 && right > left && bottom > top &&
               right <= kSequencerCanvasWidth && bottom <= kSequencerCanvasHeight;
    }
};

constexpr Bounds patternBounds(std::uint8_t pattern) noexcept
{
    return {36 + static_cast<int>(pattern) * 52, 194,
            84 + static_cast<int>(pattern) * 52, 226};
}
constexpr Bounds gridBounds(std::uint8_t track, std::uint8_t step) noexcept
{
    return {126 + static_cast<int>(step) * 48,
            250 + static_cast<int>(track) * 45,
            170 + static_cast<int>(step) * 48,
            289 + static_cast<int>(track) * 45};
}
constexpr Bounds muteBounds(std::uint8_t track) noexcept
{
    return {906, 250 + static_cast<int>(track) * 45,
            946, 289 + static_cast<int>(track) * 45};
}
constexpr Bounds rollBounds(std::uint8_t track) noexcept
{
    return {950, 250 + static_cast<int>(track) * 45,
            990, 289 + static_cast<int>(track) * 45};
}
constexpr Bounds substepBounds(std::uint8_t substep) noexcept
{
    return {1010 + static_cast<int>(substep % 4u) * 57,
            250 + static_cast<int>(substep / 4u) * 50,
            1061 + static_cast<int>(substep % 4u) * 57,
            294 + static_cast<int>(substep / 4u) * 50};
}

class Selection final
{
public:
    std::uint8_t pattern() const noexcept { return mPattern; }
    std::uint8_t track() const noexcept { return mTrack; }
    std::uint8_t mainStep() const noexcept { return mMainStep; }
    std::uint8_t substep() const noexcept { return mSubstep; }
    std::uint8_t lockSlot() const noexcept { return mLockSlot; }
    std::uint64_t generation() const noexcept { return mGeneration; }

    bool selectPattern(std::uint8_t value) noexcept
    {
        if (!::lxr::sequencer::IsValidPattern(value)) return false;
        if (mPattern != value) { mPattern = value; ++mGeneration; }
        return true;
    }
    bool selectStep(std::uint8_t track, std::uint8_t step) noexcept
    {
        if (!::lxr::sequencer::IsValidTrack(track) ||
            !::lxr::sequencer::IsValidMainStep(step)) return false;
        if (mTrack != track || mMainStep != step)
        { mTrack = track; mMainStep = step; ++mGeneration; }
        return true;
    }
    bool selectSubstep(std::uint8_t value) noexcept
    {
        if (!::lxr::sequencer::IsValidSubstep(value)) return false;
        if (mSubstep != value) { mSubstep = value; ++mGeneration; }
        return true;
    }
    bool selectLockSlot(std::uint8_t value) noexcept
    {
        if (value > 1u) return false;
        if (mLockSlot != value) { mLockSlot = value; ++mGeneration; }
        return true;
    }
private:
    std::uint8_t mPattern = 0u;
    std::uint8_t mTrack = 0u;
    std::uint8_t mMainStep = 0u;
    std::uint8_t mSubstep = 0u;
    std::uint8_t mLockSlot = 0u;
    std::uint64_t mGeneration = 0u;
};

constexpr bool stepDiffers(const ::lxr::sequencer::StepData& value,
                           const ::lxr::sequencer::StepData& expected) noexcept
{
    return value.volume != expected.volume ||
        value.probability != expected.probability || value.note != expected.note ||
        value.parameter1 != expected.parameter1 || value.value1 != expected.value1 ||
        value.parameter2 != expected.parameter2 || value.value2 != expected.value2;
}

inline bool hasStoredDetail(const ::lxr::sequencer::PatternData& pattern,
                            std::uint8_t track, std::uint8_t mainStep) noexcept
{
    if (!::lxr::sequencer::IsValidTrack(track) ||
        !::lxr::sequencer::IsValidMainStep(mainStep)) return false;
    for (std::uint8_t substep = 0u; substep < 8u; ++substep)
    {
        const auto& value = pattern.steps[track]
            [::lxr::sequencer::PhysicalPosition(mainStep, substep)];
        if (stepDiffers(value, ::lxr::sequencer::MakeDefaultStep(substep == 0u)))
            return true;
    }
    return false;
}

constexpr bool geometryValid() noexcept
{
    for (std::uint8_t pattern = 0u; pattern < 8u; ++pattern)
        if (!patternBounds(pattern).valid()) return false;
    for (std::uint8_t track = 0u; track < 7u; ++track)
    {
        if (!muteBounds(track).valid() || !rollBounds(track).valid()) return false;
        for (std::uint8_t step = 0u; step < 16u; ++step)
            if (!gridBounds(track, step).valid()) return false;
    }
    for (std::uint8_t substep = 0u; substep < 8u; ++substep)
        if (!substepBounds(substep).valid()) return false;
    return true;
}

static_assert(geometryValid(), "P19-E controls must remain inside 1280x800");

} // namespace sequencer_editor
