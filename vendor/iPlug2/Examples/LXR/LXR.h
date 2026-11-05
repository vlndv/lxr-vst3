#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "LXRParamContract.h"
#include "LXRState.h"

#include "../../../../../dsp/Engine.h"
#include "../../../../../dsp/FixedFifo.h"
#include "../../../../../dsp/ParameterArray.h"
#include "../../../../../dsp/ParameterStateAuthority.h"
#include "../../../../../dsp/MidiParser.h"
#include "../../../../../dsp/LXRAudioAdapter.h"
#include "../../../../../dsp/RouteHostControls.h"

#include <array>
#include <atomic>

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

  void OnParamChangeUI(
    int paramIdx,
    iplug::EParamSource source
  ) override;

  bool SerializeState(
    iplug::IByteChunk& chunk
  ) const override;

  int UnserializeState(
    const iplug::IByteChunk& chunk,
    int startPos
  ) override;

  void OnIdle() override;

#if defined(VST3_API)
  Steinberg::tresult PLUGIN_API setParamNormalized(
    Steinberg::Vst::ParamID tag,
    Steinberg::Vst::ParamValue value) override;
  Steinberg::tresult PLUGIN_API setState(
    Steinberg::IBStream* state) override;
  Steinberg::tresult PLUGIN_API getState(
    Steinberg::IBStream* state) override;
  Steinberg::tresult PLUGIN_API process(
    Steinberg::Vst::ProcessData& data) override;
  Steinberg::tresult PLUGIN_API setComponentHandler(
    Steinberg::Vst::IComponentHandler* handler) override;
#endif

private:
  // Test-only access to the exact loaded component; adds no ABI or runtime API.
  friend struct LXRControllerSyncTestProbe;
  friend struct P15M2GStateIntegrationTestProbe;
  friend struct P15M2HHostControlsTestProbe;

  // Native LXR synthesis/DSP engine.
  lxr::Engine mEngine;
  // Inactive fixed staging bank. Complete restore candidates are constructed
  // here before the processing owner selects them.
  lxr::Engine mStagingEngine;

  // Canonical 228-entry raw LXR parameter state.
  lxr::ParameterArray mParams;
  lxr::ParameterArray mStagingParams;

  // Native LXR MIDI parser and dispatcher.
  lxr::MidiParser mMidiParser;

  // Host-independent bridge around the Engine's fixed 32-frame,
  // four-output rendering contract.
  lxr::LXRAudioAdapter mAudioAdapter;
  bool mAssetsLoaded = false;

  using StateAuthority =
    lxr::ParameterStateAuthority<lxr::state::kParameterCount>;

  // The sole logical parameter authority. IParam mirrors host-visible values;
  // ParameterArray and Engine are processing-thread projections.
  StateAuthority mState;
  // Separate fixed routing authority and shared patch+routing coordinator.
  mutable lxr::RoutingStateAuthority mRoutingState;
  lxr::PersistentRoutingState mAppliedRouting =
    lxr::MakeDisabledRoutingState();
  uint64_t mAppliedRestoreGeneration = 0; // Routing restore epoch.
  uint64_t mAppliedStateGeneration = 0;
  uint64_t mAppliedJointGeneration = 0;

  // A host/UI parameter callback that overlaps the bounded writer gate stores
  // its latest value here. Bit 8 marks a pending 7-bit value. Processing drains
  // all mailboxes as one explicitly defined boundary generation.
  std::array<std::atomic<uint64_t>, lxr::state::kParameterCount>
    mDeferredHostValues {};
  // Route proxy mailboxes use bit16 as present, bits0..15 as the exact token,
  // and bits17..63 as the monotonic order ticket.
  std::array<std::atomic<uint64_t>, lxr::route_host::kParameterCount>
    mDeferredRouteValues {};
  std::array<std::atomic<uint64_t>, lxr::route_host::kParameterCount>
    mDeferredRouteRejections {};
  std::atomic<uint64_t> mDeferredHostOrder {1};
  std::atomic<uint64_t> mRestoreOrderCutoff {0};

  // Main-thread-only acknowledgement of authority generations projected into
  // the separate VST3 controller cache. The authority itself retains pending
  // values; no audio-to-main notification queue can overflow or lose an edit.
  std::array<uint64_t, lxr::vst::kNumParams>
    mControllerSyncGenerations {};
  bool mControllerNeedsReconcile = true;
  bool mControllerSyncBusy = false;
  uint64_t mControllerConnection = 0;

  // VST3 invokes ProcessMidiMsg() serially on the processing thread before
  // ProcessBlock(). Events that meet an in-progress restore are retained in
  // this fixed FIFO and replayed after that restore. Overflow rejects the new
  // event without changing host or native state and is observable in tests.
  static constexpr std::size_t kDeferredMidiCapacity = 4096;
  lxr::FixedFifo<iplug::IMidiMsg, kDeferredMidiCapacity> mDeferredMidi;
  std::atomic<uint64_t> mDeferredMidiOverflows {0};

  // Reset Engine and adapter state, then reconstruct the Engine from
  // one coherent authoritative state snapshot.
  void ResetEngineState();
  bool ApplyPendingStateAndParameters();
  bool FlushDeferredHostParameters(
    uint64_t maximumOrder = UINT64_MAX);
  bool TryPersistentSnapshot(
    StateAuthority::Values& parameters,
    uint64_t& parameterGeneration,
    lxr::RoutingStateAuthority::Snapshot& routing) const;
  bool PublishPersistentRestore(
    const StateAuthority::Values& parameters,
    const lxr::PersistentRoutingState& routing);
  bool StageCompleteRestore(
    const StateAuthority::Values& parameters,
    const lxr::PersistentRoutingState& routing);
  bool ApplyRoutesToEngine(
    lxr::Engine& engine,
    const std::array<lxr::PersistentRouteRecord,
                     lxr::kPersistentRouteCount>& routes) noexcept;
  bool ConfigureRouteForTest(
    const lxr::PersistentRouteRecord& route,
    uint64_t expectedEpoch) noexcept;
  bool PublishRouteHostToken(
    std::size_t routeSlot,
    lxr::route_host::RouteToken16 token,
    uint64_t ticket) noexcept;
  bool RejectRouteHostValue(
    std::size_t routeSlot,
    uint64_t ticket) noexcept;
  void DeferRouteHostToken(
    std::size_t routeSlot,
    lxr::route_host::RouteToken16 token,
    uint64_t ticket) noexcept;
  bool ProcessMidiMsgNow(const iplug::IMidiMsg& msg);
  bool EnqueueDeferredMidi(const iplug::IMidiMsg& msg) noexcept;
  void DrainDeferredMidi();
};
