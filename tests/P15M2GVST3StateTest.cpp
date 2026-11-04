// Production VST3 boundary test for P15-M2G LXRS v2 persistence.
// Original target identities Copyright 2013 Julian Schmidt; LICENSE-LXR.txt.
#include "../vendor/iPlug2/Examples/LXR/LXR.h"

#define main ExistingP12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main

#include <fstream>
#include <iterator>

struct P15M2GStateIntegrationTestProbe
{
  static LXR& plugin(HostInstance& host)
  {
    return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(
      static_cast<SingleComponentEffect*>(host.component)));
  }

  static bool authorityMatches(HostInstance& host,
                               const lxr::PersistentRoutingState& expected)
  {
    auto& instance = plugin(host);
    lxr::RoutingStateAuthority::Guard guard;
    lxr::RoutingStateAuthority::Snapshot snapshot;
    if (!instance.mRoutingState.tryAcquire(guard) ||
        !instance.mRoutingState.copyLocked(guard, snapshot))
      return false;
    for (std::size_t i = 0; i < expected.routes.size(); ++i)
    {
      const auto& actual = snapshot.state.routes[i];
      const auto& wanted = expected.routes[i];
      if (actual.owner != wanted.owner || actual.source != wanted.source ||
          actual.flags != wanted.flags ||
          actual.identityNamespace != wanted.identityNamespace ||
          actual.originalIndex != wanted.originalIndex ||
          actual.firmwareParameter != wanted.firmwareParameter ||
          actual.valueType != wanted.valueType || actual.scope != wanted.scope ||
          actual.physicalKey != wanted.physicalKey ||
          actual.menuVoiceHint != wanted.menuVoiceHint ||
          actual.reserved != wanted.reserved)
        return false;
    }
    return snapshot.state.bypass == expected.bypass;
  }

  static bool processingProjectionMatches(
    HostInstance& host, const lxr::PersistentRoutingState& expected)
  {
    const auto& applied = plugin(host).mAppliedRouting;
    for (std::size_t i = 0; i < expected.routes.size(); ++i)
    {
      if (applied.routes[i].flags != expected.routes[i].flags ||
          applied.routes[i].originalIndex != expected.routes[i].originalIndex)
        return false;
    }
    return true;
  }
};

namespace
{
std::vector<std::uint8_t> readBytes(const char* path)
{
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

void processRestoreBoundary(HostInstance& host)
{
  double left[1] {}, right[1] {};
  double* channels[] {left, right};
  AudioBusBuffers output {};
  output.numChannels = 2;
  output.channelBuffers64 = channels;
  ProcessData data {};
  data.processMode = kOffline;
  data.symbolicSampleSize = kSample64;
  data.numSamples = 1;
  data.numOutputs = 1;
  data.outputs = &output;
  require("apply restore at processing boundary",
          host.processor->process(data) == kResultOk);
}

bool decodeRouting(const std::vector<std::uint8_t>& bytes,
                   std::array<std::uint8_t, 228>& patch,
                   lxr::PersistentRoutingState& routing)
{
  return static_cast<bool>(lxr::state::DecodePersistentState(
    bytes.data(), bytes.size(), patch, routing, true));
}
}

int main(int argc, char** argv)
{
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc != 2)
    return 2;

  const auto activeFixture = readBytes(
    "tests/reference/persistence/fixtures/v2_active.bin");
  const auto v1Fixture = readBytes(
    "tests/reference/persistence/fixtures/v1.bin");
  const auto legacyFixture = readBytes(
    "tests/reference/persistence/fixtures/legacy.bin");
  require("read independent persistence fixtures",
          activeFixture.size() == 520 && v1Fixture.size() == 256 &&
          legacyFixture.size() == 1828);

  std::array<std::uint8_t, 228> expectedPatch {};
  auto expectedRouting = lxr::MakeDisabledRoutingState();
  require("decode independent active fixture",
          decodeRouting(activeFixture, expectedPatch, expectedRouting));

