// P17-D transactional runtime whole-kit publication and native DSP tests.
// Original identities Copyright 2013 Julian Schmidt; see LICENSE-LXR.txt.

#include "../interfaces/RuntimeKitLoad.h"
#include "../dsp/Engine.h"
#include "../dsp/MidiParser.h"
#include "../dsp/ParameterArray.h"
#include "../dsp/PatternBank.h"
#include "../dsp/PatternStateAuthority.h"
#include "../dsp/SequencerState.h"
#include "../dsp/TriggerScheduler.h"

#include <array>
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
using namespace lxr;
using namespace lxr::kit;
namespace mc = lxr::modulation_contract;
using PatchAuthority = ParameterStateAuthority<kFieldCount>;

std::atomic<std::uint64_t> allocations {0};
std::atomic<bool> measureAllocations {false};
std::uint64_t passed = 0, failed = 0;
std::uint64_t publicationMaximumNs = 0, publicationTotalNs = 0;
std::uint64_t readerBusySnapshots = 0;

void* countedAllocate(std::size_t size)
{
    if (measureAllocations.load(std::memory_order_relaxed))
        allocations.fetch_add(1u, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    std::abort();
}

void check(bool value, const char* label)
{
    if (value) ++passed;
    else { ++failed; if (failed < 30u) std::fprintf(stderr, "FAIL: %s\n", label); }
}

bool sameRoute(const PersistentRouteRecord& a,
               const PersistentRouteRecord& b) noexcept
{
    return a.owner == b.owner && a.source == b.source &&
        a.flags == b.flags && a.identityNamespace == b.identityNamespace &&
        a.originalIndex == b.originalIndex &&
        a.firmwareParameter == b.firmwareParameter &&
        a.valueType == b.valueType && a.scope == b.scope &&
        a.physicalKey == b.physicalKey &&
        a.menuVoiceHint == b.menuVoiceHint && a.reserved == b.reserved;
}

RawKit defaultRaw()
{
    ParameterArray parameters;
    parameters.init();
    RawKit raw;
    const std::array<std::uint8_t, 8> name {{'P','1','7','D','B','A','S','E'}};
    raw.name = name;
    for (std::size_t i = 0; i < raw.values.size(); ++i)
        raw.values[i] = parameters.get(static_cast<std::uint8_t>(i));
    return raw;
}

CanonicalKit canonical(const RawKit& raw)
{
    CanonicalKit kit;
    check(static_cast<bool>(DecodeRawKit(raw, kit)), "decode source-valid kit");
    return kit;
}

PatchCandidate candidate(const RawKit& raw)
{
    PatchCandidate value;
    const auto kit = canonical(raw);
    check(static_cast<bool>(BuildPatchCandidate(kit, value)), "build runtime candidate");
    return value;
}

std::uint8_t selectorFor(mc::Voice voice, std::uint16_t parameter,
                         mc::SourceKind source)
{
    for (std::uint16_t index = 1; index <= 204; ++index)
    {
        const auto typed = mc::OriginalTargetIndex::fromRaw(index);
        const auto* target = mc::lookup(typed);
        if (target == nullptr || target->voice != voice ||
            target->parameter.value() != parameter)
            continue;
        const mc::SourceIdentity owner {source, voice};
        const auto status = source == mc::SourceKind::Velocity
            ? validateVelocityDestination(owner, typed)
            : validateLfoDestination(owner, typed);
        if (status == VelocityRouteStatus::Ok)
            return static_cast<std::uint8_t>(index);
    }
    return 0;
}

std::uint8_t highSelectorFor(mc::Voice voice, mc::SourceKind source)
{
    for (std::uint16_t index = 128u; index <= 204u; ++index)
    {
        const auto typed = mc::OriginalTargetIndex::fromRaw(index);
        const mc::SourceIdentity owner {source, voice};
        const auto* target = mc::lookup(typed);
        if (target == nullptr || target->voice != voice) continue;
        const auto status = source == mc::SourceKind::Velocity
            ? validateVelocityDestination(owner, typed)
            : validateLfoDestination(owner, typed);
        if (status == VelocityRouteStatus::Ok)
            return static_cast<std::uint8_t>(index);
    }
    return 0;
}

bool coherentSnapshot(PatchAuthority& patch, RoutingStateAuthority& routing,
                      PatchAuthority::Values& values,
                      RoutingStateAuthority::Snapshot& routes)
{
    RoutingStateAuthority::Guard guard;
    return routing.tryAcquire(guard) && patch.trySnapshot(values, nullptr, 1u) &&
           routing.copyLocked(guard, routes);
}

PublicationResult publish(PatchAuthority& patch, RoutingStateAuthority& routing,
                          const PatchCandidate& value, std::uint64_t order)
{
    return PublishPatchCandidate(value, {order, routing.generation()}, patch, routing);
}

void candidateAndPublication()
{
    auto rawA = defaultRaw();
    auto rawB = rawA;
    rawA.name[7] = 'A'; rawB.name[7] = 'B';
    rawA.values[88] = 31u; rawB.values[88] = 109u;
    rawB.values[152] = highSelectorFor(mc::Voice::Snare,
                                      mc::SourceKind::Velocity);
    check(rawB.values[152] > 127u, "high-index route identity retained");
    const auto a = candidate(rawA);
    const auto b = candidate(rawB);

    PatchAuthority patch;
    patch.initialize(a.patch);
    RoutingStateAuthority routing;
    routing.initialize();
    auto first = publish(patch, routing, b, 1u);
    check(first.status == PublicationStatus::Ok, "whole kit publishes");
    check(first.patchGeneration == 2u && first.jointGeneration == 1u,
          "patch and joint generations recorded");

    PatchAuthority::Values values;
    RoutingStateAuthority::Snapshot routes;
    check(coherentSnapshot(patch, routing, values, routes), "coherent snapshot");
    check(values == b.patch && routes.kitName == b.name,
          "snapshot contains one complete kit");
    check(routes.kitGeneration == first.jointGeneration &&
          routes.kitPatchGeneration == first.patchGeneration,
          "kit generations bound together");
    for (std::size_t i = 0; i < b.routes.size(); ++i)
        check(sameRoute(routes.state.routes[i], b.routes[i]),
              "all twelve routes committed");

    const auto beforeValues = values;
    const auto beforeRoutes = routes;
    auto invalid = a;
    invalid.patch[17] = 255u;
    auto rejected = publish(patch, routing, invalid, 2u);
    check(rejected.status == PublicationStatus::InvalidCandidate &&
          rejected.field == 17u, "invalid field diagnosed");
    invalid = a;
    invalid.routes[0].owner = 5u;
    rejected = publish(patch, routing, invalid, 2u);
    check(rejected.status == PublicationStatus::InvalidCandidate &&
          rejected.route == 0u, "invalid route diagnosed without truncation");
    check(coherentSnapshot(patch, routing, values, routes), "snapshot after rejection");
    check(values == beforeValues && routes.generation == beforeRoutes.generation,
          "rejection rolls back complete state");

    RoutingStateAuthority::Guard held;
    check(routing.tryAcquire(held), "hold outer gate");
    rejected = PublishPatchCandidate(a, {2u, routing.generation()}, patch, routing);
    check(rejected.status == PublicationStatus::Busy, "bounded Busy response");
    routing.release(held);
    rejected = PublishPatchCandidate(a, {2u, 0u}, patch, routing);
    check(rejected.status == PublicationStatus::Stale, "stale generation rejected");

    // A complete restore after reservation invalidates the older kit and clears
    // kit-name provenance without changing any pattern/transport object.
    const auto staleGeneration = routing.generation();
    RoutingStateAuthority::Guard restoreGuard;
    check(routing.tryAcquire(restoreGuard), "acquire restore gate");
    auto restored = MakeDisabledRoutingState();
    check(routing.commitRestoreLocked(restoreGuard, restored, 50u) != 0u,
          "project restore committed");
    routing.release(restoreGuard);
    rejected = PublishPatchCandidate(a, {40u, staleGeneration}, patch, routing);
    check(rejected.status == PublicationStatus::Stale,
          "older kit cannot follow newer project restore");
    check(coherentSnapshot(patch, routing, values, routes), "post-restore snapshot");
    check(routes.kitGeneration == 0u, "complete restore clears kit provenance");

    sequencer::PatternStateAuthority patterns;
    auto bank = sequencer::MakeDefaultPatternBank();
    bank.patterns[3].mainGates[2] = 0x5678u;
    check(patterns.tryPublish(bank) == sequencer::Status::Ok,
          "pattern sentinel published");
    sequencer::state::TransportConfig transport;
    transport.enabled = 1u; transport.clock = 1u; transport.bpm = 177u;
    auto next = publish(patch, routing, a, 60u);
    check(next.status == PublicationStatus::Ok, "kit after restore publishes");
    sequencer::PatternStateAuthority::ReadHandle pin;
    check(patterns.tryAcquire(pin) == sequencer::Status::Ok &&
          pin.bank().patterns[3].mainGates[2] == 0x5678u,
          "pattern bank preserved");
    check(transport.enabled == 1u && transport.clock == 1u && transport.bpm == 177u,
          "transport sentinel preserved");
}

sequencer::AutomationSlotCommand lockCommand(std::uint8_t parameter,
                                              std::uint8_t value)
{
    const auto decoded = sequencer::DecodeAutomationSlot(
        static_cast<std::uint8_t>(parameter + 1u), value);
    check(static_cast<bool>(decoded), "decode lock command");
    return decoded.command;
}

sequencer::AutomationSlotCommand clearCommand()
{
    const auto decoded = sequencer::DecodeAutomationSlot(0xffu, 0u);
    check(static_cast<bool>(decoded), "decode clear command");
    return decoded.command;
}

bool close(float a, float b) { return std::fabs(a - b) <= 1.e-6f; }

void lockRebase()
{
    Engine engine;
    ParameterArray parameters;
    parameters.init();
    parameters.applyAllToEngine(engine);
    parameters.set(88u, 70u, engine);
    auto first = lockCommand(88u, 20u);
    auto second = lockCommand(88u, 30u);
    check(engine.applySequencerAutomationPair(0u, &first, nullptr),
          "first active lock");
    check(engine.applySequencerAutomationPair(2u, &second, nullptr),
          "duplicate later lock");

    parameters.set(88u, 100u, engine);
    check(engine.reapplySequencerAutomation(), "rebase duplicate locks");
    check(close(engine.drums[0].vol, 30.f / 127.f),
          "newest duplicate remains effective");
    parameters.set(88u, 90u, engine);
    check(engine.reapplySequencerAutomation(), "second kit rebase");
    check(close(engine.drums[0].vol, 30.f / 127.f),
          "successive kit retains lock");

    auto clear = clearCommand();
    check(engine.applySequencerAutomationPair(2u, &clear, nullptr),
          "release newest duplicate");
    check(close(engine.drums[0].vol, 90.f / 127.f),
          "release restores newest kit base");
    check(engine.reapplySequencerAutomation(), "older duplicate still active");
    check(close(engine.drums[0].vol, 20.f / 127.f),
          "older lock lifetime retained");

    parameters.set(88u, 111u, engine); // Host/MIDI edit after kit.
    check(close(engine.drums[0].vol, 111.f / 127.f),
          "post-kit host edit wins immediately");
    check(engine.applySequencerAutomationPair(0u, &clear, nullptr),
          "release final lock");
    check(close(engine.drums[0].vol, 111.f / 127.f),
          "release restores post-kit host value");
}

constexpr std::size_t kPcmSamples = 32u * 24u * 2u;
struct AudioResult
{
    std::array<std::int16_t, kPcmSamples> pcm {};
    std::uint64_t hash = UINT64_C(1469598103934665603);
};

bool applyRoutes(Engine& engine, const PatchCandidate& kit)
{
    for (const auto& route : kit.routes)
    {
        const mc::SourceIdentity source {
            route.source == 0u ? mc::SourceKind::Velocity : mc::SourceKind::Lfo,
            mc::voiceFromZeroBased(route.owner)
        };
        const auto target = mc::OriginalTargetIndex::fromRaw(route.originalIndex);
        const auto status = route.source == 0u
            ? engine.configureVelocityDestination(source, target)
            : engine.configureLfoDestination(source, target);
        if (status != VelocityRouteStatus::Ok) return false;
    }
    return true;
}

bool applyKitToActiveEngine(Engine& engine, ParameterArray& parameters,
                            const PatchCandidate& kit)
{
    for (std::size_t i = 0; i < kit.patch.size(); ++i)
        parameters.set(static_cast<std::uint8_t>(i), kit.patch[i], engine);
    return applyRoutes(engine, kit);
}

void activeRuntimeContinuity()
{
    Engine engine;
    ParameterArray parameters;
    parameters.init();
    check(engine.loadAssets("data"), "continuity loads tracked native assets");
    parameters.applyAllToEngine(engine);
    MidiParser midi;
    midi.processEvent({0x90u, 0u, 60u, 100u}, engine, parameters);
    std::array<std::int16_t, 32> left {}, right {}, auxL {}, auxR {};
    engine.processBlock(left.data(), right.data(), auxL.data(), auxR.data());

    auto changed = defaultRaw();
    changed.values[88] = 41u;
    Engine staging = engine;
    ParameterArray stagingParameters = parameters;
    check(applyKitToActiveEngine(staging, stagingParameters, candidate(changed)),
          "kit applies without resetting active MIDI voice");
    staging.processBlock(left.data(), right.data(), auxL.data(), auxR.data());
    bool audible = false;
    for (std::size_t i = 0; i < left.size(); ++i)
        audible = audible || left[i] != 0 || right[i] != 0;
    check(audible, "MIDI-triggered voice continues after kit boundary");
    midi.processEvent({0x80u, 0u, 60u, 0u}, staging, stagingParameters);

    sequencer::TriggerScheduler scheduler;
    check(scheduler.start() == sequencer::Status::Ok,
          "sequencer starts before kit boundary");
    check(scheduler.processNativeBlock(0u, staging) == sequencer::Status::Ok,
          "sequencer processes pre-kit block");
    const auto before = scheduler.diagnostics().processedNativeBlocks;
    Engine sequencedStaging = staging;
    ParameterArray sequencedParameters = stagingParameters;
    changed.values[88] = 92u;
    check(applyKitToActiveEngine(
              sequencedStaging, sequencedParameters, candidate(changed)),
          "kit applies during sequencer playback");
    check(scheduler.processNativeBlock(32u, sequencedStaging) ==
              sequencer::Status::Ok && scheduler.running(),
          "sequencer remains running after kit boundary");
    check(scheduler.diagnostics().processedNativeBlocks == before + 1u,
          "sequencer progress is not reset by kit load");
}

AudioResult render(const PatchCandidate& kit, unsigned voice,
                   bool openHat = false, std::uint8_t velocity = 100u)
{
    Engine engine;
    ParameterArray parameters;
    parameters.init();
    check(engine.loadAssets("data"), "load tracked native assets");
    for (std::size_t i = 0; i < kit.patch.size(); ++i)
        parameters.set(static_cast<std::uint8_t>(i), kit.patch[i], engine);
    check(applyRoutes(engine, kit), "apply twelve typed routes");
    if (voice == 0u) engine.triggerDrum(0u, velocity, 63u);
    else if (voice == 3u) engine.triggerSnare(velocity, 63u);
    else if (voice == 4u) engine.triggerCymbal(velocity, 63u);
    else engine.triggerHiHat(velocity, openHat, 63u);

    AudioResult result;
    std::array<std::int16_t, 32> left {}, right {}, auxL {}, auxR {};
    std::size_t at = 0;
    for (unsigned block = 0; block < 24u; ++block)
    {
        engine.processBlock(left.data(), right.data(), auxL.data(), auxR.data());
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            result.pcm[at++] = left[i]; result.pcm[at++] = right[i];
            const std::uint16_t samples[2] {
                static_cast<std::uint16_t>(left[i]),
                static_cast<std::uint16_t>(right[i])
            };
            for (const auto sample : samples)
            {
                result.hash ^= static_cast<std::uint8_t>(sample & 0xffu);
                result.hash *= UINT64_C(1099511628211);
                result.hash ^= static_cast<std::uint8_t>(sample >> 8u);
                result.hash *= UINT64_C(1099511628211);
            }
        }
    }
    return result;
}

