// P17-F identity-bearing LXRS v3 and coordinated publication tests.

#include "../vendor/iPlug2/Examples/LXR/LXRProjectState.h"
#include "../interfaces/RuntimeKitLoad.h"
#include "../dsp/Engine.h"
#include "../dsp/ParameterArray.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <thread>

namespace
{
using namespace lxr;
using namespace lxr::state;
using namespace lxr::kit;
using PatchAuthority = ParameterStateAuthority<kFieldCount>;

std::uint64_t passed = 0, failed = 0;
std::atomic<std::uint64_t> allocations {0};
std::atomic<bool> countAllocations {false};

void check(bool value, const char* label)
{
    if (value) ++passed;
    else
    {
        ++failed;
        if (failed < 30u) std::fprintf(stderr, "FAIL: %s\n", label);
    }
}

void* countedNew(std::size_t size)
{
    if (countAllocations.load(std::memory_order_relaxed))
        allocations.fetch_add(1u, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0u ? 1u : size)) return memory;
    std::abort();
}

ProjectCandidate project(const std::array<std::uint8_t, 8>& name,
                         bool present, std::uint8_t volume)
{
    ProjectCandidate result;
    ParameterArray parameters;
    parameters.init();
    for (std::size_t i = 0; i < result.patch.size(); ++i)
        result.patch[i] = parameters.get(static_cast<std::uint8_t>(i));
    result.patch[88] = volume;
    result.kitName = name;
    result.kitIdentityPresent = present;
    result.bank.patterns[2].mainGates[1] = 0x1234u;
    result.transport.enabled = 1u;
    result.transport.bpm = 173u;
    result.transport.pattern = 2u;
    result.routing.bypass = true;
    return result;
}

bool sameMusical(const ProjectCandidate& a, const ProjectCandidate& b)
{
    sequencer::state::EncodedBank ab {}, bb {};
    std::array<std::uint8_t, 16> at {}, bt {};
    bool routesEqual = true;
    for (std::size_t i = 0; i < a.routing.routes.size(); ++i)
    {
        std::array<std::uint8_t, kV2RouteSize> ar {}, br {};
        WriteRoute(ar.data(), a.routing.routes[i]);
        WriteRoute(br.data(), b.routing.routes[i]);
        routesEqual = routesEqual && ar == br;
    }
    return a.patch == b.patch && routesEqual &&
           a.routing.bypass == b.routing.bypass &&
           sequencer::state::Encode(a.bank, ab) &&
           sequencer::state::Encode(b.bank, bb) && ab == bb &&
           sequencer::state::EncodeTransport(a.transport, at.data()) &&
           sequencer::state::EncodeTransport(b.transport, bt.data()) && at == bt;
}

void rewriteCrc(ProjectBytes& bytes)
{
    const auto core = GetU32LE(bytes.data() + 8);
    PutU32LE(bytes.data() + 24, Crc32V2(bytes.data(), core));
}

void formatContract()
{
    const auto historical = project({}, false, 73u);
    ProjectBytes oldBytes {}, oldRepeat {};
    std::size_t oldSize = 0, oldRepeatSize = 0;
    check(EncodeProject(historical, oldBytes, oldSize, true),
          "encode historical v3");
    check(oldSize == kV3CanonicalStreamBytes &&
              GetU16LE(oldBytes.data() + 16) == 3u,
          "historical v3 remains exactly 51065 bytes and three modules");
    ProjectCandidate oldDecoded;
    check(DecodeProject(oldBytes.data(), oldSize, oldDecoded, true) &&
              !oldDecoded.kitIdentityPresent && oldDecoded.kitName ==
                  std::array<std::uint8_t, 8>{} &&
              sameMusical(historical, oldDecoded),
          "historical v3 migrates to absent zero identity");
    check(EncodeProject(oldDecoded, oldRepeat, oldRepeatSize, true) &&
              oldRepeatSize == oldSize &&
              std::memcmp(oldBytes.data(), oldRepeat.data(), oldSize) == 0,
          "historical v3 reserializes byte exactly");

    const std::array<std::array<std::uint8_t, 8>, 5> names {{
        {{'P','R','I','N','T',0,0,0}},
        {{'A',0,'B',0,'C',0,'D',0}},
        {{0xffu,0x80u,0x01u,0x7fu,'Z',0,0,0}},
        {{0,0,0,0,0,0,0,0}},
        {{'E','I','G','H','T','B','Y','T'}}
    }};
    for (const auto& name : names)
    {
        const auto source = project(name, true, 73u);
        ProjectBytes encoded {}, repeated {};
        std::size_t size = 0, repeatedSize = 0;
        check(EncodeProject(source, encoded, size, true),
              "encode identity-bearing v3");
        check(size == kV3IdentityStreamBytes &&
                  GetU16LE(encoded.data() + 16) == 4u &&
                  GetU32LE(encoded.data() + kV3CanonicalCoreBytes) ==
                      kV3KitIdentityTag &&
                  GetU16LE(encoded.data() + kV3CanonicalCoreBytes + 6) == 0u &&
                  GetU32LE(encoded.data() + kV3CanonicalCoreBytes + 8) == 8u,
              "identity module framing and exact 51089-byte size");
        ProjectCandidate decoded;
        check(DecodeProject(encoded.data(), size, decoded, true) &&
                  decoded.kitIdentityPresent && decoded.kitName == name &&
                  sameMusical(source, decoded),
              "all eight identity bytes round trip");
        check(EncodeProject(decoded, repeated, repeatedSize, true) &&
                  repeatedSize == size &&
                  std::memcmp(encoded.data(), repeated.data(), size) == 0,
              "identity-bearing v3 reserializes byte exactly");
    }

    std::array<std::uint8_t, kEncodedSize> v1 {};
    check(Encode(historical.patch, v1), "encode v1 migration fixture");
    ProjectCandidate migrated;
    check(DecodeProject(v1.data(), v1.size(), migrated, false) &&
              !migrated.kitIdentityPresent && migrated.kitName ==
                  std::array<std::uint8_t, 8>{},
          "LXRS v1 supplies absent identity");
    std::array<std::uint8_t, kMaximumStateSize> v2 {};
    std::size_t v2Size = 0;
    check(EncodeV2(historical.patch, historical.routing, v2, v2Size, false),
          "encode v2 migration fixture");
    check(DecodeProject(v2.data(), v2Size, migrated, false) &&
              !migrated.kitIdentityPresent,
          "LXRS v2 supplies absent identity");
}

