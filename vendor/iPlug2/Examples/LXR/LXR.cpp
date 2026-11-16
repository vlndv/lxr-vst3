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

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#if IPLUG_EDITOR
#include "IControls.h"
#endif
#include <string>
#if defined(VST3_API)
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"
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
    Steinberg::int32 pointIndex = 0;
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
    Steinberg::int32 PLUGIN_API getPointCount() override { return 1; }
    Steinberg::tresult PLUGIN_API getPoint(Steinberg::int32 index, Steinberg::int32& offset,
                                          Steinberg::Vst::ParamValue& value) override
    {
        if (index != 0)
            return Steinberg::kResultFalse;
        const auto result = source->getPoint(pointIndex, offset, value);
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

class TransportValueQueue final : public Steinberg::Vst::IParamValueQueue
{
public:
    LXR* plug = nullptr;
    Steinberg::Vst::IParamValueQueue* source = nullptr;
    Steinberg::tresult PLUGIN_API queryInterface(
        const Steinberg::TUID iid, void** object) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual(
                iid, Steinberg::Vst::IParamValueQueue::iid) ||
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
    Steinberg::Vst::ParamID PLUGIN_API getParameterId() override
    {
        return source->getParameterId();
    }
    Steinberg::int32 PLUGIN_API getPointCount() override
    {
        return source->getPointCount();
    }
    Steinberg::tresult PLUGIN_API getPoint(
        Steinberg::int32 index, Steinberg::int32& offset,
        Steinberg::Vst::ParamValue& value) override
    {
        const auto result = source->getPoint(index, offset, value);
        if (result != Steinberg::kResultOk || plug == nullptr ||
            !std::isfinite(value) || value < 0.0 || value > 1.0)
            return Steinberg::kResultFalse;
        const auto* param = plug->GetParam(static_cast<int>(getParameterId()));
        if (param == nullptr)
            return Steinberg::kResultFalse;
        value = param->ToNormalized(param->FromNormalized(value));
        return Steinberg::kResultOk;
    }
    Steinberg::tresult PLUGIN_API addPoint(
        Steinberg::int32, Steinberg::Vst::ParamValue,
        Steinberg::int32&) override
    {
        return Steinberg::kNotImplemented;
    }
};

class MidiParameterChanges final : public Steinberg::Vst::IParameterChanges
{
public:
    // One queue per registered native/proxy/bypass identity is sufficient for
    // valid host input. Counts/points from the API are otherwise unbounded.
    static constexpr Steinberg::int32 kMaxSources = lxr::vst::kNumParams +
        VST3_NUM_CC_CHANS * Steinberg::Vst::kCountCtrlNumber + 1;
    static constexpr Steinberg::int32 kMaxCCPoints = 4096;

    MidiParameterChanges(LXR& owner, Steinberg::Vst::IParameterChanges* changes,
        std::atomic<uint64_t>& rejectedQueues,
        std::atomic<uint64_t>& rejectedRecords,
        std::atomic<uint64_t>& malformedCounts)
        : plug(owner)
    {
        if (changes == nullptr)
            return;
        const auto reported = changes->getParameterCount();
        if (reported < 0)
        {
            malformedCounts.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const auto count = std::min(reported, kMaxSources);
        if (reported > count)
            rejectedQueues.fetch_add(static_cast<uint64_t>(reported - count), std::memory_order_relaxed);
        Steinberg::int32 remainingCC = kMaxCCPoints;
        for (Steinberg::int32 i = 0; i < count; ++i)
        {
            auto* queue = changes->getParameterData(i);
            if (queue == nullptr)
                continue;
            const auto tag = queue->getParameterId();
            // Do not turn an unadvertised proxy ID into another MIDI channel.
            if (tag >= iplug::kMIDICCParamStartIdx &&
                tag >= iplug::kMIDICCParamStartIdx + VST3_NUM_CC_CHANS * Steinberg::Vst::kCountCtrlNumber)
            {
                malformedCounts.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            const auto points = queue->getPointCount();
            if (points < 0)
            {
                malformedCounts.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            if (points == 0)
                continue;
            const bool cc = isCC(tag);
            const auto accepted = cc ? std::min(points, remainingCC) : 1;
            if (cc)
            {
                remainingCC -= accepted;
                if (points > accepted)
                    rejectedRecords.fetch_add(static_cast<uint64_t>(points - accepted), std::memory_order_relaxed);
            }
            if (accepted != 0)
            {
                sources[sourceCount++] = {queue, flattenedCount, accepted};
                flattenedCount += accepted;
            }
        }
    }
    Steinberg::int32 sourceQueueCount() const noexcept { return sourceCount; }
    Steinberg::Vst::IParamValueQueue* sourceQueue(Steinberg::int32 index) const noexcept
    { return sources[index].queue; }
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
    Steinberg::int32 PLUGIN_API getParameterCount() override { return flattenedCount; }
    Steinberg::Vst::IParamValueQueue* PLUGIN_API getParameterData(Steinberg::int32 index) override
    {
        if (index < 0 || index >= flattenedCount)
            return nullptr;
        // Callback-local sorted table; at most9 binary-search comparisons.
        Steinberg::int32 lo = 0, hi = sourceCount;
        while (lo < hi)
        {
            const auto mid = lo + (hi - lo) / 2;
            if (sources[mid].first <= index) lo = mid + 1;
            else hi = mid;
        }
        const auto& entry = sources[lo - 1];
        auto* queue = entry.queue;
        const auto tag = queue->getParameterId();
        if (lxr::route_host::RouteHostParamId::fromRaw(tag).valid())
        {
            routeQueue.plug = &plug;
            routeQueue.source = queue;
            return &routeQueue;
        }
        if (tag >= static_cast<Steinberg::Vst::ParamID>(
                lxr::vst::kSequencerEnableParam) &&
            tag <= static_cast<Steinberg::Vst::ParamID>(
                lxr::vst::kSequencerRollTracksParam))
        {
            transportQueue.plug = &plug;
            transportQueue.source = queue;
            return &transportQueue;
        }
        if (isCC(tag))
        {
            midiQueue.source = queue;
            midiQueue.pointIndex = index - entry.first;
            return &midiQueue;
        }
        return queue;
    }
    Steinberg::Vst::IParamValueQueue* PLUGIN_API addParameterData(const Steinberg::Vst::ParamID&,
                                                                Steinberg::int32&) override { return nullptr; }
private:
    static bool isCC(Steinberg::Vst::ParamID tag) noexcept
    {
        return tag >= iplug::kMIDICCParamStartIdx &&
            tag < iplug::kMIDICCParamStartIdx + VST3_NUM_CC_CHANS * Steinberg::Vst::kCountCtrlNumber &&
            (tag - iplug::kMIDICCParamStartIdx) % Steinberg::Vst::kCountCtrlNumber < 128;
    }
    struct SourceQueue
    {
        Steinberg::Vst::IParamValueQueue* queue = nullptr;
        Steinberg::int32 first = 0;
        Steinberg::int32 count = 0;
    };
    LXR& plug;
    std::array<SourceQueue, kMaxSources> sources {};
    Steinberg::int32 sourceCount = 0;
    Steinberg::int32 flattenedCount = 0;
    MidiValueQueue midiQueue;
    RouteValueQueue routeQueue;
    TransportValueQueue transportQueue;
};

class BoundedMidiEvents final : public Steinberg::Vst::IEventList
{
public:
    BoundedMidiEvents(Steinberg::Vst::IEventList* events,
        std::atomic<uint64_t>& rejected, std::atomic<uint64_t>& malformed)
        : source(events)
    {
        if (source == nullptr) return;
        const auto reported = source->getEventCount();
        if (reported < 0) malformed.fetch_add(1, std::memory_order_relaxed);
        else
        {
            count = std::min(reported, MidiParameterChanges::kMaxCCPoints);
            if (reported > count)
                rejected.fetch_add(static_cast<uint64_t>(reported - count), std::memory_order_relaxed);
        }
    }
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** object) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::Vst::IEventList::iid) ||
            Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid))
        { *object = static_cast<Steinberg::Vst::IEventList*>(this); return Steinberg::kResultOk; }
        *object = nullptr; return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
    Steinberg::int32 PLUGIN_API getEventCount() override { return count; }
    Steinberg::tresult PLUGIN_API getEvent(Steinberg::int32 index, Steinberg::Vst::Event& event) override
    {
        return source != nullptr && index >= 0 && index < count
            ? source->getEvent(index, event) : Steinberg::kResultFalse;
    }
    Steinberg::tresult PLUGIN_API addEvent(Steinberg::Vst::Event&) override { return Steinberg::kNotImplemented; }
private:
    Steinberg::Vst::IEventList* source = nullptr;
    Steinberg::int32 count = 0;
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
#if IPLUG_EDITOR
    mMakeGraphicsFunc = [&]() {
        return iplug::igraphics::MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS);
    };
    mLayoutFunc = [&](iplug::igraphics::IGraphics* graphics) {
        using namespace iplug::igraphics;
        if (graphics->NControls() != 0) return;
        graphics->AttachPanelBackground(COLOR_WHITE);
        graphics->LoadFont("LXRSystem", "Segoe UI", ETextStyle::Normal);
        const IText text(18, COLOR_BLACK, "LXRSystem");
        graphics->AttachControl(new ITextControl(IRECT(20, 20, 580, 65),
            "LXR sequencer bank import", text));
        auto* status = new ITextControl(IRECT(20, 150, 580, 240),
            "Load an LXQB bank. Transport uses host parameters 240-242.", text);
        graphics->AttachControl(status);
        graphics->AttachControl(new IVButtonControl(IRECT(20, 80, 310, 135),
            [this, graphics, status](IControl*) {
                WDL_String file, directory;
                graphics->PromptForFile(file, directory, EFileAction::Open, "lxqb",
                    [this, status](const WDL_String& selected, const WDL_String&) {
                        if (selected.GetLength() == 0) return;
                        status->SetStr(ImportSequencerBank(selected.Get())
                            ? "Bank loaded. Enable the sequencer to play."
                            : "Import rejected or Busy. Previous state retained.");
                    });
            }, "Import sequencer bank", DEFAULT_STYLE.WithLabelText(text)));
    };
