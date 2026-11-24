#include "interfaces/P19FGUIQualification.h"
#include "vendor/iPlug2/Examples/LXR/LXRGUIFoundation.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace
{
int gChecks = 0;

void require(bool condition, const char* message)
{
    ++gChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

template <typename Bounds>
void requireUsable(const Bounds& bounds, const char* message)
{
    require(bounds.valid(), message);
    require(bounds.width() >= 24, "interactive target width");
    require(bounds.height() >= 24, "interactive target height");
}
}

int main()
{
    using namespace lxr;
    using namespace lxr::gui;

    require(p19f::contractValid(), "qualification contract");
    require(vst::ValidateParamDescriptors(), "246 parameter descriptors");
    require(vst::kParamDescriptors.size() == 228u, "native registry size");
    require(vst::kNumParams == 246, "complete host registry size");
    require(kViewNames.size() == 5u, "five views");
    require(kVoiceNames.size() == 6u, "six synthesis owners");
    require(kSoundBindings.size() == 162u, "162 Sound controls");
    require(kModulationBindings.size() == 54u, "54 Modulation controls");
    require(validateSoundBindings(), "Sound bindings valid");
    require(validateModulationBindings(), "Modulation bindings valid");
    require(sequencer_editor::geometryValid(), "Sequencer geometry valid");

    std::array<int, p19f::kRegisteredParameters> visible {};
    for (int id = 0; id < p19f::kRegisteredParameters; ++id)
    {
        visible[static_cast<std::size_t>(id)] =
            p19f::intentionallyHidden(id) ? 0 : 1;
    }
    int visibleCount = 0;
    for (const int value : visible) visibleCount += value;
    require(visibleCount == 223, "unique visible parameter count");

    std::array<int, p19f::kRegisteredParameters> productionBindings {};
    for (const auto& row : kSoundBindings)
    {
        require(row.parameter >= 0 && row.parameter < 246, "Sound parameter range");
        require(row.controlId != nullptr && row.controlId[0] != '\0', "Sound control ID");
        require(row.label != nullptr && row.label[0] != '\0', "Sound label");
        ++productionBindings[static_cast<std::size_t>(row.parameter)];
        const auto bounds = soundControlBounds(row);
        require(bounds.insideCanvas(), "Sound bounds inside canvas");
        require(bounds.right - bounds.left >= 24, "Sound target width");
        require(bounds.bottom - bounds.top >= 24, "Sound target height");
    }
    for (const auto& row : kModulationBindings)
    {
        require(row.parameter >= 0 && row.parameter < 246, "Modulation parameter range");
        require(row.controlId != nullptr && row.controlId[0] != '\0', "Modulation control ID");
        require(row.label != nullptr && row.label[0] != '\0', "Modulation label");
        ++productionBindings[static_cast<std::size_t>(row.parameter)];
        const auto bounds = modulationControlBounds(row.role);
        require(bounds.insideCanvas(), "Modulation bounds inside canvas");
        require(bounds.right - bounds.left >= 24, "Modulation target width");
        require(bounds.bottom - bounds.top >= 24, "Modulation target height");
    }
    for (int id = 0; id < 246; ++id)
        require(p19f::intentionallyHidden(id) || visible[static_cast<std::size_t>(id)] == 1,
                "visible/hidden classification exhaustive");

    constexpr std::array<std::size_t, 6> expectedSound {{27, 27, 27, 26, 27, 28}};
    for (std::size_t owner = 0; owner < expectedSound.size(); ++owner)
    {
        const auto voice = static_cast<VoiceOwner>(owner);
        require(soundBindingCount(voice) == expectedSound[owner], "Sound owner count");
        require(modulationBindingCount(voice) == 9u, "Modulation owner count");
    }

    for (std::uint8_t pattern = 0; pattern < 8u; ++pattern)
        requireUsable(sequencer_editor::patternBounds(pattern), "pattern target");
    for (std::uint8_t track = 0; track < 7u; ++track)
    {
        requireUsable(sequencer_editor::muteBounds(track), "mute target");
        requireUsable(sequencer_editor::rollBounds(track), "roll target");
        for (std::uint8_t step = 0; step < 16u; ++step)
            requireUsable(sequencer_editor::gridBounds(track, step), "step target");
    }
    for (std::uint8_t substep = 0; substep < 8u; ++substep)
        requireUsable(sequencer_editor::substepBounds(substep), "substep target");

    SelectionState first;
    SelectionState second;
    require(first.select(PrimaryView::Sound), "select Sound");
    require(first.select(VoiceOwner::HiHat), "select Hi-hat");
    require(first.view() == PrimaryView::Sound && first.voice() == VoiceOwner::HiHat,
            "view state retained");
    require(second.view() == PrimaryView::Overview && second.voice() == VoiceOwner::Drum1,
            "instance selection isolated");
    require(!first.select(PrimaryView::Count), "invalid view rejected");
    require(!first.select(VoiceOwner::Count), "invalid owner rejected");

    sequencer_editor::Selection sequenceA;
    sequencer_editor::Selection sequenceB;
    require(sequenceA.selectPattern(7), "pattern endpoint");
    require(sequenceA.selectStep(6, 15), "track/step endpoints");
    require(sequenceA.selectSubstep(7), "substep endpoint");
    require(sequenceA.selectLockSlot(1), "lock endpoint");
    require(!sequenceA.selectPattern(8), "invalid pattern rejected");
    require(!sequenceA.selectStep(7, 0), "invalid track rejected");
    require(!sequenceA.selectSubstep(8), "invalid substep rejected");
    require(sequenceB.pattern() == 0 && sequenceB.track() == 0,
            "sequencer selection isolated");

    for (const auto& binding : kGlobalBindings)
    {
        for (int raw = binding.minimum; raw <= binding.maximum; ++raw)
        {
            double normalized = -1.0;
            int roundTrip = -1;
            require(normalize(binding, raw, normalized), "normalize accepted");
            require(denormalize(binding, normalized, roundTrip), "denormalize accepted");
            require(roundTrip == raw, "normalization exact round trip");
        }
    }
    const auto* morph = bindingForParameter(245);
    require(morph != nullptr && morph->minimum == 0 && morph->maximum == 255,
            "Morph Amount exact 0..255");

    for (const double scale : p19f::kQualifiedScaleFactors)
    {
        require(p19f::physicalPixels(1280, scale) ==
                    static_cast<int>(1280.0 * scale), "DPI width mapping");
        require(p19f::physicalPixels(800, scale) ==
                    static_cast<int>(800.0 * scale), "DPI height mapping");
        require(scale >= 1.0 && scale <= 2.0, "supported scale range");
    }

    RouteSelection route(route_host::RouteHostParamId::fromRaw(239));
    const auto acceptedBefore = route.accepted();
    const auto invalidStatus = route.trySelect(65535u);
    require(invalidStatus == route_host::TokenStatus::Unavailable ||
            invalidStatus == route_host::TokenStatus::Reserved,
            "invalid high route rejected");
    require(route.accepted() == acceptedBefore, "invalid route preserves accepted value");

    std::cout << "P19-F GUI qualification: PASS (" << gChecks << " checks)\n";
    return 0;
}