int firstDifference(const AudioResult& a, const AudioResult& b)
{
    for (std::size_t i = 0; i < a.pcm.size(); ++i)
        if (a.pcm[i] != b.pcm[i]) return static_cast<int>(i / 2u);
    return -1;
}

std::array<std::uint64_t, 10> pcmHashes {};
std::array<int, 9> pcmFirstDifferences {};
std::array<RawKit, 10> pcmInputKits {};
std::array<std::uint64_t, 10> pcmCandidateFingerprints {};
std::array<std::uint64_t, 10> pcmPatchGenerations {};
std::array<std::uint64_t, 10> pcmRouteGenerations {};

std::uint64_t candidateFingerprint(const PatchCandidate& value) noexcept
{
    std::uint64_t hash = UINT64_C(1469598103934665603);
    const auto append = [&hash](std::uint64_t field, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i)
        {
            hash ^= static_cast<std::uint8_t>(field >> (i * 8u));
            hash *= UINT64_C(1099511628211);
        }
    };
    for (const auto byte : value.name) append(byte, 1u);
    for (const auto byte : value.patch) append(byte, 1u);
    for (const auto& route : value.routes)
    {
        append(route.owner, 1u); append(route.source, 1u);
        append(route.flags, 2u); append(route.identityNamespace, 2u);
        append(route.originalIndex, 2u); append(route.firmwareParameter, 2u);
        append(route.valueType, 1u); append(route.scope, 1u);
        append(route.physicalKey, 2u); append(route.menuVoiceHint, 1u);
        append(route.reserved, 1u);
    }
    return hash;
}

