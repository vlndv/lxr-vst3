#include "KitFileIO.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstddef>

namespace lxr::kit::snd {
namespace {
void Failure(IoFailure& error, DWORD code = GetLastError()) noexcept {
    error = {code, code == ERROR_FILE_EXISTS || code == ERROR_ALREADY_EXISTS};
}
FileHandle OpenRead(void*, const wchar_t* path, IoFailure& error) noexcept {
    const auto h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (h == INVALID_HANDLE_VALUE) { Failure(error); return nullptr; }
    if (GetFileType(h) != FILE_TYPE_DISK) { CloseHandle(h); Failure(error, ERROR_INVALID_FUNCTION); return nullptr; }
    return h;
}
FileHandle CreateNew(void*, const wchar_t* path, IoFailure& error) noexcept {
    const auto h = CreateFileW(path, GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { Failure(error); return nullptr; }
    return h; // CREATE_NEW and exclusive sharing establish private ownership.
}
bool Size(void*, FileHandle h, std::uint64_t& size, IoFailure& error) noexcept {
    LARGE_INTEGER value {};
    if (!GetFileSizeEx(h, &value)) { Failure(error); return false; }
    if (value.QuadPart < 0) { Failure(error, ERROR_INVALID_DATA); return false; }
    size = static_cast<std::uint64_t>(value.QuadPart); return true;
}
bool Read(void*, FileHandle h, std::uint8_t* data, std::uint32_t size, std::uint32_t& count, IoFailure& error) noexcept {
    DWORD n = 0;
    if (!::ReadFile(h, data, size, &n, nullptr)) { Failure(error); return false; }
    count = n; return true;
}
bool Write(void*, FileHandle h, const std::uint8_t* data, std::uint32_t size, std::uint32_t& count, IoFailure& error) noexcept {
    // Even a Windows-normalized device alias must never receive kit bytes.
    if (GetFileType(h) != FILE_TYPE_DISK) { Failure(error, ERROR_INVALID_FUNCTION); return false; }
    DWORD n = 0;
    if (!::WriteFile(h, data, size, &n, nullptr)) { Failure(error); return false; }
    count = n; return true;
}
bool Flush(void*, FileHandle h, IoFailure& error) noexcept {
    if (!FlushFileBuffers(h)) { Failure(error); return false; } return true;
}
bool Publish(void*, FileHandle h, const wchar_t* destination, IoFailure& error) noexcept {
    // SDK-defined FILE_RENAME_INFO header followed by bounded WCHAR storage.
    // No cast to a variable-length C++ array/object; offsets are ABI-checked.
    struct Rename final { DWORD flags = 0; HANDLE root = nullptr; DWORD bytes = 0; WCHAR name[kPathCapacity] {}; } record;
    static_assert(offsetof(Rename, root) == offsetof(FILE_RENAME_INFO, RootDirectory));
    static_assert(offsetof(Rename, bytes) == offsetof(FILE_RENAME_INFO, FileNameLength));
    static_assert(offsetof(Rename, name) == offsetof(FILE_RENAME_INFO, FileName));
    std::size_t length = 0;
    while (length < kPathCapacity && destination[length]) ++length;
    if (length == kPathCapacity) { Failure(error, ERROR_FILENAME_EXCED_RANGE); return false; }
    for (std::size_t i = 0; i < length; ++i) record.name[i] = destination[i];
    record.bytes = static_cast<DWORD>(length * sizeof(WCHAR));
    // flags0 => ReplaceIfExists FALSE. Full absolute name, RootDirectory NULL.
    if (!SetFileInformationByHandle(h, FileRenameInfo, &record,
        static_cast<DWORD>(offsetof(Rename, name) + record.bytes))) { Failure(error); return false; }
    return true;
}
bool Discard(void*, FileHandle h, IoFailure& error) noexcept {
    FILE_DISPOSITION_INFO info {}; info.DeleteFile = TRUE;
    if (!SetFileInformationByHandle(h, FileDispositionInfo, &info, sizeof(info))) { Failure(error); return false; }
    return true; // Deletion occurs on close of this exact privately-owned object.
}
bool Close(void*, FileHandle h, IoFailure& error) noexcept {
    if (!CloseHandle(h)) { Failure(error); return false; } return true;
}
std::uint64_t Token(void*) noexcept {
    FILETIME time {}; GetSystemTimeAsFileTime(&time);
    return ((static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime) ^
        (static_cast<std::uint64_t>(GetCurrentProcessId()) << 16) ^ GetCurrentThreadId();
}
} // namespace
FileOps NativeFileOps() noexcept { return {nullptr, OpenRead, CreateNew, Size, Read, Write, Flush, Publish, Discard, Close, Token}; }
} // namespace lxr::kit::snd
#else
namespace lxr::kit::snd { FileOps NativeFileOps() noexcept { return {}; } }
#endif
