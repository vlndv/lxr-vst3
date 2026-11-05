//
// FILE: vendor/iPlug2/Examples/LXR/LXR.cpp
//
// P12b-4c — Engine-to-Host Sample-Rate Conversion
//
// Original LXR synthesis:
//   Engine sample rate: approximately 44002.757 Hz
//   Processing quantum: 32 native frames
//   Internal audio: signed 16-bit PCM
//
// Host conversion:
//   Polyphase FIR resampling through LXRAudioAdapter
//   Native synthesis timing remains unchanged.
//
// The plugin exposes ST1 as its primary stereo output.
// ST2 remains internal.
//

#include "LXR.h"
#include "IPlug_include_in_plug_src.h"

#include "../../../../dsp/LXRResourceLocator.h"
#include "../../../../../dsp/FIRLatency.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#if defined(VST3_API)
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#endif

namespace
{

#if defined(VST3_API)
// Non-owning, callback-scoped adapters for the installed iPlug2 dispatcher.
// It reads each queue synchronously and does not retain these interfaces.
// Only MIDI CC proxies are canonicalized: native automation is forwarded
// untouched. Hosts may supply float-precision normalized MIDI (e.g. CC98=1
// becomes 0.999999996 after *127); MakeControlChangeMsg otherwise truncates it
// to the wrong wire value before ProcessMidiMsg can recover the selector.
class MidiValueQueue final : public Steinberg::Vst::IParamValueQueue
{
public:
    Steinberg::Vst::IParamValueQueue* source = nullptr;
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** object) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::Vst::IParamValueQueue::iid) ||
            Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid))
        {
            *object = static_cast<Steinberg::Vst::IParamValueQueue*>(this);
            return Steinberg::kResultOk;
        }
        *object = nullptr;
        return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
    Steinberg::Vst::ParamID PLUGIN_API getParameterId() override { return source->getParameterId(); }
    Steinberg::int32 PLUGIN_API getPointCount() override { return source->getPointCount(); }
    Steinberg::tresult PLUGIN_API getPoint(Steinberg::int32 index, Steinberg::int32& offset,
                                          Steinberg::Vst::ParamValue& value) override
    {
        const auto result = source->getPoint(index, offset, value);
        if (result != Steinberg::kResultOk)
            return result;
        if (!std::isfinite(value))
            return Steinberg::kResultFalse;
        value = std::round(iplug::Clip(value, 0.0, 1.0) * 127.0) / 127.0;
        return result;
    }
    Steinberg::tresult PLUGIN_API addPoint(Steinberg::int32, Steinberg::Vst::ParamValue,
                                          Steinberg::int32&) override { return Steinberg::kNotImplemented; }
};

bool DecodeNormalizedRouteToken(
    LXR& plug, Steinberg::Vst::ParamID tag,
    Steinberg::Vst::ParamValue normalized,
    lxr::route_host::RouteToken16& token) noexcept
{
    const auto id = lxr::route_host::RouteHostParamId::fromRaw(tag);
    if (!id.valid() || !std::isfinite(normalized) ||
        normalized < 0.0 || normalized > 1.0)
        return false;
    const auto* param = plug.GetParam(static_cast<int>(tag));
    if (param == nullptr)
        return false;
    const double plain = param->FromNormalized(normalized);
    if (!std::isfinite(plain) || plain < 0.0 ||
        plain > static_cast<double>(lxr::route_host::kMaximumToken))
        return false;
    token = lxr::route_host::RouteToken16::fromRaw(
        static_cast<std::uint32_t>(plain));
    return token.valid() && static_cast<bool>(lxr::route_host::decode(id, token));
}

class RouteValueQueue final : public Steinberg::Vst::IParamValueQueue
{
public:
    LXR* plug = nullptr;
    Steinberg::Vst::IParamValueQueue* source = nullptr;
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** object) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::Vst::IParamValueQueue::iid) ||
            Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid))
        {
            *object = static_cast<Steinberg::Vst::IParamValueQueue*>(this);
            return Steinberg::kResultOk;
        }
        *object = nullptr;
        return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
    Steinberg::Vst::ParamID PLUGIN_API getParameterId() override { return source->getParameterId(); }
    Steinberg::int32 PLUGIN_API getPointCount() override { return source->getPointCount(); }
    Steinberg::tresult PLUGIN_API getPoint(Steinberg::int32 index, Steinberg::int32& offset,
                                          Steinberg::Vst::ParamValue& value) override
    {
        const auto result = source->getPoint(index, offset, value);
        lxr::route_host::RouteToken16 token;
        return result == Steinberg::kResultOk && plug != nullptr &&
               DecodeNormalizedRouteToken(*plug, getParameterId(), value, token)
            ? result : Steinberg::kResultFalse;
    }
    Steinberg::tresult PLUGIN_API addPoint(Steinberg::int32, Steinberg::Vst::ParamValue,
                                          Steinberg::int32&) override { return Steinberg::kNotImplemented; }
};

class MidiParameterChanges final : public Steinberg::Vst::IParameterChanges
{
public:
    MidiParameterChanges(LXR& owner, Steinberg::Vst::IParameterChanges& changes)
        : plug(owner), source(changes) {}
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** object) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::Vst::IParameterChanges::iid) ||
            Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid))
        {
            *object = static_cast<Steinberg::Vst::IParameterChanges*>(this);
            return Steinberg::kResultOk;
        }
        *object = nullptr;
        return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
    Steinberg::int32 PLUGIN_API getParameterCount() override { return source.getParameterCount(); }
    Steinberg::Vst::IParamValueQueue* PLUGIN_API getParameterData(Steinberg::int32 index) override
    {
        auto* queue = source.getParameterData(index);
        if (queue == nullptr)
            return nullptr;
        const auto tag = queue->getParameterId();
        if (lxr::route_host::RouteHostParamId::fromRaw(tag).valid())
        {
            routeQueue.plug = &plug;
            routeQueue.source = queue;
            return &routeQueue;
        }
        if (tag >= iplug::kMIDICCParamStartIdx &&
            tag < iplug::kMIDICCParamStartIdx + VST3_NUM_CC_CHANS * Steinberg::Vst::kCountCtrlNumber &&
            (tag - iplug::kMIDICCParamStartIdx) % Steinberg::Vst::kCountCtrlNumber < 128)
        {
            midiQueue.source = queue;
            return &midiQueue;
        }
        return queue;
    }
    Steinberg::Vst::IParamValueQueue* PLUGIN_API addParameterData(const Steinberg::Vst::ParamID&,
                                                                Steinberg::int32&) override { return nullptr; }
private:
    LXR& plug;
    Steinberg::Vst::IParameterChanges& source;
    MidiValueQueue midiQueue;
    RouteValueQueue routeQueue;
};
#endif

