# P20-C retained events, historical PCM provenance and release-gate closure

Date: 2026-11-26
Starting and final source HEAD: `0155e153ae34ff2fb5740ea7bfe6855ef5d1c220`
Engineering classification: **BLOCKED**
Full release: **NOT_CERTIFIED**

## Baseline and exact artifact

The seven P20-B paths reconciled with their preserved source/build evidence and were accepted as `ACCEPTED_INTENTIONAL_P20B_WORKTREE`; the index was empty and no unrelated starting modification existed. The repaired Import control remains present. The unchanged private candidate is SHA-256 `FB1045BD5716282607487F6A70A6082206309EE741C8F751768AE5C8F6F7650D`, with build-time source fingerprint `80DB83C0C8F79020B06580A54E5627537CE35C5B3E1FBE57DEE33413C0113400`. Every current production input matches its P20-B build manifest.

The protected installed module remains `89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`; all seven installed resources match `data/`. No candidate was rebuilt or installed. The executable now at the historical FIR path passes, but the image previously denied by Windows Application Control has no preserved SHA-256/build manifest/PE identity. Its classification is `HISTORICAL_IDENTITY_UNPROVABLE`, not an old-image PASS.

## Import and focused exact-host qualification

The exact candidate passes 833/0 focused assertions. The production `.lxqb` caller imports a valid bank, rejects eight malformed variants atomically, preserves a peer instance, restores through LXRS state and drives subsequent playback. Classification is `PRODUCTION_PASS / GRAPHICAL_BLOCKED`.

The supported desktop-control preflight timed out twice without exposing state. No input was sent. Actual REAPER graphical Import and REAPER event observation remain blocked; controlled-host traces are not relabelled as REAPER evidence.

## Retained events and callback partitions

The existing observer is test-host-only. It copies `TriggerScheduler::lastBatch()` and `lastAutomationBatch()` after a callback returns; it is not a production queue. This matters when one host callback causes multiple native refills.

The exact candidate was exercised for callback sizes 1, 16, 31, 32, 33, 64, 127, 128, 256, 512 and 1024 over identical 96,000-frame Euclidean and copied-track timelines. All 22 cases have identical PCM, state, RNG and emitted totals, with no scheduler or capture-capacity overflow. Exact post-callback trace retention is complete through 33 frames. At 64 frames and above, earlier native batches are overwritten before the host consumer can copy them; the evidence reports exact missing counts instead of silently dropping them. At 512/1024 frames the tested event batches are entirely unobservable at callback return.

A source-level, per-native-block consumer independently exercised the same eleven partitions for 384,000 host frames (eight seconds/four pattern cycles). Every case retained 448 triggers and 896 locks, zero dropped records, exact ordering/generation/native-boundary identity, the same PCM hash and RNG state, and zero measured processing allocations. Fixed capacity is 1,024 triggers plus 2,048 locks; peak combined occupancy was 1,344. Forced overflow retained 2/5 records and reported 3 drops.

The independent rational/native-block oracle uses the fixed `5632353/128` rate, 32-frame producer blocks, 96 PPQ clock/32 PPQ advance and causal resampler demand. Native tolerance was fixed at zero before the final run. Transport cases cover stop/start/restart, stopped and playing tempo changes, seeks in both directions, loop wrap, host/internal transition, pending pattern activation and near-boundary reanchor. O0 and O2 each pass 97/0.

No DSP or scheduler source changed. The missing exact-DLL batches establish a limitation of the retained observation surface, so controlled-host timing remains `PARTIAL`.

## Euclidean, copy and clear

The original-C/P16-E generator suite freshly passes 9,869/0 at O0 and O2. It exhaustively covers accepted length/pulse/rotation inputs, target gates, preservation of physical step records and unrelated metadata. The exact one-frame host trace retains 16 triggers/32 locks and the consumed generation.

Copy passes for stopped and running transport, the correct destination track 1, multiple instances and all callback partitions for PCM/state/RNG. The explicit destination-track assertion prevents recurrence of the P20-B probe error. Exact trace retention is partial at callbacks of 64 frames or more, and copy was not directly injected at a pattern-transition boundary. Track and pattern clear confirmation, generation publication/consumption, later trigger suppression, unrelated state preservation and LXRS reopen pass. Classification is `CONTROLLED_HOST_PARTIAL / REAPER_BLOCKED`.

The original P20-B wrong-destination diagnostic remains preserved; it was not rewritten or erased.

## Historical PCM provenance

The frozen WAVs, fixtures, fingerprints and original unequal samples were re-read; no historical reference changed.

| Case | Disposition | Reason |
|---|---|---|
| F7 selector 0 | `PLAUSIBLE_BUT_UNPROVEN` | Callback receipt changes reproduce the relevant timing mechanism, but the complete historical callback/native phase and transient state were never recorded. |
| F4 selector 0 | `PLAUSIBLE_BUT_UNPROVEN` | Same mechanism and missing witness; preserved repairs do not uniquely establish the historical cause. |
| F4 selector 12 | `PLAUSIBLE_BUT_UNPROVEN` | Controlled receipt schedules are deterministic, but the historical schedule is absent and the mismatch pattern is not uniquely causal. |
| F4 OFF stress | `INSUFFICIENT_HISTORICAL_EVIDENCE` | The inequality starts at host frame 0, before external MIDI. Historical filter/envelope/voice/FIR/resampler startup state, native phase and executable identity were not captured. |

No root cause is confirmed. The original raw comparisons remain failed/open. The prospective protocol in `out/p20c/prospective_pcm_protocol.md` records exact binary/reference identity, complete startup and receipt state, callback/transport/MIDI timelines, sample conversion, first difference and full mismatch summary. A future prospective PASS remains separate from historical equality.

## Regression and publication

The fresh checkpoint contains 64/64 passing commands: native O0/O2 synthesis, original-C filter (80,000 exact blocks each), snare, FIR/latency, MIDI/controller/state/concurrency, P15, P16 transport/state, P17 kits/presets, P18 morphing, P19 GUI suites, exact candidate host tests and Steinberg validator 47/47. Host IDs 0–245, LXRS v1/v2/v3 sizes, the 65,536-byte ceiling and 26-sample PDC remain covered. P16-E additionally passes 9,869/0 at both optimizations.

Development engineering is `BLOCKED`: exact-DLL retained capture is incomplete for longer callbacks, copy-at-pattern-transition is not directly observed, actual REAPER evidence is unavailable, and historical release criteria remain open. Development publication is therefore `BLOCKED`; no staging, commit or push was attempted. P16-F final release certification remains blocked.

Machine-readable evidence is under `out/p20c/`; `out/p20c/verification_report.md` is the concise checkpoint report.
