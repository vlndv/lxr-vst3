# P16-F13 development checkpoint and completion plan

This is a **development-only checkpoint** of the combined P16-F implementation.
The critical development gates pass in the measured scope below. **Release
certification remains BLOCKED.** The P16-F13 task explicitly permits this
development checkpoint despite the remaining release acceptance work. Earlier
reports and their publication prohibitions describe their historical tasks;
they are preserved, not rewritten as passes.

Starting branch: `p12b-4c-fir-resampler`. Starting local and live development
branch HEAD: `7749ef1a0203f5ab92bf0e2f447b53bb95dadf68`. Origin is
`https://github.com/vlndv/lxr-vst3.git`. The remote default HEAD is separately
`a73f310fda98fcf8d09c1d4ece0e6f8ff0abde6a`; it is not the development branch.
The initial index was empty. No reset, clean, stash, rebase, release tag, main
merge, installation or security policy change is part of this checkpoint.

## Inventory and integrity audit

All 457 starting source-file hashes match F12 fingerprint
`1822600B81B76885D8839589812916782554CEA6775D922A4665F3A6AAA30688`.
The 90 initial changed paths comprise 10 production DSP, 4 VST3 integration,
3 state persistence, 5 host-control/interface, 21 test, 33 verification helper
and 14 documentation paths. No unrelated path was identified. Per-file hashes,
categories, source dependencies and explicit commit eligibility are recorded in
`out/p16f13/changed_file_review.json`. Generated `out/` evidence remains local.

All 235 production inputs match the preserved F6 private Release x64/v145 DLL:
`4591AB2EBFDD6FE738F4983B9FBBF292C4B4E06DA7E0F7528C1E9426E050BE1D`
(1,253,376 bytes). Its seven resources match. No instrument rebuild is needed.
Cached-archive verification checks firmware `dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`,
iPlug2 `d54f69050f517e43b941d88c2a170f0a840b9ee4`, SDK
`3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96` and pinned SDK submodules.
The original firmware remains read-only and excluded from Git.

The audit covers the complete production delta: direct validity-preserving
VST3 context capture; bounded fixed-point host timeline; immutable bank pins;
coordinated patch/routes/bank/transport restore; bounded LXQB/LXRS codecs;
main-thread-only bank file ingress; transient roll controls; NRPN mute routing;
timestamped note/CC collection, sorting and dispatch; and fresh filter history.
There is no new oscillator, envelope, SVF equation, FIR coefficient, saturation
or synthesis feature change. The `zi = 0.f` initializer reproduces firmware
zero-filled startup storage while preserving repeated-init behavior.

No missing production dependency, confirmed unresolved current-source functional
defect, credential, new audio allocation, blocking synchronization or
test-only production behavior was identified. The internal probe hook is
compile-time guarded and layout-preserving; friend declarations supply no
production control inlet. The file dialog is the only production bank-import
action. The codecs reject oversized/truncated/corrupt input before publication.
Unknown outer optional LXRS modules intentionally reject in this profile.

The project adds `HostTransport.cpp`; all four affected native runners include
it. Header-only codec dependencies are present. Accepted interface facades are
in `interfaces/`. Existing standalone probes adapt only LXR.h's relative DSP
include prefix; the F7 snapshots are otherwise hash-identical to current inputs.
Some historical REAPER/diagnostic helpers require preserved `out/` fixtures,
PDBs or recipes; they are not clean-checkout production dependencies or a
self-contained release runner. The private builder currently restricts output
to its historical F3-F6 trees. These limitations are documented, not silently
worked around. The old phase summary table also lags its appended decisions.

## Development gate evidence

Results are either fresh original-path executions of preserved current-source
executables or explicitly retained, hash-bound finite evidence. They are not
claims of a fresh full source rebuild. F13 raw run records remain separate from
earlier denials and failures.

