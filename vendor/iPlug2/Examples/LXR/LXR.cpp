//
// FILE: vendor/iPlug2/Examples/LXR/LXR.cpp
//
// P12b-4c — Engine-to-Host Sample-Rate Conversion
//
// Original LXR synthesis:
//   Engine sample rate: approximately 44002.757 Hz
//   Processing quantum: 32 native frames
//   Internal audio: signed 16-bit PCM
//
// Host conversion:
//   Polyphase FIR resampling through LXRAudioAdapter
//   Native synthesis timing remains unchanged.
//
// The plugin exposes ST1 as its primary stereo output.
// ST2 remains internal.
//

#include "LXR.h"
#include "IPlug_include_in_plug_src.h"

#include "../../../../dsp/LXRResourceLocator.h"
#include "../../../../../dsp/FIRLatency.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#if IPLUG_EDITOR
#include "IControls.h"
#endif
#include <string>
#if defined(VST3_API)
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"
#endif

namespace
{

#if IPLUG_EDITOR
constexpr int kKitNameControlTag = 17001;
constexpr int kKitStatusControlTag = 17002;
constexpr int kMorphTargetControlTag = 17003;
constexpr int kMorphStatusControlTag = 17004;
constexpr int kActiveViewControlTag = 17005;
constexpr int kSelectedVoiceControlTag = 17006;
constexpr int kPatternControlTag = 17007;
constexpr int kClockControlTag = 17008;
constexpr int kTempoControlTag = 17009;
constexpr int kTransportControlTag = 17010;
constexpr int kSequencerStatusControlTag = 17011;
constexpr int kHeaderMorphControlTag = 17012;
constexpr int kOperationStatusControlTag = 17013;
constexpr int kKeyboardStatusControlTag = 17014;
constexpr int kFocusRingControlTag = 17015;
constexpr int kKitIdentityDetailTag = 17016;
constexpr int kNavControlTag = 18000;
constexpr int kVoiceControlTag = 18100;
constexpr int kOperationControlTag = 18200;
constexpr int kImportKitOperationTag = kOperationControlTag;
constexpr int kExportKitOperationTag = kOperationControlTag + 1;
constexpr int kLoadMorphOperationTag = kOperationControlTag + 2;
constexpr int kClearMorphOperationTag = kOperationControlTag + 3;
constexpr int kExportMorphOperationTag = kOperationControlTag + 4;
constexpr int kParameterControlTag = 19000;
constexpr int kNoTag = iplug::kNoTag;
constexpr std::uint8_t kKitGUIOperation = 1u;
constexpr std::uint8_t kMorphGUIOperation = 2u;
constexpr std::array<const char*, 6> kSoundGroups {{
    "lxr.view.sound.d1", "lxr.view.sound.d2", "lxr.view.sound.d3",
    "lxr.view.sound.sn", "lxr.view.sound.cy", "lxr.view.sound.hh" }};
constexpr std::array<const char*, 6> kModulationGroups {{
    "lxr.view.modulation.d1", "lxr.view.modulation.d2", "lxr.view.modulation.d3",
    "lxr.view.modulation.sn", "lxr.view.modulation.cy", "lxr.view.modulation.hh" }};

const iplug::igraphics::IColor kLXRBackground(255, 15, 20, 29);
const iplug::igraphics::IColor kLXRPanel(255, 27, 35, 48);
const iplug::igraphics::IColor kLXRPanelLight(255, 39, 50, 67);
const iplug::igraphics::IColor kLXRText(255, 233, 238, 244);
const iplug::igraphics::IColor kLXRMuted(255, 160, 174, 191);
const iplug::igraphics::IColor kLXRAccent(255, 44, 214, 188);
const iplug::igraphics::IColor kLXRWarning(255, 255, 190, 73);
const iplug::igraphics::IColor kLXRError(255, 235, 92, 92);

class LXRSoundControl final : public iplug::igraphics::IControl
{
public:
    LXRSoundControl(const iplug::igraphics::IRECT& bounds,
                    const lxr::gui::SoundParameterBinding& binding)
        : IControl(bounds, binding.parameter), mBinding(binding)
    {
        SetTooltip(binding.controlId);
    }

    void Draw(iplug::igraphics::IGraphics& graphics) override
    {
        using namespace iplug::igraphics;
        const int raw = rawValue();
        const bool unsupported = mBinding.enumeration !=
                lxr::gui::EnumerationDomain::None &&
            !lxr::gui::enumerationValueSupported(mBinding.enumeration, raw);
        graphics.FillRoundRect(kLXRPanelLight, mRECT, 5.f, &mBlend);
        graphics.DrawRoundRect(mFocused ? kLXRWarning :
                (unsupported ? kLXRError : kLXRAccent),
            mRECT, 5.f, &mBlend, mFocused ? 3.f : 1.f);
        graphics.DrawText(IText(11.f, kLXRMuted, "LXRSystem",
                          EAlign::Center, EVAlign::Top),
                          mBinding.label, mRECT.GetPadded(-4.f), &mBlend);
        WDL_String display;
        formatValue(raw, display);
        graphics.DrawText(IText(14.f, unsupported ? kLXRError : kLXRText,
                          "LXRSystem"), display.Get(),
                          mRECT.GetPadded(-4.f, -19.f, -4.f, -17.f), &mBlend);
        const IRECT track(mRECT.L + 8.f, mRECT.B - 13.f,
                          mRECT.R - 8.f, mRECT.B - 7.f);
        graphics.FillRoundRect(kLXRBackground, track, 2.f, &mBlend);
        IRECT fill = track;
        fill.R = fill.L + static_cast<float>(GetValue()) * track.W();
        graphics.FillRoundRect(unsupported ? kLXRError : kLXRAccent,
                               fill, 2.f, &mBlend);
    }

    void OnMouseDown(float x, float, const iplug::igraphics::IMouseMod& modifier) override
    {
        if (isDiscrete())
            step(modifier.S ? -1 : 1, 1);
        else
            setRawFromX(x);
    }

    void OnMouseDrag(float, float, float dX, float dY,
                     const iplug::igraphics::IMouseMod& modifier) override
    {
        if (isDiscrete()) return;
        mDragRemainder += dX - dY;
        const float threshold = modifier.S ? 8.f : 2.f;
        const int whole = static_cast<int>(mDragRemainder / threshold);
        if (whole != 0)
        {
            step(whole < 0 ? -1 : 1, whole < 0 ? -whole : whole);
            mDragRemainder -= static_cast<float>(whole) * threshold;
        }
    }

    void OnMouseUp(float, float, const iplug::igraphics::IMouseMod&) override
    {
        mDragRemainder = 0.f;
    }

    void OnMouseWheel(float, float, const iplug::igraphics::IMouseMod& modifier,
                      float delta) override
    {
        if (delta == 0.f) return;
        auto* delegate = GetDelegate();
        if (delegate == nullptr) return;
        delegate->BeginInformHostOfParamChangeFromUI(GetParamIdx());
        step(delta < 0.f ? -1 : 1, modifier.S ? 1 : 4);
        delegate->EndInformHostOfParamChangeFromUI(GetParamIdx());
    }

    void OnMouseDblClick(float, float,
                         const iplug::igraphics::IMouseMod&) override
    {
        auto* delegate = GetDelegate();
        if (delegate == nullptr) return;
        delegate->BeginInformHostOfParamChangeFromUI(GetParamIdx());
        setRaw(lxr::vst::kParamDescriptors[
            static_cast<std::size_t>(mBinding.parameter)].defaultValue);
        delegate->EndInformHostOfParamChangeFromUI(GetParamIdx());
    }

    void SetFocused(bool focused)
    {
        if (mFocused == focused) return;
        mFocused = focused;
        SetDirty(false);
    }

    void SetValueFromUserInput(double value, int valueIndex = 0) override
    {
        if (!std::isfinite(value) || value < 0.0 || value > 1.0)
            return;
        if (isDiscrete())
        {
            int raw = 0;
            if (!lxr::gui::denormalizeNativeInteger(value, raw) ||
                !lxr::gui::enumerationValueSupported(mBinding.enumeration, raw))
                return;
        }
        IControl::SetValueFromUserInput(value, valueIndex);
    }

    void KeyboardStep(int direction, int amount)
    {
        auto* delegate = GetDelegate();
        if (delegate == nullptr) return;
        delegate->BeginInformHostOfParamChangeFromUI(GetParamIdx());
        step(direction, amount);
        delegate->EndInformHostOfParamChangeFromUI(GetParamIdx());
    }

    void KeyboardEndpoint(bool maximum)
    {
        auto* delegate = GetDelegate();
        if (delegate == nullptr) return;
        delegate->BeginInformHostOfParamChangeFromUI(GetParamIdx());
        setRaw(lxr::gui::soundEndpoint(mBinding, maximum));
        delegate->EndInformHostOfParamChangeFromUI(GetParamIdx());
    }

private:
    bool isDiscrete() const noexcept
    {
        return mBinding.kind == lxr::gui::SoundControlKind::Enumeration ||
            mBinding.kind == lxr::gui::SoundControlKind::Boolean;
    }

    int rawValue() const noexcept
    {
        int raw = 0;
        (void) lxr::gui::denormalizeNativeInteger(GetValue(), raw);
        return raw;
    }

    void setRaw(int raw)
    {
        double normalized = 0.0;
        if (!lxr::gui::normalizeNativeInteger(raw, normalized)) return;
        SetValue(normalized);
        SetDirty(true);
    }

    void setRawFromX(float x)
    {
        const float width = mRECT.W();
        if (width <= 0.f) return;
        const double normalized = iplug::Clip(
            static_cast<double>((x - mRECT.L) / width), 0.0, 1.0);
        int raw = 0;
        if (lxr::gui::denormalizeNativeInteger(normalized, raw))
            setRaw(raw);
    }

    void step(int direction, int amount)
    {
        const int raw = rawValue();
        if (isDiscrete())
        {
            setRaw(lxr::gui::adjacentEnumerationValue(
                mBinding.enumeration, raw, direction));
            return;
        }
        const int signedAmount = direction < 0 ? -amount : amount;
        setRaw(iplug::Clip(raw + signedAmount, 0, 127));
    }

    void formatValue(int raw, WDL_String& display) const
    {
        if (mBinding.enumeration != lxr::gui::EnumerationDomain::None)
        {
            const char* label = lxr::gui::enumerationLabel(
                mBinding.enumeration, raw);
            if (label != nullptr)
                display.SetFormatted(48, "%s  [%d]", label, raw);
            else
                display.SetFormatted(64, "Unsupported [%d]", raw);
            return;
        }
        if (mBinding.kind == lxr::gui::SoundControlKind::Note)
        {
            static constexpr std::array<const char*, 12> names {{
                "C", "C#", "D", "D#", "E", "F",
                "F#", "G", "G#", "A", "A#", "B" }};
            display.SetFormatted(48, "%s%d  [%d]",
                names[static_cast<std::size_t>(raw % 12)], raw / 12 - 1, raw);
            return;
        }
        display.SetFormatted(24, "%d", raw);
    }

    const lxr::gui::SoundParameterBinding& mBinding;
    float mDragRemainder = 0.f;
    bool mFocused = false;
};

class LXRModulationControl final : public iplug::igraphics::IControl
{
public:
    LXRModulationControl(
        const iplug::igraphics::IRECT& bounds,
        const lxr::gui::ModulationParameterBinding& binding)
        : IControl(bounds, binding.parameter), mBinding(binding)
    {
        SetTooltip(binding.controlId);
    }

    void Draw(iplug::igraphics::IGraphics& graphics) override
    {
        using namespace iplug::igraphics;
        const int raw = rawValue();
        const bool unsupported = !valueAcceptedForDisplay(raw);
        graphics.FillRoundRect(kLXRPanelLight, mRECT, 5.f, &mBlend);
        graphics.DrawRoundRect(mFocused ? kLXRWarning :
                (unsupported ? kLXRError : kLXRAccent),
            mRECT, 5.f, &mBlend, mFocused ? 3.f : 1.f);
        graphics.DrawText(IText(11.f, kLXRMuted, "LXRSystem",
                          EAlign::Center, EVAlign::Top),
                          mBinding.label, mRECT.GetPadded(-4.f), &mBlend);
        WDL_String display;
        formatValue(raw, display);
        graphics.DrawText(IText(mBinding.domain ==
                lxr::gui::ModulationDomain::TypedRoute ? 12.f : 14.f,
                unsupported ? kLXRError : kLXRText, "LXRSystem"),
            display.Get(), mRECT.GetPadded(-5.f, -19.f, -5.f, -15.f), &mBlend);
        if (mBinding.domain == lxr::gui::ModulationDomain::IntegerByte)
        {
            const IRECT track(mRECT.L + 8.f, mRECT.B - 13.f,
                              mRECT.R - 8.f, mRECT.B - 7.f);
            graphics.FillRoundRect(kLXRBackground, track, 2.f, &mBlend);
            IRECT fill = track;
            fill.R = fill.L + static_cast<float>(GetValue()) * track.W();
            graphics.FillRoundRect(kLXRAccent, fill, 2.f, &mBlend);
        }
    }

    void OnMouseDown(float x, float,
                     const iplug::igraphics::IMouseMod& modifier) override
    {
        if (mBinding.domain == lxr::gui::ModulationDomain::IntegerByte)
            setRawFromX(x);
        else
            step(modifier.S ? -1 : 1, 1);
    }

    void OnMouseDrag(float, float, float dX, float dY,
                     const iplug::igraphics::IMouseMod& modifier) override
    {
        if (mBinding.domain != lxr::gui::ModulationDomain::IntegerByte)
            return;
        mDragRemainder += dX - dY;
        const float threshold = modifier.S ? 8.f : 2.f;
        const int whole = static_cast<int>(mDragRemainder / threshold);
        if (whole != 0)
        {
            step(whole < 0 ? -1 : 1, whole < 0 ? -whole : whole);
            mDragRemainder -= static_cast<float>(whole) * threshold;
        }
    }

    void OnMouseUp(float, float,
                   const iplug::igraphics::IMouseMod&) override
    {
        mDragRemainder = 0.f;
    }

    void OnMouseWheel(float, float,
                      const iplug::igraphics::IMouseMod& modifier,
                      float delta) override
    {
        if (delta == 0.f) return;
        auto* delegate = GetDelegate();
        if (delegate == nullptr) return;
        delegate->BeginInformHostOfParamChangeFromUI(GetParamIdx());
        step(delta < 0.f ? -1 : 1,
             modifier.S || mBinding.domain !=
                 lxr::gui::ModulationDomain::IntegerByte ? 1 : 4);
        delegate->EndInformHostOfParamChangeFromUI(GetParamIdx());
    }

    void OnMouseDblClick(float, float,
                         const iplug::igraphics::IMouseMod&) override
    {
        auto* delegate = GetDelegate();
        if (delegate == nullptr) return;
        delegate->BeginInformHostOfParamChangeFromUI(GetParamIdx());
        setRaw(mBinding.defaultValue);
        delegate->EndInformHostOfParamChangeFromUI(GetParamIdx());
    }

    void SetFocused(bool focused)
    {
        if (mFocused == focused) return;
        mFocused = focused;
        SetDirty(false);
    }

    void SetValueFromUserInput(double value, int valueIndex = 0) override
    {
        if (!std::isfinite(value) || value < 0.0 || value > 1.0)
            return;
        const int raw = denormalize(value);
        if (mBinding.domain != lxr::gui::ModulationDomain::IntegerByte &&
            !valueSelectable(raw))
            return;
        IControl::SetValueFromUserInput(value, valueIndex);
    }

    void KeyboardStep(int direction, int amount)
    {
        auto* delegate = GetDelegate();
        if (delegate == nullptr) return;
        delegate->BeginInformHostOfParamChangeFromUI(GetParamIdx());
        step(direction, amount);
        delegate->EndInformHostOfParamChangeFromUI(GetParamIdx());
    }

    void KeyboardEndpoint(bool maximum)
    {
        auto* delegate = GetDelegate();
        if (delegate == nullptr) return;
        delegate->BeginInformHostOfParamChangeFromUI(GetParamIdx());
        if (mBinding.domain == lxr::gui::ModulationDomain::TypedRoute)
        {
            const auto options = lxr::gui::routeOptions(
                lxr::route_host::RouteHostParamId::fromRaw(
                    static_cast<std::uint32_t>(mBinding.parameter)));
            setRaw(maximum && options.count != 0u
                ? options.values[options.count - 1u].token : 0);
        }
        else
            setRaw(maximum
                ? lxr::gui::modulationSelectableMaximum(mBinding.domain) : 0);
        delegate->EndInformHostOfParamChangeFromUI(GetParamIdx());
    }

private:
    int denormalize(double normalized) const noexcept
    {
        return mBinding.minimum + static_cast<int>(std::floor(
            normalized * static_cast<double>(mBinding.maximum -
                mBinding.minimum) + 0.5));
    }

    int rawValue() const noexcept { return denormalize(GetValue()); }

    bool valueSelectable(int raw) const noexcept
    {
        if (mBinding.domain != lxr::gui::ModulationDomain::TypedRoute)
            return lxr::gui::modulationValueSelectable(mBinding.domain, raw);
        if (raw == 0) return true;
        const auto options = lxr::gui::routeOptions(
            lxr::route_host::RouteHostParamId::fromRaw(
                static_cast<std::uint32_t>(mBinding.parameter)));
        for (std::size_t i = 0; i < options.count; ++i)
            if (options.values[i].token == raw) return true;
        return false;
    }

    bool valueAcceptedForDisplay(int raw) const noexcept
    {
        return mBinding.domain == lxr::gui::ModulationDomain::IntegerByte ||
               valueSelectable(raw);
    }

    void setRaw(int raw)
    {
        if (raw < mBinding.minimum || raw > mBinding.maximum) return;
        const int span = mBinding.maximum - mBinding.minimum;
        if (span <= 0) return;
        SetValue(static_cast<double>(raw - mBinding.minimum) /
                 static_cast<double>(span));
        SetDirty(true);
    }

    void setRawFromX(float x)
    {
        if (mRECT.W() <= 0.f) return;
        setRaw(denormalize(iplug::Clip(
            static_cast<double>((x - mRECT.L) / mRECT.W()), 0.0, 1.0)));
    }

    void step(int direction, int amount)
    {
        int raw = rawValue();
        if (mBinding.domain == lxr::gui::ModulationDomain::IntegerByte)
        {
            const int signedAmount = direction < 0 ? -amount : amount;
            setRaw(iplug::Clip(raw + signedAmount,
                              mBinding.minimum, mBinding.maximum));
            return;
        }
        while (amount-- > 0)
            raw = lxr::gui::adjacentModulationValue(mBinding, raw, direction);
        setRaw(raw);
    }

    void formatValue(int raw, WDL_String& display) const
    {
        if (mBinding.domain == lxr::gui::ModulationDomain::TypedRoute)
        {
            if (raw == 0) { display.Set("OFF [0]"); return; }
            const auto options = lxr::gui::routeOptions(
                lxr::route_host::RouteHostParamId::fromRaw(
                    static_cast<std::uint32_t>(mBinding.parameter)));
            for (std::size_t i = 0; i < options.count; ++i)
                if (options.values[i].token == raw)
                {
                    display.SetFormatted(128, "%u: %s", raw,
                                         options.values[i].symbol);
                    return;
                }
            display.SetFormatted(96, "%u: invalid / unsupported", raw);
            return;
        }
        const char* label = lxr::gui::modulationValueLabel(
            mBinding.domain, raw);
        if (label != nullptr)
            display.SetFormatted(64, "%s [%d]", label, raw);
        else if (mBinding.domain != lxr::gui::ModulationDomain::IntegerByte)
            display.SetFormatted(64, "Unsupported [%d]", raw);
        else
            display.SetFormatted(24, "%d", raw);
    }

    const lxr::gui::ModulationParameterBinding& mBinding;
    float mDragRemainder = 0.f;
    bool mFocused = false;
};

class LXRDiscreteSelector final : public iplug::igraphics::IControl
{
public:
    LXRDiscreteSelector(const iplug::igraphics::IRECT& bounds,
                        const lxr::gui::ParameterBinding& binding)
        : IControl(bounds, binding.parameter), mBinding(binding)
    {
        SetTooltip(binding.controlId);
    }

    void Draw(iplug::igraphics::IGraphics& graphics) override
    {
        using namespace iplug::igraphics;
        graphics.FillRect(kLXRPanelLight, mRECT, &mBlend);
        graphics.DrawRect(mFocused ? kLXRWarning : kLXRAccent,
                          mRECT, &mBlend, mFocused ? 3.f : 1.f);
        graphics.DrawText(IText(14.f, kLXRMuted, "LXRSystem",
                          EAlign::Center, EVAlign::Top),
                          mBinding.label, mRECT.GetPadded(-6.f), &mBlend);
        WDL_String display;
        formatValue(rawValue(), display);
        graphics.DrawText(IText(16.f, kLXRText, "LXRSystem"),
                          display.Get(), mRECT.GetPadded(-8.f, -20.f, -8.f, -4.f),
                          &mBlend);
    }

    void OnMouseDown(float, float, const iplug::igraphics::IMouseMod& modifier) override
    {
        step(modifier.S ? -1 : 1);
    }

    void OnMouseWheel(float, float, const iplug::igraphics::IMouseMod&,
                      float delta) override
    {
        if (delta == 0.f) return;
        auto* delegate = GetDelegate();
        if (delegate == nullptr) return;
        delegate->BeginInformHostOfParamChangeFromUI(GetParamIdx());
        step(delta < 0.f ? -1 : 1);
        delegate->EndInformHostOfParamChangeFromUI(GetParamIdx());
    }

    void OnMouseDblClick(float, float,
                         const iplug::igraphics::IMouseMod&) override
    {
        auto* delegate = GetDelegate();
        if (delegate == nullptr) return;
        delegate->BeginInformHostOfParamChangeFromUI(GetParamIdx());
        setRaw(mBinding.defaultValue);
        delegate->EndInformHostOfParamChangeFromUI(GetParamIdx());
    }