void corruptionContract()
{
    const auto source = project({{'C','O','R','R',0xffu,0,1,2}}, true, 91u);
    ProjectBytes good {};
    std::size_t size = 0;
    check(EncodeProject(source, good, size, true), "corruption source encode");
    ProjectCandidate sentinel = project({{'K','E','E','P',0,0,0,0}}, true, 12u);
    const auto before = sentinel;
    for (std::size_t i = kV3CanonicalCoreBytes;
         i < kV3CanonicalCoreBytes + kV3KitIdentityModuleBytes; ++i)
    {
        auto corrupt = good;
        corrupt[i] ^= 0x55u;
        check(!DecodeProject(corrupt.data(), size, sentinel, true) &&
                  sentinel.kitName == before.kitName &&
                  sentinel.patch == before.patch,
              "CRC rejects identity-module corruption without mutation");
    }

    const std::array<std::pair<std::size_t, std::uint32_t>, 5> invalid {{
        {kV3CanonicalCoreBytes, 5u},
        {kV3CanonicalCoreBytes + 4u, 2u},
        {kV3CanonicalCoreBytes + 6u, 1u},
        {kV3CanonicalCoreBytes + 8u, 7u},
        {kV3CanonicalCoreBytes + 12u, 1u}
    }};
    for (const auto [offset, value] : invalid)
    {
        auto corrupt = good;
        if (offset == kV3CanonicalCoreBytes + 4u ||
            offset == kV3CanonicalCoreBytes + 6u)
            PutU16LE(corrupt.data() + offset, static_cast<std::uint16_t>(value));
        else
            PutU32LE(corrupt.data() + offset, value);
        rewriteCrc(corrupt);
        check(!DecodeProject(corrupt.data(), size, sentinel, true) &&
                  sentinel.kitName == before.kitName,
              "structurally invalid identity module rejects atomically");
    }
}

PatchCandidate kitCandidate(const std::array<std::uint8_t, 8>& name,
                            std::uint8_t volume)
{
    ParameterArray parameters;
    parameters.init();
    RawKit raw;
    raw.name = name;
    for (std::size_t i = 0; i < raw.values.size(); ++i)
        raw.values[i] = parameters.get(static_cast<std::uint8_t>(i));
    raw.values[88] = volume;
    CanonicalKit canonical;
    PatchCandidate candidate;
    check(DecodeRawKit(raw, canonical) && BuildPatchCandidate(canonical, candidate),
          "build identity transaction candidate");
    return candidate;
}

