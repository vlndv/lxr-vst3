# P15-M2B firmware-faithful base mapping and synthesis timing corrections

Date: 2026-11-02. **PASS** for the four authorized corrections. Original source:
SonicPotions/LXR v0.37, `dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`.
Branch and starting/ending HEAD: `p12b-4c-fir-resampler`,
`0091651720cacbcb2ecd3172bd667581ffb83415`. No Catalyst source was used.

Only `dsp/ParameterArray.cpp` and `dsp/DrumVoice.cpp` change production behavior.
All P15-M2A contract files are preserved; that contract remains unused by the
renderer. New test fixtures, a portable runner, this report and a phase-log
append accompany the repair. No existing test assertion was modified.

## 1. Evidence established before implementation

`out/p15-m2b/correction_matrix.csv` was recorded before production edits. Its
five rows cover both parts of correction A and categories B/C/D, including
firmware/current functions, IDs, exact differences, minimal changes, tests and
saved-patch consequences. `preimplementation_analysis.md`, `mandatory_inputs.json`
and `integrity_start.json` preserve the analysis, required-input hashes, initial
tracked/untracked/index state and accepted P15-M2A hashes. Earlier audit claims
were checked against current source, rather than accepted as sufficient proof.

Firmware paths below are relative to
`ref/LXR-0.37/mainboard/LxrStm32/src/` unless prefixed otherwise. The extracted
original C functions, source lines, complete bodies and hashes are retained in
`corrected-comparison-003-final/reference/extracted_functions.json` under the
evidence root. The original reference checkout is unchanged and clean.

## 2. Correction A: cymbal and hi-hat operator mappings

`MIDI/MidiParser.c:midiParser_ccHandler`, lines 617-638 and 708-730, establishes:

| Native ID | Original destination | Conversion | Prior port behavior |
|---|---|---|---|
| 29 | CY modOsc coarse tune | high byte, then osc_recalcFreq | linear noise Hz |
| 30 | CY modOsc2 coarse tune | high byte, then osc_recalcFreq | linear noise Hz |
| 31/32 | CY operator gains | raw/127.f | already correct; retained |
| 33 | HH modOsc coarse tune | high byte, then osc_recalcFreq | D1 operator noise Hz |
| 34 | HH modOsc2 coarse tune | high byte, then osc_recalcFreq | D2 operator noise Hz |
| 35 | HH operator 1 gain | raw/127.f | D3 operator noise Hz |
| 36 | HH operator 2 gain | raw/127.f | HH operator 1 noise Hz |

The minimal correction writes the four correct operator coarse bytes, preserving
their fine bytes, and immediately calls the existing `osc_recalcFreq`. IDs35/36
now write the two HH gains. No operator reassignment or synthesis redesign was
made. In particular ID27 remains the original snare noise rate, raw/127.f*22000.

`DSPAudio/Oscillator.c:697-725` calculates detune from the low byte, forms signed
`int16_t note = coarse + baseNote - 63`, clamps it to 0..127, and multiplies the
original note-table frequency by detune. MIDI fields remain uint16 and incoming
knob bytes remain seven-bit. The source-note, fine byte, phase and phase increment
are not reset by the mapping. Existing async `osc_setFreq` still updates the
increment/table selection using frequency, pitchMod and modNodeValue.

`CymbalVoice.c:133-203` and `HiHat.c:132-215` preserve two operator blocks in
original order, separate gain narrowing, saturating addition, carrier phase
modulation, filter, transient, envelope/velocity/volume and distortion. The
carrier gains remain 1.0 for CY and 0.5 for HH. Those algorithms were not edited.
CY trigger resets operator phases; HH retains its original different lifecycle.
The comparison covers both trigger and retrigger behavior without changing it.

## 3. Correction B: cutoff scaling scope and operation order

Original expressions:

```c
const float f = msg.data2/127.f;
SVF_directSetFilterValue(filter, valueShaperF2F(f,FILTER_SHAPER));
/* ResonantFilter.c:121-126 */
filter->f = val*(0.5f*0.90f);
filter->g = fastTan(M_PI * filter->f);
```

