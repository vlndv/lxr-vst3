// P15-M2H real VST3 controller, automation and route-state integration.
// Original target identities Copyright 2013 Julian Schmidt; LICENSE-LXR.txt.
#include "../vendor/iPlug2/Examples/LXR/LXR.h"

#define main ExistingP12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main

#include <atomic>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

struct P15M2HHostControlsTestProbe
{
  using RouteGuard = lxr::RoutingStateAuthority::Guard;

  static LXR& plugin(HostInstance& host)
  {
    return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(
      static_cast<SingleComponentEffect*>(host.component)));
  }

  static void idle(HostInstance& host)
  {
    // Dispatch through the framework base vtable: the implementation lives in
    // the separately loaded private DLL and is deliberately not linked here.
    auto* delegate = static_cast<iplug::IPlugAPIBase*>(&plugin(host));
    delegate->OnIdle();
  }

  static bool snapshot(HostInstance& host,
                       lxr::RoutingStateAuthority::Snapshot& result)
  {
    auto& authority = plugin(host).mRoutingState;
    RouteGuard guard;
    return authority.tryAcquire(guard) && authority.copyLocked(guard, result);
  }

  static bool route(HostInstance& host, std::size_t slot,
                    std::uint16_t token)
  {
    lxr::RoutingStateAuthority::Snapshot state;
    return snapshot(host, state) && slot < state.state.routes.size() &&
      lxr::route_host::encode(state.state.routes[slot]).value() == token;
  }

  static bool applied(HostInstance& host, std::size_t slot,
                      std::uint16_t token)
  {
    const auto& routes = plugin(host).mAppliedRouting.routes;
    return slot < routes.size() &&
      lxr::route_host::encode(routes[slot]).value() == token;
  }

  static bool basePatchMatches(HostInstance& host,
                               const std::array<std::uint8_t, 228>& expected)
  {
    auto& instance = plugin(host);
    for (std::size_t i = 0; i < expected.size(); ++i)
      if (instance.mState.value(i) != expected[i] ||
          instance.mParams.values[i] != expected[i])
        return false;
    return true;
  }

  static bool hold(HostInstance& host, RouteGuard& guard)
  {
    return plugin(host).mRoutingState.tryAcquire(guard);
  }

  static void release(HostInstance& host, RouteGuard& guard)
  {
    plugin(host).mRoutingState.release(guard);
  }

  static bool bypass(HostInstance& host) { return plugin(host).GetBypassed(); }
};

