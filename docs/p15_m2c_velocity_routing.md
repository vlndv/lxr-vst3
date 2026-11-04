# P15-M2C original LXR velocity destination routing

Date2026-11-02. **PASS for the verified native STM32 subset.** This is an internal
DSP capability, not a completed persistent end-user selector feature. All227
host IDs/defaults, seven-bit values, CC/NRPN assignments, LXRS v1, authority/FIFO,
controller synchronization, FIR, native rate/block size and M2B corrections remain
compatible. No LFO runtime, sequencer, locks, morphing, preset or UI work occurred.

Branch: p12b-4c-fir-resampler. Starting/ending HEAD:
0091651720cacbcb2ecd3172bd667581ffb83415. Original firmware:
dee49689f3f0cdf8f9b7b5e79628cfdca1b79394, clean. No Catalyst source was used.

## 1. Pre-implementation evidence gate

`out/p15-m2c/velocity_route_matrix.csv` records all six owners, canonical IDs,
storage, functions, trigger stage, supported rows/types, arithmetic, old path,
proposed correction/tests and ambiguities. `velocity_target_gate.csv` classifies
all205 ordered AVR entries. Both precede production edits; see gate_decision.md,
mandatory_inputs.json, integrity_start.json and gate_probe.log.

The compiled original-C gate independently verifies amount IDs143..148, selector
IDs149..154 and uint16 destination storage for all six. Normal scalar volume
routes execute at trigger and block replay. A bounded probe demonstrates that
the six VEL_DEST target pointers expose only a uint8 low byte of uint16 storage:
455(0x01c7) becomes355(0x0163) under a0.5 source, preserving the high byte. Further
reset/replay could then use a changed address with a stale original-value type.
Those hazardous route-address mutations remain unsupported, not guessed.

| Gate classification | Original rows |
|---|---:|
| VERIFIED_ROUTE | 107 |
| VERIFIED_BUT_UNREACHABLE (LFO path absent) | 36 |
| UNVERIFIED_MUTATION (VEL_DEST storage targets) | 6 |
| UNSUPPORTED_TARGET | 55 |
| INTENTIONAL_NO_OP (explicit off) | 1 |

The107 rows represent101 distinct physical fields: six coarse/fine aliases share
frequency-multiplier storage. Types:75 Float,20 FrequencyMultiplier,12 UInt8.
There are39 supported indices above127. Seven additional STM32 non-menu entries
(output routing215..220 and note227) have no AVR index and are not exposed by this
native index-based API. No target is reordered or truncated.

## 2. Verified original firmware path

Paths below are relative to ref/LXR-0.37/mainboard/LxrStm32/src unless stated.

- MIDI/ParameterArray.c points143..148 to velocityModulators[i].amount,149..154
  to velocityModulators[i].destination; the latter are TYPE_UINT8 despite uint16
  storage in DSPAudio/modulationNode.h:47. The raw selector MIDI handlers remain
  no-ops; they are not unused sound-design features.
- DSPAudio/modulationNode.c:138 modNode_setDestination restores originals and
  captures the new mapped field/type. TYPE_SPECIAL_F captures base1. Other
  scalar targets capture converted DSP units, not raw knob bytes.
- DrumVoice.c:122-125, Snare.c:96-98, CymbalVoice.c:102-104 and HiHat.c:101-104
  apply velocity/127.f after original LFO retrigger and before oscillator phase,
  source-note, pitch/amplitude envelope, transient and filter-trigger operations.
  Closed/open hats share owner5. LFO retrigger itself remains outside this task.
- modulationNode.c:182 saves held lastVal and applies the expression below to
  the current field. mixer.c:287 restores all captured originals, replays held
  velocities0..5, then processes LFOs, filter coefficients, async and sync blocks.
  Here LFO routes remain off; only the verified velocity stage is added.
- MidiParser.c ends sound writes with modNode_originalValueChanged; the new
  ParameterArray post-set notification supplies the corresponding base update.

```text
current * amount * (velocity / 127.f) + (1.f - amount) * current
```

