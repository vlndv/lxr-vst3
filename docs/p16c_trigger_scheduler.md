# P16-C — seven-track trigger scheduler and native Engine integration

Status: **PASS**. Publication is recorded separately after the reviewed commit
and normal fast-forward push. This document begins with the contract recorded
before production code changed and then records the final implementation and
certification.

## Authority and scope

The behavioural authority is Sonic Potions LXR firmware 0.37 at
`dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`, principally
`Sequencer/sequencer.c:321-368,379-570,703-745,801-845,1468-1485`,
`MIDI/MidiVoiceControl.c:51-77`, and the four native voice trigger functions.
The P16-B immutable bank, deterministic PRNG and rational clock are reused.
Catalyst is not a reference. Parameter locks, chaining, host transport,
persistence and UI remain excluded.

## Pre-implementation contract

1. A clock boundary advances the seven track positions first, in ascending
   track order. Each track wraps at its source-encoded effective length. Start
   initializes every track to `rotation * 8 - 1`, so the first boundary emits
   the rotation position immediately.
2. For each track, the main gate for `position / 8` is checked before the stored
   physical step. A closed main gate suppresses all eight stored positions and
   does not modify them.
3. Probability is evaluated only for an active stored step under an active main
   gate. At substep zero, and only when that boundary step is active, one
   `GetRngValue() & 0x7f` equivalent is consumed for that track. Later substeps
   reuse the latch. An inactive boundary step preserves the preceding latch.
   The inclusive decision is `random <= probability`.
4. RNG consumption follows the firmware's track loop, tracks 0 through 6.
   Pattern-chain randomness is absent, so the deterministic instance stream is
   consumed only by active cluster-boundary probability checks.
5. Accepted events retain ascending track order. Tracks 5 and 6 therefore
   dispatch closed hi-hat before open hi-hat to shared synthesis owner 5.
6. Step `note` is the native base-note byte passed unchanged to the existing
   voice trigger. The firmware calls `osc_setBaseNote` from that value. Step
   volume's low seven bits are passed once as trigger velocity; existing voice
   amplitude and velocity-destination modulation consume it. Zero is a valid
   trigger velocity and is not treated as MIDI Note Off by the sequencer path.
7. The event record contains fixed-width values only: pattern, track, owner,
   articulation, physical/main/substep positions, note, velocity, scheduled
   native boundary, stable order and snapshot generation. It contains no raw
   pattern or Engine pointer and no lock payload application.
8. Supported tempo 1–255 yields more than 32 native frames per physical
   position. One native block can therefore contain at most one sequencer
   position and seven ordinary track events. The fixed event capacity is seven.
   A defensive overflow rejects the newest event, never reorders accepted
   events, and increments a saturating bounded diagnostic counter.
9. One P16-B `ReadHandle` is acquired at a due position boundary and remains
   pinned through all seven reads and event construction. It is released before
   trigger dispatch. Acquisition is bounded to eight attempts. `Busy` consumes
   and skips that sequencer boundary, increments diagnostics, and never falls
   back to an unpinned pointer or stale full-bank copy.
10. Events are dispatched immediately before `Engine::processBlock` for their
    quantized 32-frame boundary. The clock's ceiling policy guarantees less
    than 32 native frames of added quantization; no intra-block accuracy is
    claimed.
11. Existing external MIDI is processed through the unchanged parser before
    audio rendering. At a coincident native boundary, already-accepted MIDI has
    happened first and the ascending sequencer batch is applied immediately
    before rendering. With sequencing stopped/disabled, the existing adapter,
    Engine, modulation, FIR and PDC path is unchanged.
12. Stop emits no events and resets the source-style transport epoch. Start and
    resume both restart at the selected pattern's rotation boundary. Reset also
    resets deterministic RNG/latches and native time. Pattern selection is a
    bounded current-pattern choice only; no chain state is executed.

## Bounded implementation matrix

The machine-readable matrix is `out/p16c/implementation_matrix.csv`. Every
implemented behavior names its source range and verification obligation.
Unimplemented rows are explicit scope exclusions rather than inferred behavior.

## Implemented event contract

`TriggerEvent` is a pointer-free fixed-width value containing the quantized
native boundary, clock ordinal, immutable snapshot generation, stable order,
pattern, logical track, synthesis owner, articulation, physical/main/substep
indices, native note and native velocity. `TriggerEventBatch` owns seven inline
slots. The supported 1--255 BPM range has a minimum physical-position interval
of about 323.55 native frames, greater than the 32-frame native block, so no
normal block can need more than the seven logical-track events. Defensive
overflow rejects the newest event, retains accepted order and saturates a
diagnostic counter.

The scheduler owns one P16-B three-slot authority, rational clock, deterministic
PRNG and per-track probability latch. At every due boundary it pins one coherent
bank snapshot, advances and evaluates tracks 0 through 6, copies accepted values
into the inline batch, releases the snapshot and only then dispatches. The
default read acquisition is bounded to eight attempts. A `Busy` result consumes
the clock boundary, advances all positions from the last seven validated length
bytes, emits nothing and records the skipped boundary. It never reads an
unpinned slot, retries without a bound, or copies the 50,418-byte bank per block.

## Firmware order and semantics

The decision loop follows `sequencer.c:488-543`; transport initialization
follows `seq_setStepIndexToStart`, `sequencer.c:1468-1485`, and stop/start
semantics follow `sequencer.c:801-845`. A track advances before its gate and
physical step are inspected. Length code zero means sixteen main steps. The
selected main-gate bit suppresses all eight substeps without modifying stored
records.