#endif
    // Preserve the native parameter contract.
    static_assert(
        lxr::vst::kNativeNumParams == lxr::kNumParams,
        "Native VST and DSP parameter counts must match"
    );

    static_assert(
        lxr::vst::kNativeNumParams == 228 &&
        lxr::vst::kPersistentNumParams == 240 &&
        lxr::vst::kNumParams == 245,
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

    // Minimal P16-F1 transport surface. These controls intentionally do not
    // enter LXRS v2; a newly opened project therefore defaults sequencing off.
    GetParam(lxr::vst::kSequencerEnableParam)->InitInt(
        "Sequencer Enable", 0, 0, 1);
    GetParam(lxr::vst::kSequencerClockModeParam)->InitInt(
        "Sequencer Clock", 0, 0, 1);
    GetParam(lxr::vst::kSequencerClockModeParam)->SetDisplayFunc(
        [](double value, WDL_String& display) {
            display.Set(value >= 0.5 ? "Host Sync" : "Internal");
        });
    GetParam(lxr::vst::kSequencerInternalTempoParam)->InitInt(
        "Sequencer Internal BPM", 120, 1, 255, "BPM");

    GetParam(lxr::vst::kSequencerRollRateParam)->InitInt(
        "Sequencer Roll Rate", 8, 0, 13);
    GetParam(lxr::vst::kSequencerRollRateParam)->SetDisplayFunc(
        [](double value, WDL_String& display) {
            static const char* names[] {"One-shot", "1/1", "1/2", "1/3", "1/4",
                "1/6", "1/8", "1/12", "1/16", "1/24", "1/32", "1/48", "1/64", "1/128"};
            const auto index = static_cast<unsigned>(value);
            display.Set(index < 14u ? names[index] : "Invalid");
        });
    GetParam(lxr::vst::kSequencerRollTracksParam)->InitInt(
        "Sequencer Roll Tracks", 0, 0, 127);
    GetParam(lxr::vst::kSequencerRollTracksParam)->SetDisplayFunc(
        [](double value, WDL_String& display) {
            const auto mask = static_cast<unsigned>(value);
            static const char* names[] {"D1 ", "D2 ", "D3 ", "SN ", "CY ", "CH ", "OH "};
            display.Set(mask == 0u ? "Released" : "Hold: ");
            for (unsigned track = 0; track < 7u; ++track)
                if ((mask & (1u << track)) != 0u) display.Append(names[track]);
        });

#if defined(VST3_API)
    // VST3 3.7 hosts may omit all optional context unless the component asks.
    processContextRequirements.needProjectTimeMusic()
        .needBarPositionMusic().needCycleMusic().needTempo()
        .needTimeSignature().needTransportState();
#endif

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
    (void)mSequencer.patternAuthority().tryAcquire(mAppliedPattern);
    mSequencer.useProjectSnapshot(&mAppliedPattern);

    // Clear native cache and resampler state.
    // Host-rate configuration occurs in OnReset().
    mAudioAdapter.reset();
    mSequencer.reset(0u);
    mAppliedSequencerEnabled = false;

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
    lxr::RoutingStateAuthority::Snapshot& routing,
    lxr::sequencer::PatternStateAuthority::ReadHandle* pattern,
    lxr::sequencer::state::TransportConfig* transport,
    lxr::sequencer::RollHostControls* rolls) const
{
    lxr::RoutingStateAuthority::Guard guard;
    if (!mRoutingState.tryAcquire(guard))
        return false;
    if (!mState.trySnapshot(parameters, &parameterGeneration, 1))
        return false;
    if (pattern != nullptr && const_cast<LXR*>(this)->mSequencer.patternAuthority()
            .tryAcquire(*pattern, 1) != lxr::sequencer::Status::Ok)
        return false;
    if (transport != nullptr) *transport = mTransportConfig;
    if (rolls != nullptr) *rolls = mRollControls;
    if (!mRoutingState.copyLocked(guard, routing)) return false;
    // Capture bypass inside the same restore coordinator. A later full restore
    // must not pair its suffix with this snapshot's older patch/bank/config.
    routing.state.bypass = GetBypassed();
    return true;
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
    // A complete restore starts with fresh transient voice/RNG storage. Calling
    // init() on a copy of the playing Engine retains voice oscillator RNGs and
    // other fields that the firmware's startup-only initializers do not clear.
    // Asset views remain resident/non-owning; construction does no asset I/O.
    const auto assets = mEngine.tables;
    mStagingEngine = lxr::Engine{};
    mStagingEngine.tables = assets;
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
    mSequencer.reset(0u);
    mAppliedSequencerEnabled = false;
    mAppliedRollMask = 0u;
    mMidiParser = lxr::MidiParser();
    return true;
}