The original six dispatch paths are `MidiParser.c:230-237`, `558-565`, `640-645`
and `678-683`, for IDs37..42. All six use the post-shaper factor. SN filtering
still applies to its noise oscillator before transient addition; other voices retain their
existing filter position. Disabled USE_PEAK code is not the active 0.37 path.

`cutoffShape()` supplies only the shaped 0..1 result. The port already has an
exact `ResonantFilter::directSetFilterValue()` implementing the factor and
coefficient conversion. The old ParameterArray helper bypassed it. The corrected
helper calls it once, avoiding both duplicated equations and double scaling.
`ParamMapMisc`, direct-filter algorithms and initialization remain unchanged.

There is no new clamp, reset or filter-type/resonance exception in the setter.
The original shaper float order, `(0.5f*0.90f)` product, M_PI-equivalent double
promotion and float fastTan argument are retained. At raw127 the mapped cutoff
is approximately 0.45 cycles/sample; the existing `f>=0.4499f` damping rule still
applies. Resonance's existing q floor and all eight filter types remain intact.
Parameter writes retain filter history; original drum triggers reset filter
history, while SN/CY/HH retain their existing trigger behavior.

## 4. Correction C: live pitch update lifecycle

Original coarse/fine callbacks for IDs14..19 are at `MidiParser.c:272-289`,
`318-346` and `698-706`. Each writes the MIDI-frequency byte, then calls
`osc_recalcFreq`. `Snare_calcAsync`, `Cymbal_calcAsync` and `HiHat_calcAsync`
subsequently calculate their envelope/pitch multipliers and call `osc_setFreq`.
The parameter write changes Hz; the next native async changes the increment.

The previous port omitted recalculation and could keep stale Hz indefinitely
until a new trigger. Six existing setters now call the existing recalculation
function with Engine's preallocated note table. Neither phase nor phaseInc is
changed by that callback. No callback is moved across threads. Native rate,
32-frame blocks, source-note clamping, pitch/snap envelopes, FIR cache, host-rate
conversion and existing host MIDI ordering remain unchanged. D1-D3's already
connected next-tick pitch path was not expanded or refactored.

Independent edited/unedited original-C and C++ snapshots demonstrate zero-based
first affected PCM sample **1** for a write before async and **33** for a write
after async. In the latter case the already-computed 32-frame block is preserved;
the next block consumes the edit. All six coarse/fine IDs agree exactly. These
are matched native test boundaries, not a new host sample-offset scheduling claim.
See both optimization modes' `pitch_timing.csv`.

## 5. Correction D: exact drum instant-attack exception

`DSPAudio/DrumVoice.c:192-204`, in the active AMP_EG_SYNC==0 path:

```c
if (attack == 1 && (state == EG_A || state == EG_REPEAT)) {
    ampFilterInput = slopeEg2_calc(eg);
    lastGain = ampFilterInput;
} else {
    lastGain = ampFilterInput;
    ampFilterInput = slopeEg2_calc(eg);
}
```

The condition tests state **before** advancing the envelope. Raw attack0 maps
to increment1. The exception suppresses the otherwise spurious interpolation
rise, including the tick that transitions from ATTACK into DECAY. Raw1 and
raw127 do not meet it; raw127 still maps to zero increment. The port now uses
this exact branch in `DrumVoice::calcAsync`, calculating the envelope once.

The affected parameters are49/51/53 and voices D1-D3. SN/CY/HH and all envelope
algorithms/trigger functions remain unchanged. Repeated triggers retain the
original value lifecycle; no generic reset-to-one rule was added. The REPEAT
branch is preserved without adding a new drum repeat parameter. Existing gain
interpolation for DECAY/STOPPED and other attacks remains unchanged. Velocity
and channel volume still act in their original stages; internal zero-velocity
triggers remain silent with amplitude velocity enabled. Startup raw defaults
remain identical. The sharper first block is the original envelope behavior,
not an arbitrary amplitude increase.

## 6. Independent original-C comparison

`tests/reference/base_mapping/generate_reference.py` builds on the established
snare hardware shim and original-C extraction approach. It includes original
DSP sources unchanged, extracts complete original CC/NRPN handlers and original
structure declarations, and records provenance. No C synthesis expression is
rewritten. The adapter supplies hardware services, deterministic RNG, observation
and matched initial state. Details and reproduction commands are in
`tests/reference/base_mapping/README.md`.