An RNG byte is consumed only for an active physical step at substep zero under
an open main gate. Substeps one through seven reuse that track's latch, and an
inactive boundary preserves the preceding latch. Acceptance is the firmware's
inclusive `random <= probability`; therefore probability zero can pass for RNG
zero and probability 127 always passes. Ascending track order also defines RNG
consumption and dispatch order.

The source passes the stored note byte and volume low seven bits through
`voiceControl_noteOn` (`MidiVoiceControl.c:51-62`) to `Drum_trigger`
(`DrumVoice.c:119-168`), `Snare_trigger` (`Snare.c:94-122`),
`Cymbal_trigger` (`CymbalVoice.c:100-131`) or `HiHat_trigger`
(`HiHat.c:99-130`). P16-C does the same through the existing `Engine` trigger
methods. Note is a native base-note value, not a new MIDI transformation.
Velocity is applied once; zero remains a valid sequencer trigger. Existing
velocity-destination modulation and LFO retrigger identity remain inside those
Engine paths.

The mapping is tracks 0/1/2 to drum owners 0/1/2, track 3 to snare owner 3,
track 4 to cymbal owner 4, and tracks 5/6 to shared hi-hat owner 5 as closed/open
articulations. Because the loop remains 0--6, simultaneous hats dispatch closed
then open, leaving the shared voice in the exact state produced by those two
native calls.

## Native boundary and MIDI coexistence

`LXRAudioAdapter` invokes the optional scheduler immediately before each
`Engine::processBlock`. Events are applied at the first 32-frame native boundary
at or after their exact rational time; the added quantization is less than 32
native frames and is not described as sample-accurate. The adapter frame counter
is reset with its native/FIR cache and saturates rather than wrapping.

The plugin owns one scheduler instance, stopped by default. No host parameter,
transport sync, pattern automation or state field was added. Existing MIDI is
accepted before host audio rendering; a sequencer batch due at the same native
boundary then dispatches immediately before rendering. A direct MIDI-then-
sequencer Engine trace is PCM-identical to that explicit order. Supplying no
scheduler, or leaving it stopped, is exact with the pre-P16-C PCM path. Project
restore and deactivation reset the internal transport without altering LXRS v2,
the 228 patch values or twelve routing controls.

## Differential and native-audio certification

`tests/reference/sequencer/original_trigger_scheduler_adapter.c` compiles the
pinned firmware `sequencer.h` unchanged and independently transcribes only the
short decision/start ranges above. The complete hardware `sequencer.c` cannot be
linked without STM32 clock, UART, MIDI, trigger, automation and front-panel
services, so this is explicitly a source-derived C oracle rather than a claim of
full firmware execution or hardware entropy equivalence. With an injected
xorshift stream, 128 complete physical positions compare exact event counts,
track order, positions, note, velocity, latches and final RNG state.

The dedicated O0 and O2 runs each pass 16,161 assertions with zero failures.
They cover all eight patterns, gates on/off, substeps 0/3/7, wrap, length and
rotation, probability 0/127 and latch quirks, multitrack RNG order, stop,
source-style restart, deterministic reset, tempo 1/120/255, a live 120-to-60
BPM change, snapshot gate updates, Busy, fixed-capacity rejection,
multi-instance isolation and variable caller grouping. The measured processing
loop allocates zero heap blocks.

Native audio cases prove stopped-sequencer equality, nonzero D1 output, nonzero
output for every logical track, direct-trigger equality, closed/open/simultaneous
hat behavior, repeated/substep triggering, probability rejection, live pattern
updates, MIDI coexistence and deterministic matched-state rerender. No
oscillator, envelope, filter, distortion, mixer or FIR equation changed.

## Regression and private VST3 results

Fresh P16-A and P16-B O0/O2 pass. P15-M2A through M2H applicable native suites
pass at O0/O2, including M2D's final-source rerun. The LFO disconnected builds
remain expected negative controls. The fresh native checkpoint passes all 42
executions: MIDI parser, native synthesis, state serialization/concurrency,
original-C snare and 80,000-block filter comparisons, and FIR functional,
quality and latency. The separately documented historical FIR Functional O2
Application Control denial is not relabelled, while the current-source FIR O2
checkpoint passes normally.

An isolated VS Community 18 / MSBuild 18.10.1 / Release x64 / v145 build used
both `/p:PostBuildEventUseInBuild=false` and
`/p:PreBuildEventUseInBuild=false`. The private 810,496-byte module is SHA-256
`0AF1C01756AC8F8B10DC37B9BE9A88AAD5343811C31E065156659D5A55A95BB9`
and was packaged with the seven verified runtime resources. Against that exact
private module, state (322), runtime (1,143), controller sync (955), M2G VST3
state (91), M2H controller (684), internal probe (1,033) and Steinberg validator
(47/47) all pass. It was not installed and no REAPER claim is made.

Evidence is under `out/p16c/`, headed by `verification_report.md`,
`regression_matrix.csv`, `scheduler_test_results.json`,
`original_c_comparison.json`, `audio_render_results.json`,
`allocation_report.md` and `private_binary_manifest.json`.

## Deferred scope

P16-D still owns both parameter-lock slots, overlay lifetime and precedence.
Pattern chaining, host transport/BPM/seek/loop synchronization, sequencer
persistence, preset management, morphing, UI and user SD samples remain
unimplemented. The current minimal internal transport supports start, stop,
source-style restart, reset, BPM and stopped-state current-pattern selection.