void LXR::ResetEngineState()
{
    StateAuthority::Values parameters {};
    uint64_t parameterGeneration = 0;
    lxr::RoutingStateAuthority::Snapshot routing;
    lxr::sequencer::PatternStateAuthority::ReadHandle pattern;
    lxr::sequencer::state::TransportConfig transport;
    lxr::sequencer::RollHostControls rolls;
    if (!TryPersistentSnapshot(parameters, parameterGeneration, routing, &pattern, &transport, &rolls))
        return;

    if (!StageCompleteRestore(parameters, routing.state))
        return;
    mAppliedPattern.swap(pattern);
    mAudioTransport = transport;
    mAudioRollControls = rolls;
    mSequencer.resetSeed(transport.seed);
    (void)mSequencer.selectPattern(transport.pattern);

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
    for (const auto& value : mDeferredTransportValues)
    {
        const auto encoded = value.load(std::memory_order_acquire);
        const auto order = encoded >> 10u;
        pending |= (encoded & 0x200u) != 0u && (order <= cutoff || order <= maximumOrder);
    }
    for (const auto& value : mDeferredRollValues)
    {
        const auto encoded = value.load(std::memory_order_acquire);
        const auto order = encoded >> 9u;
        pending |= (encoded & 0x100u) != 0u && (order <= cutoff || order <= maximumOrder);
    }
    pending |= mRollReleasePending.load(std::memory_order_acquire);
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
    bool transportChanged = false;
    for (std::size_t slot = 0; slot < mDeferredTransportValues.size(); ++slot)
    {
        auto& mailbox = mDeferredTransportValues[slot];
        auto encoded = mailbox.load(std::memory_order_acquire);
        bool consumed = false;
        for (unsigned attempt = 0; attempt < 4u; ++attempt)
        {
            if ((encoded & 0x200u) == 0u) break;
            const auto order = encoded >> 10u;
            if (order > cutoff && order > maximumOrder) break;
            if (mailbox.compare_exchange_weak(encoded, 0, std::memory_order_acq_rel,
                                              std::memory_order_acquire))
            {
                consumed = true;
                if (order > cutoff)
                {
                    const auto value = static_cast<std::uint16_t>(encoded & 0x1ffu);
                    if (slot == 0) mTransportConfig.enabled = static_cast<std::uint8_t>(value);
                    if (slot == 1) mTransportConfig.clock = static_cast<std::uint8_t>(value);
                    if (slot == 2) mTransportConfig.bpm = value;
                    transportChanged = true;
                }
                const int value = slot == 0 ? mTransportConfig.enabled
                    : slot == 1 ? mTransportConfig.clock : mTransportConfig.bpm;
                GetParam(lxr::vst::kSequencerEnableParam + static_cast<int>(slot))->Set(value);
                break;
            }
        }
        if (!consumed && (encoded & 0x200u) != 0 &&
            ((encoded >> 10u) <= cutoff || (encoded >> 10u) <= maximumOrder)) complete = false;
    }
    if (transportChanged)
    {
        mSequencerEnabled.store(mTransportConfig.enabled, std::memory_order_release);
        mSequencerClockMode.store(mTransportConfig.clock, std::memory_order_release);
        mSequencerInternalTempo.store(mTransportConfig.bpm, std::memory_order_release);
        mRoutingState.commitPatchLocked(outer);
    }
    bool rollChanged = false;
    if (mRollReleasePending.exchange(false, std::memory_order_acq_rel))
    {
        const auto release = mRollReleaseCutoff.load(std::memory_order_acquire);
        if (mRollControls.pressTicket <= release)
        {
            mRollControls.tracks = 0u;
            GetParam(lxr::vst::kSequencerRollTracksParam)->Set(0.0);
            rollChanged = true;
        }
    }
    for (std::size_t slot = 0; slot < mDeferredRollValues.size(); ++slot)
    {
        auto& mailbox = mDeferredRollValues[slot];
        auto encoded = mailbox.load(std::memory_order_acquire);
        bool consumed = false;
        for (unsigned attempt = 0; attempt < 4u; ++attempt)
        {
            if ((encoded & 0x100u) == 0u) break;
            const auto order = encoded >> 9u;
            if (order > cutoff && order > maximumOrder) break;
            if (mailbox.compare_exchange_weak(encoded, 0u, std::memory_order_acq_rel,
                                              std::memory_order_acquire))
            {
                consumed = true;
                const auto release = slot == 1u
                    ? mRollReleaseCutoff.load(std::memory_order_acquire) : 0u;
                if (order > cutoff && order > release)
                {
                    const auto value = static_cast<std::uint8_t>(encoded & 0x7fu);
                    if (slot == 0u) mRollControls.rate = value;
                    else { mRollControls.tracks = value; mRollControls.pressTicket = order; }
                    rollChanged = true;
                }
                GetParam(lxr::vst::kSequencerRollRateParam + static_cast<int>(slot))->Set(
                    slot == 0u ? mRollControls.rate : mRollControls.tracks);
                break;
            }
        }
        if (!consumed && (encoded & 0x100u) != 0u &&
            ((encoded >> 9u) <= cutoff || (encoded >> 9u) <= maximumOrder)) complete = false;
    }
    if (rollChanged) mRoutingState.commitPatchLocked(outer);
    return complete;
}

