// P16-F1 exact-private-binary VST3 transport and control integration.
#include "../vendor/iPlug2/Examples/LXR/LXR.h"

#define main ExistingP12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main

#include <limits>

struct P16F1VST3TransportTestProbe
{
  static LXR& plugin(HostInstance& host)
  {
    return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(
      static_cast<SingleComponentEffect*>(host.component)));
  }

  static bool publish(HostInstance& host, const lxr::sequencer::PatternBank& bank)
  {
    auto saved = host.saveState();
    lxr::state::ProjectCandidate candidate;
    if (!lxr::state::DecodeProject(saved.data(), saved.size(), candidate, true)) return false;
    candidate.bank = bank;
    lxr::state::ProjectBytes bytes;
    std::size_t count = 0;
    if (!lxr::state::EncodeProject(candidate, bytes, count, true)) return false;
    std::vector<std::uint8_t> state(bytes.begin(), bytes.begin() + count);
    return host.loadState(state);
  }

  static const lxr::sequencer::TriggerSchedulerDiagnostics& diagnostics(
    HostInstance& host)
  {
    return plugin(host).mSequencer.diagnostics();
  }

  static const lxr::sequencer::HostTransportContext& context(HostInstance& host)
  {
    return plugin(host).mHostProcessContext;
  }

  static bool enabled(HostInstance& host)
  {
    return plugin(host).mSequencerEnabled.load(std::memory_order_acquire) != 0u;
  }

  static std::uint8_t mode(HostInstance& host)
  {
    return plugin(host).mSequencerClockMode.load(std::memory_order_acquire);
  }
};