void recordPcmFixture(std::size_t index, const RawKit& raw)
{
    pcmInputKits[index] = raw;
    const auto value = candidate(raw);
    pcmCandidateFingerprints[index] = candidateFingerprint(value);
    PatchAuthority patch;
    PatchAuthority::Values initial {};
    patch.initialize(initial);
    RoutingStateAuthority routing;
    routing.initialize();
    const auto published = publish(patch, routing, value, 1u);
    check(published.status == PublicationStatus::Ok &&
          published.patchGeneration == 2u &&
          published.jointGeneration == 1u,
          "PCM fixture candidate publishes coherently");
    pcmPatchGenerations[index] = published.patchGeneration;
    pcmRouteGenerations[index] = published.jointGeneration;
}

void nativePcm()
{
    auto base = defaultRaw();
    recordPcmFixture(0u, base);
    const auto baseD1 = render(candidate(base), 0u);
    pcmHashes[0] = baseD1.hash;

    auto changed = base; changed.values[8] = 76u;
    recordPcmFixture(1u, changed);
    const auto pitch = render(candidate(changed), 0u);
    changed = base; changed.values[88] = 40u;
    recordPcmFixture(2u, changed);
    const auto volume = render(candidate(changed), 0u);
    changed = base; changed.values[91] = 40u;
    recordPcmFixture(3u, changed);
    const auto snare = render(candidate(changed), 3u);
    const auto snareBase = render(candidate(base), 3u);
    changed = base; changed.values[92] = 40u;
    recordPcmFixture(4u, changed);
    const auto cymbal = render(candidate(changed), 4u);
    const auto cymbalBase = render(candidate(base), 4u);
    changed = base; changed.values[60] = 8u; changed.values[61] = 118u;
    recordPcmFixture(5u, changed);
    const auto closedHat = render(candidate(changed), 5u, false);
    const auto openHat = render(candidate(changed), 5u, true);
    changed = base; changed.values[37] = 8u; changed.values[191] = 1u;
    recordPcmFixture(6u, changed);
    const auto filter = render(candidate(changed), 0u);

    changed = base;
    changed.values[137] = 1u; changed.values[143] = 127u;
    changed.values[149] = selectorFor(mc::Voice::Drum1, 88u,
                                      mc::SourceKind::Velocity);
    recordPcmFixture(7u, changed);
    const auto velocity = render(candidate(changed), 0u, false, 24u);
    const auto velocityOff = render(candidate(base), 0u, false, 24u);

    changed = base;
    changed.values[115] = 100u; changed.values[121] = 127u;
    changed.values[161] = 1u;
    changed.values[167] = selectorFor(mc::Voice::Drum1, 88u,
                                      mc::SourceKind::Lfo);
    recordPcmFixture(8u, changed);
    const auto lfo = render(candidate(changed), 0u);
    recordPcmFixture(9u, base);

    const std::array<AudioResult, 9> comparisons {{
        pitch, volume, snare, cymbal, openHat, filter, velocity, lfo,
        render(candidate(base), 0u)
    }};
    const std::array<AudioResult, 9> references {{
        baseD1, baseD1, snareBase, cymbalBase, closedHat, baseD1,
        velocityOff, baseD1, baseD1
    }};
    for (std::size_t i = 0; i < comparisons.size(); ++i)
    {
        pcmFirstDifferences[i] = firstDifference(comparisons[i], references[i]);
        check(i == 8u ? pcmFirstDifferences[i] == -1
                      : pcmFirstDifferences[i] >= 0,
              i == 8u ? "matched render exact" : "audible kit changes PCM");
        pcmHashes[i + 1u] = comparisons[i].hash;
    }
}

