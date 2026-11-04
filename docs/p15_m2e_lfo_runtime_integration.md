# P15-M2E firmware-faithful native LFO runtime

Classification: **BLOCKED (verification access)**. The bounded native runtime is
implemented. No confirmed production defect remains in executed comparisons.
Final expanded O0 verification passes; Windows Application Control denies the
expanded O2 executable. The earlier O0/O2 runtime fixture passed against the
same production source. That evidence does not substitute for the inaccessible
expanded O2 fixture. The exact historical FIR Functional O2 artifact also remains
BLOCKED. The user's development exception permitted implementation, not a false
certification PASS.

Evidence: `out/p15-m2e/`. Historical M2A–M2D/R1/R2 reports are preserved.
Branch `p12b-4c-fir-resampler`, HEAD
`0091651720cacbcb2ecd3172bd667581ffb83415`. Reference is original
SonicPotions/LXR `dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`, never Catalyst.

## Source-grounded corrections

Firmware paths in this section have prefix
`ref/LXR-0.37/mainboard/LxrStm32/src/`. M2D's numbered sources and dispatch
matrix retain full target provenance. `implementation_matrix.csv` records the
pre-edit differences, minimal corrections and verification obligations.

`DSPAudio/lfo.c:lfo_init` (47–59) initializes phase0, increment1, frequency1,
multiplier1, offset0, retrigger0, sine and sync0. It does not reset the held noise
value. `Lfo::init` preserves that live-init quirk. `Engine::init` explicitly
initializes all six LFOs and clears their noise/PRNG state, routes and registry
before a restored raw patch is projected. Existing voice initializers did not
initialize their LFO members; the first integration comparison exposed that
omission in the new Engine reset path. It was corrected without changing any
assertion. Initial build/failing logs remain in runtime-001/runtime-002.

`DSPAudio/lfo.c:lfo_calc` (61–128) first converts increment to float, multiplies
the effective frequency multiplier, narrows to uint32, and adds with unsigned
wrap. Output evaluation follows this advance. These conversions are retained,
including the apparently redundant float conversion at multiplier1.

Let `p` be the updated uint32 phase, `D=(float)0xffffffff` and `x=p/D`.

|Raw waveform|Implemented expression|Boundary behavior|
|---|---|---|
|0 table sine|`((sine[p>>20]/32767.f)+1)/2.f`|Original immutable table, no interpolation; indices0..4095; maximum below1|
|1 triangle|`1.f-fabsf(x*2-1)`|Starts0, midpoint1|
|2 ascending saw|`p/D`|Unsigned phase normalization|
|3 descending saw|`(0xffffffff-p)/D`|Subtract as uint32 before float conversion|
|4 rectangle|`p>0x7fffffff ? 1.f : 0.f`|Low first half; fixed duty; integer threshold|
|5 held noise|Convert full RNG uint32 to float, then divide by D|Refresh only when old phase exceeds new phase; retain otherwise|
|6 ascending cubic|`x*x*x`|Original multiplication order|
|7 descending cubic|`y*y*y`, `y=(0xffffffff-p)/D`|Unsigned subtraction before conversion|

Outputs are unipolar. Invalid waveform bytes produce0, as in the source.
The sine table is supplied explicitly from the Engine's immutable asset view;
there is no static LFO table-owner pointer. Missing assets yield a safe zero sine
output, not a synthesized replacement. Assets must be loaded for valid rendering.

`DSPAudio/lfo.c:lfo_setFreq/lfo_calcPhaseInc` (136–214), `lfo.h:LFO_SR`:
raw rate becomes `f+=1; f/=128.f; f=f*f*f; f*=200`, then
`f/(REAL_FS/32.f)*0xffffffff`, narrowed to uint32. REAL_FS is the native float
44002.7578125Hz; the LFO cadence is approximately1375.08618Hz. Raw0 remains
0.000095367431640625Hz, not stopped. Rate edits preserve phase and retain free
frequency for later sync0 fallback. Host sample rate is absent from this math.

