// Production LXRS v2, routing authority and Engine restoration integration.
// Original target identities Copyright 2013 Julian Schmidt; LICENSE-LXR.txt.
#include "../vendor/iPlug2/Examples/LXR/LXRState.h"
#include "../dsp/Engine.h"
#include "../dsp/ParameterArray.h"
#include "../dsp/ParameterStateAuthority.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace
{
int passed = 0;
int failed = 0;
const char* group = "startup";

void check(const char* name, bool condition)
{
    if (condition)
        ++passed;
    else
    {
        ++failed;
        std::printf("FAIL [%s] %s\n", group, name);
    }
}
std::vector<std::uint8_t> readFile(const std::string& path)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr)
        return {};
    std::fseek(file, 0, SEEK_END);
    const long length = std::ftell(file);
    std::rewind(file);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    if (std::fread(bytes.data(), 1, bytes.size(), file) != bytes.size())
        bytes.clear();
    std::fclose(file);
    return bytes;
}

lxr::PersistentRouteRecord makeRoute(unsigned source, unsigned owner,
                                     unsigned index)
{
    auto record = lxr::MakeDisabledRoute(static_cast<std::uint8_t>(source),
                                         static_cast<std::uint8_t>(owner));
    if (index == 0)
        return record;
    const auto* target = lxr::modulation_contract::lookup(
        lxr::modulation_contract::OriginalTargetIndex::fromRaw(index));
    record.flags = 1;
    record.identityNamespace = 1;
    record.originalIndex = static_cast<std::uint16_t>(index);
    if (target != nullptr)
    {
        record.firmwareParameter = target->parameter.value();
        record.valueType = static_cast<std::uint8_t>(target->valueType);
        record.scope = static_cast<std::uint8_t>(target->scope);
        record.physicalKey = lxr::PersistentPhysicalKey(*target);
        record.menuVoiceHint = source == 1 ? static_cast<std::uint8_t>(owner + 1) : 0;
    }
    return record;
}

bool sameRoute(const lxr::PersistentRouteRecord& a,
               const lxr::PersistentRouteRecord& b)
{
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}

bool sameRouting(const lxr::PersistentRoutingState& a,
                 const lxr::PersistentRoutingState& b)
{
    if (a.optionalTlvBytes != b.optionalTlvBytes || a.bypass != b.bypass)
        return false;
    for (std::size_t i = 0; i < a.routes.size(); ++i)
        if (!sameRoute(a.routes[i], b.routes[i])) return false;
    return std::equal(a.optionalTlvs.begin(),
                      a.optionalTlvs.begin() + a.optionalTlvBytes,
                      b.optionalTlvs.begin());
}

void recalc(std::vector<std::uint8_t>& bytes)
{
    const std::size_t pluginBytes = lxr::state::GetU32LE(bytes.data() + 8);
    lxr::state::PutU32LE(bytes.data() + 24,
                        lxr::state::Crc32V2(bytes.data(), pluginBytes));
}

void rejectWithoutMutation(const char* name,
                           const std::vector<std::uint8_t>& bytes,
                           lxr::state::DecodeStatus expected,
                           bool transport)
{
    std::array<std::uint8_t, lxr::state::kParameterCount> patch {};
    patch.fill(73);
    auto routing = lxr::MakeDisabledRoutingState();
    routing.routes[0] = makeRoute(0, 0, 2);
    routing.bypass = true;
    const auto beforePatch = patch;
    const auto beforeRouting = routing;
    const auto result = lxr::state::DecodePersistentState(
        bytes.data(), bytes.size(), patch, routing, transport);
    check(name, result.status == expected);
    check("invalid decode leaves patch unchanged", patch == beforePatch);
    check("invalid decode leaves routes unchanged", sameRouting(routing, beforeRouting));
}

bool applyRoutes(lxr::Engine& engine,
                 const lxr::PersistentRoutingState& routing)
{
    using namespace lxr::modulation_contract;
    for (const auto& route : routing.routes)
    {
        const SourceIdentity source {
            route.source == 0 ? SourceKind::Velocity : SourceKind::Lfo,
            voiceFromZeroBased(route.owner)
        };
        const auto target = OriginalTargetIndex::fromRaw(route.originalIndex);
        const auto status = route.source == 0
            ? engine.configureVelocityDestination(source, target)
            : engine.configureLfoDestination(source, target);
        if (status != lxr::VelocityRouteStatus::Ok)
            return false;
    }
    return true;
}

