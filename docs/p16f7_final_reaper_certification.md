# P16-F7 final REAPER certification — BLOCKED

Date: 2026-11-16. Original workspace: `C:\Projects\lxr-vst3`. Branch: `p12b-4c-fir-resampler`. Starting and final local/live remote HEAD remain `7749ef1a0203f5ab92bf0e2f447b53bb95dadf68`. No staging, commits, push, reset, stash, branch changes or process termination occurred.

## Frozen source and binary

The starting 450 source hashes and 83 changed paths match the final P16-F6 inventory. Starting source fingerprint: `2BD679C3802762313AD65449BC4D32D38993497509C4C4C098FC0E0D7468D730`. F7 changes documentation only; all candidate production inputs remain byte-identical. The complete final inventory and documentation-adjusted fingerprint are in `out/p16f7/file_inventory.json` and `source_fingerprint.json`.

The verified F6 Release x64/v145 candidate is reused, rather than rebuilt unnecessarily. Its SHA-256 is `4591AB2EBFDD6FE738F4983B9FBBF292C4B4E06DA7E0F7528C1E9426E050BE1D`, size 1,253,376 bytes, at `out/p16f6/private-build-001/bundle/LXR.vst3/Contents/x86_64-win/LXR.vst3`. The build-time whole-source fingerprint was `9D7E6C531CA1A6B1E24B6B25BB9B5E3DBED0D6F4A6261CCE316F7CA0A7122845`; subsequent documentation/test additions explain the different checkpoint fingerprint. The original build manifest/log, toolchain versions, seven resource hashes, deployment-disabled configuration and every production input hash are recorded in `out/p16f7/final_candidate_manifest.json`. Dependency verification confirms the pinned firmware, iPlug2 and VST3 SDK, with existing overlays preserved.

## P16-F6 preservation

`dsp/ResonantFilter.h` initializes only `zi` to `0.f`. Firmware startup zeroes voice storage in `.ccmz`; `SVF_init` does not initialize this field. The F6 poisoned-construction/intervention experiment independently localized the first discrepancy to this state before external MIDI or FIR. Filter coefficients, equations, oscillator math, MIDI scheduling and FIR math are unchanged. Reinitializing an already-used filter still follows original C behavior.

Fresh exact-DLL runs pass all 76 frozen F4 PCM comparisons, the original 1,660 assertions including all eight CC failures, and the 548,168-check expanded CC suite. References were not regenerated. Current filter initialization tests pass 16/0 at O0/O2; original-C differential passes 80,000 bitwise blocks at each optimization. Preserved permitted F6 executables were rerun only after executable hashes and unchanged current inputs were verified. These are new executions, not transferred older results.

Timestamped notes/CC preserve boundary ordering, bounded admission, channels, NRPN, locks and modulation. The supported shared MIDI queue remains 4,096 entries; overflow is diagnosed and deterministically rejected, with recovery/no stale replay tested. Host automation retains its documented callback-level contract. No sample-accurate automation or intra-native-block fidelity is newly claimed.

## Exact-candidate host results

REAPER 7.80 x64 executable SHA-256 is `EE5990119726A3C8DBA139B0C349FB9FEBA80BB73E9F41D1341F2E48B4436203`. Each credited task process loads the frozen private module, verified by loaded module path and hash. `reaper_process_audit.json` preserves PID, CLI/project/profile, executable identity, module identity and resource readings. The raw profile scripts' pending strings are historical per-script scope; the final matrix reconciles subsequent tests separately.

* Callback127 and callback512 profiles each pass40 host checks; MIDI PCM between profiles is identical. PDC remains26 samples at48kHz.
* Rich two-instance restoration passes44 checks, all245 controls, twelve enabled routes each, full banks/metadata/transport/patch/routing/bypass restoration and three identical144,000-frame renders. Canonical state remains51,065 bytes; maximum65,536. Each bank has7,168 physical records. Native/exact-DLL suites exercise LXRSv1/v2, legacy migration and atomic corruption rejection.
* Fourteen distinct nonempty rich banks complete two192,000-frame48kHz renders around save/reopen. All14 state payloads, banks and patch/routes match expected initial/restored values. PCM SHA-256 is `56EDC4DDDFD410CCF6FEAA3299FDCD4C64598B2F4E25ADA12A31826D5D863923` for both. Routes, chaining, shuffle, locks, rolls and MIDI coexistence are active. Deferred host ticks permit teardown between reopen/render; this success does not prove the cause of earlier hangs. Measured peak working set is624,795,648 bytes; later CPU/private-byte snapshots are cumulative, not isolated render-time profiling.
* Fourteen production manual-roll rate selectors produce all expected counts and release masks. Actual host mute/roll interaction remains unexecuted; exact-DLL MIDI/mute216/0 is separate evidence.
* At60/90/120/140BPM,42 stable event batches read from actual REAPER match the expected next32-native-frame boundaries, with quantization lateness below32. This is sampled event evidence. Exact-DLL standalone tempo-boundary27/0 passes. Coarse actual-host playback captures stop/resume, nonzero start, seeks, live tempo changes and seven loop wraps.

## Remaining host gates

Production Import is `BLOCKED — REAPER_GUI_CONTEXT_UNAVAILABLE`. The exact task REAPER project/process/window was selected, but the Computer Use interface returned no accessibility and a screenshot of another application. No click or typing was sent. Graphical interaction stopped immediately. Valid/malformed/oversized Import-button operations and their GUI-imported-bank save/reopen playback remain NOT RUN. Parser-level rejection62/0 and production project restoration are supplementary; neither substitutes for this gate.

