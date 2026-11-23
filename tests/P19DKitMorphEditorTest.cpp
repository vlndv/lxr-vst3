#include "interfaces/P19DGUI.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
std::uint64_t passed = 0;
std::uint64_t failed = 0;

void check(bool condition, const char* label)
{
    if (condition) ++passed;
    else
    {
        ++failed;
        if (failed < 24u) std::fprintf(stderr, "FAIL: %s\n", label);
    }
}

std::array<wchar_t, lxr::kit::snd::kPathCapacity> temporaryPath(
    const wchar_t* leaf)
{
    std::array<wchar_t, lxr::kit::snd::kPathCapacity> path {};
    const DWORD count = GetTempPathW(static_cast<DWORD>(path.size()), path.data());
    if (count == 0u || count >= path.size()) return {};
    wchar_t suffix[96] {};
    _snwprintf_s(suffix, _countof(suffix), _TRUNCATE, L"lxr-p19d-%lu-%s",
                 static_cast<unsigned long>(GetCurrentProcessId()), leaf);
    std::size_t at = count;
    for (std::size_t i = 0; suffix[i] != 0 && at + 1u < path.size(); ++i)
        path[at++] = suffix[i];
    return path;
}

lxr::kit::CanonicalKit makeKit(
    const std::array<std::uint8_t, lxr::kit::kNameBytes>& name,
    std::uint8_t marker)
{
    auto raw = lxr::kit::MakeFirmwareInitializationKit();
    raw.name = name;
    raw.values[8] = marker;
    lxr::kit::CanonicalKit result;
    check(static_cast<bool>(lxr::kit::DecodeRawKit(raw, result)),
          "decode source-grounded kit fixture");
    return result;
}

lxr::kit::PatchCandidate patch(const lxr::kit::CanonicalKit& kit)
{
    lxr::kit::PatchCandidate result;
    check(static_cast<bool>(lxr::kit::BuildPatchCandidate(kit, result)),
          "build accepted source projection");
    return result;
}

void binaryNames()
{
    using lxr::kit::management::FormatName;
    const auto unnamed = FormatName({{0,0,0,0,0,0,0,0}});
    check(unnamed.unnamed, "all-zero name is unnamed");
    check(std::strcmp(unnamed.text.data(), "<unnamed>") == 0,
          "all-zero display label");
    const auto binary = FormatName({{'A',0,'B',0x80u,0xffu,'Z',0,0}});
    check(!binary.unnamed, "binary identity remains present");
    check(std::strcmp(binary.text.data(), "A\\x00B\\x80\\xFFZ") == 0,
          "embedded zero and high bytes use uppercase escapes");
    const auto trailing = FormatName({{'K','I','T',0,0,0,0,0}});
    check(std::strcmp(trailing.text.data(), "KIT") == 0,
          "trailing zero bytes are display-trimmed only");
}

void statusPresentation()
{
    using KS = lxr::kit::management::Status;
    using MS = lxr::morph::management::Status;
    constexpr std::array<KS, 14> kitStatuses {{
        KS::Ready, KS::Success, KS::Cancelled, KS::InvalidFileSize,
        KS::InvalidField, KS::InvalidRoute, KS::UnsupportedField,
        KS::UnsupportedRoute, KS::FileReadFailure, KS::FileWriteFailure,
        KS::ExistingDestination, KS::PublicationBusy, KS::StaleOperation,
        KS::HostUnavailable }};
    constexpr std::array<MS, 15> morphStatuses {{
        MS::Ready, MS::Success, MS::Cancelled, MS::NoTarget,
        MS::InvalidFileSize, MS::InvalidField, MS::InvalidRoute,
        MS::UnsupportedField, MS::UnsupportedRoute, MS::FileReadFailure,
        MS::FileWriteFailure, MS::ExistingDestination,
        MS::PublicationBusy, MS::StaleOperation, MS::HostUnavailable }};
    for (const auto status : kitStatuses)
    {
        const char* text = lxr::kit::management::StatusDescription(status);
        check(text != nullptr && text[0] != 0, "typed kit status has text");
    }
    for (const auto status : morphStatuses)
    {
        const char* text = lxr::morph::management::StatusDescription(status);
        check(text != nullptr && text[0] != 0, "typed morph status has text");
    }
    check(std::strstr(lxr::kit::management::StatusDescription(
              KS::Cancelled), "unchanged") != nullptr,
          "kit cancellation is not success");
    check(std::strstr(lxr::morph::management::StatusDescription(
              MS::NoTarget), "no morph target") != nullptr,
          "missing morph target is explicit");
}