    void SetFocused(bool focused)
    {
        if (mFocused == focused) return;
        mFocused = focused;
        SetDirty(false);
    }

    void KeyboardStep(int direction)
    {
        auto* delegate = GetDelegate();
        if (delegate == nullptr) return;
        delegate->BeginInformHostOfParamChangeFromUI(GetParamIdx());
        step(direction);
        delegate->EndInformHostOfParamChangeFromUI(GetParamIdx());
    }

private:
    int rawValue() const noexcept
    {
        const auto span = mBinding.maximum - mBinding.minimum;
        return mBinding.minimum + static_cast<int>(std::floor(
            GetValue() * static_cast<double>(span) + 0.5));
    }

    void setRaw(int raw)
    {
        if (raw < mBinding.minimum || raw > mBinding.maximum) return;
        const auto span = mBinding.maximum - mBinding.minimum;
        SetValue(static_cast<double>(raw - mBinding.minimum) /
                 static_cast<double>(span));
        SetDirty(true);
    }

    void step(int direction)
    {
        const auto raw = rawValue();
        if (mBinding.kind == lxr::gui::ControlKind::TypedRoute)
        {
            const auto options = lxr::gui::routeOptions(
                lxr::route_host::RouteHostParamId::fromRaw(
                    static_cast<std::uint32_t>(mBinding.parameter)));
            setRaw(options.adjacent(static_cast<std::uint16_t>(raw), direction));
            return;
        }
        if (mBinding.kind == lxr::gui::ControlKind::SourceToggle)
        {
            // Firmware DTYPE_ON_OFF selects identities 0 and 1. Restored
            // legacy byte values above 1 remain readable until explicitly
            // changed rather than being silently coerced.
            setRaw(raw == 0 ? 1 : 0);
            return;
        }
        if (mBinding.kind == lxr::gui::ControlKind::Enumeration)
        {
            // Only the source-defined built-in wave range is selectable. An
            // unsupported restored SD value remains visible until changed.
            const int at = raw >= 0 && raw <= 5 ? raw : (direction < 0 ? 0 : 5);
            setRaw((at + (direction < 0 ? 5 : 1)) % 6);
        }
    }

    void formatValue(int raw, WDL_String& display) const
    {
        if (mBinding.kind == lxr::gui::ControlKind::TypedRoute)
        {
            if (raw == 0) { display.Set("OFF"); return; }
            const auto options = lxr::gui::routeOptions(
                lxr::route_host::RouteHostParamId::fromRaw(
                    static_cast<std::uint32_t>(mBinding.parameter)));
            for (std::size_t i = 0; i < options.count; ++i)
                if (options.values[i].token == raw)
                {
                    display.SetFormatted(96, "%u: %s", raw, options.values[i].symbol);
                    return;
                }
            display.SetFormatted(64, "%u: invalid for owner", raw);
            return;
        }
        if (mBinding.kind == lxr::gui::ControlKind::SourceToggle)
        {
            display.SetFormatted(32, "%s (%d)", raw == 0 ? "OFF" : "ON", raw);
            return;
        }
        if (raw <= 5) display.SetFormatted(32, "Built-in %d", raw);
        else display.SetFormatted(64, "Unsupported SD value %d", raw);
    }

    const lxr::gui::ParameterBinding& mBinding;
    bool mFocused = false;
};

class LXRFocusRing final : public iplug::igraphics::IControl
{
public:
    LXRFocusRing() : IControl(iplug::igraphics::IRECT())
    {
        SetIgnoreMouse(true);
        Hide(true);
    }
    void Draw(iplug::igraphics::IGraphics& graphics) override
    {
        graphics.DrawRect(kLXRWarning, mRECT, &mBlend, 3.f);
    }
};

bool SetTextIfChanged(iplug::igraphics::IControl* control, const char* value)
{
    if (control == nullptr || value == nullptr) return false;
    auto* text = static_cast<iplug::igraphics::ITextControl*>(control);
    if (std::strcmp(text->GetStr(), value) == 0) return false;
    text->SetStr(value);
    return true;
}
#endif

void PopulateMorphOperationState(
    lxr::morph::management::OperationResult& operation,
    const lxr::morph::Snapshot& snapshot) noexcept
{
    operation.targetPresent = snapshot.targetPresent;
    operation.amount = snapshot.amount;
    operation.generation = snapshot.generation;
    operation.name = snapshot.targetPresent
        ? snapshot.target.name : std::array<std::uint8_t, 8>{};
}

#if defined(VST3_API)
// Non-owning, callback-scoped adapters for the installed iPlug2 dispatcher.
// It reads each queue synchronously and does not retain these interfaces.
// Only MIDI CC proxies are canonicalized: native automation is forwarded
// untouched. Hosts may supply float-precision normalized MIDI (e.g. CC98=1
// becomes 0.999999996 after *127); MakeControlChangeMsg otherwise truncates it
// to the wrong wire value before ProcessMidiMsg can recover the selector.
class MidiValueQueue final : public Steinberg::Vst::IParamValueQueue
{
public:
    Steinberg::Vst::IParamValueQueue* source = nullptr;
    Steinberg::int32 pointIndex = 0;
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** object) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::Vst::IParamValueQueue::iid) ||
            Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid))
        {
            *object = static_cast<Steinberg::Vst::IParamValueQueue*>(this);
            return Steinberg::kResultOk;
        }
        *object = nullptr;
        return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
    Steinberg::Vst::ParamID PLUGIN_API getParameterId() override { return source->getParameterId(); }
    Steinberg::int32 PLUGIN_API getPointCount() override { return 1; }
    Steinberg::tresult PLUGIN_API getPoint(Steinberg::int32 index, Steinberg::int32& offset,
                                          Steinberg::Vst::ParamValue& value) override
    {
        if (index != 0)
            return Steinberg::kResultFalse;
        const auto result = source->getPoint(pointIndex, offset, value);
        if (result != Steinberg::kResultOk)
            return result;
        if (!std::isfinite(value))
            return Steinberg::kResultFalse;
        value = std::round(iplug::Clip(value, 0.0, 1.0) * 127.0) / 127.0;
        return result;
    }
    Steinberg::tresult PLUGIN_API addPoint(Steinberg::int32, Steinberg::Vst::ParamValue,
                                          Steinberg::int32&) override { return Steinberg::kNotImplemented; }
};

bool DecodeNormalizedRouteToken(
    LXR& plug, Steinberg::Vst::ParamID tag,
    Steinberg::Vst::ParamValue normalized,
    lxr::route_host::RouteToken16& token) noexcept
{
    const auto id = lxr::route_host::RouteHostParamId::fromRaw(tag);
    if (!id.valid() || !std::isfinite(normalized) ||
        normalized < 0.0 || normalized > 1.0)
        return false;
    const auto* param = plug.GetParam(static_cast<int>(tag));
    if (param == nullptr)
        return false;
    const double plain = param->FromNormalized(normalized);
    if (!std::isfinite(plain) || plain < 0.0 ||
        plain > static_cast<double>(lxr::route_host::kMaximumToken))
        return false;
    token = lxr::route_host::RouteToken16::fromRaw(
        static_cast<std::uint32_t>(plain));
    return token.valid() && static_cast<bool>(lxr::route_host::decode(id, token));
}

class RouteValueQueue final : public Steinberg::Vst::IParamValueQueue
{
public:
    LXR* plug = nullptr;
    Steinberg::Vst::IParamValueQueue* source = nullptr;
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** object) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::Vst::IParamValueQueue::iid) ||
            Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid))
        {
            *object = static_cast<Steinberg::Vst::IParamValueQueue*>(this);
            return Steinberg::kResultOk;
        }
        *object = nullptr;
        return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
    Steinberg::Vst::ParamID PLUGIN_API getParameterId() override { return source->getParameterId(); }
    Steinberg::int32 PLUGIN_API getPointCount() override { return source->getPointCount(); }
    Steinberg::tresult PLUGIN_API getPoint(Steinberg::int32 index, Steinberg::int32& offset,
                                          Steinberg::Vst::ParamValue& value) override
    {
        const auto result = source->getPoint(index, offset, value);
        lxr::route_host::RouteToken16 token;
        return result == Steinberg::kResultOk && plug != nullptr &&
               DecodeNormalizedRouteToken(*plug, getParameterId(), value, token)
            ? result : Steinberg::kResultFalse;
    }
    Steinberg::tresult PLUGIN_API addPoint(Steinberg::int32, Steinberg::Vst::ParamValue,
                                          Steinberg::int32&) override { return Steinberg::kNotImplemented; }
};

class TransportValueQueue final : public Steinberg::Vst::IParamValueQueue
{
public:
    LXR* plug = nullptr;
    Steinberg::Vst::IParamValueQueue* source = nullptr;
    Steinberg::tresult PLUGIN_API queryInterface(
        const Steinberg::TUID iid, void** object) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual(
                iid, Steinberg::Vst::IParamValueQueue::iid) ||
            Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid))
        {
            *object = static_cast<Steinberg::Vst::IParamValueQueue*>(this);
            return Steinberg::kResultOk;
        }
        *object = nullptr;
        return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
    Steinberg::Vst::ParamID PLUGIN_API getParameterId() override
    {
        return source->getParameterId();
    }
    Steinberg::int32 PLUGIN_API getPointCount() override
    {
        return source->getPointCount();
    }
    Steinberg::tresult PLUGIN_API getPoint(
        Steinberg::int32 index, Steinberg::int32& offset,
        Steinberg::Vst::ParamValue& value) override
    {
        const auto result = source->getPoint(index, offset, value);
        if (result != Steinberg::kResultOk || plug == nullptr ||
            !std::isfinite(value) || value < 0.0 || value > 1.0)
            return Steinberg::kResultFalse;
        const auto* param = plug->GetParam(static_cast<int>(getParameterId()));
        if (param == nullptr)
            return Steinberg::kResultFalse;
        value = param->ToNormalized(param->FromNormalized(value));
        return Steinberg::kResultOk;
    }
    Steinberg::tresult PLUGIN_API addPoint(
        Steinberg::int32, Steinberg::Vst::ParamValue,
        Steinberg::int32&) override
    {
        return Steinberg::kNotImplemented;
    }
};

class MidiParameterChanges final : public Steinberg::Vst::IParameterChanges
{
public:
    // One queue per registered native/proxy/bypass identity is sufficient for
    // valid host input. Counts/points from the API are otherwise unbounded.
    static constexpr Steinberg::int32 kMaxSources = lxr::vst::kNumParams +
        VST3_NUM_CC_CHANS * Steinberg::Vst::kCountCtrlNumber + 1;
    static constexpr Steinberg::int32 kMaxCCPoints = 4096;

    MidiParameterChanges(LXR& owner, Steinberg::Vst::IParameterChanges* changes,
        std::atomic<uint64_t>& rejectedQueues,
        std::atomic<uint64_t>& rejectedRecords,
        std::atomic<uint64_t>& malformedCounts)
        : plug(owner)
    {
        if (changes == nullptr)
            return;
        const auto reported = changes->getParameterCount();
        if (reported < 0)
        {
            malformedCounts.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const auto count = std::min(reported, kMaxSources);
        if (reported > count)
            rejectedQueues.fetch_add(static_cast<uint64_t>(reported - count), std::memory_order_relaxed);
        Steinberg::int32 remainingCC = kMaxCCPoints;
        for (Steinberg::int32 i = 0; i < count; ++i)
        {
            auto* queue = changes->getParameterData(i);
            if (queue == nullptr)
                continue;
            const auto tag = queue->getParameterId();
            // Do not turn an unadvertised proxy ID into another MIDI channel.
            if (tag >= iplug::kMIDICCParamStartIdx &&
                tag >= iplug::kMIDICCParamStartIdx + VST3_NUM_CC_CHANS * Steinberg::Vst::kCountCtrlNumber)
            {
                malformedCounts.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            const auto points = queue->getPointCount();
            if (points < 0)
            {
                malformedCounts.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            if (points == 0)
                continue;
            const bool cc = isCC(tag);
            const auto accepted = cc ? std::min(points, remainingCC) : 1;
            if (cc)
            {
                remainingCC -= accepted;
                if (points > accepted)
                    rejectedRecords.fetch_add(static_cast<uint64_t>(points - accepted), std::memory_order_relaxed);
            }
            if (accepted != 0)
            {
                sources[sourceCount++] = {queue, flattenedCount, accepted};
                flattenedCount += accepted;
            }
        }
    }
    Steinberg::int32 sourceQueueCount() const noexcept { return sourceCount; }
    Steinberg::Vst::IParamValueQueue* sourceQueue(Steinberg::int32 index) const noexcept
    { return sources[index].queue; }
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** object) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::Vst::IParameterChanges::iid) ||
            Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid))
        {
            *object = static_cast<Steinberg::Vst::IParameterChanges*>(this);
            return Steinberg::kResultOk;
        }
        *object = nullptr;
        return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
    Steinberg::int32 PLUGIN_API getParameterCount() override { return flattenedCount; }
    Steinberg::Vst::IParamValueQueue* PLUGIN_API getParameterData(Steinberg::int32 index) override
    {
        if (index < 0 || index >= flattenedCount)
            return nullptr;
        // Callback-local sorted table; at most9 binary-search comparisons.
        Steinberg::int32 lo = 0, hi = sourceCount;
        while (lo < hi)
        {
            const auto mid = lo + (hi - lo) / 2;
            if (sources[mid].first <= index) lo = mid + 1;
            else hi = mid;
        }
        const auto& entry = sources[lo - 1];
        auto* queue = entry.queue;
        const auto tag = queue->getParameterId();
        if (lxr::route_host::RouteHostParamId::fromRaw(tag).valid())
        {
            routeQueue.plug = &plug;
            routeQueue.source = queue;
            return &routeQueue;
        }
        if (tag >= static_cast<Steinberg::Vst::ParamID>(
                lxr::vst::kSequencerEnableParam) &&
            tag <= static_cast<Steinberg::Vst::ParamID>(
                lxr::vst::kSequencerRollTracksParam))
        {
            transportQueue.plug = &plug;
            transportQueue.source = queue;
            return &transportQueue;
        }
        if (isCC(tag))
        {
            midiQueue.source = queue;
            midiQueue.pointIndex = index - entry.first;
            return &midiQueue;
        }
        return queue;
    }
    Steinberg::Vst::IParamValueQueue* PLUGIN_API addParameterData(const Steinberg::Vst::ParamID&,
                                                                Steinberg::int32&) override { return nullptr; }
private:
    static bool isCC(Steinberg::Vst::ParamID tag) noexcept
    {
        return tag >= iplug::kMIDICCParamStartIdx &&
            tag < iplug::kMIDICCParamStartIdx + VST3_NUM_CC_CHANS * Steinberg::Vst::kCountCtrlNumber &&
            (tag - iplug::kMIDICCParamStartIdx) % Steinberg::Vst::kCountCtrlNumber < 128;
    }
    struct SourceQueue
    {
        Steinberg::Vst::IParamValueQueue* queue = nullptr;
        Steinberg::int32 first = 0;
        Steinberg::int32 count = 0;
    };
    LXR& plug;
    std::array<SourceQueue, kMaxSources> sources {};
    Steinberg::int32 sourceCount = 0;
    Steinberg::int32 flattenedCount = 0;
    MidiValueQueue midiQueue;
    RouteValueQueue routeQueue;
    TransportValueQueue transportQueue;
};

class BoundedMidiEvents final : public Steinberg::Vst::IEventList
{
public:
    BoundedMidiEvents(Steinberg::Vst::IEventList* events,
        std::atomic<uint64_t>& rejected, std::atomic<uint64_t>& malformed)
        : source(events)
    {
        if (source == nullptr) return;
        const auto reported = source->getEventCount();
        if (reported < 0) malformed.fetch_add(1, std::memory_order_relaxed);
        else
        {
            count = std::min(reported, MidiParameterChanges::kMaxCCPoints);
            if (reported > count)
                rejected.fetch_add(static_cast<uint64_t>(reported - count), std::memory_order_relaxed);
        }
    }
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** object) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::Vst::IEventList::iid) ||
            Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid))
        { *object = static_cast<Steinberg::Vst::IEventList*>(this); return Steinberg::kResultOk; }
        *object = nullptr; return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
    Steinberg::int32 PLUGIN_API getEventCount() override { return count; }
    Steinberg::tresult PLUGIN_API getEvent(Steinberg::int32 index, Steinberg::Vst::Event& event) override
    {
        return source != nullptr && index >= 0 && index < count
            ? source->getEvent(index, event) : Steinberg::kResultFalse;
    }
    Steinberg::tresult PLUGIN_API addEvent(Steinberg::Vst::Event&) override { return Steinberg::kNotImplemented; }
private:
    Steinberg::Vst::IEventList* source = nullptr;
    Steinberg::int32 count = 0;
};
#endif

// ---------------------------------------------------------
// Convert host parameters to native LXR 7-bit values.
// ---------------------------------------------------------

uint8_t GetRawLXRParameterValue(
    const iplug::IParam* param
)
{
    if (param == nullptr)
    {
        return 0;
    }

    int value = param->Int();

    if (value < 0)
    {
        value = 0;
    }
    else if (value > 127)
    {
        value = 127;
    }

    return static_cast<uint8_t>(value);
}

// ---------------------------------------------------------
// Clear every connected host output channel.
//
// Used when native assets are unavailable or sample-rate
// conversion has not been configured successfully.
// ---------------------------------------------------------

void ClearAudioOutputs(
    double** outputs,
    int nChans,
    int nFrames
)
{
    if (outputs == nullptr ||
        nChans <= 0 ||
        nFrames <= 0)
    {
        return;
    }

    for (int channel = 0;
         channel < nChans;
         ++channel)
    {
        double* output = outputs[channel];

        if (output == nullptr)
        {
            continue;
        }

        for (int sample = 0;
             sample < nFrames;
             ++sample)
        {
            output[sample] = 0.0;
        }
    }
}

bool SameRoute(const lxr::PersistentRouteRecord& a,
               const lxr::PersistentRouteRecord& b) noexcept
{
    return a.owner == b.owner && a.source == b.source &&
           a.flags == b.flags && a.identityNamespace == b.identityNamespace &&
           a.originalIndex == b.originalIndex &&
           a.firmwareParameter == b.firmwareParameter &&
           a.valueType == b.valueType && a.scope == b.scope &&
           a.physicalKey == b.physicalKey &&
           a.menuVoiceHint == b.menuVoiceHint && a.reserved == b.reserved;
}

const char* RouteSemanticName(lxr::modulation_contract::ValueSemantic semantic) noexcept
{
    using S = lxr::modulation_contract::ValueSemantic;
    switch (semantic)
    {
        case S::FrequencyMultiplier: return "pitch";
        case S::CutoffCyclesPerSample: return "filter cutoff";
        case S::Resonance: return "filter resonance";
        case S::EnvelopeStep: return "envelope";
        case S::SignedSlopeCoefficient: return "envelope slope";
        case S::PitchEnvelopeAmount: return "pitch envelope";
        case S::Pan: return "pan";
        case S::DistortionCoefficient: return "distortion";
        case S::FilterDrive: return "filter drive";
        case S::DecimationRate: return "decimation";
        case S::OscillatorWaveform: return "oscillator waveform";
        case S::FilterEnumeration: return "filter type";
        case S::TransientWaveform: return "transient waveform";
        case S::TransientPitch: return "transient pitch";
        case S::UnitFloat: return "volume/FM amount";
        default: return "typed target";
    }
}

void FormatRouteHostValue(std::size_t routeSlot, double value,
                          WDL_String& display)
{
    const auto id = lxr::route_host::parameterForSlot(routeSlot);
    const auto token = lxr::route_host::RouteToken16::fromRaw(
        value >= 0.0 && value <= lxr::route_host::kMaximumToken
            ? static_cast<std::uint32_t>(std::round(value))
            : lxr::route_host::kMaximumToken);
    const auto decoded = lxr::route_host::decode(id, token);
    if (decoded.status == lxr::route_host::TokenStatus::Off)
    {
        display.Set("Off");
        return;
    }
    if (decoded.status == lxr::route_host::TokenStatus::Reserved)
    {
        display.SetFormatted(64, "Reserved 0x%04X", token.value());
        return;
    }
    const auto* target = lxr::modulation_contract::lookup(
        lxr::modulation_contract::OriginalTargetIndex::fromRaw(token.value()));
    if (decoded.status != lxr::route_host::TokenStatus::Accepted || target == nullptr)
    {
        display.SetFormatted(96, "Unavailable: AVR %u %s", token.value(),
            target != nullptr ? target->symbol : "invalid");
        return;
    }
    display.SetFormatted(96, "AVR %u: %s", token.value(),
                         RouteSemanticName(target->semantic));
}

} // namespace

#if defined(VST3_API)
namespace lxr::vst
{
DEF_CLASS_IID(IKitManagement)
DEF_CLASS_IID(IMorphManagement)
}

Steinberg::tresult PLUGIN_API LXR::queryInterface(
    const Steinberg::TUID iid, void** object)
{
    if (object == nullptr)
        return Steinberg::kInvalidArgument;
    if (Steinberg::FUnknownPrivate::iidEqual(
            iid, lxr::vst::IKitManagement::iid))
    {
        *object = static_cast<lxr::vst::IKitManagement*>(this);
        addRef();
        return Steinberg::kResultOk;
    }
    if (Steinberg::FUnknownPrivate::iidEqual(
            iid, lxr::vst::IMorphManagement::iid))
    {
        *object = static_cast<lxr::vst::IMorphManagement*>(this);
        addRef();
        return Steinberg::kResultOk;
    }
    return iplug::Plugin::queryInterface(iid, object);
}