Amount is raw143+owner /127.f. No addition, bipolar offset, extra clamp or raw
parameter re-conversion is introduced. Float order is preserved; UInt8 narrows
after the node by truncation. Negative amplitude slopes move toward zero without
updating their separate inverse attack slope. Pitch uses modNodeValue based at1,
not mutation of raw coarse/fine bytes or frequency knobs. Filter cutoff uses
M2B's converted post-shaper0.45 units and recalculates coefficients after replay.

## 3. AVR versus STM32 selectors

AVR Menu/Cc2Text.c has205 original global indices0..204. AVR Menu/menu.c:2367-2377
translates a velocity selection into a PAR then sends source-owner bits and PAR
bit7/low7. Preset/presetManager.c:259-271 performs the same translation for kits;
its loader bounds stored indices. STM32 MIDI/frontPanelParser.c:538-547 decodes
that internal message and calls modNode_setDestination.

The separate AVR frontPanelParser.c:68-85 NRPN relay explicitly contains TODO
uncertainty about value encoding and paramToModTarget. Physical NRPN behavior
remains **UNVERIFIED**. Native tests call the proven STM32 assignment directly
using independently compiled AVR index translation; no physical relay assumption
is needed for this subset's PASS.

Existing LXRS selector bytes are still opaque/inert. Even seven-bit values that
happen to equal original indices are not evidence of an encoding. No compression,
reinterpretation, extra parameter or serializer field was added.

## 4. Six-voice coverage and supported identities

| Owner | Amount ID | Legacy selector ID (inert) | Supported original rows |
|---|---:|---:|---:|
| D1 | 143 | 149 | 18 |
| D2 | 144 | 150 | 18 |
| D3 | 145 | 151 | 18 |
| Snare | 146 | 152 | 18 |
| Cymbal | 147 | 153 | 18 |
| Shared hi-hat | 148 | 154 | 17 |

Exact original indices, names, pointers, types and per-voice lists are in both
CSVs. Supported PAR IDs are8..19,27..59,62..67,70..77,82..96,99..113,128..133,
191..196 and209..214. These are target identities, not new MIDI assignments.

They cover main/operator/noise pitch multipliers, noise/tonal mix, FM amounts,
cutoff/resonance/drive, amplitude attack/decay/slope, pitch decay/amount, volume,
pan, distortion, voice decimation, filter enumeration and transient pitch.
Source owner must match target voice. Global targets are rejected. The UInt8
subset is pan/filter type only; no Boolean storage or trigger selector is coerced
into a uniform scalar. Filter enum0 reached by modulation retains the original
default processing behavior; no invented enum snapping occurs.

Excluded: all waveforms, repeat, open/closed saved hat decay, transient waveform/
volume, pitch slope with unresolved nonfinite endpoints, Boolean mix/volume
storage differences, feedback modulation of velocity amount, mutable destination
storage, LFO targets and MIDI/global/output configuration. Their exact rejection
reasons are explicit in the gate CSV and typed return status. Inclusion in the
original menu does not certify arbitrary runtime application as safe.

## 5. Typed native configuration and ownership

Engine::configureVelocityDestination(SourceIdentity, OriginalTargetIndex) accepts
full-range typed indices. VelocityRouting.h validates source kind, owner, scope,
availability and the verified subset before resolving a field. Complete candidate
validation precedes restoration/replacement of any existing route. Invalid
requests return exact diagnostics without changing the route or any DSP field.
Engine::configureLegacyVelocitySelector always rejects valid legacy bytes as
AmbiguousLegacySelector; normal raw149..154 setters remain stored no-ops.

Configuration is internal and processing-owner-only, or permitted while audio
is stopped. It is not a thread-safe UI entry point. No host/MIDI call configures
these routes. No new mailbox or queue is added. Host/raw parameter changes retain
the existing authority->processing-boundary->ParameterArray path.

Each Engine owns six fixed VelocityRouteState entries containing full target
identity, physical identity, type, converted base and status. No persistent live
pointer is stored, so Engine copies cannot refer to another instance's fields.
The duplicate incompatible Engine VelocityModulator definition was removed by
including the existing canonical Modulation.h type. Its legacy destination byte
remains unused for runtime routing; full identities live in the new typed state.
The factorized legacy ModTarget helper is not used for rendering arithmetic.