namespace
{
constexpr std::array<std::uint16_t, 12> kFixtureTokens {{
  34, 68, 102, 135, 169, 203, 34, 68, 102, 135, 169, 203
}};

std::vector<std::uint8_t> readBytes(const char* path)
{
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

std::string ascii(const TChar* text)
{
  std::string result;
  for (std::size_t i = 0; text != nullptr && text[i] != 0; ++i)
    result.push_back(text[i] <= 127 ? static_cast<char>(text[i]) : '?');
  return result;
}

double normalized(IEditController& controller, ParamID id,
                  std::uint32_t token)
{
  return controller.plainParamToNormalized(id, static_cast<double>(token));
}

bool decodeState(const std::vector<std::uint8_t>& bytes,
                 std::array<std::uint8_t, 228>& patch,
                 lxr::PersistentRoutingState& routes)
{
  return static_cast<bool>(lxr::state::DecodePersistentState(
    bytes.data(), bytes.size(), patch, routes, true));
}

void processBoundary(HostInstance& host)
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
  require("route processing boundary", host.processor->process(data) == kResultOk);
}

bool processRouteRaw(HostInstance& host, ParamID id, double value)
{
  ParameterChanges changes(1);
  int32 queueIndex = 0, pointIndex = 0;
  auto* queue = changes.addParameterData(id, queueIndex);
  if (queue == nullptr || queue->addPoint(0, value, pointIndex) != kResultOk)
    return false;
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
  data.inputParameterChanges = &changes;
  return host.processor->process(data) == kResultOk;
}

bool controllerToken(HostInstance& host, ParamID id, std::uint16_t token)
{
  const double actual = host.controller->getParamNormalized(id);
  return host.controller->normalizedParamToPlain(id, actual) == token;
}

bool trySaveState(HostInstance& host, std::vector<std::uint8_t>& bytes)
{
  MemoryStream state;
  if (host.component->getState(&state) != kResultOk)
    return false;
  bytes.assign(reinterpret_cast<std::uint8_t*>(state.getData()),
               reinterpret_cast<std::uint8_t*>(state.getData()) +
                 state.getSize());
  return true;
}

void setController(HostInstance& host, ParamID id, std::uint16_t token)
{
  require("accepted controller route edit",
          host.controller->setParamNormalized(
            id, normalized(*host.controller, id, token)) == kResultOk);
}

void setAutomation(HostInstance& host, ParamID id, std::uint16_t token)
{
  host.processParameter(id, normalized(*host.controller, id, token));
  P15M2HHostControlsTestProbe::idle(host);
}

bool allRoutesOff(const lxr::PersistentRoutingState& routes)
{
  for (const auto& route : routes.routes)
    if (route.flags != 0)
      return false;
  return true;
}

void registrationAndNormalization(HostInstance& host)
{
  const std::array<const char*, 12> names {{
    "Velocity Destination D1", "Velocity Destination D2",
    "Velocity Destination D3", "Velocity Destination SN",
    "Velocity Destination CY", "Velocity Destination HH",
    "LFO Destination D1", "LFO Destination D2",
    "LFO Destination D3", "LFO Destination SN",
    "LFO Destination CY", "LFO Destination HH"
  }};
  int found = 0;
  for (int32 index = 0; index < host.controller->getParameterCount(); ++index)
  {
    ParameterInfo info {};
    require("read VST3 parameter info",
            host.controller->getParameterInfo(index, info) == kResultOk);
    if (info.id < 228 || info.id > 239)
      continue;
    const auto slot = static_cast<std::size_t>(info.id - 228);
    ++found;
    check("stable route title", slot < names.size() &&
          ascii(info.title) == names[slot]);
    check("integer step count 65535", info.stepCount == 65535);
    check("explicit normalized OFF default", info.defaultNormalizedValue == 0.0);
    check("route parameter automatable",
          (info.flags & ParameterInfo::kCanAutomate) != 0);
  }
  check("exactly twelve public route IDs", found == 12);

  bool doubles = true, floats = true;
  for (ParamID id = 228; id <= 239; ++id)
    for (std::uint32_t code = 0; code <= 65535u; ++code)
    {
      const double value = normalized(*host.controller, id, code);
      doubles &= host.controller->normalizedParamToPlain(id, value) == code;
      const double rounded = static_cast<double>(static_cast<float>(value));
      floats &= host.controller->normalizedParamToPlain(id, rounded) == code;
    }
  check("all 65536 double selector codes round-trip for all routes", doubles);
  check("all 65536 float-rounded selector codes round-trip for all routes", floats);
  for (const std::uint32_t boundary : {0u, 1u, 127u, 128u, 203u, 204u, 65535u})
    check("normalization boundary exact",
          host.controller->normalizedParamToPlain(
            228, normalized(*host.controller, 228, boundary)) == boundary);

  String128 display {};
  check("OFF display formatting",
        host.controller->getParamStringByValue(228, 0.0, display) == kResultOk &&
        ascii(display) == "Off");
  check("accepted display formatting",
        host.controller->getParamStringByValue(
          239, normalized(*host.controller, 239, 203), display) == kResultOk &&
        ascii(display).find("AVR 203") != std::string::npos);
  check("unsupported display formatting",
        host.controller->getParamStringByValue(
          239, normalized(*host.controller, 239, 204), display) == kResultOk &&
        ascii(display).find("Unavailable") != std::string::npos);
  check("reserved display formatting",
        host.controller->getParamStringByValue(
          239, normalized(*host.controller, 239, 65535), display) == kResultOk &&
        ascii(display).find("Reserved") != std::string::npos);
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
  require("read independent route fixtures",
          activeFixture.size() == 520 && v1Fixture.size() == 256 &&
          legacyFixture.size() == 1828);

  HMODULE module = LoadLibraryA(argv[1]);
  require("load exact private VST3", module != nullptr);
  auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
  auto exitDll = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
  auto factoryFn = reinterpret_cast<IPluginFactory* (*)()>(
    GetProcAddress(module, "GetPluginFactory"));
  require("resolve private VST3 exports", init && exitDll && factoryFn);
  require("initialize private VST3", init());
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
  require("find LXR audio class", found);

  std::vector<std::uint8_t> activeState;
  AudioRender hostActiveAudio;
  {
    HostInstance host(factory, info.cid);
    registrationAndNormalization(host);
    host.setup(48000.0);

    // Direct controller edits and processor automation both converge through
    // the same routing authority and processing-boundary projection.
    setController(host, 228, 34);
    processBoundary(host);
    P15M2HHostControlsTestProbe::idle(host);
    check("controller edit publishes authoritative route",
          P15M2HHostControlsTestProbe::route(host, 0, 34));
    check("controller edit reaches Engine boundary",
          P15M2HHostControlsTestProbe::applied(host, 0, 34));
    check("controller readback accepted token", controllerToken(host, 228, 34));

    const auto editsBeforeReject = host.handler.edits;
    check("invalid direct controller token rejected",
          host.controller->setParamNormalized(
            228, normalized(*host.controller, 228, 204)) == kResultFalse);
    P15M2HHostControlsTestProbe::idle(host);
    check("invalid direct token leaves authority unchanged",
          P15M2HHostControlsTestProbe::route(host, 0, 34));
    check("invalid direct token reconciles controller", controllerToken(host, 228, 34));
    check("rejection emits no automation gesture", host.handler.edits == editsBeforeReject);

    setAutomation(host, 228, 31);
    check("processor automation publishes route",
          P15M2HHostControlsTestProbe::route(host, 0, 31));
    check("processor automation updates Engine",
          P15M2HHostControlsTestProbe::applied(host, 0, 31));
    check("processor automation controller readback", controllerToken(host, 228, 31));
    processRouteRaw(host, 228, normalized(*host.controller, 228, 204));
    P15M2HHostControlsTestProbe::idle(host);
    check("invalid automation leaves native route unchanged",
          P15M2HHostControlsTestProbe::route(host, 0, 31) &&
          P15M2HHostControlsTestProbe::applied(host, 0, 31));
    check("invalid automation reconciles controller", controllerToken(host, 228, 31));

    bool everyValidAccepted = true;
    for (std::size_t slot = 0; slot < 12; ++slot)
      for (std::uint16_t token = 1; token <= 204; ++token)
      {
        const auto decoded = lxr::route_host::decode(
          lxr::route_host::parameterForSlot(slot),
          lxr::route_host::RouteToken16::fromRaw(token));
        if (decoded.status != lxr::route_host::TokenStatus::Accepted)
          continue;
        everyValidAccepted &= host.controller->setParamNormalized(
          static_cast<ParamID>(228 + slot),
          normalized(*host.controller, static_cast<ParamID>(228 + slot), token)) == kResultOk;
        everyValidAccepted &= P15M2HHostControlsTestProbe::route(host, slot, token);
      }
    processBoundary(host);
    P15M2HHostControlsTestProbe::idle(host);
    check("every verified source/voice route accepted", everyValidAccepted);

    for (std::size_t slot = 0; slot < kFixtureTokens.size(); ++slot)
      setAutomation(host, static_cast<ParamID>(228 + slot), kFixtureTokens[slot]);
    activeState = host.saveState();
    check("host route state remains exact 520 bytes", activeState.size() == 520);
    std::array<std::uint8_t, 228> patch {};
    auto routes = lxr::MakeDisabledRoutingState();
    require("decode host-authored twelve-route state",
            decodeState(activeState, patch, routes));
    bool allTwelve = true;
    for (std::size_t slot = 0; slot < routes.routes.size(); ++slot)
      allTwelve &= lxr::route_host::encode(routes.routes[slot]).value() ==
                   kFixtureTokens[slot];
    check("all twelve host assignments serialize losslessly", allTwelve);
    check("saved base patch remains authoritative",
          P15M2HHostControlsTestProbe::basePatchMatches(host, patch));
    check("48 kHz PDC remains 26 samples", host.processor->getLatencySamples() == 26);

    // Same-field and different-field combinations exercise established
    // base -> velocity -> LFO processing order without changing equations.
    setAutomation(host, 228, 31);
    setAutomation(host, 234, 31);
    check("same-field velocity and LFO routes accepted",
          P15M2HHostControlsTestProbe::route(host, 0, 31) &&
          P15M2HHostControlsTestProbe::route(host, 6, 31));
    setAutomation(host, 234, 2);
    check("different-field velocity and LFO routes accepted",
          P15M2HHostControlsTestProbe::route(host, 0, 31) &&
          P15M2HHostControlsTestProbe::route(host, 6, 2));
  }

  {
    HostInstance restored(factory, info.cid);
    auto bytes = activeState;
    require("restore host-authored LXRS v2", restored.loadState(bytes));
    P15M2HHostControlsTestProbe::idle(restored);
    bool readback = true;
    for (std::size_t slot = 0; slot < kFixtureTokens.size(); ++slot)
      readback &= controllerToken(restored, static_cast<ParamID>(228 + slot),
                                  kFixtureTokens[slot]);
    check("LXRS v2 restore synchronizes all controller routes", readback);
    restored.setup(48000.0);
    processBoundary(restored);
    bool applied = true;
    for (std::size_t slot = 0; slot < kFixtureTokens.size(); ++slot)
      applied &= P15M2HHostControlsTestProbe::applied(
        restored, slot, kFixtureTokens[slot]);
    check("LXRS v2 restore applies all native routes", applied);
    check("LXRS v2 save/reload deterministic", restored.saveState() == activeState);
    hostActiveAudio = renderD1(restored);

    auto corrupt = activeState;
    corrupt[300] ^= 0x40;
    check("corrupt LXRS v2 rejected", !restored.loadState(corrupt));
    check("corrupt restore causes no partial patch or route mutation",
          restored.saveState() == activeState);
  }

  for (const auto* fixture : {&v1Fixture, &legacyFixture})
  {
    HostInstance migrated(factory, info.cid);
    auto bytes = *fixture;
    require("load backward-compatible project state", migrated.loadState(bytes));
    P15M2HHostControlsTestProbe::idle(migrated);
    std::array<std::uint8_t, 228> patch {};
    auto routes = lxr::MakeDisabledRoutingState();
    require("decode migrated project state",
            decodeState(migrated.saveState(), patch, routes));
    bool controllerOff = true;
    for (ParamID id = 228; id <= 239; ++id)
      controllerOff &= controllerToken(migrated, id, 0);
    check("legacy migration keeps every native route OFF",
          allRoutesOff(routes) && controllerOff);
    check("legacy bypass semantics preserved",
          routes.bypass == P15M2HHostControlsTestProbe::bypass(migrated));
  }

  {
    HostInstance stale(factory, info.cid);
    stale.setup(48000.0);
    auto offState = stale.saveState();
    P15M2HHostControlsTestProbe::RouteGuard held;
    require("hold routing publication gate",
            P15M2HHostControlsTestProbe::hold(stale, held));
    check("route automation remains bounded under contention",
          processRouteRaw(stale, 228, normalized(*stale.controller, 228, 34)));
    P15M2HHostControlsTestProbe::release(stale, held);
    require("restore supersedes deferred route automation",
            stale.loadState(offState));
    processBoundary(stale);
    P15M2HHostControlsTestProbe::idle(stale);
    check("stale pre-restore route cannot resurrect",
          P15M2HHostControlsTestProbe::route(stale, 0, 0) &&
          controllerToken(stale, 228, 0));

    std::atomic<bool> run {true}, ready {false};
    std::atomic<int> processed {0}, processFailures {0};
    const double concurrentValue = normalized(*stale.controller, 228, 34);
    std::thread audio([&] {
      ready.store(true, std::memory_order_release);
      while (run.load(std::memory_order_acquire))
      {
        if (!processRouteRaw(stale, 228, concurrentValue))
          ++processFailures;
        ++processed;
        std::this_thread::yield();
      }
    });
    for (unsigned attempt = 0;
         attempt < 100000u &&
           (!ready.load(std::memory_order_acquire) || processed.load() == 0);
         ++attempt)
      std::this_thread::yield();
    int snapshots = 0, snapshotBusy = 0, restores = 0;
    for (int i = 0; i < 40; ++i)
    {
      std::vector<std::uint8_t> saved;
      if (trySaveState(stale, saved))
      {
        std::array<std::uint8_t, 228> patch {};
        auto routes = lxr::MakeDisabledRoutingState();
        if (saved.size() == 520 && decodeState(saved, patch, routes))
          ++snapshots;
      }
      else
        ++snapshotBusy;
      auto restore = offState;
      if (stale.loadState(restore))
        ++restores;
    }
    run.store(false, std::memory_order_release);
    audio.join();
    auto finalRestore = offState;
    const bool restoredAfterContention = stale.loadState(finalRestore);
    std::printf("CONCURRENCY processed=%d process_failures=%d snapshots=%d snapshot_busy=%d restores=%d final_restore=%d\n",
                processed.load(), processFailures.load(), snapshots,
                snapshotBusy, restores, restoredAfterContention ? 1 : 0);
    check("concurrent automation/save/restore remains bounded and coherent",
          processed.load() > 0 && processFailures.load() == 0 &&
          snapshots > 0 && snapshots + snapshotBusy == 40 &&
          restores > 0 && restores <= 40 &&
          restoredAfterContention);
  }

  {
    HostInstance first(factory, info.cid), second(factory, info.cid);
    first.setup(48000.0); second.setup(48000.0);
    setAutomation(first, 228, 34);
    setAutomation(second, 228, 0);
    check("multiple instances keep independent route authorities",
          P15M2HHostControlsTestProbe::route(first, 0, 34) &&
          P15M2HHostControlsTestProbe::route(second, 0, 0));
  }

  // Host-authored state must reconstruct identical PCM on a clean instance.
  {
    HostInstance replay(factory, info.cid);
    auto bytes = activeState;
    require("restore state for deterministic host audio", replay.loadState(bytes));
    replay.setup(48000.0);
    processBoundary(replay);
    const auto replayed = renderD1(replay);
    check("host-routed pre/post-reload PCM exact",
          replayed.left == hostActiveAudio.left &&
          replayed.right == hostActiveAudio.right &&
          replayed.peak == hostActiveAudio.peak &&
          replayed.rms == hostActiveAudio.rms);
  }

  factory->release();
  require("terminate private VST3", exitDll());
  FreeLibrary(module);
  std::printf("SUMMARY pass=%d fail=%d ids=12 codes=65536 routes=107 stream=520\n",
              passed, failed);
  return failed == 0 ? 0 : 1;
}