bool LXR::ApplyPendingStateAndParameters()
{
    if (!FlushDeferredHostParameters())
        return false;

    StateAuthority::Values candidate {};
    uint64_t parameterGeneration = 0;
    lxr::RoutingStateAuthority::Snapshot routing;
    lxr::sequencer::PatternStateAuthority::ReadHandle pattern;
    lxr::sequencer::state::TransportConfig transport;
    lxr::sequencer::RollHostControls rolls;
    if (!TryPersistentSnapshot(candidate, parameterGeneration, routing, &pattern, &transport, &rolls))
        return false;

    if (routing.restoreEpoch != mAppliedRestoreGeneration)
    {
        if (!StageCompleteRestore(candidate, routing.state))
            return false;
        mSequencer.resetSeed(transport.seed);
        (void)mSequencer.selectPattern(transport.pattern);
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
    // Select all four domains before any event generation or rendering.
    mAppliedPattern.swap(pattern);
    mAudioTransport = transport;
    mAudioRollControls = rolls;
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

    lxr::state::ProjectCandidate candidate;
    uint64_t parameterGeneration = 0;
    lxr::RoutingStateAuthority::Snapshot routing;
    lxr::sequencer::PatternStateAuthority::ReadHandle pattern;
    if (!TryPersistentSnapshot(candidate.patch, parameterGeneration, routing,
                               &pattern, &candidate.transport)) return false;
    candidate.routing = routing.state;
    candidate.bank = pattern.bank();
    lxr::state::ProjectBytes encoded;
    std::size_t size = 0;
    if (!lxr::state::EncodeProject(candidate, encoded, size, false)) return false;
    return chunk.PutBytes(encoded.data(), static_cast<int>(size)) > 0;
}

bool LXR::PublishPersistentRestore(
    const StateAuthority::Values& parameters,
    const lxr::PersistentRoutingState& routing,
    const lxr::sequencer::PatternBank* bank,
    const lxr::sequencer::state::TransportConfig* transport,
    uint64_t expectedJointGeneration)
{
    const auto defaults = bank == nullptr ? lxr::sequencer::MakeDefaultPatternBank()
                                           : lxr::sequencer::PatternBank{};
    const auto& acceptedBank = bank != nullptr ? *bank : defaults;
    const auto config = transport != nullptr ? *transport
        : lxr::sequencer::state::TransportConfig{};
    for (auto value : parameters) if (value > 127u) return false;
    if (!lxr::sequencer::state::Validate(config) ||
        lxr::sequencer::ValidateBank(acceptedBank) != lxr::sequencer::Status::Ok)
        return false;
    lxr::sequencer::PatternStateAuthority::WriteHandle patternWriter;
    auto& authority = mSequencer.patternAuthority();
    if (authority.tryBeginEdit(patternWriter) != lxr::sequencer::Status::Ok ||
        patternWriter.replace(acceptedBank) != lxr::sequencer::Status::Ok ||
        authority.generation() == UINT64_MAX) return false;
    const uint64_t restoreOrder =
        mDeferredHostOrder.fetch_add(1, std::memory_order_acq_rel);
    if (restoreOrder == 0u)
        return false;

    lxr::RoutingStateAuthority::Guard outer;
    if (!mRoutingState.tryAcquire(outer) ||
        (expectedJointGeneration != UINT64_MAX &&
         mRoutingState.generation() != expectedJointGeneration) ||
        mRoutingState.validateRestoreLocked(outer, routing, restoreOrder) !=
            lxr::PersistentRouteStatus::Ok)
    {
        return false;
    }
    StateAuthority::WriteHandle writer;
    if (!mState.tryBeginWrite(writer))
        return false;
    if (authority.commit(patternWriter) != lxr::sequencer::Status::Ok)
    {
        mState.cancelUnmodified(writer);
        return false;
    }
    if (!mState.writeAll(writer, parameters))
    {
        mState.cancelUnmodified(writer);
        return false;
    }

    mState.commit(writer);
    mTransportConfig = config;
    mRollControls = {};
    mSequencerEnabled.store(config.enabled, std::memory_order_release);
    mSequencerClockMode.store(config.clock, std::memory_order_release);
    mSequencerInternalTempo.store(config.bpm, std::memory_order_release);
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
    GetParam(lxr::vst::kSequencerEnableParam)->Set(config.enabled);
    GetParam(lxr::vst::kSequencerClockModeParam)->Set(config.clock);
    GetParam(lxr::vst::kSequencerInternalTempoParam)->Set(config.bpm);
    GetParam(lxr::vst::kSequencerRollRateParam)->Set(8.0);
    GetParam(lxr::vst::kSequencerRollTracksParam)->Set(0.0);
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

    lxr::state::ProjectCandidate candidate;
    const auto* data = chunk.GetData() + startPos;
    const auto bytes = static_cast<std::size_t>(chunk.Size() - startPos);
    bool suffix = lxr::state::DecodeProject(data, bytes, candidate, true);
    if (!suffix && !lxr::state::DecodeProject(data, bytes, candidate, false)) return -1;
    if (!suffix) candidate.routing.bypass = GetBypassed();
    if (!PublishPersistentRestore(candidate.patch, candidate.routing,
                                   &candidate.bank, &candidate.transport)) return -1;
    return startPos + static_cast<int>(bytes - (suffix ? 4u : 0u));
}

// Called only by the editor's main-thread file-dialog completion.
bool LXR::ImportSequencerBank(const char* path)
{
    if (path == nullptr || *path == '\0') return false;
    lxr::sequencer::state::EncodedBank bytes;
    auto* file = fopen(path, "rb"); // iPlug2's Windows UTF-8 file wrapper.
    if (file == nullptr) return false;
    const auto read = std::fread(bytes.data(), 1, bytes.size(), file);
    const bool exact = read == bytes.size() && std::fgetc(file) == EOF && !std::ferror(file);
    std::fclose(file);
    lxr::state::ProjectCandidate candidate;
    if (!exact || !lxr::sequencer::state::Decode(bytes.data(), bytes.size(), candidate.bank))
        return false;
    const auto watermark = mDeferredHostOrder.load(std::memory_order_acquire) - 1u;
    if (!FlushDeferredHostParameters(watermark)) return false;
    lxr::RoutingStateAuthority::Snapshot routing;
    uint64_t generation = 0;
    if (!TryPersistentSnapshot(candidate.patch, generation, routing, nullptr, &candidate.transport))
        return false;
    candidate.routing = routing.state;
    return PublishPersistentRestore(candidate.patch, candidate.routing,
                                    &candidate.bank, &candidate.transport, routing.generation);
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
    ReleaseRollControls();
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
        ReleaseRollControls();
        mAudioAdapter.reset();
        mSequencer.reset(0u);
        mAppliedSequencerEnabled = false;
    }
}