| State | Owner/lifetime | Host/persistence |
|---|---|---|
| Raw base patch | ParameterStateAuthority and ParameterArray; normal edits | Host-visible and LXRS v1 |
| Converted active-target base | Engine route capture, refreshed after base setter | Temporary projection; not independently saved |
| Held velocity and amount | Processing owner; trigger source and raw amount setter | Held source is runtime-only; amount remains normal raw patch ID |
| Effective target field | Original trigger/tick expression, restored before replay | Never copied into raw patch/controller/serializer |
| Explicit route | Native configure until off/change/Engine.init | Not automated or persisted |

Special frequency aliases always keep base1; raw pitch updates do not recapture
their effective multiplier. Direct shortcut member writes/setup helpers are not
base-publication APIs; configure after such setup or use normal ParameterArray
setters. Future shared LFO targets will require a shared physical-base registry.

## 6. Trigger, retrigger, changes, off and restore

At trigger the source is held and original arithmetic acts on the CURRENT field,
before voice initialization. Original C can temporarily compound already-effective
values if multiple triggers occur before the next native tick. This is explicitly
tested, not silently normalized away. No compounded value becomes the captured
or saved base. At every tick the captured base is restored and the latest source
is applied once; there is no accumulating attenuation across blocks/hits. The
supported subset excludes trigger-consumed fields whose audible initialization
would require additional treatment of that transient compounding.

Depth0 leaves the base at replay; depth127 applies the full normalized velocity.
Internal velocity0 is tested, independently of amplitude velocity137..142. MIDI
velocity-zero NoteOn still follows existing note-off/no-retrigger handling.
Selecting voice volume as the destination while ordinary amplitude velocity is
enabled intentionally produces both independent velocity contributions, as in
the original C. No extra amplitude multiplier is added for other destinations.
Invalid native velocities above127 are diagnosed before phase/envelope mutation;
invalid/nonfinite depth/source/base values are rejected or suspend application
without speculative casts or clamps. Valid MIDI behavior is unchanged.

Base edits replace the separate converted base after the normal setter and are
heard at the established native boundary. Amount changes reuse held velocity on
the next tick. Destination changes/off restore the former selected field only;
other voices are not reset. A cutoff-coefficient dirty flag ensures that switching
away from cutoff still recalculates its restored coefficient at the next tick.
That connection was found and corrected by the first differential run.

The hardware setter temporarily restores other nodes globally on route changes.
This implementation avoids unrelated temporary field resets. With one own-voice
route per owner, LFOs off and trigger-consumed targets excluded, next-tick supported
DSP results match original C. It does not claim equivalence of those transient
cross-node configuration side effects or support arbitrary hardware cross-voice
preset routes. Future shared/cross-source routing must explicitly model them.

Engine.init clears routes and held velocities. Existing plugin restore calls
Engine.init then raw replay, so old projects never activate saved selector bytes.
No route survives reset/restore implicitly; callers must explicitly reconfigure
the native capability. LXRS v1 cannot save/restore this full route universe.

## 7. Independent differential and sensitivity verification

The runner uses unchanged original STM32 DSP/ModulationNode and complete original
CC/NRPN handlers through the existing M2B generator. The actual AVR table is a
separate compiled C oracle. Original assignment, trigger and reset/replay functions
execute; C++ field observation is separate from Engine's dispatcher. Assets,
note tables, RNG seed, parameter/trigger sequence and32-frame boundaries match.
No oracle expression or existing assertion/tolerance was modified.

Final evidence: out/p15-m2c/velocity-tests-004-final.

| Mode | Passed | Failed | PCM cases | Samples | Mismatches | Max/RMS error |
|---|---:|---:|---:|---:|---:|---|
| O0 | 61395 | 0 | 11989 | 383648 | 0 | 0 / 0 |
| O2 | 61395 | 0 | 11989 | 383648 | 0 | 0 / 0 |

