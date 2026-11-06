# P16-E firmware-faithful performance sequencer

## Scope and result

Final certification is **PASS** after P16-E-R1 recovery. The nineteen-file
P16-E inventory was frozen and hashed before diagnosis; no DSP, interface or
test source changed during recovery. Windows Code Integrity records show that
the earlier denied images were rejected before test execution by the enforced
`VerifiedAndReputableDesktop` policy, GUID
`{0283ac0f-fff1-49ae-ada1-8a933130cad6}`, status `0xc0e90002`. No policy,
certificate, executable header, path or security service was modified.

The existing Visual Studio 18 developer environment supplied a normal permitted
build path. Direct x64 `clang-cl` builds targeting the MSVC runtime pass the
frozen final source at optimization-disabled `/Od /Ob0` and `/O2`: each executes
9,869 assertions with zero failures. The three formerly denied O0 regressions
also execute and pass from the same frozen source fingerprint. Recovery evidence
is under `out/p16e-r1/`; the earlier supporting and denial evidence under
`out/p16e/` remains unchanged.

P16-E adds the pinned Sonic Potions LXR 0.37 pattern-chain state machine,
source-defined random-next ranges, global shuffle phase, seven-track mute,
manual roll/retrigger, coherent copy/clear, and original Euclidean generation.
It retains P16-D parameter locks and the patch -> sequencer -> velocity -> LFO
render order.

It does not add host transport/tempo following, sequencer serialization, roll
recording controls, preset management, morphing, graphical editing, or user
sample loading. SOM is explicitly deferred because its complete user and
lifecycle contract cannot be established without invention.