Both sides use the same original exported tables, trigger sequence, parameter
bytes, RNG seed, sample counts and 32-frame boundaries. The fixture explicitly
matches zeroed firmware-global filter previous-input storage. Both sides use
the original exported note table for this comparison; Engine's existing runtime
pow-generated table policy was not changed by this task.

Final results in `out/p15-m2b/corrected-comparison-003-final/`:

| Configuration | Checks passed | Failed | PCM cases | Samples | Mismatches | Max/RMS error |
|---|---:|---:|---:|---:|---:|---|
| O0 | 30169 | 0 | 1638 | 52416 | 0 | 0 / 0 |
| O2 | 30169 | 0 | 1638 | 52416 | 0 | 0 / 0 |

Float field checks require bit equality; deterministic integer PCM requires
exact equality. Every 0..127 affected operator/cutoff/pitch value is exercised,
with source-note extremes. PCM includes independent operators, combined gains,
all filter types/resonance endpoints, live edits, trigger/retrigger and drum
envelope state/velocity cases. Measured mapped-setter/render allocations:0.
No assertion tolerance was relaxed. The initial baseline comparison has15450
failing checks and31121 sample mismatches in50496 samples at each optimization;
the same common cases pass after repair. Earlier evidence is retained. The first
oracle build's missing MIDI include-path setup error is also retained, then fixed
in the test runner; it was not a production or original-source failure.

The original negative-float-to-unsigned FM cast has ARM-specific saturation
already modeled by the port, but is undefined on host x64 C. Combined FM vectors
use disclosed positive-crest operator initial phases and low source frequencies
over a 32-frame interval, so the actual original assets yield nonnegative
modulation. Operator pitch endpoints are separately tested as direct operator
PCM. Pitch/filter cases mute FM. This qualifies scope without altering original
algorithms or silently tolerating mismatches. No claim is made of exhaustive
arbitrary negative-FM hardware equivalence, ARM FMA/fast-math equivalence or
unknown ROM-guard behavior. No serialized factory-kit fixture was present in
the pinned tree; test patches are disclosed controls, not invented factory kits.

## 7. Fresh regression matrix

All tests below were freshly built/executed, separately from prior certification.
The existing native runner produced42 passing suite executions (21 per mode).

| Suite | O0 | O2 |
|---|---|---|
| DrumVoice | 11/11 | 11/11 |
| SnareCymbalHiHat | 47/47 | 47/47 |
| Oscillator | PASS, fail0 | PASS, fail0 |
| Envelopes | 31/31 | 31/31 |
| ResonantFilter | PASS, fail0/warn0 | PASS, fail0/warn0 |
| ParameterArray / ParamMapEnv / ParamMapMisc | PASS | PASS |
| MIDI parser | 5/5 | 5/5 |
| Mixer | 28/28 | 28/28 |
| Distortion/decimation | 23/23 | 23/23 |
| Transient generator | 17/17 | 17/17 |
| Native runtime synthesis | 419/419, allocations0 | 419/419, allocations0 |
| Original-C snare differential | 61/61 | 61/61 |
| Original-C filter differential | 80000 blocks, zero mismatches | 80000 blocks, zero mismatches |
| FIR functional | PASS | PASS |
| FIR quality | 38/38 | 38/38 |
| Portable FIR latency | 214/214 | 214/214 |
| LXRS serialization | 29/29 | 29/29 |
| P12c-R1 concurrency | 35/35 | 35/35 |
| Existing LFO formula test (no runtime integration) | 18/18 | 18/18 |
| Preserved P15-M2A typed contract | 121781/121781 | 121781/121781 |

Native evidence: `native-regressions-001/results.json` and logs; contract evidence:
`m2a-contract-regression/contract_tests.json`. The portable214 latency assertions
are distinguished from older historical220 snapshot-dependent assertions.
No Windows security-policy bypass or denied mandatory execution occurred.

## 8. Isolated plugin build and host validation

VS Community18 / MSBuild18.10.1 / Release x64 / v145 build succeeded. All binary,
intermediate, PDB and packaging output paths are under
`out/p15-m2b/plugin-build-001/`. Pre/post-build events were disabled, and the
normal deploy/PowerShell packaging script was not invoked. The private bundle
was assembled in new output with seven byte-identical resources.