bool LXR::CopyKitOperationPath(
    const Steinberg::Vst::TChar* source,
    Steinberg::uint32 characters,
    std::array<wchar_t, lxr::kit::snd::kPathCapacity>& destination) noexcept
{
    static_assert(sizeof(Steinberg::Vst::TChar) == sizeof(wchar_t),
                  "Windows VST3 paths require matching 16-bit code units");
    destination.fill(0);
    if (source == nullptr || characters == 0u ||
        characters >= destination.size())
        return false;
    for (Steinberg::uint32 i = 0; i < characters; ++i)
    {
        if (source[i] == 0)
            return false;
        destination[i] = static_cast<wchar_t>(source[i]);
    }
    return true;
}

void LXR::CopyKitOperationWire(
    const lxr::kit::management::OperationResult& source,
    lxr::vst::KitOperationWire& destination) noexcept
{
    destination = {};
    destination.action = static_cast<Steinberg::uint32>(source.action);
    destination.status = static_cast<Steinberg::uint32>(source.status);
    destination.fileError = static_cast<Steinberg::uint32>(source.fileError);
    destination.codecError = static_cast<Steinberg::uint32>(source.codecError);
    destination.issue = static_cast<Steinberg::uint32>(source.issue);
    destination.publicationStatus =
        static_cast<Steinberg::uint32>(source.publicationStatus);
    destination.field = source.field;
    destination.route = source.route;
    destination.nativeError = source.nativeError;
    destination.published = source.published ? 1u : 0u;
    for (std::size_t i = 0; i < source.name.size(); ++i)
        destination.name[i] = source.name[i];
}

void LXR::CopyMorphOperationWire(
    const lxr::morph::management::OperationResult& source,
    lxr::vst::MorphOperationWire& destination) noexcept
{
    destination = {};
    destination.action = static_cast<Steinberg::uint32>(source.action);
    destination.status = static_cast<Steinberg::uint32>(source.status);
    destination.fileError = static_cast<Steinberg::uint32>(source.fileError);
    destination.codecError = static_cast<Steinberg::uint32>(source.codecError);
    destination.issue = static_cast<Steinberg::uint32>(source.issue);
    destination.publicationStatus =
        static_cast<Steinberg::uint32>(source.publicationStatus);
    destination.field = source.field;
    destination.route = source.route;
    destination.nativeError = source.nativeError;
    destination.published = source.published ? 1u : 0u;
    destination.targetPresent = source.targetPresent ? 1u : 0u;
    destination.amount = source.amount;
    destination.generation = source.generation;
    for (std::size_t i = 0; i < source.name.size(); ++i)
        destination.name[i] = source.name[i];
}

