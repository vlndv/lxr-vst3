#include "LXR.h"
#include "IPlug_include_in_plug_src.h"

LXR::LXR(const iplug::InstanceInfo& info)
  : iplug::Plugin(info, iplug::MakeConfig(kNumParams, kNumPresets))
{
  for (int i = 0; i < kNumParams; i++) {
    char name[32];
    snprintf(name, sizeof(name), "PAR %d", i + 1);
    GetParam(i)->InitInt(name, 0, 0, 127);
  }

  TRACE;
}

void LXR::OnReset() {}
void LXR::OnActivate(bool active) {}
void LXR::OnParamChange(int paramIdx) {}

void LXR::ProcessBlock(double** inputs, double** outputs, int nFrames)
{
  const int nChans = NOutChansConnected();

  for (int c = 0; c < nChans; c++) {
    for (int i = 0; i < nFrames; i++) {
      outputs[c][i] = 0.0;
    }
  }
}

void LXR::ProcessMidiMsg(const iplug::IMidiMsg& msg)
{
  SendMidiMsg(msg);
}