| Gate | Accepted development evidence |
|---|---|
| Native synthesis | Fresh 44 native O0/O2 executions pass, including original-C filter 80,000 exact blocks each, original-C snare, FIR quality 38/0 each, latency 214/0 each and runtime silence 419/0 each. |
| Filter initialization | Fresh poisoned-storage 16/0 at O0/O2 and 80,000 original-C blocks each preserve the F6 repair. |
| MIDI | Fresh original 1,660/0 and expanded CC/NRPN 548,168/0. Fresh current-DLL PCM matches all 76 untouched F4 frozen files. Timestamp partition invariance and equal-offset CC-before-note ingestion order retain their tested contract. |
| Sequencer | Retained current-source B core 1,007,503/0 each O0/O2; C 16,189/0 each; D 246/0 each; E 9,869/0 each through permitted compiler equivalents. Includes chains, substeps/probability, locks, shuffle, rolls and deterministic native PCM. |
| Persistence | Retained codec 152,002/0 each O0/O2; fresh exact-DLL coherent-state 3,300/0; LXRS v1/v2/v3/legacy, corrupt rejection and cross-authority restoration. Canonical stream 51,065 bytes, maximum 65,536. |
| VST3 | Fresh state/runtime/controller/M2G/M2H/transport/F2/certification/mute/projection tests pass; fresh Steinberg validator 47/47. Exact same private binary. |
| REAPER | Retained F7/F8 exact-candidate pattern, 127/512 profiles, rich restore, constant-tempo paired PCM and fourteen-instance save/reopen evidence; 26-sample PDC at 48 kHz. No new GUI or DAW acceptance is inferred. |
| Real-time | Bounded input source table 376; CC visits 4,096, event visits 4,096, shared accepted MIDI capacity 4,096; deterministic rejection counters and fixed storage. Retained exact-DLL resource 8,252/0: zero measured processing allocations, 19,616-byte observed peak on 1 MiB. |

The C/E test summary's 2/3 allocations count fixture/thread construction; the
asserted processing-loop allocation delta is zero. No total-process allocation
count is misreported as an audio measurement. Fixed loops, one-attempt atomic
gates and bounded snapshots replace waiting; state/bank parsing, CRC and full
bank copies remain off audio. Existing native-parameter CAS publication has
no new P16-F loop; general lock-free liveness is not a wait-free proof.
Stack figures cover tested callbacks and imported allocator interception,
not every possible framework path. TSan and hardware recording equivalence are
not claimed. Multi-instance banks, clocks, queues and transient state are owned
per instance; shared table resources are resident assets.

F13's first MIDI and expanded-CC attempts exited on missing trace directories.
These are preserved harness failures (`midi-001`, `expanded-cc-001`), corrected
by creating new output directories. The unchanged executables then completed
in `midi-002` and `expanded-cc-002`. No production repair was needed. Historical
Application Control denials, including F12's expanded-CC image, remain denied
attempts; F13's ordinary permitted F6 executables are separately identified.

## Historical PCM reconciliation

All four original raw comparisons still **FAIL**, with causes **UNRESOLVED**.
F13 rehashes their original WAV/PCM and remeasures the first differing sample.
Neither frozen references nor raw failure records were changed. Original
revisions are HEAD7749ef1 plus the recorded uncommitted F4/F7 source snapshots;
there is no invented committed reference revision.

| Case | First host frame/channel; values | Repair era and disposition |
|---|---|---|
| F7 selector 0 | 11,785/0; 0 / -1 | Current private4591AB era, after note/CC/zi repairs; no external MIDI. Callback-level host mask control. |
| F4 selector 0 | 11,785/0; 0 / -1 | After note timing, before CC/zi repairs; corresponding F7 PCM is identical, so those repairs do not explain the inequality. |
| F4 selector 12 | 119,988/1; 11,348 / 11,347 | Before CC/zi repairs, after note timing. Current paired selector12 evidence matches; historical cause remains unproved. |
| F4 OFF stress | 0/1; -1,738,506 / -1,738,508 | Before CC/zi repairs, after note timing. Only 32 scalar differences, frames0..24, maximum13; precedes external NoteOn at frame9,600. |

Saved patch/routes/banks are known, but original transient Engine/filter/FIR
initialization, first native sample and complete offline callback/reset histories
were not recorded. Selectors use production IDs243/244: selector0 or12, mask
0 at0s,1 at0.25s,0 at2.5s. Control receipt can vary with callback partition;
the 98/0 receipt replay supports this explanation but does not exactly reproduce
the historical 24-bit files. OFF stress has fourteen NoteOn60/velocity118 events
at0.2s and NoteOff at0.35s, without roll controls. Its first difference cannot
be attributed to those later MIDI messages.

No comparison is retired as an established obsolete-contract case. No current
source defect has been causally established from these inequalities. The
development checkpoint accepts this disclosed uncertainty under the new task
policy; all four remain open release risks. The fresh 76-file frozen comparison
and current matched-condition tests pass; those passes do not erase these FAILs.

## Release blockers

1. Production Import: observe the genuine button/file dialog applying distinct
   BankB, reject five malformed files atomically, then save/reopen and compare
   imported-state PCM. Correct instance identity and project-state injection
   do not certify this action. Earlier operator reports are not independent
   corroboration. Supported GUI accessibility remains unavailable in F12.