All107 rows have actual effective-field change proofs. Tests cover min/mid/max
bases/depths, velocities0/1/64/127, signed/UInt8 semantics, source scopes, indices
above127, held edits, aliases, repeated triggers, multiple voices, off/change,
reset/restore, actual MIDI dispatch, independent amplitude gating and unchanged
authority/raw/LXRS bytes. Both simple and cutoff-disable coefficient transitions
are compared. Full Engine::processBlock produces a detectable modulation effect.

A deliberately disconnected applyVelocityValue body in an ignored source COPY
causes7753 failed checks and30130 sample mismatches under unchanged assertions.
The detector therefore PASSes; the negative executable's expected exit is1.
This is not a production feature flag or an ignored production failure.

velocity-tests-001 retains the initial cutoff transition failures and the new
fixture's malformed three-field MIDI event; its event initializer omitted the
channel member. The fixture now supplies all four fields, and cutoff restoration
is fixed. Assertions were not weakened. Subsequent runs pass; all logs remain.

## 8. Runtime safety and verification limits

Measured per mode:1224453 parameter update calls,4227 triggers,4185 configuration
calls and11989 rendered comparison cases, zero counted C++ new/new[] allocations.
Original C is also inside measured scopes where applicable. Source audit finds
no new malloc, new, mutex, I/O, logging, host callback, unbounded loop or runtime
table construction. Routing uses fixed arrays/switches, at most six owners and
pre-existing immutable metadata. Processing remains the sole DSP writer.
Whole-DLL allocator hooks, TSan and physical MCU execution are not claimed.

Broad CY/HH tests use controlled noise carriers; focused actual FM cases use
disclosed positive initial operator intervals, matching the existing M2B contract.
Arbitrary negative-FM x64-C/ARM casts, ARM fast-math/FMA, unknown ROM guards,
uncontrolled hardware RNG, SD samples and factory kit bytes remain qualified.
No deterministic mismatch was tolerated in the declared comparison domain.

## 9. Fresh regressions and isolated plugin

All53 positive suite executions PASS:42 existing native executions (21 O0/O2),
M2A contract O0/O2 each121781, M2B differential O0/O2 each30169/52416 exact samples,
new velocity O0/O2 above and five fresh private-bundle host/validator executions.
The existing LfoModulation test includes the standalone velocity helper and still
passes18/18. Native runtime419, concurrency35, codec29, snare61, filter80000 blocks,
FIR quality38 and portable latency214 each pass at both modes. Other module suites
and exact fresh commands/results are in velocity_test_results.json.

VS Community18/MSBuild18.10.1/Release x64/v145 private build PASS. Pre/post deploy
events disabled; seven packaged resources match tracked assets. Private executable
SHA-256:4EE573DD68724E6E162113231F04C904A4E2F8D489F75E74EE4DDD27462660D2,
774656 bytes. Host state322/322, runtime1143/1143, controller955/955,
native/FIFO probe1033/1033 and Steinberg validator47/47 freshly PASS on that binary.
No REAPER or subjective listening claim is made; neither was run.

Installed certified executable remains SHA-256:
89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82.
Before/after receipts, dependency verification and exact incremental file/hash
inventory distinguish M2C from the preserved uncommitted M2A/M2B work. M2B's four
corrections and all historical reports remain intact. No stage/commit/push or
branch operation occurred. Phase-log append is factual and preserves prior bytes.

## 10. Readiness and remaining boundary

The verified native STM32 subset is ready for explicitly configured processing-
owner use and further fidelity tests. Persistent user selection/automation/save/
restore is NOT complete. No versioned selector design or LXRS extension has been
approved or implemented. AVR physical NRPN relay and six destination-storage
mutations remain unverified/unsupported. See selector_limitations.md and gate CSV.

Recommended separately authorized P15-M2D: original-C-backed LFO primitive/tick,
waveform, retrigger/sync/reset verification, followed by a shared converted-base
and ordered physical-target design before runtime integration. Collisions/aliases,
cross-source trigger behavior, type narrowing and host selector/version policy
must be settled explicitly. Do not build LFOs on the factorized legacy helper or
assume this one-route-per-owning-voice model already covers shared LFO targets.
Sequencer, morphing, LXRS v2, preset management and GUI remain deferred.
