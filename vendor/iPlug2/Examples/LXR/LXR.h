#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "LXRParamContract.h"
#include "LXRState.h"
#include "LXRProjectState.h"
#if defined(VST3_API)
#include "LXRKitManagementInterface.h"
#include "LXRMorphManagementInterface.h"
#endif

#include "../../../../../dsp/Engine.h"
#include "../../../../../dsp/FixedFifo.h"
#include "../../../../../dsp/ParameterArray.h"
#include "../../../../../dsp/ParameterStateAuthority.h"
#include "../../../../../dsp/MidiParser.h"
#include "../../../../../dsp/LXRAudioAdapter.h"
#include "../../../../../dsp/RouteHostControls.h"
#include "../../../../../dsp/KitManagement.h"
#include "../../../../../dsp/Morphing.h"
#include "../../../../../dsp/MorphManagement.h"
#include "../../../../../dsp/RuntimeKitLoad.h"
#include "../../../../../dsp/TriggerScheduler.h"
#include "../../../../../dsp/RollHostControls.h"

#include <array>
#include <atomic>

class LXR final : public iplug::Plugin
#if defined(VST3_API)
                , public lxr::vst::IKitManagement
                , public lxr::vst::IMorphManagement
#endif
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

  // Non-audio ingress for an already decoded P17-B canonical whole kit. The
  // call validates and publishes exactly one bounded transaction; callers may
  // retry Busy/Stale explicitly, but this function never spins or performs I/O.
  lxr::kit::RuntimeLoadResult LoadRuntimeKit(
    const lxr::kit::CanonicalKit& kit);

  // Non-audio ingress for one validated original .snd target. The target and
  // amount are deliberately transient in P18-B and never enter LXRS state.
  lxr::morph::LoadResult LoadMorphTarget(
    const lxr::kit::CanonicalKit& kit);
  lxr::morph::management::OperationResult LoadMorphTargetFile(
    const wchar_t* path);
  lxr::morph::management::OperationResult ClearMorphTarget();
  lxr::morph::management::OperationResult ExportMorphedKitFile(
    const wchar_t* path);
  lxr::morph::management::OperationResult CancelMorphOperation(
    lxr::morph::management::Action action) noexcept;
  bool CurrentMorphState(lxr::morph::Snapshot& state) const noexcept;

  // Layer A: supported non-real-time production operations. The editor and
  // exact-VST3 harness use these same entry points; neither performs file I/O
  // or validation inside the processing callback.
  lxr::kit::management::OperationResult ImportKitFile(
    const wchar_t* path);
  lxr::kit::management::OperationResult ExportKitFile(
    const wchar_t* path);
  lxr::kit::management::DisplayName CurrentKitDisplayName() const;
  lxr::kit::management::OperationResult LastKitOperation() const
  {
    return mLastKitOperation;
  }

