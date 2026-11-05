# P16-B — Native sequencer core, pattern authority and deterministic clock

Status: **PASS**. This milestone is an isolated C++17 core. It is not connected
to `Engine`, MIDI, VST3 transport, LXRS state, parameter locks or audio rendering.

## Source contract

The implementation follows Sonic Potions LXR 0.37 at
`dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`, specifically
`Sequencer/sequencer.{h,c}`, `clockSync.c`, `DSPAudio/automationNode.c`, and the
front-board preset/menu definitions audited in P16-A. The Catalyst fork was not
used.

## Fixed data model

`StepData` is exactly seven ordered `uint8_t` fields: encoded active/velocity,
probability, note, parameter 1/value 1 and parameter 2/value 2. Compile-time size
and offset assertions prevent layout drift. The pattern bank contains:

- eight patterns;
- seven logical tracks per pattern;
- 128 flattened physical positions per track;
- sixteen independent main-gate bits per track;
- explicit length and rotation bytes instead of the original nonportable
  bitfield union;
- per-pattern repeat/next metadata; and
- one bank-wide shuffle value.

`PatternBank` is 50,418 bytes on the certified x64 test compiler. It is
trivially copyable but is never a wire format. Future persistence must continue
to encode fields explicitly.

Structural validation accepts source-valid storage independently of whether a
future playback phase supports a target. Volume's high bit and low seven bits
are always canonical. Probability, note and lock values are 0–127. Lock IDs are
0–227 or `0xff`; playback will later restrict these to safe targets. Encoded
length is 0–15 (zero means sixteen), rotation is 0–15, repeat is 0–127,
next-pattern is 0–14, and shuffle is 0–127. Invalid edits return a typed status
and leave the candidate unchanged.

The edit API exposes no mutable published pointer. It supports step/gate,
length, rotation, chain, shuffle, reset and copy operations. Source quirks are
retained: resetting a pattern preserves chain metadata, and copying a pattern
does not overwrite the destination chain metadata.

## Defaults

`ResetBank` reproduces firmware initialization rather than a factory preset:

- active/pending pattern 0 and tempo 120 are runtime defaults;
- all main gates off;
- velocity 100;
- every cluster boundary physical position active, other positions inactive;
- probability 127 and note 63;
- both lock destinations `0xff`, values zero;
- effective track length sixteen, rotation zero;
- repeat zero, next pattern self; and
- shuffle zero.

Every one of the 7,168 steps is compared to the independently compiled C
source-contract fixture at O0 and O2.

## Track identity

Tracks D1/D2/D3/SN/CY map to synthesis owners 0–4. Closed and open hi-hat map
to owner 5 with separate closed/open articulation identities. The core provides
only pure mapping; it performs no trigger dispatch.

## Deterministic RNG

`DeterministicRng` is instance-local xorshift32 with explicit seed, reset and
fixed-width wraparound. It is a reproducible plugin adaptation, not an attempt
to reproduce STM32 hardware entropy. The inclusive source comparison
`random <= probability` is retained, including probability zero passing for RNG
zero. `ClusterProbability` maintains one latch per track and preserves the
source quirk where an inactive cluster-boundary position leaves the prior random
value unchanged.

## Clock arithmetic

The native float rate is represented exactly as `5632353 / 128` Hz. One physical
position advances by the rational:

`(5632353 × 60) / (128 × 32 × BPM)` native frames.

The clock stores an integer frame and a denominator-relative remainder. It uses
Bresenham-style carry, so it performs no floating accumulation and has zero
rational drift over one million tested positions. Tempo is source-backed 1–255;
zero remains the original external-sync sentinel and is invalid for the internal
clock.

Start/restart emits position zero immediately. Stop emits nothing and advancing
while stopped moves the time anchor. Resume follows the firmware stop/start
semantics: it restarts position zero immediately at the current anchor. A tempo
change is re-anchored to the last emitted exact boundary. Backward time is
rejected.

Event timestamps are rounded up to the next 32-frame native block. The exact
error is less than one native block; the integer-floor diagnostic can differ by
32 when an exact fractional point lies just above a block boundary. The core
does not claim intra-block accuracy.

## Pattern authority selection

Three designs were evaluated before implementation:

| Design | Result |
|---|---|
| Two immutable buffers | Safe, but one long-lived reader of the old buffer blocks every subsequent publication. |
| Per-pattern copy-on-write | Lower edit copy size, but cannot publish a coherent eight-pattern bank without an additional aggregate lifetime protocol. Deferred until evidence shows the 50 KB writer copy is material. |
| Three fixed immutable slots | Selected. Simple coherent bank generation, two inactive candidates, explicit reader lifetime and bounded backpressure. |

Each slot contains a complete immutable bank, generation, atomic reader count and
atomic Available/Writing state. One atomic writer flag serializes editors. A
reader loads the active slot, increments its pin, then rechecks both slot state
and active index before accessing the payload. A writer may claim only an
inactive slot with zero pins, rechecks pins after the atomic claim, copies the
active immutable bank, edits and validates the private candidate, marks it
available, and publishes its index with release ordering.

This closes the load-before-pin race: if a writer claims first, the reader sees
Writing and releases; if the reader pins first, the writer's post-claim check
backs out. No generation counter is used to excuse concurrent reads of mutable
memory.

Reader acquisition is bounded to eight attempts. A writer scans the two inactive
slots once. When both are pinned, publication returns `Busy`; no slot is
overwritten and there is no spin. RAII handles pin/release snapshots and cancel
unfinished edits. Initialization is construction-time only, before concurrent
access.

Memory per authority is 151,344 bytes, of which 151,254 bytes are three pattern
payloads. The audio-facing operation is only pin/check/release. The full-bank
copy and validation occur on the editor, never once per native block.

## Verification

Fresh O0 and O2 runs each pass 1,007,503 assertions with zero failures. Coverage
includes all source defaults, boundaries and corrupt fields; RNG independence,
reset and cluster reuse; tempo extremes, start/stop/resume, boundary tempo
change, chunk invariance, block quantization and one million positions; snapshot
lifetime, pinned-slot contention, two concurrent readers plus a writer,
multi-instance isolation and invalid-edit rollback.

Measured operations allocate zero heap blocks. Representative standalone
measurements are recorded in `out/p16b/allocation_report.md`; they are diagnostic
rather than real-time deadlines.

The original-C fixture compiles the pinned `sequencer.h` unchanged for size and
offsets and transcribes the short default initialization expressions from
`seq_resetNote`, `seq_clearTrack` and `seq_init`. It is identified as a
source-extracted contract fixture, not a full hardware runtime differential.
Hardware entropy remains intentionally non-bit-exact.

The fresh native checkpoint has 42/42 PASS across O0/O2, including MIDI,
serialization/concurrency, current FIR functional/quality/latency, synthesis,
and original-C snare/filter comparisons. Fresh P15-M2A through M2F standalone
suites pass. M2G/M2H remain covered by their accepted unchanged checkpoint.
The exact historical FIR Functional O2 Application Control denial remains
BLOCKED and is not relabelled.

## P16-C boundary

P16-C may consume a pinned `ReadHandle` only at a defined processing boundary,
derive bounded events from its immutable bank, and release before reuse. It must
not retain raw step pointers after releasing the handle. Trigger ordering,
hi-hat articulation, MIDI coexistence and the pre-native-block event queue enter
P16-C. Parameter-lock application remains P16-D; host transport and persistence
remain P16-F.

No Engine, VST3, LXRS, MIDI, installed bundle or synthesis algorithm was changed
by P16-B.

The accepted facade is `interfaces/SequencerCore.h`.
