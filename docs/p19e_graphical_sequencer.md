# P19-E — Graphical sequencer and pattern editor

## Classification

P19-E engineering acceptance is **PASS**. Direct graphical REAPER acceptance is
**BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE** and is not represented as a
successful visual test. This is a development checkpoint only. P16-F release
certification remains independently blocked.

The certified starting point was
`06d37c12510998d60ac940ba4d6ae1e2aceaad30` on
`p12b-4c-fir-resampler`. Before editing, local, tracking and live remote HEADs
matched; the worktree and index were empty; the firmware, iPlug2 and VST3 SDK
pins matched; the P19-D source checkpoint was present; and the protected module
and all seven installed resources matched their recorded hashes.

## Production editing boundary

`SequencerEditFacade` is a non-real-time adapter over the existing
`PatternStateAuthority`; it does not own a second bank. A read acquires an
immutable authority slot and copies only one selected `PatternData` plus its
generation and shuffle byte. An edit acquires the existing single bounded
writer, verifies that its base generation equals the GUI's expected generation,
mutates the inactive full-bank candidate through existing P16 operations, and
publishes once. Busy, stale, invalid-coordinate, invalid-value, invalid-lock and
generation-exhausted results are explicit. A failed request cancels the writer
and leaves the accepted bank unchanged.

This preserves the three-slot authority and the pipeline:

`GUI request → generation validation → complete candidate → one publication → processing-boundary acquisition`

There is no partial step or pattern publication, pointer escape, retry loop,
audio-thread allocation, mutex, wait, file access or log operation. Rapid edits
are generation ordered. A candidate derived from an older generation cannot
overwrite a newer publication.

The facade delegates transient active/pending pattern selection, mute and roll
state to the existing `TriggerScheduler`. Persistent pattern contents remain in
the bank. No transient mute or held-roll state is added to LXRS serialization.

## Grid and step detail

The fixed 1280 × 800 Sequencer view contains eight pattern buttons, seven
logical track rows and sixteen main-step cells per row. The rows are D1, D2,
D3, Snare, Cymbal, Closed Hi-hat and Open Hi-hat; the last two retain the one
physical hi-hat engine contract. Each grid cell shows main-gate state, stored
detail, velocity, non-default probability, selection and playhead state.

The selected-step panel exposes all eight physical positions. Main gate state
is stored in `mainGates`; physical state remains in the seven-byte `StepData`
record at `mainStep * 8 + substep`. Turning a main gate off changes only its
mask bit. It does not clear or normalize physical substeps, note, velocity,
probability or locks. Consequently, an inactive main step can still display a
detail indicator and its stored data can be edited.

Note, velocity and probability controls accept and display exact source-domain
integers. Note and velocity preserve 0–127. Probability preserves 0–127 and the
inclusive source condition `(GetRngValue() & 0x7F) <= probability`; the UI
explicitly states that zero can still pass RNG zero and that the main gate is
the OFF control. Invalid values reject without mutation.

## Parameter locks and precedence

Two lock rows expose the original stored selector byte, exact 0–127 value and
an explicit OFF action. Selectors are admitted only when the existing
`DecodeAutomationSlot` contract accepts them; typed host route identities are
not rewritten. The editor scans over unsupported selector bytes rather than
coercing them to a different target.

Slot ordering is unchanged, including slot-two override for duplicate targets.
No new cache or release value is introduced. Existing processing therefore
retains Base patch → Morph projection → Sequencer lock → Velocity → LFO →
Rendering and reveals the newest accepted base/morph projection when a lock is
released.

## Track, pattern and performance operations

Track length and rotation call the existing bounded P16 operations and preserve
their wrap/normalization rules. Mute is scheduler-owned transient state per
logical track, so CH and OH remain distinct sequencer articulations without
creating a seventh synthesis engine. Manual roll uses existing host parameters
243/244 and the original fourteen rate indices. Mouse release and editor close
clear held roll state; reconstruction and project restore do not arm a roll.

Pattern controls show selected, active and pending state for eight physical
patterns. Codes 8–14 remain random-next codes and are never displayed as extra
pattern slots. Running transitions continue to use the scheduler's deferred
track-zero boundary. Repeat/next chain bytes and the exact shuffle byte are
edited without replacing the original sixteen-phase numerator rule.

Euclidean generation calls `WriteHandle::generateEuclidean`; no generator was
added. The UI makes track, length, pulses and rotation explicit. It changes the
target track's main gates and source length through the accepted P16-E contract
while preserving its physical step records and track rotation.

