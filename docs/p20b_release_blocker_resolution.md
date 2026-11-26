# P20-B release-blocker investigation

| Workstream | Disposition |
|---|---|
| A. LXQB Import | PRODUCTION_PASS and GRAPHICAL_BLOCKED |
| B. Granular event timing | PARTIAL |
| C. Euclidean and copy/clear | PARTIAL |
| D. Historical PCM | UNRESOLVED, all four original comparisons remain FAIL |
| E. Exact artifacts | PARTIAL. The new private candidate is bound to fresh tests. Historical artifact and actual REAPER evidence remain incomplete. |
| Development qualification | BLOCKED on the outstanding host observation gates |
| Publication | NOT ATTEMPTED |
| Full release and P16-F certification | NOT CERTIFIED |

## Candidate identity

The clean starting branch was `p12b-4c-fir-resampler` at
`0155e153ae34ff2fb5740ea7bfe6855ef5d1c220` locally, on the tracking ref, and
on the live remote. The P19-F/P20-A DLL rehashed to
`F8C8CBC816EAB5F51B79909DECD33808A0D24FD85ECD653B4EBD8091C398FA94`
at 1,437,184 bytes. The P19-F build manifest has 583 entries and fingerprint
`84AAA970E4EA4FBD24F69DF04D2F7CDD878E9467CF367B3678BA34715A10178C`.
At the P20-B start, 582 files still matched those build-time bytes. The
phase log had P20-A documentation changes and the worktree had three additional
P20-A docs/tool paths. The production inputs and seven bundle resources matched.