  HMODULE module = LoadLibraryA(argv[1]);
  require("load private VST3", module != nullptr);
  auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
  auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
  auto factoryFn = reinterpret_cast<IPluginFactory* (*)()>(
    GetProcAddress(module, "GetPluginFactory"));
  require("resolve VST3 exports", init && exitDll && factoryFn);
  require("initialize VST3 module", init());
  auto* factory = factoryFn();
  PClassInfo info {};
  bool found = false;
  for (int i = 0; i < factory->countClasses(); ++i)
    if (factory->getClassInfo(i, &info) == kResultOk &&
        std::strcmp(info.category, kVstAudioEffectClass) == 0)
    {
      found = true;
      break;
    }
  require("find VST3 audio class", found);

  {
    HostInstance source(factory, info.cid);
    auto active = activeFixture;
    require("load twelve-route LXRS v2 stream", source.loadState(active));
    check("authority publishes all twelve routes atomically",
          P15M2GStateIntegrationTestProbe::authorityMatches(
            source, expectedRouting));
    source.setup(48000.0);
    processRestoreBoundary(source);
    check("processing projection applies all twelve routes",
          P15M2GStateIntegrationTestProbe::processingProjectionMatches(
            source, expectedRouting));

    const auto saved = source.saveState();
    check("VST3 writes exact 520-byte stream", saved.size() == 520);
    check("deterministic VST3 save equals independent fixture",
          saved == activeFixture);

    std::array<std::uint8_t, 228> savedPatch {};
    auto savedRouting = lxr::MakeDisabledRoutingState();
    require("decode production VST3 output",
            decodeRouting(saved, savedPatch, savedRouting));
    check("full-width route identity survives VST3 boundary",
          savedRouting.routes[3].originalIndex == 135 &&
          savedRouting.routes[4].originalIndex == 169 &&
          savedRouting.routes[5].originalIndex == 203 &&
          savedRouting.routes[9].originalIndex == 135 &&
          savedRouting.routes[10].originalIndex == 169 &&
          savedRouting.routes[11].originalIndex == 203);
    check("all 228 patch values survive active-route restore",
          savedPatch == expectedPatch);

    HostInstance restored(factory, info.cid);
    auto cycle = saved;
    require("reload production VST3 output", restored.loadState(cycle));
    restored.setup(48000.0);
    processRestoreBoundary(restored);
    check("save/reload cycle remains byte deterministic",
          restored.saveState() == saved);
    check("second instance reconstructs processing routes",
          P15M2GStateIntegrationTestProbe::processingProjectionMatches(
            restored, expectedRouting));

    HostInstance isolated(factory, info.cid);
    isolated.setup(48000.0);
    processRestoreBoundary(isolated);
    std::array<std::uint8_t, 228> isolatedPatch {};
    auto isolatedRouting = lxr::MakeDisabledRoutingState();
    require("decode isolated instance state",
            decodeRouting(isolated.saveState(), isolatedPatch,
                          isolatedRouting));
    bool allOff = true;
    for (const auto& route : isolatedRouting.routes)
      allOff &= route.flags == 0;
    check("multi-instance routing isolation", allOff);

    auto corrupt = saved;
    corrupt[300] ^= 0x40;
    check("bad CRC rejected at VST3 boundary", !restored.loadState(corrupt));
    check("failed restore causes no partial mutation",
          restored.saveState() == saved);
  }

  for (const auto* fixture : {&v1Fixture, &legacyFixture})
  {
    HostInstance migrated(factory, info.cid);
    auto bytes = *fixture;
    require("load backward-compatible state", migrated.loadState(bytes));
    std::array<std::uint8_t, 228> patch {};
    auto routing = lxr::MakeDisabledRoutingState();
    require("migration saves production LXRS v2",
            decodeRouting(migrated.saveState(), patch, routing));
    bool allOff = true;
    for (const auto& route : routing.routes)
      allOff &= route.flags == 0;
    check("legacy migration never infers routes", allOff);
  }

  factory->release();
  require("terminate VST3 module", exitDll());
  FreeLibrary(module);
  std::printf("SUMMARY pass=%d fail=%d routes=12 stream=520\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
