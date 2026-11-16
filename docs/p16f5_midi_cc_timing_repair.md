# P16-F5 timestamped MIDI CC repair

Status: **FAIL; NOT CERTIFIED; NOT PUBLISHED**.

The starting74-path inventory,442 source hashes and fingerprint
`8B41C081BA0A2B982AC53B17477B2921C1DB162FF2BF94276028EC23AC885678`
match P16-F4 exactly. Branch/local/live remote remain
`p12b-4c-fir-resampler` / `7749ef1a0203f5ab92bf0e2f447b53bb95dadf68`;
index empty. All starting source bytes are archived under
`out/p16f5/baseline-source/`. Firmware/iPlug2/SDK pins and protected installed
DLL89F6DF...E7FF82 with seven resources match. Earlier evidence remains untouched.

Before production edits, the unchanged final-source exact-DLL executable reran
normally against F4 private179E6643...FD0FC8:1652 checks PASS,8 FAIL. All76
note-only PCM artifacts match the F4 byte hashes, protecting the repaired note
offset behavior. The actual failed fixture and eight identifiers are recorded in
`out/p16f5/midi_cc_failure_reproducer.json`.

The first loss is pinned ProcessParameterChanges requesting only the final
IParamValueQueue point. LXR's timed queue never receives the earlier same-CC value.
The pre-edit ordering/capacity contract is in
`out/p16f5/midi_event_order_contract.md` and `queue_capacity_analysis.md`.
Implemented narrow correction: bounded non-owning single-point CC views in the LXR
shell, retaining the existing note array/sort/sample dispatch, native timing,
host parameter behavior, parser, authority and synthesis/FIR equations.

The local adapter exposes each ordinary CC point to the pinned dispatcher,
which still reads one point from each presented queue. Native host parameters
keep their existing final-point/callback behavior. Equal-time queue/point arrival
order is retained; downstream timestamp/sequence sorting is unchanged. No shared
iPlug2 or DSP equation was edited. The advertised CC channel remains channel0;
seven note channels and the existing instance-wide NRPN selector are preserved.

Input counts are not assumed small. A fixed376-record source table admits
4096 CC points; the event-list view admits4096 records; the shared existing MIDI
collector admits4096 messages in pinned CC-before-note ingestion order. Excess
records reject deterministically with counters. There is no latest-value
coalescing, dynamic queue growth, audio-thread lock or file operation.

Private build001 is
`1B2869DCD2C7982E32A9EDE83DECB46E90B2D46CD4B18DCC120EB172C43004AB`
(1,253,376bytes), VS18/MSBuild18.10.1/Release x64/v145, pre/post deployment
disabled. All later production input hashes still match this build. Its
whole-source build fingerprint is
`32A22B0BE893D003AB02FABB2D36A90C852F3D93DA5B39987982BC932F1A32B7`;
later test/docs changes have a separate final working fingerprint in the report.

The original eight CC failures execute and pass:1660/0. Expanded exact-DLL
coverage passes548168/0, including three distinct values, repeated values,
NRPN1/CC88, notes, automation/readback, locks, velocity/LFO, overflow/reset and
independent instances.135 renders cover15 rate/mode combinations at32/48/96kHz
and nine callback profiles, with exact within-candidate partition PCM,15 matched
repeats and15 observable CC/control differences. The original harness declared
max256 but sent512; it now declares512. The six failing original comparisons at
valid sizes128/256/alternating independently establish the production CC defect.
No assertion was relaxed to correct that setup contract.

The exact-DLL resource probe passes8252/0 on a1MiB thread: zero measured
processing allocations across4000 varied callbacks,4096 notes,4097 CC points plus
a note, and overflow recovery. Peak processing watermark is19616bytes; full CC
scope15096bytes. Initialization/import allocations and non-processing state
stack are separately recorded. These are measured scopes, not a universal proof.
Validator47/47 and PDC26 at48kHz pass. Current source snare/percussion and all
executed functional source equivalents pass; raw policy denials remain separate.
The standalone LLVM lock test originally used a1MiB linker default and overflowed;
its original GCC test reserves2MiB. A normal same-source rebuild with the matching
reserve passes246/0 atOd/O2. The plugin processing probe remains1MiB.

REAPER7.80 loads this same module in every final profile. Both profiles pass40
host checks, rich restore44, all245 controls/twelve routes, and matched reopen PCM.
All six real MIDI CC/NRPN/note WAV comparisons are identical at127/512 callbacks.
Live play/stop/resume, seeks,60/120/180BPM and seven wraps are observed; a dedicated
sample-exact tempo probe is denied under both approved compilers. Production
Import remains BLOCKED: correct process/project/module verified, but Computer Use
state capture twice reports app approval timed out; zero clicks/typing occurred.
Parser rejection62/0 is not credited as Import-button acceptance.

Fourteen rich, independently banked instances produce a valid192000-frame first
WAV. Reopen/rerender never reaches its completion marker. The host remains
responsive; no disk/memory exhaustion or crash cause is established. Fourteen
selector values and released masks are observed. The empty-default-bank count
formula is inapplicable to these short, chained/shuffled banks; no event-count
PASS is claimed from that formula. Earlier F4 raw stress and selector inequalities
remain preserved and unreconciled.

The user reports "all is good" for the seven final-DLL listening cases. This is
recorded as no reported auditory issues; player/output were not supplied. It
does not override the following objective failure.

The final frozen-audio audit fails: **47 of76 note-only PCM files differ from F4**.
The same corrected host executable, normally loading the old F4 DLL, reproduces
all76 frozen files exactly. Loading the F5 DLL changes47. Thus the max-block
harness correction does not explain the discrepancy. The original single-voice
disabled fixture remains identical; Host Sync and the simultaneous multi-owner
disabled fixture differ. In the Sync microscope the first differing native
ST1 sample is frame0: F4=-47,F5=-14460, maximum integer difference14413. The
first host difference is also frame0. This precedes FIR conversion and the first
external MIDI event. Both DLLs still pass their own callback-partition checks.

The earlier internal input/state field responsible for this native difference
has not been established. Identical resource and DSP source hashes do not prove
audio preservation. No uninitialized-state, synthesis or resampler cause is
asserted. Production changes and active certification stopped under P16-F5
conditions6/12/13; no speculative Engine/FIR repair was attempted.

Evidence: `out/p16f5/midi_note_cross_binary_failure.json`,
`same-harness-old-dll-001/`, `cc-valid-host-002-trace/`,
`midi_note_first_divergence.md`, `regression_matrix.csv` and
`verification_report.md` (all36 requested report items and complete file lists).
All141348 earlier evidence files rehash unchanged; protected DLL/seven resources
are unchanged. Index empty, local/live HEAD7749ef1 unchanged. No staging, commit,
push or successor features. Conditional publication is prohibited at this FAIL
checkpoint.