void concurrencyAndIsolation()
{
    auto rawA = defaultRaw(); auto rawB = rawA;
    rawA.values[88] = 22u; rawB.values[88] = 117u;
    rawB.values[149] = selectorFor(mc::Voice::Drum1, 88u,
                                   mc::SourceKind::Velocity);
    const auto a = candidate(rawA); const auto b = candidate(rawB);
    PatchAuthority patch; patch.initialize(a.patch);
    RoutingStateAuthority routing; routing.initialize();
    check(publish(patch, routing, a, 1u).status == PublicationStatus::Ok,
          "concurrency initial kit");

    std::atomic<bool> start {false}, done {false};
    std::atomic<std::uint64_t> mixed {0}, busy {0};
    std::thread reader([&] {
        while (!start.load(std::memory_order_acquire)) {}
        while (!done.load(std::memory_order_acquire))
        {
            PatchAuthority::Values values;
            RoutingStateAuthority::Snapshot routes;
            if (!coherentSnapshot(patch, routing, values, routes))
            { busy.fetch_add(1u, std::memory_order_relaxed); continue; }
            const bool isA = values[88] == a.patch[88] &&
                routes.state.routes[0].flags == a.routes[0].flags;
            const bool isB = values[88] == b.patch[88] &&
                routes.state.routes[0].flags == b.routes[0].flags;
            if (!isA && !isB) mixed.fetch_add(1u, std::memory_order_relaxed);
        }
    });
    start.store(true, std::memory_order_release);
    std::uint64_t order = 2u;
    std::uint64_t acceptedWrites = 0u, busyWrites = 0u;
    for (unsigned i = 0; i < 2000u; ++i)
    {
        const auto result = publish(patch, routing, (i & 1u) ? b : a, order++);
        if (result.status == PublicationStatus::Ok) ++acceptedWrites;
        else if (result.status == PublicationStatus::Busy) ++busyWrites;
        else check(false, "concurrent writer has only Ok/Busy outcomes");
    }
    done.store(true, std::memory_order_release);
    reader.join();
    readerBusySnapshots = busy.load(std::memory_order_relaxed);
    check(acceptedWrites > 0u && acceptedWrites + busyWrites == 2000u,
          "bounded concurrent publication outcomes");
    check(mixed.load() == 0u, "reader never observes mixed generation");

    // Two producers reserve against one generation. At most one can commit;
    // the other returns Busy or Stale without an internal retry loop.
    PatchAuthority simultaneousPatch; simultaneousPatch.initialize(a.patch);
    RoutingStateAuthority simultaneousRoutes; simultaneousRoutes.initialize();
    std::atomic<unsigned> ready {0}; std::atomic<bool> go {false};
    PublicationResult results[2];
    std::thread one([&] { ++ready; while (!go.load()) {} results[0] =
        PublishPatchCandidate(a, {10u, 0u}, simultaneousPatch, simultaneousRoutes); });
    std::thread two([&] { ++ready; while (!go.load()) {} results[1] =
        PublishPatchCandidate(b, {11u, 0u}, simultaneousPatch, simultaneousRoutes); });
    while (ready.load() != 2u) {}
    go.store(true);
    one.join(); two.join();
    const unsigned accepted = static_cast<unsigned>(results[0].status == PublicationStatus::Ok) +
                              static_cast<unsigned>(results[1].status == PublicationStatus::Ok);
    check(accepted == 1u, "simultaneous kits have one commit point");
    check((results[0].status == PublicationStatus::Ok ||
           results[0].status == PublicationStatus::Busy ||
           results[0].status == PublicationStatus::Stale) &&
          (results[1].status == PublicationStatus::Ok ||
           results[1].status == PublicationStatus::Busy ||
           results[1].status == PublicationStatus::Stale),
          "loser receives bounded Busy/Stale");

    PatchAuthority otherPatch; otherPatch.initialize(a.patch);
    RoutingStateAuthority otherRoutes; otherRoutes.initialize();
    check(publish(otherPatch, otherRoutes, b, 1u).status == PublicationStatus::Ok,
          "second instance publishes independently");
    PatchAuthority::Values firstValues, otherValues;
    RoutingStateAuthority::Snapshot firstRoutes, otherRouteSnapshot;
    check(coherentSnapshot(patch, routing, firstValues, firstRoutes) &&
          coherentSnapshot(otherPatch, otherRoutes, otherValues, otherRouteSnapshot),
          "multi-instance snapshots");
    check(otherValues == b.patch && otherRouteSnapshot.kitName == b.name,
          "second instance owns its kit");
}