// ---------------------------------------------------------
// Convert host parameters to native LXR 7-bit values.
// ---------------------------------------------------------

uint8_t GetRawLXRParameterValue(
    const iplug::IParam* param
)
{
    if (param == nullptr)
    {
        return 0;
    }

    int value = param->Int();

    if (value < 0)
    {
        value = 0;
    }
    else if (value > 127)
    {
        value = 127;
    }

    return static_cast<uint8_t>(value);
}

// ---------------------------------------------------------
// Clear every connected host output channel.
//
// Used when native assets are unavailable or sample-rate
// conversion has not been configured successfully.
// ---------------------------------------------------------

void ClearAudioOutputs(
    double** outputs,
    int nChans,
    int nFrames
)
{
    if (outputs == nullptr ||
        nChans <= 0 ||
        nFrames <= 0)
    {
        return;
    }

    for (int channel = 0;
         channel < nChans;
         ++channel)
    {
        double* output = outputs[channel];

        if (output == nullptr)
        {
            continue;
        }

        for (int sample = 0;
             sample < nFrames;
             ++sample)
        {
            output[sample] = 0.0;
        }
    }
}

bool SameRoute(const lxr::PersistentRouteRecord& a,
               const lxr::PersistentRouteRecord& b) noexcept
{
    return a.owner == b.owner && a.source == b.source &&
           a.flags == b.flags && a.identityNamespace == b.identityNamespace &&
           a.originalIndex == b.originalIndex &&
           a.firmwareParameter == b.firmwareParameter &&
           a.valueType == b.valueType && a.scope == b.scope &&
           a.physicalKey == b.physicalKey &&
           a.menuVoiceHint == b.menuVoiceHint && a.reserved == b.reserved;
}

const char* RouteSemanticName(lxr::modulation_contract::ValueSemantic semantic) noexcept
{
    using S = lxr::modulation_contract::ValueSemantic;
    switch (semantic)
    {
        case S::FrequencyMultiplier: return "pitch";
        case S::CutoffCyclesPerSample: return "filter cutoff";
        case S::Resonance: return "filter resonance";
        case S::EnvelopeStep: return "envelope";
        case S::SignedSlopeCoefficient: return "envelope slope";
        case S::PitchEnvelopeAmount: return "pitch envelope";
        case S::Pan: return "pan";
        case S::DistortionCoefficient: return "distortion";
        case S::FilterDrive: return "filter drive";
        case S::DecimationRate: return "decimation";
        case S::OscillatorWaveform: return "oscillator waveform";
        case S::FilterEnumeration: return "filter type";
        case S::TransientWaveform: return "transient waveform";
        case S::TransientPitch: return "transient pitch";
        case S::UnitFloat: return "volume/FM amount";
        default: return "typed target";
    }
}

void FormatRouteHostValue(std::size_t routeSlot, double value,
                          WDL_String& display)
{
    const auto id = lxr::route_host::parameterForSlot(routeSlot);
    const auto token = lxr::route_host::RouteToken16::fromRaw(
        value >= 0.0 && value <= lxr::route_host::kMaximumToken
            ? static_cast<std::uint32_t>(std::round(value))
            : lxr::route_host::kMaximumToken);
    const auto decoded = lxr::route_host::decode(id, token);
    if (decoded.status == lxr::route_host::TokenStatus::Off)
    {
        display.Set("Off");
        return;
    }
    if (decoded.status == lxr::route_host::TokenStatus::Reserved)
    {
        display.SetFormatted(64, "Reserved 0x%04X", token.value());
        return;
    }
    const auto* target = lxr::modulation_contract::lookup(
        lxr::modulation_contract::OriginalTargetIndex::fromRaw(token.value()));
    if (decoded.status != lxr::route_host::TokenStatus::Accepted || target == nullptr)
    {
        display.SetFormatted(96, "Unavailable: AVR %u %s", token.value(),
            target != nullptr ? target->symbol : "invalid");
        return;
    }
    display.SetFormatted(96, "AVR %u: %s", token.value(),
                         RouteSemanticName(target->semantic));
}

} // namespace

// ---------------------------------------------------------
// Constructor
// ---------------------------------------------------------

LXR::LXR(
    const iplug::InstanceInfo& info
)
    : iplug::Plugin(
          info,
          iplug::MakeConfig(
              lxr::vst::kNumParams,
              lxr::vst::kNumPresets
          )
      )
{
    // Preserve the native parameter contract.
    static_assert(
        lxr::vst::kNativeNumParams == lxr::kNumParams,
        "Native VST and DSP parameter counts must match"
    );

    static_assert(
        lxr::vst::kNativeNumParams == 228 &&
        lxr::vst::kNumParams == 240,
        "Unexpected native/host parameter counts"
    );

    // Register each native LXR parameter.
    for (int paramIdx = 0;
         paramIdx < lxr::vst::kNativeNumParams;
         ++paramIdx)
    {
        const auto& descriptor =
            lxr::vst::kParamDescriptors[
                static_cast<std::size_t>(paramIdx)
            ];

        auto* param = GetParam(paramIdx);

        assert(param != nullptr);

        if (param == nullptr)
        {
            continue;
        }

        param->InitInt(
            descriptor.name,
            descriptor.defaultValue,
            descriptor.minValue,
            descriptor.maxValue
        );
    }

    // These twelve integer parameters are host projections of mRoutingState.
    // They are not appended to ParameterArray or the 228-byte patch payload.
    for (std::size_t routeSlot = 0;
         routeSlot < lxr::route_host::kDescriptors.size(); ++routeSlot)
    {
        const auto& descriptor = lxr::route_host::kDescriptors[routeSlot];
        auto* param = GetParam(static_cast<int>(descriptor.id.value()));
        assert(param != nullptr);
        if (param == nullptr)
            continue;
        param->InitInt(descriptor.name, 0, 0,
                       static_cast<int>(lxr::route_host::kMaximumToken));
        param->SetDisplayFunc([routeSlot](double value, WDL_String& display) {
            FormatRouteHostValue(routeSlot, value, display);
        });
    }

    // Initialize canonical native parameters.
    mParams.init();

    StateAuthority::Values initialState {};
    for (int paramIdx = 0; paramIdx < lxr::vst::kNativeNumParams; ++paramIdx)
    {
        const std::size_t index = static_cast<std::size_t>(paramIdx);
        initialState[index] = mParams.get(static_cast<uint8_t>(paramIdx));
        mDeferredHostValues[index].store(0, std::memory_order_relaxed);
    }
    for (std::size_t routeSlot = 0;
         routeSlot < lxr::route_host::kParameterCount; ++routeSlot)
    {
        mDeferredRouteValues[routeSlot].store(0, std::memory_order_relaxed);
        mDeferredRouteRejections[routeSlot].store(0, std::memory_order_relaxed);
    }
    mState.initialize(initialState);
    mRoutingState.initialize();
    mAppliedRouting = lxr::MakeDisabledRoutingState();

    // Clear native cache and resampler state.
    // Host-rate configuration occurs in OnReset().
    mAudioAdapter.reset();

    // Resolve assets relative to the plugin module,
    // never relative to the DAW working directory.
    const std::string assetDirectory =
        lxr::GetPluginAssetDirectory();

    // Load original native oscillator/sample assets.
    // Rendering remains disabled if loading fails.
    mAssetsLoaded =
        !assetDirectory.empty() &&
        mEngine.loadAssets(
            assetDirectory.c_str()
        );

    // Do not call Engine::processBlock() unless
    // asset loading succeeded.
    TRACE;
}