void authorityContract()
{
    const auto a = kitCandidate({{'K','I','T','A',0,0,0,0}}, 20u);
    const auto b = kitCandidate({{'K','I','T','B',0xffu,0,0,0}}, 110u);
    PatchAuthority patch;
    patch.initialize(a.patch);
    RoutingStateAuthority routes;
    routes.initialize();
    auto published = PublishPatchCandidate(a, {1u, 0u}, patch, routes);
    check(published && published.jointGeneration != 0u,
          "whole-kit import publishes identity with patch/routes");
    RoutingStateAuthority::Guard guard;
    RoutingStateAuthority::Snapshot snapshot;
    check(routes.tryAcquire(guard) && routes.copyLocked(guard, snapshot) &&
              snapshot.kitIdentityPresent && snapshot.kitName == a.name,
          "accepted import owns exact identity");
    routes.release(guard);

    check(routes.tryAcquire(guard), "hold identity coordinator gate");
    const auto busy = PublishPatchCandidate(
        b, {2u, routes.generation()}, patch, routes);
    check(busy.status == PublicationStatus::Busy,
          "concurrent restore gate makes kit load bounded Busy");
    routes.release(guard);
    check(routes.tryAcquire(guard) && routes.copyLocked(guard, snapshot) &&
              snapshot.kitName == a.name,
          "Busy load preserves previous identity");
    routes.release(guard);

    PersistentRoutingState restored = MakeDisabledRoutingState();
    const std::array<std::uint8_t, 8> restoredName {{0,0xffu,'R',0,'S',0,0,0}};
    check(routes.tryAcquire(guard) &&
              routes.commitRestoreLocked(guard, restored, 10u,
                                         restoredName, true) != 0u,
          "complete restore commits identity in coordinator");
    routes.release(guard);
    check(routes.tryAcquire(guard) && routes.copyLocked(guard, snapshot) &&
              snapshot.kitIdentityPresent && snapshot.kitName == restoredName,
          "restored identity visible with restored routes");
    routes.release(guard);
    check(routes.tryAcquire(guard) &&
              routes.commitRestoreLocked(guard, restored, 9u, b.name, true) == 0u,
          "stale restore rejected");
    routes.release(guard);
    check(routes.tryAcquire(guard) && routes.copyLocked(guard, snapshot) &&
              snapshot.kitName == restoredName,
          "stale restore preserves identity");
    routes.release(guard);

    check(routes.tryAcquire(guard) &&
              routes.commitRestoreLocked(guard, restored, 11u) != 0u,
          "legacy restore commits absent default identity");
    routes.release(guard);
    check(routes.tryAcquire(guard) && routes.copyLocked(guard, snapshot) &&
              !snapshot.kitIdentityPresent && snapshot.kitName ==
                  std::array<std::uint8_t, 8>{},
          "legacy restore clears name to documented default");
    routes.release(guard);

    const auto zero = kitCandidate({}, 65u);
    check(PublishPatchCandidate(zero, {12u, routes.generation()},
                                patch, routes).status == PublicationStatus::Ok,
          "all-zero named kit imports successfully");
    check(routes.tryAcquire(guard) && routes.copyLocked(guard, snapshot) &&
              snapshot.kitIdentityPresent && snapshot.kitName ==
                  std::array<std::uint8_t, 8>{},
          "explicit all-zero import retains identity provenance");
    routes.release(guard);

    allocations.store(0u);
    countAllocations.store(true);
    for (std::uint64_t order = 13u; order < 1013u; ++order)
        check(PublishPatchCandidate(b, {order, routes.generation()},
                                    patch, routes).status == PublicationStatus::Ok,
              "identity publication remains allocation free");
    countAllocations.store(false);
    check(allocations.load() == 0u, "zero identity publication allocations");
}

std::uint64_t render(const ProjectCandidate& state)
{
    Engine engine;
    ParameterArray parameters;
    parameters.init();
    check(engine.loadAssets("data"), "load native assets");
    for (std::size_t i = 0; i < state.patch.size(); ++i)
        parameters.set(static_cast<std::uint8_t>(i), state.patch[i], engine);
    engine.triggerDrum(0u, 100u, 63u);
    std::array<std::int16_t, 32> left {}, right {}, auxL {}, auxR {};
    std::uint64_t hash = UINT64_C(1469598103934665603);
    for (unsigned block = 0; block < 20u; ++block)
    {
        engine.processBlock(left.data(), right.data(), auxL.data(), auxR.data());
        for (const auto sample : left)
        {
            hash ^= static_cast<std::uint16_t>(sample);
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

void pcmContract()
{
    const auto a = project({{'N','A','M','E','A',0,0,0}}, true, 80u);
    const auto b = project({{0xffu,0,'B',0,1,2,3,4}}, true, 80u);
    ProjectBytes bytes {};
    std::size_t size = 0;
    ProjectCandidate restored;
    check(EncodeProject(b, bytes, size, true) &&
              DecodeProject(bytes.data(), size, restored, true),
          "PCM restore fixture round trip");
    const auto first = render(a);
    const auto renamed = render(b);
    const auto roundTrip = render(restored);
    check(first == renamed && renamed == roundTrip,
          "identity-only changes preserve deterministic native PCM");
    const auto changed = render(project(b.kitName, true, 20u));
    check(changed != first, "musically distinct restored kit changes PCM");
    std::printf("\"pcm_identity\":\"%016llX\",\"pcm_changed\":\"%016llX\",",
                static_cast<unsigned long long>(first),
                static_cast<unsigned long long>(changed));
}
} // namespace

void* operator new(std::size_t size) { return countedNew(size); }
void* operator new[](std::size_t size) { return countedNew(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main()
{
    formatContract();
    corruptionContract();
    authorityContract();
    std::printf("{");
    pcmContract();
    std::printf("\"pass\":%llu,\"fail\":%llu,\"publication_allocations\":%llu}\n",
                static_cast<unsigned long long>(passed),
                static_cast<unsigned long long>(failed),
                static_cast<unsigned long long>(allocations.load()));
    return failed == 0u ? 0 : 1;
}
