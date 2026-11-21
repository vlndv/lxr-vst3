#include "../vendor/iPlug2/Examples/LXR/LXRParamContract.h"
#include "../dsp/PatternBank.h"
#include "../dsp/RouteHostControls.h"
#include "../dsp/SequencerParameterLocks.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace
{
std::uint64_t assertions = 0;
std::uint64_t failures = 0;

void check(bool condition)
{
    ++assertions;
    if (!condition)
        ++failures;
}
}

int main()
{
    using namespace lxr;
    using namespace lxr::sequencer;

    check(vst::kNativeNumParams == 228);
    check(vst::kRouteNumParams == 12);
    check(vst::kPersistentNumParams == 240);
    check(vst::kSequencerEnableParam == 240);
    check(vst::kSequencerClockModeParam == 241);
    check(vst::kSequencerInternalTempoParam == 242);
    check(vst::kSequencerRollRateParam == 243);
    check(vst::kSequencerRollTracksParam == 244);
    check(vst::kMorphAmountParam == 245);
    check(vst::kNumParams == 246);

    for (std::size_t index = 0; index < vst::kParamDescriptors.size(); ++index)
    {
        const auto& descriptor = vst::kParamDescriptors[index];
        check(descriptor.name != nullptr && descriptor.name[0] != '\0');
        check(descriptor.minValue == 0);
        check(descriptor.maxValue == 127);
        check(descriptor.defaultValue >= descriptor.minValue);
        check(descriptor.defaultValue <= descriptor.maxValue);
        for (std::size_t prior = 0; prior < index; ++prior)
            check(std::string_view(descriptor.name) !=
                  std::string_view(vst::kParamDescriptors[prior].name));
    }

    check(route_host::kFirstParameter == 228);
    check(route_host::kLastParameter == 239);
    check(route_host::kMaximumToken == 65535);
    check(route_host::validateDescriptors());
    std::uint64_t acceptedRoutes = 0;
    std::uint64_t acceptedAbove127 = 0;
    for (const auto& descriptor : route_host::kDescriptors)
    {
        check(descriptor.id.valid());
        check(descriptor.source < 2);
        check(descriptor.owner < 6);
        for (std::uint32_t token = 0; token <= route_host::kMaximumToken; ++token)
        {
            const auto decoded = route_host::decode(
                descriptor.id, route_host::RouteToken16::fromRaw(token));
            if (decoded.status == route_host::TokenStatus::Accepted)
            {
                ++acceptedRoutes;
                if (token > 127u)
                    ++acceptedAbove127;
                check(decoded.record.originalIndex == token);
                check(decoded.record.source == descriptor.source);
                check(decoded.record.owner == descriptor.owner);
            }
        }
    }
    check(acceptedRoutes != 0);
    check(acceptedAbove127 != 0);

    check(kPatternCount == 8);
    check(kTrackCount == 7);
    check(kSynthesisOwnerCount == 6);
    check(kMainStepCount == 16);
    check(kSubstepCount == 8);
    check(kPhysicalPositionCount == 128);
    check(kNoAutomation == 255);
    check(kMaximumAutomationParameter == 227);

    PatternBank bank = MakeDefaultPatternBank();
    StepData detail {};
    detail.volume = 117u;
    detail.probability = 0u;
    detail.note = 93u;
    detail.parameter1 = 2u;
    detail.value1 = 17u;
    detail.parameter2 = 2u;
    detail.value2 = 91u;
    const auto position = PhysicalPosition(3u, 5u);
    check(SetStep(bank, 0u, 0u, position, detail) == Status::Ok);
    check(SetMainGate(bank, 0u, 0u, 3u, false) == Status::Ok);
    StepData retained {};
    check(GetStep(bank, 0u, 0u, position, retained) == Status::Ok);
    check(retained.volume == detail.volume);
    check(retained.probability == 0u);
    check(retained.note == 93u);
    check(retained.parameter1 == retained.parameter2);
    check(retained.value1 == 17u && retained.value2 == 91u);

    check(DecodeAutomationSlot(kFrontPanelAutomationOff, 0u).status ==
          AutomationValidationStatus::Clear);
    check(DecodeAutomationSlot(kFirmwareAutomationSentinel, 0u).status ==
          AutomationValidationStatus::Clear);
    check(!IsCurrentAutomationRuntimeTarget(0u));
    check(!IsCurrentAutomationRuntimeTarget(149u));
    check(!IsCurrentAutomationRuntimeTarget(167u));
    check(!IsCurrentAutomationRuntimeTarget(221u));
    check(IsCurrentAutomationRuntimeTarget(2u));

    std::cout << "{\"assertions\":" << assertions
              << ",\"failures\":" << failures
              << ",\"accepted_routes\":" << acceptedRoutes
              << ",\"accepted_routes_above_127\":" << acceptedAbove127
              << "}\n";
    return failures == 0 ? 0 : 1;
}
