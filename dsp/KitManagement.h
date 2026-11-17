#pragma once

// Non-real-time user operation contract for original LXR 0.37 whole-kit files.
// Original identities Copyright 2013 Julian Schmidt; see LICENSE-LXR.txt.

#include "KitFileIO.h"
#include "RuntimeKitLoad.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace lxr::kit::management
{

enum class Action : std::uint8_t
{
    None,
    Import,
    Export
};

enum class Status : std::uint8_t
{
    Ready,
    Success,
    Cancelled,
    InvalidFileSize,
    InvalidField,
    InvalidRoute,
    UnsupportedField,
    UnsupportedRoute,
    FileReadFailure,
    FileWriteFailure,
    ExistingDestination,
    PublicationBusy,
    StaleOperation,
    HostUnavailable
};

struct OperationResult final
{
    Action action = Action::None;
    Status status = Status::Ready;
    snd::FileError fileError = snd::FileError::Ok;
    snd::Error codecError = snd::Error::Ok;
    kit::Issue issue = kit::Issue::None;
    PublicationStatus publicationStatus = PublicationStatus::InvalidCandidate;
    std::uint16_t field = 0xffffu;
    std::uint8_t route = 0xffu;
    std::uint32_t nativeError = 0;
    bool published = false;
    std::array<std::uint8_t, kNameBytes> name {};

    constexpr explicit operator bool() const noexcept
    {
        return status == Status::Success;
    }
};

struct DisplayName final
{
    // Worst case is eight escaped bytes ("\\xNN") plus terminator.
    std::array<char, kNameBytes * 4u + 1u> text {};
    bool unnamed = true;
};

inline DisplayName FormatName(
    const std::array<std::uint8_t, kNameBytes>& raw) noexcept
{
    DisplayName display;
    std::size_t end = raw.size();
    while (end != 0u && raw[end - 1u] == 0u)
        --end;
    if (end == 0u)
    {
        constexpr char label[] = "<unnamed>";
        for (std::size_t i = 0; i < sizeof(label); ++i)
            display.text[i] = label[i];
        return display;
    }

    static constexpr char hex[] = "0123456789ABCDEF";
    std::size_t at = 0;
    for (std::size_t i = 0; i < end; ++i)
    {
        const auto value = raw[i];
        if (value >= 0x20u && value <= 0x7eu)
            display.text[at++] = static_cast<char>(value);
        else
        {
            display.text[at++] = '\\';
            display.text[at++] = 'x';
            display.text[at++] = hex[value >> 4u];
            display.text[at++] = hex[value & 0x0fu];
        }
    }
    display.text[at] = 0;
    display.unnamed = false;
    return display;
}

inline Status CodecStatus(snd::Error error) noexcept
{
    switch (error)
    {
        case snd::Error::Truncated:
        case snd::Error::Oversized:
        case snd::Error::BufferTooSmall:
            return Status::InvalidFileSize;
        case snd::Error::InvalidSelector:
        case snd::Error::InvalidRoute:
            return Status::InvalidRoute;
        case snd::Error::UnsupportedRoute:
        case snd::Error::NonrepresentableRoute:
            return Status::UnsupportedRoute;
        case snd::Error::InvalidField:
        case snd::Error::InvalidArgument:
        default:
            return Status::InvalidField;
    }
}

inline OperationResult FromFileFailure(
    Action action, const snd::FileResult& file) noexcept
{
    OperationResult result;
    result.action = action;
    result.fileError = file.error;
    result.codecError = file.codec.error;
    result.issue = file.codec.detail;
    result.field = file.codec.field;
    result.route = file.codec.route;
    result.nativeError = file.io.native;
    result.published = file.published;
    if (file.error == snd::FileError::Codec)
        result.status = CodecStatus(file.codec.error);
    else if (file.error == snd::FileError::DestinationExists)
        result.status = Status::ExistingDestination;
    else if (action == Action::Import)
        result.status = Status::FileReadFailure;
    else
        result.status = Status::FileWriteFailure;
    return result;
}

inline OperationResult FromPublicationFailure(
    const RuntimeLoadResult& load) noexcept
{
    OperationResult result;
    result.action = Action::Import;
    result.issue = load.candidate.issue;
    result.field = load.candidate.field;
    result.route = load.publication.route;
    result.publicationStatus = load.publication.status;
    if (!load.candidate)
    {
        if (load.candidate.status == kit::Status::Unsupported)
            result.status = load.candidate.issue == kit::Issue::ExternalSample ||
                            load.candidate.issue == kit::Issue::NativeTarget
                ? Status::UnsupportedField : Status::UnsupportedRoute;
        else if (load.candidate.issue == kit::Issue::Selector ||
                 load.candidate.issue == kit::Issue::CrossVoice ||
                 load.candidate.issue == kit::Issue::GlobalRoute ||
                 load.candidate.issue == kit::Issue::RouteMetadata ||
                 load.candidate.issue == kit::Issue::RouteNamespace ||
                 load.candidate.issue == kit::Issue::RouteIdentity)
            result.status = Status::InvalidRoute;
        else
            result.status = Status::InvalidField;
    }
    else if (load.publication.status == PublicationStatus::Busy)
        result.status = Status::PublicationBusy;
    else if (load.publication.status == PublicationStatus::Stale)
        result.status = Status::StaleOperation;
    else
        result.status = Status::HostUnavailable;
    return result;
}

inline const char* StatusDescription(Status status) noexcept
{
    switch (status)
    {
        case Status::Ready: return "Ready.";
        case Status::Success: return "Operation completed.";
        case Status::Cancelled: return "Operation cancelled; state unchanged.";
        case Status::InvalidFileSize: return "Rejected: .snd file must be exactly 236 bytes.";
        case Status::InvalidField: return "Rejected: invalid kit field; state unchanged.";
        case Status::InvalidRoute: return "Rejected: invalid modulation route; state unchanged.";
        case Status::UnsupportedField: return "Rejected: kit uses an unsupported sample or field.";
        case Status::UnsupportedRoute: return "Rejected: route cannot be represented in original .snd.";
        case Status::FileReadFailure: return "Import failed while reading the selected file.";
        case Status::FileWriteFailure: return "Export failed before a confirmed result; plugin state is unchanged.";
        case Status::ExistingDestination: return "Export rejected: destination already exists.";
        case Status::PublicationBusy: return "Import Busy; previous kit retained. Try again.";
        case Status::StaleOperation: return "Import became stale; newer state retained.";
        case Status::HostUnavailable: return "Host state is unavailable; operation not applied.";
        default: return "Operation failed.";
    }
}

} // namespace lxr::kit::management
