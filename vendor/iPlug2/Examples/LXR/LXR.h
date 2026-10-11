#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "LXRParamContract.h"

#include "../../../../../dsp/Engine.h"
#include "../../../../../dsp/ParameterArray.h"
#include "../../../../../dsp/MidiParser.h"
#include "../../../../../dsp/LXRAudioAdapter.h"

class LXR final : public iplug::Plugin
{
public:
  explicit LXR(
    const iplug::InstanceInfo& info
  );

  void ProcessBlock(
    double** inputs,
    double** outputs,
    int nFrames
  ) override;

  void ProcessMidiMsg(
    const iplug::IMidiMsg& msg
  ) override;

  void OnReset() override;

  void OnActivate(
    bool active
  ) override;

  void OnParamChange(
    int paramIdx
  ) override;

private:
  // Native LXR synthesis/DSP engine.
  lxr::Engine mEngine;

  // Canonical 228-entry raw LXR parameter state.
  lxr::ParameterArray mParams;

  // Native LXR MIDI parser and dispatcher.
  lxr::MidiParser mMidiParser;

  // Host-independent bridge around the Engine's fixed 32-frame,
  // four-output rendering contract.
  lxr::LXRAudioAdapter mAudioAdapter;
  bool mAssetsLoaded = false;

  // Reset Engine and adapter state, then reconstruct the Engine from
  // the current host parameter values.
  void ResetEngineState();
};