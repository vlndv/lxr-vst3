# P20-E release gate decision register

Date: 2026-11-26
Decision-review classification: **PASS**
Development-publication eligibility: **REQUIRES_EXPLICIT_OWNER_APPROVAL**
Full release: **NOT_CERTIFIED**

## Baseline and authority

The eighteen intentional P20-B/C/D paths match the final P20-D diagnostic source manifest and are classified `ACCEPTED_INTENTIONAL_P20BCD_WORKTREE`. The index was empty and no unrelated path existed before P20-E. Local, tracking and live branch HEAD were `0155e153ae34ff2fb5740ea7bfe6855ef5d1c220`.

The unchanged production candidate is `FB1045BD5716282607487F6A70A6082206309EE741C8F751768AE5C8F6F7650D`. The test-only diagnostic candidate is `53A51D63980196DD15647E4584ECE73B80078FC8349646700C88F60EB13C547D`; its capture exists only when `LXR_P20D_DIAGNOSTIC_CAPTURE` is explicitly injected by the diagnostic targets file. The protected installed DLL remains `89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`, with all seven resources unchanged.

The authoritative machine register is `out/p20e/release_gate_register.csv`. It contains 38 mandatory gates: 25 PASS, 6 EXTERNAL_BLOCKED, 5 with insufficient historical evidence/identity, one final-package dependency block and one publication owner-decision gate. Seven rows require an owner decision: development publication, four PCM replacement proposals, historical FIR replacement and the native-accessibility criterion.

## Evidence authorities

Evidence classes remain non-interchangeable unless the original criterion or an explicit owner decision permits substitution:

1. Original firmware source defines firmware behavior.
2. Original-C differential tests compare unchanged reference C in their tested scope.
3. Standalone native tests qualify current source, not a VST3 image or REAPER.
4. Exact-DLL controlled-host tests qualify the identified DLL under the controlled host, not REAPER.
5. Steinberg validator establishes VST3 interface compliance, not graphical or musical acceptance.
6. Actual REAPER production-path observation is required where the gate explicitly names REAPER processing.
7. Actual REAPER graphical interaction is required for visible-control/manual gates.
8. Historical artifact/provenance records establish only what was actually preserved.
9. An owner-approved replacement criterion becomes authoritative only after explicit approval; none is currently approved.

## Gate register summary

| ID | Requirement | Status | Primary evidence / dependency |
|---|---|---|---|
| PF-001 | Original synthesis | PASS | P20-D regression matrix; firmware/original-C tests |
| PF-002 | Modulation | PASS | P15 plus exact-host regressions |
| PF-003 | MIDI | PASS | Timestamped MIDI/controller/state tests |
| PF-004 | Seven-track sequencer | PASS | P16/P20-D native and exact-host tests |
| PF-005 | Host synchronization | PASS | P20-D transport results |
| PF-006 | Pattern persistence | PASS | LXRS save/reopen tests |
| PF-007 | `.lxqb` production Import | PASS | P20-B 833/0 exact-host result |
| PF-008 | Original `.snd` management | PASS | P17 exact-native/host results |
| PF-009 | Host presets | PASS | P17-F state/preset tests |
| PF-010 | Firmware-faithful morphing | PASS | P18 original-C/native/exact-host tests |
| PF-011 | Five-view GUI implementation | PASS | P19-F exact-host qualification |
| EI-001 | Native PCM regression | PASS | Native O0/O2 and 76 frozen references |
| EI-002 | Original-C differential | PASS | Filter/snare/performance differentials |
| EI-003 | FIR quality and latency | PASS | FIR thresholds and 26-sample PDC |
| EI-004 | Host parameter compatibility | PASS | IDs 0-245 and controller tests |
| EI-005 | LXRS compatibility | PASS | v1/v2/v3 and size ceilings |
| EI-006 | Controller synchronization | PASS | ControllerSyncProbe 1057/0 |
| EI-007 | Processing allocations | PASS | Zero measured callback allocations |
| EI-008 | Multi-instance isolation | PASS | P20-D instance-local capture/state/PCM |
| EI-009 | Steinberg validator | PASS | Production and diagnostic 47/47 |
| EI-010 | Production image identity | PASS | Production manifest and SHA-256 |
| EI-011 | Protected installation | PASS | Installed DLL/seven-resource manifest |
| EI-012 | Diagnostic guard isolation | PASS | Macro absent from ordinary project definitions |
| EI-013 | Controlled-host event timing | PASS | 11,001 batches, 448 triggers, 896 locks |
| EI-014 | Pattern-transition copy | PASS | Six boundary/transport cases plus stale/Busy |
| RS-001 | Direct REAPER `.lxqb` Import | EXTERNAL_BLOCKED | Supported GUI access or owner manual run |
| RS-002 | Actual REAPER event timing | EXTERNAL_BLOCKED | Supported REAPER-specific event capture |
| RS-003 | Euclidean/copy-clear REAPER traces | EXTERNAL_BLOCKED | Actual playback interaction/trace |
| RS-004 | F7 selector-0 historical PCM equality | HISTORICAL_EVIDENCE_INSUFFICIENT | Missing exact receipt/native/transient provenance |
| RS-005 | F4 selector-0 historical PCM equality | HISTORICAL_EVIDENCE_INSUFFICIENT | Same missing historical witness |
| RS-006 | F4 selector-12 historical PCM equality | HISTORICAL_EVIDENCE_INSUFFICIENT | Same missing historical witness |
| RS-007 | F4 OFF-stress historical PCM equality | HISTORICAL_EVIDENCE_INSUFFICIENT | Frame-zero startup provenance absent |
| RS-008 | Historical FIR executable identity | HISTORICAL_IDENTITY_UNPROVABLE | Original denied executable hash absent |
| RS-009 | Manual graphical acceptance | EXTERNAL_BLOCKED | Actual REAPER interaction unavailable |
| RS-010 | Physical DPI/dialog placement | EXTERNAL_BLOCKED | Physical multi-scale/dialog observation unavailable |
| RS-011 | Native accessibility | EXTERNAL_BLOCKED | Keyboard passes; native semantic/screen-reader result absent |
| RS-012 | Final release package certification | BLOCKED_BY_DEPENDENCIES | RS-001 through RS-011 remain open |
| GOV-001 | Development-only source publication | OWNER_DECISION_REQUIRED | No current explicit authorization supersedes P20-D |

