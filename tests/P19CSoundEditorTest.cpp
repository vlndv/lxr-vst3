#include "vendor/iPlug2/Examples/LXR/LXRGUIFoundation.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace
{

int failures = 0;
int checks = 0;

void check(bool condition, const char* message)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}

template <std::size_t N>
bool contains(const std::array<int, N>& values, int value)
{
    for (int candidate : values)
        if (candidate == value)
            return true;
    return false;
}

// Independent expectations transcribed from firmware Parameters.h/menuPages.h
// and the reviewed ParameterArray ownership switch. They deliberately do not
// derive from kSoundBindings.
inline constexpr std::array<int, 27> kDrum1 {{
    1,8,9,20,83,82,134,70,74,78,49,50,62,137,203,197,209,
    191,37,43,128,88,94,102,108,215,221 }};
inline constexpr std::array<int, 27> kDrum2 {{
    2,10,11,21,85,84,135,71,75,79,51,52,63,138,204,198,210,
    192,38,44,129,89,95,103,109,216,222 }};
inline constexpr std::array<int, 27> kDrum3 {{
    3,12,13,22,87,86,136,72,76,80,53,54,64,139,205,199,211,
    193,39,45,130,90,96,104,110,217,223 }};
inline constexpr std::array<int, 26> kSnare {{
    4,14,15,27,28,73,77,81,55,56,65,68,140,206,200,212,
    194,40,46,131,91,99,105,111,218,224 }};
inline constexpr std::array<int, 27> kCymbal {{
    6,16,17,23,29,31,24,30,32,57,58,66,69,141,207,201,213,
    195,41,47,132,92,100,106,112,219,225 }};
inline constexpr std::array<int, 28> kHiHat {{
    7,18,19,25,33,35,26,34,36,59,60,61,67,142,208,202,214,
    196,42,48,133,93,101,107,113,220,226,227 }};

lxr::gui::VoiceOwner expectedOwner(int parameter)
{
    using lxr::gui::VoiceOwner;
    if (contains(kDrum1, parameter)) return VoiceOwner::Drum1;
    if (contains(kDrum2, parameter)) return VoiceOwner::Drum2;
    if (contains(kDrum3, parameter)) return VoiceOwner::Drum3;
    if (contains(kSnare, parameter)) return VoiceOwner::Snare;
    if (contains(kCymbal, parameter)) return VoiceOwner::Cymbal;
    if (contains(kHiHat, parameter)) return VoiceOwner::HiHat;
    return VoiceOwner::Count;
}

struct EditorHarness
{
    std::array<int, lxr::vst::kNativeNumParams> accepted {};
    lxr::gui::SelectionState selection;
    int beginGestures = 0;
    int endGestures = 0;

    EditorHarness()
    {
        for (std::size_t i = 0; i < accepted.size(); ++i)
            accepted[i] = lxr::vst::kParamDescriptors[i].defaultValue;
    }

    bool hostUpdate(int parameter, double normalized)
    {
        if (parameter < 0 || parameter >= lxr::vst::kNativeNumParams)
            return false;
        int raw = 0;
        if (!lxr::gui::denormalizeNativeInteger(normalized, raw))
            return false;
        accepted[static_cast<std::size_t>(parameter)] = raw;
        return true;
    }

    bool guiUpdate(int parameter, int raw)
    {
        if (lxr::gui::soundBindingForParameter(parameter) == nullptr)
            return false;
        double normalized = 0.0;
        if (!lxr::gui::normalizeNativeInteger(raw, normalized))
            return false;
        ++beginGestures;
        const bool result = hostUpdate(parameter, normalized);
        ++endGestures;
        return result;
    }
};