The P20-B private candidate is
`out/p20b/candidate-001/bundle/LXR.vst3/Contents/x86_64-win/LXR.vst3`,
1,440,256 bytes, SHA-256
`FB1045BD5716282607487F6A70A6082206309EE741C8F751768AE5C8F6F7650D`.
The build-time tracked/source inventory fingerprint is
`80DB83C0C8F79020B06580A54E5627537CE35C5B3E1FBE57DEE33413C0113400`.
The focused test source was corrected and expanded after the build; its
execution receipt carries its separate, later source hash. The one changed
production input, LXR.cpp, was reconciled against the candidate's build manifest.
The build used VS Community 18, MSBuild 18.10.1, v145 and Release x64, with
pre/post build events disabled and the established P19-F fractional DPI source
override. The protected installed SHA-256 is
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`.
Its seven resources rehashed against the protected manifest.

## Import

Commit `06d37c12510998d60ac940ba4d6ae1e2aceaad30` removed the only caller of
`ImportSequencerBank`. The baseline exact-DLL test reproduced a missing
Sequencer Import hit target. The repair restores the existing importer through
a Sequencer button and includes it in the established keyboard focus path.
The parser, state coordinator, and audio adoption path are the existing ones.

The new exact-DLL test verifies the control's visibility in the attached editor
and invokes the private production file method using the matching PDB and PE
symbol. It imports a 50,449-byte version-1 bank, rejects eight malformed inputs
while retaining the entire accepted LXRS stream, checks instance isolation, and
restores the accepted bank in fresh components for timed audible playback.
The retained file-path probe also passed 62 checks against the new DLL,
including Busy rejection with a held authority and byte-identical saved state.
The parser verifies the header, exact EOF, CRC and bank fields before requesting
coordinated publication. The file read takes place on the editor side.

The P20-A computer-control failure remains the only desktop capability evidence.
To close the manual gate, use the P20-A isolated REAPER instructions with the
P20-B bundle and SHA above. Open Sequencer and activate **Import bank**.
Use `out/p20b/focused-003/valid.lxqb` to observe the four gates on track 1,
play the bank, and save/reopen a disposable project. Test Cancel and the
eight `rejected-*.lxqb` fixtures, verifying the status and accepted bank
after each. Capture the actual window and module identity.

## Event retention and edits

The exact-DLL test owns both calls to VST3 process and batch reads on the same
thread. At 48 kHz it submits one sample per call and copies each new native
boundary's completed batches before the next call. It checks the 32-frame
boundary delta and reconciles every observed batch against emitted counters.
The test host holds 256 trigger and 256 lock slots and verifies drop-new behavior
with a forced two-slot overflow. Actual capture drops were zero. A second
instance with identical inputs and no capture matched output PCM, state,
trigger counts and RNG state.

At 60, 90, 120 and 140 BPM the imported fixture yielded four triggers and eight
locks per tempo. The oracle selects the first 32-native-frame boundary at or
after each exact rational musical due time using the rate 5,632,353/128 Hz.
It checks stable order, track, physical step, and consumed snapshot generation.
The full REAPER first-event transition grid, tempo maps, and retention across
arbitrary callback lengths remain open. A diagnostic stream at the in-process
native boundary with an owned drain is needed for those live-host measurements.

The production Euclidean and copy controls were exercised in the exact-DLL
editor. A full bar then emitted 16 triggers/32 locks for Euclidean generation
and 32 triggers/64 locks for the copied tracks. Clearing the pattern produced
zero triggers in the tested reopened render. During a separate continuous
render, track clear published generation 2 and the next main boundary emitted
only the copied track. Pattern clear published generation 3, which the
processing path adopted before the next silent boundary. A first click on
each clear action left the generation unchanged. Actual REAPER traces remain
open, including the broader edit-during-transport-change scenarios.

## Historical PCM

| Original case | First host frame / channel (zero-based) | Expected → observed | Disposition |
|---|---:|---:|---|
| F7 selector 0 | 11,785 / 0 | 0 → −1 | UNRESOLVED |
| F4 selector 0 | 11,785 / 0 | 0 → −1 | UNRESOLVED |
| F4 selector 12 | 119,988 / 1 | 11,348 → 11,347 | UNRESOLVED |
| F4 OFF stress | 0 / 1 | −1,738,506 → −1,738,508 | UNRESOLVED |

The original WAV, payload, and input fixture hashes were rechecked, and the
first unequal samples, counts, peaks and last unequal frames were remeasured.
F4 and F7 selector-0 PCM match per corresponding callback profile across the
two repair eras. A current replay verifies callback-level receipt as an
explanation for differences between its own 127- and 512-frame roll control
timelines. It leaves the historical host WAV inequality unresolved. The
historical selector-12 pair has matching first audible onset and a later F7
matched pair. The OFF stress pair differs in 32 scalar samples, all in frames
0–24, before the first external MIDI at frame 9,600. Original transient
startup state and callback/native-phase traces are missing for all four cases.
No causal synthesis or resampler defect was established.

## Regression results and remaining artifact gates

The new candidate passed all 20 exact-DLL host rows, including 10,993
assertions and Steinberg validator 47/47. The focused event/Import probe
passed 441 checks, the separate Import Busy probe passed 62, and the
resource probe passed 8,250. It measured zero processing allocations and a
20,432-byte processing stack watermark in 4,000 callbacks. The fresh native
GCC O0/O2 matrix passed 44/44 rows, including both original-C filter and
snare comparisons, FIR and latency tests. The fresh timestamped MIDI
replay passed 1,660 checks and matched all 76 frozen PCM files. The current
roll receipt replay passed 98 checks.

The executable now present at the historical FIR Functional O2 path
ran normally and reported zero failures. Its current SHA-256 is
`7E01CB32B1E68C88297A4DA23E739883028ABBEC8047401F1BFDB30979E4B9C7`.
The preserved reports identify the old path but do not establish that the
previously denied image had this same hash. The historical exact-image gate
therefore remains open. The F12 observer's prior policy denial is retained
under its original identity; the P20-B microscope is a separate executable.

## Evidence and next scope

`out/p20b/` contains the initial classification matrix, remeasured PCM pairs,
artifact lineage, exact test receipts and traces, and regression reports.
P20-C should focus on actual REAPER Import and retained event capture for the
original callback and transition grids, plus provenance and causal reproduction
for the four PCM pairs and outstanding exact artifacts.
