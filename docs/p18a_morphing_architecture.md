# P18-A firmware-faithful morphing audit

Classification: **PASS — SOURCE_CONFIRMED_ACTIVE, architecture only**.

Pinned Sonic Potions LXR v0.37 contains a real, user-facing preset sound morph.
It is a byte-parameter transformation performed by the AVR front panel, not a
dual-engine audio crossfade, not a pattern morph, and not an autonomous timed
transition. P18-A adds no production morphing and changes no synthesis,
sequencer, state, controller or plugin source.

## Authority and scope

The authority is `ref/LXR-0.37` at
`dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`. The Catalyst fork was not used.
The original owner's manual was checked through the Internet Archive snapshot
dated 2015-09-18 because the current wiki endpoint has an invalid TLS
certificate; certificate verification was not bypassed. The current official
Sonic Potions LXR page independently advertises realtime preset morphing.

This audit started from and ends at Git HEAD
`b723036ad4027746d69b0aa5b9779e464cd17c4b` on
`p12b-4c-fir-resampler`. The installed VST3 and seven resources were not
modified or rebuilt.

## Original user contract

Performance mode exposes:

- manual roll rate;
- Morph amount (`mrp`), described as the ratio between original and target;
- global sample-rate reduction; and
- shuffle.

The user selects `MorphSnd` on the load page and changes the preset number. The
target is a normal 236-byte `.snd` sound kit. It is loaded immediately; amount
zero produces no audible change. Increasing amount gradually transforms the
current sound. Selecting `MorphSnd` on the save page writes the current mixture
as a normal sound kit.

The source handlers match every part of that manual flow. There is no source or
manual control for morph direction, duration, rate, curve, scene, cancellation
command, or target capture. Amount zero is the only source-defined cancellation.

## Source and destination ownership

The front panel owns two arrays:

| Role | Storage | Lifetime |
|---|---|---|
| A: source/current kit | `parameter_values[NUM_PARAMS]`; IDs 0--227 are sound | Live mutable UI/MIDI/base state |
| B: morph target | `parameters2[END_OF_SOUND_PARAMETERS]` | RAM until another target load or reboot |
| Amount | `parameter_values[PAR_MORPH]`, ID 229 | RAM performance value 0--255 |

A is not a frozen snapshot. Ordinary parameter edits update A, and the next
morph calculation observes the changed source. B changes only on a target load.
A normal kit load replaces A but retains B and amount, then recomputes the
mixture. Morphing does not modify either anchor; only the receiving DSP fields
change. At amount zero the exact A bytes are sent. At 255 some wide ranges do
not reach B because the denominator is 256.

The firmware uses one `preset_currentName[8]` for the selected/load/save name;
it does not maintain separate persistent source and target identity objects.

## Exact interpolation

For each byte `a`, target `b` and amount `x`:

```text
fixed = a*256 + (b-a)*x
q = fixed / 256
result = q when (fixed & 255) < 127, otherwise q+1
```

Arithmetic is integer. Rounding is upward at remainders 127--255. Despite the
source comment, it is not banker's rounding. The calculation is stateless and
always starts from A and B, so repeated same-amount updates do not drift.

Exhaustive original-C testing establishes:

- amount 0 is exact A;
- equal anchors are invariant;
- `0 -> 255` at 255 yields 254;
- `255 -> 0` at 255 yields 1;
- 8,001 ascending pairs and 8,385 descending pairs miss exact B at 255;
- `0 -> 1` changes at amount 127; `1 -> 0` changes at 130.

There is no special discrete handling. Enums and switches pass through the same
numeric rule and can traverse intermediate enum codes.

## Parameter participation

`preset_morph` loops across all IDs 0--227. The source-derived matrix has:

| Classification | Count |
|---|---:|
| Continuous byte scalar | 140 |
| Enumerated/discrete byte | 65 |
| Typed route selector | 12 |
| LFO voice hint | 6 |
| Reserved opaque | 5 |

All 228 are numerically calculated and transmitted. The semantic exception is
the 18 route-related bytes:

- IDs 149--154: velocity destination selectors;
- IDs 161--166: LFO voice hints; and
- IDs 167--172: LFO destination selectors.

Routes are installed separately from A with special messages. Generic morph
messages for the LFO selector/hint cases are explicit no-ops in the STM32
handler, and the morph save writer explicitly copies all 18 from A. Thus the
source-defined effective policy is 210 numeric sound bytes plus source-anchored
routes. Interpolating route identities in a plugin would be unfaithful and
unsafe.

The matrix also identifies 38 shared-hi-hat rows, one global all-voice
decimation row, five reserved rows, and eleven opaque/non-scalar runtime rows.
No row remains UNKNOWN. “Eligible” here describes byte participation, not proof
that every reserved or unsupported byte has a DSP effect.

## Timing and active synthesis

Each amount edit runs one 228-iteration loop and emits 684 UART bytes at
500 kbaud. The wire lower bound is 13.68 ms. The STM32 uses a 64-byte receive
FIFO and consumes one byte per main-loop iteration while also handling MIDI,
USB, native audio generation, sequencer ticks and triggers. Original hardware
application is therefore sequential and non-atomic; mixed old/new fields can
exist while a 32-frame native block is generated.