#if defined(VST3_API)
  Steinberg::tresult PLUGIN_API queryInterface(
    const Steinberg::TUID iid, void** object) override;
  Steinberg::uint32 PLUGIN_API addRef() override
  {
    return iplug::Plugin::addRef();
  }
  Steinberg::uint32 PLUGIN_API release() override
  {
    return iplug::Plugin::release();
  }
  Steinberg::tresult PLUGIN_API importSnd(
    const Steinberg::Vst::TChar* path,
    Steinberg::uint32 pathCharacters,
    lxr::vst::KitOperationWire* result) override;
  Steinberg::tresult PLUGIN_API exportSnd(
    const Steinberg::Vst::TChar* path,
    Steinberg::uint32 pathCharacters,
    lxr::vst::KitOperationWire* result) override;
  Steinberg::tresult PLUGIN_API getKitOperation(
    lxr::vst::KitOperationWire* result) override;
  Steinberg::tresult PLUGIN_API importMorphSnd(
    const Steinberg::Vst::TChar* path,
    Steinberg::uint32 pathCharacters,
    lxr::vst::MorphOperationWire* result) override;
  Steinberg::tresult PLUGIN_API clearMorphTarget(
    lxr::vst::MorphOperationWire* result) override;
  Steinberg::tresult PLUGIN_API exportMorphSnd(
    const Steinberg::Vst::TChar* path,
    Steinberg::uint32 pathCharacters,
    lxr::vst::MorphOperationWire* result) override;
  Steinberg::tresult PLUGIN_API cancelMorphOperation(
    Steinberg::uint32 action,
    lxr::vst::MorphOperationWire* result) override;
  Steinberg::tresult PLUGIN_API getMorphState(
    lxr::vst::MorphOperationWire* result) override;
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
  friend struct P16F1VST3TransportTestProbe;
  friend struct P16F2VST3StateTestProbe;
  friend struct P16F3VST3CertificationProbe;
  friend struct P17DRuntimeKitLoadTestProbe;
  friend struct P17EKitManagementTestProbe;
  friend struct P18BMorphRuntimeTestProbe;
  friend struct P18CMorphManagementTestProbe;

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
  // Sequencer transport and the existing immutable three-slot bank authority.
  lxr::sequencer::TriggerScheduler mSequencer;
  // Declared after scheduler, so this pin releases before its authority dies.
  lxr::sequencer::PatternStateAuthority::ReadHandle mAppliedPattern;
  lxr::sequencer::state::TransportConfig mTransportConfig {}; // outer gate
  lxr::sequencer::state::TransportConfig mAudioTransport {}; // audio owner
  std::array<std::atomic<uint64_t>, 3> mDeferredTransportValues {};
  lxr::sequencer::RollHostControls mRollControls {}; // outer coordinator
  lxr::sequencer::RollHostControls mAudioRollControls {}; // processing owner
  std::array<std::atomic<uint64_t>, 2> mDeferredRollValues {};
  std::atomic<uint64_t> mRollReleaseCutoff {0};
  std::atomic<bool> mRollReleasePending {false};
  std::uint8_t mAppliedRollMask = 0;
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
  uint64_t mAppliedKitGeneration = 0;
  uint64_t mAppliedKitNativeBoundary = 0;

  // Transient, per-instance target B and amount. Source A remains the sole
  // persistent patch+routing authority above. Engine/ParameterArray receive
  // only a derived processing-boundary projection.
  lxr::morph::StateAuthority mMorphState;
  std::atomic<uint64_t> mDeferredMorphAmount {0};
  uint64_t mAppliedMorphGeneration = 0;
  uint64_t mAppliedMorphNativeBoundary = 0;
  lxr::morph::Values mAppliedMorphProjection {};
  lxr::morph::management::OperationResult mLastMorphOperation {};
  std::array<char, 1024> mLastMorphOperationPath {};

  // Main-thread/non-real-time kit-management presentation state. The musical
  // authority remains mState+mRoutingState; these fields never feed rendering.
  lxr::kit::management::OperationResult mLastKitOperation {};
  std::array<char, 1024> mLastKitOperationPath {};

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

  // VST3 extracts all MIDI before ProcessBlock. Keep valid callback-relative
  // offsets until the host sample is reached; native PCM remains 32-frame
  // quantized by the unchanged adapter. Sequence breaks equal-offset ties.
  struct CallbackMidi {
    iplug::IMidiMsg message;
    std::uint32_t sequence = 0;
  };
  std::array<CallbackMidi, kDeferredMidiCapacity> mCallbackMidi {};
  std::size_t mCallbackMidiCount = 0;
  int mCallbackMidiFrames = 0;
  bool mCollectCallbackMidi = false;
  std::atomic<uint64_t> mInvalidMidiOffsets {0};
  std::atomic<uint64_t> mRejectedParameterQueues {0};
  std::atomic<uint64_t> mRejectedMidiInputRecords {0};
  std::atomic<uint64_t> mMalformedMidiInputCounts {0};
  std::uint32_t mCallbackMidiPeak = 0; // processing owner; read only while stopped
  std::uint64_t mDispatchedMidiMessages = 0; // successful parser dispatches

  // IDs 240--242 project LXRS v3 transport configuration. Legacy state supplies
  // OFF/Internal/120 defaults; these remain outside the frozen LXRS v2 format.
  std::atomic<std::uint8_t> mSequencerEnabled {0u};
  std::atomic<std::uint8_t> mSequencerClockMode {0u};
  std::atomic<std::uint16_t> mSequencerInternalTempo {120u};
  // Captured directly from the callback-scoped VST3 ProcessContext. The
  // pinned iPlug2 ITimeInfo drops individual validity flags and can retain
  // stale optional values, so Host Sync deliberately does not consume it.
  lxr::sequencer::HostTransportContext mHostProcessContext {};
  bool mAppliedSequencerEnabled = false;
  std::uint8_t mAppliedSequencerClockMode = 0u;
  std::uint16_t mAppliedSequencerInternalTempo = 120u;

  // Reset Engine and adapter state, then reconstruct the Engine from
  // one coherent authoritative state snapshot.
  void ResetEngineState();
  bool ApplyPendingStateAndParameters();
  bool FlushDeferredMorphAmount(
    uint64_t maximumOrder = UINT64_MAX);
  bool FlushDeferredHostParameters(
    uint64_t maximumOrder = UINT64_MAX);
  bool TryPersistentSnapshot(
    StateAuthority::Values& parameters,
    uint64_t& parameterGeneration,
    lxr::RoutingStateAuthority::Snapshot& routing,
    lxr::sequencer::PatternStateAuthority::ReadHandle* pattern = nullptr,
    lxr::sequencer::state::TransportConfig* transport = nullptr,
    lxr::sequencer::RollHostControls* rolls = nullptr) const;
  bool PublishPersistentRestore(
    const StateAuthority::Values& parameters,
    const lxr::PersistentRoutingState& routing,
    const std::array<std::uint8_t, 8>& kitName,
    bool kitIdentityPresent,
    const lxr::sequencer::PatternBank* bank = nullptr,
    const lxr::sequencer::state::TransportConfig* transport = nullptr,
    uint64_t expectedJointGeneration = UINT64_MAX);
  bool ImportSequencerBank(const char* path);
  bool SnapshotCurrentKit(
    lxr::kit::CanonicalKit& kit,
    std::array<lxr::PersistentRouteRecord,
               lxr::kPersistentRouteCount>& routes) const;
  void SetLastKitOperationPath(const char* path) noexcept;