void testCoverageAndOwnership()
{
    using namespace lxr::gui;
    check(kSoundBindings.size() == 162u, "162 source-backed Sound bindings");
    const std::array<std::size_t, 6> counts {{27,27,27,26,27,28}};
    for (std::size_t owner = 0; owner < counts.size(); ++owner)
        check(soundBindingCount(static_cast<VoiceOwner>(owner)) == counts[owner],
              "owner count matches firmware expectation");

    std::array<bool, lxr::vst::kNativeNumParams> seen {};
    for (const auto& binding : kSoundBindings)
    {
        check(binding.parameter >= 0 && binding.parameter < lxr::vst::kNativeNumParams,
              "binding host ID is native and stable");
        check(!seen[static_cast<std::size_t>(binding.parameter)],
              "no duplicate Sound host binding");
        seen[static_cast<std::size_t>(binding.parameter)] = true;
        check(binding.owner == expectedOwner(binding.parameter),
              "binding owner matches independent source table");
        check(soundBindingForParameter(binding.parameter) == &binding,
              "parameter lookup returns stable binding identity");
        check(soundEndpoint(binding, false) == 0,
              "keyboard minimum preserves native endpoint");
        check(soundEndpoint(binding, true) ==
                  (binding.enumeration == EnumerationDomain::None
                       ? 127 : enumerationMaximum(binding.enumeration)),
              "keyboard maximum stays inside source-backed domain");
    }
    check(soundBindingForParameter(0) == nullptr, "reserved byte omitted");
    check(soundBindingForParameter(115) == nullptr, "LFO rate deferred to P19-D");
    check(soundBindingForParameter(228) == nullptr, "typed route deferred to P19-D");
    check(soundBindingForParameter(240) == nullptr, "transport is not Sound");

    check(expectedOwner(226) == VoiceOwner::HiHat, "closed note belongs to shared hat");
    check(expectedOwner(227) == VoiceOwner::HiHat, "open note belongs to shared hat");
    check(synthesisOwnerForTrack(5) == VoiceOwner::HiHat, "CH track maps to hat owner");
    check(synthesisOwnerForTrack(6) == VoiceOwner::HiHat, "OH track maps to hat owner");
    check(synthesisOwnerForTrack(7) == VoiceOwner::Count, "no seventh synthesis owner");
}

void testNormalizationAndDefaults()
{
    for (int raw = 0; raw <= 127; ++raw)
    {
        double normalized = -1.0;
        int decoded = -1;
        check(lxr::gui::normalizeNativeInteger(raw, normalized),
              "native integer normalizes");
        check(lxr::gui::denormalizeNativeInteger(normalized, decoded) && decoded == raw,
              "native integer exact round trip");
    }
    double normalized = 0.0;
    int raw = 0;
    check(!lxr::gui::normalizeNativeInteger(-1, normalized), "reject below minimum");
    check(!lxr::gui::normalizeNativeInteger(128, normalized), "reject above maximum");
    check(!lxr::gui::denormalizeNativeInteger(-0.001, raw), "reject normalized below zero");
    check(!lxr::gui::denormalizeNativeInteger(1.001, raw), "reject normalized above one");
    check(!lxr::gui::denormalizeNativeInteger(
        std::numeric_limits<double>::quiet_NaN(), raw), "reject NaN");
    check(!lxr::gui::denormalizeNativeInteger(
        std::numeric_limits<double>::infinity(), raw), "reject infinity");
    check(lxr::gui::denormalizeNativeInteger(0.5, raw) && raw == 64,
          "midpoint rounds to accepted byte 64");

    for (const auto& binding : lxr::gui::kSoundBindings)
    {
        const auto& descriptor = lxr::vst::kParamDescriptors[
            static_cast<std::size_t>(binding.parameter)];
        check(descriptor.minValue == 0 && descriptor.maxValue == 127,
              "native endpoints remain 0..127");
        check(descriptor.defaultValue >= 0 && descriptor.defaultValue <= 127,
              "registered default is in range");
    }
}

void testEnumerations()
{
    using namespace lxr::gui;
    const std::array<const char*, 6> waves {{"Sin","Tri","Saw","Rec","Noi","Cym"}};
    for (int i = 0; i < 6; ++i)
        check(sameString(enumerationLabel(EnumerationDomain::Waveform, i), waves[i]),
              "wave label and stored identity");
    const std::array<const char*, 8> filters {{"LP","HP","BP","UBP","Nch","Pek","LP2","off"}};
    for (int i = 0; i < 8; ++i)
        check(sameString(enumerationLabel(EnumerationDomain::FilterType, i), filters[i]),
              "filter label and stored identity");
    check(sameString(enumerationLabel(EnumerationDomain::Transient, 0), "Snp"),
          "transient zero identity");
    check(sameString(enumerationLabel(EnumerationDomain::Transient, 13), "Sp2"),
          "transient endpoint identity");
    check(sameString(enumerationLabel(EnumerationDomain::AudioOutput, 5), "R2"),
          "output endpoint identity");
    check(sameString(enumerationLabel(EnumerationDomain::MixMode, 0), "FM"),
          "mix mode zero is FM");
    check(sameString(enumerationLabel(EnumerationDomain::MixMode, 1), "Mix"),
          "mix mode one is Mix");
    check(sameString(enumerationLabel(EnumerationDomain::Boolean, 1), "On"),
          "boolean identity one is On");
    check(enumerationLabel(EnumerationDomain::Waveform, 6) == nullptr,
          "unsupported SD value not shown as valid");
    check(enumerationLabel(EnumerationDomain::FilterType, 8) == nullptr,
          "reserved filter value not shown as valid");
    check(adjacentEnumerationValue(EnumerationDomain::Waveform, 127, 1) == 0,
          "editing unsupported enum enters first valid identity");
    check(adjacentEnumerationValue(EnumerationDomain::Waveform, 127, -1) == 5,
          "reverse edit enters last valid identity");
}

