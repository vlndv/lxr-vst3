#include "KitFileIO.h"

namespace lxr::kit::snd {
namespace {
bool Slash(wchar_t c) noexcept { return c == L'\\' || c == L'/'; }
wchar_t Upper(wchar_t c) noexcept { return c >= L'a' && c <= L'z' ? c - (L'a' - L'A') : c; }
bool Path(const wchar_t* path, std::size_t& length) noexcept {
    if (!path) return false;
    length = 0;
    while (length < kPathCapacity - 80 && path[length] != 0) ++length;
    if (length < 4 || length >= kPathCapacity - 80) return false;
    const bool drive = Upper(path[0]) >= L'A' && Upper(path[0]) <= L'Z' && path[1] == L':' && Slash(path[2]);
    const bool unc = Slash(path[0]) && Slash(path[1]) && path[2] != L'.' && path[2] != L'?' && !Slash(path[2]);
    if (!drive && !unc) return false;
    if (Slash(path[length - 1]) || path[length - 1] == L'.' || path[length - 1] == L' ') return false;
    std::size_t leaf = 0;
    for (std::size_t i = 0; i < length; ++i) {
        if (path[i] == L':' && (!drive || i != 1)) return false; // No ADS/device streams.
        if (Slash(path[i])) leaf = i + 1;
    }
    // Reject DOS device leaf names even when written with a directory/extension.
    std::array<wchar_t, 5> stem {};
    unsigned n = 0;
    while (leaf + n < length && path[leaf + n] != L'.' && n < stem.size()) {
        stem[n] = Upper(path[leaf + n]); ++n;
    }
    if (n == 3 && ((stem[0] == L'C' && stem[1] == L'O' && stem[2] == L'N') ||
                  (stem[0] == L'P' && stem[1] == L'R' && stem[2] == L'N') ||
                  (stem[0] == L'A' && stem[1] == L'U' && stem[2] == L'X') ||
                  (stem[0] == L'N' && stem[1] == L'U' && stem[2] == L'L'))) return false;
    if (n == 4 && stem[3] >= L'1' && stem[3] <= L'9' &&
        ((stem[0] == L'C' && stem[1] == L'O' && stem[2] == L'M') ||
         (stem[0] == L'L' && stem[1] == L'P' && stem[2] == L'T'))) return false;
    return true;
}
bool ReadOps(const FileOps& ops) noexcept { return ops.openRead && ops.size && ops.read && ops.close; }
bool WriteOps(const FileOps& ops) noexcept {
    return ops.createNew && ops.write && ops.flush && ops.publishNoReplace && ops.discardOwned && ops.close && ops.temporaryToken;
}
FileResult ReadBytes(const wchar_t* path, Bytes& output, const FileOps& ops) noexcept {
    std::size_t length = 0;
    if (!Path(path, length)) return {FileError::InvalidPath};
    if (!ReadOps(ops)) return {FileError::InvalidOperations};
    IoFailure error {};
    const auto handle = ops.openRead(ops.context, path, error);
    if (!handle) return {FileError::Open, {}, error};
    FileResult result {};
    std::uint64_t size = 0;
    Bytes candidate {};
    if (!ops.size(ops.context, handle, size, error)) result = {FileError::Size, {}, error};
    else if (size != kSize) result = {FileError::Codec, {size < kSize ? Error::Truncated : Error::Oversized}};
    else {
        std::uint32_t count = 0;
        if (!ops.read(ops.context, handle, candidate.data(), static_cast<std::uint32_t>(candidate.size()), count, error))
            result = {FileError::Read, {}, error};
        else if (count != kSize) result = {FileError::ShortRead};
        else {
            std::uint8_t extra = 0;
            if (!ops.read(ops.context, handle, &extra, 1, count, error)) result = {FileError::Read, {}, error};
            else if (count != 0) result = {FileError::Codec, {Error::Oversized}};
        }
    }
    IoFailure closeError {};
    if (!ops.close(ops.context, handle, closeError)) {
        if (result) result = {FileError::Close, {}, closeError};
        else { result.cleanupFailed = true; result.cleanup = closeError; }
    }
    if (result) output = candidate;
    return result;
}
FileResult Discard(FileResult result, FileHandle handle,
                   const std::array<wchar_t, kPathCapacity>& temporary, const FileOps& ops) noexcept {
    IoFailure error {};
    if (!ops.discardOwned(ops.context, handle, error)) {
        result.cleanupFailed = true; result.cleanup = error;
    }
    IoFailure closeError {};
    if (!ops.close(ops.context, handle, closeError)) {
        result.cleanupFailed = true;
        if (result.cleanup.native == 0) result.cleanup = closeError;
    }
    if (result.cleanupFailed) result.temporary = temporary;
    return result;
}
FileResult WriteBytes(const wchar_t* path, const Bytes& bytes, const FileOps& ops) noexcept {
    std::size_t length = 0;
    if (!Path(path, length)) return {FileError::InvalidPath};
    if (!WriteOps(ops)) return {FileError::InvalidOperations};
    std::array<wchar_t, kPathCapacity> temporary {};
    for (std::size_t i = 0; i < length; ++i) temporary[i] = path[i];
    const auto token = ops.temporaryToken(ops.context);
    FileHandle handle = nullptr;
    IoFailure error {};
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        std::size_t at = length;
        for (const auto c : L".lxr-tmp-") if (c) temporary[at++] = c;
        for (unsigned digit = 0; digit < 16; ++digit)
            temporary[at++] = L"0123456789abcdef"[(token >> ((15 - digit) * 4)) & 15u];
        temporary[at++] = L'-'; temporary[at++] = L"0123456789abcdef"[attempt]; temporary[at] = 0;
        error = {};
        handle = ops.createNew(ops.context, temporary.data(), error);
        if (handle) break;
        if (!error.exists) return {FileError::Create, {}, error};
    }
    if (!handle) return {FileError::TemporaryExhausted, {}, error};
    std::uint32_t count = 0;
    if (!ops.write(ops.context, handle, bytes.data(), static_cast<std::uint32_t>(bytes.size()), count, error))
        return Discard({FileError::Write, {}, error}, handle, temporary, ops);
    if (count != kSize) return Discard({FileError::ShortWrite}, handle, temporary, ops);
    if (!ops.flush(ops.context, handle, error)) return Discard({FileError::Flush, {}, error}, handle, temporary, ops);
    // Rename the opened object while private/exclusive; never close and reopen
    // a temporary path, so replacement races cannot change cleanup ownership.
    if (!ops.publishNoReplace(ops.context, handle, path, error))
        return Discard({error.exists ? FileError::DestinationExists : FileError::Publish, {}, error}, handle, temporary, ops);
    FileResult result {}; result.published = true;
    if (!ops.close(ops.context, handle, error)) {
        // Rename already committed a complete flushed file. Report honestly;
        // never delete that now-published destination as "temporary cleanup".
        result.error = FileError::Close; result.io = error;
    }
    return result;
}
FileResult CodecFailure(Result result) noexcept { return {FileError::Codec, result}; }
} // namespace
FileResult ReadFile(const wchar_t* path, RawKit& output, const FileOps& ops) noexcept {
    Bytes bytes {};
    const auto result = ReadBytes(path, bytes, ops);
    if (!result) return result;
    const auto decoded = DecodeRaw(bytes.data(), bytes.size(), output);
    return decoded ? result : CodecFailure(decoded);
}
FileResult ReadFile(const wchar_t* path, CanonicalKit& output, const FileOps& ops) noexcept {
    Bytes bytes {};
    const auto result = ReadBytes(path, bytes, ops);
    if (!result) return result;
    const auto decoded = DecodeCanonical(bytes.data(), bytes.size(), output);
    return decoded ? result : CodecFailure(decoded);
}
FileResult WriteNewFile(const wchar_t* path, const RawKit& raw, const FileOps& ops) noexcept {
    Bytes bytes {};
    const auto result = EncodeRaw(raw, bytes.data(), bytes.size());
    return result ? WriteBytes(path, bytes, ops) : CodecFailure(result);
}
FileResult WriteNewFile(const wchar_t* path, const CanonicalKit& kit, const FileOps& ops) noexcept {
    Bytes bytes {};
    const auto result = EncodeCanonical(kit, bytes.data(), bytes.size());
    return result ? WriteBytes(path, bytes, ops) : CodecFailure(result);
}
FileResult WriteNewFile(const wchar_t* path, const CanonicalKit& kit,
    const std::array<PersistentRouteRecord, kPersistentRouteCount>& routes, const FileOps& ops) noexcept {
    Bytes bytes {};
    const auto result = EncodeCanonical(kit, routes, bytes.data(), bytes.size());
    return result ? WriteBytes(path, bytes, ops) : CodecFailure(result);
}
} // namespace lxr::kit::snd
