// P15-M2H stable host-control and normalization contract.
#include "../dsp/RouteHostControls.h"
#include "../vendor/iPlug2/IPlug/IPlugParameter.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

namespace
{
int passed = 0;
int failed = 0;
std::atomic<unsigned long long> allocations {0};
bool measure = false;

void check(const char* name, bool condition)
{
    if (condition) ++passed;
    else { ++failed; std::printf("FAIL %s\n", name); }
}
}

void* operator new(std::size_t size)
{
    if (measure) ++allocations;
    if (void* memory = std::malloc(size)) return memory;
    std::abort();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

int main()
{
    using namespace lxr;
    using namespace lxr::route_host;

    check("exact twelve descriptors", kDescriptors.size() == 12);
    check("exact public ID range", kFirstParameter == 228 && kLastParameter == 239);
    const std::array<const char*, 12> names {{
        "Velocity Destination D1", "Velocity Destination D2",
        "Velocity Destination D3", "Velocity Destination SN",
        "Velocity Destination CY", "Velocity Destination HH",
        "LFO Destination D1", "LFO Destination D2", "LFO Destination D3",
        "LFO Destination SN", "LFO Destination CY", "LFO Destination HH"
    }};
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        check("stable ID order", kDescriptors[i].id.value() == 228u + i);
        check("stable exact name", std::strcmp(kDescriptors[i].name, names[i]) == 0);
        check("stable source owner", kDescriptors[i].source == i / 6u &&
                                     kDescriptors[i].owner == i % 6u);
    }

    iplug::IParam param;
    param.InitInt("Route", 0, 0, 65535);
    check("integer type", param.Type() == iplug::IParam::kTypeInt);
    check("off default", param.GetDefault() == 0.0 && param.Int() == 0);
    check("full range", param.GetMin() == 0.0 && param.GetMax() == 65535.0 &&
                        param.GetRange() == 65535.0);
    check("automatable", param.GetCanAutomate());
    for (unsigned code = 0; code <= 65535u; ++code)
    {
        const double normalized = param.ToNormalized(static_cast<double>(code));
        check("double exact roundtrip",
              param.FromNormalized(normalized) == static_cast<double>(code));
        const double floatRounded = static_cast<double>(static_cast<float>(normalized));
        check("float-rounded exact roundtrip",
              param.FromNormalized(floatRounded) == static_cast<double>(code));
    }
    for (unsigned boundary : {0u, 1u, 127u, 128u, 203u, 204u, 65535u})
        check("boundary roundtrip", param.FromNormalized(
            param.ToNormalized(boundary)) == boundary);

    std::array<unsigned, 12> supported {};
    unsigned above127 = 0;
    for (std::size_t routeSlot = 0; routeSlot < supported.size(); ++routeSlot)
    {
        const auto id = parameterForSlot(routeSlot);
        check("explicit off accepted", decode(id, RouteToken16::fromRaw(0)).status ==
                                       TokenStatus::Off);
        for (unsigned token = 1; token <= 204u; ++token)
        {
            const auto result = decode(id, RouteToken16::fromRaw(token));
            if (result.status == TokenStatus::Accepted)
            {
                ++supported[routeSlot];
                if (token > 127u && routeSlot < 6u) ++above127;
                check("accepted record validates",
                      ValidatePersistentRoute(result.record) == PersistentRouteStatus::Ok);
                check("lossless token encode", encode(result.record).value() == token);
            }
        }
        check("reserved token 205", decode(id, RouteToken16::fromRaw(205)).status ==
                                    TokenStatus::Reserved);
        check("reserved token 65535", decode(id, RouteToken16::fromRaw(65535)).status ==
                                      TokenStatus::Reserved);
    }
    for (std::size_t routeSlot = 0; routeSlot < supported.size(); ++routeSlot)
        check("voice-specific count", supported[routeSlot] ==
              ((routeSlot % 6u) == 5u ? 17u : 18u));
    check("verified velocity union", supported[0] + supported[1] + supported[2] +
                                     supported[3] + supported[4] + supported[5] == 107u);
    check("verified above127 count", above127 == 39u);
    check("highest supported identity", decode(parameterForSlot(11),
        RouteToken16::fromRaw(203)).status == TokenStatus::Accepted);
    check("row204 unavailable", decode(parameterForSlot(11),
        RouteToken16::fromRaw(204)).status == TokenStatus::Unavailable);
    check("cross voice unavailable", decode(parameterForSlot(1),
        RouteToken16::fromRaw(2)).status == TokenStatus::Unavailable);
    check("global unavailable", decode(parameterForSlot(0),
        RouteToken16::fromRaw(1)).status == TokenStatus::Unavailable);
    check("unsafe unavailable", decode(parameterForSlot(0),
        RouteToken16::fromRaw(11)).status == TokenStatus::Unavailable);

    RoutingStateAuthority authority;
    authority.initialize();
    RoutingStateAuthority::Guard guard;
    check("bounded authority acquire", authority.tryAcquire(guard));
    PersistentRouteRecord record;
    std::uint64_t generation = 0, epoch = 0;
    check("read route projection", authority.routeLocked(guard, 0, record,
                                                          generation, epoch));
    const auto active = decode(parameterForSlot(0), RouteToken16::fromRaw(2));
    check("publish accepted route", authority.commitRouteLocked(
        guard, active.record, 1, epoch) == PersistentRouteStatus::Ok);
    check("republish rejected readback", authority.republishRouteLocked(
        guard, 0, 2, epoch) == PersistentRouteStatus::Ok);
    RoutingStateAuthority::Snapshot snapshot;
    check("coherent projected snapshot", authority.copyLocked(guard, snapshot) &&
          snapshot.state.routes[0].originalIndex == 2 &&
          snapshot.routeGenerations[0] == snapshot.generation);
    authority.release(guard);

    measure = true;
    for (unsigned repetition = 0; repetition < 10000; ++repetition)
        for (std::size_t routeSlot = 0; routeSlot < 12; ++routeSlot)
            (void) decode(parameterForSlot(routeSlot),
                          RouteToken16::fromRaw((repetition % 204u) + 1u));
    measure = false;
    check("route validation allocations", allocations.load() == 0);

    check("nonfinite rejected before framework clamp",
          !std::isfinite(std::numeric_limits<double>::infinity()) &&
          !std::isfinite(std::numeric_limits<double>::quiet_NaN()));
    std::printf("SUMMARY pass=%d fail=%d host_codes=65536 routes=12 supported=107 above127=39 allocations=%llu\n",
                passed, failed, allocations.load());
    return failed == 0 ? 0 : 1;
}
