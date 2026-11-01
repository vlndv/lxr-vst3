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

class MidiParameterChanges final : public Steinberg::Vst::IParameterChanges
{
public:
    explicit MidiParameterChanges(Steinberg::Vst::IParameterChanges& changes) : source(changes) {}
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
    Steinberg::Vst::IParameterChanges& source;
    MidiValueQueue midiQueue;
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
        lxr::vst::kNumParams == lxr::kNumParams,
        "VST and DSP parameter counts must match"
    );

    static_assert(
        lxr::vst::kNumParams == 228,
        "Unexpected LXR parameter count"
    );

    // Register each native LXR parameter.
    for (int paramIdx = 0;
         paramIdx < lxr::vst::kNumParams;
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

    // Initialize canonical native parameters.
    mParams.init();

    StateAuthority::Values initialState {};
    for (int paramIdx = 0; paramIdx < lxr::vst::kNumParams; ++paramIdx)
    {
        const std::size_t index = static_cast<std::size_t>(paramIdx);
        initialState[index] = mParams.get(static_cast<uint8_t>(paramIdx));
        mDeferredHostValues[index].store(0, std::memory_order_relaxed);
    }
    mState.initialize(initialState);

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

void LXR::ResetEngineState()
{
    StateAuthority::Values parameters {};
    uint64_t generation = 0;
    if (!mState.trySnapshot(parameters, &generation, 64))
    {
        // Host callback thread context is not guaranteed here. A concurrent
        // writer leaves the previous complete runtime state intact; the next
        // processing boundary retries without blocking.
        return;
    }

    // Reset DSP voices, modulation and mixer.
    mEngine.init();

    // Discard cached audio and FIR history.
    // Preserve the configured host sample rate.
    mAudioAdapter.reset();

    // Discard any incomplete MIDI controller sequence.
    mMidiParser = lxr::MidiParser();

    // Initialize the native parameter array.
    mParams.init();

    // Reapply one coherent authoritative generation.
    for (int paramIdx = 0; paramIdx < lxr::vst::kNumParams; ++paramIdx)
    {
        mParams.set(
            static_cast<uint8_t>(paramIdx),
            parameters[static_cast<std::size_t>(paramIdx)],
            mEngine
        );
    }
    mAppliedStateGeneration = generation;
    const uint64_t restoreGeneration =
        mPendingRestoreGeneration.load(std::memory_order_acquire);
    if (restoreGeneration <= generation)
        mAppliedRestoreGeneration = restoreGeneration;

    // ORIGINAL QUIRK:
    // Internal engine timing and native PCM processing
    // remain unchanged regardless of host sample rate.
}

// ---------------------------------------------------------
// Apply UI-thread state/parameter changes at a processing
// boundary without locking or allocating on the audio thread.
// ---------------------------------------------------------

bool LXR::FlushDeferredHostParameters()
{
    bool pending = false;
    for (const auto& value : mDeferredHostValues)
        pending |= value.load(std::memory_order_acquire) != 0;
    if (!pending)
        return true;

    StateAuthority::WriteHandle writer;
    if (!mState.tryBeginWrite(writer))
        return false;

    bool changed = false;
    for (std::size_t i = 0; i < mDeferredHostValues.size(); ++i)
    {
        const uint64_t encoded =
            mDeferredHostValues[i].exchange(0, std::memory_order_acq_rel);
        if ((encoded & 0x100u) == 0u)
            continue;
        const uint8_t value = static_cast<uint8_t>(encoded & 0x7fu);
        mState.write(writer, i, value);
        auto* param = GetParam(static_cast<int>(i));
        if (param != nullptr)
            param->Set(static_cast<double>(value));
        changed = true;
    }

    if (changed)
        mState.commit(writer);
    else
        mState.cancelUnmodified(writer);
    return true;
}

bool LXR::ApplyPendingStateAndParameters()
{
    if (!FlushDeferredHostParameters())
        return false;

    StateAuthority::Values candidate {};
    uint64_t generation = 0;
    if (!mState.trySnapshot(candidate, &generation))
        return false;

    const uint64_t restoreGeneration =
        mPendingRestoreGeneration.load(std::memory_order_acquire);
    if (restoreGeneration > generation)
        return false;

    if (restoreGeneration != mAppliedRestoreGeneration)
    {
        // A successful state recall reconstructs runtime-only state from the
        // validated parameter candidate. Assets and FIR coefficients remain
        // resident; only voice/cache/history state is cleared.
        mEngine.init();
        mAudioAdapter.reset();
        mMidiParser = lxr::MidiParser();
        mParams.init();
        for (std::size_t i = 0; i < candidate.size(); ++i)
            mParams.set(static_cast<uint8_t>(i), candidate[i], mEngine);
        mAppliedRestoreGeneration = restoreGeneration;
    }
    else if (generation != mAppliedStateGeneration)
    {
        // Individually ordered automation generations do not reset voices.
        for (std::size_t i = 0; i < candidate.size(); ++i)
        {
            const uint8_t index = static_cast<uint8_t>(i);
            if (mParams.get(index) != candidate[i])
                mParams.set(index, candidate[i], mEngine);
        }
    }
    mAppliedStateGeneration = generation;
    return true;
}

// ---------------------------------------------------------
// Versioned state serialization
// ---------------------------------------------------------

bool LXR::SerializeState(iplug::IByteChunk& chunk) const
{
    StateAuthority::Values parameters {};
    if (!mState.trySnapshot(parameters, nullptr, 32))
        return false;

    std::array<uint8_t, lxr::state::kEncodedSize> encoded {};
    if (!lxr::state::Encode(parameters, encoded))
    {
        return false;
    }

    return chunk.PutBytes(encoded.data(), static_cast<int>(encoded.size())) > 0;
}

int LXR::UnserializeState(const iplug::IByteChunk& chunk, int startPos)
{
    if (startPos < 0 || startPos >= chunk.Size())
    {
        return -1;
    }

    std::array<uint8_t, lxr::state::kParameterCount> candidate {};
    const auto result = lxr::state::Decode(
        chunk.GetData() + startPos,
        static_cast<std::size_t>(chunk.Size() - startPos),
        candidate,
        true // iPlug2 VST3 appends a four-byte bypass value to plugin state.
    );
    if (!result)
    {
        return -1;
    }

    StateAuthority::WriteHandle writer;
    if (!mState.tryBeginWrite(writer))
        return -1;

    // Parameter callbacks that linearized before this restore are superseded
    // by the complete patch. Tickets claimed after this boundary remain in
    // their mailboxes and are ordered after the restore.
    const uint64_t restoreOrder =
        mDeferredHostOrder.fetch_add(1, std::memory_order_acq_rel);
    for (auto& deferred : mDeferredHostValues)
    {
        uint64_t encoded = deferred.load(std::memory_order_acquire);
        while ((encoded & 0x100u) != 0u && (encoded >> 9u) < restoreOrder &&
               !deferred.compare_exchange_weak(
                   encoded, 0, std::memory_order_acq_rel,
                   std::memory_order_acquire))
        {
        }
    }

    if (!mState.writeAll(writer, candidate))
    {
        mState.cancelUnmodified(writer);
        return -1;
    }

    for (std::size_t i = 0; i < candidate.size(); ++i)
    {
        auto* param = GetParam(static_cast<int>(i));
        if (param != nullptr)
        {
            param->Set(static_cast<double>(candidate[i]));
        }
    }

    // Publish the reset marker before the even state generation. An acquiring
    // snapshot can therefore never treat a restore payload as ordinary
    // automation and expose it to the Engine without the required reset.
    mPendingRestoreGeneration.store(
        writer.generation(), std::memory_order_release);
    mState.commit(writer);
    return startPos + static_cast<int>(result.consumed);
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
    // Only IDs 0..227 belong to the native LXR.
    if (paramIdx < 0 ||
        paramIdx >= lxr::vst::kNumParams)
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
    StateAuthority::WriteHandle writer;
    if (!mState.tryBeginWrite(writer))
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
Steinberg::tresult PLUGIN_API LXR::process(Steinberg::Vst::ProcessData& data)
{
    if (data.inputParameterChanges == nullptr)
        return iplug::Plugin::process(data);
    MidiParameterChanges changes(*data.inputParameterChanges);
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

    StateAuthority::WriteHandle projection;
    if (!mState.tryBeginWrite(projection))
        return; // Retain every unacknowledged generation for the next idle.

    mControllerSyncBusy = true;
    auto delivered = mControllerSyncGenerations;
    bool pending = mControllerNeedsReconcile;
    bool complete = true;
    for (std::size_t i = 0; i < delivered.size(); ++i)
    {
        const uint64_t generation = mState.valueGeneration(i);
        if (!mControllerNeedsReconcile && delivered[i] == generation)
            continue;

        const auto* param = GetParam(static_cast<int>(i));
        // Hold the existing bounded writer gate only during projection. A newer
        // producer cannot slip between generation validation and cache update.
        // Do not write IParam or call OnParamChange here (no feedback producer).
        complete &= param != nullptr && SetVST3ParamNormalized(
            static_cast<Steinberg::Vst::ParamID>(i),
            param->ToNormalized(static_cast<double>(mState.value(i))));
        delivered[i] = generation;
        pending = true;
    }
    mState.cancelUnmodified(projection);

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
    StateAuthority::WriteHandle writer;
    if (mayChangeParameters && !mState.tryBeginWrite(writer))
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
        mAppliedStateGeneration = generation;
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
