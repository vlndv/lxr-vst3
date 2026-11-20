// P18-C morph-management contract tests.
// Original identities Copyright 2013 Julian Schmidt; see LICENSE-LXR.txt.

#include "../interfaces/MorphManagement.h"
#include "reference/morph/original_morph_adapter.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <new>

namespace
{
using namespace lxr;

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
        if (failed < 20u) std::fprintf(stderr, "FAIL: %s\n", label);
    }
}

kit::RawKit defaultRaw()
{
    auto raw = kit::MakeFirmwareInitializationKit();
    raw.name = {{'S','O','U','R','C','E','A',0}};
    return raw;
}

kit::CanonicalKit canonical(const kit::RawKit& raw)
{
    kit::CanonicalKit result;
    check(static_cast<bool>(kit::DecodeRawKit(raw, result)),
          "decode canonical fixture");
    return result;
}

kit::PatchCandidate patch(const kit::CanonicalKit& kitValue)
{
    kit::PatchCandidate result;
    check(static_cast<bool>(kit::BuildPatchCandidate(kitValue, result)),
          "build source candidate");
    return result;
}

void projectionExportContract()
{
    auto sourceRaw = defaultRaw();
    sourceRaw.values[8] = 17u;
    sourceRaw.values[37] = 91u;
    sourceRaw.values[88] = 103u;
    const auto sourceKit = canonical(sourceRaw);
    const auto source = patch(sourceKit);

    morph::Values sourceValues;
    check(static_cast<bool>(morph::BuildSourceValues(
              source.patch, source.routes, sourceValues)),
          "reconstruct source A");

    auto targetRaw = defaultRaw();
    targetRaw.name = {{'M', 0x01u, 'R', 0u, 'P', 0u, 0u, 0u}};
    targetRaw.values[8] = 91u;
    targetRaw.values[37] = 11u;
    targetRaw.values[60] = 9u;
    targetRaw.values[61] = 119u;
    targetRaw.values[88] = 23u;
    targetRaw.values[134] = 0u;
    targetRaw.values[191] = 7u;
    // Retain source-valid selectors; P18-C must not reinterpret these bytes.
    for (std::size_t i = 149u; i <= 154u; ++i)
        targetRaw.values[i] = sourceValues[i];
    for (std::size_t i = 161u; i <= 172u; ++i)
        targetRaw.values[i] = sourceValues[i];
    const auto targetKit = canonical(targetRaw);
    morph::Target target;
    check(static_cast<bool>(morph::ValidateTarget(targetKit, target)),
          "validate target B");

    const auto display = kit::management::FormatName(target.name);
    check(std::strcmp(display.text.data(), "M\\x01R\\x00P") == 0,
          "binary target identity display");

    for (const auto amount : std::array<std::uint8_t, 3>{{0u, 127u, 255u}})
    {
        morph::Snapshot snapshot;
        snapshot.target = target;
        snapshot.amount = amount;
        snapshot.targetPresent = true;
        snapshot.generation = 22u;
        kit::CanonicalKit projected;

        measureAllocations.store(true, std::memory_order_relaxed);
        const auto built = morph::management::BuildProjectedKit(
            source.patch, source.routes, snapshot, projected);
        measureAllocations.store(false, std::memory_order_relaxed);
        check(static_cast<bool>(built), "build representable projected kit");
        check(projected.name == target.name,
              "MorphSnd identity belongs to target B");

        kit::snd::Bytes actual {};
        const auto encoded = kit::snd::EncodeCanonical(
            projected, source.routes, actual.data(), actual.size());
        check(static_cast<bool>(encoded), "encode projected kit");

        kit::snd::Bytes expected {};
        check(p18_original_save_morph(
                  sourceValues.data(), target.values.data(), amount,
                  target.name.data(), expected.data(), expected.size()) ==
              expected.size(), "unchanged original MorphSnd oracle");
        // Rendering/export routing is source-owned in the accepted P18-B
        // plugin contract. Restore those typed identities after the unchanged
        // firmware numeric oracle has produced all 228 scalar bytes.
        for (std::size_t i = 149u; i <= 154u; ++i)
            expected[kit::snd::kPayloadOffset + i] = sourceValues[i];
        for (std::size_t i = 161u; i <= 172u; ++i)
            expected[kit::snd::kPayloadOffset + i] = sourceValues[i];
        check(actual == expected, "exact 236-byte MorphSnd materialization");
    }

    morph::Snapshot absent;
    kit::CanonicalKit unused;
    check(morph::management::BuildProjectedKit(
              source.patch, source.routes, absent, unused).status ==
          morph::management::Status::NoTarget,
          "export without target rejects explicitly");

    auto invalidRoutes = source.routes;
    invalidRoutes[0].originalIndex = 255u;
    morph::Snapshot snapshot;
    snapshot.target = target;
    snapshot.targetPresent = true;
    const auto invalid = morph::management::BuildProjectedKit(
        source.patch, invalidRoutes, snapshot, unused);
    check(invalid.status == morph::management::Status::InvalidRoute ||
          invalid.status == morph::management::Status::UnsupportedRoute,
          "nonrepresentable route rejects without truncation");
}

