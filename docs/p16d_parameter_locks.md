# P16-D firmware-faithful parameter locks

## Scope and result

P16-D integrates the two stored automation pairs in every seven-byte sequencer
step. It does not add pattern serialization, pattern chaining, host transport or
tempo synchronization, presets, morphing, or a GUI.

The implementation keeps the VST3 patch and persistent LXRS v2 routing state as
the only durable authorities. A sequencer lock is an Engine-local effective
write. It never changes `ParameterArray::values`, `ParameterStateAuthority<228>`,
the twelve routing records, IParam state, or host automation gestures.

## Authoritative firmware sequence

The pinned source is Sonic Potions/LXR revision
`dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`.

| Finding | Original source | Exact behavior | Port path | Verification |
|---|---|---|---|---|
| Tick acceptance | `Sequencer/sequencer.c:488-544` | Track order 0--6; main gate, active byte, then inclusive cluster probability | `TriggerScheduler::generateEvents` | gate, inactive and probability lifetime cases |
| Per-track order | `Sequencer/sequencer.c:321-354` | Parse automation before triggering that track | `TriggerScheduler::processNativeBlock` | event order and PCM cases |
| Slot dispatch | `Sequencer/sequencer.c:321-328` | Set destinations 1 then 2; update values 1 then 2 | `Engine::applySequencerAutomationPair` | unchanged original-C differential |
| Old destination | `DSPAudio/automationNode.c:44-63` | Restore the current stored base before replacing the destination | Engine raw authoritative mirror | newest-base restoration case |
| New value | `DSPAudio/automationNode.c:66-79` | Send value only when destination is not `NO_AUTOMATION` | Engine effective setter | one/two-slot cases |
| Lower selector offset | `MIDI/frontPanelParser.c:453-478` | Nonzero lower front-panel parameter is incremented before storage | `DecodeAutomationSlot` | selector 2 and 127 boundaries |
| Upper selector | `DSPAudio/automationNode.c:50-55,70-75` | Values above 127 use CC2 and keep the firmware parameter identity | `DecodeAutomationSlot` | selector 128 and upper cases |
| Stored base | `MIDI/MidiParser.c:157-168,802-812` | Ordinary input updates `midiParser_originalCcValues`; automation replay does not | `ParameterArray::set` plus Engine mirror | base immutability/update/release |
| Stop/start | `Sequencer/sequencer.c:801-845` | Transport state resets; automation nodes and effective fields are not cleared | scheduler stop/start | explicit lifetime case |

The original parser mutates live firmware fields. The VST3 adaptation instead
reuses the same raw-to-physical setter for a transient effective write. This is
the required separation between source-faithful sound behavior and durable host
state.

## Selector identity

`param1Nr` and `param2Nr` are original automation/MIDI selectors, not flat VST3
parameter IDs.

| Stored byte | Meaning | Candidate patch ID | Result |
|---:|---|---:|---|
| 0 | Front-panel `PAR_NONE` | none | Clear that slot |
| 1 | Original CC1/mod-wheel no-op | 0 | Reject without changing the slot |
| 2--127 | Direct lower CC | selector - 1 | Validate against the typed original contract |
| 128--227 | Direct CC2 identity | selector | Validate against the typed original contract |
| 228--254 | Outside the 228-byte original patch | none | Reject |
| 255 | `NO_AUTOMATION` sentinel | none | Clear that slot |

There is no stored selector for patch ID 127: lower selector 128 begins the CC2
space and maps to ID 128. Host-only routing IDs 228--239 are never decoded from
step data.

An apply command is accepted only when P15-M2A identifies a real original
firmware target with `TypedField` and `DirectStm32` behavior and the current
`ParameterArray` has a live setter. The following categories reject safely:

- MIDI protocol/reserved/no-op parameters 0, 5, 97, 98 and 127;
- mutable velocity/LFO destination selectors 149--154 and 161--172;
- MIDI note-filter fields 221--227, which are valid firmware identities but are
  not implemented in the current native plugin runtime;
- missing table entries, host-only controls and out-of-range bytes.

The six audio-output fields 215--220 are original internal-message targets, not
normal front-panel automation-menu rows. They remain representable by raw step
data and have direct typed runtime setters. No track-to-voice restriction is
invented: the firmware dispatches the selected global parameter regardless of
which track owns the step.

Invalid commands never change a slot destination or effective field. They do
not suppress the otherwise accepted trigger. Rejection and capacity counters
are saturating diagnostics.

## Event model and bounded ordering

`AutomationEvent` contains only the native boundary, clock ordinal, pattern,
track, synthesis owner, physical position, slot, validated command, snapshot
generation and stable order. It contains no DSP pointer.

At the maximum ordinary density, one native boundary can accept seven tracks
and fourteen automation operations. `AutomationEventBatch` therefore has fixed
capacity 14, while the P16-C trigger batch remains capacity 7. Both use a
deterministic reject-newest overflow policy and explicit counters. At 255 BPM a
physical-position interval is still longer than one 32-frame native block, so
the clock contract cannot create two ordinary positions in one block.

One P16-B three-slot snapshot remains pinned while all step bytes are read. The
event batch copies the coherent validated values and generation before the pin
is released. Dispatch then follows:

