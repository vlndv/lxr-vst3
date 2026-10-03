#pragma once
#include "IPlug_include_in_plug_hdr.h"

const int kNumPresets = 1;
const int kNumParams = 10;

class LXR final : public iplug::Plugin
{
public:
  LXR(const iplug::InstanceInfo& info);
  void ProcessBlock(double** inputs, double** outputs, int nFrames) override;
  void ProcessMidiMsg(const iplug::IMidiMsg& msg) override;
  void OnReset() override;
  void OnActivate(bool active) override;
  void OnParamChange(int paramIdx) override;
};