Out-of-range float-to-uint32 casts are not portable firmware semantics. The
existing ARM-qualified saturation policy is retained for phase offset127 and
guarded phase narrowing; invalid/nonfinite phase arithmetic fails closed. Exact
comparisons cover the defined finite domain, not undefined endpoint behavior on
every C compiler. MCU fast-math/FMA equivalence is unverified.

## Ownership, cadence and event order

The LFO members of drums0/1/2, snare, cymbal and shared hihat are the six owners.
Each Engine has its own state. `Engine::processBlock` calls the complete native
modulation stage once before async processing, even for silent voices, depth0
or destination off. Voice/sample rendering and FIR processing never tick LFOs.
The old `beginVelocityModulationTick` remains a velocity-only module-test stage;
the production block path calls `beginModulationTick`.

The source order is `DSPAudio/mixer.c:mixer_calcNextSampleBlock` (287–307),
`modulationNode.c:modNode_resetTargets/modNode_reassignVeloMod` (108–133) and
`lfo.c:lfo_dispatchNextValue` (130–134):

1. Restore each participating converted physical base.
2. Replay held velocity nodes0..5.
3. For each LFO0..5, advance, evaluate, then immediately dispatch its value.
4. Recalculate affected cutoff coefficients, then run existing async, sync,
   filter, decimator and mixer processing.

There is no end-of-block restoration. Effective values remain live between
blocks. Render scratch previously shared by Engine instances is now bounded
stack storage (six32-frame int16 buffers); no cross-instance scratch survives.

`DSPAudio/lfo.c:lfo_retrigger` (243–269), `DrumVoice.c:Drum_trigger`,
`Snare.c:Snare_trigger`, `CymbalVoice.c:Cymbal_trigger` and
`HiHat.c:HiHat_trigger` scan all six selectors before velocity application and
voice oscillator/envelope trigger work. A selector equals zero-based trigger
identity+1; zero is off. Both hat articulations use identity5/selector6. Accepted
triggers reset matching phases to their current offsets without evaluating or
dispatching an LFO. Invalid velocities are rejected before the scan.

For native block Bk=[32k,32k+31], a change before its boundary can first affect
frame32k; a change after rendering that block cannot alter its cached PCM and
can first affect frame32(k+1). Audible divergence can be later because of zeros
in the oscillator/envelope/filter. Tests verify cached frames1..31 after an
event, the next boundary, global scans, repeated triggers, and native phase
increments while resampling at44100/48000/96000. This does not certify UART/USB
receipt latency or sample-offset host scheduling.

## Native clock contract

`Sequencer/sequencer.c:seq_setBpm/seq_getBpm` (286–296) stores uint16 BPM and
notifies `lfo_recalcSync`; `main.c:237` publishes120 at startup.
`lfo.c:lfo_calcPhaseInc/lfo_setSync/lfo_recalcSync` implements sync1..11 as
`BPM/60.f/4.f` times .25/.5/1/2/3/4/6/8/12/16/32. Invalid sync and sync0 use
the stored free frequency; changing sync or BPM recalculates without phase reset.

`Engine::setNativeTempo(uint16_t)` is a processing-owner-only explicit native
service. It starts at120 and accepts the verified test domain0..300; higher
values reject transactionally. This domain is a plugin capability limit, not
a claim that firmware clamps uint16 BPM to300. No host transport or REAPER tempo
is connected. Future P16 must supply ordered integer BPM publications equivalent
to `seq_setBpm`; broader tempo domains need new coverage. Raw rate/sync setters
use this Engine snapshot. No sequencer is implemented.

## Routing and physical-base ownership

`MIDI/ParameterArray.c:parameterArray_init`, original AVR `front/LxrAvr/Menu/
Cc2Text.c` and `DSPAudio/modulationNode.c:modNode_setDestination` (138–179)
ground physical field identities. Typed native configuration admits exactly the
107 M2C scalar rows (101 physical fields), plus off. D1/D2/D3/SN/CY have18 rows
each; HH has17. Thirty-nine supported indices exceed127 and remain full-width.