void codec(const std::string& fixtures)
{
    group = "codec";
    const auto active = readFile(fixtures + "/v2_active.bin");
    const auto off = readFile(fixtures + "/v2_off.bin");
    check("independent fixture sizes", active.size() == 520 && off.size() == 516);
    const std::uint8_t vector[] = {'1','2','3','4','5','6','7','8','9'};
    check("IEEE CRC vector", lxr::state::Crc32V2(vector, 9) == 0xcbf43926u);

    std::array<std::uint8_t, 228> patch {};
    auto routing = lxr::MakeDisabledRoutingState();
    auto decoded = lxr::state::DecodePersistentState(
        active.data(), active.size(), patch, routing, true);
    check("decode independent v2 transport", static_cast<bool>(decoded));
    check("v2 consumed plugin bytes", decoded.consumed == 516);
    check("v2 bypass suffix", routing.bypass);
    for (std::size_t i = 0; i < patch.size(); ++i)
        check("all fixture patch bytes", patch[i] == (i * 37u + 11u) % 128u);
    for (std::size_t i = 0; i < routing.routes.size(); ++i)
    {
        check("all twelve routes enabled", routing.routes[i].flags == 1);
        check("canonical route order",
              routing.routes[i].source * 6u + routing.routes[i].owner == i);
    }
    check("full-width index above 127", routing.routes[5].originalIndex == 203);

    std::array<std::uint8_t, lxr::state::kMaximumStateSize> encoded {};
    std::size_t encodedSize = 0;
    check("encode production v2 transport",
          lxr::state::EncodeV2(patch, routing, encoded, encodedSize, true));
    check("exact 520 byte stream", encodedSize == 520);
    check("byte-equal independent fixture",
          std::equal(active.begin(), active.end(), encoded.begin()));
    std::size_t againSize = 0;
    std::array<std::uint8_t, lxr::state::kMaximumStateSize> again {};
    check("deterministic second encode",
          lxr::state::EncodeV2(patch, routing, again, againSize, true) &&
          againSize == encodedSize && again == encoded);
    check("header offsets",
          lxr::state::GetU16LE(encoded.data() + 4) == 2 &&
          lxr::state::GetU16LE(encoded.data() + 6) == 32 &&
          lxr::state::GetU32LE(encoded.data() + 8) == 516 &&
          lxr::state::GetU32LE(encoded.data() + 16) == 228 &&
          lxr::state::GetU32LE(encoded.data() + 20) == 256);
    check("routing TLV offsets",
          lxr::state::GetU32LE(encoded.data() + 260) == 1 &&
          lxr::state::GetU32LE(encoded.data() + 268) == 240 &&
          lxr::state::GetU16LE(encoded.data() + 280) == 12);

    auto offRouting = lxr::MakeDisabledRoutingState();
    decoded = lxr::state::DecodePersistentState(
        off.data(), off.size(), patch, offRouting, false);
    check("exact 516 byte core", static_cast<bool>(decoded));
    for (const auto& route : offRouting.routes)
        check("explicit canonical off", lxr::ValidatePersistentRoute(route) ==
              lxr::PersistentRouteStatus::Disabled);

    // Preserve an unknown optional record byte-for-byte.
    auto optional = off;
    optional.resize(536, 0);
    lxr::state::PutU32LE(optional.data() + 8, 536);
    lxr::state::PutU32LE(optional.data() + 20, 276);
    lxr::state::PutU32LE(optional.data() + 516, 2);
    lxr::state::PutU16LE(optional.data() + 520, 77);
    lxr::state::PutU32LE(optional.data() + 524, 4);
    optional[532] = 1; optional[533] = 2; optional[534] = 3; optional[535] = 4;
    recalc(optional);
    auto optionalRouting = lxr::MakeDisabledRoutingState();
    decoded = lxr::state::DecodePersistentState(
        optional.data(), optional.size(), patch, optionalRouting, false);
    check("unknown optional accepted", static_cast<bool>(decoded));
    check("unknown optional retained", optionalRouting.optionalTlvBytes == 20);
    check("unknown optional re-encode",
          lxr::state::EncodeV2(patch, optionalRouting, encoded, encodedSize, false) &&
          encodedSize == optional.size() &&
          std::equal(optional.begin(), optional.end(), encoded.begin()));
    optional[522] = 1; recalc(optional);
    rejectWithoutMutation("unknown mandatory rejected", optional,
        lxr::state::DecodeStatus::UnsupportedMandatoryTag, false);

    auto maximum = lxr::MakeDisabledRoutingState();
    maximum.optionalTlvBytes = 3576;
    lxr::state::PutU32LE(maximum.optionalTlvs.data(), 2);
    lxr::state::PutU16LE(maximum.optionalTlvs.data() + 4, 1);
    lxr::state::PutU32LE(maximum.optionalTlvs.data() + 8, 3560);
    std::fill(maximum.optionalTlvs.begin() + 16, maximum.optionalTlvs.end(), 0x5a);
    check("maximum 4096 stream",
          lxr::state::EncodeV2(patch, maximum, encoded, encodedSize, true) &&
          encodedSize == 4096);
}