The sweep does not reset the Engine or sequencer. Active oscillator/envelope/LFO
phases and filter history continue. A changed field affects an active voice when
that field is next read. Note-trigger controls affect subsequent triggers. LFO
retrigger mode changes do not themselves retrigger the LFO. Patterns, transport,
rolls, mute and kit file state are not replaced.

MIDI and sequencer automation may interleave with the physical sweep. The exact
collision result depends on message timing and is not claimed deterministic.

## Modulation and lock relationship

Morph is a change to effective base synthesizer fields, not another LFO or
velocity source. The accepted plugin order is currently:

```text
base -> sequencer lock -> velocity -> LFO -> render
```

The proposed safe extension is:

```text
authoritative base A -> derived morph projection -> sequencer lock
  -> velocity -> LFO -> render
```

This keeps locks authoritative for their documented lifetime; release reveals
the newest morph projection. Target routes never replace A's typed routes.
Source A remains host-visible authoritative state; temporary projected values
must not corrupt A or cause 228 unsolicited controller edit gestures.

## Persistence

Original target, amount and target selection are transient. `.all` and `.prf`
save the base kit. `MorphSnd` creates an ordinary `.snd` containing the current
mixture and the source route bytes.

P18-A changes no LXRS format. The minimum faithful plugin feature can likewise
make the morph session transient. If host continuity is later required, the
design proposes a 240-byte optional body inside the existing 16-byte LXRS module
framing. The resulting 256-byte module would grow a current 51,089-byte state to
51,345 bytes, remaining inside 65,536. Tag/version/CRC/order/duplicate/unknown
module behavior still requires a separate authorization gate.

## Proposed real-time architecture

P18-B should reuse existing authorities:

1. A remains owned by `ParameterStateAuthority` and the coordinated typed route
   authority.
2. B is a fixed 228-byte, per-instance validated target record; no pointers or
   dynamic storage cross the publication boundary.
3. Amount is a fixed byte. A target/amount generation reserves a preallocated
   inactive projection.
4. A complete projection is computed outside rendering using the exact original
   formula; route identities remain A's.
5. The processing owner accepts it at one 32-frame native boundary, with a
   bounded stale check and no half-applied block.
6. Invalid, Busy and stale requests leave the previous projection intact.
7. No heap, mutex, file I/O, logging, blocking wait or unbounded retry occurs in
   audio processing.
8. Normal source kit load retains B/amount, matching the original, and schedules
   a new projection without changing patterns or transport.
9. Controller readback distinguishes authoritative A from audible derived
   state.

Atomic boundary application is an intentional VST3 safety difference. It must
not be marketed as bit-exact hardware update timing.

## Related firmware inventory

- Velocity, LFO, automation, Euclidean generation, copy/clear, pattern
  chaining/random-next/probability, rotation, rolls and mute are distinct
  original mechanisms; their implemented plugin status is recorded in the
  evidence inventory.
- SOM has actual interpolation/node/RNG/trigger code. However
  `USE_DRUM_MAP_GENERATOR` is zero, the AVR page is compiled out, and no source
  setter makes the STM32 `seq_SomModeActive` flag true. It is
  SOURCE_CONFIRMED_UNREACHABLE in pinned v0.37 and is not a morph dependency.
- No verified sound-state randomizer or scene-crossfade mechanism was found.

## Standalone verification

The P18-A adapter includes unchanged `presetManager.c` through the established
bounded P17 hardware shims. It calls the file-local original interpolation and
the original morph/save functions. It does not emulate AVR timing and makes no
production VST3 claim.

| Toolchain | Optimization | Result | Assertions |
|---|---|---:|---:|
| GCC | O0 | PASS | 16,778,153 |
| GCC | O2 | PASS | 16,778,153 |
| VS Community LLVM | O0-equivalent | PASS | 16,778,153 |
| VS Community LLVM | O2 | PASS | 16,778,153 |

Each run covers all 16,777,216 interpolation triples, 228 emitted messages,
18 route bytes in saved output, exact 236-byte output, endpoint counts, names
and discrete vectors. Earlier failing test-vector/status assumptions are
preserved in separate diagnostic run directories; production source was never
changed to satisfy them.

## Limits

- The archive manual observation corroborates the user contract; pinned C is
  the behavior authority.
- UART wire-time is a protocol lower bound, not a cycle-accurate hardware trace.
- No native PCM or active-voice differential was required because P18-A does
  not implement or modify production rendering.
- No VST3 was built, installed, launched or validated in this source-only phase.
- P16-F release certification remains independently BLOCKED/open; P18-A does
  not alter that status.

## Evidence

The complete evidence set is under `out/p18a/`, including the search matrix,
call graph, 228-row participation matrix, arithmetic/timing/precedence designs,
test command receipts, executable hashes, roadmap, integrity report and exact
file inventory.
