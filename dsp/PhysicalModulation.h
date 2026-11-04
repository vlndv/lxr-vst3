#pragma once
// Converted base cache only; saved raw patch authority remains unchanged.
#include "VelocityRouting.h"
namespace lxr {
using LfoRouteStatus = VelocityRouteStatus;
using LfoRouteState = VelocityRouteState;
constexpr LfoRouteStatus validateLfoDestination(modulation_contract::SourceIdentity source,
    modulation_contract::OriginalTargetIndex target) noexcept {
    using namespace modulation_contract;
    if (validateSource(source)!=Status::Ok || source.kind!=SourceKind::Lfo)
        return LfoRouteStatus::InvalidSource;
    // Deliberately admit exactly the reviewed 107 own-voice scalar rows.
    return validateVelocityDestination({SourceKind::Velocity,source.owner},target);
}
struct PhysicalModulationBase {
    modulation_contract::DestinationId destination=modulation_contract::DestinationId::NONE;
    modulation_contract::FirmwareParameterId parameter=modulation_contract::FirmwareParameterId::fromRaw(0);
    modulation_contract::ParameterValueType type=modulation_contract::ParameterValueType::None;
    float value=0.f;
    uint32_t revision=0;
    uint16_t sources=0;
};
static_assert(sizeof(PhysicalModulationBase)<=32,"bounded converted base cache");
static_assert(12*sizeof(PhysicalModulationBase)+6*sizeof(LfoRouteState)+
    6*2*sizeof(float)+sizeof(uint32_t)+sizeof(uint16_t)<=1024,
    "M2E incremental registry/routes/LFO scalars budget");
} // namespace lxr