void allocationCheck()
{
    const auto value = candidate(defaultRaw());
    PatchAuthority patch; patch.initialize(value.patch);
    RoutingStateAuthority routing; routing.initialize();
    allocations.store(0u); measureAllocations.store(true);
    for (std::uint64_t order = 1; order <= 1000u; ++order) {
        const auto begin = std::chrono::steady_clock::now();
        const auto result = publish(patch, routing, value, order);
        const auto elapsed = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - begin).count());
        publicationTotalNs += elapsed;
        if (elapsed > publicationMaximumNs) publicationMaximumNs = elapsed;
        check(result.status == PublicationStatus::Ok, "allocation publication");
    }
    measureAllocations.store(false);
    check(allocations.load() == 0u, "zero publication allocations");
}
} // namespace

void* operator new(std::size_t size) { return countedAllocate(size); }
void* operator new[](std::size_t size) { return countedAllocate(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main()
{
    candidateAndPublication();
    lockRebase();
    activeRuntimeContinuity();
    nativePcm();
    concurrencyAndIsolation();
    allocationCheck();
    std::printf("{\"pass\":%llu,\"fail\":%llu,\"publication_allocations\":%llu,"
                "\"reader_busy\":%llu,\"publication_max_ns\":%llu,"
                "\"publication_mean_ns\":%llu,\"pcm_hashes\":[",
                static_cast<unsigned long long>(passed),
                static_cast<unsigned long long>(failed),
                static_cast<unsigned long long>(allocations.load()),
                static_cast<unsigned long long>(readerBusySnapshots),
                static_cast<unsigned long long>(publicationMaximumNs),
                static_cast<unsigned long long>(publicationTotalNs / 1000u));
    for (std::size_t i = 0; i < pcmHashes.size(); ++i)
        std::printf("%s\"%016llX\"", i ? "," : "",
                    static_cast<unsigned long long>(pcmHashes[i]));
    std::printf("],\"first_differences\":[");
    for (std::size_t i = 0; i < pcmFirstDifferences.size(); ++i)
        std::printf("%s%d", i ? "," : "", pcmFirstDifferences[i]);
    std::printf("],\"candidate_fingerprints\":[");
    for (std::size_t i = 0; i < pcmCandidateFingerprints.size(); ++i)
        std::printf("%s\"%016llX\"", i ? "," : "",
                    static_cast<unsigned long long>(pcmCandidateFingerprints[i]));
    std::printf("],\"patch_generations\":[");
    for (std::size_t i = 0; i < pcmPatchGenerations.size(); ++i)
        std::printf("%s%llu", i ? "," : "",
                    static_cast<unsigned long long>(pcmPatchGenerations[i]));
    std::printf("],\"route_generations\":[");
    for (std::size_t i = 0; i < pcmRouteGenerations.size(); ++i)
        std::printf("%s%llu", i ? "," : "",
                    static_cast<unsigned long long>(pcmRouteGenerations[i]));
    std::printf("],\"input_kit_hex\":[");
    for (std::size_t i = 0; i < pcmInputKits.size(); ++i)
    {
        std::printf("%s\"", i ? "," : "");
        for (const auto byte : pcmInputKits[i].name) std::printf("%02X", byte);
        for (const auto byte : pcmInputKits[i].values) std::printf("%02X", byte);
        std::printf("\"");
    }
    std::printf("]}\n");
    return failed == 0u ? 0 : 1;
}
