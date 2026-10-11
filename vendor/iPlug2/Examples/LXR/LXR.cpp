
#include "LXR.h"
#include "IPlug_include_in_plug_src.h"

#include "../../../../../dsp/LXRResourceLocator.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>

namespace
{

// Convert a host parameter to the native LXR 7-bit representation.
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

// Write silence to every connected host output channel.
// This is used when the Engine cannot safely render audio.
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

  for (int channel = 0; channel < nChans; ++channel)
  {
    double* output = outputs[channel];

    if (output == nullptr)
    {
      continue;
    }

    for (int sample = 0; sample < nFrames; ++sample)
    {
      output[sample] = 0.0;
    }
  }
}

} // namespace

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
  // Ensure host and DSP parameter counts remain identical.
  static_assert(
    lxr::vst::kNumParams == lxr::kNumParams,
    "VST and DSP parameter counts must match"
  );

  static_assert(
    lxr::vst::kNumParams == 228,
    "Unexpected LXR parameter count"
  );

  // Register all native LXR parameters.
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

  // Initialize the canonical native parameter state.
  mParams.init();

  // Discard any cached audio.
  mAudioAdapter.reset();

  // Resolve resources relative to the loaded plugin module.
  //
  // Never depend on the host application's working directory.
  // This operation must remain outside ProcessBlock().
  const std::string assetDirectory =
    lxr::GetPluginAssetDirectory();

  // Load the five native oscillator/sample assets.
  //
  // If resolution or loading fails, rendering is disabled.
  mAssetsLoaded =
    !assetDirectory.empty() &&
    mEngine.loadAssets(assetDirectory.c_str());

  // Do not call Engine::processBlock() unless loading succeeded.
  //
  // OnReset() restores the Engine from the current host parameters.
  TRACE;
}

void LXR::ResetEngineState()
{
  // Reset native DSP voices and mixer.
  mEngine.init();

  // Invalidate cached samples from the previous Engine state.
  mAudioAdapter.reset();

  // Reset the canonical parameter storage.
  mParams.init();

  // Reconstruct Engine state from the current host parameters.
  //
  // This preserves the host's parameter values across reset,
  // project restoration and sample-rate changes.
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

  // Asset pointers are retained by the current Engine::init().
  // They are not reloaded on the realtime reset path.
}

void LXR::OnReset()
{
  ResetEngineState();
}

void LXR::OnActivate(
  bool active
)
{
  // The native Engine has no separate activation API.
  //
  // Reset the audio cache on deactivation so old buffered
  // samples are not replayed when processing resumes.
  if (!active)
  {
    mAudioAdapter.reset();
  }
}

void LXR::OnParamChange(
  int paramIdx
)
{
  // Only IDs 0..227 are native LXR parameters.
  //
  // Additional iPlug MIDI-controller parameters must not
  // access the native 228-entry ParameterArray.
  if (paramIdx < 0 ||
      paramIdx >= lxr::vst::kNumParams)
  {
    return;
  }

  const auto* param = GetParam(paramIdx);

  if (param == nullptr)
  {
    return;
  }

  const uint8_t rawValue =
    GetRawLXRParameterValue(param);

  // Host parameter -> ParameterArray -> Engine.
  mParams.set(
    static_cast<uint8_t>(paramIdx),
    rawValue,
    mEngine
  );
}

void LXR::ProcessMidiMsg(
  const iplug::IMidiMsg& msg
)
{
  // Reject MIDI messages if native assets are unavailable.
  //
  // This prevents triggering voices that cannot render safely.
  if (!mAssetsLoaded)
  {
    return;
  }

  // iPlug exposes the message type and channel separately.
  const auto statusMsg = msg.StatusMsg();

  if (statusMsg == iplug::IMidiMsg::kNone)
  {
    return;
  }

  const int channel = msg.Channel();

  if (channel < 0 || channel > 15)
  {
    return;
  }

  // Convert iPlug's message-type enum into the native
  // MIDI status representation expected by MidiParser.
  const uint8_t status =
    static_cast<uint8_t>(
      static_cast<uint8_t>(statusMsg) << 4
    );

  const uint8_t data1 =
    static_cast<uint8_t>(msg.mData1 & 0x7F);

  const uint8_t data2 =
    static_cast<uint8_t>(msg.mData2 & 0x7F);

  lxr::MidiEvent event {};

  event.status = status;

  event.channel =
    static_cast<uint8_t>(channel);

  event.data1 = data1;
  event.data2 = data2;

  // Preserve the native MIDI parser as the routing authority.
  mMidiParser.processEvent(
    event,
    mEngine,
    mParams
  );
}

void LXR::ProcessBlock(
  double** inputs,
  double** outputs,
  int nFrames
)
{
  // LXR currently operates as an instrument.
  (void) inputs;

  if (outputs == nullptr ||
      nFrames <= 0)
  {
    return;
  }

  const int nChans = NOutChansConnected();

  if (nChans <= 0)
  {
    return;
  }

  // Never execute the native DSP without loaded assets.
  //
  // The Engine uses waveform/sample pointers which may be
  // null when loading fails.
  if (!mAssetsLoaded)
  {
    ClearAudioOutputs(
      outputs,
      nChans,
      nFrames
    );

    return;
  }

  // Convert native signed 16-bit PCM to floating-point audio.
  //
  // -32768 -> -1.0
  //      0 ->  0.0
  //  32767 -> approximately +0.999969
  constexpr double kInt16ToDouble =
    1.0 / 32768.0;

  // Process any host buffer length.
  //
  // LXRAudioAdapter internally renders fixed 32-frame
  // Engine blocks and returns one native stereo sample
  // at a time.
  for (int sample = 0;
       sample < nFrames;
       ++sample)
  {
    int16_t nativeLeft = 0;
    int16_t nativeRight = 0;

    mAudioAdapter.getNextST1Sample(
      mEngine,
      nativeLeft,
      nativeRight
    );

    const double left =
      static_cast<double>(nativeLeft) *
      kInt16ToDouble;

    const double right =
      static_cast<double>(nativeRight) *
      kInt16ToDouble;

    // Mono output configuration.
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

    // ST2 remains internal until additional host buses
    // are explicitly implemented.
    //
    // Silence every additional connected output.
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