void authorityClearContract()
{
    auto raw = defaultRaw();
    raw.name = {{'T','A','R','G','E','T','B',0}};
    raw.values[8] = 77u;
    morph::Target target;
    check(static_cast<bool>(morph::ValidateTarget(canonical(raw), target)),
          "validate clear target");

    for (const auto amount : std::array<std::uint8_t, 3>{{0u, 127u, 255u}})
    {
        morph::StateAuthority authority;
        const auto loaded = authority.tryPublishTarget(target, 1u, 0u);
        check(loaded.status == morph::PublicationStatus::Ok,
              "publish target before clear");
        const auto changed = authority.tryPublishAmount(amount, 2u);
        check(changed.status == morph::PublicationStatus::Ok,
              "publish amount before clear");

        morph::StateAuthority::WriteHandle held;
        check(authority.tryBeginWrite(held), "acquire bounded writer");
        check(authority.tryClear(3u, changed.generation).status ==
                  morph::PublicationStatus::Busy,
              "clear returns Busy without waiting");
        morph::Snapshot locked;
        check(authority.copyLocked(held, locked) && locked.targetPresent,
              "locked export snapshot is coherent");
        authority.cancelUnmodified(held);

        const auto cleared = authority.tryClear(3u, changed.generation);
        check(cleared.status == morph::PublicationStatus::Ok,
              "clear publishes atomically");
        morph::Snapshot final;
        check(authority.trySnapshot(final) && !final.targetPresent &&
              final.amount == amount,
              "clear retains host-visible amount and removes target");
        check(authority.tryClear(3u, changed.generation).status ==
                  morph::PublicationStatus::Stale,
              "stale clear cannot replace newer state");
    }
}
} // namespace

void* operator new(std::size_t size)
{
    if (measureAllocations.load(std::memory_order_relaxed))
        allocations.fetch_add(1u, std::memory_order_relaxed);
    if (void* value = std::malloc(size)) return value;
    std::abort();
}

void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }

int main()
{
    projectionExportContract();
    authorityClearContract();
    check(allocations.load(std::memory_order_relaxed) == 0u,
          "projection/publication perform no heap allocation");
    constexpr std::size_t exportSnapshotStackBytes =
        sizeof(lxr::RoutingStateAuthority::Snapshot) +
        sizeof(std::array<std::uint8_t, lxr::kit::kFieldCount>) +
        sizeof(lxr::morph::Snapshot) + sizeof(lxr::kit::CanonicalKit) +
        sizeof(lxr::RoutingStateAuthority::Guard) +
        sizeof(lxr::morph::StateAuthority::WriteHandle);
    std::printf("{\"pass\":%llu,\"fail\":%llu,\"allocations\":%llu,"
                "\"bounded_export_stack_bytes\":%llu}\n",
        static_cast<unsigned long long>(passed),
        static_cast<unsigned long long>(failed),
        static_cast<unsigned long long>(allocations.load()),
        static_cast<unsigned long long>(exportSnapshotStackBytes));
    return failed == 0u ? 0 : 1;
}