void migration(const std::string& fixtures)
{
    group = "migration";
    for (const char* filename : {"v1.bin", "legacy.bin"})
    {
        const auto bytes = readFile(fixtures + "/" + filename);
        std::array<std::uint8_t, 228> patch {};
        auto routing = lxr::MakeDisabledRoutingState();
        const auto result = lxr::state::DecodePersistentState(
            bytes.data(), bytes.size(), patch, routing, true);
        check("legacy format accepted", static_cast<bool>(result));
        check("legacy bypass preserved", routing.bypass);
        for (std::size_t i = 0; i < patch.size(); ++i)
            check("legacy patch exact", patch[i] == (i * 37u + 11u) % 128u);
        for (const auto& route : routing.routes)
            check("legacy never infers routes",
                  lxr::ValidatePersistentRoute(route) ==
                      lxr::PersistentRouteStatus::Disabled);
        std::array<std::uint8_t, lxr::state::kMaximumStateSize> encoded {};
        std::size_t encodedSize = 0;
        check("migration saves v2",
              lxr::state::EncodeV2(patch, routing, encoded, encodedSize, true) &&
              encodedSize == 520);
    }
}

void routeCoverage()
{
    group = "routing";
    unsigned supported = 0;
    unsigned above127 = 0;
    std::array<unsigned, 6> ownerCounts {};
    for (unsigned source = 0; source < 2; ++source)
    {
        for (unsigned owner = 0; owner < 6; ++owner)
        {
            for (unsigned index = 0; index <= 204; ++index)
            {
                const auto record = makeRoute(source, owner, index);
                const auto status = lxr::ValidatePersistentRoute(record);
                if (status == lxr::PersistentRouteStatus::Ok && source == 0)
                {
                    ++supported;
                    ++ownerCounts[owner];
                    if (index > 127) ++above127;
                }
                if (status == lxr::PersistentRouteStatus::Ok)
                {
                    auto state = lxr::MakeDisabledRoutingState();
                    state.routes[source * 6u + owner] = record;
                    check("every verified destination validates",
                          lxr::ValidatePersistentRoutes(state.routes) ==
                              lxr::PersistentRouteStatus::Ok);
                }
            }
        }
    }
    check("107 verified own-voice destinations", supported == 107);
    check("39 verified indices above 127", above127 == 39);
    for (unsigned owner = 0; owner < 6; ++owner)
        check("six owner coverage", ownerCounts[owner] == (owner == 5 ? 17u : 18u));
    check("index 127 remains full-width and rejected by profile",
          makeRoute(1, 3, 127).originalIndex == 127 &&
          lxr::ValidatePersistentRoute(makeRoute(1, 3, 127)) !=
              lxr::PersistentRouteStatus::Ok);
    check("index 128 remains full-width and rejected by profile",
          makeRoute(1, 3, 128).originalIndex == 128 &&
          lxr::ValidatePersistentRoute(makeRoute(1, 3, 128)) !=
              lxr::PersistentRouteStatus::Ok);
    check("highest supported index 203",
          lxr::ValidatePersistentRoute(makeRoute(1, 5, 203)) ==
              lxr::PersistentRouteStatus::Ok);
    check("row 204 identifiable but unsupported",
          makeRoute(1, 5, 204).originalIndex == 204 &&
          lxr::ValidatePersistentRoute(makeRoute(1, 5, 204)) !=
              lxr::PersistentRouteStatus::Ok);
    auto cross = makeRoute(0, 1, 2);
    check("cross voice rejected", lxr::ValidatePersistentRoute(cross) ==
          lxr::PersistentRouteStatus::UnsupportedRoute);
    auto unsafe = makeRoute(0, 0, 11);
    check("unsafe destination storage rejected",
          lxr::ValidatePersistentRoute(unsafe) ==
              lxr::PersistentRouteStatus::UnsupportedRoute);
}