// ---------------------------------------------------------
// Restore the native Engine from current host parameters.
// ---------------------------------------------------------

bool LXR::TryPersistentSnapshot(
    StateAuthority::Values& parameters,
    uint64_t& parameterGeneration,
    lxr::RoutingStateAuthority::Snapshot& routing) const
{
    lxr::RoutingStateAuthority::Guard guard;
    if (!mRoutingState.tryAcquire(guard))
        return false;
    if (!mState.trySnapshot(parameters, &parameterGeneration, 1))
        return false;
    return mRoutingState.copyLocked(guard, routing);
}

bool LXR::ApplyRoutesToEngine(
    lxr::Engine& engine,
    const std::array<lxr::PersistentRouteRecord,
                     lxr::kPersistentRouteCount>& routes) noexcept
{
    using namespace lxr::modulation_contract;
    for (std::size_t i = 0; i < routes.size(); ++i)
    {
        const auto& route = routes[i];
        const SourceIdentity source {
            route.source == 0u ? SourceKind::Velocity : SourceKind::Lfo,
            voiceFromZeroBased(route.owner)
        };
        const auto target = OriginalTargetIndex::fromRaw(route.originalIndex);
        const auto status = route.source == 0u
            ? engine.configureVelocityDestination(source, target)
            : engine.configureLfoDestination(source, target);
        if (status != lxr::VelocityRouteStatus::Ok)
            return false;
    }
    return true;
}

bool LXR::StageCompleteRestore(
    const StateAuthority::Values& parameters,
    const lxr::PersistentRoutingState& routing)
{
    // Copy only the fixed Engine bank. Asset views are non-owning and remain
    // resident; init performs no allocation or resource loading.
    mStagingEngine = mEngine;
    mStagingEngine.init();
    mStagingParams.init();
    for (std::size_t i = 0; i < parameters.size(); ++i)
    {
        mStagingParams.set(static_cast<uint8_t>(i), parameters[i],
                           mStagingEngine);
    }
    if (!ApplyRoutesToEngine(mStagingEngine, routing.routes))
        return false;

    // Selection is the processing-boundary linearization point. No renderer or
    // event consumer runs between these fixed assignments.
    mEngine = mStagingEngine;
    mParams = mStagingParams;
    mAppliedRouting = routing;
    mAudioAdapter.reset();
    mMidiParser = lxr::MidiParser();
    return true;
}

void LXR::ResetEngineState()
{
    StateAuthority::Values parameters {};
    uint64_t parameterGeneration = 0;
    lxr::RoutingStateAuthority::Snapshot routing;
    if (!TryPersistentSnapshot(parameters, parameterGeneration, routing))
        return;

    if (!StageCompleteRestore(parameters, routing.state))
        return;

    mAppliedStateGeneration = parameterGeneration;
    mAppliedRestoreGeneration = routing.restoreEpoch;
    mAppliedJointGeneration = routing.generation;

    // ORIGINAL QUIRK:
    // Internal engine timing and native PCM processing
    // remain unchanged regardless of host sample rate.
}

// ---------------------------------------------------------
// Apply UI-thread state/parameter changes at a processing
// boundary without locking or allocating on the audio thread.
// ---------------------------------------------------------

