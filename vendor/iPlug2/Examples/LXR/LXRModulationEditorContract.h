#pragma once

// P19-D production Modulation editor contract.
//
// This file describes existing host parameters only. It does not introduce a
// modulation authority, expose transient effective DSP values, or reinterpret
// the original selector bytes as menu indices. Route host parameters retain
// their full-width original LXR v0.37 target identities.

// Included by LXRGUIFoundation.h after VoiceOwner, geometry constants, the
// route contract and standard fixed-size headers are available, inside
// namespace lxr::gui.

enum class ModulationRole : std::uint8_t
{
    VelocityAmount = 0,
    VelocityDestination,
    LfoWaveform,
    LfoRate,
    LfoAmount,
    LfoDestination,
    LfoRetrigger,
    LfoSync,
    LfoPhaseOffset,
    Count
};

enum class ModulationDomain : std::uint8_t
{
    IntegerByte = 0,
    LfoWaveform,
    Retrigger,
    Sync,
    TypedRoute
};

struct ModulationParameterBinding final
{
    int parameter = -1;
    int originalParameter = -1;
    int originalVoiceHintParameter = -1;
    const char* controlId = nullptr;
    const char* label = nullptr;
    VoiceOwner owner = VoiceOwner::Count;
    ModulationRole role = ModulationRole::VelocityAmount;
    ModulationDomain domain = ModulationDomain::IntegerByte;
    int minimum = 0;
    int maximum = 127;
    int defaultValue = 0;
    bool automatable = true;
    bool persistent = true;
};

inline constexpr std::array<const char*, 9> kModulationRoleNames {{
    "Velocity Amount", "Velocity Destination", "LFO Waveform", "LFO Rate",
    "LFO Amount", "LFO Destination", "LFO Retrigger", "LFO Sync",
    "LFO Phase Offset"
}};

// The three-character labels are the stored-identity order from the original
// AVR MenuText.h lfoWaveNames table. The long labels are presentation only.
inline constexpr std::array<const char*, 8> kLfoWaveformFirmwareLabels {{
    "sin", "tri", "sup", "sdn", "sqr", "rnd", "xup", "xdn"
}};

inline constexpr std::array<const char*, 8> kLfoWaveformLabels {{
    "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H Random",
    "Exponential Up", "Exponential Down"
}};

inline constexpr std::array<const char*, 7> kLfoRetriggerLabels {{
    "Off", "Drum 1", "Drum 2", "Drum 3", "Snare", "Cymbal", "Hi-hat"
}};

inline constexpr std::array<const char*, 12> kLfoSyncLabels {{
    "Off", "4/1", "2/1", "1/1", "1/2", "1/3", "1/4", "1/6",
    "1/8", "1/12", "1/16", "1/32"
}};

