#pragma once
// Non-real-time file boundary. See LICENSE-LXR.txt for derived format semantics.
#include "KitFileCodec.h"

namespace lxr::kit::snd {
inline constexpr std::size_t kPathCapacity = 1024;
struct IoFailure final { std::uint32_t native = 0; bool exists = false; };
using FileHandle = void*; // Opaque OS handle, never an owning heap pointer.

// Borrowed synchronous OS boundary, also permits independent deterministic fault
// injection. No callback may retain a supplied buffer/path. Handle operations
// must act on the opened object, not reopen a lexical temporary path.
struct FileOps final {
    void* context = nullptr;
    FileHandle (*openRead)(void*, const wchar_t*, IoFailure&) noexcept = nullptr;
    FileHandle (*createNew)(void*, const wchar_t*, IoFailure&) noexcept = nullptr;
    bool (*size)(void*, FileHandle, std::uint64_t&, IoFailure&) noexcept = nullptr;
    bool (*read)(void*, FileHandle, std::uint8_t*, std::uint32_t, std::uint32_t&, IoFailure&) noexcept = nullptr;
    bool (*write)(void*, FileHandle, const std::uint8_t*, std::uint32_t, std::uint32_t&, IoFailure&) noexcept = nullptr;
    bool (*flush)(void*, FileHandle, IoFailure&) noexcept = nullptr;
    bool (*publishNoReplace)(void*, FileHandle, const wchar_t*, IoFailure&) noexcept = nullptr;
    bool (*discardOwned)(void*, FileHandle, IoFailure&) noexcept = nullptr;
    bool (*close)(void*, FileHandle, IoFailure&) noexcept = nullptr;
    std::uint64_t (*temporaryToken)(void*) noexcept = nullptr;
};
enum class FileError : std::uint8_t {
    Ok, Codec, InvalidPath, InvalidOperations, Open, Size, Read, ShortRead,
    Create, TemporaryExhausted, Write, ShortWrite, Flush, DestinationExists, Publish, Close
};
struct FileResult final {
    FileError error = FileError::Ok;
    Result codec {};
    IoFailure io {};
    bool published = false; // Commit point is successful no-replace rename.
    bool cleanupFailed = false;
    IoFailure cleanup {};
    std::array<wchar_t, kPathCapacity> temporary {}; // Only on incomplete cleanup.
    constexpr explicit operator bool() const noexcept { return error == FileError::Ok; }
};

FileOps NativeFileOps() noexcept; // Windows implementation; no mutable globals.
// Explicit absolute drive/UNC paths, bounded and no device/stream namespace.
// No directory creation, overwrite option or UI. Never call on audio.
FileResult ReadFile(const wchar_t*, RawKit&, const FileOps&) noexcept;
FileResult ReadFile(const wchar_t*, CanonicalKit&, const FileOps&) noexcept;
FileResult WriteNewFile(const wchar_t*, const RawKit&, const FileOps&) noexcept;
FileResult WriteNewFile(const wchar_t*, const CanonicalKit&, const FileOps&) noexcept;
FileResult WriteNewFile(const wchar_t*, const CanonicalKit&,
    const std::array<PersistentRouteRecord, kPersistentRouteCount>&, const FileOps&) noexcept;
// Convenience overloads still remain explicitly non-real-time operations.
inline FileResult ReadFile(const wchar_t* p, RawKit& k) noexcept { return ReadFile(p, k, NativeFileOps()); }
inline FileResult ReadFile(const wchar_t* p, CanonicalKit& k) noexcept { return ReadFile(p, k, NativeFileOps()); }
inline FileResult WriteNewFile(const wchar_t* p, const RawKit& k) noexcept { return WriteNewFile(p, k, NativeFileOps()); }
inline FileResult WriteNewFile(const wchar_t* p, const CanonicalKit& k) noexcept { return WriteNewFile(p, k, NativeFileOps()); }
inline FileResult WriteNewFile(const wchar_t* p, const CanonicalKit& k,
    const std::array<PersistentRouteRecord, kPersistentRouteCount>& r) noexcept {
    return WriteNewFile(p, k, r, NativeFileOps());
}
} // namespace lxr::kit::snd