2. Stable production-boundary event retention and native qualification, replacing
   latest-block polling. F12 captured6 of44 required events, not a plugin failure.
3. Complete independently checked first events for play, resume, seek, loop,
   tempo change, callback-size change and pattern transition at60/90/120/140BPM,
   plus the four tempo maps and matched-condition PCM.
4. Complete Euclidean-generated and copy/clear actual host event/result traces;
   earlier full state and paired384,000-frame PCM are supporting evidence only.
5. Reconcile all four raw historical inequalities or retain them as explicitly
   unresolved release blockers. No tolerance substitution or reference rewrite.
6. Permitted execution of remaining historical exact FIR/observer qualification
   artifacts where the release contract requires them. Current FIR quality and
   latency pass; the distinct historical exact-artifact gate is not renamed.

No release tag, installed-plugin replacement or release-certified claim is
authorized. Future source or instrument binary changes require targeted
recertification; exact-DLL host evidence is never transferred solely because a
new build has similar source.

## Observer recovery design

Implement one isolated certification-instrumentation change after this source
checkpoint. Capture a callback-entry context/transition record and each actual
accepted trigger/lock dispatch at its production boundary, before `mLastBatch`
is cleared. Include callback sequence, instance/session, transport epoch, host
absolute start and receipt offset, native boundary, event ordinal, stable order,
pattern/track/position, trigger kind and lock operation. Capture actual dispatch,
not an inferred prediction. Record the boundary's context even if it emits none.

Use an instance-owned preallocated SPSC ring with64-byte fixed records and
65,536 entries (4 MiB), allocated during inactive prepare. Audio is the sole
producer; a non-audio consumer owns reads. Producer checks acquire read cursor,
writes one POD record, then release-publishes the write cursor. Consumer acquires
write cursor and release-publishes consumed cursor after copying. Never copy a
slot concurrently with reuse; plain external ReadProcessMemory polling of mutable
objects is not the consumer protocol. Require lock-free64-bit cursor atomics on
the supported target. Bound per-callback work by existing event limits plus one
context record, with no allocation, formatting, locks, file I/O or host calls.

Full ring drops new records and increments a saturating overflow counter; it
never overwrites unread data or changes synthesis/control decisions. Any loss,
sequence gap, identity mismatch, timeout, truncation or epoch mismatch invalidates
the run. Records survive block resets. A drained explicit end marker includes
attempted, published, dropped and high-water counts; capture reset happens only
while inactive after consumer acknowledgment. Never reset cursors under a reader.

Drain/export using a supported non-audio diagnostic interface with explicit
session ownership. Its ABI and activation path must be designed and qualified;
there is currently no such retained-event production interface. Compile-time
instrumentation changes binary identity, so it cannot retroactively certify
unobservable events inside4591AB. Either qualify the instrumentation as a
controlled diagnostic build and keep attribution separate, or qualify a new
production candidate once and repeat its required exact-binary gates.

Qualification: native callback sizes1/32/127/512 plus irregular partitions,
repeated identical sessions, shared-hat same-boundary ordering, lock/roll/mute,
seek/reset/loop epochs, forced saturation, slow/stopped consumers and multiple
instances. Compare complete records with an independent original-C/rational
timing oracle; require44/44 for the known fixture, zero overflow and unchanged
PCM/latency. Run existing seven collector tests but do not substitute them for
native acquisition. A finite capture without context and end markers cannot pass.

Production Import has no supported public command in the current LXR shell:
`ImportSequencerBank` is private and called only from the graphics file-dialog
completion. VST3 `setState` and ReaScript FX state operations bypass the actual
Import UI; internal probe injection and arbitrary private calls are not genuine
Import acceptance. Use supported graphical accessibility or an observed operator
workflow when available. Do not use blind coordinates, memory writes or invented
named-config commands. This task analyzes that barrier without another futile
REAPER launch or production-control change.

## Remaining development roadmap

The following are bounded future tasks, not implemented by F13. Preserve current
IDs/state compatibility and test each new module independently. The immediate
next milestone is observer qualification and release-gate evidence recovery;
then start preset/kit management as a separately scoped development phase.