void corruption(const std::string& fixtures)
{
    group = "corruption";
    const auto golden = readFile(fixtures + "/v2_off.bin");
    rejectWithoutMutation("empty", {}, lxr::state::DecodeStatus::Empty, false);
    for (std::size_t length : {4u, 7u, 31u, 515u})
        rejectWithoutMutation("truncated", std::vector<std::uint8_t>(
            golden.begin(), golden.begin() + length),
            lxr::state::DecodeStatus::InvalidLength, false);
    rejectWithoutMutation("oversized", std::vector<std::uint8_t>(4097, 0),
        lxr::state::DecodeStatus::TooLarge, false);
    auto bytes = golden;
    lxr::state::PutU16LE(bytes.data() + 4, 3);
    rejectWithoutMutation("unsupported version", bytes,
        lxr::state::DecodeStatus::UnsupportedVersion, false);
    bytes = golden; bytes[32] ^= 1;
    rejectWithoutMutation("bad CRC", bytes,
        lxr::state::DecodeStatus::InvalidChecksum, false);
    bytes = golden; lxr::state::PutU32LE(bytes.data() + 268, 239); recalc(bytes);
    rejectWithoutMutation("bad TLV length", bytes,
        lxr::state::DecodeStatus::InvalidLength, false);
    bytes = golden;
    std::copy(bytes.begin() + 324, bytes.begin() + 340, bytes.begin() + 340);
    recalc(bytes);
    rejectWithoutMutation("duplicate mandatory record", bytes,
        lxr::state::DecodeStatus::InvalidRoutingRecord, false);
    bytes = golden; bytes[324] = 6; recalc(bytes);
    rejectWithoutMutation("invalid route identity", bytes,
        lxr::state::DecodeStatus::InvalidRoutingRecord, false);
    bytes = golden; bytes.push_back(0);
    rejectWithoutMutation("unexpected trailing data", bytes,
        lxr::state::DecodeStatus::UnexpectedTrailingBytes, false);
}

void authority()
{
    group = "authority";
    using Patch = lxr::ParameterStateAuthority<228>;
    Patch patch;
    Patch::Values initial {};
    patch.initialize(initial);
    lxr::RoutingStateAuthority routes;
    routes.initialize();

    lxr::RoutingStateAuthority::Guard held;
    check("first bounded gate attempt", routes.tryAcquire(held));
    lxr::RoutingStateAuthority::Guard contended;
    check("contended gate returns immediately", !routes.tryAcquire(contended));
    routes.release(held);

    auto state = lxr::MakeDisabledRoutingState();
    state.routes[0] = makeRoute(0, 0, 34);
    lxr::RoutingStateAuthority::Guard outer;
    check("restore gate", routes.tryAcquire(outer));
    check("restore preflight", routes.validateRestoreLocked(outer, state, 10) ==
          lxr::PersistentRouteStatus::Ok);
    Patch::WriteHandle writer;
    check("patch writer under outer", patch.tryBeginWrite(writer));
    Patch::Values restored {}; restored.fill(17);
    check("whole patch candidate", patch.writeAll(writer, restored));
    patch.commit(writer);
    check("joint restore commit", routes.commitRestoreLocked(outer, state, 10) == 1);
    routes.release(outer);

    check("route edit gate", routes.tryAcquire(outer));
    check("stale pre-restore edit rejected",
          routes.commitRouteLocked(outer, lxr::MakeDisabledRoute(0, 0), 9, 1) ==
              lxr::PersistentRouteStatus::Stale);
    check("new route edit accepted",
          routes.commitRouteLocked(outer, lxr::MakeDisabledRoute(0, 0), 11, 1) ==
              lxr::PersistentRouteStatus::Disabled);
    routes.release(outer);

    for (std::uint64_t order = 12; order < 112; ++order)
    {
        auto candidate = lxr::MakeDisabledRoutingState();
        candidate.routes[(order - 12) % 12] = makeRoute(
            static_cast<unsigned>((order - 12) % 12 / 6),
            static_cast<unsigned>((order - 12) % 6),
            ((order - 12) % 6) * 34 + 34);
        if (lxr::ValidatePersistentRoutes(candidate.routes) !=
            lxr::PersistentRouteStatus::Ok)
            candidate = lxr::MakeDisabledRoutingState();
        check("rapid restore acquire", routes.tryAcquire(outer));
        check("rapid restore publish", routes.commitRestoreLocked(
            outer, candidate, order) != 0);
        routes.release(outer);
    }

    std::atomic<int> coherent {0};
    std::atomic<int> busy {0};
    auto reader = [&]() {
        for (int i = 0; i < 2000; ++i)
        {
            lxr::RoutingStateAuthority::Guard guard;
            if (!routes.tryAcquire(guard)) { ++busy; continue; }
            lxr::RoutingStateAuthority::Snapshot snapshot;
            if (routes.copyLocked(guard, snapshot) &&
                lxr::ValidatePersistentRoutes(snapshot.state.routes) ==
                    lxr::PersistentRouteStatus::Ok)
                ++coherent;
        }
    };
    std::thread a(reader), b(reader);
    a.join(); b.join();
    check("concurrent snapshots remain complete", coherent.load() > 0);
    check("bounded contention observable", busy.load() >= 0);

    lxr::RoutingStateAuthority other;
    other.initialize();
    lxr::RoutingStateAuthority::Snapshot first, second;
    check("instance one snapshot", routes.tryAcquire(outer));
    routes.copyLocked(outer, first); routes.release(outer);
    lxr::RoutingStateAuthority::Guard otherGuard;
    check("instance two snapshot", other.tryAcquire(otherGuard));
    other.copyLocked(otherGuard, second);
    check("multi-instance isolation", first.generation != second.generation);
}

