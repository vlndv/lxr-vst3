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

inline constexpr std::array<std::array<int, 9>, 6> kExpectedParameters {{
    {{143,228,155,115,121,234,173,179,185}},
    {{144,229,156,116,122,235,174,180,186}},
    {{145,230,157,117,123,236,175,181,187}},
    {{146,231,158,118,124,237,176,182,188}},
    {{147,232,159,119,125,238,177,183,189}},
    {{148,233,160,120,126,239,178,184,190}}
}};

inline constexpr std::array<std::array<std::uint16_t, 18>, 5>
kExpectedEighteenRoutes {{
    {{2,3,5,6,7,8,10,14,15,20,21,22,23,24,31,32,33,34}},
    {{36,37,39,40,41,42,44,48,49,54,55,56,57,58,65,66,67,68}},
    {{70,71,73,74,75,76,78,82,83,88,89,90,91,92,99,100,101,102}},
    {{104,105,106,107,109,110,112,113,115,121,122,123,124,125,132,133,134,135}},
    {{137,138,140,141,143,147,148,149,150,155,156,157,158,159,166,167,168,169}}
}};

inline constexpr std::array<std::uint16_t, 17> kExpectedHiHatRoutes {{
    171,172,174,177,181,182,183,184,189,190,191,192,193,200,201,202,203
}};

bool expectedRoute(std::size_t owner, std::uint16_t token)
{
    if (token == 0u)
        return true;
    if (owner < kExpectedEighteenRoutes.size())
    {
        for (const auto value : kExpectedEighteenRoutes[owner])
            if (value == token)
                return true;
        return false;
    }
    for (const auto value : kExpectedHiHatRoutes)
        if (value == token)
            return true;
    return false;
}

struct EditorHarness final
{
    std::array<int, lxr::vst::kNumParams> values {};
    lxr::gui::SelectionState selection;
    int begins = 0;
    int edits = 0;
    int ends = 0;

    EditorHarness()
    {
        for (std::size_t i = 0; i < lxr::vst::kNativeNumParams; ++i)
            values[i] = lxr::vst::kParamDescriptors[i].defaultValue;
    }

    bool hostReadback(int parameter, int raw)
    {
        const auto* binding = lxr::gui::modulationBindingForParameter(parameter);
        if (binding == nullptr || raw < binding->minimum || raw > binding->maximum)
            return false;
        if (binding->domain == lxr::gui::ModulationDomain::TypedRoute)
        {
            const auto decoded = lxr::route_host::decode(
                lxr::route_host::RouteHostParamId::fromRaw(
                    static_cast<std::uint32_t>(parameter)),
                lxr::route_host::RouteToken16::fromRaw(
                    static_cast<std::uint32_t>(raw)));
            if (!decoded)
                return false;
        }
        values[static_cast<std::size_t>(parameter)] = raw;
        return true;
    }

    bool userEdit(int parameter, int raw)
    {
        ++begins;
        ++edits;
        const bool accepted = hostReadback(parameter, raw);
        ++ends;
        return accepted;
    }
};

void testCoverage()
{
    using namespace lxr::gui;
    check(kModulationBindings.size() == 54u, "exact 54-control coverage");
    std::array<bool, lxr::vst::kNumParams> seen {};
    for (std::size_t owner = 0; owner < 6u; ++owner)
    {
        check(modulationBindingCount(static_cast<VoiceOwner>(owner)) == 9u,
              "nine controls for owner");
        for (std::size_t role = 0; role < 9u; ++role)
        {
            const int expected = kExpectedParameters[owner][role];
            const auto* binding = modulationBindingForParameter(expected);
            check(binding != nullptr, "independent expected host parameter present");
            if (binding == nullptr)
                continue;
            check(binding->owner == static_cast<VoiceOwner>(owner),
                  "source owner identity");
            check(binding->role == static_cast<ModulationRole>(role),
                  "source modulation role");
            check(!seen[static_cast<std::size_t>(expected)], "unique host binding");
            seen[static_cast<std::size_t>(expected)] = true;
            check(binding->automatable && binding->persistent,
                  "accepted base or route is automated and persisted");
            check(modulationControlBounds(binding->role).insideCanvas(),
                  "control geometry inside 1280x800");
        }
    }
    check(modulationBindingForParameter(149) == nullptr,
          "legacy velocity selector is not a second GUI authority");
    check(modulationBindingForParameter(161) == nullptr,
          "LFO voice hint is provenance, not an editable host proxy");
    check(modulationBindingForParameter(167) == nullptr,
          "legacy LFO selector is not exposed as persistent output");
    check(modulationBindingForParameter(137) == nullptr,
          "velocity-to-amplitude remains in Sound");
}

void testWaveformsAndByteDomains()
{
    using namespace lxr::gui;
    constexpr std::array<const char*, 8> firmware {{
        "sin","tri","sup","sdn","sqr","rnd","xup","xdn"
    }};
    constexpr std::array<const char*, 8> display {{
        "Sine","Triangle","Saw Up","Saw Down","Square","S&H Random",
        "Exponential Up","Exponential Down"
    }};
    for (int raw = 0; raw < 8; ++raw)
    {
        check(modulationSameString(kLfoWaveformFirmwareLabels[raw], firmware[raw]),
              "original waveform stored identity order");
        check(modulationSameString(
                  modulationValueLabel(ModulationDomain::LfoWaveform, raw),
                  display[raw]), "source-confirmed waveform display");
    }
    check(modulationValueLabel(ModulationDomain::LfoWaveform, 8) == nullptr,
          "unsupported waveform remains unsupported");
    check(modulationValueLabel(ModulationDomain::Retrigger, 0) != nullptr &&
          modulationValueLabel(ModulationDomain::Retrigger, 6) != nullptr &&
          modulationValueLabel(ModulationDomain::Retrigger, 7) == nullptr,
          "retrigger domain 0..6");
    check(modulationValueLabel(ModulationDomain::Sync, 0) != nullptr &&
          modulationValueLabel(ModulationDomain::Sync, 11) != nullptr &&
          modulationValueLabel(ModulationDomain::Sync, 12) == nullptr,
          "sync domain 0..11");

    for (int raw = 0; raw <= 127; ++raw)
    {
        double normalized = 0.0;
        int roundTrip = -1;
        check(normalizeNativeInteger(raw, normalized), "byte normalizes");
        check(denormalizeNativeInteger(normalized, roundTrip) && roundTrip == raw,
              "rate amount phase exact byte round trip");
    }
    int raw = 0;
    check(!denormalizeNativeInteger(std::numeric_limits<double>::quiet_NaN(), raw),
          "nonfinite byte automation rejected");
}

