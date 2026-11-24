#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace lxr::p19f
{

inline constexpr int kEditorWidth = 1280;
inline constexpr int kEditorHeight = 800;
inline constexpr int kRegisteredParameters = 246;
inline constexpr int kVisibleParameters = 223;
inline constexpr int kHiddenParameters = 23;
inline constexpr int kRefreshHz = 30;

enum class AccessibilityClass : std::uint8_t
{
    NativeAccessible,
    KeyboardAccessibleOnly,
    FrameworkLimited,
    NotTested,
    Blocked,
    Failed
};

struct Rect final
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
               right <= kEditorWidth && bottom <= kEditorHeight;
    }
};

inline constexpr Rect kHeader {0, 0, 1280, 80};
inline constexpr Rect kNavigation {0, 80, 1280, 64 + 80};
inline constexpr Rect kVoiceNavigation {0, 144, 1280, 184};
inline constexpr Rect kViewContent {0, 184, 1280, 744};
inline constexpr Rect kStatusFooter {0, 744, 1280, 800};

inline constexpr std::array<int, kHiddenParameters> kIntentionallyHidden {{
    0, 5, 97, 98, 127,
    149, 150, 151, 152, 153, 154,
    161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172
}};

inline constexpr std::array<double, 4> kQualifiedScaleFactors {{
    1.0, 1.25, 1.5, 2.0
}};

constexpr bool intentionallyHidden(int id) noexcept
{
    for (const int hidden : kIntentionallyHidden)
        if (id == hidden)
            return true;
    return false;
}

constexpr int physicalPixels(int logicalPixels, double scale) noexcept
{
    return static_cast<int>(logicalPixels * scale + 0.5);
}

constexpr bool contractValid() noexcept
{
    if (!kHeader.valid() || !kNavigation.valid() ||
        !kVoiceNavigation.valid() || !kViewContent.valid() ||
        !kStatusFooter.valid())
        return false;
    for (std::size_t i = 0; i < kIntentionallyHidden.size(); ++i)
    {
        if (kIntentionallyHidden[i] < 0 ||
            kIntentionallyHidden[i] >= kRegisteredParameters)
            return false;
        for (std::size_t j = 0; j < i; ++j)
            if (kIntentionallyHidden[i] == kIntentionallyHidden[j])
                return false;
    }
    return kRegisteredParameters - kHiddenParameters == kVisibleParameters;
}

static_assert(contractValid(), "P19-F qualification contract must remain coherent");

} // namespace lxr::p19f
