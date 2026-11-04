# P15-M2D LFO primitive, timing and physical-field ordering audit

2026-11-02. **BLOCKED by mandatory Windows Application Control denials.**
The source investigation and bounded M2E design are recorded; runtime LFO
integration is not implemented. The final O0 original-C harness passes224377
assertions. Its final O2 executable was denied with WinError4551. M2A O0,
M2C O0 and six checkpoint executables were also denied. No security bypass,
production correction, plugin build/install or persistent selection was made.

Branch before/after: `p12b-4c-fir-resampler`. HEAD before/after:
`0091651720cacbcb2ecd3172bd667581ffb83415`. Original SonicPotions/LXR reference:
`dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`, clean. No Catalyst reference.
The certified installed VST3 remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`.

## Evidence and classifications

Evidence root: `out/p15-m2d/`. `integrity_start.json` snapshots8590 existing
source/evidence files and the index before task writes. Existing M2C task hashes
also match its historical receipt. Required docs and evidence hashes are in
`mandatory_inputs.json`. `source_catalog.json` supplies exact function bodies,
paths, inclusive line ranges and source/body hashes. `numbered_sources/` retains
the full inspected routing/timing/DSP/wrapper files with line numbers.

All short firmware paths below are relative to
`ref/LXR-0.37/mainboard/LxrStm32/src/`. AVR paths explicitly begin
`ref/LXR-0.37/front/LxrAvr/`. Source text, source execution and hardware behavior
are separate claims:

| Classification | Meaning in this audit |
|---|---|
| Confirmed firmware source | Exact C/AVR path and operations identified |
| O0 VERIFIED | Unchanged original C executed against exact deterministic assertions |
| Confirmed C++ divergence | Current code contradicts the original, or measured primitive comparison differs; left unchanged |
| Source-inferred, untested | Ordered source path understood, no complete transport/hardware execution |
| Hardware-dependent / UNVERIFIED | Entropy, MCU arithmetic endpoints, receipt-to-DAC timing, clock jitter |
| BLOCKED | Required executable denied/unavailable; historical PASS is not substituted |

## Primitive contract

`DSPAudio/lfo.h:44-83` defines eight wave IDs0..7 and a uint32 phase/increment,
uint8 waveform/retrigger/sync, uint32 offset, float random/frequency/multiplier
and embedded ModulationNode. Every waveform is unipolar under its valid finite
inputs; no duty control or bipolar depth exists. `lfo_calc:61-128` advances phase
**before** evaluating its waveform. Inputs and conversions are in
`lfo_primitive_matrix.csv` and `lfo_scalar_matrix.csv`.

Let D be the float conversion of0xffffffff, which is4294967296.f, and P the
already advanced uint32 phase. Preserve the parentheses and types below.

| ID / waveform | Original expression and source lines | Endpoints / qualification |
|---|---|---|
| 0 sine | `((sine_table[P>>20]/32767.f)+1)/2.f`, lfo.c72-74 | 4096 int16 indices, no interpolation. Table is approximately -cos, not conventional sin; exact extrema0 and0.9999847412109375 |
| 1 triangle | `1.f-fabsf((P/D)*2-1)`,76-78 | 0 at phase0,1 at half cycle,0 near wrap after float rounding |
| 2 saw up | `P/D`,80-82 | float quantization can round to1 before uint32 wrap |
| 3 saw down | `(0xffffffff-P)/D`,105-107 | unsigned subtraction **before** float conversion; do not substitute1-P/D |
| 4 rectangle | `P>0x7fffffff ? 1.f : 0.f`,84-93 | first half low, second half high; exact integer threshold, fixed50% duty |
| 5 held noise | on `oldPhase>P`, assign RNG to float rnd, divide rnd by D; return rnd,95-103 | no new RNG call without wrap. Hardware samples are not deterministic reference data |
| 6 exponential up | `x=P/D; x*x*x`,109-114 | cubic, left-associated float products |
| 7 exponential down | `x=(0xffffffff-P)/D; x*x*x`,116-121 | unsigned subtraction first, then cubic |
| other uint8 | return0,123-128 | no wrap-to-valid-wave or enum snapping |

The sine binary used by the harness is checked byte-for-byte against the
unchanged original wavetable initializer (4097 entries including its guard;
LFO indices use only0..4095). Source and binary hashes are retained in
`lfo-tests-002/sine_provenance.json`. Table values at indices0/1024/2048/3072 are
-32767/12/32766/-37; ideal quarter-cycle trigonometry is not a valid oracle.

`lfo_init:47-59` sets phase0, increment1, offset0, retrigger0, waveformSINE,
sync0, freq1 and modNodeValue1, then initializes its modulation target. It does
**not** initialize rnd. Static globals initially provide zero; live LFO reinit
retains the previous random float. Increment1 is not the increment for1Hz.
Voice init, subsequent tempo publication and raw parameter setters must not be
conflated with this primitive's initialization. Actual factory-kit startup
content is outside this audit.

## Rate, phase and amount

`lfo_setFreq:207-214` takes float raw f and performs:

```c
f += 1;
f = f/128.f;
f = f*f*f;
lfo->freq = f*200;
lfo->phaseInc = lfo_calcPhaseInc(lfo->freq,lfo->sync);
```

The raw0 rate is0.000095367431640625Hz, raw64 is26.19028091430664Hz and raw127
is200Hz. Zero raw is **not stopped**. Explicit zero frequency passed to the
increment helper yields0. Native REAL_FS is44002.7578125f; LFO_SR is
REAL_FS/32.f =1375.086181640625Hz (`config.h:53,86`; lfo.h definitions).
Free increment is `freq/LFO_SR * 0xffffffff`, with float rounding then uint32
truncation (`lfo_calcPhaseInc:136-205`). Native block duration is about0.727227ms.

At every advance (`lfo_calc:63-67`): uint32 phaseInc converts to float, multiplies
effective float modNodeValue, converts back to uint32, and adds to phase using
unsigned wrap. This second narrowing is observable; it must not be collapsed
to integer phaseInc when multiplier1. The integer-to-float conversion can round
up. Phase/wrap fixtures include near-maximum phases and increment conversion
edges, with only defined in-range float-to-integer conversions asserted.

Frequency/sync setters preserve phase. Wave changes select another expression
at the next calculation, without phase reset. Offset writes store a future
reset value; they do not alter current phase. The six MIDI handlers use
`raw/127.f * 0xffffffff` (`MidiParser.c:1028-1044`, IDs185..190). Raw0..126 are
exactly tested; raw127 produces float2^32 and ISO C conversion is out of range.
The existing port's saturated0xffffffff endpoint follows the previously
qualified ARM policy, not portable original-C equality or hardware measurement.
No new ARM endpoint claim is made here.

Amount IDs121..126 map raw/127.f to LFO node amounts;143..148 do the same for
velocity. `modNode_updateValue:182-216` saves lastVal and applies:

```text
current * amount * source + (1.f - amount) * current
```

It reads the current effective physical field, not the saved original. Float
targets retain expression order; UINT8/UINT32 targets narrow **after each node**.
No sign inversion, bipolar offset, nearest rounding or validity clamp is added.
Zero-depth finite Float/UInt8 identity is covered. High UInt32 values can round
through float even at depth0, and max phase-offset casts can be out of range;
those endpoints remain qualified, not silently cleaned up.

## Native timing and first affected sample

`main.c:267-299` calls usb_tick, parses UART MIDI, parses front-panel input,
parses an available USB message, generates a needed block, then seq_tick and
trigger_tick. `calcNextSampleBlock:180-188` calls the mixer. Hardware DMA buffer
readiness is separate (`Hardware/cs4344_cs5343.c:74`); physical receipt times do
not establish a particular sample label without hardware evidence.

The complete `mixer_calcNextSampleBlock:287-378` establishes:

1. Restore six velocity targets then six LFO targets.
2. Replay held velocities0..5.
3. Advance/evaluate/immediately dispatch LFOs D1,D2,D3,SN,CY,HH, one each.
4. Recalculate all six filter frequencies.
5. Run six async voices (envelopes and oscillator increments).
6. Render D1,D2,D3,SN,CY,HH sync blocks. Each voice's original filter remains
   inside that voice's rendering path; snare filters noise before transient.
7. Decimate, pan and route/add each voice to mixer output, then the next block.

An accepted trigger runs `lfo_retrigger` first, then velocity update, then its
oscillator phase/base-note/EG/transient/filter trigger operations. Sources:
Drum_trigger119-168, Snare_trigger94-122, Cymbal_trigger100-131,
HiHat_trigger99-130. `MidiVoiceControl.c:51-77` dispatches tracks0..6 into those
functions. MIDI filtering/mutes/note overrides precede dispatch
(`MidiParser.c:1129-1168`); these broader compatibility policies were not ported
or re-certified by this task.

For generated native block Bk=[32k,32k+31], a processed change before its boundary
can affect frame32k. After the block is generated, the first possible affected
frame is32(k+1). Immediately before a trigger, offset/retrigger controls can
affect that trigger; the LFO output is still calculated at the next block, after
adding an increment to the reset phase. During sustain, LFOs continue advancing
even if no voice is sounding or destination/depth is off. Between repeated
triggers, velocity can repeatedly mutate the current field; only the newest held
velocity replays at the next block. No trigger itself advances the LFO.

`lfo-tests-002/O0/timing.csv` verifies first changed native frame0 for a pre-B0
volume-depth edit and frame32 for the same edit after B0. These are actual
original-mixer render fixtures. They are **not a universal audible index**:
zeros/interpolation/filtering can delay a particular signal difference (M2B
pitch fixtures legitimately report1/33). Nor are they host output indices.
The current wrapper processes MIDI before audio and does not carry per-event
sample offsets into the native event object; the adapter consumes already
generated32-frame caches before refilling (`dsp/LXRAudioAdapter.cpp:37-72`).
FIR latency/history and UART/USB/DAC receipt latency remain separate.

## Retrigger and synchronization

`lfo_retrigger:243-269` checks **all six LFOs** for equality to `voice+1` and
assigns matching phases from each effective phaseOffset. Selector0 is off for
normal voice arguments0..5. Any number of LFOs may restart from one trigger.
Closed/open hats both call lfo_retrigger(5): selector6 restarts either, selector7
does not distinguish open hat. The helper's isolated argument6 can match7,
but the inspected voice dispatch never supplies that argument. No standalone
reset-on-note-off or reset-on-LFO-parameter-change path exists in the traced code.

Sync is a rate choice, not an LFO phase-lock loop. `lfo_calcPhaseInc:136-205`
uses the STM32 `seq_getBpm()` uint16 result (converted to float); bpm>0 maps to
bpm/60.f/4, otherwise0. Sync1..11 multiply bar rate by
0.25,0.5,1,2,3,4,6,8,12,16,32. Sync0 and invalid selectors fall back to the
stored free frequency. `lfo_setSync:216-220` stores the byte and recalculates;
`lfo_recalcSync:222-241` recalculates all six without phase reset.

`Sequencer/sequencer.c:286-301` shows seq_setBpm storing uint16 tempo and calling
lfo_recalcSync. `clockSync.c:47-89` derives a uint16 BPM from averaged hardware
systick MIDI pulse intervals, and calls seq_setBpm. Its clock transport code and
start/stop arming (`:91-104`) do not directly reset LFO phase. Front-panel BPM
arrives through `MIDI/frontPanelParser.c:555-563`; the AVR does not supply the
DSP LFO advance cadence. Actual external clock jitter and transport-to-trigger
timing require sequencer/hardware evidence and are not exercised here.

Native primitive sync can be verified with an explicit integer-BPM service.
P16 must eventually own BPM acquisition/publication and ordered tempo-change
events; a host tempo substitution is not approved. The harness supplies BPM0,
1,120,300 as deterministic service inputs and executes the unchanged rate/sync
functions. It does not run the sequencer or claim hardware transport equality.
Unbounded high BPM/sync products can exceed uint32 conversion range; no invented
hardware result or replacement clamp is asserted.

## Destination selection and all36 LFO-dependent entries

`destination_dispatch_matrix.csv` records all205 rows for each of six owners
(1230 entries), actual PAR, physical identity, pointer/type, scope, source lines,
normal menu eligibility, timing, rejection and proposed coverage. It preserves
indices above127. Original AVR Cc2Text has205 ordered rows; off is0, global
decimation is1, and203 voice entries span2..204. Normal LFO menus can target any
of six destination voices; velocity menus restrict their own voice. Source
owner and destination voice are independent identities in original LFO routing.

`ref/LXR-0.37/front/LxrAvr/Menu/menu.c:1569-1618` stores a full global row index,
uses VOICE_LFO as a destination-menu hint and translates modTargets[row].param
to an actual PAR. It sends source owner plus PAR high bit/low7. STM32
`frontPanelParser.c:428-451` selects the source LFO and calls
modNode_setDestination with that PAR. The DSP node stores uint16 destination;
there is no DSP VOICE_LFO field. IDs161..172 have no ParameterArray pointers and
their direct STM32 MIDI handlers are no-ops. The separately documented AVR NRPN
relay remains UNVERIFIED. The plugin's seven-bit selector slots remain inert
opaque stored bytes; none are decoded into these routing identities.

`modNode_setDestination:138-179` restores all captured fields, restores the old
node target again, updates destination and captures the new field (SpecialF
captures1). It can run during a sustained voice. Its pointer/null check comes
**after indexing** parameterArray; it does not validate destination bounds.
paramArray_setParameter50-80 has a bounds/null guard for restoration, but that
does not make arbitrary node addresses safe. Future APIs must validate before
calling/resolving. Missing/special tags are not silently mapped to float.

The O0 harness independently compiles the original AVR table and checks all205
PAR identities,204 non-null pointers and their actual tags. It executes update
and restoration for198 non-off rows at safe deterministic scalar inputs.
Six VEL_DEST low-byte address-mutation targets are deliberately excluded from
mutation/replay. The test covers the original generic storage operation, not
every row's complete audible lifecycle or arbitrary endpoint safety.

| LFO-dependent rows (6 each) | PAR IDs | Original indices D1/D2/D3/SN/CY/HH | Actual consumption |
|---|---|---|---|
| frequency |115..120|25/59/93/126/160/194|SpecialF multiplier read before phase advance; does not change raw frequency |
| sync |179..184|26/60/94/127/161/195|UInt8 storage write only; increment unchanged until setter or tempo recalculation |
| amount |121..126|27/61/95/128/162/196|Float node amount read during its ordered update |
| waveform |155..160|28/62/96/129/163/197|UInt8 read at ordered waveform calculation |
| retrigger |173..178|29/63/97/130/164/198|UInt8 read by trigger's all-LFO scan |
| phase offset |185..190|30/64/98/131/165/199|UInt32 read on matching retrigger; high endpoint arithmetic qualified |

Thus all36 are real original destinations, with different effects. Being
listed is insufficient for blanket plugin activation. Source-control ordering,
trigger consumption and UInt32 endpoints require their own integration gates.
No original target is reordered or compressed to obtain a seven-bit selector.

## Shared fields and safe base ownership

The complete state-transition table and qualification are in
`out/p15-m2d/shared_field_ordering.md`. Original behavior is multiplicative,
current-field based, with per-node narrowing. Same-field native tick result is
LFO_after(velocity_after(B)), not base+velocity+LFO. Triggers between ticks can
temporarily compound that effective result, and the following tick restores
captured base and replays the latest velocity once. Base notifications update
all nodes addressing that PAR before subsequent replay. Coarse/fine share
multiplier storage based at1. Signed slope changes do not recompute invSlope.

Lower-owner LFO -> later-owner LFO frequency modulation takes effect in the
same tick. Reverse order writes after consumption and normally disappears on
the next restore; it is not a generic delayed feedback connection. Sync-byte
modulation does not call the sync setter; a tempo publication can compute from
a temporary sync byte and leave that increment even after byte restoration.
These behaviors are covered by distinct O0 state comparisons.

M2C's independent one-base-per-velocity-route cache is sufficient for its own
restricted velocity subset, but cannot safely own shared LFO/velocity bases.
Add one per-Engine physical converted-base registry, max12 active entries for
12 sources, keyed by DestinationId/type, with a base revision and source mask.
Keep raw authority, converted base and effective working projection separate.
All source routes refer to that shared entry; a new source must never capture an
already effective field, and no stale node snapshot may overwrite a newer base.
Only the processing owner mutates it; reset/restore clears it before raw replay.
No live pointer persists through Engine copy. Exact design, files, coverage
limits and stop conditions are in `out/p15-m2d/m2e_implementation_contract.md`.

That proposed first integration deliberately retains own-voice destination
scope to satisfy the requested no-cross-voice-mutation restriction. Original
cross-voice routing is documented and tested at the generic C storage/order
level, but is not silently added to the plugin scope. The initial candidate
LFO coverage is the existing107 scalar rows plus explicit off;36 source-control
rows and other excluded classes need additional explicit coverage gates.

## Independent harness and measured divergences

New files: `tests/P15M2DLfoFidelityTest.cpp`,
`tests/reference/lfo/original_lfo_adapter.h/.c`,
`tools/verification/run_lfo_fidelity_tests.py` and
`tools/verification/collect_lfo_audit_evidence.py`.
The reference uses unchanged original lfo/modulation/voice C through the
unchanged M2B generator; complete mixer/add/decimate/clear functions are extracted
verbatim, with bodies/lines/hashes saved. GPIO jack availability and trigger IO
are disclosed service stubs. Original RNG calls use the existing deterministic
host service. No DSP equation or original source file is modified.

Predeclared acceptance is exact finite float bit equality and integer equality;
no tolerance is fitted after comparison. Waveforms cover4096 grid phases plus
five edges for every owner and IDs0..8; further tests cover rates0..127, sync
scalers/invalid values, wrapping, held RNG, reset/reinit, global retrigger,
actual hat dispatch, phase offset0..126, destination fields, signed/narrowed
shared values, aliases, base changes, off, full-mixer cadence and first changed
native frame. Each group records assertion count, first divergence, max/RMS.

Reproduction (new output directories required):

```powershell
python tools/verification/run_lfo_fidelity_tests.py --output out/p15-m2d/lfo-tests-002
```

The shown output directory already exists; use a new name for any later
authorized execution. Do not rebuild or relocate denied programs to evade policy.

`lfo-tests-001` is retained:224376 pass/1 fail in each mode. Its initial-default
check accidentally ran after the fixture published tempo, recalculating the
increment. The fixture was corrected to call original lfo_init immediately
before checking its default; the expected value remains1. No original logic or
existing test/assertion was weakened. Final `lfo-tests-002` O0 passes224377/0;
its O2 launch is BLOCKED. Initial O2 waveform/order comparisons completed with
zero differences, but that run's overall fixture assertion failed and is not
reported as a passing final O2 suite.

Current C++ observational comparisons are separate from original-C assertions:

| Finding | O0 measured scope / consequence |
|---|---|
| Rate increment denominator |128/128 differ; current setRate uses Fs instead of Fs/32. It would be32x too slow if simply inserted once per native block |
| Sine |24606/24606 differ; max absolute error0.7070340514, RMS0.4998340789; std::sin versus original indexed table |
| Triangle |24594/24606 differ; max0.5, RMS0.4083728043; current starts at0.5 instead of0 |
| Rectangle |24600/24606 differ; max1; current reversed polarity and float threshold |
| Descending saw |102/24606 differ; max3.4924596548e-9; subtraction order |
| Descending cubic |102/24606 differ; max1.0658141036e-14; subtraction order |
| Saw up / cubic up / invalid wave |same in the sampled zero-increment primitive domain; not full runtime equality |
| Noise |held fixture equal without wrap; current wrap uses low16-bit/65535, original full uint32-to-float/D. Hardware sequence equality not claimed |
| Initialization |current increment0 versus original1; current clears held noise on init |
| Trigger |current per-object trigger compares supplied voice directly; original scans all six for voice+1. Engine currently invokes neither LFO tick nor global retrigger |
| Sync |current raw179..184 only store sync; raw rate setters ignore it, no stored free-Hz field, helper clamps invalid sync to11 and sync0 returns without restoring free rate. Original setters recalculate and invalid selectors fall back |

Production `dsp/Lfo.*` and all runtime files remain unchanged. Primitive
comparisons are not an audio-fidelity claim; no integrated C++ LFO PCM exists
to certify in this milestone. Original firmware Makefile uses Cortex-M4,
fast-math/reciprocal/single-precision flags; this strict host oracle deliberately
records its own flags (`-ffp-contract=off`, gnu11 C, C++17, O0/O2). Last-bit MCU
FMA/fast-math behavior is not inferred from host equality.

## Fresh regression execution

Commands followed `docs/build_and_test.md`: unchanged M2A contract, M2B mapping,
M2C velocity runners and full checkpoint runner, in new directories under this
evidence root. `regression_results.json` retains raw commands, statuses, hashes
and qualification; the checkpoint runner labels loader exceptions FAIL, so this
audit separately classifies logs explicitly containing WinError4551 as BLOCKED.
No assertion failure is relabeled as a policy denial.

| Suite | Fresh result |
|---|---|
| M2A typed contract |O0 BLOCKED; O2 compiled but runner stops after first denied launch |
| M2B original-C mapping |O0/O2 PASS30169 each;52416 exact PCM samples each,0 differences |
| M2C original-C velocity |O0 BLOCKED; O2 PASS61395,383648 exact samples,0 differences |
| Existing LFO / parameter mapping / MIDI |O0/O2 LFO18,ParameterArray33,MIDI5 PASS; ParamMapMisc PASS; ParamMapEnv O0 BLOCKED/O2 PASS |
| Codec / R1 concurrency |O0/O2 PASS29/35 |
| Installed public host suites |state322, runtime1143, controller955, validator47 PASS |
| Additional checkpoint denials |O0 Mixer,TransientGen; O2 Oscillator,FIR functional,FIR quality; plus ParamMapEnv O0 above |
| Current private-layout probe versus installed binary |995 pass/38 fail; INVALID_ABI, raw result retained |
| Same private-layout probe versus preserved matching M2C bundle |1033/0 PASS; bundle SHA4EE573DD68724E6E162113231F04C904A4E2F8D489F75E74EE4DDD27462660D2 |

The private probe requires exact class headers. Current M2C Engine has additional
route state and an expanded velocity-modulator structure before ParameterArray
and authority in LXR. The installed certified binary predates that layout.
Using the preserved matching M2C bundle, without rebuilding/installing it,
passes the unchanged probe. This resolves that result as a harness ABI mismatch,
not a confirmed unrelated production regression. It does not certify all current
DSP changes in the older installed binary. Public ABI checks are independent.

Totals excluding the new LFO harness:44 fresh valid PASS,8 BLOCKED,1 not run
after denial,1 invalid ABI execution retained. No denied executable was retried
or moved/rebuilt as a workaround. Historical M2A/B/C reports remain unchanged
and are not counted as fresh executions.

## Runtime, host and persistence assessment

The proposed registry requires no allocation/lock/new queue in audio. At most12
active fields restore,12 nodes apply,6 LFOs advance and6 filter coefficients
update per block. A trigger compares6 LFO selectors then applies one velocity
node. Estimated additional state is<=720 bytes before final padding, with a
proposed<=1KiB bound; implementation must measure actual storage and allocations.
Immutable metadata maps identities; Engine-owned resolution prevents stale
pointers, field alias corruption and cross-instance references. Ordered base
publication prevents older restoration overwriting newer changes. These are
design constraints, not measured runtime LFO safety results.

`ParameterStateAuthority<228>` stays authoritative. No temporary effective,
held velocity, phase or registry value can enter IParam/controller/LXRS state.
ParameterArray is a processing projection and its setter's converted-base
notification must cover both sources. Existing generation/FIFO/mailbox and
processing-boundary restore semantics are preserved; no new host notification
is produced for modulation. Reset/reload invalidates routes and all stale bases
before new patch projection. FIR conversion remains independent.

IDs0..227, CC/NRPN addresses, defaults, controller normalization, LXRS v1 and
existing patches remain unchanged. Full original indices128..204 stay internal
without narrowing. A complete user modulation system still needs separately
authorized versioned selector/persistence and automation semantics, including
legacy inert-byte import, full original row identity, source/destination hints,
transactional state restoration and lossless export rules. No LXRS v2, new host
parameter, new mapping, preset migration or sequencer storage was introduced.

## Integrity, remaining gates and next task

iPlug2 d54f69050f517e43b941d88c2a170f0a840b9ee4, SDK root
3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96 and all seven SDK module pins were
verified against immutable archives, retaining the approved framework patch.
The default archive cache was absent; archives were downloaded only to this
task's cache, then the existing verifier ran **verify-only**. No dependency source
was acquired/replaced.395 original firmware files also match the pinned archive.
Before/after verification and all file hashes are in integrity_report.md and
file_inventory.json. The index and all existing production/M2A/M2B/M2C source
and evidence remain byte-identical. No phase-log append was made because strict
acceptance remains blocked. Exact new source/docs paths are listed there.

Unresolved hardware cases: RNG entropy, MCU fast-math/FMA and unsafe unsigned
cast endpoints, actual UART/USB/DMA receipt latency, external clock jitter and
sequencer transport scheduling, low-byte destination-address mutation and AVR
physical NRPN relay. No hardware-dependent ordering guess is used to establish
the finite scalar shared-field contract.

The source-grounded design is ready for review; **strict P15-M2D is not PASS and
P15-M2E acceptance readiness is conditional**. Next task: execute the blocked
mandatory configurations on a policy-approved environment, retaining exact
identities/results and the matching-header rule for private probes, then reassess
M2D. After that, separately authorize the bounded M2E contract. Stop at M2D here.
