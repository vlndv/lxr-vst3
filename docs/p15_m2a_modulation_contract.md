# P15-M2A typed modulation and selector contract

Date: 2026-11-02. **PASS, contract scope only.** The new layer is not included by
the renderer or plugin. No synthesis equation, parameter mapping, MIDI dispatch,
host ID/default, state codec, FIR, latency or packaging behavior changed. P16,
modulation application, morphing, presets and UI were not implemented.

Baseline branch: `p12b-4c-fir-resampler`. Starting/ending HEAD:
`0091651720cacbcb2ecd3172bd667581ffb83415`. Original SonicPotions/LXR reference:
`dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`, clean; no Catalyst reference.
iPlug2 unpacked source matches `d54f69050f517e43b941d88c2a170f0a840b9ee4` and the
documented patch/SDK overlay. Certified installed VST3 SHA-256 remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`.

## 1. Source, table structure and counts

Authoritative table: `ref/LXR-0.37/front/LxrAvr/Menu/Cc2Text.c:19`, with actual
AVR parameter enumeration in `front/LxrAvr/Parameters.h`. Mainboard pointer/type
metadata: `mainboard/LxrStm32/src/MIDI/ParameterArray.c:82` and `.h:39`.
All firmware-relative paths below are under `ref/LXR-0.37/`.

| Quantity | Count / bounds | Meaning |
|---|---:|---|
| Ordered AVR table | 205, indices 0..204 | Original order retained exactly |
| Off | 1, index 0 | PAR_NONE; null storage, not an active DSP destination |
| Non-null table references | 204 | Includes configuration/dispatch fields |
| Non-off normal voice-menu entries | 203, indices 2..204 | LFO destination lists; velocity limited to owner |
| Normally automation-only menu entry | 1, index 1 | Global decimation; normal LFO/velocity menus omit it |
| Distinct table storage fields | 198 | Six additional coarse/fine rows alias existing fields |
| Alias groups | 6, containing 12 rows | Coarse/fine share oscillator modNodeValue |
| Mutable destination entries needing review | 6 | VEL_DEST targets; represented but runtime use deferred |
| Additional STM32 pointer targets absent from table | 7 | AUDIO_OUT1..6 and MIDI_NOTE7 |
| All STM32 non-null parameter references | 211 | Menu references plus seven additional parameters |
| Distinct storage fields across full STM32 registry | 205 | This is a different count from the 205 AVR rows |
| Unknown table identity mappings | 0 | Unknown legacy encoding is a separate issue |

The 203 count excludes off. A normal LFO can select these across six destination
voices plus off; a velocity modulator has only its own voice's subset plus off.
These counts do not claim 203 independent, immediately audible modulation paths.
The current certified plugin's modulation routes remain disconnected.

| Destination voice | AVR table indices | Non-off choices |
|---|---|---:|
| D1 | 2..35 | 34 |
| D2 | 36..69 | 34 |
| D3 | 70..103 | 34 |
| SN | 104..136 | 33 |
| CY | 137..170 | 34 |
| HH, shared closed/open | 171..204 | 34 |

Bounds come from `Cc2Text.c:310`; nearby comments saying 33/32 are not inclusive
counts. The 43-position gap bitmap at `Cc2Text.c:323` describes logical menu
positions across heterogeneous voices. A missing logical type maps to off when
changing destination voice. Logical position 0 is coarse tune, **not off**;
the original off/no-slot sentinel is 0xff.

`out/p15-m2a/target_manifest.csv` records all 205 original indices, parameter and
label symbols, compiled label IDs, native domains, scope, type, availability,
aliases, MIDI disposition and source locations. `internal_only_targets.json`
records the seven additional pointer targets without inventing table indices.
No alphabetic reordering or duplicate-row removal occurred.

## 2. What selection and dispatch actually permit

Normal encoder/pot selection is checked in `front/LxrAvr/Menu/menu.c:1540` and
`:2248`. Velocity uses its source/owner voice's range. LFO uses the separately
selected destination voice's range. Changing VOICE_LFO preserves logical target
type through the original gap map (`menu.c:1569`, `Cc2Text.c:339,375`).

Kit read/restore is less restrictive. `Preset/presetManager.c:177` reads raw byte
arrays, replaces target indices >=205 with off, and zero-fills short data.
`preset_sendDrumsetParameters` at `:258` sends each stored target directly through
the table; it normalizes invalid VOICE_LFO hints to 1 but does not reconcile the
target row with that hint or enforce the velocity owner's menu range. Thus a
stored row determines the actual DSP destination even if its menu hint disagrees.
The pure preset resolver preserves this distinction and reports menu consistency.
It rejects invalid typed indices rather than silently guessing their meaning.

The comment on entry 1 says automation-only; normal menus agree. Raw kit/internal
dispatch does not exclude that row. The contract distinguishes normal menu
availability from source-level dispatch acceptance rather than treating the
comment as an additional runtime guard.

STM32 internal messages (`MIDI/frontPanelParser.c:428,538`) send a source-owner
number plus an actual PAR ID. Their payload is:

```text
upper = (sourceOwnerZeroBased << 1) | ((PAR & 0x80) >> 7)
lower = PAR & 0x7f
```

The message status identifies velocity or LFO. Neither the table index nor the
destination voice is encoded in the source-owner bits. Typed encode/decode
helpers validate all fields and also account for the seven non-menu STM32
targets. Null/unmapped PAR IDs are rejected. They do not send any messages or
change the certified MIDI address space.

## 3. C++ data model

Canonical header: `dsp/ModulationContract.h`; immutable generated data:
`dsp/ModulationContractData.inc`; accepted facade:
`interfaces/ModulationContract.h`. Namespace: `lxr::modulation_contract`.

| Type | Identity/domain |
|---|---|
| Voice / Track | Six synthesis voices versus seven trigger tracks; both hats map to HiHat |
| SourceIdentity | SourceKind (velocity/LFO) plus fixed owner voice |
| OriginalTargetIndex | Validated 0..204 AVR table row |
| HostParameterId | Existing 0..227 host slot |
| FirmwareParameterId | Original 0..227 mainboard PAR identity; distinct C++ type |
| DestinationId | Physical storage identity; coarse/fine aliases deliberately share it |
| SevenBitValue | Validated 0..127 external byte |
| OriginalVoiceSelector | Original destination-menu voice hint, with explicit 1..6 conversion |
| LogicalTargetSlot | Original 0..42 gap-map coordinate, not a host encoding |
| LegacySelectorValue | Host selector ID plus unchanged opaque seven-bit byte |
| ExplicitSelection | Source, full row, physical destination, resolved target voice and independent menu hint |
| ParameterValueType | Exact original type tags, including unused special tags and null |
| ValueSemantic / Consumption | Native field domain and when/how the original consumes it |
| TargetAvailability / RuntimePolicy | Normal menu availability versus application-review constraints |

Bounded IDs have no public unchecked or narrowing constructor, no implicit
integer conversion, and distinct tags. `fromRaw(uint64_t)` validates before
narrowing; invalid construction retains an invalid sentinel. Results carry
explicit status, with no active selection returned on failed legacy decoding.
Callers must check status. Metadata contains source strings, not live DSP pointers.

Pure helpers provide lookup, host/PAR identity conversion, menu validation,
explicit original kit resolution, logical retargeting, numeric packing checks,
MIDI address descriptions and internal-message conversion. Physical-to-host
reverse lookup rejects coarse/fine ambiguity instead of choosing a host ID.
Full target-to-host row conversion is unambiguous.

All data are inline constexpr fixed arrays. Compile-time validation checks sizes,
indices, unique PAR IDs, ranges, bitmap populations, physical identities and
non-menu entries. Lookups are bounded scans or direct indexing. No mutable global
state, allocation, locks, engine dependencies or runtime table construction exist.
The layer performs no modulation arithmetic and does not evaluate an audio sample.

## 4. Value types and operation semantics

Original tags among the 205 rows: 100 TYPE_FLT, 72 TYPE_UINT8, 26 TYPE_SPECIAL_F,
6 TYPE_UINT32 and 1 null. Special pan/filter tags exist in the header but are not
used by these table rows. Pan is UINT8 despite centered front-panel display.
The six velocity destination members are uint16 but advertised as TYPE_UINT8.
The independent fixture uses the actual original ModulationNode declaration and
checks this width discrepancy.

`DSPAudio/modulationNode.c:182` applies multiplication to the **mapped engine
field**, not raw knob values, using the original expression/order:

```text
current * amount * source + (1.f - amount) * current
```

No additive modulation, nearest-value quantizer or interpolation rule is implied.
UINT8/UINT32 targets narrow after each node; float targets retain original float
operation order. There is no extra menu clamp in the node. Out-of-range/nonfinite
C conversions require existing ARM/fidelity qualifications, not a guessed wrap
or saturation rule implemented here. No such computation is added by this phase.

| Domain | Required future behavior / limitation |
|---|---|
| Frequency-scaled coarse/fine/noise/LFO rate | Multiply shared modNodeValue based at 1; do not overwrite raw tune bytes or rerun Hz conversion |
| Cutoff, resonance, drive, distortion, decimation | Operate on converted field; retain coefficient timing and original units/floors |
| Envelope time | Multiply the decrement step; attenuating a step can lengthen time |
| Signed slope | Multiply coefficient, not a centered UI integer; amplitude inverseSlope is a different field |
| Pan / repeat / wave / filter enum | Integer narrowing at each node; no invented enum interpolation or validity clamp |
| Boolean controls | Original storage is UINT8/nonzero truth, not C++ bool; do not normalize arbitrary nonzero raw bytes to 1 before original-style modulation |
| Hat open/closed decay, repeat, retrigger/offset | Trigger-consumed configuration; changing saved settings need not change an already-copied active value |
| Sync byte | Direct node byte writes do not themselves recalculate phaseInc; setters/tempo updates do |
| MIDI note filter | MIDI dispatch configuration, not a per-sample audio field |
| VEL_DEST target | Low-byte destination mutation can alter restoration/aliasing; represented, explicitly RequiresDestinationMutationReview |
| Off / null / absent menu parameters | Not a modulation destination; no invented reserved rows |

The six mutable destination targets have known identities and source metadata,
but future runtime application remains unsupported until dedicated original-C
vectors settle mutation/restoration hazards. This is not an unknown index mapping.
Other typed fields are represented, not certified safe for every arbitrary value.

Absolute sequencer replacement is a separate raw-parameter setter operation.
It can invoke a direct original CC/CC2 setter where one exists; it is not an
alternative ModulationNode formula. A menu listing does not make VEL_DEST's
STM32 no-op handler into a functioning automation setter. No lock computation or
interpolation is supplied by this contract.

## 5. Selector-width problem and architectural options

Original AVR VEL_DEST and TARGET_LFO store **global table indices**, in uint8
kit fields. VOICE_LFO stores a destination voice hint. DSP ModulationNode stores
the translated PAR in uint16. The port's host values, authority validation and
LXRS v1 all require 0..127. Indices 128..204 (77 rows), including all non-off
CY/HH rows and the upper SN rows, cannot be packed losslessly into seven bits.
Even indices <=127 do not prove a legacy encoding.

| Option | Assessment |
|---|---|
| A: full typed internal indices, unchanged external selectors | Necessary and implemented; complete internal identity, but cannot supply full legacy persistence or infer routing meaning |
| B: per-voice translation tables | Original logical-slot map is exact for explicit menu operations and fits small coordinates. It would be a new external encoding, not evidence about old bytes; arbitrary stored kits need not be menu-consistent |
| C: future versioned extension | Required for lossless persistent active routing with explicit encoding/provenance. Must not claim an extended payload is unchanged LXRS v1 |
| D: independent explicit destination identity | Implemented as pure data model; preserves row, storage alias and menu hint separately while relating to unchanged host slots |

Selected smallest correct current design: **A+D**, with **C a required future
state/host-semantics dependency**. B helpers exist for source-backed menu
translation only; no compressed seven-bit host scheme is assigned. Detailed
tradeoffs across fidelity, coverage, compatibility, automation, MIDI, persistence,
realtime operation, locks, morph and migration are in
`out/p15-m2a/implementation_decisions.md`.

## 6. Legacy and future migration contract

The certified plugin's 18 selector slots (149..154 and 161..172) were inert.
Their historical bytes have no established routing-encoding tag. All 2,304
slot/value combinations are preserved verbatim by `preserveLegacySelector`;
`decodeLegacyForRouting` returns `LegacyEncodingUnspecified`, including for zero.
No old project acquires an active route merely by including this header.

`originalIndexAsSevenBits` reports numeric feasibility only: 0..127 fits;
128..204 returns `NoSevenBitRepresentation`. It is not a v1 routing encoder.
Passing host ID 128 as target index 128 without an explicit type conversion is
not possible. Passing target index 128 to seven-bit construction cannot silently
turn it into zero.

Future migration requirements, without implementing a serializer:

1. Retain the original 228 raw bytes exactly when importing LXRS v1. Initialize
   any future explicit routes off; retain opaque selector bytes for roundtrip.
2. Use a new format/version with an explicit OriginalAvrTargetIndexV037 encoding
   tag and pinned table identity. Preserve six velocity and six LFO full-row
   references, source identity and independent LFO menu-voice hints as needed.
3. Persist original row identity, not only DestinationId: fine/coarse aliases
   otherwise lose the user's selected menu item. No raw C++ object bytes/pointers.
4. Validate the complete candidate before coherent publication. Reject unknown
   versions, indices or sources. Diagnose menu-hint mismatches instead of changing
   the actual row to match the hint. Define six hazardous target policies before use.
5. Refuse a purported lossless export to LXRS v1 if explicit route state cannot
   be represented. No silent truncation or discarded routes.
6. Decide versioned host-selector semantics separately before enabling old IDs
   for new routing. No new parameters, ID changes or MIDI remapping are implied.

The existing v1 header requires flags/extension size zero and rejects unsupported
versions/trailing data. Merely appending a route block cannot be called v1
compatibility. Serializer, authority width and controller normalization are
unchanged in this milestone. Shipping host-controlled active routes still needs
the separately authorized version/semantics decision; internal representability
does not resolve that product compatibility dependency.

## 7. AVR versus STM32 and MIDI findings

VEL_DEST: AVR parameter_values stores table index; menu translates to PAR and
sends CC_VELO_TARGET; STM32 binds the corresponding velocity node. TARGET_LFO:
same table-index translation via CC_LFO_TARGET. VOICE_LFO: destination-list hint
and logical-target remap on a menu operation; it is not a modulation source
selector or a DSP pointer by itself. All three families are saved in kits.

Direct STM32 MIDI handlers do not configure these selector destinations. AVR
`frontPanelParser.c:38` contains an NRPN relay for VEL_DEST/TARGET_LFO, translating
an incoming actual PAR value and returning an internal destination message; its
comments explicitly say it needs checking. Physical relay behavior remains
**UNVERIFIED**. VOICE_LFO NRPN stores an AVR value without the menu remap path.
Typed `OriginalMidiDisposition` preserves these distinctions. No statement that
every hardware selector is categorically unwritable over MIDI is made.

Certified port address space is preserved: nominal CC1..127 -> host ID1..127,
NRPN1..100 -> IDs128..227. CC98/99 are parser selectors; CC6 is conditional data
entry. Original firmware uses CC=PAR+1 for PAR1..126 and NRPN=PAR-128 for the
upper range; PAR127 has no valid CC128. Protocol/reserved host bytes retain their
existing no-op/storage meanings. Address helpers describe both spaces without
dispatching MIDI or changing an existing assignment.

Original global-channel/track-channel behavior, persistent NRPN selection, relay
channel normalization and AVR/STM32 mute-number differences require separate
compatibility review. NRPN200..206 remain intercepted by the certified parser;
they are not sound parameter IDs200..206. No new compatibility mode is supplied.

## 8. Future base/effective ownership

This is an ownership contract, not a runtime implementation or scheduler.

| Layer | Owner/lifetime | Host-visible and persisted? |
|---|---|---|
| Raw base patch | Existing authority; until explicit user/host/MIDI base edit | Yes, unchanged228-slot LXRS1 representation |
| Explicit route configuration | Future coherently published validated configuration | Future tagged format; never inferred from old bytes |
| Locked raw value | Audio-owned automation/step overlay; restores old destination when node changes | Does not permanently replace base patch; readback policy must be explicit before implementation |
| Converted effective base | Audio-owned, after appropriate raw setter conversion | No independent serialized DSP pointer/float snapshot |
| Velocity-modulated field | Trigger-held source, applied on trigger and replayed each native tick | No; note velocity is runtime state |
| LFO-modulated field | Ordered per-native-tick effective state | No; phase/RNG/runtime fields are not new patch parameters |
| Final DSP field | Audio owner; consumed at tick/sample/trigger/configuration stage | No; must never overwrite saved raw base |
| Host/controller projections | Existing IParam/controller caches of authoritative base state | Existing P12c generation rules retained |

Firmware order is reset originals -> replay velocity0..5 -> calculate/apply
LFO0..5 -> recalc filter coefficients -> voice async -> sample blocks -> mixer.
Triggers retrigger all selected LFOs, apply velocity, then update phases/EGs and
trigger-consumed configuration. Base+effective separation must preserve per-node
integer narrowing and float operation order; a single final multiplier/cast is
not automatically equivalent. No effective value accumulates permanently into
the base between ticks.

Shared physical storage and parameter identity are distinct: coarse/fine base
setters alter different packed note bytes, while their modulation targets share
one frequency multiplier. Amplitude slope's complementary inverse coefficient
must not become an unintended second modulation target.

## 9. Sequencer and morph prerequisites

Original `DSPAudio/automationNode.c:48` restores a previous automation destination
from original CC values and applies the new raw absolute value without replacing
that original. `Sequencer/sequencer.c:321,333` processes two automation slots
before the trigger. Future locks need audio-owned temporary overlays, exact
two-slot/restore order and explicit precedence with host/MIDI/restore.

Current authority mailboxes preserve latest values, not a timestamped event
history. The 4096-event MIDI FIFO is restore deferral, not a parameter-lock
scheduler. iPlug2 reads the last queue point; LXR does not schedule it onto native
32-frame ticks. Native/FIR cache timing, overflow, transport and parameter
readback under locks require a separate design. Mailbox CAS fallback also has no
fixed retry cap; fixed storage alone is not a hard latency guarantee.

Original morphing interpolates raw sound values using a256 denominator and
0x7f rounding threshold (`Preset/presetManager.c:840,856`). Selector routes are
installed separately on kit load and retained when saving a morphed kit; a
generic interpolation of route IDs would be wrong. Preserve two immutable raw
anchors, route identity and separate effective scalar state. Do not use repeated
UnserializeState to morph: it resets voices/FIR/parser. AVR integer-promotion
edges and full morph/lock interactions remain unverified here.

## 10. Tests, independence and reproducibility

Test: `tests/P15M2AModulationContractTest.cpp`. Reference fixtures:
`tests/reference/modulation/original_avr_targets.c` and
`original_stm32_targets.c`. These compile the unchanged original C and headers,
not the generated C++ table. AVR flash reads and STM32 integer includes have
host-only shims; mock voice fields expose pointer identity/type without rendering.
The actual original ModulationNode declaration is used. This verifies metadata,
not hardware register behavior, exact full voice layout or DSP equations.

Final fresh source builds at **O0 and O2 each pass121781 checks**, fail0, measured
C++ lookup/conversion allocations0. Coverage includes:

- All205 parameter/label IDs, order, source trace, compiled types and voice ranges.
- Every pairwise menu storage-alias relationship against original C pointers.
- All original gap slots/voice translations, menu validation and kit distinctions.
- All228 host/PAR identities, existing CC/NRPN helper behavior and interception.
- All2304 legacy selector values preserved and refused as unspecified routes.
- Full indices0/127/128/204, invalid205/wide inputs, reserved/null IDs, incompatible
  voices, internal-message roundtrips including seven additional pointers.
- Independent golden LXRS v1 size/header/CRC/payload and decode roundtrip.
- Strong-type/static bounds and deterministic repeated immutable lookup.

The first run found40 wrong label IDs: the extractor counted disabled
USE_CODEC_EQ enum entries. The parser was corrected; assertions were not weakened.
`contract-run-001` retains that failure. `contract-run-002` passes both builds;
`contract-run-003-final` passes the final accepted interface and linear constexpr
validation. New execution products include source manifests, commands, logs and
executable hashes. No execution-policy workaround was needed or attempted.

Fresh existing regressions:15/15 PASS. ParameterArray, ParamMapEnv, ParamMapMisc,
MIDI parser, LXRS codec and P12c-R1 concurrency each ran at O0/O2. Installed state
passes322/322; controller955/955; native/controller probe1033/1033. Existing test
executables were hash-verified, their preserved source inputs match current
unchanged DSP/test inputs, and their fresh executions are distinguished from
historical certification. No VST3 rebuild/install or REAPER process was required.
P15-M1's blocked audio harness was not retried or counted as evidence here.

Reproduce from the repository root with the documented native compiler setup:

```powershell
python tools/verification/extract_modulation_contract.py --output out/new-contract-extraction --check-data dsp/ModulationContractData.inc
python tools/verification/run_modulation_contract_tests.py --output out/new-contract-tests
```

The test runner refuses an existing output directory and never installs a plugin.
`--cc`/`--cxx` allow explicit GCC executable paths. Reference shims/flags and
limits are documented in `tests/reference/modulation/README.md`.

Machine-readable evidence: `out/p15-m2a/target_manifest.csv`,
`selector_compatibility.json`, `contract_tests.json`, `implementation_decisions.md`,
`extraction_provenance.json`, `regression_tests.json`, input/command manifests and
before/after integrity receipts. Original custom non-commercial/full-source
licence continues to apply; no production dependency was added.

## 11. Unresolved questions and next scope

- Legacy selector encoding has no proven routing meaning: explicitly unsupported
  for automatic migration. A future versioned semantics/persistence change needs
  separate authorization; this does not block pure contract acceptance.
- Six destination-mutation targets, AVR NRPN relay behavior, ARM cast/NaN/FMA/RNG
  and ROM guard qualifications remain unresolved. Do not guess their behavior.
- The existing two incompatible VelocityModulator definitions and disconnected
  ModulationSystem remain unchanged. Consolidate only during authorized runtime
  integration; the new namespace does not reuse either conflicting type.
- Complete sequencer/morph scheduling, lock readback policy, source feedback
  consumption order, default factory kit bytes and hardware audio fidelity are
  not certified by these metadata tests.

Proposed **P15-M2B**: isolated original-C-backed repairs for cutoff37..42,
CY/HH operator29/30/33..36, live SN/CY/HH pitch14..19 and drum instant-attack
exception49/51/53. Keep current MIDI/host/state contracts; use matched initial
conditions and mapped audio vectors, then regression tests. No such repair was
made here. The typed contract is ready for that work and later explicit-route
test planning; shipping active selector routing still depends on the future
compatibility/version decision. P16 and morph implementation remain deferred.