One transient LFO route per owner is available through
`configureLfoDestination(SourceIdentity,OriginalTargetIndex)`. It validates the
whole request before mutation. Global and cross-voice destinations, mutable
route-storage addresses, all36 LFO-control rows, UInt32 offsets, unsupported
trigger-consumed/enumeration/feedback targets and ambiguous selectors remain
rejected. `LfoUnavailable` is the preserved M2C diagnostic for deferred source
control rows, not a claim that the ordinary LFO runtime is absent. This is a
disclosed subset of original menu capability, not complete original routing.

The new twelve-entry per-Engine registry keys by physical DestinationId/type,
stores converted base, source mask and processing revision, and resolves fields
on its Engine. No live field pointer is stored. `ParameterStateAuthority<228>`
remains the only saved-patch authority. Route base members remain read-only
inspection mirrors for existing M2C callers/tests; restoration never reads those
mirrors. A new source joins the shared physical base rather than capturing a
temporary effective value. Special frequency aliases always use base1.

After the existing ParameterArray conversion, its existing notification now
refreshes the shared registry and every inspection view. Newer base writes win
before replay. Valid route changes temporarily restore all participating fields
without replay, matching original configuration semantics. Off releases ownership
and retains dirty cutoff restoration work. Rejected changes preserve fields/routes.

Supported reset/restore is Engine initialization followed by complete raw patch
projection; all six voice and modulation states are reset together. An isolated
direct voice.init while Engine routes are live is not a supported Engine lifecycle
service. No unverified hardware kit-loading or single-voice reset semantics are
invented. Explicit routes must be configured again after full reset/restore.

## Shared-field arithmetic

`DSPAudio/modulationNode.c:modNode_updateValue` (182–216) is preserved as
`current*amount*source+(1.f-amount)*current`, with the store/narrowing after each
node. It is not factorized and not additive. Amount and velocity map raw/127.f.
Finite sources/depths must remain0..1; invalid native inputs diagnose/skip without
logging or host notification. Negative finite destination slopes retain sign.

|Scenario|Effective result|Following tick|
|---|---|---|
|Velocity only|V(current) on trigger|B -> held V(B)|
|LFO only|L(B) on native dispatch|B -> next L(B)|
|Different fields|Independent V(Bx), L(By)|Restore both, replay in fixed order|
|Same field|L(V(B)) with per-node narrowing|B -> newest held V -> next L|
|Trigger after LFO|V(L(V(B))) temporarily|Restore B; replay only newest velocity once|
|Several triggers before tick|Each V operates on current effective storage|Restore B, newest velocity once, L once|
|Base update B'|Setter writes/captures B'; no old source snapshot wins|B' -> held V -> next L|
|Route off/reset/restore|Restore/release active ownership, then new lifecycle projection|No stale cleanup after newer patch|

M2D's original state-transition report remains unchanged. The shared cache
deduplicates restoration of equal physical bases; it does not merge modulation
operations or defer UInt8 narrowing. Only the verified scalar domain is enabled.

## Verification and numerical limits

The new runner compiles original C directly through the established compatibility
generator and complete verbatim mixer extractions. The production comparator
uses C++ DSP independently, the original AVR table and a separate public-field
observer. The original C algorithm, tables, assertions and previous fixtures are
unchanged. Strict flags include `-ffp-contract=off`; acceptance is bitwise float,
integer and int16 equality, with no tolerance.

|Execution|Checks|Native voice PCM|Result|
|---|---:|---:|---|
|runtime-003 O0|1,609,225|2,962,176 samples, zero differences|PASS|
|runtime-003 O2|1,609,225|2,962,176 samples, zero differences|PASS|
|expanded runtime-final O0|1,772,097|3,701,760 samples in115,680 cases; zero max/RMS error|PASS|
|expanded runtime-final O2|Not executed|No comparison result|BLOCKED, WinError4551|

Expanded fixtures add noise wraps, maximal twelve-field occupancy, silent phases,
cached-frame/native-boundary timing and host-rate independence. O0/O2 negative
controls disable dispatch only in copied Engine translation units; the unchanged
expanded comparator detects106,494 failed checks and598,568 PCM mismatches in
each mode. Those expected failures are sensitivity evidence, not production FAILs
and not substitutes for the blocked positive O2 run.

