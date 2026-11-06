#pragma once

#include "ModulationContract.h"

#include <cstdint>

namespace lxr::sequencer
{

inline constexpr std::uint8_t kFrontPanelAutomationOff = 0u;
inline constexpr std::uint8_t kFirmwareAutomationSentinel = 0xffu;

enum class AutomationAction : std::uint8_t
{
    Apply = 0,
    Clear = 1
};

enum class AutomationValidationStatus : std::uint8_t
{
    Ok = 0,
    Clear,
    InvalidValue,
    ModWheelNoOp,
    UnrepresentableParameter,
    NotOriginalTarget,
    UnsupportedRuntimeTarget
};

struct AutomationSlotCommand
{
    AutomationAction action = AutomationAction::Clear;
    modulation_contract::HostParameterId target {};
    modulation_contract::SevenBitValue value {};
};

struct AutomationDecodeResult
{
    AutomationValidationStatus status =
        AutomationValidationStatus::UnrepresentableParameter;
    AutomationSlotCommand command {};

    constexpr explicit operator bool() const noexcept
    {
        return status == AutomationValidationStatus::Ok ||
               status == AutomationValidationStatus::Clear;
    }
};

constexpr bool IsCurrentAutomationRuntimeTarget(std::uint16_t parameter) noexcept
{
    // ParameterArray intentionally retains these original protocol/reserved,
    // mutable-destination and note-filter bytes as no-op patch storage.
    if(parameter==0u || parameter==5u || parameter==97u ||
       parameter==98u || parameter==127u || parameter>=221u)
        return false;
    if((parameter>=149u && parameter<=154u) ||
       (parameter>=161u && parameter<=172u))
        return false;
    return parameter<modulation_contract::kSoundSlotCount;
}

// The stored byte is the original automation/MIDI selector, not a VST3 ID.
// Lower parameters use their direct CC number (parameter + 1); upper
// parameters use the firmware parameter number unchanged.
constexpr AutomationDecodeResult DecodeAutomationSlot(
    std::uint8_t storedSelector,
    std::uint8_t storedValue) noexcept
{
    using namespace modulation_contract;
    if (storedValue > 127u)
        return {AutomationValidationStatus::InvalidValue, {}};
    if (storedSelector == kFrontPanelAutomationOff ||
        storedSelector == kFirmwareAutomationSentinel)
    {
        return {AutomationValidationStatus::Clear,
                {AutomationAction::Clear, {}, SevenBitValue::fromRaw(storedValue)}};
    }
    if (storedSelector == 1u)
        return {AutomationValidationStatus::ModWheelNoOp, {}};

    const std::uint16_t rawParameter = storedSelector < 128u
        ? static_cast<std::uint16_t>(storedSelector - 1u)
        : static_cast<std::uint16_t>(storedSelector);
    const HostParameterId host = HostParameterId::fromRaw(rawParameter);
    if (!host.valid())
        return {AutomationValidationStatus::UnrepresentableParameter, {}};

    const TargetMetadata* metadata =
        lookupFirmwareParameter(firmwareParameter(host));
    if (metadata == nullptr)
        return {AutomationValidationStatus::NotOriginalTarget, {}};
    if (metadata->runtimePolicy != RuntimePolicy::TypedField ||
        metadata->midi != OriginalMidiDisposition::DirectStm32 ||
        !IsCurrentAutomationRuntimeTarget(rawParameter))
    {
        return {AutomationValidationStatus::UnsupportedRuntimeTarget, {}};
    }

    return {AutomationValidationStatus::Ok,
            {AutomationAction::Apply, host,
             SevenBitValue::fromRaw(storedValue)}};
}

} // namespace lxr::sequencer
