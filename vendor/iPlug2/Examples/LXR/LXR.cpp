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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>

namespace
{

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
    // Reset DSP voices, modulation and mixer.
    mEngine.init();

    // Discard cached audio and FIR history.
    // Preserve the configured host sample rate.
    mAudioAdapter.reset();

    // Initialize the native parameter array.
    mParams.init();

    // Reapply all current host parameter values.
    for (int paramIdx = 0;
         paramIdx < lxr::vst::kNumParams;
         ++paramIdx)
    {
        const auto* param = GetParam(paramIdx);

        if (param == nullptr)
        {
            continue;
        }

        const uint8_t rawValue =
            GetRawLXRParameterValue(param);

        mParams.set(
            static_cast<uint8_t>(paramIdx),
            rawValue,
            mEngine
        );
    }

    // ORIGINAL QUIRK:
    // Internal engine timing and native PCM processing
    // remain unchanged regardless of host sample rate.
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
    mAudioAdapter.configure(
        GetSampleRate()
    );
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

    const auto* param =
        GetParam(paramIdx);

    if (param == nullptr)
    {
        return;
    }

    const uint8_t rawValue =
        GetRawLXRParameterValue(param);

    // Preserve canonical routing:
    // Host -> ParameterArray -> Engine.
    mParams.set(
        static_cast<uint8_t>(paramIdx),
        rawValue,
        mEngine
    );
}

// ---------------------------------------------------------
// MIDI input
// ---------------------------------------------------------

void LXR::ProcessMidiMsg(
    const iplug::IMidiMsg& msg
)
{
    // Never trigger synthesis without valid assets.
    if (!mAssetsLoaded)
    {
        return;
    }

    const auto statusMsg =
        msg.StatusMsg();

    if (statusMsg == iplug::IMidiMsg::kNone)
    {
        return;
    }

    // iPlug uses zero-based MIDI channels.
    const int channel =
        msg.Channel();

    if (channel < 0 ||
        channel > 15)
    {
        return;
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

    event.status = status;

    event.channel =
        static_cast<uint8_t>(channel);

    event.data1 = data1;
    event.data2 = data2;

    // Preserve original native MIDI dispatch.
    mMidiParser.processEvent(
        event,
        mEngine,
        mParams
    );
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
