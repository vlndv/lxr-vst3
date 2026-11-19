// P18-B firmware-faithful morph projection and native runtime tests.
// Original identities Copyright 2013 Julian Schmidt; see LICENSE-LXR.txt.

#include "../interfaces/Morphing.h"
#include "../dsp/Engine.h"
#include "../dsp/ParameterArray.h"
#include "reference/presets/original_preset_adapter.h"
#include "reference/morph/original_morph_adapter.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace
{
using namespace lxr;
namespace mc = lxr::modulation_contract;

std::atomic<std::uint64_t> allocations {0};
std::atomic<bool> measureAllocations {false};
std::uint64_t passed = 0;
std::uint64_t failed = 0;

void check(bool value, const char* label)
{
    if (value) ++passed;
    else
    {
        ++failed;
        if (failed < 30u) std::fprintf(stderr, "FAIL: %s\n", label);
    }
}

kit::RawKit defaultRaw()
{
    ParameterArray parameters;
    parameters.init();
    kit::RawKit raw;
    raw.name = {{'P','1','8','B','B','A','S','E'}};
    for (std::size_t i = 0; i < raw.values.size(); ++i)
        raw.values[i] = parameters.get(static_cast<std::uint8_t>(i));
    return raw;
}

std::uint8_t selectorFor(mc::Voice voice, std::uint16_t parameter,
                         mc::SourceKind source, bool requireHigh = false)
{
    for (std::uint16_t index = requireHigh ? 128u : 1u;
         index <= 204u; ++index)
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
    return 0u;
}

std::uint8_t highSelectorFor(mc::Voice voice, mc::SourceKind source)
{
    for (std::uint16_t index = 128u; index <= 204u; ++index)
    {
        const auto typed = mc::OriginalTargetIndex::fromRaw(index);
        const auto* target = mc::lookup(typed);
        if (target == nullptr || target->voice != voice) continue;
        const mc::SourceIdentity owner {source, voice};
        const auto status = source == mc::SourceKind::Velocity
            ? validateVelocityDestination(owner, typed)
            : validateLfoDestination(owner, typed);
        if (status == VelocityRouteStatus::Ok)
            return static_cast<std::uint8_t>(index);
    }
    return 0u;
}

kit::CanonicalKit canonical(const kit::RawKit& raw)
{
    kit::CanonicalKit result;
    check(static_cast<bool>(kit::DecodeRawKit(raw, result)),
          "decode canonical kit");
    return result;
}

kit::PatchCandidate patchCandidate(const kit::RawKit& raw)
{
    kit::PatchCandidate result;
    const auto decoded = canonical(raw);
    check(static_cast<bool>(kit::BuildPatchCandidate(decoded, result)),
          "build runtime patch candidate");
    return result;
}

bool applyRoutes(Engine& engine, const kit::PatchCandidate& source)
{
    for (const auto& route : source.routes)
    {
        const mc::SourceIdentity owner {
            route.source == 0u ? mc::SourceKind::Velocity : mc::SourceKind::Lfo,
            mc::voiceFromZeroBased(route.owner)
        };
        const auto target = mc::OriginalTargetIndex::fromRaw(route.originalIndex);
        const auto status = route.source == 0u
            ? engine.configureVelocityDestination(owner, target)
            : engine.configureLfoDestination(owner, target);
        if (status != VelocityRouteStatus::Ok) return false;
    }
    return true;
}

struct AudioResult
{
    std::uint64_t hash = UINT64_C(1469598103934665603);
    std::array<std::int16_t, 512> pcm {};
};

AudioResult render(const morph::Values& values,
                   const kit::PatchCandidate& sourceRoutes)
{
    Engine engine;
    ParameterArray parameters;
    parameters.init();
    check(engine.loadAssets("data"), "load tracked native assets");
    for (std::size_t i = 0; i < values.size(); ++i)
        parameters.set(static_cast<std::uint8_t>(i), values[i], engine);
    check(applyRoutes(engine, sourceRoutes), "apply source-owned routes");
    engine.triggerDrum(0u, 100u, 63u);

    AudioResult result;
    std::array<std::int16_t, 32> left {}, right {}, auxL {}, auxR {};
    std::size_t at = 0;
    for (unsigned block = 0; block < 8u; ++block)
    {
        engine.processBlock(left.data(), right.data(), auxL.data(), auxR.data());
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            const std::uint16_t samples[2] {
                static_cast<std::uint16_t>(left[i]),
                static_cast<std::uint16_t>(right[i])
            };
            result.pcm[at++] = left[i];
            result.pcm[at++] = right[i];
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

void interpolationParity()
{
    for (unsigned source = 0; source < 256u; ++source)
        for (unsigned target = 0; target < 256u; ++target)
            for (unsigned amount = 0; amount < 256u; ++amount)
                check(morph::InterpolateByte(
                          static_cast<std::uint8_t>(source),
                          static_cast<std::uint8_t>(target),
                          static_cast<std::uint8_t>(amount)) ==
                      p18_original_interpolate(
                          static_cast<std::uint8_t>(source),
                          static_cast<std::uint8_t>(target),
                          static_cast<std::uint8_t>(amount)),
                      "production scalar equals unchanged original C");
}

void projectionAndRoutes(kit::PatchCandidate& sourcePatch,
                         morph::Target& target,
                         morph::Values& sourceValues)
{
    auto sourceRaw = defaultRaw();
    sourceRaw.values[152] = highSelectorFor(
        mc::Voice::Snare, mc::SourceKind::Velocity);
    sourceRaw.values[164] = 4u;
    sourceRaw.values[170] = highSelectorFor(
        mc::Voice::Snare, mc::SourceKind::Lfo);
    check(sourceRaw.values[152] > 127u && sourceRaw.values[170] > 127u,
          "source route identities exercise full byte domain");
    sourcePatch = patchCandidate(sourceRaw);
    check(static_cast<bool>(morph::BuildSourceValues(
              sourcePatch.patch, sourcePatch.routes, sourceValues)),
          "reconstruct source values from typed routes");
    check(sourceValues[152] == sourceRaw.values[152] &&
          sourceValues[170] == sourceRaw.values[170] &&
          sourceValues[164] == sourceRaw.values[164],
          "typed selectors and LFO hint are not seven-bit truncated");

    auto targetRaw = defaultRaw();
    targetRaw.name = {{'P','1','8','B','T','A','R','G'}};
    targetRaw.values[8] = 91u;
    targetRaw.values[37] = 11u;
    targetRaw.values[60] = 9u;
    targetRaw.values[61] = 119u;
    targetRaw.values[88] = 23u;
    targetRaw.values[134] = 0u;
    targetRaw.values[149] = selectorFor(
        mc::Voice::Drum1, 37u, mc::SourceKind::Velocity);
    targetRaw.values[161] = 1u;
    targetRaw.values[167] = selectorFor(
        mc::Voice::Drum1, 37u, mc::SourceKind::Lfo);
    targetRaw.values[191] = 7u;
    check(static_cast<bool>(morph::ValidateTarget(canonical(targetRaw), target)),
          "validate original target B");
    check(target.values == targetRaw.values,
          "target retains all 228 original bytes");

    const std::array<std::uint8_t, 9> amounts {{
        0u, 1u, 126u, 127u, 128u, 129u, 130u, 254u, 255u
    }};
    for (const auto amount : amounts)
    {
        morph::Values originalNumeric;
        morph::Values effective;
        morph::InterpolateAll(sourceValues, target.values, amount,
                              originalNumeric);
        morph::BuildEffectiveProjection(sourceValues, target.values, amount,
                                        effective);
        for (std::size_t i = 0; i < effective.size(); ++i)
        {
            const auto original = p18_original_interpolate(
                sourceValues[i], target.values[i], amount);
            check(originalNumeric[i] == original,
                  "all 228 numeric bytes match original C");
            check(effective[i] == (morph::IsSourceOwnedRouteByte(i)
                    ? sourceValues[i] : original),
                  "effective route/scalar policy");
        }
    }
}

void authorityContracts(const morph::Target& target)
{
    morph::StateAuthority authority;
    morph::Snapshot snapshot;
    check(authority.trySnapshot(snapshot) && !snapshot.targetPresent &&
          snapshot.amount == 0u && snapshot.generation == 0u,
          "initial transient morph session");

    morph::StateAuthority::WriteHandle held;
    check(authority.tryBeginWrite(held), "hold bounded morph writer");
    check(authority.tryPublishTarget(target, 1u, 0u).status ==
              morph::PublicationStatus::Busy,
          "target publication returns Busy without waiting");
    authority.cancelUnmodified(held);

    const auto loaded = authority.tryPublishTarget(target, 1u, 0u);
    check(loaded.status == morph::PublicationStatus::Ok &&
          loaded.generation == 2u, "target publication generation");
    check(authority.tryPublishTarget(target, 2u, 0u).status ==
              morph::PublicationStatus::Stale,
          "stale target generation rejected");
    check(authority.tryPublishAmount(173u, 2u).status ==
              morph::PublicationStatus::Ok,
          "amount publication accepted");
    check(authority.tryPublishAmount(19u, 1u).status ==
              morph::PublicationStatus::Stale,
          "older amount order rejected");
    check(authority.trySnapshot(snapshot) && snapshot.targetPresent &&
          snapshot.target.values == target.values &&
          snapshot.amount == 173u && snapshot.order == 2u &&
          snapshot.generation == 4u,
          "coherent target and amount snapshot");

    morph::StateAuthority other;
    check(other.tryPublishTarget(target, 1u, 0u).status ==
              morph::PublicationStatus::Ok &&
          other.tryPublishAmount(9u, 2u).status ==
              morph::PublicationStatus::Ok,
          "second instance publishes independently");
    morph::Snapshot independent;
    check(other.trySnapshot(independent) && independent.amount == 9u &&
          snapshot.amount == 173u,
          "multi-instance morph state is independent");
}

void invalidRollback()
{
    auto raw = defaultRaw();
    raw.values[1] = 6u; // User SD samples remain outside runtime scope.
    morph::Target output;
    const auto result = morph::ValidateTarget(canonical(raw), output);
    check(!result && result.issue == kit::Issue::ExternalSample &&
          result.field == 1u, "invalid target diagnosed before publication");
}

void activeRuntimeAndLocks(const kit::PatchCandidate& sourcePatch,
                           const morph::Target& target,
                           const morph::Values& sourceValues,
                           std::array<std::uint64_t, 3>& hashes)
{
    morph::Values midpoint;
    morph::Values endpoint;
    morph::BuildEffectiveProjection(sourceValues, target.values, 128u, midpoint);
    morph::BuildEffectiveProjection(sourceValues, target.values, 255u, endpoint);

    const auto sourceAudio = render(sourceValues, sourcePatch);
    const auto midpointAudio = render(midpoint, sourcePatch);
    const auto midpointRepeat = render(midpoint, sourcePatch);
    const auto endpointAudio = render(endpoint, sourcePatch);
    hashes = {{sourceAudio.hash, midpointAudio.hash, endpointAudio.hash}};
    check(midpointAudio.hash == midpointRepeat.hash &&
          midpointAudio.pcm == midpointRepeat.pcm,
          "controlled morph PCM is deterministic");
    check(sourceAudio.hash != midpointAudio.hash &&
          midpointAudio.hash != endpointAudio.hash,
          "audible target changes deterministic PCM");

    Engine engine;
    ParameterArray parameters;
    parameters.init();
    check(engine.loadAssets("data"), "active voice loads assets");
    for (std::size_t i = 0; i < sourceValues.size(); ++i)
        parameters.set(static_cast<std::uint8_t>(i), sourceValues[i], engine);
    check(applyRoutes(engine, sourcePatch), "active voice applies routes");
    engine.triggerDrum(0u, 100u, 63u);
    std::array<std::int16_t, 32> left {}, right {}, auxL {}, auxR {};
    engine.processBlock(left.data(), right.data(), auxL.data(), auxR.data());
    const auto oscillatorPhase = engine.drums[0].osc.phase;
    const auto lfoPhase = engine.drums[0].lfo.phase;
    for (std::size_t i = 0; i < midpoint.size(); ++i)
        parameters.set(static_cast<std::uint8_t>(i), midpoint[i], engine);
    check(engine.drums[0].osc.phase == oscillatorPhase &&
          engine.drums[0].lfo.phase == lfoPhase,
          "morph projection does not reset oscillator or LFO phase");
    engine.processBlock(left.data(), right.data(), auxL.data(), auxR.data());
    bool active = false;
    for (std::size_t i = 0; i < left.size(); ++i)
        active |= left[i] != 0 || right[i] != 0;
    check(active, "active voice continues after projection boundary");

    const sequencer::AutomationSlotCommand lock {
        sequencer::AutomationAction::Apply,
        mc::HostParameterId::fromRaw(88u),
        mc::SevenBitValue::fromRaw(11u)
    };
    check(engine.applySequencerAutomationPair(0u, &lock, nullptr),
          "apply active sequencer lock");
    for (std::size_t i = 0; i < endpoint.size(); ++i)
        parameters.set(static_cast<std::uint8_t>(i), endpoint[i], engine);
    check(engine.reapplySequencerAutomation(),
          "reapply locks after morph projection");
    check(std::fabs(engine.drums[0].vol - 11.0f / 127.0f) < 1.0e-7f,
          "sequencer lock wins after morph");
    const sequencer::AutomationSlotCommand clear {
        sequencer::AutomationAction::Clear,
        mc::HostParameterId{}, mc::SevenBitValue{}
    };
    check(engine.applySequencerAutomationPair(0u, &clear, nullptr),
          "clear active sequencer lock");
    check(std::fabs(engine.drums[0].vol -
                    static_cast<float>(endpoint[88]) / 127.0f) < 1.0e-7f,
          "lock release reveals newest morph projection");

    measureAllocations.store(true, std::memory_order_relaxed);
    const auto before = allocations.load(std::memory_order_relaxed);
    morph::Values measured;
    for (unsigned i = 0; i < 1000u; ++i)
        morph::BuildEffectiveProjection(
            sourceValues, target.values, static_cast<std::uint8_t>(i), measured);
    const auto after = allocations.load(std::memory_order_relaxed);
    measureAllocations.store(false, std::memory_order_relaxed);
    check(before == after, "bounded projection performs zero heap allocations");
}
} // namespace

void* operator new(std::size_t size)
{
    if (measureAllocations.load(std::memory_order_relaxed))
        allocations.fetch_add(1u, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    std::abort();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

int main()
{
    interpolationParity();
    lxr::kit::PatchCandidate sourcePatch;
    lxr::morph::Target target;
    lxr::morph::Values sourceValues;
    projectionAndRoutes(sourcePatch, target, sourceValues);
    authorityContracts(target);
    invalidRollback();
    std::array<std::uint64_t, 3> hashes {};
    activeRuntimeAndLocks(sourcePatch, target, sourceValues, hashes);
    std::printf(
        "{\"pass\":%llu,\"fail\":%llu,\"exhaustive_inputs\":16777216,"
        "\"projection_allocations\":%llu,\"pcm_hashes\":[%llu,%llu,%llu]}\n",
        static_cast<unsigned long long>(passed),
        static_cast<unsigned long long>(failed),
        static_cast<unsigned long long>(allocations.load()),
        static_cast<unsigned long long>(hashes[0]),
        static_cast<unsigned long long>(hashes[1]),
        static_cast<unsigned long long>(hashes[2]));
    return failed == 0u ? 0 : 1;
}