bool LXR::FlushDeferredHostParameters(uint64_t maximumOrder)
{
    bool pending = false;
    const uint64_t cutoff = mRestoreOrderCutoff.load(std::memory_order_acquire);
    for (const auto& value : mDeferredHostValues)
    {
        const uint64_t encoded = value.load(std::memory_order_acquire);
        const uint64_t order = encoded >> 9u;
        pending |= (encoded & 0x100u) != 0u &&
                   (order <= cutoff || order <= maximumOrder);
    }
    for (const auto& value : mDeferredRouteValues)
    {
        const uint64_t encoded = value.load(std::memory_order_acquire);
        const uint64_t order = encoded >> 17u;
        pending |= (encoded & UINT64_C(0x10000)) != 0u &&
                   (order <= cutoff || order <= maximumOrder);
    }
    for (const auto& value : mDeferredRouteRejections)
    {
        const uint64_t order = value.load(std::memory_order_acquire);
        pending |= order != 0u && (order <= cutoff || order <= maximumOrder);
    }
    if (!pending)
        return true;

    lxr::RoutingStateAuthority::Guard outer;
    if (!mRoutingState.tryAcquire(outer) ||
        !mRoutingState.canPublishLocked(outer))
        return false;
    StateAuthority::WriteHandle writer;
    if (!mState.tryBeginWrite(writer))
        return false;

    bool changed = false;
    bool complete = true;
    for (std::size_t i = 0; i < mDeferredHostValues.size(); ++i)
    {
        auto& mailbox = mDeferredHostValues[i];
        uint64_t encoded = mailbox.load(std::memory_order_acquire);
        bool consumed = false;
        for (unsigned attempt = 0; attempt < 4u; ++attempt)
        {
            if ((encoded & 0x100u) == 0u)
                break;
            const uint64_t order = encoded >> 9u;
            if (order > cutoff && order > maximumOrder)
                break;
            if (mailbox.compare_exchange_weak(
                    encoded, 0, std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                consumed = true;
                if (order > cutoff)
                {
                    const uint8_t value = static_cast<uint8_t>(encoded & 0x7fu);
                    mState.write(writer, i, value);
                    auto* param = GetParam(static_cast<int>(i));
                    if (param != nullptr)
                        param->Set(static_cast<double>(value));
                    changed = true;
                }
                break;
            }
        }
        if (!consumed && (encoded & 0x100u) != 0u &&
            ((encoded >> 9u) <= cutoff || (encoded >> 9u) <= maximumOrder))
            complete = false;
    }

    if (changed)
    {
        mState.commit(writer);
        mRoutingState.commitPatchLocked(outer);
    }
    else
        mState.cancelUnmodified(writer);

    // Patch publication is ordered before route slots, then route slots are
    // consumed in stable velocity0..5/LFO0..5 order.
    for (std::size_t routeSlot = 0;
         routeSlot < mDeferredRouteValues.size(); ++routeSlot)
    {
        auto& mailbox = mDeferredRouteValues[routeSlot];
        uint64_t encoded = mailbox.load(std::memory_order_acquire);
        bool consumed = false;
        for (unsigned attempt = 0; attempt < 4u; ++attempt)
        {
            if ((encoded & UINT64_C(0x10000)) == 0u)
                break;
            const uint64_t order = encoded >> 17u;
            if (order > cutoff && order > maximumOrder)
                break;
            if (mailbox.compare_exchange_weak(
                    encoded, 0, std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                consumed = true;
                lxr::PersistentRouteRecord current;
                uint64_t routeGeneration = 0;
                uint64_t restoreEpoch = 0;
                if (!mRoutingState.routeLocked(
                        outer, routeSlot, current,
                        routeGeneration, restoreEpoch))
                {
                    complete = false;
                    break;
                }
                (void)routeGeneration;
                const auto id = lxr::route_host::parameterForSlot(routeSlot);
                auto* param = GetParam(static_cast<int>(id.value()));
                if (order > cutoff)
                {
                    const auto token = lxr::route_host::RouteToken16::fromRaw(
                        static_cast<std::uint32_t>(encoded & 0xffffu));
                    const auto decoded = lxr::route_host::decode(id, token);
                    const auto status = decoded
                        ? mRoutingState.commitRouteLocked(
                              outer, decoded.record, order, restoreEpoch)
                        : lxr::PersistentRouteStatus::InvalidTarget;
                    if (status == lxr::PersistentRouteStatus::Ok ||
                        status == lxr::PersistentRouteStatus::Disabled)
                        break;
                }
                if (param != nullptr)
                    param->Set(static_cast<double>(
                        lxr::route_host::encode(current).value()));
                break;
            }
        }
        if (!consumed && (encoded & UINT64_C(0x10000)) != 0u &&
            ((encoded >> 17u) <= cutoff ||
             (encoded >> 17u) <= maximumOrder))
            complete = false;
    }

    for (std::size_t routeSlot = 0;
         routeSlot < mDeferredRouteRejections.size(); ++routeSlot)
    {
        auto& mailbox = mDeferredRouteRejections[routeSlot];
        uint64_t order = mailbox.load(std::memory_order_acquire);
        bool consumed = false;
        for (unsigned attempt = 0; attempt < 4u; ++attempt)
        {
            if (order == 0u || (order > cutoff && order > maximumOrder))
                break;
            if (mailbox.compare_exchange_weak(
                    order, 0, std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                consumed = true;
                lxr::PersistentRouteRecord current;
                uint64_t routeGeneration = 0;
                uint64_t restoreEpoch = 0;
                if (!mRoutingState.routeLocked(
                        outer, routeSlot, current,
                        routeGeneration, restoreEpoch))
                {
                    complete = false;
                    break;
                }
                (void)routeGeneration;
                const auto id = lxr::route_host::parameterForSlot(routeSlot);
                auto* param = GetParam(static_cast<int>(id.value()));
                if (param != nullptr)
                    param->Set(static_cast<double>(
                        lxr::route_host::encode(current).value()));
                if (order > cutoff)
                {
                    const auto status = mRoutingState.republishRouteLocked(
                        outer, routeSlot, order, restoreEpoch);
                    if (status != lxr::PersistentRouteStatus::Ok &&
                        status != lxr::PersistentRouteStatus::Stale)
                        complete = false;
                }
                break;
            }
        }
        if (!consumed && order != 0u &&
            (order <= cutoff || order <= maximumOrder))
            complete = false;
    }
    return complete;
}

bool LXR::ApplyPendingStateAndParameters()
{
    if (!FlushDeferredHostParameters())
        return false;

    StateAuthority::Values candidate {};
    uint64_t parameterGeneration = 0;
    lxr::RoutingStateAuthority::Snapshot routing;
    if (!TryPersistentSnapshot(candidate, parameterGeneration, routing))
        return false;

    if (routing.restoreEpoch != mAppliedRestoreGeneration)
    {
        if (!StageCompleteRestore(candidate, routing.state))
            return false;
        mAppliedRestoreGeneration = routing.restoreEpoch;
    }
    else if (routing.generation != mAppliedJointGeneration)
    {
        mStagingEngine = mEngine;
        mStagingParams = mParams;
        for (std::size_t i = 0; i < candidate.size(); ++i)
        {
            const uint8_t index = static_cast<uint8_t>(i);
            if (mStagingParams.get(index) != candidate[i])
                mStagingParams.set(index, candidate[i], mStagingEngine);
        }
        for (std::size_t i = 0; i < routing.state.routes.size(); ++i)
        {
            if (SameRoute(mAppliedRouting.routes[i], routing.state.routes[i]))
                continue;
            const auto& route = routing.state.routes[i];
            using namespace lxr::modulation_contract;
            const SourceIdentity source {
                route.source == 0u ? SourceKind::Velocity : SourceKind::Lfo,
                voiceFromZeroBased(route.owner)
            };
            const auto target = OriginalTargetIndex::fromRaw(route.originalIndex);
            const auto status = route.source == 0u
                ? mStagingEngine.configureVelocityDestination(source, target)
                : mStagingEngine.configureLfoDestination(source, target);
            if (status != lxr::VelocityRouteStatus::Ok)
                return false;
        }
        mEngine = mStagingEngine;
        mParams = mStagingParams;
        mAppliedRouting = routing.state;
    }
    mAppliedStateGeneration = parameterGeneration;
    mAppliedJointGeneration = routing.generation;
    return true;
}

// ---------------------------------------------------------
// Versioned state serialization
// ---------------------------------------------------------

bool LXR::SerializeState(iplug::IByteChunk& chunk) const
{
    const uint64_t watermark =
        mDeferredHostOrder.load(std::memory_order_acquire) - 1u;
    if (!const_cast<LXR*>(this)->FlushDeferredHostParameters(watermark))
        return false;

    StateAuthority::Values parameters {};
    uint64_t parameterGeneration = 0;
    lxr::RoutingStateAuthority::Snapshot routing;
    if (!TryPersistentSnapshot(parameters, parameterGeneration, routing))
        return false;
    (void)parameterGeneration;
    routing.state.bypass = GetBypassed();

    std::array<uint8_t, lxr::state::kMaximumStateSize> encoded {};
    std::size_t encodedSize = 0;
    if (!lxr::state::EncodeV2(parameters, routing.state, encoded,
                              encodedSize, false))
        return false;

    return chunk.PutBytes(encoded.data(), static_cast<int>(encodedSize)) > 0;
}

bool LXR::PublishPersistentRestore(
    const StateAuthority::Values& parameters,
    const lxr::PersistentRoutingState& routing)
{
    const uint64_t restoreOrder =
        mDeferredHostOrder.fetch_add(1, std::memory_order_acq_rel);
    if (restoreOrder == 0u)
        return false;

    lxr::RoutingStateAuthority::Guard outer;
    if (!mRoutingState.tryAcquire(outer) ||
        mRoutingState.validateRestoreLocked(outer, routing, restoreOrder) !=
            lxr::PersistentRouteStatus::Ok)
    {
        return false;
    }
    StateAuthority::WriteHandle writer;
    if (!mState.tryBeginWrite(writer))
        return false;
    if (!mState.writeAll(writer, parameters))
    {
        mState.cancelUnmodified(writer);
        return false;
    }

    mState.commit(writer);
    if (mRoutingState.commitRestoreLocked(outer, routing, restoreOrder) == 0u)
        return false; // Preflight above makes this unreachable without corruption.
    mRestoreOrderCutoff.store(restoreOrder, std::memory_order_release);

    // Logical publication is complete before any host-facing projection.
    for (std::size_t i = 0; i < parameters.size(); ++i)
    {
        auto* param = GetParam(static_cast<int>(i));
        if (param != nullptr)
            param->Set(static_cast<double>(parameters[i]));
    }
    for (std::size_t routeSlot = 0;
         routeSlot < routing.routes.size(); ++routeSlot)
    {
        const auto id = lxr::route_host::parameterForSlot(routeSlot);
        auto* param = GetParam(static_cast<int>(id.value()));
        if (param != nullptr)
            param->Set(static_cast<double>(
                lxr::route_host::encode(routing.routes[routeSlot]).value()));
    }
    SetBypassed(routing.bypass);
    mControllerNeedsReconcile = true;
    return true;
}

int LXR::UnserializeState(const iplug::IByteChunk& chunk, int startPos)
{
    if (startPos < 0 || startPos >= chunk.Size())
    {
        return -1;
    }

    StateAuthority::Values candidate {};
    lxr::PersistentRoutingState routing = lxr::MakeDisabledRoutingState();
    auto result = lxr::state::DecodePersistentState(
        chunk.GetData() + startPos,
        static_cast<std::size_t>(chunk.Size() - startPos),
        candidate, routing, true
    );
    if (!result)
    {
        result = lxr::state::DecodePersistentState(
            chunk.GetData() + startPos,
            static_cast<std::size_t>(chunk.Size() - startPos),
            candidate, routing, false);
        if (!result)
            return -1;
        // Core-only formats do not carry bypass state.
        routing.bypass = GetBypassed();
    }
    if (!PublishPersistentRestore(candidate, routing))
        return -1;
    return startPos + static_cast<int>(result.consumed);
}

bool LXR::ConfigureRouteForTest(
    const lxr::PersistentRouteRecord& route,
    uint64_t expectedEpoch) noexcept
{
    const uint64_t ticket =
        mDeferredHostOrder.fetch_add(1, std::memory_order_acq_rel);
    if (ticket == 0u)
        return false;
    lxr::RoutingStateAuthority::Guard outer;
    if (!mRoutingState.tryAcquire(outer))
        return false;
    const auto status = mRoutingState.commitRouteLocked(
        outer, route, ticket, expectedEpoch);
    return status == lxr::PersistentRouteStatus::Ok ||
           status == lxr::PersistentRouteStatus::Disabled;
}

void LXR::DeferRouteHostToken(
    std::size_t routeSlot, lxr::route_host::RouteToken16 token,
    uint64_t ticket) noexcept
{
    if (routeSlot >= mDeferredRouteValues.size() || !token.valid() ||
        ticket >= (UINT64_C(1) << 47u))
        return;
    auto& mailbox = mDeferredRouteValues[routeSlot];
    const uint64_t desired = (ticket << 17u) | UINT64_C(0x10000) |
                             token.value();
    uint64_t current = mailbox.load(std::memory_order_acquire);
    for (unsigned attempt = 0; attempt < 4u; ++attempt)
    {
        if ((current & UINT64_C(0x10000)) != 0u &&
            (current >> 17u) >= ticket)
            return;
        if (mailbox.compare_exchange_weak(
                current, desired, std::memory_order_release,
                std::memory_order_acquire))
            return;
    }
}

bool LXR::PublishRouteHostToken(
    std::size_t routeSlot, lxr::route_host::RouteToken16 token,
    uint64_t ticket) noexcept
{
    const auto id = lxr::route_host::parameterForSlot(routeSlot);
    const auto decoded = lxr::route_host::decode(id, token);
    if (!decoded)
        return RejectRouteHostValue(routeSlot, ticket);

    lxr::RoutingStateAuthority::Guard outer;
    if (!mRoutingState.tryAcquire(outer))
        return false;
    lxr::PersistentRouteRecord current;
    uint64_t routeGeneration = 0;
    uint64_t restoreEpoch = 0;
    if (!mRoutingState.routeLocked(outer, routeSlot, current,
                                   routeGeneration, restoreEpoch))
        return true;
    (void)routeGeneration;
    const auto status = mRoutingState.commitRouteLocked(
        outer, decoded.record, ticket, restoreEpoch);
    if (status == lxr::PersistentRouteStatus::Ok ||
        status == lxr::PersistentRouteStatus::Disabled)
        return true;

    // A restore or newer route already won. Restore the IParam mirror to the
    // accepted authority; its route generation is projected on main-thread idle.
    auto* param = GetParam(static_cast<int>(id.value()));
    if (param != nullptr)
        param->Set(static_cast<double>(lxr::route_host::encode(current).value()));
    return true;
}

bool LXR::RejectRouteHostValue(
    std::size_t routeSlot, uint64_t ticket) noexcept
{
    if (routeSlot >= lxr::route_host::kParameterCount)
        return true;
    lxr::RoutingStateAuthority::Guard outer;
    if (!mRoutingState.tryAcquire(outer))
    {
        auto& pending = mDeferredRouteRejections[routeSlot];
        uint64_t current = pending.load(std::memory_order_acquire);
        for (unsigned attempt = 0; attempt < 4u && current < ticket; ++attempt)
            if (pending.compare_exchange_weak(
                    current, ticket, std::memory_order_release,
                    std::memory_order_acquire))
                break;
        return false;
    }
    lxr::PersistentRouteRecord current;
    uint64_t routeGeneration = 0;
    uint64_t restoreEpoch = 0;
    if (!mRoutingState.routeLocked(outer, routeSlot, current,
                                   routeGeneration, restoreEpoch))
        return true;
    (void)routeGeneration;
    const auto id = lxr::route_host::parameterForSlot(routeSlot);
    auto* param = GetParam(static_cast<int>(id.value()));
    if (param != nullptr)
        param->Set(static_cast<double>(lxr::route_host::encode(current).value()));
    const auto status = mRoutingState.republishRouteLocked(
        outer, routeSlot, ticket, restoreEpoch);
    return status == lxr::PersistentRouteStatus::Ok ||
           status == lxr::PersistentRouteStatus::Stale;
}

// ---------------------------------------------------------
// Host reset callback
// ---------------------------------------------------------

void LXR::OnReset()
{
    // Reset native synthesis and processing state.
    ResetEngineState();

    // P12b-4c:
    // Configure the FIR using the DAW's actual rate.
    //
    // Coefficients are generated here, not inside
    // ProcessBlock().
    //
    // The adapter rejects unsupported sample rates,
    // and ProcessBlock() returns silence if not ready.
    //
    // IMPORTANT:
    // This must not execute concurrently with audio
    // processing because it rewrites the coefficient bank.
    // iPlug2 also calls OnReset from setProcessing(false), potentially on the
    // processing thread. Retain the bank at an unchanged rate. VST3 rate changes
    // arrive via setupProcessing while inactive, before process callbacks.
    const double hostRate = GetSampleRate();
    if (!mAudioAdapter.ready() || mAudioAdapter.hostRate() != hostRate)
    {
        mAudioAdapter.configure(hostRate);
    }

    // Failed configuration invalidates readiness and reports zero latency.
    // SetLatency stores the value and iPlug2 notifies an attached VST3 handler
    // with restartComponent(kLatencyChanged). Avoid redundant restart requests
    // on ordinary transport resets. This adds no physical delay to the audio.
    const int latency = mAudioAdapter.ready()
        ? lxr::firLatency::hostSamples(hostRate) : 0;
    if (GetLatency() != latency)
    {
        SetLatency(latency);
    }
}

// ---------------------------------------------------------
// Host activation/deactivation callback
//
// Required by LXR.h.
//
// Restores the missing function that caused LNK2001.
// ---------------------------------------------------------

void LXR::OnActivate(
    bool active
)
{
    // The original LXR Engine has no distinct
    // activation/deactivation API.
    //
    // Clear all cached audio when deactivated.
    //
    // This avoids replaying stale samples after a
    // transport or plugin lifecycle transition.
    //
    // The configured FIR coefficients are retained.
    if (!active)
    {
        mAudioAdapter.reset();
    }
}

// ---------------------------------------------------------
// Host parameter change
// ---------------------------------------------------------

void LXR::OnParamChange(
    int paramIdx
)
{
    const auto routeId = lxr::route_host::RouteHostParamId::fromRaw(
        paramIdx >= 0 ? static_cast<std::uint32_t>(paramIdx) : 0u);
    if (routeId.valid())
    {
        const auto routeSlot = lxr::route_host::slot(routeId);
        auto* param = GetParam(paramIdx);
        const uint64_t ticket =
            mDeferredHostOrder.fetch_add(1, std::memory_order_acq_rel);
        if (param == nullptr || ticket == 0u)
            return;
        const auto token = lxr::route_host::RouteToken16::fromRaw(
            static_cast<std::uint32_t>(param->Int()));
        const auto decoded = lxr::route_host::decode(routeId, token);
        if (!decoded)
        {
            RejectRouteHostValue(routeSlot, ticket);
            return;
        }
        if (!PublishRouteHostToken(routeSlot, token, ticket))
            DeferRouteHostToken(routeSlot, token, ticket);
        return;
    }

    // Only IDs 0..227 belong to the native LXR.
    if (paramIdx < 0 ||
        paramIdx >= lxr::vst::kNativeNumParams)
    {
        return;
    }

    auto* param =
        GetParam(paramIdx);

    if (param == nullptr)
    {
        return;
    }

    const uint8_t value = GetRawLXRParameterValue(param);
    lxr::RoutingStateAuthority::Guard outer;
    StateAuthority::WriteHandle writer;
    if (!mRoutingState.tryAcquire(outer) ||
        !mRoutingState.canPublishLocked(outer) ||
        !mState.tryBeginWrite(writer))
    {
        // A restore or another host producer already owns the bounded writer
        // gate. The latest callback value is accepted at the next processing
        // boundary; bit 8 distinguishes value zero from an empty mailbox.
        const uint64_t order =
            mDeferredHostOrder.fetch_add(1, std::memory_order_relaxed);
        auto& mailbox =
            mDeferredHostValues[static_cast<std::size_t>(paramIdx)];
        const uint64_t desired = (order << 9u) | 0x100u | value;
        uint64_t current = mailbox.load(std::memory_order_acquire);
        while ((current & 0x100u) == 0u || (current >> 9u) < order)
        {
            if (mailbox.compare_exchange_weak(
                    current, desired, std::memory_order_release,
                    std::memory_order_acquire))
                break;
        }
        return;
    }

    const std::size_t index = static_cast<std::size_t>(paramIdx);
    if (mState.value(index) == value)
    {
        mState.cancelUnmodified(writer);
        return;
    }
    mState.write(writer, index, value);
    mState.commit(writer);
    mRoutingState.commitPatchLocked(outer);
}

void LXR::OnParamChangeUI(int paramIdx, iplug::EParamSource source)
{
    (void)source;
    // In the non-distributed VST3 adapter, controller/UI changes update IParam
    // and call only OnParamChangeUI(). Route that producer through the same
    // authority as processing-thread automation.
    OnParamChange(paramIdx);
}

// ---------------------------------------------------------
// Main-thread controller projection and host cache invalidation
// ---------------------------------------------------------

#if defined(VST3_API)
Steinberg::tresult PLUGIN_API LXR::setParamNormalized(
    Steinberg::Vst::ParamID tag,
    Steinberg::Vst::ParamValue value)
{
    const auto id = lxr::route_host::RouteHostParamId::fromRaw(tag);
    if (!id.valid())
        return iplug::Plugin::setParamNormalized(tag, value);

    lxr::route_host::RouteToken16 token;
    if (!DecodeNormalizedRouteToken(*this, tag, value, token))
    {
        const uint64_t ticket =
            mDeferredHostOrder.fetch_add(1, std::memory_order_acq_rel);
        if (ticket != 0u)
            RejectRouteHostValue(lxr::route_host::slot(id), ticket);
        return Steinberg::kResultFalse;
    }
    return iplug::Plugin::setParamNormalized(tag, value);
}

Steinberg::tresult PLUGIN_API LXR::getState(Steinberg::IBStream* state)
{
    if (state == nullptr)
        return Steinberg::kInvalidArgument;
    const uint64_t watermark =
        mDeferredHostOrder.load(std::memory_order_acquire) - 1u;
    if (!FlushDeferredHostParameters(watermark))
        return Steinberg::kResultFalse;

    StateAuthority::Values parameters {};
    uint64_t parameterGeneration = 0;
    lxr::RoutingStateAuthority::Snapshot routing;
    if (!TryPersistentSnapshot(parameters, parameterGeneration, routing))
        return Steinberg::kResultFalse;
    (void)parameterGeneration;
    routing.state.bypass = GetBypassed();

    std::array<uint8_t, lxr::state::kMaximumStateSize> encoded {};
    std::size_t encodedSize = 0;
    if (!lxr::state::EncodeV2(parameters, routing.state, encoded,
                              encodedSize, true))
        return Steinberg::kResultFalse;
    Steinberg::int32 written = 0;
    const auto result = state->write(encoded.data(),
        static_cast<Steinberg::int32>(encodedSize), &written);
    return (result == Steinberg::kResultOk || result == Steinberg::kResultTrue) &&
           written == static_cast<Steinberg::int32>(encodedSize)
        ? Steinberg::kResultOk : Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API LXR::setState(Steinberg::IBStream* state)
{
    if (state == nullptr)
        return Steinberg::kInvalidArgument;

    // Read at most one byte beyond the contract ceiling so oversize streams
    // are rejected without an unbounded allocation.
    std::array<uint8_t, lxr::state::kMaximumStateSize + 1u> bytes {};
    std::size_t size = 0;
    for (;;)
    {
        const std::size_t remaining = bytes.size() - size;
        if (remaining == 0u)
            return Steinberg::kResultFalse;
        const Steinberg::int32 request = static_cast<Steinberg::int32>(
            remaining > 128u ? 128u : remaining);
        Steinberg::int32 bytesRead = 0;
        const auto readResult = state->read(bytes.data() + size, request,
                                            &bytesRead);
        if (bytesRead < 0 || bytesRead > request)
            return Steinberg::kResultFalse;
        if (bytesRead == 0)
            break;
        size += static_cast<std::size_t>(bytesRead);
        if (size > lxr::state::kMaximumStateSize)
            return Steinberg::kResultFalse;
        if (readResult != Steinberg::kResultOk &&
            readResult != Steinberg::kResultTrue && GetHost() != iplug::kHostWaveLab)
            return Steinberg::kResultFalse;
    }

    StateAuthority::Values parameters {};
    auto routing = lxr::MakeDisabledRoutingState();
    const auto decoded = lxr::state::DecodePersistentState(
        bytes.data(), size, parameters, routing, true);
    if (!decoded || !PublishPersistentRestore(parameters, routing))
        return Steinberg::kResultFalse;

    // Update the framework's separate controller cache only after the complete
    // patch+routing+bypass transaction has committed.
    UpdateParams(this, routing.bypass ? 1 : 0);
    OnRestoreState();
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API LXR::process(Steinberg::Vst::ProcessData& data)
{
    if (data.inputParameterChanges == nullptr)
        return iplug::Plugin::process(data);
    // The pinned dispatcher reads only the last point. Preflight that exact
    // point so rejected route automation republishes canonical readback while
    // the adapter prevents the invalid value from reaching IParam.
    const auto queueCount = data.inputParameterChanges->getParameterCount();
    for (Steinberg::int32 i = 0; i < queueCount; ++i)
    {
        auto* queue = data.inputParameterChanges->getParameterData(i);
        if (queue == nullptr)
            continue;
        const auto id = lxr::route_host::RouteHostParamId::fromRaw(
            queue->getParameterId());
        const auto pointCount = queue->getPointCount();
        if (!id.valid() || pointCount <= 0)
            continue;
        Steinberg::int32 offset = 0;
        Steinberg::Vst::ParamValue value = 0.0;
        lxr::route_host::RouteToken16 token;
        if (queue->getPoint(pointCount - 1, offset, value) !=
                Steinberg::kResultOk ||
            !DecodeNormalizedRouteToken(*this, id.value(), value, token))
        {
            const uint64_t ticket =
                mDeferredHostOrder.fetch_add(1, std::memory_order_acq_rel);
            if (ticket != 0u)
                RejectRouteHostValue(lxr::route_host::slot(id), ticket);
        }
    }
    MidiParameterChanges changes(*this, *data.inputParameterChanges);
    auto adapted = data;
    adapted.inputParameterChanges = &changes;
    return iplug::Plugin::process(adapted);
}

Steinberg::tresult PLUGIN_API LXR::setComponentHandler(
    Steinberg::Vst::IComponentHandler* handler)
{
    const auto result = iplug::Plugin::setComponentHandler(handler);
    if (result == Steinberg::kResultOk)
    {
        ++mControllerConnection;
        mControllerNeedsReconcile = true;
    }
    return result;
}
#endif

void LXR::OnIdle()
{
#if defined(VST3_API)
    // The installed adapter is SingleComponentEffect: processor and controller
    // share this object, but not their parameter storage. Only the main thread
    // touches the VST3 ParameterContainer or calls the component handler.
    if (mControllerSyncBusy)
        return;

    lxr::RoutingStateAuthority::Guard outer;
    StateAuthority::WriteHandle projection;
    if (!mRoutingState.tryAcquire(outer) ||
        !mState.tryBeginWrite(projection))
        return; // Retain every unacknowledged generation for the next idle.

    mControllerSyncBusy = true;
    auto delivered = mControllerSyncGenerations;
    lxr::RoutingStateAuthority::Snapshot routeSnapshot;
    if (!mRoutingState.copyLocked(outer, routeSnapshot))
    {
        mState.cancelUnmodified(projection);
        mControllerSyncBusy = false;
        return;
    }
    bool pending = mControllerNeedsReconcile;
    bool complete = true;
    for (std::size_t i = 0; i < delivered.size(); ++i)
    {
        const bool native = i < lxr::state::kParameterCount;
        const std::size_t routeSlot = native
            ? 0u : i - lxr::state::kParameterCount;
        const uint64_t generation = native
            ? mState.valueGeneration(i)
            : routeSnapshot.routeGenerations[routeSlot];
        if (!mControllerNeedsReconcile && delivered[i] == generation)
            continue;

        const auto* param = GetParam(static_cast<int>(i));
        // Hold the existing bounded writer gate only during projection. A newer
        // producer cannot slip between generation validation and cache update.
        // Do not write IParam or call OnParamChange here (no feedback producer).
        complete &= param != nullptr && SetVST3ParamNormalized(
            static_cast<Steinberg::Vst::ParamID>(i),
            param->ToNormalized(static_cast<double>(native
                ? mState.value(i)
                : lxr::route_host::encode(
                    routeSnapshot.state.routes[routeSlot]).value())));
        delivered[i] = generation;
        pending = true;
    }
    mState.cancelUnmodified(projection);
    mRoutingState.release(outer);

    // Release the gate before any host callback: it may synchronously read the
    // cache, restore state, or deliver automation. Newer generations then remain
    // different from delivered[] and are sent on the next idle. No gestures:
    // MIDI must not create a second DAW automation recording of the same edit.
    const uint64_t connection = mControllerConnection;
    Steinberg::IPtr<Steinberg::Vst::IComponentHandler> handler = GetComponentHandler();
    if (pending && complete && handler &&
        handler->restartComponent(Steinberg::Vst::kParamValuesChanged) ==
            Steinberg::kResultOk && connection == mControllerConnection)
    {
        mControllerSyncGenerations = delivered;
        mControllerNeedsReconcile = false;
    }
    mControllerSyncBusy = false;
#endif
}

// ---------------------------------------------------------
// MIDI input
// ---------------------------------------------------------

bool LXR::EnqueueDeferredMidi(const iplug::IMidiMsg& msg) noexcept
{
    if (!mDeferredMidi.push(msg))
    {
        mDeferredMidiOverflows.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

void LXR::DrainDeferredMidi()
{
    while (auto* msg = mDeferredMidi.front())
    {
        if (!ProcessMidiMsgNow(*msg))
            return;
        mDeferredMidi.pop();
    }
}

void LXR::ProcessMidiMsg(const iplug::IMidiMsg& msg)
{
    if (!ApplyPendingStateAndParameters())
    {
        EnqueueDeferredMidi(msg);
        return;
    }

    DrainDeferredMidi();
    if (!mDeferredMidi.empty() || !ProcessMidiMsgNow(msg))
        EnqueueDeferredMidi(msg);
}

bool LXR::ProcessMidiMsgNow(const iplug::IMidiMsg& msg)
{

    // Never trigger synthesis without valid assets.
    if (!mAssetsLoaded)
    {
        return true;
    }

    const auto statusMsg =
        msg.StatusMsg();

    if (statusMsg == iplug::IMidiMsg::kNone)
    {
        return true;
    }

    // iPlug uses zero-based MIDI channels.
    const int channel =
        msg.Channel();

    if (channel < 0 ||
        channel > 15)
    {
        return true;
    }

    // Translate iPlug's message type to the native
    // MIDI status representation.
    const uint8_t status =
        static_cast<uint8_t>(
            static_cast<uint8_t>(statusMsg) << 4
        );

    const uint8_t data1 =
        static_cast<uint8_t>(
            msg.mData1 & 0x7F
        );

    const uint8_t data2 =
        static_cast<uint8_t>(
            msg.mData2 & 0x7F
        );

    lxr::MidiEvent event {};

    // Preserve the MIDI message type and channel.
    event.status = static_cast<uint8_t>(
        status | static_cast<uint8_t>(channel)
    );

    event.channel =
        static_cast<uint8_t>(channel);

    event.data1 = data1;
    event.data2 = data2;

    const bool mayChangeParameters = (event.status & 0xF0u) == 0xB0u;
    lxr::RoutingStateAuthority::Guard outer;
    StateAuthority::WriteHandle writer;
    if (mayChangeParameters &&
        (!mRoutingState.tryAcquire(outer) ||
         !mRoutingState.canPublishLocked(outer) ||
         !mState.tryBeginWrite(writer)))
        return false;

    std::array<uint8_t, lxr::state::kParameterCount> valuesBefore {};
    if (mayChangeParameters)
    {
        for (std::size_t i = 1; i < valuesBefore.size(); ++i)
        {
            valuesBefore[i] = mParams.get(static_cast<uint8_t>(i));
        }
    }

    // Preserve original native MIDI dispatch.
    mMidiParser.processEvent(
        event,
        mEngine,
        mParams
    );

    if (mayChangeParameters)
    {
        for (std::size_t i = 1; i < valuesBefore.size(); ++i)
        {
            const uint8_t value = mParams.get(static_cast<uint8_t>(i));
            if (value == valuesBefore[i])
                continue;

            mState.write(writer, i, value);

            auto* param = GetParam(static_cast<int>(i));
            if (param != nullptr)
            {
                param->Set(static_cast<double>(value));
            }
        }

        const uint64_t generation = mState.commit(writer);
        const uint64_t jointGeneration =
            mRoutingState.commitPatchLocked(outer);
        mAppliedStateGeneration = generation;
        mAppliedJointGeneration = jointGeneration;
    }
    return true;
}

// ---------------------------------------------------------
// Audio processing callback
//
// P12b-4c:
// Convert native LXR audio into host-rate output.
//
// The adapter owns:
//   - native block buffering
//   - PCM-to-double conversion
//   - polyphase FIR resampling
//   - fractional source position
//   - filter history
//
// This callback performs no allocations or file I/O.
// ---------------------------------------------------------

void LXR::ProcessBlock(
    double** inputs,
    double** outputs,
    int nFrames
)
{
    // LXR currently operates as an instrument.
    (void)inputs;

    if (ApplyPendingStateAndParameters())
        DrainDeferredMidi();

    if (outputs == nullptr ||
        nFrames <= 0)
    {
        return;
    }

    const int nChans =
        NOutChansConnected();

    if (nChans <= 0)
    {
        return;
    }

    // Protect the native DSP from missing assets.
    if (!mAssetsLoaded)
    {
        ClearAudioOutputs(
            outputs,
            nChans,
            nFrames
        );

        return;
    }

    // Never process with an unconfigured FIR.
    if (!mAudioAdapter.ready())
    {
        ClearAudioOutputs(
            outputs,
            nChans,
            nFrames
        );

        return;
    }

    // Generate exactly the number of host frames
    // requested by the current DAW callback.
    //
    // The fractional source position and FIR history
    // persist across arbitrary callback boundaries.
    for (int sample = 0;
         sample < nFrames;
         ++sample)
    {
        double left = 0.0;
        double right = 0.0;

        // Render one host-rate stereo sample.
        mAudioAdapter.getNextHostST1Sample(
            mEngine,
            left,
            right
        );

        // Mono host configuration.
        if (nChans == 1)
        {
            if (outputs[0] != nullptr)
            {
                outputs[0][sample] =
                    0.5 * (left + right);
            }

            continue;
        }

        // Main stereo output: ST1.
        if (outputs[0] != nullptr)
        {
            outputs[0][sample] = left;
        }

        if (outputs[1] != nullptr)
        {
            outputs[1][sample] = right;
        }

        // ST2 remains internal for this phase.
        // Keep all additional host channels silent.
        for (int channel = 2;
             channel < nChans;
             ++channel)
        {
            if (outputs[channel] != nullptr)
            {
                outputs[channel][sample] = 0.0;
            }
        }
    }
}