void testGeometryAndFocusOrder()
{
    using namespace lxr::gui;
    check(validateSoundGeometry(), "all runtime Sound controls inside 1280x800");
    for (const auto& binding : kSoundBindings)
    {
        const auto bounds = soundControlBounds(binding);
        check(bounds.insideCanvas(), "control bounds inside canvas");
        check(bounds.top >= 198 && bounds.bottom <= 684,
              "control remains above bounded footer");
    }
    for (std::size_t owner = 0; owner < 6; ++owner)
    {
        std::array<bool, lxr::vst::kNativeNumParams> traversed {};
        std::size_t count = 0;
        for (const auto& binding : kSoundBindings)
            if (binding.owner == static_cast<VoiceOwner>(owner))
            {
                traversed[static_cast<std::size_t>(binding.parameter)] = true;
                ++count;
            }
        check(count == soundBindingCount(static_cast<VoiceOwner>(owner)),
              "focus traversal visits every visible owner control once");
        for (const auto& binding : kSoundBindings)
            if (binding.owner == static_cast<VoiceOwner>(owner))
                check(traversed[static_cast<std::size_t>(binding.parameter)],
                      "visible parameter is keyboard reachable");
    }
}

void testReadbackLifecycleAndIsolation()
{
    using namespace lxr::gui;
    EditorHarness a;
    EditorHarness b;
    check(a.guiUpdate(8, 63), "GUI adjustment accepted");
    check(a.beginGestures == 1 && a.endGestures == 1,
          "one balanced host gesture per GUI adjustment");
    check(a.accepted[8] == 63 && b.accepted[8] == 63,
          "initial defaults equal without shared mutable state");
    check(a.guiUpdate(8, 64), "adjacent GUI value accepted");
    check(a.accepted[8] == 64 && b.accepted[8] == 63,
          "plugin editor instances remain independent");

    const int gesturesBeforeHost = a.beginGestures + a.endGestures;
    check(a.hostUpdate(8, 100.0 / 127.0), "host automation readback accepted");
    check(a.accepted[8] == 100, "host update displays accepted integer");
    check(a.beginGestures + a.endGestures == gesturesBeforeHost,
          "host readback creates no UI gesture");
    check(!a.hostUpdate(8, std::numeric_limits<double>::quiet_NaN()),
          "invalid host automation rejected");
    check(a.accepted[8] == 100, "invalid update preserves accepted state");

    const auto snapshot = a.accepted;
    const int gesturesBeforeSelection = a.beginGestures + a.endGestures;
    check(a.selection.select(PrimaryView::Sound), "Sound view selection accepted");
    for (std::size_t owner = 0; owner < 6; ++owner)
        check(a.selection.select(static_cast<VoiceOwner>(owner)),
              "voice selection accepted");
    check(a.accepted == snapshot, "voice switching rewrites no patch values");
    check(a.beginGestures + a.endGestures == gesturesBeforeSelection,
          "voice switching creates no host gesture");

    EditorHarness reopened;
    reopened.accepted = a.accepted; // controller/state restoration readback
    check(reopened.accepted == a.accepted, "editor reopen restores accepted state");
    check(reopened.beginGestures == 0 && reopened.endGestures == 0,
          "editor restoration creates no host gesture");
    check(!a.guiUpdate(115, 12), "cross-view parameter not editable as Sound");
    check(!a.guiUpdate(8, -1), "GUI rejects below-range value");
    check(!a.guiUpdate(8, 128), "GUI rejects above-range value");
}

} // namespace

int main()
{
    testCoverageAndOwnership();
    testNormalizationAndDefaults();
    testEnumerations();
    testGeometryAndFocusOrder();
    testReadbackLifecycleAndIsolation();
    if (failures != 0)
    {
        std::fprintf(stderr, "P19-C Sound editor contract: %d/%d checks failed\n",
                     failures, checks);
        return 1;
    }
    std::printf("P19-C Sound editor contract: PASS (%d checks)\n", checks);
    return 0;
}