#if defined(VST3_API)
  static void CopyKitOperationWire(
    const lxr::kit::management::OperationResult& source,
    lxr::vst::KitOperationWire& destination) noexcept;
  static void CopyMorphOperationWire(
    const lxr::morph::management::OperationResult& source,
    lxr::vst::MorphOperationWire& destination) noexcept;
  static bool CopyKitOperationPath(
    const Steinberg::Vst::TChar* source,
    Steinberg::uint32 characters,
    std::array<wchar_t, lxr::kit::snd::kPathCapacity>& destination) noexcept;
#endif
#if IPLUG_EDITOR
  void RefreshKitManagementControls();
  void RefreshMorphManagementControls();
#endif
  void SetLastMorphOperationPath(const char* path) noexcept;
  bool StageCompleteRestore(
    const StateAuthority::Values& parameters,
    const lxr::PersistentRoutingState& routing);
  bool PrepareCompleteRestore(
    const StateAuthority::Values& parameters,
    const lxr::PersistentRoutingState& routing);
  void CommitCompleteRestore(
    const lxr::PersistentRoutingState& routing) noexcept;
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
  void FlushCallbackMidi();
  bool EnqueueDeferredMidi(const iplug::IMidiMsg& msg) noexcept;
  void DrainDeferredMidi();
  void ApplySequencerTransport(int callbackFrames) noexcept;
  void ApplyRollControls() noexcept;
  void ReleaseRollControls() noexcept;
};