namespace
{
using namespace lxr::sequencer;

PatternBank audibleBank()
{
  PatternBank bank {};
  for (std::size_t pattern = 0; pattern < kPatternCount; ++pattern)
    bank.patterns[pattern].nextPattern = static_cast<std::uint8_t>(pattern);
  bank.patterns[0].mainGates[0] = 0xffffu;
  for (auto& step : bank.patterns[0].steps[0])
  {
    SetStepActive(step, true);
    step.note = 60u;
    step.probability = 127u;
  }
  bank.patterns[0].steps[0][0].parameter1 = 89u;
  bank.patterns[0].steps[0][0].value1 = 40u;
  return bank;
}

struct BlockResult
{
  std::uint64_t hash = UINT64_C(1469598103934665603);
  bool nonzero = false;
};

void hash(BlockResult& result, double value)
{
  std::uint64_t bits = 0u;
  std::memcpy(&bits, &value, sizeof(value));
  for (unsigned byte = 0u; byte < 8u; ++byte)
  {
    result.hash ^= (bits >> (byte * 8u)) & 0xffu;
    result.hash *= UINT64_C(1099511628211);
  }
  result.nonzero = result.nonzero || value != 0.0;
}

BlockResult processBlock(HostInstance& host, std::int64_t samplePosition,
                         double ppq, double bpm, int frames, bool playing,
                         bool cycle = false, double cycleStart = 0.0,
                         double cycleEnd = 0.0,
                         IParameterChanges* changes = nullptr,
                         bool provideContext = true)
{
  std::vector<double> left(static_cast<std::size_t>(frames), 0.0);
  std::vector<double> right(static_cast<std::size_t>(frames), 0.0);
  double* channels[] {left.data(), right.data()};
  AudioBusBuffers output {};
  output.numChannels = 2;
  output.channelBuffers64 = channels;
  ProcessContext context {};
  context.state = ProcessContext::kProjectTimeMusicValid |
                  ProcessContext::kTempoValid |
                  ProcessContext::kTimeSigValid;
  if (playing) context.state |= ProcessContext::kPlaying;
  if (cycle)
    context.state |= ProcessContext::kCycleActive | ProcessContext::kCycleValid;
  context.sampleRate = 48000.0;
  context.projectTimeSamples = samplePosition;
  context.projectTimeMusic = ppq;
  context.tempo = bpm;
  context.timeSigNumerator = 4;
  context.timeSigDenominator = 4;
  context.cycleStartMusic = cycleStart;
  context.cycleEndMusic = cycleEnd;
  ProcessData data {};
  data.processMode = kOffline;
  data.symbolicSampleSize = kSample64;
  data.numSamples = frames;
  data.numOutputs = 1;
  data.outputs = &output;
  data.inputParameterChanges = changes;
  data.processContext = provideContext ? &context : nullptr;
  require("process P16-F1 host block", host.processor->process(data) == kResultOk);
  BlockResult result;
  for (int sample = 0; sample < frames; ++sample)
  {
    hash(result, left[static_cast<std::size_t>(sample)]);
    hash(result, right[static_cast<std::size_t>(sample)]);
  }
  return result;
}

void addPoint(ParameterChanges& changes, ParamID id, double value)
{
  int32 queueIndex = 0;
  int32 pointIndex = 0;
  auto* queue = changes.addParameterData(id, queueIndex);
  require("add P16-F1 control point", queue != nullptr &&
          queue->addPoint(0, value, pointIndex) == kResultOk);
}

void enableHostSync(HostInstance& host)
{
  ParameterChanges changes(2);
  addPoint(changes, 240u, 1.0);
  addPoint(changes, 241u, 1.0);
  (void)processBlock(host, 0, 0.0, 120.0, 64, true,
                     false, 0.0, 0.0, &changes);
}

void verifyParameterInfo(HostInstance& host)
{
  check("P16-F1 controls are inside registered parameter range",
        host.controller->getParameterCount() >= 244);
  const int expectedSteps[] {1, 1, 254};
  const double expectedDefaults[] {0.0, 0.0, 119.0 / 254.0};
  for (int index = 0; index < 3; ++index)
  {
    ParameterInfo info {};
    require("query P16-F1 parameter info",
            host.controller->getParameterInfo(240 + index, info) == kResultOk);
    check("P16-F1 stable parameter ID", info.id == 240u + index);
    check("P16-F1 parameter step count", info.stepCount == expectedSteps[index]);
    check("P16-F1 parameter default", std::abs(
          info.defaultNormalizedValue - expectedDefaults[index]) < 1.0e-12);
  }
}
}

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    std::puts("Usage: P16F1VST3TransportTest.exe <absolute VST3 DLL path>");
    return 2;
  }
  HMODULE module = LoadLibraryA(argv[1]);
  require("load P16-F1 VST3", module != nullptr);
  auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
  auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
  auto getFactory = reinterpret_cast<IPluginFactory* (*)()>(
      GetProcAddress(module, "GetPluginFactory"));
  require("P16-F1 module exports", init && exitDll && getFactory);
  require("P16-F1 module init", init());
  IPluginFactory* factory = getFactory();
  require("P16-F1 factory", factory != nullptr);
  PClassInfo info {};
  bool found = false;
  for (int i = 0; i < factory->countClasses(); ++i)
    if (factory->getClassInfo(i, &info) == kResultOk &&
        std::strcmp(info.category, kVstAudioEffectClass) == 0)
    { found = true; break; }
  require("P16-F1 component class", found);

  {
    HostInstance host(factory, info.cid);
    verifyParameterInfo(host);
    IProcessContextRequirements* requirements = nullptr;
    require("query process context requirements",
      host.component->queryInterface(IProcessContextRequirements::iid,
        reinterpret_cast<void**>(&requirements)) == kResultOk);
    const uint32 flags = requirements->getProcessContextRequirements();
    check("request project PPQ", (flags & IProcessContextRequirements::kNeedProjectTimeMusic) != 0u);
    check("request tempo", (flags & IProcessContextRequirements::kNeedTempo) != 0u);
    check("request transport", (flags & IProcessContextRequirements::kNeedTransportState) != 0u);
    check("request cycle", (flags & IProcessContextRequirements::kNeedCycleMusic) != 0u);
    requirements->release();

    host.setup(48000.0);
    require("publish exact-binary transport fixture",
            P16F1VST3TransportTestProbe::publish(host, audibleBank()));
    const int editsBefore = host.handler.edits;
    enableHostSync(host);
    check("host controls reach processor",
          P16F1VST3TransportTestProbe::enabled(host) &&
          P16F1VST3TransportTestProbe::mode(host) == 1u);
    check("no unsolicited control gestures", host.handler.edits == editsBefore);
    const auto& captured = P16F1VST3TransportTestProbe::context(host);
    check("raw VST3 context validity captured",
          captured.contextPresent && captured.offline && captured.playing &&
          (captured.validFields & (HostSamplePosition | HostPpqPosition |
           HostTempo | HostTimeSignature | HostTransportState)) ==
          (HostSamplePosition | HostPpqPosition | HostTempo |
           HostTimeSignature | HostTransportState));

    std::int64_t sample = 64;
    double ppq = 64.0 / 24000.0;
    BlockResult aggregate;
    for (int block = 0; block < 32; ++block)
    {
      const auto rendered = processBlock(host, sample, ppq, 120.0, 127, true);
      aggregate.nonzero = aggregate.nonzero || rendered.nonzero;
      aggregate.hash ^= rendered.hash;
      sample += 127;
      ppq += 127.0 / 24000.0;
    }
    check("exact binary host-sync renders audible PCM", aggregate.nonzero);
    const auto beforeStop = P16F1VST3TransportTestProbe::diagnostics(host);
    (void)processBlock(host, sample, ppq, 120.0, 64, false);
    (void)processBlock(host, sample, ppq, 120.0, 64, true);
    sample += 64;
    ppq += 64.0 / 24000.0;
    (void)processBlock(host, 96000, 4.0, 120.0, 64, true);
    (void)processBlock(host, 0, 0.0, 120.0, 64, true,
                       true, 0.0, 4.0);
    (void)processBlock(host, 64, 64.0 / 24000.0, 60.0, 64, true);
    (void)processBlock(host, 128, 64.0 / 24000.0 + 64.0 / 48000.0,
                       60.0, 64, true,
                       false, 0.0, 0.0, nullptr, false);
    const auto after = P16F1VST3TransportTestProbe::diagnostics(host);
    check("exact binary stop transition", after.hostStops > beforeStop.hostStops);
    check("exact binary resume transition", after.hostResumes != 0u);
    check("exact binary seek transition", after.hostSeeks != 0u);
    check("exact binary loop transition", after.hostLoops != 0u);
    check("exact binary tempo transition", after.hostTempoChanges != 0u);
    check("missing context rejected", after.invalidHostCallbacks != 0u &&
          !P16F1VST3TransportTestProbe::context(host).contextPresent);

    ParameterChanges invalid(1);
    addPoint(invalid, 240u, std::numeric_limits<double>::quiet_NaN());
    (void)processBlock(host, 192, 192.0 / 48000.0, 60.0, 1, true,
                       false, 0.0, 0.0, &invalid);
    check("invalid transport automation rejected",
          P16F1VST3TransportTestProbe::enabled(host));
    check("direct invalid transport value rejected",
          host.controller->setParamNormalized(240u, -0.1) == kResultFalse &&
          host.controller->setParamNormalized(241u, 1.1) == kResultFalse);

    const auto state = host.saveState();
    check("LXRS v3 sequencer state size", state.size() == lxr::state::kV3CanonicalStreamBytes);
    HostInstance reopened(factory, info.cid);
    auto stateCopy = state;
    require("restore existing LXRS with transport controls",
            reopened.loadState(stateCopy));
    check("transport controls persist in LXRS v3",
          reopened.controller->getParamNormalized(240u) == 1.0 &&
          reopened.controller->getParamNormalized(241u) == 1.0);
  }

  factory->release();
  exitDll();
  FreeLibrary(module);
  std::printf("SUMMARY pass=%d fail=%d\n", passed, failed);
  return failed ? 1 : 0;
}