Steinberg::tresult PLUGIN_API LXR::importSnd(
    const Steinberg::Vst::TChar* path,
    Steinberg::uint32 pathCharacters,
    lxr::vst::KitOperationWire* result)
{
    if (result == nullptr)
        return Steinberg::kInvalidArgument;
    std::array<wchar_t, lxr::kit::snd::kPathCapacity> nativePath;
    if (!CopyKitOperationPath(path, pathCharacters, nativePath))
        return Steinberg::kInvalidArgument;
    CopyKitOperationWire(ImportKitFile(nativePath.data()), *result);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API LXR::exportSnd(
    const Steinberg::Vst::TChar* path,
    Steinberg::uint32 pathCharacters,
    lxr::vst::KitOperationWire* result)
{
    if (result == nullptr)
        return Steinberg::kInvalidArgument;
    std::array<wchar_t, lxr::kit::snd::kPathCapacity> nativePath;
    if (!CopyKitOperationPath(path, pathCharacters, nativePath))
        return Steinberg::kInvalidArgument;
    CopyKitOperationWire(ExportKitFile(nativePath.data()), *result);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API LXR::getKitOperation(
    lxr::vst::KitOperationWire* result)
{
    if (result == nullptr)
        return Steinberg::kInvalidArgument;
    CopyKitOperationWire(mLastKitOperation, *result);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API LXR::importMorphSnd(
    const Steinberg::Vst::TChar* path,
    Steinberg::uint32 pathCharacters,
    lxr::vst::MorphOperationWire* result)
{
    if (result == nullptr)
        return Steinberg::kInvalidArgument;
    std::array<wchar_t, lxr::kit::snd::kPathCapacity> nativePath;
    if (!CopyKitOperationPath(path, pathCharacters, nativePath))
        return Steinberg::kInvalidArgument;
    CopyMorphOperationWire(LoadMorphTargetFile(nativePath.data()), *result);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API LXR::clearMorphTarget(
    lxr::vst::MorphOperationWire* result)
{
    if (result == nullptr)
        return Steinberg::kInvalidArgument;
    CopyMorphOperationWire(ClearMorphTarget(), *result);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API LXR::exportMorphSnd(
    const Steinberg::Vst::TChar* path,
    Steinberg::uint32 pathCharacters,
    lxr::vst::MorphOperationWire* result)
{
    if (result == nullptr)
        return Steinberg::kInvalidArgument;
    std::array<wchar_t, lxr::kit::snd::kPathCapacity> nativePath;
    if (!CopyKitOperationPath(path, pathCharacters, nativePath))
        return Steinberg::kInvalidArgument;
    CopyMorphOperationWire(ExportMorphedKitFile(nativePath.data()), *result);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API LXR::cancelMorphOperation(
    Steinberg::uint32 action,
    lxr::vst::MorphOperationWire* result)
{
    using lxr::morph::management::Action;
    if (result == nullptr ||
        action > static_cast<Steinberg::uint32>(Action::ExportMorphed))
        return Steinberg::kInvalidArgument;
    CopyMorphOperationWire(CancelMorphOperation(
        static_cast<Action>(action)), *result);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API LXR::getMorphState(
    lxr::vst::MorphOperationWire* result)
{
    if (result == nullptr)
        return Steinberg::kInvalidArgument;
    auto current = mLastMorphOperation;
    lxr::morph::Snapshot snapshot;
    if (!CurrentMorphState(snapshot))
    {
        current.status = lxr::morph::management::Status::PublicationBusy;
        current.publicationStatus = lxr::morph::PublicationStatus::Busy;
        CopyMorphOperationWire(current, *result);
        return Steinberg::kResultOk;
    }
    current.targetPresent = snapshot.targetPresent;
    current.amount = snapshot.amount;
    current.generation = snapshot.generation;
    current.name = snapshot.targetPresent
        ? snapshot.target.name : std::array<std::uint8_t, 8>{};
    CopyMorphOperationWire(current, *result);
    return Steinberg::kResultOk;
}
#endif

// ---------------------------------------------------------
// Constructor
// ---------------------------------------------------------

LXR::LXR(
    const iplug::InstanceInfo& info
)
    : iplug::Plugin(
          info,
          iplug::MakeConfig(
              lxr::vst::kNumParams,
              lxr::vst::kNumPresets
          )
      )
{
#if IPLUG_EDITOR
    mMakeGraphicsFunc = [&]() {
        return iplug::igraphics::MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS);
    };
    mLayoutFunc = [&](iplug::igraphics::IGraphics* graphics) {
        using namespace iplug::igraphics;
        if (graphics->NControls() != 0) return;
        graphics->AttachPanelBackground(kLXRBackground);
        graphics->LoadFont("LXRSystem", "Segoe UI", ETextStyle::Normal);
        const IText title(25.f, kLXRText, "LXRSystem", EAlign::Near);
        const IText text(15.f, kLXRText, "LXRSystem", EAlign::Near);
        const IText centred(14.f, kLXRText, "LXRSystem");
        const IText muted(13.f, kLXRMuted, "LXRSystem", EAlign::Near);
        const IVStyle buttonStyle = DEFAULT_STYLE
            .WithColor(kBG, kLXRPanelLight).WithColor(kFG, kLXRText)
            .WithColor(kPR, kLXRAccent).WithLabelText(centred)
            .WithValueText(centred);
        const IVStyle controlStyle = DEFAULT_STYLE
            .WithColor(kBG, kLXRPanel).WithColor(kFG, kLXRText)
            .WithColor(kPR, kLXRAccent).WithLabelText(centred)
            .WithValueText(centred);

        graphics->AttachControl(new ITextControl(IRECT(24, 14, 220, 50),
            "LXR 0.37", title, kLXRBackground));
        graphics->AttachControl(new ITextControl(IRECT(230, 13, 550, 35),
            "Current kit: <unnamed>", text, kLXRBackground), kKitNameControlTag);
        graphics->AttachControl(new ITextControl(IRECT(230, 37, 430, 61),
            "Voice: Drum 1", text, kLXRBackground), kSelectedVoiceControlTag);
        graphics->AttachControl(new ITextControl(IRECT(445, 37, 610, 61),
            "Pattern: 1", text, kLXRBackground), kPatternControlTag);
        graphics->AttachControl(new ITextControl(IRECT(625, 13, 800, 35),
            "Clock: Internal", text, kLXRBackground), kClockControlTag);
        graphics->AttachControl(new ITextControl(IRECT(625, 37, 800, 61),
            "BPM: 120", text, kLXRBackground), kTempoControlTag);
        graphics->AttachControl(new ITextControl(IRECT(815, 13, 980, 35),
            "Host: unavailable", text, kLXRBackground), kTransportControlTag);
        graphics->AttachControl(new ITextControl(IRECT(815, 37, 980, 61),
            "Sequencer: OFF", text, kLXRBackground), kSequencerStatusControlTag);
        graphics->AttachControl(new ITextControl(IRECT(995, 13, 1255, 35),
            "Morph Amount: 0", text, kLXRBackground), kHeaderMorphControlTag);
        graphics->AttachControl(new ITextControl(IRECT(995, 37, 1255, 61),
            "Operation: Ready", muted, kLXRBackground), kOperationStatusControlTag);

        for (std::size_t i = 0; i < lxr::gui::kViewNames.size(); ++i)
        {
            const float left = 24.f + static_cast<float>(i) * 176.f;
            graphics->AttachControl(new IVButtonControl(
                IRECT(left, 76, left + 164.f, 116),
                [this, i](IControl*) {
                    SelectGUIView(static_cast<lxr::gui::PrimaryView>(i));
                }, lxr::gui::kViewNames[i], buttonStyle),
                kNavControlTag + static_cast<int>(i));
        }
        graphics->AttachControl(new ITextControl(IRECT(930, 76, 1255, 116),
            "Active view: Overview", centred, kLXRPanel), kActiveViewControlTag);

        graphics->AttachControl(new ITextControl(IRECT(24, 132, 165, 169),
            "SYNTH OWNER", muted, kLXRBackground));
        for (std::size_t i = 0; i < lxr::gui::kVoiceNames.size(); ++i)
        {
            const float left = 170.f + static_cast<float>(i) * 96.f;
            graphics->AttachControl(new IVButtonControl(
                IRECT(left, 128, left + 86.f, 171),
                [this, i](IControl*) {
                    SelectGUIVoice(static_cast<lxr::gui::VoiceOwner>(i));
                }, lxr::gui::kVoiceShortNames[i], buttonStyle),
                kVoiceControlTag + static_cast<int>(i));
        }
        graphics->AttachControl(new ITextControl(IRECT(770, 128, 1255, 171),
            "Tab moves focus; arrows adjust; Enter activates navigation.",
            muted, kLXRBackground), kKeyboardStatusControlTag);

        // Overview contains only global editable controls. Voice-owned levels
        // have their sole editable attachment in the corresponding Sound
        // editor, preventing duplicate host gestures and competing focus.
        constexpr const char* overview = "lxr.view.overview";
        graphics->AttachControl(new ITextControl(IRECT(35, 195, 850, 235),
            "OVERVIEW  /  GLOBAL STATUS", title, kLXRBackground), kNoTag, overview);
        graphics->AttachControl(new ITextControl(IRECT(38, 248, 800, 286),
            "Voice levels and all source-owned synthesis fields are edited in Sound.",
            text, kLXRPanel), kNoTag, overview);
        graphics->AttachControl(new IVKnobControl(IRECT(840, 250, 955, 405),
            114, "Global Decimation", controlStyle, true, true),
            kParameterControlTag + 114, overview);
        graphics->AttachControl(new IVToggleControl(IRECT(980, 250, 1105, 325),
            240, "Sequencer Enable", controlStyle),
            kParameterControlTag + 240, overview);
        graphics->AttachControl(new IVToggleControl(IRECT(1120, 250, 1245, 325),
            241, "Clock: Internal / Host", controlStyle, "INTERNAL", "HOST"),
            kParameterControlTag + 241, overview);
        graphics->AttachControl(new IVKnobControl(IRECT(980, 345, 1105, 480),
            242, "Internal BPM", controlStyle, true, true),
            kParameterControlTag + 242, overview);
        graphics->AttachControl(new ITextControl(IRECT(38, 448, 800, 490),
            "Pattern, transport and clock status above are coherent readback; host transport is not editable here.",
            muted, kLXRBackground), kNoTag, overview);
        graphics->AttachControl(new IVButtonControl(IRECT(38, 515, 285, 565),
            [this](IControl*) { SelectGUIView(lxr::gui::PrimaryView::KitsMorph); },
            "Kit Import / Export  >", buttonStyle), kNoTag, overview);
        graphics->AttachControl(new IVButtonControl(IRECT(305, 515, 552, 565),
            [this](IControl*) { SelectGUIView(lxr::gui::PrimaryView::KitsMorph); },
            "Morph Target  >", buttonStyle), kNoTag, overview);
        graphics->AttachControl(new ITextControl(IRECT(38, 590, 1240, 650),
            "Core production controls only. Full voice editor, meters, matrix and sequencer grid are deferred.",
            text, kLXRPanel), kNoTag, overview);

        for (std::size_t owner = 0; owner < 6u; ++owner)
        {
            const auto ownerId = static_cast<lxr::gui::VoiceOwner>(owner);
            for (std::size_t section = 0;
                 section < lxr::gui::kSoundSectionBounds.size(); ++section)
            {
                const auto bounds = lxr::gui::kSoundSectionBounds[section];
                graphics->AttachControl(new ITextControl(
                    IRECT(static_cast<float>(bounds.left),
                          static_cast<float>(bounds.top),
                          static_cast<float>(bounds.right),
                          static_cast<float>(bounds.bottom)),
                    lxr::gui::kSoundSectionNames[section],
                    IText(12.f, kLXRMuted, "LXRSystem", EAlign::Near,
                          EVAlign::Top), kLXRPanel),
                    kNoTag, kSoundGroups[owner]);
            }
            for (const auto& binding : lxr::gui::kSoundBindings)
            {
                if (binding.owner != ownerId) continue;
                const auto bounds = lxr::gui::soundControlBounds(binding);
                graphics->AttachControl(new LXRSoundControl(
                    IRECT(static_cast<float>(bounds.left),
                          static_cast<float>(bounds.top),
                          static_cast<float>(bounds.right),
                          static_cast<float>(bounds.bottom)), binding),
                    kParameterControlTag + binding.parameter,
                    kSoundGroups[owner]);
            }
            graphics->AttachControl(new ITextControl(IRECT(24, 690, 1256, 718),
                ownerId == lxr::gui::VoiceOwner::HiHat
                    ? "Shared hi-hat engine: closed/open are trigger articulations, not separate synthesis owners."
                    : "Exact byte-domain values; Enter permits text entry; Shift-drag/wheel is fine adjustment.",
                muted, kLXRBackground), kNoTag, kSoundGroups[owner]);

            graphics->AttachControl(new ITextControl(IRECT(35, 195, 1000, 235),
                "MODULATION  /  VELOCITY + LFO", title, kLXRBackground),
                kNoTag, kModulationGroups[owner]);
            graphics->AttachControl(new ITextControl(IRECT(40, 240, 430, 385),
                "VELOCITY", IText(12.f, kLXRMuted, "LXRSystem",
                EAlign::Near, EVAlign::Top), kLXRPanel),
                kNoTag, kModulationGroups[owner]);
            graphics->AttachControl(new ITextControl(IRECT(440, 240, 1240, 530),
                "LFO", IText(12.f, kLXRMuted, "LXRSystem",
                EAlign::Near, EVAlign::Top), kLXRPanel),
                kNoTag, kModulationGroups[owner]);
            for (const auto& binding : lxr::gui::kModulationBindings)
            {
                if (binding.owner != ownerId) continue;
                const auto bounds = lxr::gui::modulationControlBounds(binding.role);
                graphics->AttachControl(new LXRModulationControl(
                    IRECT(static_cast<float>(bounds.left),
                          static_cast<float>(bounds.top),
                          static_cast<float>(bounds.right),
                          static_cast<float>(bounds.bottom)), binding),
                    kParameterControlTag + binding.parameter,
                    kModulationGroups[owner]);
            }
            graphics->AttachControl(new ITextControl(IRECT(55, 550, 1230, 610),
                "ROUTING STATUS  Accepted identities are shown verbatim. OFF is identity 0. Invalid, reserved and cross-owner identities are rejected and the last accepted route remains authoritative.",
                text, kLXRPanel), kNoTag, kModulationGroups[owner]);
            graphics->AttachControl(new ITextControl(IRECT(55, 630, 1230, 680),
                "Rate, amount and phase use the original byte domain. Wave, retrigger and sync menus preserve their stored firmware identities; no frequency conversion is implied.",
                muted, kLXRBackground), kNoTag, kModulationGroups[owner]);
        }

        constexpr const char* sequencer = "lxr.view.sequencer";
        graphics->AttachControl(new ITextControl(IRECT(35, 195, 900, 235),
            "SEQUENCER  /  STATUS", title, kLXRBackground), kNoTag, sequencer);
        graphics->AttachControl(new ITextControl(IRECT(60, 270, 1220, 430),
            "Current pattern, sequencer enable, clock mode, valid BPM and playback status are shown in the persistent header.\n\nThe seven-track sequencer grid is not implemented in P19-B.",
            text, kLXRPanel), kNoTag, sequencer);

        constexpr const char* kits = "lxr.view.kits-morph";
        graphics->AttachControl(new ITextControl(IRECT(35, 195, 900, 235),
            "KITS + MORPH  /  PRODUCTION OPERATIONS", title, kLXRBackground),
            kNoTag, kits);
        graphics->AttachControl(new ITextControl(IRECT(55, 242, 620, 275),
            "Current kit: <unnamed> (identity present)", text, kLXRBackground),
            kKitIdentityDetailTag, kits);
        graphics->AttachControl(new ITextControl(IRECT(55, 275, 620, 335),
            "Ready. Import and export run outside audio processing.", text,
            kLXRBackground), kKitStatusControlTag, kits);
        graphics->AttachControl(new IVButtonControl(IRECT(55, 350, 300, 402),
            [this, graphics](IControl*) {
                WDL_String file, directory;
                graphics->PromptForFile(file, directory, EFileAction::Open, "snd",
                    [this](const WDL_String& selected, const WDL_String&) {
                        if (selected.GetLength() == 0)
                        {
                            mLastKitOperation = {};
                            mLastKitOperation.action =
                                lxr::kit::management::Action::Import;
                            mLastKitOperation.status =
                                lxr::kit::management::Status::Cancelled;
                            SetLastKitOperationPath(nullptr);
                        }
                        else
                        {
                            const iplug::UTF8AsUTF16 wide(selected);
                            (void) ImportKitFile(wide.Get());
                            SetLastKitOperationPath(selected.Get());
                        }
                        mLatestGUIOperationKind = kKitGUIOperation;
                        RefreshKitManagementControls();
                    });
            }, "Import .snd kit", buttonStyle), kImportKitOperationTag, kits);
        graphics->AttachControl(new IVButtonControl(IRECT(320, 350, 565, 402),
            [this, graphics](IControl*) {
                WDL_String file, directory;
                graphics->PromptForFile(file, directory, EFileAction::Save, "snd",
                    [this](const WDL_String& selected, const WDL_String&) {
                        if (selected.GetLength() == 0)
                        {
                            mLastKitOperation = {};
                            mLastKitOperation.action =
                                lxr::kit::management::Action::Export;
                            mLastKitOperation.status =
                                lxr::kit::management::Status::Cancelled;
                            SetLastKitOperationPath(nullptr);
                        }
                        else
                        {
                            const iplug::UTF8AsUTF16 wide(selected);
                            (void) ExportKitFile(wide.Get());
                            SetLastKitOperationPath(selected.Get());
                        }
                        mLatestGUIOperationKind = kKitGUIOperation;
                        RefreshKitManagementControls();
                    });
            }, "Export .snd kit", buttonStyle), kExportKitOperationTag, kits);
        graphics->AttachControl(new ITextControl(IRECT(55, 430, 620, 530),
            "HOST PRESETS\nStandard VST3 preset save/load is provided through the host. The plugin does not own the host preset filesystem and includes no fabricated factory preset browser.",
            muted, kLXRPanel), kNoTag, kits);
        graphics->AttachControl(new ITextControl(IRECT(650, 242, 1215, 275),
            "Morph target: <none>", text, kLXRBackground),
            kMorphTargetControlTag, kits);
        graphics->AttachControl(new IVButtonControl(IRECT(650, 290, 895, 342),
            [this, graphics](IControl*) {
                WDL_String file, directory;
                graphics->PromptForFile(file, directory, EFileAction::Open, "snd",
                    [this](const WDL_String& selected, const WDL_String&) {
                        if (selected.GetLength() == 0)
                        {
                            (void) CancelMorphOperation(
                                lxr::morph::management::Action::LoadTarget);
                            SetLastMorphOperationPath(nullptr);
                        }
                        else
                        {
                            const iplug::UTF8AsUTF16 wide(selected);
                            (void) LoadMorphTargetFile(wide.Get());
                            SetLastMorphOperationPath(selected.Get());
                        }
                        mLatestGUIOperationKind = kMorphGUIOperation;
                        RefreshMorphManagementControls();
                    });
            }, "Load Morph Target", buttonStyle), kLoadMorphOperationTag, kits);
        graphics->AttachControl(new IVButtonControl(IRECT(915, 290, 1160, 342),
            [this](IControl*) {
                (void) ClearMorphTarget();
                SetLastMorphOperationPath(nullptr);
                mLatestGUIOperationKind = kMorphGUIOperation;
                RefreshMorphManagementControls();
            }, "Clear Morph Target", buttonStyle), kClearMorphOperationTag, kits);
        graphics->AttachControl(new IVKnobControl(IRECT(650, 370, 800, 510),
            245, "Morph Amount (0-255)", controlStyle, true, true),
            kParameterControlTag + 245, kits);
        graphics->AttachControl(new IVButtonControl(IRECT(825, 385, 1160, 440),
            [this, graphics](IControl*) {
                WDL_String file, directory;
                graphics->PromptForFile(file, directory, EFileAction::Save, "snd",
                    [this](const WDL_String& selected, const WDL_String&) {
                        if (selected.GetLength() == 0)
                        {
                            (void) CancelMorphOperation(
                                lxr::morph::management::Action::ExportMorphed);
                            SetLastMorphOperationPath(nullptr);
                        }
                        else
                        {
                            const iplug::UTF8AsUTF16 wide(selected);
                            (void) ExportMorphedKitFile(wide.Get());
                            SetLastMorphOperationPath(selected.Get());
                        }
                        mLatestGUIOperationKind = kMorphGUIOperation;
                        RefreshMorphManagementControls();
                    });
            }, "Export Morphed Kit (.snd)", buttonStyle),
            kExportMorphOperationTag, kits);
        graphics->AttachControl(new ITextControl(IRECT(650, 525, 1215, 600),
            "Ready. Load, clear and export run outside audio processing.",
            text, kLXRBackground), kMorphStatusControlTag, kits);
        graphics->AttachControl(new ITextControl(IRECT(55, 620, 1215, 675),
            "Morph Amount is host parameter 245. Amount 255 preserves original firmware arithmetic and is not forced to equal the target exactly. Morph target state remains transient.",
            muted, kLXRBackground), kNoTag, kits);

        graphics->AttachControl(new ITextControl(IRECT(24, 728, 1255, 755),
            "Operation status: Ready", muted, kLXRBackground),
            kOperationStatusControlTag + 100);
        graphics->AttachControl(new ITextControl(IRECT(24, 760, 1255, 790),
            "Development GUI foundation — fixed 1280 x 800 — no host resize",
            muted, kLXRBackground));
        graphics->AttachControl(new LXRFocusRing(), kFocusRingControlTag);
        graphics->SetKeyHandlerFunc([this](const iplug::IKeyPress& key, bool isUp) {
            return HandleGUIKey(key, isUp);
        });
        SelectGUIView(lxr::gui::PrimaryView::Overview);
        SelectGUIVoice(lxr::gui::VoiceOwner::Drum1);
        RefreshKitManagementControls();
        RefreshMorphManagementControls();
        RefreshGUIReadback();
    };
#endif
    // Preserve the native parameter contract.
    static_assert(
        lxr::vst::kNativeNumParams == lxr::kNumParams,
        "Native VST and DSP parameter counts must match"
    );

    static_assert(
        lxr::vst::kNativeNumParams == 228 &&
        lxr::vst::kPersistentNumParams == 240 &&
        lxr::vst::kNumParams == 246,
        "Unexpected native/host parameter counts"
    );

    // Register each native LXR parameter.
    for (int paramIdx = 0;
         paramIdx < lxr::vst::kNativeNumParams;
         ++paramIdx)
    {
        const auto& descriptor =
            lxr::vst::kParamDescriptors[
                static_cast<std::size_t>(paramIdx)
            ];

        auto* param = GetParam(paramIdx);

        assert(param != nullptr);

        if (param == nullptr)
        {
            continue;
        }

        param->InitInt(
            descriptor.name,
            descriptor.defaultValue,
            descriptor.minValue,
            descriptor.maxValue
        );
    }

    // These twelve integer parameters are host projections of mRoutingState.
    // They are not appended to ParameterArray or the 228-byte patch payload.
    for (std::size_t routeSlot = 0;
         routeSlot < lxr::route_host::kDescriptors.size(); ++routeSlot)
    {
        const auto& descriptor = lxr::route_host::kDescriptors[routeSlot];
        auto* param = GetParam(static_cast<int>(descriptor.id.value()));
        assert(param != nullptr);
        if (param == nullptr)
            continue;
        param->InitInt(descriptor.name, 0, 0,
                       static_cast<int>(lxr::route_host::kMaximumToken));
        param->SetDisplayFunc([routeSlot](double value, WDL_String& display) {
            FormatRouteHostValue(routeSlot, value, display);
        });
    }

    // Minimal P16-F1 transport surface. These controls intentionally do not
    // enter LXRS v2; a newly opened project therefore defaults sequencing off.
    GetParam(lxr::vst::kSequencerEnableParam)->InitInt(
        "Sequencer Enable", 0, 0, 1);
    GetParam(lxr::vst::kSequencerClockModeParam)->InitInt(
        "Sequencer Clock", 0, 0, 1);
    GetParam(lxr::vst::kSequencerClockModeParam)->SetDisplayFunc(
        [](double value, WDL_String& display) {
            display.Set(value >= 0.5 ? "Host Sync" : "Internal");
        });
    GetParam(lxr::vst::kSequencerInternalTempoParam)->InitInt(
        "Sequencer Internal BPM", 120, 1, 255, "BPM");

    GetParam(lxr::vst::kSequencerRollRateParam)->InitInt(
        "Sequencer Roll Rate", 8, 0, 13);
    GetParam(lxr::vst::kSequencerRollRateParam)->SetDisplayFunc(
        [](double value, WDL_String& display) {
            static const char* names[] {"One-shot", "1/1", "1/2", "1/3", "1/4",
                "1/6", "1/8", "1/12", "1/16", "1/24", "1/32", "1/48", "1/64", "1/128"};
            const auto index = static_cast<unsigned>(value);
            display.Set(index < 14u ? names[index] : "Invalid");
        });
    GetParam(lxr::vst::kSequencerRollTracksParam)->InitInt(
        "Sequencer Roll Tracks", 0, 0, 127);
    GetParam(lxr::vst::kSequencerRollTracksParam)->SetDisplayFunc(
        [](double value, WDL_String& display) {
            const auto mask = static_cast<unsigned>(value);
            static const char* names[] {"D1 ", "D2 ", "D3 ", "SN ", "CY ", "CH ", "OH "};
            display.Set(mask == 0u ? "Released" : "Hold: ");
            for (unsigned track = 0; track < 7u; ++track)
                if ((mask & (1u << track)) != 0u) display.Append(names[track]);
        });

    // Original performance amount is an unsigned byte. Target B is loaded
    // separately and remains transient; this automatable value is likewise
    // deliberately absent from LXRS serialization in P18-B.
    GetParam(lxr::vst::kMorphAmountParam)->InitInt(
        "Morph Amount", 0, 0, 255);

#if defined(VST3_API)
    // VST3 3.7 hosts may omit all optional context unless the component asks.
    processContextRequirements.needProjectTimeMusic()
        .needBarPositionMusic().needCycleMusic().needTempo()
        .needTimeSignature().needTransportState();
#endif

    // Initialize canonical native parameters.
    mParams.init();

    StateAuthority::Values initialState {};
    for (int paramIdx = 0; paramIdx < lxr::vst::kNativeNumParams; ++paramIdx)
    {
        const std::size_t index = static_cast<std::size_t>(paramIdx);
        initialState[index] = mParams.get(static_cast<uint8_t>(paramIdx));
        mDeferredHostValues[index].store(0, std::memory_order_relaxed);
    }
    for (std::size_t routeSlot = 0;
         routeSlot < lxr::route_host::kParameterCount; ++routeSlot)
    {
        mDeferredRouteValues[routeSlot].store(0, std::memory_order_relaxed);
        mDeferredRouteRejections[routeSlot].store(0, std::memory_order_relaxed);
    }
    mState.initialize(initialState);
    mRoutingState.initialize();
    mMorphState.initialize();
    mDeferredMorphAmount.store(0u, std::memory_order_relaxed);
    mAppliedMorphProjection = initialState;
    mAppliedRouting = lxr::MakeDisabledRoutingState();
    (void)mSequencer.patternAuthority().tryAcquire(mAppliedPattern);
    mSequencer.useProjectSnapshot(&mAppliedPattern);

    // Clear native cache and resampler state.
    // Host-rate configuration occurs in OnReset().
    mAudioAdapter.reset();
    mSequencer.reset(0u);
    mAppliedSequencerEnabled = false;

    // Resolve assets relative to the plugin module,
    // never relative to the DAW working directory.
    const std::string assetDirectory =
        lxr::GetPluginAssetDirectory();

    // Load original native oscillator/sample assets.
    // Rendering remains disabled if loading fails.
    mAssetsLoaded =
        !assetDirectory.empty() &&
        mEngine.loadAssets(
            assetDirectory.c_str()
        );

    // Do not call Engine::processBlock() unless
    // asset loading succeeded.
    TRACE;
}

// ---------------------------------------------------------
// Restore the native Engine from current host parameters.
// ---------------------------------------------------------

bool LXR::TryPersistentSnapshot(
    StateAuthority::Values& parameters,
    uint64_t& parameterGeneration,
    lxr::RoutingStateAuthority::Snapshot& routing,
    lxr::sequencer::PatternStateAuthority::ReadHandle* pattern,
    lxr::sequencer::state::TransportConfig* transport,
    lxr::sequencer::RollHostControls* rolls) const
{
    lxr::RoutingStateAuthority::Guard guard;
    if (!mRoutingState.tryAcquire(guard))
        return false;
    if (!mState.trySnapshot(parameters, &parameterGeneration, 1))
        return false;
    if (pattern != nullptr && const_cast<LXR*>(this)->mSequencer.patternAuthority()
            .tryAcquire(*pattern, 1) != lxr::sequencer::Status::Ok)
        return false;
    if (transport != nullptr) *transport = mTransportConfig;
    if (rolls != nullptr) *rolls = mRollControls;
    if (!mRoutingState.copyLocked(guard, routing)) return false;
    // Capture bypass inside the same restore coordinator. A later full restore
    // must not pair its suffix with this snapshot's older patch/bank/config.
    routing.state.bypass = GetBypassed();
    return true;
}

bool LXR::ApplyRoutesToEngine(
    lxr::Engine& engine,
    const std::array<lxr::PersistentRouteRecord,
                     lxr::kPersistentRouteCount>& routes) noexcept
{
    using namespace lxr::modulation_contract;
    for (std::size_t i = 0; i < routes.size(); ++i)
    {
        const auto& route = routes[i];
        const SourceIdentity source {
            route.source == 0u ? SourceKind::Velocity : SourceKind::Lfo,
            voiceFromZeroBased(route.owner)
        };
        const auto target = OriginalTargetIndex::fromRaw(route.originalIndex);
        const auto status = route.source == 0u
            ? engine.configureVelocityDestination(source, target)
            : engine.configureLfoDestination(source, target);
        if (status != lxr::VelocityRouteStatus::Ok)
            return false;
    }
    return true;
}

bool LXR::PrepareCompleteRestore(
    const StateAuthority::Values& parameters,
    const lxr::PersistentRoutingState& routing)
{
    // A complete restore starts with fresh transient voice/RNG storage. Calling
    // init() on a copy of the playing Engine retains voice oscillator RNGs and
    // other fields that the firmware's startup-only initializers do not clear.
    // Asset views remain resident/non-owning; construction does no asset I/O.
    const auto assets = mEngine.tables;
    mStagingEngine = lxr::Engine{};
    mStagingEngine.tables = assets;
    mStagingParams.init();
    for (std::size_t i = 0; i < parameters.size(); ++i)
    {
        mStagingParams.set(static_cast<uint8_t>(i), parameters[i],
                           mStagingEngine);
    }
    if (!ApplyRoutesToEngine(mStagingEngine, routing.routes))
        return false;

    return true;
}

void LXR::CommitCompleteRestore(
    const lxr::PersistentRoutingState& routing) noexcept
{
    // Selection is the processing-boundary linearization point. No renderer or
    // event consumer runs between these fixed assignments.
    mEngine = mStagingEngine;
    mParams = mStagingParams;
    mAppliedRouting = routing;
    mAudioAdapter.reset();
    mSequencer.reset(0u);
    mAppliedSequencerEnabled = false;
    mAppliedRollMask = 0u;
    mMidiParser = lxr::MidiParser();
}

bool LXR::StageCompleteRestore(
    const StateAuthority::Values& parameters,
    const lxr::PersistentRoutingState& routing)
{
    if (!PrepareCompleteRestore(parameters, routing))
        return false;
    CommitCompleteRestore(routing);
    return true;
}

void LXR::ResetEngineState()
{
    StateAuthority::Values parameters {};
    uint64_t parameterGeneration = 0;
    lxr::RoutingStateAuthority::Snapshot routing;
    lxr::sequencer::PatternStateAuthority::ReadHandle pattern;
    lxr::sequencer::state::TransportConfig transport;
    lxr::sequencer::RollHostControls rolls;
    if (!TryPersistentSnapshot(parameters, parameterGeneration, routing, &pattern, &transport, &rolls))
        return;

    lxr::morph::Snapshot morph;
    lxr::morph::Values source;
    lxr::morph::Values projection = parameters;
    if (!mMorphState.trySnapshot(morph, 1u))
        return;
    if (morph.targetPresent)
    {
        if (!lxr::morph::BuildSourceValues(
                parameters, routing.state.routes, source))
            return;
        lxr::morph::BuildEffectiveProjection(
            source, morph.target.values, morph.amount, projection);
    }

    if (!StageCompleteRestore(projection, routing.state))
        return;
    mAppliedPattern.swap(pattern);
    mAudioTransport = transport;
    mAudioRollControls = rolls;
    mSequencer.resetSeed(transport.seed);
    (void)mSequencer.selectPattern(transport.pattern);

    mAppliedStateGeneration = parameterGeneration;
    mAppliedRestoreGeneration = routing.restoreEpoch;
    mAppliedJointGeneration = routing.generation;
    mAppliedMorphGeneration = morph.generation;
    mAppliedMorphProjection = projection;
    mAppliedMorphNativeBoundary = mAudioAdapter.nativeFrame();

    // ORIGINAL QUIRK:
    // Internal engine timing and native PCM processing
    // remain unchanged regardless of host sample rate.
}

// ---------------------------------------------------------
// Apply UI-thread state/parameter changes at a processing
// boundary without locking or allocating on the audio thread.
// ---------------------------------------------------------

bool LXR::FlushDeferredMorphAmount(uint64_t maximumOrder)
{
    uint64_t encoded = mDeferredMorphAmount.load(std::memory_order_acquire);
    if ((encoded & 0x100u) == 0u)
        return true;
    const uint64_t order = encoded >> 9u;
    const uint64_t cutoff =
        mRestoreOrderCutoff.load(std::memory_order_acquire);
    if (order > cutoff && order > maximumOrder)
        return true;

    lxr::morph::PublicationResult publication;
    if (order <= cutoff)
    {
        publication.status = lxr::morph::PublicationStatus::Stale;
    }
    else
    {
        publication = mMorphState.tryPublishAmount(
            static_cast<std::uint8_t>(encoded & 0xffu), order);
        if (publication.status == lxr::morph::PublicationStatus::Busy ||
            publication.status ==
                lxr::morph::PublicationStatus::GenerationExhausted)
            return false;
    }

    // Clear only the value just handled. A newer callback may already have
    // replaced it and must remain pending for the next boundary.
    (void)mDeferredMorphAmount.compare_exchange_strong(
        encoded, 0u, std::memory_order_acq_rel,
        std::memory_order_acquire);
    return true;
}

bool LXR::FlushDeferredHostParameters(uint64_t maximumOrder)
{
    bool pending = false;
    const uint64_t cutoff = mRestoreOrderCutoff.load(std::memory_order_acquire);
    for (const auto& value : mDeferredHostValues)
    {
        const uint64_t encoded = value.load(std::memory_order_acquire);
        const uint64_t order = encoded >> 9u;
        pending |= (encoded & 0x100u) != 0u &&
                   (order <= cutoff || order <= maximumOrder);
    }
    for (const auto& value : mDeferredRouteValues)
    {
        const uint64_t encoded = value.load(std::memory_order_acquire);
        const uint64_t order = encoded >> 17u;
        pending |= (encoded & UINT64_C(0x10000)) != 0u &&
                   (order <= cutoff || order <= maximumOrder);
    }
    for (const auto& value : mDeferredRouteRejections)
    {
        const uint64_t order = value.load(std::memory_order_acquire);
        pending |= order != 0u && (order <= cutoff || order <= maximumOrder);
    }
    for (const auto& value : mDeferredTransportValues)
    {
        const auto encoded = value.load(std::memory_order_acquire);
        const auto order = encoded >> 10u;
        pending |= (encoded & 0x200u) != 0u && (order <= cutoff || order <= maximumOrder);
    }
    for (const auto& value : mDeferredRollValues)
    {
        const auto encoded = value.load(std::memory_order_acquire);
        const auto order = encoded >> 9u;
        pending |= (encoded & 0x100u) != 0u && (order <= cutoff || order <= maximumOrder);
    }
    {
        const auto encoded =
            mDeferredMorphAmount.load(std::memory_order_acquire);
        const auto order = encoded >> 9u;
        pending |= (encoded & 0x100u) != 0u &&
                   (order <= cutoff || order <= maximumOrder);
    }
    pending |= mRollReleasePending.load(std::memory_order_acquire);
    if (!pending)
        return true;

    lxr::RoutingStateAuthority::Guard outer;
    if (!mRoutingState.tryAcquire(outer) ||
        !mRoutingState.canPublishLocked(outer))
        return false;
    StateAuthority::WriteHandle writer;
    if (!mState.tryBeginWrite(writer))
        return false;

    bool changed = false;
    bool complete = true;
    for (std::size_t i = 0; i < mDeferredHostValues.size(); ++i)
    {
        auto& mailbox = mDeferredHostValues[i];
        uint64_t encoded = mailbox.load(std::memory_order_acquire);
        bool consumed = false;
        for (unsigned attempt = 0; attempt < 4u; ++attempt)
        {
            if ((encoded & 0x100u) == 0u)
                break;
            const uint64_t order = encoded >> 9u;
            if (order > cutoff && order > maximumOrder)
                break;
            if (mailbox.compare_exchange_weak(
                    encoded, 0, std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                consumed = true;
                if (order > cutoff)
                {
                    const uint8_t value = static_cast<uint8_t>(encoded & 0x7fu);
                    mState.write(writer, i, value);
                    auto* param = GetParam(static_cast<int>(i));
                    if (param != nullptr)
                        param->Set(static_cast<double>(value));
                    changed = true;
                }
                break;
            }
        }
        if (!consumed && (encoded & 0x100u) != 0u &&
            ((encoded >> 9u) <= cutoff || (encoded >> 9u) <= maximumOrder))
            complete = false;
    }

    if (changed)
    {
        mState.commit(writer);
        mRoutingState.commitPatchLocked(outer);
    }
    else
        mState.cancelUnmodified(writer);

    // Patch publication is ordered before route slots, then route slots are
    // consumed in stable velocity0..5/LFO0..5 order.
    for (std::size_t routeSlot = 0;
         routeSlot < mDeferredRouteValues.size(); ++routeSlot)
    {
        auto& mailbox = mDeferredRouteValues[routeSlot];
        uint64_t encoded = mailbox.load(std::memory_order_acquire);
        bool consumed = false;
        for (unsigned attempt = 0; attempt < 4u; ++attempt)
        {
            if ((encoded & UINT64_C(0x10000)) == 0u)
                break;
            const uint64_t order = encoded >> 17u;
            if (order > cutoff && order > maximumOrder)
                break;
            if (mailbox.compare_exchange_weak(
                    encoded, 0, std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                consumed = true;
                lxr::PersistentRouteRecord current;
                uint64_t routeGeneration = 0;
                uint64_t restoreEpoch = 0;
                if (!mRoutingState.routeLocked(
                        outer, routeSlot, current,
                        routeGeneration, restoreEpoch))
                {
                    complete = false;
                    break;
                }
                (void)routeGeneration;
                const auto id = lxr::route_host::parameterForSlot(routeSlot);
                auto* param = GetParam(static_cast<int>(id.value()));
                if (order > cutoff)
                {
                    const auto token = lxr::route_host::RouteToken16::fromRaw(
                        static_cast<std::uint32_t>(encoded & 0xffffu));
                    const auto decoded = lxr::route_host::decode(id, token);
                    const auto status = decoded
                        ? mRoutingState.commitRouteLocked(
                              outer, decoded.record, order, restoreEpoch)
                        : lxr::PersistentRouteStatus::InvalidTarget;
                    if (status == lxr::PersistentRouteStatus::Ok ||
                        status == lxr::PersistentRouteStatus::Disabled)
                        break;
                }
                if (param != nullptr)
                    param->Set(static_cast<double>(
                        lxr::route_host::encode(current).value()));
                break;
            }
        }
        if (!consumed && (encoded & UINT64_C(0x10000)) != 0u &&
            ((encoded >> 17u) <= cutoff ||
             (encoded >> 17u) <= maximumOrder))
            complete = false;
    }

    for (std::size_t routeSlot = 0;
         routeSlot < mDeferredRouteRejections.size(); ++routeSlot)
    {
        auto& mailbox = mDeferredRouteRejections[routeSlot];
        uint64_t order = mailbox.load(std::memory_order_acquire);
        bool consumed = false;
        for (unsigned attempt = 0; attempt < 4u; ++attempt)
        {
            if (order == 0u || (order > cutoff && order > maximumOrder))
                break;
            if (mailbox.compare_exchange_weak(
                    order, 0, std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                consumed = true;
                lxr::PersistentRouteRecord current;
                uint64_t routeGeneration = 0;
                uint64_t restoreEpoch = 0;
                if (!mRoutingState.routeLocked(
                        outer, routeSlot, current,
                        routeGeneration, restoreEpoch))
                {
                    complete = false;
                    break;
                }
                (void)routeGeneration;
                const auto id = lxr::route_host::parameterForSlot(routeSlot);
                auto* param = GetParam(static_cast<int>(id.value()));
                if (param != nullptr)
                    param->Set(static_cast<double>(
                        lxr::route_host::encode(current).value()));
                if (order > cutoff)
                {
                    const auto status = mRoutingState.republishRouteLocked(
                        outer, routeSlot, order, restoreEpoch);
                    if (status != lxr::PersistentRouteStatus::Ok &&
                        status != lxr::PersistentRouteStatus::Stale)
                        complete = false;
                }
                break;
            }
        }
        if (!consumed && order != 0u &&
            (order <= cutoff || order <= maximumOrder))
            complete = false;
    }
    bool transportChanged = false;
    for (std::size_t slot = 0; slot < mDeferredTransportValues.size(); ++slot)
    {
        auto& mailbox = mDeferredTransportValues[slot];
        auto encoded = mailbox.load(std::memory_order_acquire);
        bool consumed = false;
        for (unsigned attempt = 0; attempt < 4u; ++attempt)
        {
            if ((encoded & 0x200u) == 0u) break;
            const auto order = encoded >> 10u;
            if (order > cutoff && order > maximumOrder) break;
            if (mailbox.compare_exchange_weak(encoded, 0, std::memory_order_acq_rel,
                                              std::memory_order_acquire))
            {
                consumed = true;
                if (order > cutoff)
                {
                    const auto value = static_cast<std::uint16_t>(encoded & 0x1ffu);
                    if (slot == 0) mTransportConfig.enabled = static_cast<std::uint8_t>(value);
                    if (slot == 1) mTransportConfig.clock = static_cast<std::uint8_t>(value);
                    if (slot == 2) mTransportConfig.bpm = value;
                    transportChanged = true;
                }
                const int value = slot == 0 ? mTransportConfig.enabled
                    : slot == 1 ? mTransportConfig.clock : mTransportConfig.bpm;
                GetParam(lxr::vst::kSequencerEnableParam + static_cast<int>(slot))->Set(value);
                break;
            }
        }
        if (!consumed && (encoded & 0x200u) != 0 &&
            ((encoded >> 10u) <= cutoff || (encoded >> 10u) <= maximumOrder)) complete = false;
    }
    if (transportChanged)
    {
        mSequencerEnabled.store(mTransportConfig.enabled, std::memory_order_release);
        mSequencerClockMode.store(mTransportConfig.clock, std::memory_order_release);
        mSequencerInternalTempo.store(mTransportConfig.bpm, std::memory_order_release);
        mRoutingState.commitPatchLocked(outer);
    }
    bool rollChanged = false;
    if (mRollReleasePending.exchange(false, std::memory_order_acq_rel))
    {
        const auto release = mRollReleaseCutoff.load(std::memory_order_acquire);
        if (mRollControls.pressTicket <= release)
        {
            mRollControls.tracks = 0u;
            GetParam(lxr::vst::kSequencerRollTracksParam)->Set(0.0);
            rollChanged = true;
        }
    }
    for (std::size_t slot = 0; slot < mDeferredRollValues.size(); ++slot)
    {
        auto& mailbox = mDeferredRollValues[slot];
        auto encoded = mailbox.load(std::memory_order_acquire);
        bool consumed = false;
        for (unsigned attempt = 0; attempt < 4u; ++attempt)
        {
            if ((encoded & 0x100u) == 0u) break;
            const auto order = encoded >> 9u;
            if (order > cutoff && order > maximumOrder) break;
            if (mailbox.compare_exchange_weak(encoded, 0u, std::memory_order_acq_rel,
                                              std::memory_order_acquire))
            {
                consumed = true;
                const auto release = slot == 1u
                    ? mRollReleaseCutoff.load(std::memory_order_acquire) : 0u;
                if (order > cutoff && order > release)
                {
                    const auto value = static_cast<std::uint8_t>(encoded & 0x7fu);
                    if (slot == 0u) mRollControls.rate = value;
                    else { mRollControls.tracks = value; mRollControls.pressTicket = order; }
                    rollChanged = true;
                }
                GetParam(lxr::vst::kSequencerRollRateParam + static_cast<int>(slot))->Set(
                    slot == 0u ? mRollControls.rate : mRollControls.tracks);
                break;
            }
        }
        if (!consumed && (encoded & 0x100u) != 0u &&
            ((encoded >> 9u) <= cutoff || (encoded >> 9u) <= maximumOrder)) complete = false;
    }
    if (rollChanged) mRoutingState.commitPatchLocked(outer);
    complete &= FlushDeferredMorphAmount(maximumOrder);
    return complete;
}

bool LXR::ApplyPendingStateAndParameters()
{
    if (!FlushDeferredHostParameters())
        return false;

    StateAuthority::Values candidate {};
    uint64_t parameterGeneration = 0;
    lxr::RoutingStateAuthority::Snapshot routing;
    lxr::sequencer::PatternStateAuthority::ReadHandle pattern;
    lxr::sequencer::state::TransportConfig transport;
    lxr::sequencer::RollHostControls rolls;
    if (!TryPersistentSnapshot(candidate, parameterGeneration, routing, &pattern, &transport, &rolls))
        return false;

    lxr::morph::Snapshot morph;
    if (!mMorphState.trySnapshot(morph, 1u))
        return false;
    lxr::morph::Values source;
    lxr::morph::Values projection = candidate;
    if (morph.targetPresent)
    {
        if (!lxr::morph::BuildSourceValues(
                candidate, routing.state.routes, source))
            return false;
        lxr::morph::BuildEffectiveProjection(
            source, morph.target.values, morph.amount, projection);
    }

    const auto stillCurrent = [&]() noexcept {
        return mState.generation() == parameterGeneration &&
               mRoutingState.generation() == routing.generation &&
               mMorphState.generation() == morph.generation;
    };

    if (routing.restoreEpoch != mAppliedRestoreGeneration)
    {
        if (!PrepareCompleteRestore(projection, routing.state) ||
            !stillCurrent())
            return false;
        CommitCompleteRestore(routing.state);
        mSequencer.resetSeed(transport.seed);
        (void)mSequencer.selectPattern(transport.pattern);
        mAppliedRestoreGeneration = routing.restoreEpoch;
    }
    else if (routing.generation != mAppliedJointGeneration ||
             morph.generation != mAppliedMorphGeneration)
    {
        const bool appliesKit = routing.kitGeneration != 0u &&
            routing.kitGeneration > mAppliedKitGeneration;
        mStagingEngine = mEngine;
        mStagingParams = mParams;
        for (std::size_t i = 0; i < projection.size(); ++i)
        {
            const uint8_t index = static_cast<uint8_t>(i);
            if (mStagingParams.get(index) != projection[i])
                mStagingParams.set(index, projection[i], mStagingEngine);
        }
        if (morph.targetPresent)
        {
            // P18-B precedence is authoritative A -> morph projection ->
            // sequencer lock. Replaying active nodes also makes later lock
            // release restore the newest projected base, never a pre-morph A.
            if (!mStagingEngine.reapplySequencerAutomation())
                return false;
        }
        else if (appliesKit)
        {
            // Kit replacement rebases every still-active lock. A host/MIDI
            // edit published after the kit remains authoritative under the
            // pre-existing ordering, so replay those newer bytes last.
            if (!mStagingEngine.reapplySequencerAutomation())
                return false;
            for (std::size_t i = 0; i < candidate.size(); ++i)
            {
                const auto valueGeneration = mState.valueGeneration(i);
                if (valueGeneration <= routing.kitPatchGeneration ||
                    valueGeneration > parameterGeneration)
                    continue;
                const auto index = static_cast<std::uint8_t>(i);
                mStagingParams.set(index, candidate[i], mStagingEngine);
            }
        }
        for (std::size_t i = 0; i < routing.state.routes.size(); ++i)
        {
            if (SameRoute(mAppliedRouting.routes[i], routing.state.routes[i]))
                continue;
            const auto& route = routing.state.routes[i];
            using namespace lxr::modulation_contract;
            const SourceIdentity source {
                route.source == 0u ? SourceKind::Velocity : SourceKind::Lfo,
                voiceFromZeroBased(route.owner)
            };
            const auto target = OriginalTargetIndex::fromRaw(route.originalIndex);
            const auto status = route.source == 0u
                ? mStagingEngine.configureVelocityDestination(source, target)
                : mStagingEngine.configureLfoDestination(source, target);
            if (status != lxr::VelocityRouteStatus::Ok)
                return false;
        }
        // Reject a candidate whose A, typed routes, B, or amount changed while
        // the resident staging objects were being constructed. The previous
        // live Engine remains completely untouched in that case.
        if (!stillCurrent())
            return false;
        mEngine = mStagingEngine;
        mParams = mStagingParams;
        mAppliedRouting = routing.state;
        if (appliesKit)
        {
            // Engine mutation occurs after the preceding native block was
            // completely rendered. The next producer refill begins exactly at
            // this 32-frame native boundary; cached old PCM is not rewritten.
            mAppliedKitGeneration = routing.kitGeneration;
            mAppliedKitNativeBoundary = mAudioAdapter.nativeFrame();
        }
        if (morph.generation != mAppliedMorphGeneration ||
            mAppliedMorphProjection != projection)
            mAppliedMorphNativeBoundary = mAudioAdapter.nativeFrame();
    }
    // Select all four domains before any event generation or rendering.
    mAppliedPattern.swap(pattern);
    mAudioTransport = transport;
    mAudioRollControls = rolls;
    mAppliedStateGeneration = parameterGeneration;
    mAppliedJointGeneration = routing.generation;
    mAppliedMorphGeneration = morph.generation;
    mAppliedMorphProjection = projection;
    return true;
}

lxr::kit::RuntimeLoadResult LXR::LoadRuntimeKit(
    const lxr::kit::CanonicalKit& kit)
{
    lxr::kit::RuntimeLoadResult result;
    lxr::kit::PatchCandidate candidate;
    result.candidate = lxr::kit::BuildPatchCandidate(kit, candidate);
    if (!result.candidate)
    {
        result.publication.field = result.candidate.field;
        return result;
    }

    // Reserve one position in the same monotonic order used by host patch,
    // route, transport and full-restore producers. Drain only older mailboxes;
    // later automation remains pending and therefore wins after this kit.
    const std::uint64_t reservation =
        mDeferredHostOrder.fetch_add(1u, std::memory_order_acq_rel);
    if (reservation == 0u)
    {
        result.publication.status =
            lxr::kit::PublicationStatus::GenerationExhausted;
        return result;
    }
    if (!FlushDeferredHostParameters(reservation - 1u))
    {
        result.publication.status = lxr::kit::PublicationStatus::Busy;
        result.publication.reservation = reservation;
        return result;
    }

    const lxr::kit::PublicationReservation publication {
        reservation, mRoutingState.generation()
    };
    result.publication = lxr::kit::PublishPatchCandidate(
        candidate, publication, mState, mRoutingState);
    if (!result.publication)
        return result;

    // Project the accepted authority only while its generation is still
    // current. If a newer producer already won, OnIdle reconciles that newer
    // state instead; stale kit values never overwrite the IParam mirror.
    lxr::RoutingStateAuthority::Guard outer;
    StateAuthority::WriteHandle projection;
    if (mRoutingState.tryAcquire(outer) &&
        mRoutingState.generation() == result.publication.jointGeneration &&
        mState.tryBeginWrite(projection))
    {
        for (std::size_t i = 0; i < candidate.patch.size(); ++i)
            if (auto* param = GetParam(static_cast<int>(i)); param != nullptr)
                param->Set(static_cast<double>(candidate.patch[i]));
        for (std::size_t slot = 0; slot < candidate.routes.size(); ++slot)
        {
            const auto id = lxr::route_host::parameterForSlot(slot);
            if (auto* param = GetParam(static_cast<int>(id.value()));
                param != nullptr)
            {
                param->Set(static_cast<double>(
                    lxr::route_host::encode(candidate.routes[slot]).value()));
            }
        }
        mState.cancelUnmodified(projection);
    }
    mControllerNeedsReconcile = true;
    return result;
}

lxr::morph::LoadResult LXR::LoadMorphTarget(
    const lxr::kit::CanonicalKit& kit)
{
    lxr::morph::LoadResult result;
    lxr::morph::Target candidate;
    result.candidate = lxr::morph::ValidateTarget(kit, candidate);
    if (!result.candidate)
    {
        result.publication.status =
            lxr::morph::PublicationStatus::InvalidCandidate;
        return result;
    }

    const auto reservation =
        mDeferredHostOrder.fetch_add(1u, std::memory_order_acq_rel);
    if (reservation == 0u)
    {
        result.publication.status =
            lxr::morph::PublicationStatus::GenerationExhausted;
        return result;
    }
    if (!FlushDeferredHostParameters(reservation - 1u))
    {
        result.publication.status = lxr::morph::PublicationStatus::Busy;
        result.publication.order = reservation;
        return result;
    }

    const auto expected = mMorphState.generation();
    result.publication = mMorphState.tryPublishTarget(
        candidate, reservation, expected);
    return result;
}

bool LXR::CurrentMorphState(lxr::morph::Snapshot& state) const noexcept
{
    return mMorphState.trySnapshot(state, 1u);
}

lxr::morph::management::OperationResult LXR::LoadMorphTargetFile(
    const wchar_t* path)
{
    using namespace lxr::morph::management;
    lxr::kit::CanonicalKit target;
    const auto file = lxr::kit::snd::ReadFile(path, target);
    if (!file)
        mLastMorphOperation = FromFileFailure(Action::LoadTarget, file);
    else
    {
        const auto load = LoadMorphTarget(target);
        if (!load)
            mLastMorphOperation = FromPublicationFailure(load);
        else
        {
            mLastMorphOperation = {};
            mLastMorphOperation.action = Action::LoadTarget;
            mLastMorphOperation.status = Status::Success;
            mLastMorphOperation.publicationStatus = load.publication.status;
            mLastMorphOperation.published = true;
        }
    }

    lxr::morph::Snapshot snapshot;
    if (CurrentMorphState(snapshot))
        PopulateMorphOperationState(mLastMorphOperation, snapshot);
    return mLastMorphOperation;
}

lxr::morph::management::OperationResult LXR::ClearMorphTarget()
{
    using namespace lxr::morph::management;
    OperationResult result;
    result.action = Action::ClearTarget;
    const auto reservation =
        mDeferredHostOrder.fetch_add(1u, std::memory_order_acq_rel);
    if (reservation == 0u)
    {
        result.status = Status::HostUnavailable;
        result.publicationStatus =
            lxr::morph::PublicationStatus::GenerationExhausted;
    }
    else if (!FlushDeferredHostParameters(reservation - 1u))
    {
        result.status = Status::PublicationBusy;
        result.publicationStatus = lxr::morph::PublicationStatus::Busy;
    }
    else
    {
        const auto expected = mMorphState.generation();
        const auto publication = mMorphState.tryClear(reservation, expected);
        result.publicationStatus = publication.status;
        result.status = publication.status == lxr::morph::PublicationStatus::Ok
            ? Status::Success
            : publication.status == lxr::morph::PublicationStatus::Busy
                ? Status::PublicationBusy
                : publication.status == lxr::morph::PublicationStatus::Stale
                    ? Status::StaleOperation : Status::HostUnavailable;
        result.published = publication.status ==
            lxr::morph::PublicationStatus::Ok;
    }
    lxr::morph::Snapshot snapshot;
    if (CurrentMorphState(snapshot))
        PopulateMorphOperationState(result, snapshot);
    mLastMorphOperation = result;
    return mLastMorphOperation;
}

lxr::morph::management::OperationResult LXR::CancelMorphOperation(
    lxr::morph::management::Action action) noexcept
{
    mLastMorphOperation = {};
    mLastMorphOperation.action = action;
    mLastMorphOperation.status =
        lxr::morph::management::Status::Cancelled;
    lxr::morph::Snapshot snapshot;
    if (CurrentMorphState(snapshot))
        PopulateMorphOperationState(mLastMorphOperation, snapshot);
    return mLastMorphOperation;
}

lxr::morph::management::OperationResult LXR::ExportMorphedKitFile(
    const wchar_t* path)
{
    using namespace lxr::morph::management;
    OperationResult result;
    result.action = Action::ExportMorphed;

    // Drain only operations older than this export. Newer host edits may enter
    // fixed mailboxes while the gates are held and are applied after export.
    const auto reservation =
        mDeferredHostOrder.fetch_add(1u, std::memory_order_acq_rel);
    if (reservation == 0u ||
        !FlushDeferredHostParameters(reservation - 1u))
    {
        result.status = reservation == 0u
            ? Status::HostUnavailable : Status::PublicationBusy;
        result.publicationStatus = reservation == 0u
            ? lxr::morph::PublicationStatus::GenerationExhausted
            : lxr::morph::PublicationStatus::Busy;
        mLastMorphOperation = result;
        return mLastMorphOperation;
    }

    lxr::RoutingStateAuthority::Guard routingGuard;
    StateAuthority::WriteHandle patchGuard;
    lxr::morph::StateAuthority::WriteHandle morphGuard;
    if (!mRoutingState.tryAcquire(routingGuard) ||
        !mState.tryBeginWrite(patchGuard) ||
        !mMorphState.tryBeginWrite(morphGuard))
    {
        if (morphGuard.active()) mMorphState.cancelUnmodified(morphGuard);
        if (patchGuard.active()) mState.cancelUnmodified(patchGuard);
        if (routingGuard.active()) mRoutingState.release(routingGuard);
        result.status = Status::PublicationBusy;
        result.publicationStatus = lxr::morph::PublicationStatus::Busy;
        mLastMorphOperation = result;
        return mLastMorphOperation;
    }

    lxr::RoutingStateAuthority::Snapshot routing;
    StateAuthority::Values patch;
    lxr::morph::Snapshot morph;
    const bool copied = mRoutingState.copyLocked(routingGuard, routing) &&
        mMorphState.copyLocked(morphGuard, morph);
    for (std::size_t i = 0; copied && i < patch.size(); ++i)
        patch[i] = mState.value(i);

    lxr::kit::CanonicalKit projected;
    result = copied
        ? BuildProjectedKit(patch, routing.state.routes, morph, projected)
        : OperationResult{};
    if (!copied)
    {
        result.action = Action::ExportMorphed;
        result.status = Status::HostUnavailable;
    }

    lxr::kit::snd::FileResult file;
    if (result)
    {
        file = lxr::kit::snd::WriteNewFile(
            path, projected, routing.state.routes);
        if (!file)
        {
            const auto retainedName = result.name;
            const auto retainedAmount = result.amount;
            const auto retainedGeneration = result.generation;
            result = FromFileFailure(Action::ExportMorphed, file);
            result.targetPresent = true;
            result.name = retainedName;
            result.amount = retainedAmount;
            result.generation = retainedGeneration;
        }
        else
        {
            result.published = file.published;
            result.publicationStatus = lxr::morph::PublicationStatus::Ok;
        }
    }

    // No accepted producer can change A, routes, B, or amount before the
    // complete file is flushed and either published or rejected. The audio
    // callback never waits on these gates; it keeps the prior projection.
    mMorphState.cancelUnmodified(morphGuard);
    mState.cancelUnmodified(patchGuard);
    mRoutingState.release(routingGuard);
    mLastMorphOperation = result;
    return mLastMorphOperation;
}

bool LXR::SnapshotCurrentKit(
    lxr::kit::CanonicalKit& kit,
    std::array<lxr::PersistentRouteRecord,
               lxr::kPersistentRouteCount>& routes) const
{
    StateAuthority::Values patch;
    std::uint64_t generation = 0;
    lxr::RoutingStateAuthority::Snapshot routing;
    if (!TryPersistentSnapshot(patch, generation, routing))
        return false;

    lxr::kit::RawKit raw;
    raw.name = routing.kitName;
    raw.values = patch;
    lxr::kit::CanonicalKit candidate;
    if (!lxr::kit::DecodeRawKit(raw, candidate))
        return false;
    kit = candidate;
    routes = routing.state.routes;
    return true;
}

lxr::kit::management::OperationResult LXR::ImportKitFile(
    const wchar_t* path)
{
    lxr::kit::CanonicalKit kit;
    const auto file = lxr::kit::snd::ReadFile(path, kit);
    if (!file)
    {
        mLastKitOperation = lxr::kit::management::FromFileFailure(
            lxr::kit::management::Action::Import, file);
        return mLastKitOperation;
    }

    const auto load = LoadRuntimeKit(kit);
    if (!load)
    {
        mLastKitOperation =
            lxr::kit::management::FromPublicationFailure(load);
        return mLastKitOperation;
    }

    mLastKitOperation = {};
    mLastKitOperation.action = lxr::kit::management::Action::Import;
    mLastKitOperation.status = lxr::kit::management::Status::Success;
    mLastKitOperation.publicationStatus = load.publication.status;
    mLastKitOperation.published = true;
    mLastKitOperation.name = kit.name;
    return mLastKitOperation;
}

lxr::kit::management::OperationResult LXR::ExportKitFile(
    const wchar_t* path)
{
    lxr::kit::CanonicalKit kit;
    std::array<lxr::PersistentRouteRecord,
               lxr::kPersistentRouteCount> routes;
    if (!SnapshotCurrentKit(kit, routes))
    {
        mLastKitOperation = {};
        mLastKitOperation.action = lxr::kit::management::Action::Export;
        mLastKitOperation.status =
            lxr::kit::management::Status::HostUnavailable;
        return mLastKitOperation;
    }

    const auto file = lxr::kit::snd::WriteNewFile(path, kit, routes);
    if (!file)
    {
        mLastKitOperation = lxr::kit::management::FromFileFailure(
            lxr::kit::management::Action::Export, file);
        mLastKitOperation.name = kit.name;
        return mLastKitOperation;
    }

    mLastKitOperation = {};
    mLastKitOperation.action = lxr::kit::management::Action::Export;
    mLastKitOperation.status = lxr::kit::management::Status::Success;
    mLastKitOperation.published = file.published;
    mLastKitOperation.name = kit.name;
    return mLastKitOperation;
}

lxr::kit::management::DisplayName LXR::CurrentKitDisplayName() const
{
    lxr::RoutingStateAuthority::Guard guard;
    lxr::RoutingStateAuthority::Snapshot routing;
    if (mRoutingState.tryAcquire(guard) &&
        mRoutingState.copyLocked(guard, routing))
        return lxr::kit::management::FormatName(routing.kitName);
    return lxr::kit::management::FormatName(mLastKitOperation.name);
}

void LXR::SetLastKitOperationPath(const char* path) noexcept
{
    mLastKitOperationPath.fill(0);
    if (path == nullptr)
        return;
    std::size_t at = 0;
    while (at + 1u < mLastKitOperationPath.size() && path[at] != 0)
    {
        mLastKitOperationPath[at] = path[at];
        ++at;
    }
}

void LXR::SetLastMorphOperationPath(const char* path) noexcept
{
    mLastMorphOperationPath.fill(0);
    if (path == nullptr)
        return;
    std::size_t at = 0;
    while (at + 1u < mLastMorphOperationPath.size() && path[at] != 0)
    {
        mLastMorphOperationPath[at] = path[at];
        ++at;
    }
}

#if IPLUG_EDITOR
void LXR::SelectGUIView(lxr::gui::PrimaryView view)
{
    if (!mGUISelection.select(view)) return;
    auto* graphics = GetUI();
    if (graphics == nullptr) return;
    const auto active = mGUISelection.view();
    const auto owner = static_cast<std::size_t>(mGUISelection.voice());
    auto setGroup = [graphics](const char* group, bool visible) {
        graphics->ForControlInGroup(group, [visible](iplug::igraphics::IControl* control) {
            control->Hide(!visible);
            control->SetDisabled(!visible);
        });
    };
    setGroup(lxr::gui::kViewGroups[0], active == lxr::gui::PrimaryView::Overview);
    for (std::size_t i = 0; i < kSoundGroups.size(); ++i)
    {
        setGroup(kSoundGroups[i], active == lxr::gui::PrimaryView::Sound && i == owner);
        setGroup(kModulationGroups[i],
                 active == lxr::gui::PrimaryView::Modulation && i == owner);
    }
    setGroup(lxr::gui::kViewGroups[3], active == lxr::gui::PrimaryView::Sequencer);
    setGroup(lxr::gui::kViewGroups[4], active == lxr::gui::PrimaryView::KitsMorph);

    WDL_String label;
    label.SetFormatted(96, "Active view: %s",
        lxr::gui::kViewNames[static_cast<std::size_t>(active)]);
    SetTextIfChanged(graphics->GetControlWithTag(kActiveViewControlTag), label.Get());
    if (mGUIFocusTag >= 0)
    {
        auto* focused = graphics->GetControlWithTag(mGUIFocusTag);
        if (focused == nullptr || focused->IsHidden())
        {
            mGUIFocusTag = -1;
            auto* ring = graphics->GetControlWithTag(kFocusRingControlTag);
            if (ring != nullptr) ring->Hide(true);
        }
    }
}

void LXR::SelectGUIVoice(lxr::gui::VoiceOwner voice)
{
    if (!mGUISelection.select(voice)) return;
    auto* graphics = GetUI();
    if (graphics == nullptr) return;
    WDL_String label;
    label.SetFormatted(96, "Voice: %s",
        lxr::gui::kVoiceNames[static_cast<std::size_t>(voice)]);
    SetTextIfChanged(graphics->GetControlWithTag(kSelectedVoiceControlTag), label.Get());
    // Re-evaluate owner-specific visibility without touching any parameter.
    SelectGUIView(mGUISelection.view());
}

bool LXR::HandleGUIKey(const iplug::IKeyPress& key, bool isUp)
{
    using namespace iplug;
    using namespace iplug::igraphics;
    const bool relevant = key.VK == kVK_TAB || key.VK == kVK_RETURN ||
        key.VK == kVK_SPACE || key.VK == kVK_LEFT || key.VK == kVK_RIGHT ||
        key.VK == kVK_UP || key.VK == kVK_DOWN || key.VK == kVK_HOME ||
        key.VK == kVK_END || key.VK == kVK_PRIOR || key.VK == kVK_NEXT;
    if (!relevant) return false;
    if (isUp) return true;
    auto* graphics = GetUI();
    if (graphics == nullptr) return false;

    std::array<int, 128> order {};
    std::size_t count = 0u;
    for (std::size_t i = 0; i < lxr::gui::kViewNames.size(); ++i)
        order[count++] = kNavControlTag + static_cast<int>(i);
    for (std::size_t i = 0; i < lxr::gui::kVoiceNames.size(); ++i)
        order[count++] = kVoiceControlTag + static_cast<int>(i);
    for (int tag = kImportKitOperationTag; tag <= kExportMorphOperationTag;
         ++tag)
    {
        auto* control = graphics->GetControlWithTag(tag);
        if (control != nullptr && !control->IsHidden()) order[count++] = tag;
    }
    for (const auto& binding : lxr::gui::kSoundBindings)
    {
        auto* control = graphics->GetControlWithTag(kParameterControlTag + binding.parameter);
        if (control != nullptr && !control->IsHidden())
            order[count++] = kParameterControlTag + binding.parameter;
    }
    for (const auto& binding : lxr::gui::kCoreBindings)
    {
        if (lxr::gui::soundBindingForParameter(binding.parameter) != nullptr ||
            lxr::gui::modulationBindingForParameter(binding.parameter) != nullptr)
            continue;
        auto* control = graphics->GetControlWithTag(kParameterControlTag + binding.parameter);
        if (control != nullptr && !control->IsHidden())
            order[count++] = kParameterControlTag + binding.parameter;
    }
    for (const auto& binding : lxr::gui::kModulationBindings)
    {
        auto* control = graphics->GetControlWithTag(
            kParameterControlTag + binding.parameter);
        if (control != nullptr && !control->IsHidden())
            order[count++] = kParameterControlTag + binding.parameter;
    }
    for (const auto& binding : lxr::gui::kGlobalBindings)
    {
        auto* control = graphics->GetControlWithTag(kParameterControlTag + binding.parameter);
        if (control != nullptr && !control->IsHidden())
            order[count++] = kParameterControlTag + binding.parameter;
    }

    auto setFocus = [&](int tag) {
        if (mGUIFocusTag >= kParameterControlTag)
        {
            const int oldParameter = mGUIFocusTag - kParameterControlTag;
            const auto* oldSound = lxr::gui::soundBindingForParameter(oldParameter);
            const auto* oldModulation =
                lxr::gui::modulationBindingForParameter(oldParameter);
            const auto* oldBinding = lxr::gui::bindingForParameter(oldParameter);
            auto* oldControl = graphics->GetControlWithTag(mGUIFocusTag);
            if (oldSound != nullptr && oldControl != nullptr)
                static_cast<LXRSoundControl*>(oldControl)->SetFocused(false);
            else if (oldModulation != nullptr && oldControl != nullptr)
                static_cast<LXRModulationControl*>(oldControl)->SetFocused(false);
            else if (oldBinding != nullptr && oldControl != nullptr &&
                (oldBinding->kind == lxr::gui::ControlKind::Enumeration ||
                 oldBinding->kind == lxr::gui::ControlKind::SourceToggle ||
                 oldBinding->kind == lxr::gui::ControlKind::TypedRoute))
                static_cast<LXRDiscreteSelector*>(oldControl)->SetFocused(false);
        }
        mGUIFocusTag = tag;
        auto* control = graphics->GetControlWithTag(tag);
        auto* ring = graphics->GetControlWithTag(kFocusRingControlTag);
        if (control != nullptr && ring != nullptr)
        {
            ring->SetTargetAndDrawRECTs(control->GetRECT());
            ring->Hide(false);
            ring->SetDirty(false);
        }
        if (tag >= kParameterControlTag)
        {
            const int parameter = tag - kParameterControlTag;
            const auto* sound = lxr::gui::soundBindingForParameter(parameter);
            const auto* modulation =
                lxr::gui::modulationBindingForParameter(parameter);
            const auto* binding = lxr::gui::bindingForParameter(parameter);
            if (sound != nullptr)
                static_cast<LXRSoundControl*>(control)->SetFocused(true);
            else if (modulation != nullptr)
                static_cast<LXRModulationControl*>(control)->SetFocused(true);
            else if (binding != nullptr &&
                (binding->kind == lxr::gui::ControlKind::Enumeration ||
                 binding->kind == lxr::gui::ControlKind::SourceToggle ||
                 binding->kind == lxr::gui::ControlKind::TypedRoute))
                static_cast<LXRDiscreteSelector*>(control)->SetFocused(true);
        }
    };

    if (key.VK == kVK_TAB)
    {
        std::size_t at = 0u;
        while (at < count && order[at] != mGUIFocusTag) ++at;
        if (key.S)
            at = at == 0u || at == count ? count - 1u : at - 1u;
        else
            at = at == count ? 0u : (at + 1u) % count;
        setFocus(order[at]);
        return true;
    }
    if (mGUIFocusTag < 0) return false;
    if (mGUIFocusTag >= kNavControlTag && mGUIFocusTag < kVoiceControlTag)
    {
        if (key.VK == kVK_RETURN || key.VK == kVK_SPACE)
            SelectGUIView(static_cast<lxr::gui::PrimaryView>(mGUIFocusTag - kNavControlTag));
        return true;
    }
    if (mGUIFocusTag >= kVoiceControlTag &&
        mGUIFocusTag < kOperationControlTag)
    {
        if (key.VK == kVK_RETURN || key.VK == kVK_SPACE)
            SelectGUIVoice(static_cast<lxr::gui::VoiceOwner>(mGUIFocusTag - kVoiceControlTag));
        return true;
    }
    if (mGUIFocusTag >= kOperationControlTag &&
        mGUIFocusTag <= kExportMorphOperationTag)
    {
        if (key.VK == kVK_RETURN || key.VK == kVK_SPACE)
        {
            auto* control = graphics->GetControlWithTag(mGUIFocusTag);
            if (control != nullptr)
            {
                const auto bounds = control->GetRECT();
                const float x = (bounds.L + bounds.R) * 0.5f;
                const float y = (bounds.T + bounds.B) * 0.5f;
                control->OnMouseDown(x, y,
                    iplug::igraphics::IMouseMod(true, false, key.S));
                control->OnMouseUp(x, y, iplug::igraphics::IMouseMod());
            }
        }
        return true;
    }

    const int parameter = mGUIFocusTag - kParameterControlTag;
    const auto* sound = lxr::gui::soundBindingForParameter(parameter);
    const auto* modulation =
        lxr::gui::modulationBindingForParameter(parameter);
    const auto* binding = lxr::gui::bindingForParameter(parameter);
    auto* control = graphics->GetControlWithTag(mGUIFocusTag);
    if ((sound == nullptr && modulation == nullptr && binding == nullptr) ||
        control == nullptr) return false;
    if (key.VK == kVK_RETURN || key.VK == kVK_SPACE)
    {
        control->PromptUserInput();
        return true;
    }
    int direction = 0;
    if (key.VK == kVK_LEFT || key.VK == kVK_DOWN || key.VK == kVK_NEXT)
        direction = -1;
    if (key.VK == kVK_RIGHT || key.VK == kVK_UP || key.VK == kVK_PRIOR)
        direction = 1;
    if (sound != nullptr)
    {
        auto* soundControl = static_cast<LXRSoundControl*>(control);
        if (key.VK == kVK_HOME || key.VK == kVK_END)
            soundControl->KeyboardEndpoint(key.VK == kVK_END);
        else if (direction != 0)
            soundControl->KeyboardStep(direction,
                (key.VK == kVK_PRIOR || key.VK == kVK_NEXT) ? 10 : 1);
        return true;
    }
    if (modulation != nullptr)
    {
        auto* modulationControl = static_cast<LXRModulationControl*>(control);
        if (key.VK == kVK_HOME || key.VK == kVK_END)
            modulationControl->KeyboardEndpoint(key.VK == kVK_END);
        else if (direction != 0)
            modulationControl->KeyboardStep(direction,
                (key.VK == kVK_PRIOR || key.VK == kVK_NEXT) ? 10 : 1);
        return true;
    }
    if (binding->kind == lxr::gui::ControlKind::Enumeration ||
        binding->kind == lxr::gui::ControlKind::SourceToggle ||
        binding->kind == lxr::gui::ControlKind::TypedRoute)
    {
        if (direction != 0)
            static_cast<LXRDiscreteSelector*>(control)->KeyboardStep(direction);
        return true;
    }

    double normalized = control->GetValue();
    if (key.VK == kVK_HOME) normalized = 0.0;
    else if (key.VK == kVK_END) normalized = 1.0;
    else if (direction != 0)
    {
        const int span = binding->maximum - binding->minimum;
        const int steps = (key.VK == kVK_PRIOR || key.VK == kVK_NEXT) ? 10 : 1;
        normalized = iplug::Clip(normalized + direction *
            static_cast<double>(steps) / static_cast<double>(span), 0.0, 1.0);
    }
    else return true;
    auto* delegate = control->GetDelegate();
    delegate->BeginInformHostOfParamChangeFromUI(parameter);
    control->SetValue(normalized);
    control->SetDirty(true);
    delegate->EndInformHostOfParamChangeFromUI(parameter);
    return true;
}

void LXR::RefreshGUIReadback()
{
    auto* graphics = GetUI();
    if (graphics == nullptr) return;
    ++mGUIRefreshCount;
    lxr::gui::AudioReadback snapshot;
    if (!mGUIReadback.trySnapshot(snapshot)) return;
    mLastGUIReadback = snapshot;
    bool changed = false;
    WDL_String value;
    value.SetFormatted(64, "Pattern: %u", static_cast<unsigned>(snapshot.activePattern) + 1u);
    changed |= SetTextIfChanged(graphics->GetControlWithTag(kPatternControlTag), value.Get());
    value.SetFormatted(64, "Clock: %s", snapshot.clockMode == 0u ? "Internal" : "Host Sync");
    changed |= SetTextIfChanged(graphics->GetControlWithTag(kClockControlTag), value.Get());
    if (snapshot.clockMode == 0u)
        value.SetFormatted(64, "BPM: %u", static_cast<unsigned>(snapshot.internalTempo));
    else if (snapshot.hostTempoValid)
        value.SetFormatted(64, "BPM: %.2f", snapshot.hostTempoBpm);
    else
        value.Set("BPM: unavailable");
    changed |= SetTextIfChanged(graphics->GetControlWithTag(kTempoControlTag), value.Get());
    if (!snapshot.hostTransportValid)
        value.Set("Host: unavailable");
    else
        value.Set(snapshot.hostPlaying ? "Host: playing" : "Host: stopped");
    changed |= SetTextIfChanged(graphics->GetControlWithTag(kTransportControlTag), value.Get());
    value.SetFormatted(64, "Sequencer: %s%s",
        snapshot.sequencerEnabled ? "ON" : "OFF",
        snapshot.sequencerEnabled && snapshot.sequencerRunning ? " / running" : "");
    changed |= SetTextIfChanged(graphics->GetControlWithTag(kSequencerStatusControlTag), value.Get());
    const auto* morph = GetParam(lxr::vst::kMorphAmountParam);
    value.SetFormatted(64, "Morph Amount: %u",
        morph != nullptr ? static_cast<unsigned>(std::floor(morph->Value() + 0.5)) : 0u);
    changed |= SetTextIfChanged(graphics->GetControlWithTag(kHeaderMorphControlTag), value.Get());
    if (changed) ++mGUIRefreshChangedCount;
}

void LXR::RefreshKitManagementControls()
{
    auto* graphics = GetUI();
    if (graphics == nullptr)
        return;
    auto* nameControl = graphics->GetControlWithTag(kKitNameControlTag);
    auto* identityControl = graphics->GetControlWithTag(kKitIdentityDetailTag);
    auto* statusControl = graphics->GetControlWithTag(kKitStatusControlTag);
    if (nameControl != nullptr || identityControl != nullptr)
    {
        lxr::RoutingStateAuthority::Guard guard;
        lxr::RoutingStateAuthority::Snapshot routing;
        const bool coherent = mRoutingState.tryAcquire(guard) &&
            mRoutingState.copyLocked(guard, routing);
        const auto name = coherent
            ? lxr::kit::management::FormatName(routing.kitName)
            : CurrentKitDisplayName();
        WDL_String text;
        text.SetFormatted(96, "Current kit: %s", name.text.data());
        SetTextIfChanged(nameControl, text.Get());
        if (identityControl != nullptr)
        {
            text.SetFormatted(160, "Current kit: %s (%s)", name.text.data(),
                coherent && routing.kitIdentityPresent
                    ? "embedded identity present"
                    : "no embedded identity");
            SetTextIfChanged(identityControl, text.Get());
        }
    }
    if (statusControl != nullptr)
    {
        WDL_String text;
        const auto* description =
            lxr::kit::management::StatusDescription(mLastKitOperation.status);
        if (mLastKitOperationPath[0] != 0)
            text.SetFormatted(1400, "%s\n%s", description,
                              mLastKitOperationPath.data());
        else
            text.Set(description);
        if (mLastKitOperation.field != 0xffffu)
            text.AppendFormatted(64, " Field %u.", mLastKitOperation.field);
        if (mLastKitOperation.route != 0xffu)
            text.AppendFormatted(64, " Route %u.", mLastKitOperation.route);
        SetTextIfChanged(statusControl, text.Get());
        WDL_String operation;
        operation.SetFormatted(256, "Kit: %s", text.Get());
        if (mLatestGUIOperationKind == kKitGUIOperation)
        {
            SetTextIfChanged(graphics->GetControlWithTag(kOperationStatusControlTag), operation.Get());
            SetTextIfChanged(graphics->GetControlWithTag(kOperationStatusControlTag + 100), operation.Get());
        }
    }
}

void LXR::RefreshMorphManagementControls()
{
    auto* graphics = GetUI();
    if (graphics == nullptr)
        return;
    auto* targetControl = graphics->GetControlWithTag(kMorphTargetControlTag);
    auto* statusControl = graphics->GetControlWithTag(kMorphStatusControlTag);
    lxr::morph::Snapshot snapshot;
    const bool available = CurrentMorphState(snapshot);
    if (targetControl != nullptr)
    {
        WDL_String text;
        if (available && snapshot.targetPresent)
        {
            const auto name = lxr::kit::management::FormatName(
                snapshot.target.name);
            text.SetFormatted(160, "Morph target: %s (loaded)",
                              name.text.data());
        }
        else if (available)
            text.Set("Morph target: <none>");
        else
            text.Set("Morph target: Busy; display unchanged soon.");
        SetTextIfChanged(targetControl, text.Get());
    }
    if (statusControl != nullptr)
    {
        WDL_String text;
        const auto* description =
            lxr::morph::management::StatusDescription(
                mLastMorphOperation.status);
        const unsigned amount = available
            ? snapshot.amount : mLastMorphOperation.amount;
        text.SetFormatted(1600, "Amount %u. %s", amount, description);
        if (mLastMorphOperationPath[0] != 0)
            text.AppendFormatted(1100, "\n%s", mLastMorphOperationPath.data());
        if (mLastMorphOperation.field != 0xffffu)
            text.AppendFormatted(64, " Field %u.", mLastMorphOperation.field);
        if (mLastMorphOperation.route != 0xffu)
            text.AppendFormatted(64, " Route %u.", mLastMorphOperation.route);
        SetTextIfChanged(statusControl, text.Get());
        if (mLatestGUIOperationKind == kMorphGUIOperation &&
            mLastMorphOperation.action != lxr::morph::management::Action::None)
        {
            WDL_String operation;
            operation.SetFormatted(256, "Morph: %s", text.Get());
            SetTextIfChanged(graphics->GetControlWithTag(kOperationStatusControlTag), operation.Get());
            SetTextIfChanged(graphics->GetControlWithTag(kOperationStatusControlTag + 100), operation.Get());
        }
    }
}
#endif

void LXR::PublishGUIReadback() noexcept
{
    const bool tempoFlag = (mHostProcessContext.validFields &
        lxr::sequencer::HostTempo) != 0u;
    const bool transportFlag = (mHostProcessContext.validFields &
        lxr::sequencer::HostTransportState) != 0u;
    mGUIReadback.publish(
        mHostProcessContext.tempoBpm,
        mHostProcessContext.contextPresent,
        tempoFlag,
        transportFlag,
        mHostProcessContext.playing,
        mSequencer.running(),
        mSequencer.selectedPattern(),
        mSequencer.pendingPattern(),
        mAppliedSequencerEnabled ? 1u : 0u,
        mAppliedSequencerClockMode,
        mAppliedSequencerInternalTempo);
}

// ---------------------------------------------------------
// Versioned state serialization
// ---------------------------------------------------------

bool LXR::SerializeState(iplug::IByteChunk& chunk) const
{
    const uint64_t watermark =
        mDeferredHostOrder.load(std::memory_order_acquire) - 1u;
    if (!const_cast<LXR*>(this)->FlushDeferredHostParameters(watermark))
        return false;

    lxr::state::ProjectCandidate candidate;
    uint64_t parameterGeneration = 0;
    lxr::RoutingStateAuthority::Snapshot routing;
    lxr::sequencer::PatternStateAuthority::ReadHandle pattern;
    if (!TryPersistentSnapshot(candidate.patch, parameterGeneration, routing,
                               &pattern, &candidate.transport)) return false;
    candidate.routing = routing.state;
    candidate.bank = pattern.bank();
    candidate.kitName = routing.kitName;
    candidate.kitIdentityPresent = routing.kitIdentityPresent;
    lxr::state::ProjectBytes encoded;
    std::size_t size = 0;
    if (!lxr::state::EncodeProject(candidate, encoded, size, false)) return false;
    return chunk.PutBytes(encoded.data(), static_cast<int>(size)) > 0;
}

bool LXR::PublishPersistentRestore(
    const StateAuthority::Values& parameters,
    const lxr::PersistentRoutingState& routing,
    const std::array<std::uint8_t, 8>& kitName,
    bool kitIdentityPresent,
    const lxr::sequencer::PatternBank* bank,
    const lxr::sequencer::state::TransportConfig* transport,
    uint64_t expectedJointGeneration)
{
    const auto defaults = bank == nullptr ? lxr::sequencer::MakeDefaultPatternBank()
                                           : lxr::sequencer::PatternBank{};
    const auto& acceptedBank = bank != nullptr ? *bank : defaults;
    const auto config = transport != nullptr ? *transport
        : lxr::sequencer::state::TransportConfig{};
    for (auto value : parameters) if (value > 127u) return false;
    if (!lxr::sequencer::state::Validate(config) ||
        lxr::sequencer::ValidateBank(acceptedBank) != lxr::sequencer::Status::Ok)
        return false;
    lxr::sequencer::PatternStateAuthority::WriteHandle patternWriter;
    auto& authority = mSequencer.patternAuthority();
    if (authority.tryBeginEdit(patternWriter) != lxr::sequencer::Status::Ok ||
        patternWriter.replace(acceptedBank) != lxr::sequencer::Status::Ok ||
        authority.generation() == UINT64_MAX) return false;
    const uint64_t restoreOrder =
        mDeferredHostOrder.fetch_add(1, std::memory_order_acq_rel);
    if (restoreOrder == 0u)
        return false;

    lxr::RoutingStateAuthority::Guard outer;
    if (!mRoutingState.tryAcquire(outer) ||
        (expectedJointGeneration != UINT64_MAX &&
         mRoutingState.generation() != expectedJointGeneration) ||
        mRoutingState.validateRestoreLocked(outer, routing, restoreOrder) !=
            lxr::PersistentRouteStatus::Ok)
    {
        return false;
    }
    StateAuthority::WriteHandle writer;
    if (!mState.tryBeginWrite(writer))
        return false;
    if (authority.commit(patternWriter) != lxr::sequencer::Status::Ok)
    {
        mState.cancelUnmodified(writer);
        return false;
    }
    if (!mState.writeAll(writer, parameters))
    {
        mState.cancelUnmodified(writer);
        return false;
    }

    mState.commit(writer);
    mTransportConfig = config;
    mRollControls = {};
    mSequencerEnabled.store(config.enabled, std::memory_order_release);
    mSequencerClockMode.store(config.clock, std::memory_order_release);
    mSequencerInternalTempo.store(config.bpm, std::memory_order_release);
    if (mRoutingState.commitRestoreLocked(
            outer, routing, restoreOrder, kitName, kitIdentityPresent) == 0u)
        return false; // Preflight above makes this unreachable without corruption.
    mRestoreOrderCutoff.store(restoreOrder, std::memory_order_release);

    // Logical publication is complete before any host-facing projection.
    for (std::size_t i = 0; i < parameters.size(); ++i)
    {
        auto* param = GetParam(static_cast<int>(i));
        if (param != nullptr)
            param->Set(static_cast<double>(parameters[i]));
    }
    for (std::size_t routeSlot = 0;
         routeSlot < routing.routes.size(); ++routeSlot)
    {
        const auto id = lxr::route_host::parameterForSlot(routeSlot);
        auto* param = GetParam(static_cast<int>(id.value()));
        if (param != nullptr)
            param->Set(static_cast<double>(
                lxr::route_host::encode(routing.routes[routeSlot]).value()));
    }
    GetParam(lxr::vst::kSequencerEnableParam)->Set(config.enabled);
    GetParam(lxr::vst::kSequencerClockModeParam)->Set(config.clock);
    GetParam(lxr::vst::kSequencerInternalTempoParam)->Set(config.bpm);
    GetParam(lxr::vst::kSequencerRollRateParam)->Set(8.0);
    GetParam(lxr::vst::kSequencerRollTracksParam)->Set(0.0);
    SetBypassed(routing.bypass);
    mControllerNeedsReconcile = true;
    return true;
}

int LXR::UnserializeState(const iplug::IByteChunk& chunk, int startPos)
{
    if (startPos < 0 || startPos >= chunk.Size())
    {
        return -1;
    }

    lxr::state::ProjectCandidate candidate;
    const auto* data = chunk.GetData() + startPos;
    const auto bytes = static_cast<std::size_t>(chunk.Size() - startPos);
    bool suffix = lxr::state::DecodeProject(data, bytes, candidate, true);
    if (!suffix && !lxr::state::DecodeProject(data, bytes, candidate, false)) return -1;
    if (!suffix) candidate.routing.bypass = GetBypassed();
    if (!PublishPersistentRestore(candidate.patch, candidate.routing,
                                   candidate.kitName,
                                   candidate.kitIdentityPresent,
                                   &candidate.bank, &candidate.transport)) return -1;
    mLastKitOperation = {};
    mLastKitOperation.name = candidate.kitName;
    return startPos + static_cast<int>(bytes - (suffix ? 4u : 0u));
}

// Called only by the editor's main-thread file-dialog completion.
bool LXR::ImportSequencerBank(const char* path)
{
    if (path == nullptr || *path == '\0') return false;
    lxr::sequencer::state::EncodedBank bytes;
    auto* file = fopen(path, "rb"); // iPlug2's Windows UTF-8 file wrapper.
    if (file == nullptr) return false;
    const auto read = std::fread(bytes.data(), 1, bytes.size(), file);
    const bool exact = read == bytes.size() && std::fgetc(file) == EOF && !std::ferror(file);
    std::fclose(file);
    lxr::state::ProjectCandidate candidate;
    if (!exact || !lxr::sequencer::state::Decode(bytes.data(), bytes.size(), candidate.bank))
        return false;
    const auto watermark = mDeferredHostOrder.load(std::memory_order_acquire) - 1u;
    if (!FlushDeferredHostParameters(watermark)) return false;
    lxr::RoutingStateAuthority::Snapshot routing;
    uint64_t generation = 0;
    if (!TryPersistentSnapshot(candidate.patch, generation, routing, nullptr, &candidate.transport))
        return false;
    candidate.routing = routing.state;
    candidate.kitName = routing.kitName;
    candidate.kitIdentityPresent = routing.kitIdentityPresent;
    return PublishPersistentRestore(candidate.patch, candidate.routing,
                                    candidate.kitName,
                                    candidate.kitIdentityPresent,
                                    &candidate.bank, &candidate.transport, routing.generation);
}

bool LXR::ConfigureRouteForTest(
    const lxr::PersistentRouteRecord& route,
    uint64_t expectedEpoch) noexcept
{
    const uint64_t ticket =
        mDeferredHostOrder.fetch_add(1, std::memory_order_acq_rel);
    if (ticket == 0u)
        return false;
    lxr::RoutingStateAuthority::Guard outer;
    if (!mRoutingState.tryAcquire(outer))
        return false;
    const auto status = mRoutingState.commitRouteLocked(
        outer, route, ticket, expectedEpoch);
    return status == lxr::PersistentRouteStatus::Ok ||
           status == lxr::PersistentRouteStatus::Disabled;
}

void LXR::DeferRouteHostToken(
    std::size_t routeSlot, lxr::route_host::RouteToken16 token,
    uint64_t ticket) noexcept
{
    if (routeSlot >= mDeferredRouteValues.size() || !token.valid() ||
        ticket >= (UINT64_C(1) << 47u))
        return;
    auto& mailbox = mDeferredRouteValues[routeSlot];
    const uint64_t desired = (ticket << 17u) | UINT64_C(0x10000) |
                             token.value();
    uint64_t current = mailbox.load(std::memory_order_acquire);
    for (unsigned attempt = 0; attempt < 4u; ++attempt)
    {
        if ((current & UINT64_C(0x10000)) != 0u &&
            (current >> 17u) >= ticket)
            return;
        if (mailbox.compare_exchange_weak(
                current, desired, std::memory_order_release,
                std::memory_order_acquire))
            return;
    }
}

bool LXR::PublishRouteHostToken(
    std::size_t routeSlot, lxr::route_host::RouteToken16 token,
    uint64_t ticket) noexcept
{
    const auto id = lxr::route_host::parameterForSlot(routeSlot);
    const auto decoded = lxr::route_host::decode(id, token);
    if (!decoded)
        return RejectRouteHostValue(routeSlot, ticket);

    lxr::RoutingStateAuthority::Guard outer;
    if (!mRoutingState.tryAcquire(outer))
        return false;
    lxr::PersistentRouteRecord current;
    uint64_t routeGeneration = 0;
    uint64_t restoreEpoch = 0;
    if (!mRoutingState.routeLocked(outer, routeSlot, current,
                                   routeGeneration, restoreEpoch))
        return true;
    (void)routeGeneration;
    const auto status = mRoutingState.commitRouteLocked(
        outer, decoded.record, ticket, restoreEpoch);
    if (status == lxr::PersistentRouteStatus::Ok ||
        status == lxr::PersistentRouteStatus::Disabled)
        return true;

    // A restore or newer route already won. Restore the IParam mirror to the
    // accepted authority; its route generation is projected on main-thread idle.
    auto* param = GetParam(static_cast<int>(id.value()));
    if (param != nullptr)
        param->Set(static_cast<double>(lxr::route_host::encode(current).value()));
    return true;
}

bool LXR::RejectRouteHostValue(
    std::size_t routeSlot, uint64_t ticket) noexcept
{
    if (routeSlot >= lxr::route_host::kParameterCount)
        return true;
    lxr::RoutingStateAuthority::Guard outer;
    if (!mRoutingState.tryAcquire(outer))
    {
        auto& pending = mDeferredRouteRejections[routeSlot];
        uint64_t current = pending.load(std::memory_order_acquire);
        for (unsigned attempt = 0; attempt < 4u && current < ticket; ++attempt)
            if (pending.compare_exchange_weak(
                    current, ticket, std::memory_order_release,
                    std::memory_order_acquire))
                break;
        return false;
    }
    lxr::PersistentRouteRecord current;
    uint64_t routeGeneration = 0;
    uint64_t restoreEpoch = 0;
    if (!mRoutingState.routeLocked(outer, routeSlot, current,
                                   routeGeneration, restoreEpoch))
        return true;
    (void)routeGeneration;
    const auto id = lxr::route_host::parameterForSlot(routeSlot);
    auto* param = GetParam(static_cast<int>(id.value()));
    if (param != nullptr)
        param->Set(static_cast<double>(lxr::route_host::encode(current).value()));
    const auto status = mRoutingState.republishRouteLocked(
        outer, routeSlot, ticket, restoreEpoch);
    return status == lxr::PersistentRouteStatus::Ok ||
           status == lxr::PersistentRouteStatus::Stale;
}

// ---------------------------------------------------------
// Host reset callback
// ---------------------------------------------------------

void LXR::OnReset()
{
    ReleaseRollControls();
    // Reset native synthesis and processing state.
    ResetEngineState();

    // P12b-4c:
    // Configure the FIR using the DAW's actual rate.
    //
    // Coefficients are generated here, not inside
    // ProcessBlock().
    //
    // The adapter rejects unsupported sample rates,
    // and ProcessBlock() returns silence if not ready.
    //
    // IMPORTANT:
    // This must not execute concurrently with audio
    // processing because it rewrites the coefficient bank.
    // iPlug2 also calls OnReset from setProcessing(false), potentially on the
    // processing thread. Retain the bank at an unchanged rate. VST3 rate changes
    // arrive via setupProcessing while inactive, before process callbacks.
    const double hostRate = GetSampleRate();
    if (!mAudioAdapter.ready() || mAudioAdapter.hostRate() != hostRate)
    {
        mAudioAdapter.configure(hostRate);
    }

    // Failed configuration invalidates readiness and reports zero latency.
    // SetLatency stores the value and iPlug2 notifies an attached VST3 handler
    // with restartComponent(kLatencyChanged). Avoid redundant restart requests
    // on ordinary transport resets. This adds no physical delay to the audio.
    const int latency = mAudioAdapter.ready()
        ? lxr::firLatency::hostSamples(hostRate) : 0;
    if (GetLatency() != latency)
    {
        SetLatency(latency);
    }
}

// ---------------------------------------------------------
// Host activation/deactivation callback
//
// Required by LXR.h.
//
// Restores the missing function that caused LNK2001.
// ---------------------------------------------------------

void LXR::OnActivate(
    bool active
)
{
    // The original LXR Engine has no distinct
    // activation/deactivation API.
    //
    // Clear all cached audio when deactivated.
    //
    // This avoids replaying stale samples after a
    // transport or plugin lifecycle transition.
    //
    // The configured FIR coefficients are retained.
    if (!active)
    {
        ReleaseRollControls();
        mAudioAdapter.reset();
        mSequencer.reset(0u);
        mAppliedSequencerEnabled = false;
    }
}

// ---------------------------------------------------------
// Host parameter change
// ---------------------------------------------------------

void LXR::OnParamChange(
    int paramIdx
)
{
    if (paramIdx == lxr::vst::kMorphAmountParam)
    {
        const auto* param = GetParam(paramIdx);
        if (param == nullptr) return;
        const int value = param->Int();
        if (value < 0 || value > 255) return;
        const auto ticket =
            mDeferredHostOrder.fetch_add(1u, std::memory_order_acq_rel);
        const auto desired = (ticket << 9u) | 0x100u |
            static_cast<std::uint64_t>(value);
        auto current = mDeferredMorphAmount.load(std::memory_order_acquire);
        for (unsigned attempt = 0;
             attempt < 4u && (current >> 9u) < ticket; ++attempt)
        {
            if (mDeferredMorphAmount.compare_exchange_weak(
                    current, desired, std::memory_order_release,
                    std::memory_order_acquire))
                break;
        }
        return;
    }
    if (paramIdx == lxr::vst::kSequencerRollRateParam ||
        paramIdx == lxr::vst::kSequencerRollTracksParam)
    {
        const auto* param = GetParam(paramIdx);
        if (param == nullptr) return;
        const int value = param->Int();
        const auto slot = static_cast<std::size_t>(paramIdx - lxr::vst::kSequencerRollRateParam);
        if (value < 0 || value > (slot == 0u ? 13 : 127)) return;
        const auto ticket = mDeferredHostOrder.fetch_add(1u, std::memory_order_acq_rel);
        auto& mailbox = mDeferredRollValues[slot];
        const auto desired = (ticket << 9u) | 0x100u | static_cast<uint64_t>(value);
        auto current = mailbox.load(std::memory_order_acquire);
        for (unsigned attempt = 0; attempt < 4u && (current >> 9u) < ticket; ++attempt)
            if (mailbox.compare_exchange_weak(current, desired,
                    std::memory_order_release, std::memory_order_acquire)) break;
        return;
    }
    if (paramIdx == lxr::vst::kSequencerEnableParam ||
        paramIdx == lxr::vst::kSequencerClockModeParam ||
        paramIdx == lxr::vst::kSequencerInternalTempoParam)
    {
        const auto* param = GetParam(paramIdx);
        if (param == nullptr)
            return;
        const int value = param->Int();
        const auto slot = static_cast<std::size_t>(paramIdx - lxr::vst::kSequencerEnableParam);
        if (value < (slot == 2u ? 1 : 0) || value > (slot == 2u ? 255 : 1)) return;
        const auto ticket = mDeferredHostOrder.fetch_add(1, std::memory_order_acq_rel);
        const auto encoded = (ticket << 10u) | 0x200u | static_cast<uint64_t>(value);
        auto& mailbox = mDeferredTransportValues[slot];
        auto current = mailbox.load(std::memory_order_acquire);
        for (unsigned attempt = 0; attempt < 4u && (current >> 10u) < ticket; ++attempt)
            if (mailbox.compare_exchange_weak(current, encoded,
                    std::memory_order_release, std::memory_order_acquire)) break;
        return;
    }

    const auto routeId = lxr::route_host::RouteHostParamId::fromRaw(
        paramIdx >= 0 ? static_cast<std::uint32_t>(paramIdx) : 0u);
    if (routeId.valid())
    {
        const auto routeSlot = lxr::route_host::slot(routeId);
        auto* param = GetParam(paramIdx);
        const uint64_t ticket =
            mDeferredHostOrder.fetch_add(1, std::memory_order_acq_rel);
        if (param == nullptr || ticket == 0u)
            return;
        const auto token = lxr::route_host::RouteToken16::fromRaw(
            static_cast<std::uint32_t>(param->Int()));
        const auto decoded = lxr::route_host::decode(routeId, token);
        if (!decoded)
        {
            RejectRouteHostValue(routeSlot, ticket);
            return;
        }
        if (!PublishRouteHostToken(routeSlot, token, ticket))
            DeferRouteHostToken(routeSlot, token, ticket);
        return;
    }

    // Only IDs 0..227 belong to the native LXR.
    if (paramIdx < 0 ||
        paramIdx >= lxr::vst::kNativeNumParams)
    {
        return;
    }

    auto* param =
        GetParam(paramIdx);

    if (param == nullptr)
    {
        return;
    }

    const uint8_t value = GetRawLXRParameterValue(param);
    lxr::RoutingStateAuthority::Guard outer;
    StateAuthority::WriteHandle writer;
    if (!mRoutingState.tryAcquire(outer) ||
        !mRoutingState.canPublishLocked(outer) ||
        !mState.tryBeginWrite(writer))
    {
        // A restore or another host producer already owns the bounded writer
        // gate. The latest callback value is accepted at the next processing
        // boundary; bit 8 distinguishes value zero from an empty mailbox.
        const uint64_t order =
            mDeferredHostOrder.fetch_add(1, std::memory_order_relaxed);
        auto& mailbox =
            mDeferredHostValues[static_cast<std::size_t>(paramIdx)];
        const uint64_t desired = (order << 9u) | 0x100u | value;
        uint64_t current = mailbox.load(std::memory_order_acquire);
        while ((current & 0x100u) == 0u || (current >> 9u) < order)
        {
            if (mailbox.compare_exchange_weak(
                    current, desired, std::memory_order_release,
                    std::memory_order_acquire))
                break;
        }
        return;
    }

    const std::size_t index = static_cast<std::size_t>(paramIdx);
    if (mState.value(index) == value)
    {
        mState.cancelUnmodified(writer);
        return;
    }
    mState.write(writer, index, value);
    mState.commit(writer);
    mRoutingState.commitPatchLocked(outer);
}

void LXR::OnParamChangeUI(int paramIdx, iplug::EParamSource source)
{
    (void)source;
    // In the non-distributed VST3 adapter, controller/UI changes update IParam
    // and call only OnParamChangeUI(). Route that producer through the same
    // authority as processing-thread automation.
    OnParamChange(paramIdx);
}

// ---------------------------------------------------------
// Main-thread controller projection and host cache invalidation
// ---------------------------------------------------------

#if defined(VST3_API)
Steinberg::tresult PLUGIN_API LXR::setParamNormalized(
    Steinberg::Vst::ParamID tag,
    Steinberg::Vst::ParamValue value)
{
    if (tag >= static_cast<Steinberg::Vst::ParamID>(
            lxr::vst::kSequencerEnableParam) &&
        tag <= static_cast<Steinberg::Vst::ParamID>(
            lxr::vst::kSequencerRollTracksParam))
    {
        if (!std::isfinite(value) || value < 0.0 || value > 1.0 ||
            GetParam(static_cast<int>(tag)) == nullptr)
            return Steinberg::kResultFalse;
        return iplug::Plugin::setParamNormalized(tag, value);
    }

    const auto id = lxr::route_host::RouteHostParamId::fromRaw(tag);
    if (!id.valid())
        return iplug::Plugin::setParamNormalized(tag, value);

    lxr::route_host::RouteToken16 token;
    if (!DecodeNormalizedRouteToken(*this, tag, value, token))
    {
        const uint64_t ticket =
            mDeferredHostOrder.fetch_add(1, std::memory_order_acq_rel);
        if (ticket != 0u)
            RejectRouteHostValue(lxr::route_host::slot(id), ticket);
        return Steinberg::kResultFalse;
    }
    return iplug::Plugin::setParamNormalized(tag, value);
}

Steinberg::tresult PLUGIN_API LXR::getState(Steinberg::IBStream* state)
{
    if (state == nullptr)
        return Steinberg::kInvalidArgument;
    const uint64_t watermark =
        mDeferredHostOrder.load(std::memory_order_acquire) - 1u;
    if (!FlushDeferredHostParameters(watermark))
        return Steinberg::kResultFalse;

    lxr::state::ProjectCandidate candidate;
    uint64_t parameterGeneration = 0;
    lxr::RoutingStateAuthority::Snapshot routing;
    lxr::sequencer::PatternStateAuthority::ReadHandle pattern;
    if (!TryPersistentSnapshot(candidate.patch, parameterGeneration, routing,
                               &pattern, &candidate.transport)) return Steinberg::kResultFalse;
    candidate.routing = routing.state;
    candidate.bank = pattern.bank();
    candidate.kitName = routing.kitName;
    candidate.kitIdentityPresent = routing.kitIdentityPresent;
    lxr::state::ProjectBytes encoded;
    std::size_t encodedSize = 0;
    if (!lxr::state::EncodeProject(candidate, encoded, encodedSize, true))
        return Steinberg::kResultFalse;
    Steinberg::int32 written = 0;
    const auto result = state->write(encoded.data(),
        static_cast<Steinberg::int32>(encodedSize), &written);
    return (result == Steinberg::kResultOk || result == Steinberg::kResultTrue) &&
           written == static_cast<Steinberg::int32>(encodedSize)
        ? Steinberg::kResultOk : Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API LXR::setState(Steinberg::IBStream* state)
{
    if (state == nullptr)
        return Steinberg::kInvalidArgument;

    // Read at most one byte beyond the contract ceiling so oversize streams
    // are rejected without an unbounded allocation.
    std::array<uint8_t, lxr::state::kV3MaximumStreamBytes + 1u> bytes {};
    std::size_t size = 0;
    for (;;)
    {
        const std::size_t remaining = bytes.size() - size;
        if (remaining == 0u)
            return Steinberg::kResultFalse;
        const Steinberg::int32 request = static_cast<Steinberg::int32>(
            remaining > 128u ? 128u : remaining);
        Steinberg::int32 bytesRead = 0;
        const auto readResult = state->read(bytes.data() + size, request,
                                            &bytesRead);
        if (bytesRead < 0 || bytesRead > request)
            return Steinberg::kResultFalse;
        if (bytesRead == 0)
            break;
        size += static_cast<std::size_t>(bytesRead);
        if (size > lxr::state::kV3MaximumStreamBytes)
            return Steinberg::kResultFalse;
        if (readResult != Steinberg::kResultOk &&
            readResult != Steinberg::kResultTrue && GetHost() != iplug::kHostWaveLab)
            return Steinberg::kResultFalse;
    }

    lxr::state::ProjectCandidate candidate;
    if (!lxr::state::DecodeProject(bytes.data(), size, candidate, true) ||
        !PublishPersistentRestore(candidate.patch, candidate.routing,
                                    candidate.kitName,
                                    candidate.kitIdentityPresent,
                                    &candidate.bank, &candidate.transport))
        return Steinberg::kResultFalse;

    mLastKitOperation = {};
    mLastKitOperation.name = candidate.kitName;

    // Update the framework's separate controller cache only after the complete
    // patch+routing+bypass transaction has committed.
    UpdateParams(this, candidate.routing.bypass ? 1 : 0);
    OnRestoreState();
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API LXR::process(Steinberg::Vst::ProcessData& data)
{
    mCallbackMidiCount = 0;
    mCallbackMidiFrames = data.numSamples;
    mCollectCallbackMidi = true;
    mHostProcessContext = {};
    mHostProcessContext.callbackFrames = data.numSamples;
    mHostProcessContext.offline =
        data.processMode == Steinberg::Vst::kOffline;
    const auto* context = data.processContext;
    if (context != nullptr)
    {
        mHostProcessContext.contextPresent = true;
        mHostProcessContext.validFields =
            lxr::sequencer::HostSamplePosition |
            lxr::sequencer::HostTransportState;
        mHostProcessContext.sampleRate = context->sampleRate;
        mHostProcessContext.projectSamplePosition =
            context->projectTimeSamples;
        mHostProcessContext.playing =
            (context->state & Steinberg::Vst::ProcessContext::kPlaying) != 0;
        mHostProcessContext.cycleActive =
            (context->state & Steinberg::Vst::ProcessContext::kCycleActive) != 0;
        if ((context->state &
             Steinberg::Vst::ProcessContext::kProjectTimeMusicValid) != 0)
        {
            mHostProcessContext.validFields |=
                lxr::sequencer::HostPpqPosition;
            mHostProcessContext.projectPpqPosition =
                context->projectTimeMusic;
        }
        if ((context->state & Steinberg::Vst::ProcessContext::kTempoValid) != 0)
        {
            mHostProcessContext.validFields |= lxr::sequencer::HostTempo;
            mHostProcessContext.tempoBpm = context->tempo;
        }
        if ((context->state & Steinberg::Vst::ProcessContext::kCycleValid) != 0)
        {
            mHostProcessContext.validFields |= lxr::sequencer::HostCycle;
            mHostProcessContext.cycleStartPpq = context->cycleStartMusic;
            mHostProcessContext.cycleEndPpq = context->cycleEndMusic;
        }
        if ((context->state & Steinberg::Vst::ProcessContext::kTimeSigValid) != 0)
        {
            mHostProcessContext.validFields |=
                lxr::sequencer::HostTimeSignature;
            mHostProcessContext.timeSigNumerator = context->timeSigNumerator;
            mHostProcessContext.timeSigDenominator = context->timeSigDenominator;
        }
        if ((context->state & Steinberg::Vst::ProcessContext::kBarPositionValid) != 0)
        {
            mHostProcessContext.validFields |=
                lxr::sequencer::HostBarPosition;
            mHostProcessContext.barPositionPpq = context->barPositionMusic;
        }
    }

    MidiParameterChanges changes(*this, data.inputParameterChanges,
        mRejectedParameterQueues, mRejectedMidiInputRecords, mMalformedMidiInputCounts);
    BoundedMidiEvents events(data.inputEvents, mRejectedMidiInputRecords, mMalformedMidiInputCounts);
    // The pinned dispatcher reads only the last point. Preflight that exact
    // point so rejected route automation republishes canonical readback while
    // the adapter prevents the invalid value from reaching IParam.
    const auto queueCount = changes.sourceQueueCount();
    for (Steinberg::int32 i = 0; i < queueCount; ++i)
    {
        auto* queue = changes.sourceQueue(i);
        if (queue == nullptr)
            continue;
        const auto id = lxr::route_host::RouteHostParamId::fromRaw(
            queue->getParameterId());
        const auto pointCount = queue->getPointCount();
        if (!id.valid() || pointCount <= 0)
            continue;
        Steinberg::int32 offset = 0;
        Steinberg::Vst::ParamValue value = 0.0;
        lxr::route_host::RouteToken16 token;
        if (queue->getPoint(pointCount - 1, offset, value) !=
                Steinberg::kResultOk ||
            !DecodeNormalizedRouteToken(*this, id.value(), value, token))
        {
            const uint64_t ticket =
                mDeferredHostOrder.fetch_add(1, std::memory_order_acq_rel);
            if (ticket != 0u)
                RejectRouteHostValue(lxr::route_host::slot(id), ticket);
        }
    }
    auto adapted = data;
    adapted.inputParameterChanges = &changes;
    adapted.inputEvents = data.inputEvents != nullptr ? &events : nullptr;
    const auto result = iplug::Plugin::process(adapted);
    FlushCallbackMidi();
    return result;
}

Steinberg::tresult PLUGIN_API LXR::setComponentHandler(
    Steinberg::Vst::IComponentHandler* handler)
{
    const auto result = iplug::Plugin::setComponentHandler(handler);
    if (result == Steinberg::kResultOk)
    {
        ++mControllerConnection;
        mControllerNeedsReconcile = true;
    }
    return result;
}
#endif

void LXR::OnIdle()
{
#if IPLUG_EDITOR
    RefreshKitManagementControls();
    RefreshMorphManagementControls();
    RefreshGUIReadback();
#endif
    if (mRollReleasePending.load(std::memory_order_acquire) &&
        !FlushDeferredHostParameters()) return;
#if defined(VST3_API)
    // The installed adapter is SingleComponentEffect: processor and controller
    // share this object, but not their parameter storage. Only the main thread
    // touches the VST3 ParameterContainer or calls the component handler.
    if (mControllerSyncBusy)
        return;

    lxr::RoutingStateAuthority::Guard outer;
    StateAuthority::WriteHandle projection;
    lxr::morph::Snapshot morph;
    if (!mMorphState.trySnapshot(morph, 1u))
        return;
    if (!mRoutingState.tryAcquire(outer) ||
        !mState.tryBeginWrite(projection))
        return; // Retain every unacknowledged generation for the next idle.

    mControllerSyncBusy = true;
    auto delivered = mControllerSyncGenerations;
    lxr::RoutingStateAuthority::Snapshot routeSnapshot;
    if (!mRoutingState.copyLocked(outer, routeSnapshot))
    {
        mState.cancelUnmodified(projection);
        mControllerSyncBusy = false;
        return;
    }
    bool pending = mControllerNeedsReconcile;
    bool complete = true;
    for (std::size_t i = 0; i < delivered.size(); ++i)
    {
        const bool native = i < lxr::state::kParameterCount;
        const bool morphAmount =
            i == static_cast<std::size_t>(lxr::vst::kMorphAmountParam);
        const bool transport =
            i >= lxr::vst::kPersistentNumParams && !morphAmount;
        const std::size_t routeSlot = native || transport || morphAmount
            ? 0u : i - lxr::state::kParameterCount;
        const uint64_t generation = native ? mState.valueGeneration(i)
            : morphAmount ? morph.generation
            : transport ? routeSnapshot.generation
            : routeSnapshot.routeGenerations[routeSlot];
        const double transportValue = i == lxr::vst::kSequencerEnableParam ? mTransportConfig.enabled
            : i == lxr::vst::kSequencerClockModeParam ? mTransportConfig.clock
            : i == lxr::vst::kSequencerInternalTempoParam ? mTransportConfig.bpm
            : i == lxr::vst::kSequencerRollRateParam ? mRollControls.rate : mRollControls.tracks;
        if (!mControllerNeedsReconcile && delivered[i] == generation)
            continue;

        const auto* param = GetParam(static_cast<int>(i));
        // Hold the existing bounded writer gate only during projection. A newer
        // producer cannot slip between generation validation and cache update.
        // Do not write IParam or call OnParamChange here (no feedback producer).
        complete &= param != nullptr && SetVST3ParamNormalized(
            static_cast<Steinberg::Vst::ParamID>(i),
            param->ToNormalized(static_cast<double>(native
                ? mState.value(i)
                : morphAmount ? morph.amount
                : transport ? transportValue : lxr::route_host::encode(
                    routeSnapshot.state.routes[routeSlot]).value())));
        delivered[i] = generation;
        pending = true;
    }
    mState.cancelUnmodified(projection);
    mRoutingState.release(outer);

    // Release the gate before any host callback: it may synchronously read the
    // cache, restore state, or deliver automation. Newer generations then remain
    // different from delivered[] and are sent on the next idle. No gestures:
    // MIDI must not create a second DAW automation recording of the same edit.
    const uint64_t connection = mControllerConnection;
    Steinberg::IPtr<Steinberg::Vst::IComponentHandler> handler = GetComponentHandler();
    if (pending && complete && handler &&
        handler->restartComponent(Steinberg::Vst::kParamValuesChanged) ==
            Steinberg::kResultOk && connection == mControllerConnection)
    {
        mControllerSyncGenerations = delivered;
        mControllerNeedsReconcile = false;
    }
    mControllerSyncBusy = false;
#endif
}

// ---------------------------------------------------------
// MIDI input
// ---------------------------------------------------------

bool LXR::EnqueueDeferredMidi(const iplug::IMidiMsg& msg) noexcept
{
    if (!mDeferredMidi.push(msg))
    {
        mDeferredMidiOverflows.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

void LXR::FlushCallbackMidi()
{
    mCollectCallbackMidi = false;
    // The framework skips ProcessBlock while bypassed. Keep accepting MIDI
    // state changes there, and in zero-audio callbacks, as before this repair.
    std::sort(mCallbackMidi.begin(), mCallbackMidi.begin() + mCallbackMidiCount,
        [](const CallbackMidi& a, const CallbackMidi& b) noexcept {
            return a.message.mOffset < b.message.mOffset ||
                (a.message.mOffset == b.message.mOffset && a.sequence < b.sequence);
        });
    for (std::size_t i = 0; i < mCallbackMidiCount; ++i)
        ProcessMidiMsg(mCallbackMidi[i].message);
    mCallbackMidiCount = 0;
}

void LXR::DrainDeferredMidi()
{
    while (auto* msg = mDeferredMidi.front())
    {
        if (!ProcessMidiMsgNow(*msg))
            return;
        mDeferredMidi.pop();
    }
}

void LXR::ProcessMidiMsg(const iplug::IMidiMsg& msg)
{
    if (mCollectCallbackMidi)
    {
        // Zero-frame VST3 flush calls may deliver offset zero. Other offsets
        // must belong to this callback; never pull an invalid future event in.
        if (msg.mOffset < 0 ||
            (msg.mOffset >= mCallbackMidiFrames &&
             !(mCallbackMidiFrames == 0 && msg.mOffset == 0)))
        {
            mInvalidMidiOffsets.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        if (mCallbackMidiCount == mCallbackMidi.size())
        {
            mDeferredMidiOverflows.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const auto sequence = static_cast<std::uint32_t>(mCallbackMidiCount);
        mCallbackMidi[mCallbackMidiCount++] = {msg, sequence};
        mCallbackMidiPeak = std::max(mCallbackMidiPeak, static_cast<std::uint32_t>(mCallbackMidiCount));
        return;
    }

    if (!ApplyPendingStateAndParameters())
    {
        EnqueueDeferredMidi(msg);
        return;
    }

    DrainDeferredMidi();
    if (!mDeferredMidi.empty() || !ProcessMidiMsgNow(msg))
        EnqueueDeferredMidi(msg);
}

bool LXR::ProcessMidiMsgNow(const iplug::IMidiMsg& msg)
{

    // Never trigger synthesis without valid assets.
    if (!mAssetsLoaded)
    {
        return true;
    }

    const auto statusMsg =
        msg.StatusMsg();

    if (statusMsg == iplug::IMidiMsg::kNone)
    {
        return true;
    }

    // iPlug uses zero-based MIDI channels.
    const int channel =
        msg.Channel();

    if (channel < 0 ||
        channel > 15)
    {
        return true;
    }

    // Translate iPlug's message type to the native
    // MIDI status representation.
    const uint8_t status =
        static_cast<uint8_t>(
            static_cast<uint8_t>(statusMsg) << 4
        );

    const uint8_t data1 =
        static_cast<uint8_t>(
            msg.mData1 & 0x7F
        );

    const uint8_t data2 =
        static_cast<uint8_t>(
            msg.mData2 & 0x7F
        );

    lxr::MidiEvent event {};

    // Preserve the MIDI message type and channel.
    event.status = static_cast<uint8_t>(
        status | static_cast<uint8_t>(channel)
    );

    event.channel =
        static_cast<uint8_t>(channel);

    event.data1 = data1;
    event.data2 = data2;

    const bool mayChangeParameters = (event.status & 0xF0u) == 0xB0u;
    lxr::RoutingStateAuthority::Guard outer;
    StateAuthority::WriteHandle writer;
    if (mayChangeParameters &&
        (!mRoutingState.tryAcquire(outer) ||
         !mRoutingState.canPublishLocked(outer) ||
         !mState.tryBeginWrite(writer)))
        return false;

    std::array<uint8_t, lxr::state::kParameterCount> valuesBefore {};
    if (mayChangeParameters)
    {
        for (std::size_t i = 1; i < valuesBefore.size(); ++i)
        {
            valuesBefore[i] = mParams.get(static_cast<uint8_t>(i));
        }
    }

    // Preserve original native MIDI dispatch.
    mMidiParser.processEvent(
        event,
        mEngine,
        mParams,
        &mSequencer
    );
    ++mDispatchedMidiMessages;

    if (mayChangeParameters)
    {
        for (std::size_t i = 1; i < valuesBefore.size(); ++i)
        {
            const uint8_t value = mParams.get(static_cast<uint8_t>(i));
            if (value == valuesBefore[i])
                continue;

            mState.write(writer, i, value);

            auto* param = GetParam(static_cast<int>(i));
            if (param != nullptr)
            {
                param->Set(static_cast<double>(value));
            }
        }

        const uint64_t generation = mState.commit(writer);
        const uint64_t jointGeneration =
            mRoutingState.commitPatchLocked(outer);
        mAppliedStateGeneration = generation;
        mAppliedJointGeneration = jointGeneration;
    }
    return true;
}

// ---------------------------------------------------------
// Audio processing callback
//
// P12b-4c:
// Convert native LXR audio into host-rate output.
//
// The adapter owns:
//   - native block buffering
//   - PCM-to-double conversion
//   - polyphase FIR resampling
//   - fractional source position
//   - filter history
//
// This callback performs no allocations or file I/O.
// ---------------------------------------------------------

void LXR::ApplySequencerTransport(int callbackFrames) noexcept
{
    const bool enabled =
        mAudioTransport.enabled != 0u;
    const std::uint8_t mode =
        mAudioTransport.clock;
    const std::uint16_t internalTempo =
        mAudioTransport.bpm;
    const std::uint64_t nativeFrame = mAudioAdapter.nativeFrame();

    if (!enabled)
    {
        if (mAppliedRollMask != 0u || (mAudioRollControls.tracks != 0u &&
            mAudioRollControls.pressTicket > mRollReleaseCutoff.load(std::memory_order_acquire)))
            ReleaseRollControls();
        if (mAppliedSequencerEnabled)
        {
            (void)mEngine.clearSequencerAutomation();
            mSequencer.reset(nativeFrame);
        }
        mAppliedSequencerEnabled = false;
        mAppliedSequencerClockMode = mode;
        mAppliedSequencerInternalTempo = internalTempo;
        return;
    }

    const bool changedMode = !mAppliedSequencerEnabled ||
        mAppliedSequencerClockMode != mode;
    if (changedMode)
    {
        if (mAppliedSequencerEnabled) ReleaseRollControls();
        mAppliedRollMask = 0u;
        (void)mEngine.clearSequencerAutomation();
        mSequencer.reset(nativeFrame);
        mSequencer.resetSeed(mAudioTransport.seed);
        (void)mSequencer.selectPattern(mAudioTransport.pattern);
    }

    mAppliedSequencerEnabled = true;
    mAppliedSequencerClockMode = mode;
    mAppliedSequencerInternalTempo = internalTempo;

    if (mode == 0u)
    {
        (void)mSequencer.setTempo(internalTempo);
        (void)mEngine.setNativeTempo(internalTempo);
        if (!mSequencer.running())
            (void)mSequencer.start();
        return;
    }

    auto host = mHostProcessContext;
    host.callbackFrames = callbackFrames;

    if ((host.validFields & lxr::sequencer::HostTempo) != 0u &&
        std::isfinite(host.tempoBpm) && host.tempoBpm >= 1.0 &&
        host.tempoBpm <= 255.0)
    {
        (void)mEngine.setNativeTempo(static_cast<std::uint16_t>(
            std::floor(host.tempoBpm + 0.5)));
    }
    const auto decision = mSequencer.beginHostCallback(host, nativeFrame, mEngine);
    if (decision.transition == lxr::sequencer::HostTransition::Stop ||
        decision.transition == lxr::sequencer::HostTransition::InvalidContext ||
        (decision.transition == lxr::sequencer::HostTransition::Seek && !host.playing))
        ReleaseRollControls();
}

void LXR::ReleaseRollControls() noexcept
{
    const auto cutoff = mDeferredHostOrder.fetch_add(1u, std::memory_order_acq_rel);
    mRollReleaseCutoff.store(cutoff, std::memory_order_release);
    mRollReleasePending.store(true, std::memory_order_release);
    mSequencer.cancelRolls();
    mAppliedRollMask = 0u;
}

void LXR::ApplyRollControls() noexcept
{
    if (!mAppliedSequencerEnabled) return;
    (void)mSequencer.setRollRate(mAudioRollControls.rate);
    const auto desired = mAudioRollControls.pressTicket >
        mRollReleaseCutoff.load(std::memory_order_acquire)
        ? mAudioRollControls.tracks : static_cast<std::uint8_t>(0u);
    for (std::uint8_t track = 0u; track < 7u; ++track)
    {
        const auto bit = static_cast<std::uint8_t>(1u << track);
        if (((desired ^ mAppliedRollMask) & bit) != 0u)
            (void)mSequencer.setRoll(track, (desired & bit) != 0u);
    }
    mAppliedRollMask = desired;
}

void LXR::ProcessBlock(
    double** inputs,
    double** outputs,
    int nFrames
)
{
    // LXR currently operates as an instrument.
    (void)inputs;

    mCollectCallbackMidi = false;
    // iPlug2 delivers synthetic CC parameter queues before the note-event
    // list, so arrival order need not be timestamp order. std::sort operates
    // in resident storage; the explicit sequence preserves simultaneous order.
    std::sort(mCallbackMidi.begin(), mCallbackMidi.begin() + mCallbackMidiCount,
        [](const CallbackMidi& a, const CallbackMidi& b) noexcept {
            return a.message.mOffset < b.message.mOffset ||
                (a.message.mOffset == b.message.mOffset && a.sequence < b.sequence);
        });

    if (ApplyPendingStateAndParameters())
        DrainDeferredMidi();

    ApplySequencerTransport(nFrames);
    ApplyRollControls();
    PublishGUIReadback();

    // Flush/no-output calls have no sample loop. Retain their MIDI/state
    // publication behavior without attempting native or FIR processing.
    if (outputs == nullptr || nFrames <= 0 || NOutChansConnected() <= 0 ||
        !mAssetsLoaded || !mAudioAdapter.ready())
        FlushCallbackMidi();

    if (outputs == nullptr ||
        nFrames <= 0)
    {
        return;
    }

    const int nChans =
        NOutChansConnected();

    if (nChans <= 0)
    {
        return;
    }

    // Protect the native DSP from missing assets.
    if (!mAssetsLoaded)
    {
        ClearAudioOutputs(
            outputs,
            nChans,
            nFrames
        );

        return;
    }

    // Never process with an unconfigured FIR.
    if (!mAudioAdapter.ready())
    {
        ClearAudioOutputs(
            outputs,
            nChans,
            nFrames
        );

        return;
    }

    // Generate exactly the number of host frames
    // requested by the current DAW callback.
    //
    // The fractional source position and FIR history
    // persist across arbitrary callback boundaries.
    std::size_t midiIndex = 0;
    for (int sample = 0;
         sample < nFrames;
         ++sample)
    {
        while (midiIndex < mCallbackMidiCount &&
               mCallbackMidi[midiIndex].message.mOffset <= sample)
        {
            ProcessMidiMsg(mCallbackMidi[midiIndex++].message);
        }
        double left = 0.0;
        double right = 0.0;

        // Render one host-rate stereo sample.
        mAudioAdapter.getNextHostST1Sample(
            mEngine,
            left,
            right,
            &mSequencer,
            sample
        );

        // Mono host configuration.
        if (nChans == 1)
        {
            if (outputs[0] != nullptr)
            {
                outputs[0][sample] =
                    0.5 * (left + right);
            }

            continue;
        }

        // Main stereo output: ST1.
        if (outputs[0] != nullptr)
        {
            outputs[0][sample] = left;
        }

        if (outputs[1] != nullptr)
        {
            outputs[1][sample] = right;
        }

        // ST2 remains internal for this phase.
        // Keep all additional host channels silent.
        for (int channel = 2;
             channel < nChans;
             ++channel)
        {
            if (outputs[channel] != nullptr)
            {
                outputs[channel][sample] = 0.0;
            }
        }
    }
    mCallbackMidiCount = 0;
}
