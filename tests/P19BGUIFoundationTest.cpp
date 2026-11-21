#include "../vendor/iPlug2/Examples/LXR/LXRGUIFoundation.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <thread>

namespace
{
std::atomic<std::uint64_t> gAllocations {0u};
std::uint64_t gAssertions = 0u;

void check(bool condition, const char* message)
{
    ++gAssertions;
    if (!condition)
    {
        std::fprintf(stderr, "P19-B failure: %s\n", message);
        std::exit(1);
    }
}

const lxr::gui::ParameterBinding& binding(int parameter)
{
    const auto* result = lxr::gui::bindingForParameter(parameter);
    check(result != nullptr, "required binding missing");
    return *result;
}
}

void* operator new(std::size_t size)
{
    gAllocations.fetch_add(1u, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    std::abort();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new[](std::size_t size)
{
    gAllocations.fetch_add(1u, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    std::abort();
}
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main()
{
    using namespace lxr::gui;
    check(kCanvasWidth == 1280 && kCanvasHeight == 800, "canvas contract");
    check(kViewNames.size() == 5u, "exactly five views");
    check(kVoiceNames.size() == 6u, "exactly six synthesis owners");
    check(synthesisOwnerForTrack(5u) == VoiceOwner::HiHat, "closed hat owner");
    check(synthesisOwnerForTrack(6u) == VoiceOwner::HiHat, "open hat owner");
    check(!valid(synthesisOwnerForTrack(7u)), "seventh owner rejected");

    SelectionState selection;
    for (std::size_t i = 0; i < kViewNames.size(); ++i)
    {
        check(selection.select(static_cast<PrimaryView>(i)), "view selection accepted");
        for (std::size_t j = 0; j < kViewNames.size(); ++j)
            check((selection.view() == static_cast<PrimaryView>(j)) == (i == j),
                  "exactly one active view");
    }
    const auto selectionGeneration = selection.generation();
    check(!selection.select(PrimaryView::Count), "invalid view rejected");
    check(selection.generation() == selectionGeneration, "invalid view retained state");
    for (std::size_t i = 0; i < kVoiceNames.size(); ++i)
    {
        check(selection.select(static_cast<VoiceOwner>(i)), "voice selection accepted");
        check(selection.voice() == static_cast<VoiceOwner>(i), "selected voice readback");
    }

    check(validateBindings(), "binding uniqueness");
    for (const auto& row : kCoreBindings)
    {
        check(row.parameter >= 0 && row.parameter < lxr::vst::kNumParams,
              "core parameter registered");
        if (row.parameter < lxr::vst::kNativeNumParams)
        {
            const auto& descriptor = lxr::vst::kParamDescriptors[
                static_cast<std::size_t>(row.parameter)];
            check(row.minimum == descriptor.minValue &&
                  row.maximum == descriptor.maxValue &&
                  row.defaultValue == descriptor.defaultValue,
                  "core binding matches independent native descriptor");
        }
        else
        {
            check(row.kind == ControlKind::TypedRoute && row.minimum == 0 &&
                  row.maximum == 65535 && row.defaultValue == 0,
                  "typed route binding preserves host width");
        }
        for (const auto& other : kCoreBindings)
            if (&row != &other)
                check(row.parameter != other.parameter, "core parameter not duplicated");
    }

    // Independent explicit expectations: these values are source/manifest
    // contracts, not derived from the GUI metadata under test.
    struct Expected { int id, minimum, maximum, initial; };
    constexpr Expected expected[] = {
        {88, 0, 127, 100}, {8, 0, 127, 63}, {37, 0, 127, 64},
        {1, 0, 127, 1}, {137, 0, 127, 1}, {228, 0, 65535, 0},
        {240, 0, 1, 0}, {241, 0, 1, 0}, {242, 1, 255, 120},
        {245, 0, 255, 0}, {114, 0, 127, 127}
    };
    for (const auto& item : expected)
    {
        const auto& row = binding(item.id);
        check(row.minimum == item.minimum && row.maximum == item.maximum &&
              row.defaultValue == item.initial, "independent parameter contract");
        if (item.id < lxr::vst::kNativeNumParams)
        {
            const auto& native = lxr::vst::kParamDescriptors[static_cast<std::size_t>(item.id)];
            check(native.minValue == item.minimum && native.maxValue == item.maximum &&
                  native.defaultValue == item.initial, "native descriptor contract");
        }
        double normalized = -1.0;
        int raw = -1;
        check(normalize(row, item.minimum, normalized) && normalized == 0.0,
              "minimum normalization");
        check(normalize(row, item.maximum, normalized) && normalized == 1.0,
              "maximum normalization");
        check(denormalize(row, 0.0, raw) && raw == item.minimum,
              "minimum denormalization");
        check(denormalize(row, 1.0, raw) && raw == item.maximum,
              "maximum denormalization");
        check(!normalize(row, item.minimum - 1, normalized), "low raw rejected");
        check(!normalize(row, item.maximum + 1, normalized), "high raw rejected");
        check(!denormalize(row, -0.001, raw), "low normalized rejected");
        check(!denormalize(row, 1.001, raw), "high normalized rejected");
        check(!denormalize(row, NAN, raw) && !denormalize(row, INFINITY, raw),
              "nonfinite normalized rejected");
    }

    bool foundHigh = false;
    for (std::size_t slot = 0; slot < 6u; ++slot)
    {
        const auto id = lxr::route_host::RouteHostParamId::fromRaw(
            static_cast<std::uint16_t>(228u + slot));
        const auto options = routeOptions(id);
        check(options.count > 1u && options.values[0].token == 0u,
              "typed selector begins with OFF");
        RouteSelection route(id);
        for (std::size_t i = 0; i < options.count; ++i)
        {
            const auto token = options.values[i].token;
            const auto status = route.trySelect(token);
            check(status == (token == 0u ? lxr::route_host::TokenStatus::Off
                                        : lxr::route_host::TokenStatus::Accepted),
                  "listed route accepted");
            check(route.accepted() == token, "stored identity is token, not menu index");
            foundHigh = foundHigh || token > 127u;
        }
        const auto retained = route.accepted();
        check(route.trySelect(65535u) != lxr::route_host::TokenStatus::Accepted,
              "reserved route rejected");
        check(route.accepted() == retained, "rejected route retains accepted state");

        bool crossVoiceRejected = false;
        for (std::uint16_t token = 1u;
             token < lxr::modulation_contract::kOriginalTargetCount; ++token)
        {
            if (options.contains(token)) continue;
            if (route.trySelect(token) == lxr::route_host::TokenStatus::Unavailable)
            {
                crossVoiceRejected = true;
                check(route.accepted() == retained, "cross-owner route retained state");
                break;
            }
        }
        check(crossVoiceRejected, "cross-owner destination rejected");
    }
    check(foundHigh, "typed selector includes identity above 127");

    ReadbackAuthority first;
    ReadbackAuthority second;
    AudioReadback one;
    AudioReadback two;
    const auto allocationStart = gAllocations.load(std::memory_order_relaxed);
    first.publish(123.5, true, true, true, true, true, 3u, 4u, 1u, 1u, 117u);
    second.publish(400.0, true, true, true, false, false, 6u, 2u, 0u, 0u, 89u);
    check(first.trySnapshot(one), "first snapshot acquired");
    check(second.trySnapshot(two), "second snapshot acquired");
    const auto allocationEnd = gAllocations.load(std::memory_order_relaxed);
    check(allocationEnd == allocationStart, "publish and snapshot allocate nothing");
    check(one.hostTempoValid && one.hostTempoBpm == 123.5 && one.hostPlaying,
          "valid host readback");
    check(!two.hostTempoValid && two.hostTempoBpm == 0.0 && !two.hostPlaying,
          "invalid host tempo not inferred");
    check(one.activePattern == 3u && one.pendingPattern == 4u &&
          one.sequencerEnabled == 1u && one.clockMode == 1u &&
          one.sequencerRunning && one.internalTempo == 117u,
          "coherent first-instance status");
    check(two.activePattern == 6u && two.pendingPattern == 2u &&
          two.sequencerEnabled == 0u, "independent second-instance status");

    std::atomic<bool> run {true};
    std::thread writer([&] {
        for (std::uint32_t i = 0u; i < 100000u; ++i)
            first.publish(100.0 + static_cast<double>(i & 31u), true, true,
                          true, (i & 1u) != 0u, true,
                          static_cast<std::uint8_t>(i & 7u),
                          static_cast<std::uint8_t>((i + 1u) & 7u), 1u, 1u, 120u);
        run.store(false, std::memory_order_release);
    });
    while (run.load(std::memory_order_acquire))
    {
        if (!first.trySnapshot(one)) continue;
        check(one.hostTempoValid && one.hostTempoBpm >= 100.0 &&
              one.hostTempoBpm <= 131.0, "concurrent snapshot is coherent");
        check(one.pendingPattern == static_cast<std::uint8_t>((one.activePattern + 1u) & 7u),
              "concurrent packed fields share generation");
    }
    writer.join();

    constexpr std::uint32_t refreshIterations = 1000000u;
    const auto refreshStart = std::chrono::steady_clock::now();
    for (std::uint32_t i = 0u; i < refreshIterations; ++i)
    {
        first.publish(120.0, true, true, true, false, true,
                      static_cast<std::uint8_t>(i & 7u),
                      static_cast<std::uint8_t>((i + 1u) & 7u), 1u, 0u, 120u);
        check(first.trySnapshot(one), "measured snapshot acquired");
    }
    const auto refreshElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - refreshStart).count();
    const auto refreshRoundtripNs = static_cast<unsigned long long>(
        refreshElapsed / refreshIterations);

    std::printf("{\"status\":\"PASS\",\"assertions\":%llu,"
                "\"views\":5,\"voices\":6,\"bindings\":41,"
                "\"high_route_identity\":true,\"audio_allocations\":0,"
                "\"snapshot_roundtrip_ns\":%llu}\n",
                static_cast<unsigned long long>(gAssertions), refreshRoundtripNs);
    return 0;
}
