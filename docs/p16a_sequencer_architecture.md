# P16-A — Original LXR sequencer architecture and timing contract

Status: architecture and source-audit milestone. No production sequencer code is
introduced by P16-A.

## Authority and scope

The behavioural authority is Sonic Potions LXR firmware 0.37 at
`dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`, principally:

- `mainboard/LxrStm32/src/Sequencer/sequencer.{h,c}`
- `mainboard/LxrStm32/src/Sequencer/clockSync.c`
- `mainboard/LxrStm32/src/Sequencer/{EuklidGenerator,SomGenerator}.{h,c}`
- `mainboard/LxrStm32/src/DSPAudio/automationNode.c`
- `mainboard/LxrStm32/src/MIDI/{MidiVoiceControl,frontPanelParser}.c`
- `front/LxrAvr/Preset/{SeqStep.h,presetManager.c}`

The [official owner's manual](https://www.sonic-potions.com/public/SonicPotions_LXR_OwnersManual.pdf)
is corroborating user-facing evidence. Where its simplified description differs
from executable source, the source wins. The Catalyst fork was not used.
AVR-to-STM32 UART framing is hardware transport and is not part of the plugin
design.

## Verified data model

The bank contains eight patterns, each with seven logical tracks and 128 flat
physical positions. A physical position is one of eight substep positions under
one of sixteen main steps. Main activation is stored separately as a 16-bit mask
per pattern and track.

`Step` is seven consecutive `uint8_t` fields:

| Field | Meaning | Contract |
|---|---|---|
| `volume` | active flag and velocity | bit 7 active; bits 0–6 value 0–127 |
| `prob` | probability threshold | 0–127; play when random value is `<= prob` |
| `note` | pitch/note | 0–127; default 63 |
| `param1Nr` | lock slot 1 destination | 8-bit firmware parameter identity; `0xff` means none |
| `param1Val` | lock slot 1 value | absolute 0–127 |
| `param2Nr` | lock slot 2 destination | as slot 1 |
| `param2Val` | lock slot 2 value | absolute 0–127 |

Defaults from `seq_resetNote` and `seq_clearTrack` are velocity 100, probability
127, note 63, both destinations `0xff`, and values zero. Each cluster's first
physical substep is active by default, while the corresponding main-step gate is
off. Enabling a new main step therefore normally creates a boundary trigger.

There is no tie field. A rest is an inactive physical position, an inactive main
gate, a muted track, or a probability rejection. A main gate does not itself
trigger: its active physical positions do. An inactive main gate suppresses all
eight stored positions without erasing them.

`PatternSetting` contains `uint8_t changeBar` and `uint8_t nextPattern`.
`LengthRotate` is a compiler-ABI-sensitive union: a byte view overlays two
four-bit `unsigned` bitfields. P16 must represent these fields explicitly and
must never serialize this union. Length zero means sixteen main steps; 1–15 mean
that many main steps. Rotation is 0–15.

The original front-board pattern file is a field stream, not a dump of
`PatternSet`. Its payload is 50,361 bytes: 50,176 step bytes, 112 main-gate
bytes, 16 pattern-setting bytes, one shuffle byte, and 56 length bytes. Rotation
is not written by that format. A `.pat` adds an eight-byte name (50,369 bytes
total). Version-2 `.all`/`.perf` files add name, version, a 64-byte global or
performance area and a 512-byte kit area (50,946 bytes total); their load path
validates the version and restores globals/kit before the pattern stream.

## Seven tracks and six synthesis owners

| Track | Logical role | Native synthesis owner | Articulation |
|---:|---|---:|---|
| 0 | Drum 1 | 0 | drum |
| 1 | Drum 2 | 1 | drum |
| 2 | Drum 3 | 2 | drum |
| 3 | Snare | 3 | snare/clap |
| 4 | Cymbal | 4 | cymbal |
| 5 | Closed hi-hat | 5 | closed |
| 6 | Open hi-hat | 5 | open |

Tracks 5 and 6 are not independent voices. `seq_triggerVoice` clears both hat
trigger outputs before calling `voiceControl_noteOn`; the latter passes
`voice - 5` as the articulation. Simultaneous events are visited in ascending
track order, so closed is dispatched before open and open is the final trigger.
Both tracks must use velocity/LFO owner 5 in the existing modulation contract.

Track mute is a seven-bit pre-trigger mask. Track lengths are independent and
are consulted per track, so source-defined polymeter is real. Track 0 alone is
the reference for pattern/bar boundaries. Rotation changes the starting main
step and is reset to zero on stop in the firmware.

## Main-step and substep timing

The STM32 configures SysTick at 4,000 Hz. `seq_calcDeltaT` computes a 96-PPQ
internal pulse in quarter-millisecond units:

`delta = (60,000 / BPM) / 96 * 4`

The polling clock advances the sequencer every third pulse, so physical
positions are 32 PPQ. At 120 BPM one physical position is 15.625 ms, one main
step is 125 ms, and 128 positions are two seconds. MIDI clock output advances
every fourth pulse (24 PPQ). Floating `seq_deltaT` and late-poll carry preserve
fractional delay, but the hardware clock is not audio-sample synchronized.

On internal start, the track indices are initialized to rotation × 8 minus one
and `seq_deltaT` is zero. The first successful `seq_tick` therefore emits the
rotation boundary immediately. Stop clears running state, master/bar position,
rotation and track indices; the synthesis note-off path does not imply an
instantaneous audio-tail cut.

Shuffle uses the exact 16-entry table documented in `timing_reference.json`.
The current factor minus the previous factor is applied to the 96-PPQ interval,
which telescopes to a phase displacement rather than independently stretching
every interval.

Timing sketches (`|` is one physical position):

```
single main trigger:     M0 [X.......] M1 [........]
                         ^ event at substep 0 only

multiple substeps:       M0 [X..X..X.] M1 [........]
                         ^  ^  ^ three independently pitched/locked events

inactive main gate:      M0 [x..x..x.]  stored bits, no events

concurrent tracks:       t0 X -> t1 X -> ... -> t5 closed -> t6 open
                         same timestamp, deterministic ascending-track order
```

No special within-step offset exists beyond the equal 32-PPQ grid. Manual roll
is evaluated after ordinary pattern triggering and can therefore add a second
trigger at the same sequencer position.

## Probability and deterministic rendering

The firmware uses the STM32 hardware RNG. For an active substep at position
`position % 8 == 0`, it samples `GetRngValue() & 0x7f`; the whole eight-position
cluster then shares that per-track value. The comparison is inclusive. Thus
threshold zero still passes for random zero (1/128), while 127 always passes.

ORIGINAL QUIRK: random generation occurs inside the active-substep branch. If a
main gate is active but its boundary substep is inactive, later active substeps
reuse the previous cluster's random value. This is source behaviour and must be
tested, not normalized into a generic probability model.

Hardware RNG bits cannot be reproduced offline. P16 should use a fixed,
specified per-instance PRNG and persisted musical seed, resetting/reconstructing
it at defined transport epochs. That is an intentional VST3 adaptation. The
test PRNG must be injected. Pattern-random selection and step probability must
consume one documented stream or separate streams; this choice is deferred to
P16-B because firmware uses the same hardware source but does not expose state.

## Parameter locks

The two slots are absolute 7-bit parameter writes. On a successful triggered
step, both destinations are changed first, restoring each previous destination
from `midiParser_originalCcValues`; slot 1 is then applied before slot 2. If both
target the same parameter, slot 2 wins. Probability-rejected and inactive steps
do not update or release locks.

The plugin must implement a sequencer-effective overlay, not write the patch
authority or routing authority:

`base patch → sequencer lock overlay → velocity replay → LFO dispatch → render`

This matches the source ordering: automation is parsed before note-on, while the
existing mixer path restores base values and replays velocity/LFO. Each track has
two slot owners. A slot remains effective until a later successfully triggered
step changes or clears that slot. Stop, seek, pattern restore and project restore
must reconstruct overlays deterministically; clearing them at those boundaries
is a documented host adaptation where hardware behaviour is not explicit.

Initial P16 production validation should allow only safe original patch targets.
It must reject route selectors, host-only controls and unsafe/global identities.
Locks must never emit host automation gestures.

## Pattern playback and chaining

Every track advances independently. When track 0 wraps, the global bar counter
increments. If no manual change is pending, `changeBar + 1` is the modulo period
for choosing `nextPattern`. Values 0–7 select a fixed pattern. Runtime code treats
values 8–14 as random among the first 2–8 patterns respectively; the older header
comment mentioning only 8 and 9 is stale.

A manual pattern request becomes pending and is committed only at the next track
0 boundary. On change, every track index resets to that pattern's rotation start,
all notes are turned off, and playback continues. The chain is global, not
per-track. `seq_resetBarOnPatternChange` optionally resets the absolute modulo
phase. Pattern copy copies steps, gates and length/rotation but not the two
pattern-setting bytes; pattern clear likewise does not clear chain settings.

## Performance functions

- Shuffle, track mute, rotation/length, manual roll, pattern copy/clear, MIDI
  note recording, automation recording and Euclidean generation are
  source-confirmed runtime features.
- Euclidean generation operates on sixteen main gates, forces at least one pulse,
  supports length 1–16 and rotation, and also sets track length.
- Recording supports off/8/16/32/64 quantization. It rounds at half a quantum,
  wraps at position 128, sets probability 127, and clears the default boundary
  substep when recording the first note into an inactive main gate.
- The global MIDI channel records to the selected track. Per-track MIDI channels
  trigger voices but are not recorded by that path.
- SOM generation is source-confirmed runtime code, but its complete user contract
  remains UNVERIFIED and should follow the core scheduler rather than enter P16-B.
- Pattern editor, preset browser and GUI work remain outside P16-A.

## Scheduler and Engine boundary

The current Engine consumes whole 32-frame native blocks, and current iPlug MIDI
delivery does not make `mOffset` effective inside those cached blocks. The first
production policy is therefore explicit native-block-boundary quantization, not
arbitrary host-sample accuracy. The bound is less than 32 native frames (about
0.727 ms at 44,002.758 Hz), plus the already reported FIR latency.

Future scheduling occurs immediately before each native block is rendered. A
fixed-capacity event array contains track, owner/articulation, pattern,
step/substep, velocity, note, two lock references, fixed-point event time,
priority and trigger identity. Same-time ordering is:

1. pattern-boundary/transport transition;
2. tracks 0 through 6;
3. within a track, lock destinations, lock values, then note trigger;
4. manual-roll trigger after the ordinary pattern trigger.

Pattern snapshots and event buffers are preallocated. Audio processing performs
bounded iteration only. There is no heap allocation, mutex, file I/O, logging or
host callback on the audio thread.

## Internal and host-sync timelines

Internal mode uses an integer/fixed-point native-frame phase accumulator derived
from the exact rational `nativeRate * 60 / (BPM * 32)`, plus source shuffle phase.
It retains the immediate-start convention and avoids floating cumulative drift.

Host-sync mode is an adaptation. It anchors a Q32.32 (or wider rational)
quarter-note timeline to valid host PPQ and sample position each callback and
enumerates every crossed 1/32-quarter boundary. It then maps each event to the
next eligible native-block boundary. Callback size cannot alter the musical
timeline. Invalid host fields are never guessed from stale values.

The contract for play, stop, seek, loop, tempo changes, reload and offline render
is in `out/p16a/host_sync_contract.md`. Original LXR bars remain sixteen main
steps and pattern transitions follow track 0; DAW time signature does not change
pattern length.

## Ownership and publication

Persisted musical state is an immutable/coherent bank snapshot: eight patterns,
steps, gates, lengths, chain settings, shuffle/configuration and deterministic
seed. An editor builds a complete candidate off the audio thread, validates it,
then publishes it with a monotonic generation and bounded lock-free handoff.

Ephemeral per-instance state comprises current/pending pattern, per-track
positions, bar/master counters, transport anchors, runtime RNG state, pending
events, active lock overlays, mutes, rolls and host discontinuity state. Restore
publishes pattern, existing patch and routing snapshots under a future shared
transaction epoch; the processing boundary consumes a coherent tuple. Existing
patch and routing authorities remain the only authorities for their domains.

## Persistence sizing and migration

There are 7,168 `Step` records (128 × 8 × 7), totaling 50,176 bytes. The exact
front-board payload is 50,361 bytes before a module header or integrity data.
Adding even a minimal 16-byte TLV and 32-byte sequencer header to today's
516-byte plugin state produces about 50,925 bytes (50,929 with VST3 bypass), so
the 4,096-byte stream limit cannot hold dense state.

P16-F must approve a larger bounded stream limit and an independently versioned
sequencer module (most cleanly an LXRS v3 container or a rigorously specified v2
mandatory extension). Encoding is explicit little-endian fixed-width data, not
raw C objects or bitfields. Sparse/compressed encoding may reduce common states,
but acceptance must cover its dense worst case. LXRS v1/v2 without a sequencer
module migrates to a validated default bank. P16-A changes no wire format.

## Implementation roadmap

- **P16-B:** typed bank model, validation, deterministic PRNG, fixed-point
  internal clock, coherent snapshot authority, standalone tests.
- **P16-C:** seven-track event scheduler, hat articulation/order, native-block
  queue and Engine boundary; MIDI coexistence.
- **P16-D:** substeps, exact probability quirk, two-slot lock overlay, recording
  primitives and deterministic audio comparisons.
- **P16-E:** chaining, per-track length/rotation, mute, shuffle, roll, Euclidean
  generation; SOM remains gated on a separate verified contract.
- **P16-F:** bounded sequencer state module, atomic project restore, host-sync
  timeline and controller contract.
- **P16-G:** full private VST3 build, validator, REAPER variable-block/seek/loop/
  offline-render certification, regressions and publication.

P16-B must not yet expose a pattern GUI, implement preset browsing or modify the
current LXRS container.

## Known ambiguities and limits

- The exact STM32 compiler size/layout of `LengthRotate` was not executed because
  the pinned ARM 4.8 toolchain is not installed. Host GCC demonstrates why the
  raw layout is nonportable; the field protocol itself is fully source-defined.
- Hardware RNG sequences are inherently unavailable for bit-exact comparison.
- SOM's complete performance semantics and any undocumented front-panel gestures
  remain UNVERIFIED.
- The hardware's polling jitter relative to DMA blocks is not reproducible or
  desirable as a plugin timeline. Native-block quantization is the declared
  fidelity boundary.

Detailed evidence and test results are under `out/p16a/`.