## Development-publication policy finding

The governing records do distinguish development checkpoints from release certification. `docs/p16f13_development_checkpoint.md` explicitly permitted that checkpoint while release certification remained blocked and established explicit-path staging, full staged-diff/blob review, tested-source binding and normal push rules. P17-P20-A history confirms that development commits can coexist with unresolved graphical or historical release gates.

Those precedents are not blanket authorization. P16-F13 says its task explicitly permits its checkpoint, and later phase entries attach authorization to their own milestones. P20-B/C/D deliberately retained these exact changes; P20-D records development publication BLOCKED and supplies no authority to supersede that decision. P20-E likewise says ambiguity requires owner approval.

Therefore:

- actual REAPER graphical PASS is not a universal prerequisite for every development commit;
- historical PCM closure is not a universal prerequisite for every development commit;
- source-only diagnostic work is publishable in principle when isolated and authorized;
- the Import repair and diagnostic capture meet development engineering standards;
- no engineering defect blocks publication;
- this checkpoint still requires explicit owner approval before staging because no current authorization applies to this exact retained worktree.

Eligibility is `REQUIRES_EXPLICIT_OWNER_APPROVAL`. Approval would authorize development commits only and would not waive release gates.

## Proposed commit allocation

Group A, `fix(lxr): restore pattern bank Import control`, contains the Import tag/control/focus hunks from `LXR.cpp`, the intact P20-B exact-host regression translation unit, P20-B report/tooling, its phase-log hunk and P20-B private-candidate builder hunks. Keeping the P20-B test intact avoids inventing an unqualified intermediate test source even though it also contains controlled-host event checks.

Group B, `test(lxr): qualify retained native event capture`, depends on Group A and contains the diagnostic `LXR.cpp` hunk, adapter/capture hook, P20-C/D tests, P19 helper guard, diagnostic targets/runner, P20-C/D/E documentation and reporting tools, P20-D builder hunks and the P20-C/D/E phase-log hunks.

Shared files require patch-based staging; whole-file staging is not acceptable for `LXR.cpp`, `build_p16f3_candidate.py` or `phase_log.md`. No staging was performed. `out/`, binaries, executables, WAVs, profiles and logs remain excluded.

## Shortest external REAPER closure

Use the preserved isolated REAPER 7.80 profile and first verify the production DLL hash. For Import: observe the real control; import one valid bank; verify pattern readback and playback; reject one malformed bank without state loss; save/reopen; record screenshots or equivalent actual observations.

For event timing, a supported non-blind capture mechanism must record actual REAPER callback index/size, sample/PPQ/tempo/transport, native boundary, pattern/generation and ordered trigger/lock records for start/stop/restart, tempo change, seeks, loop wrap, pending transition and Euclidean/copy/clear during playback. The oracle remains native rate `5632353/128`, 32-frame blocks and zero native-block identity tolerance. Controlled-host traces cannot populate this evidence file.

No changed desktop capability is available, so these gates remain `EXTERNAL_BLOCKED`; the failed attachment campaign is not repeated.

## Historical replacement proposals

The four original inequalities and mismatch counts remain unchanged. Exact historical executable/toolchain, full callback schedule, native phase and transient startup state were not recorded. Receipt schedules make three causes plausible but do not identify the historical cause; OFF stress differs at frame zero before external MIDI.

The proposed prospective protocol pins source commit/fingerprint, executable/DLL hashes, compiler/optimization, complete initial and transient state, callback/control/MIDI/transport timeline, RNG/resampler initialization, native and host PCM, and complete mismatch reporting. It can establish prospective exact-artifact reproducibility. It cannot prove historical equality or historical root cause. Each replacement remains `PROPOSED_UNAPPROVED`.

The historical FIR image previously denied by Windows Application Control has no preserved identity. An equivalent-source rebuild proposal can establish a new prospective artifact, not that the denied image passed. It also remains unapproved.

## Independent decisions

- Continue engineering: **GO**; no current implementation defect blocks bounded work.
- Development publication: **CONDITIONAL GO**, requiring explicit owner approval and the prepared staging review.
- Private internal testing: **GO**, isolated, non-installed and with protected-install verification.
- Packaging preparation: **CONDITIONAL GO** for source/licence/resource/checklist design only; no binary distribution.
- Public release: **NO-GO** while mandatory external, historical, accessibility/DPI and package gates remain open.

P16-F certification remains BLOCKED/open. Decision-review PASS does not change that classification.