Full exact host tempo transition coverage remains BLOCKED. An enhanced observer's first Lua fixture had a quoting error; it was corrected in a new task profile. Neither attempt produced an executed supplemental timeline: each observer retains31PASS/1FAIL with zero accepted events. No plugin cause is established. Exact live-change/seek/loop/resume boundary traces and matched90/140BPM repeated offline PCM remain unavailable. Earlier steady-tempo traces and standalone tests do not fulfill those missing cases.

Granular actual-REAPER witnesses for repeat/track0 transition timing, probability/RNG, length/rotation, Euclidean generation, copy/clear and mute/roll interaction are not supplied by native-only tests. These gaps are explicit in `reaper_performance_results.json`.

The unchanged independent cross-profile analyzer retains a selector0 PCM inequality at byte70,710. Host control automation is callback-level; this raw extra comparison is not relabeled PASS or used to invalidate the proven timestamped MIDI contract. Its comparison/contract reconciliation remains explicit. Selector12 now matches. Earlier OFF-route stress raw inequality remains preserved and was not independently resolved or retested on this final candidate.

## Regression, real-time and listening evidence

All eleven current exact-DLL host suites pass, including state322/0, runtime1,143/0, controller955/0, M2G91/0, M2H689/0, transport105/0, coherent-state3,300/0, certification260/0, MIDI/mute216/0, internal controller1,033/0 and Steinberg validator47/47. Current native/dedicated P12/P15/P16 suites retain each original assertion and compiler/optimization provenance. Permitted VS LLVM/MSVC equivalents recover snare/percussion, velocity/LFO, scheduler, M2H and P16E policy denials. Mixer O2 passes28/0 through ordinary execution of its preserved permitted artifact after hash-verifying the complete reachable Mixer/Decimator/test source closure unchanged; Engine/filter are unused by that test. Current RuntimeSilence O2 remains denied; its O0 PASS does not fulfill O2, and older artifacts using Engine cannot substitute. Raw denied counterparts and the historical restricted exact FIR Functional O2 remain BLOCKED separately. The final matrix preserves every473-row F5 requirement plus current actual executions and outstanding host gates; counts are not unique functional-case totals.

Exact-DLL resource probe passes8,252 assertions. Four thousand callbacks,4,096 events, overflow and recovery measure zero processing allocations through four imported allocator hooks with positive controls. Maximum processing stack watermark is19,616 bytes on a1MiB reserved thread. This matches F5/F6 queue measurements and is larger than historical F3; no regression from the final F6 candidate is observed. Non-real-time import/state stack/allocations are separate. Finite hook/workload coverage is not a universal allocation/stack proof.

The operator replied **“all approved”** to the fifteen-case final-DLL listening checklist. This is recorded as approval of that exact checklist; no detailed auditory observations or player/output were supplied. Historical F3/F5 records remain intact.

## Integrity and publication

The protected installed DLL remains `89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`; all seven resources match. Earlier173,216 evidence files totaling11,211,807,907 bytes rehash unchanged. No reference PCM, prior report or security setting was modified.

Overall and combined P16-F certification are BLOCKED. F5/F6 repair preservation passes within its measured scope; this does not confer full host certification. No current confirmed synthesis/MIDI implementation defect was found. Publication is BLOCKED by the unresolved mandatory host/regression gates. Index remains empty, no commits were created, and push was not attempted. Next musical-feature milestone is not authorized or ready. Detailed34-item report: `out/p16f7/verification_report.md`.


## Renewed F7 request reconciled with F8 evidence - 2026-11-16

The existing F7 report above is preserved. This renewed request resumes the more
advanced F8 checkpoint; it does not rewind source or overwrite historical F7
results. Fresh baseline matches452 source files/85 changed paths, empty index,
local/live HEAD7749ef1 and frozen4591AB candidate. Pinned dependencies and protected
89F6DF module/seven resources verify unchanged. Fresh original MIDI1660/0,
frozen76/76, initialization16/0 and original-C filter80000 bit-exact blocks at
both O0/O2 pass. The unchanged expanded CC executable now receives WinError4551
before entry, confirmed Code Integrity3077 under policy0283ac0f-fff1-49ae-ada1-
8a933130cad6. Earlier548168/0 remains a separate measured PASS; this new execution
is BLOCKED. No image/path/policy bypass or production rebuild occurred.

A fresh isolated import project/PID29672 and exact loaded module were verified.
Its watcher was active before the operator reported successful production Import
and the message "Bank loaded. Enable the sequencer to play." The intended-process
read-only applied-bank fingerprint nevertheless remains084C0955...AEAF008 and
no bank-change/render artifact was captured. The operator cannot currently save
the visible project to the requested evidence path or supply its main title.
Both observations are retained. End-to-end Import is BLOCKED pending reconciliation,
not a manufactured PASS or a confirmed implementation defect. All five production
rejection cases and imported-bank save/reopen/PCM remain NOT RUN.

F8 exact-candidate granular performance/constant-tempo results, fourteen-instance
save/reopen identity, LXRSv3, allocation/stack, validator47/47, PDC26 and15-case
listening acceptance remain valid within their measured scope. The renewed F7
contract additionally requires actual host acceptance of Euclidean-generated and
copy/clear results. Those results remain NOT RUN; the absence of graphical editing
controls does not waive these separately requested result tests. Remaining first-
transition/live-map timing/PCM and raw inequalities remain open.

Current34-item report and reconciled full matrix:out/p16f8/resume-002/. Original
out/p16f7 evidence remains untouched. Overall/publication BLOCKED. No source fix,
staging, commit, push, deployment, process termination or new feature milestone.