// Nine controls for each of the six physical synthesis owners. Native patch
// rows keep their original parameter identity. The two typed route rows point
// back to the original source-owned selector/hint bytes for provenance while
// editing only the accepted host routing projections 228..239.
inline constexpr std::array<ModulationParameterBinding, 54>
kModulationBindings {{
    {143,143,-1,"lxr.param.143","Velocity Amount",VoiceOwner::Drum1,ModulationRole::VelocityAmount,ModulationDomain::IntegerByte,0,127,0,true,true},
    {228,149,-1,"lxr.param.228","Velocity Destination",VoiceOwner::Drum1,ModulationRole::VelocityDestination,ModulationDomain::TypedRoute,0,65535,0,true,true},
    {155,155,-1,"lxr.param.155","LFO Waveform",VoiceOwner::Drum1,ModulationRole::LfoWaveform,ModulationDomain::LfoWaveform,0,127,0,true,true},
    {115,115,-1,"lxr.param.115","LFO Rate",VoiceOwner::Drum1,ModulationRole::LfoRate,ModulationDomain::IntegerByte,0,127,64,true,true},
    {121,121,-1,"lxr.param.121","LFO Amount",VoiceOwner::Drum1,ModulationRole::LfoAmount,ModulationDomain::IntegerByte,0,127,0,true,true},
    {234,167,161,"lxr.param.234","LFO Destination",VoiceOwner::Drum1,ModulationRole::LfoDestination,ModulationDomain::TypedRoute,0,65535,0,true,true},
    {173,173,-1,"lxr.param.173","LFO Retrigger",VoiceOwner::Drum1,ModulationRole::LfoRetrigger,ModulationDomain::Retrigger,0,127,0,true,true},
    {179,179,-1,"lxr.param.179","LFO Sync",VoiceOwner::Drum1,ModulationRole::LfoSync,ModulationDomain::Sync,0,127,0,true,true},
    {185,185,-1,"lxr.param.185","LFO Phase Offset",VoiceOwner::Drum1,ModulationRole::LfoPhaseOffset,ModulationDomain::IntegerByte,0,127,0,true,true},

    {144,144,-1,"lxr.param.144","Velocity Amount",VoiceOwner::Drum2,ModulationRole::VelocityAmount,ModulationDomain::IntegerByte,0,127,0,true,true},
    {229,150,-1,"lxr.param.229","Velocity Destination",VoiceOwner::Drum2,ModulationRole::VelocityDestination,ModulationDomain::TypedRoute,0,65535,0,true,true},
    {156,156,-1,"lxr.param.156","LFO Waveform",VoiceOwner::Drum2,ModulationRole::LfoWaveform,ModulationDomain::LfoWaveform,0,127,0,true,true},
    {116,116,-1,"lxr.param.116","LFO Rate",VoiceOwner::Drum2,ModulationRole::LfoRate,ModulationDomain::IntegerByte,0,127,64,true,true},
    {122,122,-1,"lxr.param.122","LFO Amount",VoiceOwner::Drum2,ModulationRole::LfoAmount,ModulationDomain::IntegerByte,0,127,0,true,true},
    {235,168,162,"lxr.param.235","LFO Destination",VoiceOwner::Drum2,ModulationRole::LfoDestination,ModulationDomain::TypedRoute,0,65535,0,true,true},
    {174,174,-1,"lxr.param.174","LFO Retrigger",VoiceOwner::Drum2,ModulationRole::LfoRetrigger,ModulationDomain::Retrigger,0,127,0,true,true},
    {180,180,-1,"lxr.param.180","LFO Sync",VoiceOwner::Drum2,ModulationRole::LfoSync,ModulationDomain::Sync,0,127,0,true,true},
    {186,186,-1,"lxr.param.186","LFO Phase Offset",VoiceOwner::Drum2,ModulationRole::LfoPhaseOffset,ModulationDomain::IntegerByte,0,127,0,true,true},

    {145,145,-1,"lxr.param.145","Velocity Amount",VoiceOwner::Drum3,ModulationRole::VelocityAmount,ModulationDomain::IntegerByte,0,127,0,true,true},
    {230,151,-1,"lxr.param.230","Velocity Destination",VoiceOwner::Drum3,ModulationRole::VelocityDestination,ModulationDomain::TypedRoute,0,65535,0,true,true},
    {157,157,-1,"lxr.param.157","LFO Waveform",VoiceOwner::Drum3,ModulationRole::LfoWaveform,ModulationDomain::LfoWaveform,0,127,0,true,true},
    {117,117,-1,"lxr.param.117","LFO Rate",VoiceOwner::Drum3,ModulationRole::LfoRate,ModulationDomain::IntegerByte,0,127,64,true,true},
    {123,123,-1,"lxr.param.123","LFO Amount",VoiceOwner::Drum3,ModulationRole::LfoAmount,ModulationDomain::IntegerByte,0,127,0,true,true},
    {236,169,163,"lxr.param.236","LFO Destination",VoiceOwner::Drum3,ModulationRole::LfoDestination,ModulationDomain::TypedRoute,0,65535,0,true,true},
    {175,175,-1,"lxr.param.175","LFO Retrigger",VoiceOwner::Drum3,ModulationRole::LfoRetrigger,ModulationDomain::Retrigger,0,127,0,true,true},
    {181,181,-1,"lxr.param.181","LFO Sync",VoiceOwner::Drum3,ModulationRole::LfoSync,ModulationDomain::Sync,0,127,0,true,true},
    {187,187,-1,"lxr.param.187","LFO Phase Offset",VoiceOwner::Drum3,ModulationRole::LfoPhaseOffset,ModulationDomain::IntegerByte,0,127,0,true,true},

    {146,146,-1,"lxr.param.146","Velocity Amount",VoiceOwner::Snare,ModulationRole::VelocityAmount,ModulationDomain::IntegerByte,0,127,0,true,true},
    {231,152,-1,"lxr.param.231","Velocity Destination",VoiceOwner::Snare,ModulationRole::VelocityDestination,ModulationDomain::TypedRoute,0,65535,0,true,true},
    {158,158,-1,"lxr.param.158","LFO Waveform",VoiceOwner::Snare,ModulationRole::LfoWaveform,ModulationDomain::LfoWaveform,0,127,0,true,true},
    {118,118,-1,"lxr.param.118","LFO Rate",VoiceOwner::Snare,ModulationRole::LfoRate,ModulationDomain::IntegerByte,0,127,64,true,true},
    {124,124,-1,"lxr.param.124","LFO Amount",VoiceOwner::Snare,ModulationRole::LfoAmount,ModulationDomain::IntegerByte,0,127,0,true,true},
    {237,170,164,"lxr.param.237","LFO Destination",VoiceOwner::Snare,ModulationRole::LfoDestination,ModulationDomain::TypedRoute,0,65535,0,true,true},
    {176,176,-1,"lxr.param.176","LFO Retrigger",VoiceOwner::Snare,ModulationRole::LfoRetrigger,ModulationDomain::Retrigger,0,127,0,true,true},
    {182,182,-1,"lxr.param.182","LFO Sync",VoiceOwner::Snare,ModulationRole::LfoSync,ModulationDomain::Sync,0,127,0,true,true},
    {188,188,-1,"lxr.param.188","LFO Phase Offset",VoiceOwner::Snare,ModulationRole::LfoPhaseOffset,ModulationDomain::IntegerByte,0,127,0,true,true},

    {147,147,-1,"lxr.param.147","Velocity Amount",VoiceOwner::Cymbal,ModulationRole::VelocityAmount,ModulationDomain::IntegerByte,0,127,0,true,true},
    {232,153,-1,"lxr.param.232","Velocity Destination",VoiceOwner::Cymbal,ModulationRole::VelocityDestination,ModulationDomain::TypedRoute,0,65535,0,true,true},
    {159,159,-1,"lxr.param.159","LFO Waveform",VoiceOwner::Cymbal,ModulationRole::LfoWaveform,ModulationDomain::LfoWaveform,0,127,0,true,true},
    {119,119,-1,"lxr.param.119","LFO Rate",VoiceOwner::Cymbal,ModulationRole::LfoRate,ModulationDomain::IntegerByte,0,127,64,true,true},
    {125,125,-1,"lxr.param.125","LFO Amount",VoiceOwner::Cymbal,ModulationRole::LfoAmount,ModulationDomain::IntegerByte,0,127,0,true,true},
    {238,171,165,"lxr.param.238","LFO Destination",VoiceOwner::Cymbal,ModulationRole::LfoDestination,ModulationDomain::TypedRoute,0,65535,0,true,true},
    {177,177,-1,"lxr.param.177","LFO Retrigger",VoiceOwner::Cymbal,ModulationRole::LfoRetrigger,ModulationDomain::Retrigger,0,127,0,true,true},
    {183,183,-1,"lxr.param.183","LFO Sync",VoiceOwner::Cymbal,ModulationRole::LfoSync,ModulationDomain::Sync,0,127,0,true,true},
    {189,189,-1,"lxr.param.189","LFO Phase Offset",VoiceOwner::Cymbal,ModulationRole::LfoPhaseOffset,ModulationDomain::IntegerByte,0,127,0,true,true},

    {148,148,-1,"lxr.param.148","Velocity Amount",VoiceOwner::HiHat,ModulationRole::VelocityAmount,ModulationDomain::IntegerByte,0,127,0,true,true},
    {233,154,-1,"lxr.param.233","Velocity Destination",VoiceOwner::HiHat,ModulationRole::VelocityDestination,ModulationDomain::TypedRoute,0,65535,0,true,true},
    {160,160,-1,"lxr.param.160","LFO Waveform",VoiceOwner::HiHat,ModulationRole::LfoWaveform,ModulationDomain::LfoWaveform,0,127,0,true,true},
    {120,120,-1,"lxr.param.120","LFO Rate",VoiceOwner::HiHat,ModulationRole::LfoRate,ModulationDomain::IntegerByte,0,127,64,true,true},
    {126,126,-1,"lxr.param.126","LFO Amount",VoiceOwner::HiHat,ModulationRole::LfoAmount,ModulationDomain::IntegerByte,0,127,0,true,true},
    {239,172,166,"lxr.param.239","LFO Destination",VoiceOwner::HiHat,ModulationRole::LfoDestination,ModulationDomain::TypedRoute,0,65535,0,true,true},
    {178,178,-1,"lxr.param.178","LFO Retrigger",VoiceOwner::HiHat,ModulationRole::LfoRetrigger,ModulationDomain::Retrigger,0,127,0,true,true},
    {184,184,-1,"lxr.param.184","LFO Sync",VoiceOwner::HiHat,ModulationRole::LfoSync,ModulationDomain::Sync,0,127,0,true,true},
    {190,190,-1,"lxr.param.190","LFO Phase Offset",VoiceOwner::HiHat,ModulationRole::LfoPhaseOffset,ModulationDomain::IntegerByte,0,127,0,true,true}
}};

