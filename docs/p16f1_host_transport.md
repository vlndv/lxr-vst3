# P16-F1 VST3 host transport and tempo synchronization

## Classification

Available implementation verification is **PASS** in the native O0/O2 harness
and against the exact private VST3. The formal implementation, overall P16-F1
and publication classifications are **BLOCKED** because a
production instance has the firmware-faithful cleared pattern bank and P16-F1
has no permitted pattern editing/loading/persistence surface. REAPER therefore
cannot perform the mandatory pattern-driven sequencer, lock, shuffle and roll
audio cases. No commit or push is authorized while that mandatory gate is
blocked.

This is not a transport defect: a test-only, compile-time internal probe injects
an immutable bank into the exact private binary and all host-transport cases
pass. The probe is absent from production. Adding a factory pattern, hidden
production test inlet, pattern host parameters or ad-hoc state would violate the
P16-F1 scope and source-fidelity/persistence boundary.

## Host context findings

The pinned iPlug2 `ITimeInfo` conversion cannot be used as a validity-preserving
source. It assigns sample position only when `kProjectTimeMusicValid` is set,
then copies tempo, bar, cycle and time-signature members without checking their
individual VST3 validity flags. P16-F1 therefore overrides the LXR VST3 process
entry only and captures the pinned SDK `ProcessContext` directly. Shared pinned
iPlug2 sources remain unmodified.

The component requests project musical time, bar position, cycle range, tempo,
time signature and transport state. Each optional value enters the sequencer
context only when its native flag is present. `projectTimeSamples`, sample rate,
playing/cycle state, process mode and callback sample count are sampled once at
callback entry. A missing context is never interpreted as zero. The detailed
matrix is `out/p16f1/host_context_matrix.csv`.

## Host controls and state boundary

Exactly three parameters were added:

| ID | Name | Type/range | Default | Display |
|---:|---|---|---:|---|
| 240 | Sequencer Enable | integer boolean 0..1 | 0 | normal iPlug integer |
| 241 | Sequencer Clock | integer enum 0..1 | 0 | Internal / Host Sync |
| 242 | Sequencer Internal BPM | integer 1..255 | 120 | integer BPM |

IDs 0--239 retain their meanings. Values use a fixed atomic/deferred mailbox and
audio-boundary application; NaN, infinity and out-of-range values reject. The
exact-binary test verifies controller readback, invalid rejection and
multi-instance isolation. No gestures are emitted by transport processing.

LXRS v2 remains exactly 516-byte core / 520-byte VST3 stream: the three controls
are deliberately not serialized in P16-F1. Reopen restores them to the defaults
above. Patch values, twelve routes and bypass remain compatible. P16-F2 owns any
sequencer-state schema.

## Timeline arithmetic

Host PPQ is rounded once into signed Q32.32, bounded to plus/minus 1,000,000
quarter notes. BPM is rounded to integer micro-BPM in 1..255 BPM and integer
sample rates in 1..768,000 Hz are accepted. A callback position is:

`anchorQ32 + floor(samples * microBPM * 2^32 / (60 * 10^6 * sampleRate))`

The implementation splits quotient/remainder and rejects sample magnitudes over
1,000,000,000 before any overflowing multiply. At the admitted PPQ bound the
Q32.32 magnitude is about 2^52, leaving more than eleven signed magnitude bits.
Physical boundaries use the exact integer shuffle numerator over denominator
`32 * 1524`; a fixed 32-candidate search finds the first boundary at or after an
anchor. No callback-to-callback floating accumulator exists. A one-Q32-LSB
tolerance covers the sole double-to-fixed conversion.

The scheduler is still invoked only when the FIR adapter needs a new 32-frame
native block. The adapter supplies the host-sample offset that caused that
render. Callback lengths 1, 32, 64, 127, 256 and 512 produce the same intended
event/PCM timeline in the dedicated tests. No transport latency is added and the
existing VST3 latency remains 26 host samples at 48 kHz.

## Transport contract

Internal mode uses the unchanged P16-E rational clock. Host Sync uses PPQ as the
authoritative phase and host state as the play gate. A valid start at any PPQ
positions the seven tracks from the absolute physical ordinal. A stop generates
no further sequencer triggers and clears only temporary sequencer overlays;
voice tails and external MIDI continue. Resume at the same stopped position
keeps chain/RNG state. A changed stopped position or a discontinuity is a seek.

Seeks use a bounded reset-and-reanchor policy, not historical replay: the
current epoch pattern is selected, track positions are reconstructed from the
absolute ordinal, RNG/probability/chain history resets, and temporary locks
clear. A validated cycle wrap uses the same bounded reanchor and the saved host
epoch pattern. This is deterministic but is not claimed as reconstruction of
unobservable random-next history or live edits. No catch-up loop runs on the
audio thread.

Valid PPQ has priority. If PPQ is missing, exactly continuous sample position
may extend the previous Q32 anchor. If BPM is missing/invalid, the previous valid
tempo may be reused. Without those bounded fallbacks, the callback is invalid
and host sequencing stops. One BPM value applies to the callback; intra-callback
tempo ramps are not invented. Bar and time signature are captured for evidence
but are not used to infer chain state.

The existing per-boundary order remains snapshot acquisition, physical-step
decision, probability/chain RNG, two lock slots, trigger dispatch, then Engine
velocity and LFO rendering. Shuffle, rolls, substeps, shared hats and track-0
chain transitions stay in `TriggerScheduler`; host sync changes only when a
physical boundary is due.

## Verification

- Dedicated native O0/O2: 6,237/0 each; zero measured processing allocations.
- P16-E current source O0/O2: 9,869/0 each.
- Exact private VST3 host transport: 104/0.
- Existing private state/runtime/controller/M2G/M2H/probe: 322/0, 1,143/0,
  955/0, 91/0, 687/0 and 1,033/0.
- Steinberg validator: 47/47.
- REAPER 7.80 x64 loaded the exact private module and passed controls,
  two-instance isolation, bypass, MIDI-only audio, bit-identical repeated
  offline render, save/reopen compatibility and PDC 26. Its empty-bank Host Sync
  render was silent as designed; mandatory pattern-driven cases are BLOCKED.
- The private binary is 825,344 bytes, SHA-256
  `C88C6B264AC70BC4236B564ADE60F1310F2DD18BC526829F7242FD8855C64C7D`.
- The protected installed binary remains SHA-256
  `89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`,
  with all seven resources unchanged.

The portable checkpoint runner freshly built all native programs. Thirty-five
of forty-four executions passed; nine newly generated executables were denied
before entry by Windows Application Control (`WinError 4551`). Dedicated P16-F1
and P16-E O0/O2 executions pass. The denials are classified BLOCKED, not test
failures, and no policy bypass was attempted. The historical exact FIR
Functional O2 denial also remains separately BLOCKED.

Detailed evidence is under `out/p16f1/`. The working implementation is retained
unstaged. P16-F2 is not ready to begin as a publication successor until the
pattern-data ingress/persistence design makes the required REAPER sequencer
audio cases executable and P16-F1 is rerun to a full PASS.
