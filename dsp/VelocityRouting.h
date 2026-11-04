#pragma once
// Verified STM32 velocity subset. Original Copyright 2013 Julian Schmidt.
// Derived material: LICENSE-LXR.txt. Native processing-owner API, not host state.
#include "ModulationContract.h"

namespace lxr {
enum class VelocityRouteStatus : std::uint8_t {
    Ok, Disabled, InvalidSource, InvalidIndex, VoiceMismatch, GlobalUnsupported,
    MutableDestinationUnsupported, LfoUnavailable, UnsupportedTarget,
    AmbiguousLegacySelector, InvalidLegacyValue, NonFiniteBase, InvalidDepth,
    InvalidVelocity
};
constexpr bool velocityParameterSupported(modulation_contract::FirmwareParameterId id) noexcept {
    if (!id.valid()) return false;
    const auto p=id.value();
    return (p>=8&&p<=19)||(p>=27&&p<=59)||(p>=62&&p<=67)||
           (p>=70&&p<=77)||(p>=82&&p<=96)||(p>=99&&p<=113)||
           (p>=128&&p<=133)||(p>=191&&p<=196)||(p>=209&&p<=214);
}
constexpr VelocityRouteStatus validateVelocityDestination(
    modulation_contract::SourceIdentity source,
    modulation_contract::OriginalTargetIndex index) noexcept {
    using namespace modulation_contract;
    if (validateSource(source)!=Status::Ok || source.kind!=SourceKind::Velocity)
        return VelocityRouteStatus::InvalidSource;
    const auto* t=lookup(index);
    if (!t) return VelocityRouteStatus::InvalidIndex;
    if (index.value()==0) return VelocityRouteStatus::Ok;
    if (t->scope!=RoutingScope::Voice) return VelocityRouteStatus::GlobalUnsupported;
    if (t->voice!=source.owner) return VelocityRouteStatus::VoiceMismatch;
    if (t->runtimePolicy==RuntimePolicy::RequiresDestinationMutationReview)
        return VelocityRouteStatus::MutableDestinationUnsupported;
    const auto p=t->parameter.value();
    if ((p>=115&&p<=126)||(p>=155&&p<=190)) return VelocityRouteStatus::LfoUnavailable;
    return velocityParameterSupported(t->parameter)?VelocityRouteStatus::Ok:VelocityRouteStatus::UnsupportedTarget;
}
// baseFloat/baseByte are compatibility inspection views of the Engine's shared
// converted-base registry. Only that registry owns restoration; these views are
// never read as an independent base authority. No live pointer survives a copy.
struct VelocityRouteState {
    modulation_contract::OriginalTargetIndex target=modulation_contract::OriginalTargetIndex::fromRaw(0);
    modulation_contract::DestinationId destination=modulation_contract::DestinationId::NONE;
    modulation_contract::ParameterValueType type=modulation_contract::ParameterValueType::None;
    float baseFloat=0.f;
    std::uint8_t baseByte=0;
    bool enabled=false;
    VelocityRouteStatus status=VelocityRouteStatus::Disabled;
};
} // namespace lxr