The authoritative reference is Sonic Potions/LXR revision
`dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`. The official
[Sonic Potions owner's manual](https://sonic-potions.com/public/SonicPotions_LXR_OwnersManual.pdf)
was used only to clarify user-visible behavior; the pinned C defines runtime
ordering.

## Firmware contract

| Behavior | Original source | Port boundary | Verified result |
|---|---|---|---|
| Track-0 chain boundary | `Sequencer/sequencer.c:379-463` | `TriggerScheduler::preparePhysicalBoundary` | Only track 0 wrap increments the absolute bar and may transition |
| Repeat/modulo | `sequencer.c:370-377` | cached pattern repeat | `barCounter % (changeBar + 1)` selects next; repeat is absolute-song aligned |
| Random next | `sequencer.c:418-425` | instance RNG | codes 8--14 use one full RNG value modulo 2--8, before new-pattern probability |
| Transition reset | `sequencer.c:414-449,612-639` | cached length/rotation controls | all track positions become new rotation times 8 minus 1, then advance in the same boundary |
| Independent lengths | `sequencer.c:381-405,461-489` | per-track position array | encoding 0 means 16; other track wraps never change pattern |
| Shuffle | `sequencer.c:119-136,258-284` | `SequencerClock` | exact 16-entry phase curve on the rational global physical-position timeline |
| Mute | `sequencer.c:490-551,869-889` | atomic seven-bit mask | progress continues; ordinary gate/probability/lock/trigger work is skipped; rolls remain active |
| Periodic roll | `sequencer.c:538-551,990-1054` | fixed roll state/rate | ordinary event first, then current-step roll at exact firmware divisor |
| One-shot roll | `sequencer.c:973-988` | pending fixed bitmask | next-native-boundary adaptation, volume 100, note 63, current-step lock parsing |
| Euclidean | `Sequencer/EuklidGenerator.c` | `GenerateEuclidean` | unchanged original C comparison for every length, pulse and rotation input |
| Copy/clear | `sequencer.c:1257-1360` | PatternBank candidate edit | exact step/gate/length/rotation scopes; pattern repeat/next excluded |
| SOM | `Sequencer/SomGenerator.c:94-139` | deferred | source entry point characterized; no invented runtime API |

The complete pre-implementation matrix is
`out/p16e/source_contract_matrix.csv`.

## Pattern chaining and snapshots

The scheduler owns an active pattern, atomic pending pattern, manual-load flag,
eight cached repeat/next records, an eight-bit-style absolute bar counter, and
the instance-local P16-B RNG. A running selection is queued to the next track-0
wrap; a stopped selection is immediate. If the active and pending patterns are
the same at wrap, the automatic repeat decision runs first exactly as in the
firmware. Codes 0--7 select fixed patterns. Codes 8--14 calculate limits 2--8
and consume `rng.next() % limit` once.

One immutable bank snapshot is pinned for a physical-position boundary. It
contains both the outgoing chain metadata and every possible incoming pattern,
so a transition and the first new-pattern event never mix generations. The
snapshot releases before Engine mutation. If acquisition is Busy, the boundary
is consumed and the fixed cached controls advance chain/track state without
reading unpinned step bytes; no event is emitted. This retains the established
bounded P16-C policy.

Each track reads its own length after transition selection. Rotation is applied
at start and pattern activation. The firmware destructively clears the active
pattern's performance rotations on stop; the immutable VST adaptation records
equivalent per-pattern runtime zero-rotation overrides. A later explicit
rotation publication clears the affected override. Restart begins at position
zero without rewriting published pattern bytes.

## Shuffle timing

The firmware float table is exactly representable as numerators over 64:

`0, 1, 4, 9, 16, 25, 36, 49, 64, 63, 60, 55, 48, 39, 28, 15`.

Its cumulative physical-position offset reduces to:

`baseStep * phaseNumerator * shuffle / 1524`

where shuffle is 0--127. The unshuffled P16-B integer-rational timeline remains
authoritative, so phase changes do not accumulate floating-point drift. The
adjusted event is rounded up to the next 32-frame native block. On the falling
side of the curve, original timer deltas can be negative; the hardware poller
still processes one sequencer position at a time. The native adaptation
therefore enforces at least one native block between dispatches and catches up
as soon as the ideal phase moves ahead. This prevents inversion and duplicate
dispatch without claiming intra-block accuracy. Live shuffle changes use the
same monotonic rule.

The official manual describes the perceptual effect as delaying alternating
sixteenth notes. The source shows that the implementation is global and applies
to all physical-position ticks, including roll eligibility; it is not a
track-local gate offset.

## Mute and roll ordering

Mute is evaluated after every track position advances. A muted ordinary track
does not inspect the main gate or active byte, does not consume a probability
draw, does not parse locks, and does not trigger. External MIDI is not routed
through this mask. The firmware's note-off side effect is not imposed on the
plugin's independent external MIDI path.

The exact roll selector periods are:

| Selector | Period in physical positions |
|---:|---:|
| 0 | one-shot (`0xff`) |
| 1--4 | 128, 64, 43, 32 |
| 5--8 | 50, 16, 11, 8 |
| 9--13 | 6, 4, 3, 2, 1 |

The selector-5 value 50 is retained even though the source comment calls it
1/6. A periodic roll ignores mute, main gate, active bit and probability. It
uses volume 100, the current physical step's note, and parses both current-step
automation slots before triggering. If an ordinary step and roll coincide,
ordinary lock/trigger dispatch happens first for that track. One-shot presses
use note 63 and volume 100; because the original call occurs outside the native
Engine block, the plugin queues it to the next processing boundary.

The proven worst boundary is 21 triggers: seven ordinary, seven periodic rolls,
and seven one-shots already queued before a rate change. Two lock slots give a
42-operation bound. Both batches are inline, reject newest defensively, retain
insertion order, and saturate overflow diagnostics. No allocation or dynamic
lookup was added.

## Euclidean, copy and clear

`GenerateEuclidean` ports the original recursive group/remainder construction;
it is not replaced by a generic Bjorklund implementation. Length/pulse zero is
coerced to one and excess pulses clamp to length, matching the source setters;
length above 16 and rotation above length reject because they are outside the
bounded UI domain. Rotation equal to length is the original no-op quirk. The edit writes only the 16-bit main-gate word and
length encoding. It preserves all 128 physical StepData records, automation,
notes, velocity, probability and the stored track-rotation field.

Track copy copies 128 seven-byte records, the main-gate word, length and
rotation inside one pattern. Pattern copy does that for all seven tracks but
retains destination repeat/next metadata. Track clear restores all StepData
defaults, active substep zero in each cluster, zero gates, length 16 and
rotation zero. Pattern clear repeats track clear and retains chain metadata.
All operations act on an inactive candidate slot and publish atomically;
validation failure or Busy leaves the active bank unchanged.

## SOM assessment

`som_tick` is real pinned firmware code. On track-0 positions divisible by
eight it interpolates seven learned thresholds from `SomData.h`, consumes
hardware RNG and respects the mute mask. The sequencer calls it from inside the
seven-track loop, producing an unusual repeated-call/RNG contract. No official
manual control, complete storage lifecycle, or safe bounded activation contract
was found. SOM is therefore **DEFERRED**, as explicitly permitted by P16-E.
See `out/p16e/som_assessment.md`.

## Parameter locks, modulation and PCM

Pattern changes, mute, rejected probability and inactive steps introduce no
new lock-clear rule. A roll is a real source-style trigger and parses slot 1
then slot 2 before velocity and LFO processing. The Engine overlay remains
transient: base patch bytes, twelve LXRS v2 routing records and host controls do
not change. Stop/restart retains active overlay values; Engine reset/complete
restore retains P16-D's clear behavior.

Native tests render distinguishable nonzero PCM for different patterns and
rolls, preserve stopped-sequencer MIDI PCM exactly, and retain the full P16-D,
velocity and LFO differential suites. The scheduler is still stopped by
default in the VST3 and has no host-facing sequencer controls, so no REAPER
transport or GUI claim is made.

## Concurrency and real-time safety

Performance control masks and pending pattern/roll requests use lock-free
atomics. The audio path uses fixed arrays, bounded snapshot attempts, bounded
track/event loops and no heap, mutex, file I/O, logging or host callback.
Dedicated measurement covers shuffle calculation, chain selection, mute/roll,
lock dispatch and rendering-boundary processing with zero allocations.

The concurrency fixture holds an unpublished candidate behind an explicit
barrier, proves the reader sees the complete old generation, then commits and
observes the complete new generation. Existing P16-B and P12c-R1 targeted
authority tests remain passing. ThreadSanitizer was not run; ordinary stress is
not presented as a race-freedom proof.

## Verification and boundary for P16-F

- Final-source P16-E-R1 `/Od` and `/O2`: 9,869 assertions per build, zero
  failures. Both binaries launched directly under the normal enforced policy.
- Exhaustive unchanged-original-C Euclidean differential: all valid length,
  pulse and rotation combinations.
- Fresh P16-A/B/C/D and P15-M2A--M2E applicable O0/O2 suites pass.
- The previously accepted native checkpoint's 39 passing executions remain
  source-identical. Recovery builds execute the remaining O0 cases:
  `TransientGenTest` 17/0, `P12b4cResamplerTest` zero failures and the exact
  original-C `P12b4cSnareRenderTest` 61/0 with zero render allocations.
- Private Release x64/v145 VST3, six host/controller probes and Steinberg
  validator 47/47 pass. R1 reran validator 47/47 against the exact
  `1DF287...6140` private binary. The protected installed bundle and all seven
  resources are unchanged.
- The historical exact FIR Functional O2 executable remains BLOCKED by Windows
  Application Control and is not relabelled.

P16-E is ready for its controlled Git checkpoint. P16-F may begin only under
separate authorization and should remain bounded to host transport/tempo
integration and/or a separately designed sequencer persistence contract. It
must not silently add pattern persistence to the current 4,096-byte LXRS
ceiling, expose temporary locks as host patch state, or combine host sync, GUI,
presets and morphing into one milestone.