1. native physical-position boundary;
2. track 0 through 6;
3. restore/set slot 1 destination;
4. restore/set slot 2 destination;
5. write slot 1 value;
6. write slot 2 value;
7. trigger that track through the existing velocity-aware Engine path.

This detail matters for collisions. Slot 2 wins when both slots newly write the
same target. A later track wins when several tracks write the same target at one
boundary. The original does not recompute all still-pointing nodes: changing or
clearing one node can restore the authoritative base even if another node still
names that field. P16-D preserves that sequential quirk.

## Lifetime

An accepted step parses both slots. An off/sentinel slot restores its previous
destination from the newest authoritative base and becomes clear. Therefore an
accepted unlocked step clears the corresponding prior node.

No slot parsing occurs when the main gate is closed, the step is inactive, or
probability rejects it. Existing effective automation remains in those cases.
Scheduler stop, restart and stopped pattern selection also preserve Engine lock
state, matching the source's retained automation nodes. Pattern chaining and
host-transport-dependent lifetime remain deferred.

`Engine::init` clears all fourteen node destinations and the raw-base mirror.
The normal plugin complete-restore path then replays all 228 authoritative patch
bytes into the new Engine, so reset and project-state restoration cannot retain
old transient locks. Partial host/MIDI patch updates do not clear slot identity.
They update the authoritative raw mirror and live effective field immediately;
the newer base wins, and a later node release restores that newest byte. There
is no obsolete saved-value restoration operation.

## Velocity and LFO layering

The mapping switch was factored into `ApplyRawParameterToEngine`. Normal
`ParameterArray::set` still stores the authoritative byte and now also refreshes
the Engine's raw mirror. Sequencer writes call only the effective mapping
function and refresh the existing physical-base registry; they do not call the
authoritative setter.

At the native render tick the existing order remains:

`authoritative patch -> sequencer effective value -> velocity replay -> LFO replay -> filter coefficient repair -> rendering`

The overlay does not add a second modulation equation. It supplies the physical
base from which the already-certified velocity and LFO equations operate. The
P15-M2C and P15-M2E full original-C PCM regressions remain exact after this
change.

## Timing and PCM

P16-C's next-native-block boundary is unchanged. The scheduler runs immediately
before `Engine::processBlock` for the block beginning at that boundary. The
automation pair is effective before the same track's trigger and before the
block's velocity/LFO replay. Changes after a native block has been rendered do
not alter its cached 32 samples.

O0 and O2 tests demonstrate native PCM changes for drum-1 coarse pitch, filter
cutoff, volume, two different slot targets and duplicate-target precedence.
Across the matrix the first observed changed sample is native sample 1. The
volume, pitch and cutoff cases all render through the real Engine; no test is
satisfied by inspecting only the overlay state.

## MIDI, state and concurrency

External MIDI remains processed before sequencer generation at the next native
render boundary. A same-boundary MIDI base edit therefore updates both the live
field and raw mirror before an accepted sequencer event is applied. CC/NRPN
state publication, controller reconciliation, deferred MIDI, FIR buffering and
PDC are unchanged.

LXRS v1/v2 serialization reads only the persistent authorities. Transient slots
and effective values are absent from the stream. The twelve modulation routes
are neither rewritten nor re-encoded by sequencer automation.

The dedicated concurrency case uses an explicit start barrier while alternating
two complete pattern banks. Every observed automation pair belongs to one
snapshot generation; no torn pair was observed. Busy acquisition remains
bounded. This stress case is not a ThreadSanitizer claim; TSan was not run.

## Original-C verification and limitations

The test build compiles the unchanged pinned `automationNode.c`. A small adapter
provides the original base array and MIDI setter boundary, records the actual
restore/write calls, and compares their observable parameter state with the C++
Engine. It verifies destination restore order, value order, duplicate targets,
and newest-base release semantics.

This is a focused original-C comparison, not execution of the whole STM32
sequencer image. Hardware services and the full MIDI/front-panel environment are
stubbed at the setter boundary. Scheduler gate/probability behavior remains
covered by the P16-C source-derived oracle and direct pinned-source audit.

## Verification summary

- P16-D O0/O2: 218 checks per build, zero failures, four PCM comparison groups,
  127 changed sample positions in the aggregate, zero measured allocations.
- P16-A, P16-B and P16-C fresh O0/O2 regressions pass.
- P15-M2A, M2B, M2C, M2D and M2E applicable fresh O0/O2 suites pass; M2E's
  deliberately disconnected builds remain detected negative controls.
- Fresh native checkpoint: 42/42 executions pass, including MIDI, state codec,
  concurrency, FIR, original-C snare and 80,000-block filter differential.
- Private VST3 state 322, runtime 1,143, controller 955, M2G state 91, M2H
  controls 684, internal probe 1,033 and Steinberg validator 47/47 pass.
- The historical exact FIR Functional O2 artifact remains independently blocked
  by Windows Application Control and is not relabelled as a pass. Current-source
  FIR O2 passes in the fresh checkpoint.

Evidence is under `out/p16d/`. The private VST3 was not installed. P16-E may
address the next explicitly authorized sequencer milestone, but host transport,
pattern persistence, chaining, presets, morphing and GUI work remain outside
P16-D.