Private executable: `package/LXR.vst3/Contents/x86_64-win/LXR.vst3` beneath that
directory,730624 bytes, SHA-256:
`55E79A82AC938A41830C0D9563A2EB75CEC7C54E598F1B75B21E10BBB103FA5B`.

Fresh SDK/host tools tested that exact new executable:

| Host suite | Result |
|---|---|
| VST3 state | 322/322 |
| VST3 runtime | 1143/1143 |
| P12c-R2 controller | 955/955 |
| Controller/native/authority/FIFO probe | 1033/1033 |
| Steinberg validator | 47 passed,0 failed |

Evidence: `host-regressions-001/`, including commands, toolchain, input manifests
and before/after plugin hashes. REAPER and subjective listening were **NOT RUN**;
no claim is made that REAPER loaded the new binary. Installation was unnecessary.
The installed certified executable before/after remains SHA-256:
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`.

## 9. Expected audio changes and persistence compatibility

Old saved patches retain exactly the same parameter bytes, IDs, normalization,
LXRS v1 layout/CRC rules and defaults. No migration or patch rewriting occurs.
Audio equivalence with the buggy renderer is deliberately not claimed:

- CY/HH operator controls now set original pitched operators and HH depths;
  unintended drum writes are removed. Their timbres can change substantially.
- Cutoff values37..42 now select the correct lower normalized cutoff. Filtered
  sound and resonance response can change; bypass still passes through.
- SN/CY/HH active tails respond to coarse/fine edits at the next native async
  boundary, without a retrigger or phase reset.
- D1-D3 attack0 has the original sharp onset instead of a 32-sample interpolation
  fade. Other attacks and voice envelope algorithms retain their behavior.

`audio_difference_report.md` and `reference_comparison.json` quantify the disclosed
controlled vectors, not a percentage of overall instrument fidelity.

All host/plugin/MIDI/serializer/authority code and existing regression assertions
are unchanged. CC/NRPN addressing remains the certified plugin contract; the
reference adapter explicitly translates PAR IDs to original hardware addresses
only inside the oracle. P15-M2A seven-bit selectors remain opaque and inert.

## 10. Real-time ownership and bounded work

The existing processing-boundary model remains authoritative. Host/UI/restore
publish to `ParameterStateAuthority`; the processing owner projects raw values
to ParameterArray/Engine. MIDI retains the existing processing-owner/FIFO path.
The changes add only bounded field assignments, existing arithmetic/recalculation
and one branch. Frequency lookups use the preallocated128-entry note table.

There are no added allocations, locks, I/O, logging, queues, callbacks, table
construction or cross-thread engine writes. Each drum tick still evaluates its
amplitude envelope exactly once. Measured setter/render allocation tests and the
existing native runtime/concurrency/controller probes pass. Whole-DLL allocation
instrumentation, TSan and hardware execution are not claimed. Original compiler
warnings about unchanged narrowing/embedded SD pointers remain qualified.

## 11. Integrity and next scope

Before/after receipts, dependency archive verification, exact changed-file hashes
and accepted P15-M2A preservation checks are in `integrity_report.md` and
`file_inventory.json`. Branch/HEAD and index remain unchanged/empty; the phase-log
entry is append-only, preserving all existing P15-M2A text. No staging, commit,
push, cleanup, branch switch or installed bundle replacement occurred. Existing
interface headers need no change because no production API/layout changed.

Remaining gaps are the previously identified velocity/LFO rendering routes,
selector persistence/version semantics and associated lifecycle/arithmetic
qualifications. They are neither repaired nor claimed complete here. P16,
sequencer locks, morphing, preset management, UI and multi-output work remain
outside this milestone.

Recommended P15-M2C: a separately authorized velocity runtime design/integration
using the preserved typed identities, ordered converted-base/effective values,
trigger-held source state and original-C narrowing/alias tests. Before enabling
host destination routing, settle the explicitly versioned selector/persistence
policy from M2A; legacy bytes must not acquire guessed meanings. Do not combine
that work with LFO integration or sequencer/morph implementation.