void fileWorkflow()
{
    using namespace lxr::kit;
    using namespace lxr::kit::snd;
    const auto path = temporaryPath(L"kit.snd");
    const auto shortPath = temporaryPath(L"short.snd");
    DeleteFileW(path.data());
    DeleteFileW(shortPath.data());

    const auto source = makeKit({{'S','R','C',0,0,0,0,0}}, 37u);
    const auto accepted = patch(source);
    const auto write = WriteNewFile(path.data(), source, accepted.routes);
    check(static_cast<bool>(write) && write.published,
          "atomic no-replace .snd commit");
    WIN32_FILE_ATTRIBUTE_DATA attributes {};
    check(GetFileAttributesExW(path.data(), GetFileExInfoStandard,
                               &attributes) != FALSE &&
          attributes.nFileSizeHigh == 0u && attributes.nFileSizeLow == kSize,
          "export is exactly 236 bytes");
    const auto existing = WriteNewFile(path.data(), source, accepted.routes);
    check(existing.error == FileError::DestinationExists,
          "export preserves no-overwrite semantics");
    CanonicalKit restored;
    const auto read = ReadFile(path.data(), restored);
    check(static_cast<bool>(read), "valid .snd import");
    check(restored.name == source.name, "binary identity survives import/export");

    HANDLE shortFile = CreateFileW(shortPath.data(), GENERIC_WRITE, 0, nullptr,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(shortFile != INVALID_HANDLE_VALUE, "create malformed fixture");
    if (shortFile != INVALID_HANDLE_VALUE)
    {
        std::array<std::uint8_t, kSize - 1u> bytes {};
        DWORD written = 0;
        check(WriteFile(shortFile, bytes.data(), static_cast<DWORD>(bytes.size()),
                        &written, nullptr) != FALSE && written == bytes.size(),
              "write malformed fixture");
        CloseHandle(shortFile);
    }
    CanonicalKit unchanged = source;
    const auto invalid = ReadFile(shortPath.data(), unchanged);
    check(invalid.error == FileError::Codec &&
          invalid.codec.error == Error::Truncated,
          "235-byte import is rejected as invalid size");
    check(unchanged.name == source.name,
          "failed import preserves accepted candidate output");

    DeleteFileW(path.data());
    DeleteFileW(shortPath.data());
}

void morphWorkflow()
{
    const auto sourceKit = makeKit({{'S','O','U','R','C','E',0,0}}, 10u);
    const auto source = patch(sourceKit);
    const auto targetKit = makeKit({{'T',0x01u,'R','G','E','T',0,0}}, 110u);
    const auto replacementKit = makeKit({{'N','E','W',0,0,0,0,0}}, 70u);
    lxr::morph::Target target;
    lxr::morph::Target replacement;
    check(static_cast<bool>(lxr::morph::ValidateTarget(targetKit, target)),
          "validate complete morph target");
    check(static_cast<bool>(lxr::morph::ValidateTarget(replacementKit,
                                                       replacement)),
          "validate complete replacement target");

    lxr::morph::StateAuthority first;
    const auto load = first.tryPublishTarget(target, 1u, first.generation());
    check(load.status == lxr::morph::PublicationStatus::Ok,
          "target load publishes atomically");
    const auto stale = first.tryPublishTarget(replacement, 2u, 0u);
    check(stale.status == lxr::morph::PublicationStatus::Stale,
          "stale replacement rejected");
    lxr::morph::Snapshot snapshot;
    check(first.trySnapshot(snapshot) && snapshot.target.name == target.name,
          "failed replacement preserves prior target");
    const auto replacementResult = first.tryPublishTarget(
        replacement, 3u, first.generation());
    check(static_cast<bool>(replacementResult), "valid target replacement");
    check(static_cast<bool>(first.tryPublishAmount(255u, 4u)),
          "host-visible amount accepts 255");
    const auto clear = first.tryClear(5u, first.generation());
    check(static_cast<bool>(clear), "target clear succeeds");
    check(first.trySnapshot(snapshot) && !snapshot.targetPresent &&
          snapshot.amount == 255u,
          "clear removes B and retains Morph Amount");

    lxr::morph::StateAuthority restored;
    check(static_cast<bool>(restored.tryPublishAmount(127u, 1u)),
          "restored amount publishes independently");
    check(restored.trySnapshot(snapshot) && !snapshot.targetPresent &&
          snapshot.amount == 127u,
          "amount restoration does not fabricate target");

    lxr::morph::Snapshot exportSnapshot;
    exportSnapshot.target = target;
    exportSnapshot.amount = 255u;
    exportSnapshot.targetPresent = true;
    exportSnapshot.generation = 8u;
    lxr::kit::CanonicalKit projected;
    const auto projectedResult = lxr::morph::management::BuildProjectedKit(
        source.patch, source.routes, exportSnapshot, projected);
    check(static_cast<bool>(projectedResult), "MorphSnd coherent projection");
    check(projected.name == target.name, "MorphSnd uses target binary identity");
    check(lxr::morph::InterpolateByte(127u, 0u, 255u) == 1u,
          "amount 255 firmware quirk is preserved");

    lxr::morph::StateAuthority second;
    check(second.trySnapshot(snapshot) && !snapshot.targetPresent,
          "multiple instances retain independent targets");
}
}

int main()
{
    binaryNames();
    statusPresentation();
    fileWorkflow();
    morphWorkflow();
    if (failed != 0u)
    {
        std::fprintf(stderr, "P19-D Kits/Morph editor: FAIL (%llu/%llu)\n",
                     static_cast<unsigned long long>(failed),
                     static_cast<unsigned long long>(passed + failed));
        return 1;
    }
    std::printf("P19-D Kits/Morph editor: PASS (%llu checks)\n",
                static_cast<unsigned long long>(passed));
    return 0;
}