struct ModulationBounds final
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    constexpr bool insideCanvas() const noexcept
    {
        return left >= 0 && top >= 0 && right > left && bottom > top &&
               right <= kCanvasWidth && bottom <= kCanvasHeight;
    }
};

constexpr ModulationBounds modulationControlBounds(ModulationRole role) noexcept
{
    switch (role)
    {
        case ModulationRole::VelocityAmount:      return {55, 260, 190, 360};
        case ModulationRole::VelocityDestination: return {205, 250, 420, 370};
        case ModulationRole::LfoWaveform:         return {455, 250, 595, 370};
        case ModulationRole::LfoRate:             return {610, 260, 735, 360};
        case ModulationRole::LfoAmount:           return {750, 260, 875, 360};
        case ModulationRole::LfoDestination:      return {890, 250, 1230, 370};
        case ModulationRole::LfoRetrigger:        return {455, 410, 650, 510};
        case ModulationRole::LfoSync:             return {670, 410, 875, 510};
        case ModulationRole::LfoPhaseOffset:      return {895, 410, 1090, 510};
        default:                                  return {};
    }
}

constexpr const ModulationParameterBinding* modulationBindingForParameter(
    int parameter) noexcept
{
    for (const auto& binding : kModulationBindings)
        if (binding.parameter == parameter)
            return &binding;
    return nullptr;
}