Copy pattern, copy track, clear pattern and clear track use the existing P16-E
operations. Destinations are explicit. Pattern copy/clear retain destination
chain metadata as defined by that contract. Clear-pattern and clear-track use a
two-activation confirmation and state their destructive scope; no generic bank
clear is exposed.

## Readback, transport and state

P19-B's per-instance bounded GUI authority remains even/odd sequenced with four
acquisition attempts and a 30 Hz idle refresh. P19-E adds one fixed 64-bit
performance word carrying seven bounded playhead positions plus mute/roll
masks. The existing state word carries running, host/internal clock, active and
pending pattern, valid tempo, reanchor and host-transition state. The GUI never
reads mutable scheduler or bank objects directly and does not copy the bank at
30 Hz.

Host parameter IDs remain exactly 0–245. No step became a host parameter.
LXRS v1/v2/v3 layout and limits remain unchanged; edits continue through the
existing bank section. Exact-DLL save/reopen verifies selected pattern, disabled
main-gate detail, locks and track metadata while held roll remains transient.
Two simultaneous plugin instances have independent bank, selection, snapshot
and performance state.

## Keyboard and accessibility

The established focus ring and deterministic tab order now include pattern,
track, grid, mute, roll, substep, value and operation controls. Arrows navigate
patterns/tracks/steps/substeps or adjust exact integer fields; Space/Enter
activates gates and actions; Home/End selects value endpoints; Escape cancels
armed destructive actions. Exact entry remains available through the pinned
iPlug2 text-entry path. Essential operations are not drag-only.

Pinned iPlug2 does not prove a complete native Windows screen-reader tree for
custom-drawn controls. Screen-reader, DPI/scaling and arbitrary-resize
qualification remain P19-F work. Arbitrary resizing was not enabled.

## Verification

The dedicated standalone matrix passes under GCC O0/O2 and Visual Studio LLVM
O0/O2: 170 facade assertions and 413 editor/readback assertions per
configuration, 2,332 total. Coverage includes topology, gate/detail
independence, note/velocity/probability boundaries, both lock slots and duplicate
precedence, length/rotation, mute/roll, pattern/pending selection, shuffle,
Euclidean generation, copy/clear, invalid rollback, stale/Busy rejection, rapid
ordered edits, snapshot consistency and instance isolation.

The exact private-DLL P19-E GUI test passes 109/0 against the production editor:
1280 × 800 attachment, navigation, grid/detail edits, gate preservation,
note/velocity/probability, locks, track metadata, shuffle, Euclidean generation,
copy, selected-pattern persistence, balanced roll gestures, reopen and
multi-instance isolation. Retained P19-B/C/D exact-DLL suites pass 104/0,
721/0 and 254/0. PDC remains 26 samples at 48 kHz.

Fresh retained P16 architecture/core/locks/performance/persistence, P18 morph,
P15 velocity/LFO, P19-A and full native O0/O2 campaigns pass. Native results
include zero counted render allocations, FIR quality/latency, state/concurrency,
original snare and 80,000 bitwise original-C filter blocks per optimization.
The full exact-DLL host matrix and Steinberg validator pass against the final
private module. Exact allocation/stack evidence is recorded separately under
`out/p19e/`.

The private Release x64/v145 build uses Visual Studio Community 18 and MSBuild
18.10.1 with pre/post deployment events disabled. The private binary manifest,
exact SHA-256, resources and toolchain are in `out/p19e/private_binary_manifest.json`.
The protected installed DLL remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`,
and its seven resources remain byte-identical to `data/`.

## Direct REAPER gate

An isolated REAPER 7.80 x64 profile was generated for the exact P19-E bundle and
a unique `P19E_SEQUENCER_GUI` task window was observed. Computer-control state
acquisition initially timed out; on the bounded retry, captures remained
occluded by a non-target terminal and no targetable plugin/editor child was
exposed. Activation did not resolve the occlusion. Zero clicks and zero
keystrokes were sent. Therefore graphical pattern/track interaction, visual
layout, DPI and host save/reopen are **BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE**,
not claimed successful.

## Remaining scope

P19-F retains native accessibility, DPI/scaling, host visual acceptance and any
remaining production GUI qualification. P16-F release certification retains
its independent Import/event-observation/timing and historical PCM blockers;
P19-E does not weaken, close or relabel those gates.