// ---------------------------------------------------------
// Host parameter change
// ---------------------------------------------------------

void LXR::OnParamChange(
    int paramIdx
)
{
    if (paramIdx == lxr::vst::kSequencerRollRateParam ||
        paramIdx == lxr::vst::kSequencerRollTracksParam)
    {
        const auto* param = GetParam(paramIdx);
        if (param == nullptr) return;
        const int value = param->Int();
        const auto slot = static_cast<std::size_t>(paramIdx - lxr::vst::kSequencerRollRateParam);
        if (value < 0 || value > (slot == 0u ? 13 : 127)) return;
        const auto ticket = mDeferredHostOrder.fetch_add(1u, std::memory_order_acq_rel);
        auto& mailbox = mDeferredRollValues[slot];
        const auto desired = (ticket << 9u) | 0x100u | static_cast<uint64_t>(value);
        auto current = mailbox.load(std::memory_order_acquire);
        for (unsigned attempt = 0; attempt < 4u && (current >> 9u) < ticket; ++attempt)
            if (mailbox.compare_exchange_weak(current, desired,
                    std::memory_order_release, std::memory_order_acquire)) break;
        return;
    }
    if (paramIdx == lxr::vst::kSequencerEnableParam ||
        paramIdx == lxr::vst::kSequencerClockModeParam ||
        paramIdx == lxr::vst::kSequencerInternalTempoParam)
    {
        const auto* param = GetParam(paramIdx);
        if (param == nullptr)
            return;
        const int value = param->Int();
        const auto slot = static_cast<std::size_t>(paramIdx - lxr::vst::kSequencerEnableParam);
        if (value < (slot == 2u ? 1 : 0) || value > (slot == 2u ? 255 : 1)) return;
        const auto ticket = mDeferredHostOrder.fetch_add(1, std::memory_order_acq_rel);
        const auto encoded = (ticket << 10u) | 0x200u | static_cast<uint64_t>(value);
        auto& mailbox = mDeferredTransportValues[slot];
        auto current = mailbox.load(std::memory_order_acquire);
        for (unsigned attempt = 0; attempt < 4u && (current >> 10u) < ticket; ++attempt)
            if (mailbox.compare_exchange_weak(current, encoded,
                    std::memory_order_release, std::memory_order_acquire)) break;
        return;
    }

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
    if (tag >= static_cast<Steinberg::Vst::ParamID>(
            lxr::vst::kSequencerEnableParam) &&
        tag <= static_cast<Steinberg::Vst::ParamID>(
            lxr::vst::kSequencerRollTracksParam))
    {
        if (!std::isfinite(value) || value < 0.0 || value > 1.0 ||
            GetParam(static_cast<int>(tag)) == nullptr)
            return Steinberg::kResultFalse;
        return iplug::Plugin::setParamNormalized(tag, value);
    }

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

    lxr::state::ProjectCandidate candidate;
    uint64_t parameterGeneration = 0;
    lxr::RoutingStateAuthority::Snapshot routing;
    lxr::sequencer::PatternStateAuthority::ReadHandle pattern;
    if (!TryPersistentSnapshot(candidate.patch, parameterGeneration, routing,
                               &pattern, &candidate.transport)) return Steinberg::kResultFalse;
    candidate.routing = routing.state;
    candidate.bank = pattern.bank();
    lxr::state::ProjectBytes encoded;
    std::size_t encodedSize = 0;
    if (!lxr::state::EncodeProject(candidate, encoded, encodedSize, true))
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
    std::array<uint8_t, lxr::state::kV3MaximumStreamBytes + 1u> bytes {};
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
        if (size > lxr::state::kV3MaximumStreamBytes)
            return Steinberg::kResultFalse;
        if (readResult != Steinberg::kResultOk &&
            readResult != Steinberg::kResultTrue && GetHost() != iplug::kHostWaveLab)
            return Steinberg::kResultFalse;
    }

    lxr::state::ProjectCandidate candidate;
    if (!lxr::state::DecodeProject(bytes.data(), size, candidate, true) ||
        !PublishPersistentRestore(candidate.patch, candidate.routing,
                                    &candidate.bank, &candidate.transport))
        return Steinberg::kResultFalse;

    // Update the framework's separate controller cache only after the complete
    // patch+routing+bypass transaction has committed.
    UpdateParams(this, candidate.routing.bypass ? 1 : 0);
    OnRestoreState();
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API LXR::process(Steinberg::Vst::ProcessData& data)
{
    mCallbackMidiCount = 0;
    mCallbackMidiFrames = data.numSamples;
    mCollectCallbackMidi = true;
    mHostProcessContext = {};
    mHostProcessContext.callbackFrames = data.numSamples;
    mHostProcessContext.offline =
        data.processMode == Steinberg::Vst::kOffline;
    const auto* context = data.processContext;
    if (context != nullptr)
    {
        mHostProcessContext.contextPresent = true;
        mHostProcessContext.validFields =
            lxr::sequencer::HostSamplePosition |
            lxr::sequencer::HostTransportState;
        mHostProcessContext.sampleRate = context->sampleRate;
        mHostProcessContext.projectSamplePosition =
            context->projectTimeSamples;
        mHostProcessContext.playing =
            (context->state & Steinberg::Vst::ProcessContext::kPlaying) != 0;
        mHostProcessContext.cycleActive =
            (context->state & Steinberg::Vst::ProcessContext::kCycleActive) != 0;
        if ((context->state &
             Steinberg::Vst::ProcessContext::kProjectTimeMusicValid) != 0)
        {
            mHostProcessContext.validFields |=
                lxr::sequencer::HostPpqPosition;
            mHostProcessContext.projectPpqPosition =
                context->projectTimeMusic;
        }
        if ((context->state & Steinberg::Vst::ProcessContext::kTempoValid) != 0)
        {
            mHostProcessContext.validFields |= lxr::sequencer::HostTempo;
            mHostProcessContext.tempoBpm = context->tempo;
        }
        if ((context->state & Steinberg::Vst::ProcessContext::kCycleValid) != 0)
        {
            mHostProcessContext.validFields |= lxr::sequencer::HostCycle;
            mHostProcessContext.cycleStartPpq = context->cycleStartMusic;
            mHostProcessContext.cycleEndPpq = context->cycleEndMusic;
        }
        if ((context->state & Steinberg::Vst::ProcessContext::kTimeSigValid) != 0)
        {
            mHostProcessContext.validFields |=
                lxr::sequencer::HostTimeSignature;
            mHostProcessContext.timeSigNumerator = context->timeSigNumerator;
            mHostProcessContext.timeSigDenominator = context->timeSigDenominator;
        }
        if ((context->state & Steinberg::Vst::ProcessContext::kBarPositionValid) != 0)
        {
            mHostProcessContext.validFields |=
                lxr::sequencer::HostBarPosition;
            mHostProcessContext.barPositionPpq = context->barPositionMusic;
        }
    }

    MidiParameterChanges changes(*this, data.inputParameterChanges,
        mRejectedParameterQueues, mRejectedMidiInputRecords, mMalformedMidiInputCounts);
    BoundedMidiEvents events(data.inputEvents, mRejectedMidiInputRecords, mMalformedMidiInputCounts);
    // The pinned dispatcher reads only the last point. Preflight that exact
    // point so rejected route automation republishes canonical readback while
    // the adapter prevents the invalid value from reaching IParam.
    const auto queueCount = changes.sourceQueueCount();
    for (Steinberg::int32 i = 0; i < queueCount; ++i)
    {
        auto* queue = changes.sourceQueue(i);
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
    auto adapted = data;
    adapted.inputParameterChanges = &changes;
    adapted.inputEvents = data.inputEvents != nullptr ? &events : nullptr;
    const auto result = iplug::Plugin::process(adapted);
    FlushCallbackMidi();
    return result;
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
    if (mRollReleasePending.load(std::memory_order_acquire) &&
        !FlushDeferredHostParameters()) return;
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
        const bool transport = i >= lxr::vst::kPersistentNumParams;
        const std::size_t routeSlot = native || transport
            ? 0u : i - lxr::state::kParameterCount;
        const uint64_t generation = native ? mState.valueGeneration(i)
            : transport ? routeSnapshot.generation : routeSnapshot.routeGenerations[routeSlot];
        const double transportValue = i == lxr::vst::kSequencerEnableParam ? mTransportConfig.enabled
            : i == lxr::vst::kSequencerClockModeParam ? mTransportConfig.clock
            : i == lxr::vst::kSequencerInternalTempoParam ? mTransportConfig.bpm
            : i == lxr::vst::kSequencerRollRateParam ? mRollControls.rate : mRollControls.tracks;
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
                : transport ? transportValue : lxr::route_host::encode(
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

void LXR::FlushCallbackMidi()
{
    mCollectCallbackMidi = false;
    // The framework skips ProcessBlock while bypassed. Keep accepting MIDI
    // state changes there, and in zero-audio callbacks, as before this repair.
    std::sort(mCallbackMidi.begin(), mCallbackMidi.begin() + mCallbackMidiCount,
        [](const CallbackMidi& a, const CallbackMidi& b) noexcept {
            return a.message.mOffset < b.message.mOffset ||
                (a.message.mOffset == b.message.mOffset && a.sequence < b.sequence);
        });
    for (std::size_t i = 0; i < mCallbackMidiCount; ++i)
        ProcessMidiMsg(mCallbackMidi[i].message);
    mCallbackMidiCount = 0;
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
    if (mCollectCallbackMidi)
    {
        // Zero-frame VST3 flush calls may deliver offset zero. Other offsets
        // must belong to this callback; never pull an invalid future event in.
        if (msg.mOffset < 0 ||
            (msg.mOffset >= mCallbackMidiFrames &&
             !(mCallbackMidiFrames == 0 && msg.mOffset == 0)))
        {
            mInvalidMidiOffsets.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        if (mCallbackMidiCount == mCallbackMidi.size())
        {
            mDeferredMidiOverflows.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const auto sequence = static_cast<std::uint32_t>(mCallbackMidiCount);
        mCallbackMidi[mCallbackMidiCount++] = {msg, sequence};
        mCallbackMidiPeak = std::max(mCallbackMidiPeak, static_cast<std::uint32_t>(mCallbackMidiCount));
        return;
    }

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
        mParams,
        &mSequencer
    );
    ++mDispatchedMidiMessages;

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

void LXR::ApplySequencerTransport(int callbackFrames) noexcept
{
    const bool enabled =
        mAudioTransport.enabled != 0u;
    const std::uint8_t mode =
        mAudioTransport.clock;
    const std::uint16_t internalTempo =
        mAudioTransport.bpm;
    const std::uint64_t nativeFrame = mAudioAdapter.nativeFrame();

    if (!enabled)
    {
        if (mAppliedRollMask != 0u || (mAudioRollControls.tracks != 0u &&
            mAudioRollControls.pressTicket > mRollReleaseCutoff.load(std::memory_order_acquire)))
            ReleaseRollControls();
        if (mAppliedSequencerEnabled)
        {
            (void)mEngine.clearSequencerAutomation();
            mSequencer.reset(nativeFrame);
        }
        mAppliedSequencerEnabled = false;
        mAppliedSequencerClockMode = mode;
        mAppliedSequencerInternalTempo = internalTempo;
        return;
    }

    const bool changedMode = !mAppliedSequencerEnabled ||
        mAppliedSequencerClockMode != mode;
    if (changedMode)
    {
        if (mAppliedSequencerEnabled) ReleaseRollControls();
        mAppliedRollMask = 0u;
        (void)mEngine.clearSequencerAutomation();
        mSequencer.reset(nativeFrame);
        mSequencer.resetSeed(mAudioTransport.seed);
        (void)mSequencer.selectPattern(mAudioTransport.pattern);
    }

    mAppliedSequencerEnabled = true;
    mAppliedSequencerClockMode = mode;
    mAppliedSequencerInternalTempo = internalTempo;

    if (mode == 0u)
    {
        (void)mSequencer.setTempo(internalTempo);
        (void)mEngine.setNativeTempo(internalTempo);
        if (!mSequencer.running())
            (void)mSequencer.start();
        return;
    }

    auto host = mHostProcessContext;
    host.callbackFrames = callbackFrames;

    if ((host.validFields & lxr::sequencer::HostTempo) != 0u &&
        std::isfinite(host.tempoBpm) && host.tempoBpm >= 1.0 &&
        host.tempoBpm <= 255.0)
    {
        (void)mEngine.setNativeTempo(static_cast<std::uint16_t>(
            std::floor(host.tempoBpm + 0.5)));
    }
    const auto decision = mSequencer.beginHostCallback(host, nativeFrame, mEngine);
    if (decision.transition == lxr::sequencer::HostTransition::Stop ||
        decision.transition == lxr::sequencer::HostTransition::InvalidContext ||
        (decision.transition == lxr::sequencer::HostTransition::Seek && !host.playing))
        ReleaseRollControls();
}

void LXR::ReleaseRollControls() noexcept
{
    const auto cutoff = mDeferredHostOrder.fetch_add(1u, std::memory_order_acq_rel);
    mRollReleaseCutoff.store(cutoff, std::memory_order_release);
    mRollReleasePending.store(true, std::memory_order_release);
    mSequencer.cancelRolls();
    mAppliedRollMask = 0u;
}

void LXR::ApplyRollControls() noexcept
{
    if (!mAppliedSequencerEnabled) return;
    (void)mSequencer.setRollRate(mAudioRollControls.rate);
    const auto desired = mAudioRollControls.pressTicket >
        mRollReleaseCutoff.load(std::memory_order_acquire)
        ? mAudioRollControls.tracks : static_cast<std::uint8_t>(0u);
    for (std::uint8_t track = 0u; track < 7u; ++track)
    {
        const auto bit = static_cast<std::uint8_t>(1u << track);
        if (((desired ^ mAppliedRollMask) & bit) != 0u)
            (void)mSequencer.setRoll(track, (desired & bit) != 0u);
    }
    mAppliedRollMask = desired;
}

void LXR::ProcessBlock(
    double** inputs,
    double** outputs,
    int nFrames
)
{
    // LXR currently operates as an instrument.
    (void)inputs;

    mCollectCallbackMidi = false;
    // iPlug2 delivers synthetic CC parameter queues before the note-event
    // list, so arrival order need not be timestamp order. std::sort operates
    // in resident storage; the explicit sequence preserves simultaneous order.
    std::sort(mCallbackMidi.begin(), mCallbackMidi.begin() + mCallbackMidiCount,
        [](const CallbackMidi& a, const CallbackMidi& b) noexcept {
            return a.message.mOffset < b.message.mOffset ||
                (a.message.mOffset == b.message.mOffset && a.sequence < b.sequence);
        });

    if (ApplyPendingStateAndParameters())
        DrainDeferredMidi();

    ApplySequencerTransport(nFrames);
    ApplyRollControls();

    // Flush/no-output calls have no sample loop. Retain their MIDI/state
    // publication behavior without attempting native or FIR processing.
    if (outputs == nullptr || nFrames <= 0 || NOutChansConnected() <= 0 ||
        !mAssetsLoaded || !mAudioAdapter.ready())
        FlushCallbackMidi();

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
    std::size_t midiIndex = 0;
    for (int sample = 0;
         sample < nFrames;
         ++sample)
    {
        while (midiIndex < mCallbackMidiCount &&
               mCallbackMidi[midiIndex].message.mOffset <= sample)
        {
            ProcessMidiMsg(mCallbackMidi[midiIndex++].message);
        }
        double left = 0.0;
        double right = 0.0;

        // Render one host-rate stereo sample.
        mAudioAdapter.getNextHostST1Sample(
            mEngine,
            left,
            right,
            &mSequencer,
            sample
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
    mCallbackMidiCount = 0;
}