constexpr std::size_t modulationBindingCount(VoiceOwner owner) noexcept
{
    std::size_t count = 0;
    for (const auto& binding : kModulationBindings)
        if (binding.owner == owner)
            ++count;
    return count;
}

constexpr int modulationSelectableMaximum(ModulationDomain domain) noexcept
{
    switch (domain)
    {
        case ModulationDomain::LfoWaveform: return 7;
        case ModulationDomain::Retrigger: return 6;
        case ModulationDomain::Sync: return 11;
        case ModulationDomain::IntegerByte: return 127;
        case ModulationDomain::TypedRoute: return 65535;
        default: return 0;
    }
}

constexpr bool modulationValueSelectable(ModulationDomain domain,
                                         int value) noexcept
{
    return value >= 0 && value <= modulationSelectableMaximum(domain);
}

constexpr const char* modulationValueLabel(ModulationDomain domain,
                                           int value) noexcept
{
    if (!modulationValueSelectable(domain, value))
        return nullptr;
    switch (domain)
    {
        case ModulationDomain::LfoWaveform:
            return kLfoWaveformLabels[static_cast<std::size_t>(value)];
        case ModulationDomain::Retrigger:
            return kLfoRetriggerLabels[static_cast<std::size_t>(value)];
        case ModulationDomain::Sync:
            return kLfoSyncLabels[static_cast<std::size_t>(value)];
        default:
            return nullptr;
    }
}

