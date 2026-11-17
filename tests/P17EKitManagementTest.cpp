// P17-E non-real-time kit-management contract and native integration tests.
// Original identities Copyright 2013 Julian Schmidt; see LICENSE-LXR.txt.

#include "../interfaces/KitManagement.h"
#include "../interfaces/KitFileCodec.h"
#include "../interfaces/KitFileIO.h"
#include "../interfaces/RuntimeKitLoad.h"
#include "../dsp/Engine.h"
#include "../dsp/ParameterArray.h"
#include "../dsp/SequencerParameterLocks.h"
#include "../dsp/TriggerScheduler.h"

#include <windows.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace
{
using namespace lxr;
using namespace lxr::kit;
namespace manage = lxr::kit::management;
namespace file = lxr::kit::snd;
using PatchAuthority = ParameterStateAuthority<kFieldCount>;

std::atomic<std::uint64_t> allocations {0};
std::atomic<bool> countAllocations {false};
std::uint64_t passed = 0, failed = 0;

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

std::array<wchar_t, file::kPathCapacity> wide(const char* text)
{
    std::array<wchar_t, file::kPathCapacity> result {};
    if (text == nullptr) return result;
    std::size_t i = 0;
    for (; i + 1u < result.size() && text[i] != 0; ++i)
        result[i] = static_cast<unsigned char>(text[i]);
    return result;
}

std::array<wchar_t, file::kPathCapacity> child(
    const char* folder, const wchar_t* name)
{
    auto result = wide(folder);
    std::size_t at = 0;
    while (at < result.size() && result[at] != 0) ++at;
    if (at != 0u && result[at - 1u] != L'\\' && at + 1u < result.size())
        result[at++] = L'\\';
    for (std::size_t i = 0; name[i] != 0 && at + 1u < result.size(); ++i)
        result[at++] = name[i];
    return result;
}

RawKit rawKit(const std::array<std::uint8_t, 8>& name, std::uint8_t volume)
{
    ParameterArray parameters;
    parameters.init();
    RawKit raw;
    raw.name = name;
    for (std::size_t i = 0; i < raw.values.size(); ++i)
        raw.values[i] = parameters.get(static_cast<std::uint8_t>(i));
    raw.values[88] = volume;
    return raw;
}

CanonicalKit canonical(const RawKit& raw)
{
    CanonicalKit result;
    check(static_cast<bool>(DecodeRawKit(raw, result)), "source-valid canonical kit");
    return result;
}

PatchCandidate candidate(const CanonicalKit& kit)
{
    PatchCandidate result;
    check(static_cast<bool>(BuildPatchCandidate(kit, result)), "runtime candidate");
    return result;
}

bool sameCanonical(const CanonicalKit& a, const CanonicalKit& b)
{
    if (a.name != b.name) return false;
    for (std::uint16_t id = 0; id < kFieldCount; ++id)
    {
        std::uint16_t av = 0, bv = 0;
        if (!ReadField(a, id, av) || !ReadField(b, id, bv) || av != bv)
            return false;
    }
    return true;
}

void nameContract()
{
    const std::array<std::uint8_t, 8> printable {{'K','I','T',' ','A',0,0,0}};
    const auto shown = manage::FormatName(printable);
    check(std::strcmp(shown.text.data(), "KIT A") == 0 && !shown.unnamed,
          "printable name and trailing zeros");
    const std::array<std::uint8_t, 8> binary {{'A',0x01u,0xffu,'Z',0,0,0,0}};
    const auto escaped = manage::FormatName(binary);
    check(std::strcmp(escaped.text.data(), "A\\x01\\xFFZ") == 0,
          "binary name escaped deterministically");
    check(binary[1] == 0x01u && binary[2] == 0xffu,
          "display conversion does not modify raw bytes");
    const auto unnamed = manage::FormatName({});
    check(std::strcmp(unnamed.text.data(), "<unnamed>") == 0 && unnamed.unnamed,
          "all-zero name is unnamed");
}

void statusContract()
{
    file::FileResult result;
    result.error = file::FileError::Codec;
    result.codec.error = file::Error::Truncated;
    check(manage::FromFileFailure(manage::Action::Import, result).status ==
              manage::Status::InvalidFileSize,
          "invalid-size status");
    result.codec.error = file::Error::InvalidSelector;
    check(manage::FromFileFailure(manage::Action::Import, result).status ==
              manage::Status::InvalidRoute,
          "invalid-selector status");
    result.codec.error = file::Error::NonrepresentableRoute;
    check(manage::FromFileFailure(manage::Action::Export, result).status ==
              manage::Status::UnsupportedRoute,
          "unsupported-route status");
    result.error = file::FileError::DestinationExists;
    check(manage::FromFileFailure(manage::Action::Export, result).status ==
              manage::Status::ExistingDestination,
          "existing-destination status");
    manage::OperationResult cancelled;
    cancelled.status = manage::Status::Cancelled;
    check(!cancelled && std::strstr(manage::StatusDescription(cancelled.status),
                                    "unchanged") != nullptr,
          "cancellation explicitly preserves state");
}

void fileOperations(const char* folder)
{
    const std::array<std::uint8_t, 8> name {{'B',0x01u,0xffu,'N','A','M','E',0}};
    const auto source = canonical(rawKit(name, 109u));
    const auto runtime = candidate(source);
    const auto importPath = child(folder, L"binary-name.snd");
    const auto exportPath = child(folder, L"typed-export.snd");
    const auto unsupportedPath = child(folder, L"unsupported.snd");
    const auto invalidPath = child(folder, L"invalid-size.snd");
    const auto missingPath = child(folder, L"missing.snd");

    auto written = file::WriteNewFile(importPath.data(), source);
    check(static_cast<bool>(written) && written.published,
          "safe no-replace import fixture write");
    CanonicalKit decoded;
    const auto read = file::ReadFile(importPath.data(), decoded);
    check(static_cast<bool>(read) && sameCanonical(source, decoded),
          "real file import exact canonical state");
    check(decoded.name == name, "binary name exact through real file");

    written = file::WriteNewFile(exportPath.data(), decoded, runtime.routes);
    check(static_cast<bool>(written) && written.published,
          "typed route export published");
    CanonicalKit exported;
    check(file::ReadFile(exportPath.data(), exported) &&
              sameCanonical(source, exported),
          "exported file imports exactly");
    LARGE_INTEGER bytes {};
    HANDLE handle = CreateFileW(exportPath.data(), GENERIC_READ,
                                FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    check(handle != INVALID_HANDLE_VALUE && GetFileSizeEx(handle, &bytes) &&
              bytes.QuadPart == 236,
          "export is exactly 236 bytes");
    if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    const auto existing = file::WriteNewFile(exportPath.data(), decoded,
                                              runtime.routes);
    check(existing.error == file::FileError::DestinationExists &&
              !existing.published,
          "existing destination is never overwritten");

    auto unsupported = runtime.routes;
    unsupported[3].flags = 1u;
    unsupported[3].identityNamespace = 1u;
    unsupported[3].originalIndex = 205u;
    const auto rejected = file::WriteNewFile(unsupportedPath.data(), decoded,
                                             unsupported);
    check(rejected.error == file::FileError::Codec &&
              rejected.codec.error == file::Error::NonrepresentableRoute &&
              rejected.codec.route == 3u && !rejected.published,
          "nonrepresentable route rejected with identity");
    check(GetFileAttributesW(unsupportedPath.data()) == INVALID_FILE_ATTRIBUTES,
          "unsupported export emits no destination");

    const std::array<std::uint8_t, 235> shortBytes {};
    handle = CreateFileW(invalidPath.data(), GENERIC_WRITE, 0, nullptr,
                         CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD transferred = 0;
    check(handle != INVALID_HANDLE_VALUE &&
              WriteFile(handle, shortBytes.data(),
                        static_cast<DWORD>(shortBytes.size()), &transferred,
                        nullptr) && transferred == shortBytes.size(),
          "construct invalid-size fixture");
    if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    const auto before = decoded;
    const auto invalid = file::ReadFile(invalidPath.data(), decoded);
    check(invalid.error == file::FileError::Codec &&
              invalid.codec.error == file::Error::Truncated &&
              sameCanonical(before, decoded),
          "invalid-size import has no mutation");
    check(file::ReadFile(missingPath.data(), decoded).error ==
              file::FileError::Open && sameCanonical(before, decoded),
          "read failure has no mutation");
}

PublicationResult publish(PatchAuthority& patch, RoutingStateAuthority& routes,
                          const PatchCandidate& value, std::uint64_t order,
                          std::uint64_t expected)
{
    return PublishPatchCandidate(value, {order, expected}, patch, routes);
}

void transactionContract()
{
    const auto first = candidate(canonical(rawKit(
        {{'F','I','R','S','T',0,0,0}}, 30u)));
    const auto second = candidate(canonical(rawKit(
        {{'S','E','C','O','N','D',0,0}}, 115u)));
    PatchAuthority patch;
    patch.initialize(first.patch);
    RoutingStateAuthority routes;
    routes.initialize();
    auto result = publish(patch, routes, first, 1u, 0u);
    check(result.status == PublicationStatus::Ok, "first import transaction");
    result = publish(patch, routes, second, 2u, routes.generation());
    check(result.status == PublicationStatus::Ok, "second import transaction");
    PatchAuthority::Values snapshot;
    RoutingStateAuthority::Snapshot routing;
    RoutingStateAuthority::Guard guard;
    check(routes.tryAcquire(guard) && patch.trySnapshot(snapshot, nullptr, 1u) &&
              routes.copyLocked(guard, routing),
          "coherent imported snapshot");
    routes.release(guard);
    check(snapshot == second.patch && routing.kitName == second.name,
          "newest whole kit is authoritative");

    check(routes.tryAcquire(guard), "hold publication gate");
    result = publish(patch, routes, first, 3u, routes.generation());
    check(result.status == PublicationStatus::Busy, "bounded Busy import");
    routes.release(guard);
    result = publish(patch, routes, first, 3u, 0u);
    check(result.status == PublicationStatus::Stale, "stale import rejected");

    PatchAuthority otherPatch;
    otherPatch.initialize(first.patch);
    RoutingStateAuthority otherRoutes;
    otherRoutes.initialize();
    check(publish(otherPatch, otherRoutes, first, 1u, 0u).status ==
              PublicationStatus::Ok,
          "second instance publishes independently");

    allocations.store(0u);
    countAllocations.store(true);
    for (std::uint64_t order = 4u; order < 1004u; ++order)
        check(publish(patch, routes, second, order, routes.generation()).status ==
                  PublicationStatus::Ok,
              "allocation-free bounded publication");
    countAllocations.store(false);
    check(allocations.load() == 0u, "zero publication allocations");
}

void activeRuntimeContinuity()
{
    Engine engine;
    ParameterArray parameters;
    parameters.init();
    check(engine.loadAssets("data"), "continuity assets load");
    parameters.applyAllToEngine(engine);
    parameters.set(88u, 70u, engine);
    const auto decoded = sequencer::DecodeAutomationSlot(89u, 20u);
    check(static_cast<bool>(decoded) &&
              engine.applySequencerAutomationPair(0u, &decoded.command, nullptr),
          "active lock established before import");
    parameters.set(88u, 100u, engine);
    check(engine.reapplySequencerAutomation() &&
              std::fabs(engine.drums[0].vol - 20.f / 127.f) < 1.e-6f,
          "active lock remains above imported base");
    const auto clear = sequencer::DecodeAutomationSlot(0xffu, 0u);
    check(static_cast<bool>(clear) &&
              engine.applySequencerAutomationPair(0u, &clear.command, nullptr) &&
              std::fabs(engine.drums[0].vol - 100.f / 127.f) < 1.e-6f,
          "lock release restores newest imported base");

    sequencer::TriggerScheduler scheduler;
    check(scheduler.start() == sequencer::Status::Ok &&
              scheduler.processNativeBlock(0u, engine) == sequencer::Status::Ok,
          "sequencer runs before import boundary");
    const auto blocks = scheduler.diagnostics().processedNativeBlocks;
    parameters.set(88u, 91u, engine);
    check(scheduler.processNativeBlock(32u, engine) == sequencer::Status::Ok &&
              scheduler.running() &&
              scheduler.diagnostics().processedNativeBlocks == blocks + 1u,
          "sequencer remains running across import boundary");

    engine.triggerDrum(0u, 100u, 63u);
    std::array<std::int16_t, 32> left {}, right {}, auxL {}, auxR {};
    engine.processBlock(left.data(), right.data(), auxL.data(), auxR.data());
    parameters.set(88u, 55u, engine);
    engine.processBlock(left.data(), right.data(), auxL.data(), auxR.data());
    bool audible = false;
    for (std::size_t i = 0; i < left.size(); ++i)
        audible = audible || left[i] != 0 || right[i] != 0;
    check(audible, "active MIDI voice continues across import boundary");
}

std::uint64_t renderHash(const PatchCandidate& value)
{
    Engine engine;
    ParameterArray parameters;
    parameters.init();
    check(engine.loadAssets("data"), "native assets load");
    for (std::size_t i = 0; i < value.patch.size(); ++i)
        parameters.set(static_cast<std::uint8_t>(i), value.patch[i], engine);
    engine.triggerDrum(0u, 100u, 63u);
    std::array<std::int16_t, 32> left {}, right {}, auxL {}, auxR {};
    std::uint64_t hash = UINT64_C(1469598103934665603);
    for (unsigned block = 0; block < 20u; ++block)
    {
        engine.processBlock(left.data(), right.data(), auxL.data(), auxR.data());
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            const std::uint16_t samples[] {
                static_cast<std::uint16_t>(left[i]),
                static_cast<std::uint16_t>(right[i])};
            for (const auto sample : samples)
            {
                hash ^= static_cast<std::uint8_t>(sample);
                hash *= UINT64_C(1099511628211);
                hash ^= static_cast<std::uint8_t>(sample >> 8u);
                hash *= UINT64_C(1099511628211);
            }
        }
    }
    return hash;
}

void nativePcm()
{
    const auto quiet = candidate(canonical(rawKit(
        {{'Q','U','I','E','T',0,0,0}}, 20u)));
    const auto loud = candidate(canonical(rawKit(
        {{'L','O','U','D',0,0,0,0}}, 120u)));
    const auto first = renderHash(quiet);
    const auto repeat = renderHash(quiet);
    const auto changed = renderHash(loud);
    check(first == repeat, "matched production kit PCM is deterministic");
    check(first != changed, "distinct imported volume changes native PCM");
    std::printf("\"pcm_quiet\":\"%016llX\",\"pcm_loud\":\"%016llX\",",
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

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    nameContract();
    statusContract();
    fileOperations(argv[1]);
    transactionContract();
    activeRuntimeContinuity();
    std::printf("{");
    nativePcm();
    std::printf("\"pass\":%llu,\"fail\":%llu,\"publication_allocations\":%llu}\n",
                static_cast<unsigned long long>(passed),
                static_cast<unsigned long long>(failed),
                static_cast<unsigned long long>(allocations.load()));
    return failed == 0u ? 0 : 1;
}
