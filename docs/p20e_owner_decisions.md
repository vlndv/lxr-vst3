# P20-E owner decision package

No approval is recorded in this document. Each field requires an explicit owner response.

## Recorded disposition — 2026-11-26

Owner Decision 1 is **APPROVED** for development-only publication on
`p12b-4c-fir-resampler`, limited to these two reviewed commit groups:

1. `fix(lxr): restore pattern bank Import control`
2. `test(lxr): qualify retained native event capture`

The authorization permits exact-manifest reconciliation, final engineering checks,
explicit scoped staging, the two commits, and a normal fast-forward push to
`origin/p12b-4c-fir-resampler`. It excludes release certification, binary
distribution, installation, tags, main-branch changes, force-push, historical
PCM/FIR replacement criteria, accessibility waivers, and any waiver of REAPER
graphical or event acceptance.

Owner Decisions 2, 3 and 4 remain **NOT APPROVED**. No owner signature is inferred
or recorded here; this disposition records the explicit authorization supplied in
the P20-E Owner Decision 1 execution task.

## Decision 1 — Development publication

**Question:** Should the verified P20-B/C/D source be committed and pushed as development-only work while all external and historical release gates remain open?

- Current requirement: current milestone-specific authorization plus reviewed staging, tested-source binding, protected-install verification and fast-forward push.
- Available evidence: P20-D engineering PASS; 64/64 regression commands; focused 646/0 and 833/0; P20-C 97/0 at O0/O2; P16-E 9,869/0 at O0/O2; production/diagnostic validator 47/47; compile guard verified.
- Remaining uncertainty: no existing text explicitly authorizes superseding the preserved P20-D publication block for this exact worktree.
- Risk of approval: development commits may be mistaken for release certification unless their messages/docs retain the open gates.
- Risk of withholding: eighteen verified paths remain uncommitted and harder to review or build on.
- Recommended option: **APPROVE development-only publication** of the two prepared groups, expressly without waiving any release gate.
- Consequence: permits reviewed staging, two normal commits and fast-forward push only; no release tag, binary publication, main merge or certification claim.
- Explicit approval: `[x] APPROVE  [ ] REJECT  [ ] DEFER`  Date: `2026-11-26`

## Decision 2 — Historical PCM replacement criteria

**Question:** Should a prospective exact-artifact reproducibility protocol replace one or more historically unprovable exact-equality gates?

- Current requirement: each original comparison must equal its frozen reference unless an explicitly approved replacement criterion is adopted.
- Available evidence: exact original WAV/fixture hashes, first/last differences and mismatch counts are preserved; three callback-receipt mechanisms are plausible; OFF stress begins at frame zero.
- Remaining uncertainty: historical executable/toolchain, complete callback/control schedule, native phase and transient startup state are missing. No historical root cause is confirmed.
- Risk of approval: future prospective equality cannot establish that the historical render was equal or identify its cause.
- Risk of withholding: the four gates may remain permanently uncloseable despite complete modern reproducibility.
- Recommended option: **APPROVE only as a named replacement per case**, after reviewing the prospective protocol; retain the original failure beside every replacement result.
- Consequence: an approved prospective result may close only the specifically named release gate; it must never be labelled historical equality.
- Cases: `[ ] F7 selector 0  [ ] F4 selector 0  [ ] F4 selector 12  [ ] F4 OFF stress`
- Explicit approval: `[ ] APPROVE SELECTED  [ ] REJECT  [ ] DEFER`  Owner/date: `________________`

## Decision 3 — Historical FIR identity

**Question:** Should an equivalent-source rebuild and qualification replace the missing exact identity of the earlier security-denied FIR executable?

- Current requirement: prove the denied executable's exact identity and result.
- Available evidence: current source/pins and a currently passing executable are known; the earlier path/security denial is preserved.
- Remaining uncertainty: the earlier executable SHA-256, PE identity and build manifest do not exist in preserved evidence.
- Risk of approval: equivalent source does not prove that the denied historical image passed.
- Risk of withholding: exact historical identity is likely permanently unprovable.
- Recommended option: **APPROVE a clearly prospective equivalent-source criterion**, retaining `HISTORICAL_IDENTITY_UNPROVABLE` as the historical disposition.
- Consequence: can qualify a new reproducible FIR artifact but cannot relabel the prior security denial PASS.
- Explicit approval: `[ ] APPROVE  [ ] REJECT  [ ] DEFER`  Owner/date: `________________`

## Decision 4 — External REAPER acceptance

**Question:** Which supported path will supply the remaining real REAPER evidence?

- Current requirement: actual REAPER 7.80 graphical Import/manual interaction and actual host-specific event timing; controlled-host evidence is not a substitute.
- Available evidence: exact production operations and controlled-host timing are complete; preserved isolated profile and manual procedures exist.
- Remaining uncertainty: no supported desktop or REAPER event-capture facility is currently available.
- Risk of approval: none if the selected method records actual observations and exact candidate identity; blind interaction remains prohibited.
- Risk of withholding: full release remains externally blocked.
- Recommended option: **owner-operated manual graphical run plus a supported REAPER-specific event capture**, using the schemas in `out/p20e/`.
- Consequence: successful evidence may close only the observed REAPER/DPI/accessibility gates; historical gates remain separate.
- Selected path: `[ ] OWNER MANUAL  [ ] SUPPORTED COMPUTER CONTROL  [ ] DEFER`
- Explicit approval/assignment: `________________________________________`

## Native accessibility criterion

This is an additional release-policy choice exposed by the register. Automated keyboard acceptance passes, but native semantic custom-control descendants and a screen-reader run are absent. If keyboard-only accessibility is intended to be sufficient for release, record that as an explicit replacement criterion; otherwise schedule the native acceptance run. No approval is currently recorded.