constexpr int adjacentModulationValue(const ModulationParameterBinding& binding,
                                      int current, int direction) noexcept
{
    if (binding.domain == ModulationDomain::TypedRoute)
    {
        const auto id = route_host::RouteHostParamId::fromRaw(
            static_cast<std::uint32_t>(binding.parameter));
        std::array<std::uint16_t,
                   modulation_contract::kOriginalTargetCount + 1u> options {};
        std::size_t count = 0u;
        options[count++] = 0u;
        for (std::uint16_t token = 1u;
             token < modulation_contract::kOriginalTargetCount; ++token)
        {
            const auto decoded = route_host::decode(
                id, route_host::RouteToken16::fromRaw(token));
            if (decoded.status == route_host::TokenStatus::Accepted)
                options[count++] = token;
        }
        std::size_t at = 0u;
        while (at < count && options[at] != current)
            ++at;
        if (at == count)
            return options[0];
        return direction < 0
            ? options[(at + count - 1u) % count]
            : options[(at + 1u) % count];
    }
    if (binding.domain == ModulationDomain::IntegerByte)
    {
        if (current < 0 || current > 127)
            return direction < 0 ? 127 : 0;
        return direction < 0 ? (current == 0 ? 127 : current - 1)
                             : (current == 127 ? 0 : current + 1);
    }
    const int maximum = modulationSelectableMaximum(binding.domain);
    if (current < 0 || current > maximum)
        return direction < 0 ? maximum : 0;
    return direction < 0 ? (current == 0 ? maximum : current - 1)
                         : (current == maximum ? 0 : current + 1);
}

constexpr bool modulationSameString(const char* a, const char* b) noexcept
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

constexpr bool validateModulationBindings() noexcept
{
    if (kModulationBindings.size() != 54u)
        return false;
    for (std::size_t i = 0; i < kModulationBindings.size(); ++i)
    {
        const auto& row = kModulationBindings[i];
        if (row.parameter < 0 || row.parameter >= vst::kNumParams ||
            row.originalParameter < 0 || row.originalParameter >= vst::kNativeNumParams ||
            row.controlId == nullptr || row.controlId[0] == '\0' ||
            row.label == nullptr || row.label[0] == '\0' ||
            static_cast<std::size_t>(row.owner) >= 6u ||
            row.minimum > row.defaultValue || row.defaultValue > row.maximum ||
            !modulationControlBounds(row.role).insideCanvas())
            return false;
        if (row.domain == ModulationDomain::TypedRoute)
        {
            if (row.parameter < route_host::kFirstParameter ||
                row.parameter > route_host::kLastParameter ||
                row.minimum != 0 || row.maximum != 65535)
                return false;
        }
        else if (row.minimum != 0 || row.maximum != 127)
            return false;
        for (std::size_t j = 0; j < i; ++j)
            if (row.parameter == kModulationBindings[j].parameter ||
                modulationSameString(row.controlId,
                                     kModulationBindings[j].controlId))
                return false;
    }
    for (std::size_t owner = 0; owner < 6u; ++owner)
        if (modulationBindingCount(static_cast<VoiceOwner>(owner)) != 9u)
            return false;
    return true;
}

static_assert(validateModulationBindings(),
              "P19-D must expose exactly nine valid controls per owner");