| Workstream | Required functionality and reusable components | Missing code, dependencies and minimum viable implementation | Critical tests, done condition and release blockers |
|---|---|---|---|
| Presets and kits | Source-reviewed sound-kit load/save, names and defaults; reuse ParameterArray, typed routes, complete-state coordinator and v1/v2/v3 decoders. Pinned AVR `Preset/presetManager.c` is authoritative. | First specify the sound/global boundary and original `.snd` field order/version/target encodings from C. Implement one off-audio validated kit candidate, atomic apply, user-file open/save and explicit name handling. Keep plugin transport/pattern state separate unless original all/performance save is explicitly selected. Minimum: one kit save/load roundtrip with retained routing, no factory browser. Factory kit extraction requires independently verified original assets and licence notices; no guessed startup kit. | Original-C file goldens, truncation/invalid fields, all sound values/routes, rollback/Busy, save/reopen, active voices versus defined reset policy, lock interaction, multiple instances and allocation checks. Done when exact bytes/semantics and production action roundtrip pass. Blockers: original file compatibility, route mapping, ownership/thread policy and GUI access; factory kit claims remain blocked until assets are proven. |
| Morph and remaining firmware | Two immutable sound anchors and raw-value interpolation; reuse route identity contracts and transient overlay machinery. Audit AVR `interpolate`, `preset_morph`, kit save and STM32 dispatch together. SOM source is present; Euclidean and copy/clear code already exist. | Specify AVR promotions/wrap/rounding, exact eligible parameter universe, discrete destinations and precedence with base edits/locks/modulation before coding. Minimum morph: two anchors plus one bounded processing-boundary control, without calling full restore per movement. Implement SOM only after its lifecycle/RNG/node/control contract is settled. Native `.pat` and all/performance files are independent compatibility tasks, not bundled into morph. | Exhaustive scalar interpolation against original C including descending pairs, endpoint quirks and enum transitions; retained route identities; locks/base edits; callback partition PCM and save/reopen of a versioned schema if anchors persist. Done per feature after accepted interface and original-C/host tests. Blockers: unresolved source semantics, no persistence schema for anchors, SOM lifecycle and hardware RNG uncertainty. No speculative effects or SD samples. |
| Custom graphical interface | Accessible synth/track controls, seven-track pattern editing, two automation slots, transport/roll/mute, chain/shuffle/Euclidean/copy/clear and kit/morph surfaces as those modules become available. Reuse stable IDs0..244, controller reconciliation, immutable edits and iPlug2 graphics. | First produce the missing P15 UI layout/control mapping with readable names, keyboard/focus behavior and explicit shared-hat semantics. Minimum: synth controls plus one selected pattern/track editor using inactive bank candidates and validated commands; later add kit/morph panels. No audio thread widget calls or direct shared-bank mutation. | Every control maps to authoritative readback and expected DSP target; automation gestures, MIDI updates without feedback, atomic edits/undo scope, invalid commands, resize/DPI/focus/keyboard and correct-instance Import. Done when control inventory and production host workflows pass. Blockers: unresolved UI spec, accessibility, kit/morph interfaces and trademark/design review. |
| Final testing, packaging and release | Reuse native/original-C tests, frozen PCM, private builder, validator, REAPER projects, state/RT probes and evidence manifests. Maintain custom non-commercial licence and full-source distribution. | Consolidate a clean-checkout fixture/test driver, eliminating historical-out dependencies from required release tests. Build one isolated Release x64/v145 candidate with deployment disabled; qualify that exact candidate, then create an explicit resource/source/licence package and installation/rollback instructions. Minimum: one supported Windows x64 VST3 package with all seven assets and documented tested host/rates. | Original-C and frozen audio, full state compatibility, all observer/Import/timing/performance gates, four historical comparisons, validator, PDC, sustained multi-instance CPU/RT, clean-machine asset discovery, package hashes and source completeness. Done only when all mandatory release rows pass for the packaged binary and protected-install checks pass. Blockers: all six above plus any newly confirmed production defect; no release from development PASS alone. |

Suggested independently testable sequence: observer ABI/capture qualification;
production Import and timing/performance closure; kit file contract and codec;
atomic kit integration; morph scalar/overlay contract and integration; remaining
source-backed firmware modules one at a time; UI layout then incremental panels;
one final candidate/package qualification. Development can use the new checkpoint
policy, but shipping still requires full release certification.

## Commit and publication policy

Keep the combined production change, required interfaces, fixture/test sources,
runner dependencies and historical reports in one coherent development commit:
transport and persistence share scheduler pins/restore state and the MIDI tests
share the v3 host harness. Splitting those arbitrarily would create misleading
intermediate test contracts. Only explicit reviewed paths are staged. Exclude
all `out/`, binaries, WAV/PCM, REAPER projects, backups and local environment data.
Check and inspect the entire staged diff and bind every staged blob back to
the reviewed/tested bytes before committing. Final commit, push and local/live
HEAD verification receipts are in `out/p16f13/verification_report.md`.
