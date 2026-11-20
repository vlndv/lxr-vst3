#pragma once

// Non-real-time user operation contract for original LXR 0.37 morph targets.
// Original identities Copyright 2013 Julian Schmidt; see LICENSE-LXR.txt.

#include "KitManagement.h"
#include "Morphing.h"

#include <array>
#include <cstdint>

namespace lxr::morph::management
{

enum class Action : std::uint8_t
{
    None,
    LoadTarget,
    ClearTarget,
    ExportMorphed
};

enum class Status : std::uint8_t
{
    Ready,
    Success,
    Cancelled,
    NoTarget,
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
    kit::snd::FileError fileError = kit::snd::FileError::Ok;
    kit::snd::Error codecError = kit::snd::Error::Ok;
    kit::Issue issue = kit::Issue::None;
    PublicationStatus publicationStatus = PublicationStatus::InvalidCandidate;
    std::uint16_t field = 0xffffu;
    std::uint8_t route = 0xffu;
    std::uint32_t nativeError = 0;
    bool published = false;
    bool targetPresent = false;
    std::uint8_t amount = 0;
    std::uint64_t generation = 0;
    std::array<std::uint8_t, kit::kNameBytes> name {};

    constexpr explicit operator bool() const noexcept
    {
        return status == Status::Success;
    }
};

inline Status CodecStatus(kit::snd::Error error) noexcept
{
    switch (error)
    {
        case kit::snd::Error::Truncated:
        case kit::snd::Error::Oversized:
        case kit::snd::Error::BufferTooSmall:
            return Status::InvalidFileSize;
        case kit::snd::Error::InvalidSelector:
        case kit::snd::Error::InvalidRoute:
            return Status::InvalidRoute;
        case kit::snd::Error::UnsupportedRoute:
        case kit::snd::Error::NonrepresentableRoute:
            return Status::UnsupportedRoute;
        case kit::snd::Error::InvalidField:
        case kit::snd::Error::InvalidArgument:
        default:
            return Status::InvalidField;
    }
}

inline OperationResult FromFileFailure(
    Action action, const kit::snd::FileResult& file) noexcept
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
    if (file.error == kit::snd::FileError::Codec)
        result.status = CodecStatus(file.codec.error);
    else if (file.error == kit::snd::FileError::DestinationExists)
        result.status = Status::ExistingDestination;
    else if (action == Action::LoadTarget)
        result.status = Status::FileReadFailure;
    else
        result.status = Status::FileWriteFailure;
    return result;
}

inline OperationResult FromPublicationFailure(
    const LoadResult& load) noexcept
{
    OperationResult result;
    result.action = Action::LoadTarget;
    result.issue = load.candidate.issue;
    result.field = load.candidate.field;
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

inline OperationResult BuildProjectedKit(
    const std::array<std::uint8_t, kit::kFieldCount>& sourcePatch,
    const std::array<PersistentRouteRecord, kPersistentRouteCount>& routes,
    const Snapshot& morph,
    kit::CanonicalKit& output) noexcept
{
    OperationResult result;
    result.action = Action::ExportMorphed;
    result.targetPresent = morph.targetPresent;
    result.amount = morph.amount;
    result.generation = morph.generation;
    result.name = morph.targetPresent
        ? morph.target.name : std::array<std::uint8_t, kit::kNameBytes>{};
    if (!morph.targetPresent)
    {
        result.status = Status::NoTarget;
        return result;
    }

    Values source;
    const auto sourceResult = BuildSourceValues(sourcePatch, routes, source);
    if (!sourceResult)
    {
        result.issue = sourceResult.issue;
        result.field = sourceResult.field;
        result.status = sourceResult.issue == kit::Issue::RouteMetadata ||
                        sourceResult.issue == kit::Issue::RouteIdentity ||
                        sourceResult.issue == kit::Issue::RouteNamespace ||
                        sourceResult.issue == kit::Issue::Source
            ? Status::InvalidRoute : Status::InvalidField;
        return result;
    }

    kit::RawKit raw;
    raw.name = morph.target.name;
    BuildEffectiveProjection(
        source, morph.target.values, morph.amount, raw.values);
    kit::CanonicalKit candidate;
    const auto decoded = kit::DecodeRawKit(raw, candidate);
    if (!decoded)
    {
        result.issue = decoded.issue;
        result.field = decoded.field;
        result.status = decoded.issue == kit::Issue::Selector ||
                        decoded.issue == kit::Issue::CrossVoice ||
                        decoded.issue == kit::Issue::RouteMetadata
            ? Status::InvalidRoute : Status::InvalidField;
        return result;
    }
    const auto representable = kit::snd::CheckRepresentability(
        candidate, routes);
    if (!representable)
    {
        result.codecError = representable.error;
        result.issue = representable.detail;
        result.field = representable.field;
        result.route = representable.route;
        result.status = CodecStatus(representable.error);
        return result;
    }

    output = candidate;
    result.status = Status::Success;
    return result;
}

inline const char* StatusDescription(Status status) noexcept
{
    switch (status)
    {
        case Status::Ready: return "Ready.";
        case Status::Success: return "Morph operation completed.";
        case Status::Cancelled: return "Operation cancelled; morph state unchanged.";
        case Status::NoTarget: return "Export rejected: no morph target is loaded.";
        case Status::InvalidFileSize: return "Rejected: .snd file must be exactly 236 bytes.";
        case Status::InvalidField: return "Rejected: invalid morph-target field; prior target retained.";
        case Status::InvalidRoute: return "Rejected: invalid modulation route; prior target retained.";
        case Status::UnsupportedField: return "Rejected: target uses an unsupported sample or field.";
        case Status::UnsupportedRoute: return "Rejected: source route cannot be represented in original .snd.";
        case Status::FileReadFailure: return "Target load failed while reading the selected file.";
        case Status::FileWriteFailure: return "Morph export failed before a confirmed result.";
        case Status::ExistingDestination: return "Morph export rejected: destination already exists.";
        case Status::PublicationBusy: return "Morph operation Busy; accepted state retained. Try again.";
        case Status::StaleOperation: return "Morph operation became stale; newer state retained.";
        case Status::HostUnavailable: return "A coherent morph snapshot is unavailable; try again.";
        default: return "Morph operation failed.";
    }
}

} // namespace lxr::morph::management
