#include "vendor/iPlug2/Examples/LXR/LXRGUIFoundation.h"
#include "dsp/SequencerRng.h"

#include <cstdint>
#include <cstdio>

namespace
{
int checks = 0;
int failures = 0;
void check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}

void testTopologyAndGeometry()
{
    using namespace lxr::gui::sequencer_editor;
    check(kTrackNames.size() == 7u, "seven logical track labels");
    check(lxr::sequencer::kPatternCount == 8u, "eight physical patterns");
    check(lxr::sequencer::kMainStepCount == 16u, "sixteen main steps");
    check(lxr::sequencer::kSubstepCount == 8u, "eight physical substeps");
    check(geometryValid(), "all sequencer controls inside fixed canvas");
    for (std::uint8_t pattern = 0u; pattern < 8u; ++pattern)
        check(patternBounds(pattern).valid(), "pattern selector bounded");
    for (std::uint8_t track = 0u; track < 7u; ++track)
    {
        check(muteBounds(track).valid(), "mute hit target bounded");
        check(rollBounds(track).valid(), "roll hit target bounded");
        for (std::uint8_t step = 0u; step < 16u; ++step)
        {
            const auto bounds = gridBounds(track, step);
            check(bounds.valid() && bounds.width() >= 32 && bounds.height() >= 32,
                  "grid hit target usable and bounded");
        }
    }
    for (std::uint8_t substep = 0u; substep < 8u; ++substep)
        check(substepBounds(substep).valid() && substepBounds(substep).width() >= 44,
              "substep detail hit target usable");
}

void testSelectionAndStoredDetail()
{
    using namespace lxr::gui::sequencer_editor;
    Selection selection;
    check(selection.pattern() == 0u && selection.track() == 0u &&
          selection.mainStep() == 0u && selection.substep() == 0u,
          "deterministic initial selection");
    for (std::uint8_t pattern = 0u; pattern < 8u; ++pattern)
        check(selection.selectPattern(pattern) && selection.pattern() == pattern,
              "all pattern tabs selectable");
    for (std::uint8_t track = 0u; track < 7u; ++track)
        for (std::uint8_t step = 0u; step < 16u; ++step)
            check(selection.selectStep(track, step) &&
                  selection.track() == track && selection.mainStep() == step,
                  "all grid cells deterministically selectable");
    for (std::uint8_t substep = 0u; substep < 8u; ++substep)
        check(selection.selectSubstep(substep) && selection.substep() == substep,
              "all physical positions selectable");
    check(selection.selectLockSlot(0u) && selection.selectLockSlot(1u),
          "two lock slots selectable");
    check(!selection.selectPattern(8u) && !selection.selectStep(7u, 0u) &&
          !selection.selectStep(0u, 16u) && !selection.selectSubstep(8u) &&
          !selection.selectLockSlot(2u), "invalid selections reject");

    auto bank = lxr::sequencer::MakeDefaultPatternBank();
    check(!hasStoredDetail(bank.patterns[0], 0u, 0u),
          "source default boundary substep is not falsely marked edited");
    auto& step = bank.patterns[0].steps[0][3];
    lxr::sequencer::SetStepActive(step, true);
    check(hasStoredDetail(bank.patterns[0], 0u, 0u),
          "inactive main gate still reports stored physical detail");
    bank.patterns[0].mainGates[0] = 0u;
    check(hasStoredDetail(bank.patterns[0], 0u, 0u),
          "detail indicator independent of main gate");
}

void testProbabilityAndIdentity()
{
    check(lxr::sequencer::ProbabilityPasses(0u, 0u),
          "inclusive probability zero passes RNG zero");
    check(!lxr::sequencer::ProbabilityPasses(1u, 0u),
          "probability zero rejects RNG one");
    for (std::uint8_t draw = 0u; draw < 127u; ++draw)
        check(lxr::sequencer::ProbabilityPasses(draw, 127u),
              "probability 127 always passes");
    const auto closed = lxr::sequencer::kTrackIdentities[5];
    const auto open = lxr::sequencer::kTrackIdentities[6];
    check(closed.synthesisOwner == 5u && open.synthesisOwner == 5u,
          "closed and open hats share one synthesis owner");
    check(closed.articulation != open.articulation,
          "hat logical tracks retain distinct articulations");
    check(lxr::sequencer::kMaximumPatternNextCode == 14u,
          "random-next codes end at 14 without ninth pattern");
}
}

int main()
{
    testTopologyAndGeometry();
    testSelectionAndStoredDetail();
    testProbabilityAndIdentity();
    if (failures == 0) std::printf("PASS (%d checks)\n", checks);
    return failures == 0 ? 0 : 1;
}