void audio(const std::string& dataDirectory,
           const std::string& fixtures)
{
    group = "audio";
    const auto activeBytes = readFile(fixtures + "/v2_active.bin");
    const auto offBytes = readFile(fixtures + "/v2_off.bin");
    std::array<std::uint8_t, 228> patch {};
    auto activeRouting = lxr::MakeDisabledRoutingState();
    auto offRouting = lxr::MakeDisabledRoutingState();
    check("active decode for audio", static_cast<bool>(
        lxr::state::DecodePersistentState(activeBytes.data(), activeBytes.size(),
                                          patch, activeRouting, true)));
    std::array<std::uint8_t, 228> offPatch {};
    check("off decode for audio", static_cast<bool>(
        lxr::state::DecodePersistentState(offBytes.data(), offBytes.size(),
                                          offPatch, offRouting, false)));

    lxr::Engine restored, direct, offA, offB;
    check("assets restored", restored.loadAssets(dataDirectory.c_str()));
    check("assets direct", direct.loadAssets(dataDirectory.c_str()));
    check("assets off A", offA.loadAssets(dataDirectory.c_str()));
    check("assets off B", offB.loadAssets(dataDirectory.c_str()));
    lxr::ParameterArray restoredParams, directParams, offParamsA, offParamsB;
    restoredParams.init(); directParams.init(); offParamsA.init(); offParamsB.init();
    for (std::size_t i = 0; i < patch.size(); ++i)
    {
        restoredParams.set(static_cast<std::uint8_t>(i), patch[i], restored);
        directParams.set(static_cast<std::uint8_t>(i), patch[i], direct);
        offParamsA.set(static_cast<std::uint8_t>(i), offPatch[i], offA);
        offParamsB.set(static_cast<std::uint8_t>(i), offPatch[i], offB);
    }
    check("restored all twelve native routes", applyRoutes(restored, activeRouting));
    check("direct all twelve native routes", applyRoutes(direct, activeRouting));
    check("off route application", applyRoutes(offA, offRouting));
    restored.triggerDrum(0, 101, 48); direct.triggerDrum(0, 101, 48);
    offA.triggerSnare(100, 60); offB.triggerSnare(100, 60);
    for (int block = 0; block < 8; ++block)
    {
        std::array<std::int16_t, 32> al{}, ar{}, a2l{}, a2r{};
        std::array<std::int16_t, 32> bl{}, br{}, b2l{}, b2r{};
        restored.processBlock(al.data(), ar.data(), a2l.data(), a2r.data());
        direct.processBlock(bl.data(), br.data(), b2l.data(), b2r.data());
        check("active restored audio deterministic",
              al == bl && ar == br && a2l == b2l && a2r == b2r);
        offA.processBlock(al.data(), ar.data(), a2l.data(), a2r.data());
        offB.processBlock(bl.data(), br.data(), b2l.data(), b2r.data());
        check("routes off preserve native audio",
              al == bl && ar == br && a2l == b2l && a2r == b2r);
    }
    for (std::size_t i = 0; i < patch.size(); ++i)
        check("modulation never mutates base patch",
              restoredParams.get(static_cast<std::uint8_t>(i)) == patch[i]);
}
}

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    codec(argv[1]);
    migration(argv[1]);
    routeCoverage();
    corruption(argv[1]);
    authority();
    audio(argv[2], argv[1]);
    std::printf("SUMMARY pass=%d fail=%d routes=12 supported=107 stream=520\n",
                passed, failed);
    return failed == 0 ? 0 : 1;
}