The unchanged M2D original-C audit freshly passes224,377 assertions at both O0/O2,
with all67 reference groups exact, max/RMS0. Its observational C++ subfixture does
not supply the sine table and assumes the former init increment; those old
observations are not current runtime assertions. M2E uses loaded original assets.

All107 rows have deterministic rendered-voice coverage across base0/64/127,
velocity0/1/64/127, depth0/64/127 and all eight waves, with shared velocity/LFO
fields and explicit LFO depth variation. This is exact native voice PCM for the
fixture domain. It is not full hardware PCM, arbitrary-patch mixer PCM or REAPER
integration certification. Noise service inputs are independently controlled at
LFO/voice boundaries because original hardware entropy is shared while plugin
PRNGs are per source. No hardware random sequence claim is made.

The legacy standalone test retains18 assertions but replaces the known incorrect
44003-tick and one-based trigger fixture; its nonmatching-trigger check is also
strengthened by changing phase before the call. Its before-source is preserved.
All other existing test sources/assertions remain unchanged.

## Regression, private binary and realtime evidence

Final regression matrix:55 PASS,1 BLOCKED,0 FAIL. The55 passes are42 native,
M2A/B/C/D O0/O2 (eight), and five matching-header private host/validator executions.
The additional blocked row is the unchanged exact historical FIR O2 artifact.
Fresh current-source FIR functional O0/O2, quality and latency tests pass; this
does not certify the historically denied executable. A preliminary ParamMapEnv
O0 denial is separately preserved; the final changed-source native suite passed.
No denied executable was retried or repackaged to evade policy.

Private build: VS Community18, MSBuild18.10.1, Release x64/v145, deploy hooks off.
Bundle has all seven verified resources. Binary780288 bytes, SHA-256
`9FC87F425C2D7241909C56C9494A9D2DD42BCB19E705ABD2FF58675436693C28`.
Fresh host results: state322, runtime1143, controller955, internal probe1033;
Steinberg validator47 pass/0 fail. The probe uses matching current Engine headers.
REAPER was not launched and no installed plugin was replaced.

Measured expanded O0 scopes include configuration, parameter application,
triggers, native blocks and resampled rendering: zero C++ allocations. Earlier
O0/O2 scopes also measure zero. Fixed native state has twelve20-byte base entries
and six added16-byte LFO route views; each LFO grows36 to44 bytes. Engine grows
3448 to3840 bytes (+392), within the1KiB incremental bound. Baseline sizeof
execution was denied; its size is independently emitted as a compile-only layout
constant, not represented as an executed test. See allocation_report.md.

At most12 restores,12 node operations,6 advances and6 cutoff recalculations per
native block. Trigger cost is6 comparisons/up to6 phase assignments and one
velocity node. Base publication/configuration use fixed arrays and identities.
No new locks, waits, queues, atomics, I/O, logging, host callbacks or allocation
are introduced in processing. Existing assets load only outside processing.
All configuration/tempo APIs require the processing owner or stopped processing.

## Host compatibility and remaining gates

IDs0..227, defaults, MIDI CC/NRPN, normalization, LXRS v1/legacy migration,
ParameterStateAuthority, deferred FIFO, controller publication, FIR coefficients
and latency are unchanged. Tests verify raw authority snapshots and serialized
bytes stay identical during modulation. Opaque149..154/161..172 remain inert.
No route, phase, PRNG or effective field is serialized or host-notified.

Native routing capability is not a finished user-facing modulation system.
Persistent full-width selectors, versioning/import/automation semantics and
transactional restore require separate authorization; no LXRS v2 is introduced.
Host tempo, P16, morphing, presets and GUI remain outside scope.

Unverified hardware cases remain entropy, Cortex-M4 fast-math/FMA/out-of-range
casts, UART/USB/DMA receipt timing, external clock jitter, actual sequencer
transport, AVR relay and mutable destination storage. Rejected domains are not
implemented from assumptions.

Next milestone: separately authorize P15-M2E verification closure using the exact
blocked expanded O2 and historical FIR artifacts on an approved execution path.
Keep their identities/assertions, preserve the original-C and private-host results,
and do not expand runtime scope to bypass certification. No commit or push made.