void testTypedRoutes()
{
    using namespace lxr;
    for (std::size_t source = 0; source < 2u; ++source)
    {
        for (std::size_t owner = 0; owner < 6u; ++owner)
        {
            const auto id = route_host::RouteHostParamId::fromRaw(
                static_cast<std::uint32_t>(228u + source * 6u + owner));
            const auto options = gui::routeOptions(id);
            check(options.count == (owner == 5u ? 18u : 19u),
                  "OFF plus source-owner valid destination rows");
            check(options.values[0].token == 0u && options.contains(0u),
                  "OFF is explicit identity zero");
            for (std::uint16_t token = 1u; token <= 204u; ++token)
            {
                const auto decoded = route_host::decode(
                    id, route_host::RouteToken16::fromRaw(token));
                check((decoded.status == route_host::TokenStatus::Accepted) ==
                          expectedRoute(owner, token),
                      "owner-filtered valid route matrix");
            }
            const std::uint16_t high = owner == 5u ? 203u
                : kExpectedEighteenRoutes[owner].back();
            check(options.contains(high), "highest owner route retained");
            if (high > 127u)
                check(route_host::decode(id, route_host::RouteToken16::fromRaw(high)).record.originalIndex == high,
                      "high identity stays full width");

            const std::uint16_t crossOwner = owner == 0u ? 36u : 2u;
            const auto cross = route_host::decode(
                id, route_host::RouteToken16::fromRaw(crossOwner));
            check(cross.status == route_host::TokenStatus::Unavailable &&
                  cross.validation == PersistentRouteStatus::UnsupportedRoute,
                  "cross-owner route rejected");
            const auto unsupported204 = route_host::decode(
                id, route_host::RouteToken16::fromRaw(204u));
            check(unsupported204.status == route_host::TokenStatus::Unavailable,
                  "identity 204 is known but unsupported");
            const auto reserved = route_host::decode(
                id, route_host::RouteToken16::fromRaw(205u));
            check(reserved.status == route_host::TokenStatus::Reserved,
                  "reserved identity above original table rejected");
        }
    }
    const auto hhLfo = route_host::RouteHostParamId::fromRaw(239u);
    const auto highest = route_host::decode(
        hhLfo, route_host::RouteToken16::fromRaw(203u));
    check(highest.status == route_host::TokenStatus::Accepted &&
          highest.record.originalIndex == 203u,
          "highest supported original identity accepted exactly");
}

void testGesturesReadbackAndIsolation()
{
    EditorHarness first;
    EditorHarness second;
    check(first.userEdit(115, 65), "user byte edit accepted");
    check(first.begins == 1 && first.edits == 1 && first.ends == 1,
          "one balanced gesture for byte edit");
    check(first.userEdit(228, 8), "typed route user edit accepted");
    check(first.begins == 2 && first.edits == 2 && first.ends == 2,
          "one balanced gesture for route edit");
    check(first.values[115] == 65 && first.values[228] == 8,
          "accepted controls update visible state");
    check(second.values[115] == 64 && second.values[228] == 0,
          "multiple instances are isolated");

    const int gestures = first.begins + first.edits + first.ends;
    check(first.hostReadback(155, 7), "external waveform automation accepted");
    check(first.hostReadback(239, 203), "external high route automation accepted");
    check(first.values[155] == 7 && first.values[239] == 203,
          "external automation updates exact readback");
    check(first.begins + first.edits + first.ends == gestures,
          "external readback emits no GUI gesture");
    check(!first.hostReadback(239, 204), "invalid external route rejected");
    check(first.values[239] == 203, "rejection preserves accepted route");

    const auto snapshot = first.values;
    check(first.selection.select(lxr::gui::PrimaryView::Modulation),
          "Modulation navigation accepted");
    for (std::size_t owner = 0; owner < 6u; ++owner)
        check(first.selection.select(static_cast<lxr::gui::VoiceOwner>(owner)),
              "six modulation owners selectable");
    check(first.values == snapshot, "view and owner changes do not mutate state");
    check(first.begins + first.edits + first.ends == gestures,
          "view and owner changes emit no gestures");

    EditorHarness reopened;
    reopened.values = first.values;
    check(reopened.values == first.values && reopened.begins == 0,
          "restored editor reconstructs without gesture");
}

} // namespace

int main()
{
    testCoverage();
    testWaveformsAndByteDomains();
    testTypedRoutes();
    testGesturesReadbackAndIsolation();
    if (failures != 0)
    {
        std::fprintf(stderr, "P19-D Modulation editor: %d/%d checks failed\n",
                     failures, checks);
        return 1;
    }
    std::printf("P19-D Modulation editor: PASS (%d checks)\n", checks);
    return 0;
}
