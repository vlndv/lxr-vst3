# P16-F4 certification recovery

Status: **FAIL; NOT CERTIFIED; NOT PUBLISHED**. Evidence and the complete
32-item report are in `out/p16f4/verification_report.md`. The original F1/F2/F3
reports remain historical evidence, with their original classifications.

The starting branch and local/live remote HEAD were
`p12b-4c-fir-resampler` / `7749ef1a0203f5ab92bf0e2f447b53bb95dadf68`.
The index was empty. All 70 changed paths and all 438 source hashes matched the
F3 inventory. Working-source fingerprint was
`80349EEB26C937FF75EF3CFE2A677AC8175E3360A8904B29E488AF5383498BC5`;
the F3 tested-build fingerprint was
`DD32D5822B61FB20491BE1C7ADE785E53BB69B71F0C0ACC4D20043EE4D0C8C39`.
Firmware and dependency archive verification passed. A pre-edit inventory hashes
119,769 preserved F1/F2/F3 evidence files. No reset, clean, stash, branch change,
staging, commit, push, installation or security-policy change was performed.

## MIDI first divergence and repair

The actual failing REAPER fixture sends channel-1 Note On 60/118 at sample 9,600
and Note Off 60 at sample 16,800, at 48 kHz. Its event offsets are valid. The
preserved 24-bit renders first differ at byte 55,149 / stereo frame 9,191.

The pinned iPlug2 dispatcher retains VST3 sampleOffset in IMidiMsg.mOffset.
LXR previously discarded that offset and dispatched before the callback's audio.
The same Note On therefore arrived at callback start 9,525/offset 75 with
127-frame callbacks, versus start 9,216/offset 384 with 512-frame callbacks.
Their next native render boundaries were 8,736 and 8,448 respectively.
No event-order divergence or invalid fixture was found.

An exact-DLL MSVC diagnostic reproduced the discrepancy with sequencing disabled
first, then Host Sync. One-frame microscope runs replayed the demonstrated
legacy effective receipt times and reproduced each real callback render exactly.
They captured the resident native blocks and Engine phase/velocity observations.
Native PCM first differed at frame 8,448, before FIR conversion: disabled 0 versus
-22, maximum integer difference 3,816; synchronized 4 versus -34, maximum 3,820.
Uncompensated double host PCM first differed at frames 9,216/9,217 respectively.
Clean-reset repeats were identical. The original compensated/quantized REAPER
first difference and the diagnostic double-PCM first differences are separate
measurements. Full raw PCM and event/native-boundary CSVs are retained.

The correction is local to LXR's VST3 shell. It collects up to 4,096 valid MIDI
messages in resident instance storage, orders by offset and arrival sequence,
and dispatches each at its host sample before requesting audio. iPlug2's CC
queues precede its note list, so timestamp sorting is necessary. Equal-offset
arrival order is preserved. Native buffering still quantizes synthesis to the
next available 32-frame render boundary; the FIR and reported PDC are unchanged.
Invalid offsets reject observably; capacity overflow rejects newest. No future
event is clamped into another callback. The existing bounded deferred FIFO still
handles publication contention. Bypass and zero-audio calls retain MIDI/state
dispatch. Direct non-VST3 ProcessMidiMsg callers retain their existing behavior.

The original exact-DLL fixture passes 248 checks. Expanded boundary, simultaneous,
seven-channel/shared-hat and velocity cases pass 1,406 checks through approved VS
LLVM. Fixed 32/64/128/256, alternating, irregular, 127 and 512 callbacks produce
bit-identical host PCM with sequencing disabled and enabled. Captured native PCM
also matches. Actual REAPER MIDI coexistence PCM now matches across 127/512.
These results do not claim the entire requested MIDI/transport matrix is closed.

The final mandatory CC extension independently demonstrates a remaining genuine
ingestion defect. It sends Note On at9,500, CC88=0 at9,600, CC88=100 at9,664,
then Note Off at16,800. Both ascending CC points are valid in one VST3
IParamValueQueue. The pinned ProcessParameterChanges dispatcher reads only
getPoint(numPoints-1); grouping both values in one callback therefore discards
the first value before LXR receives MIDI. The 128/256/alternating/512 profiles
differ from32/64/irregular/127, with sequencing disabled and enabled. First host
differences are9,600/9,601 and maximum absolute difference0.04967037262126655.
The expanded exact-DLL suite reports1,652 PASS /8 FAIL. This inherited point-loss
path is separate from the repaired note-offset defect. The user's stop
condition8 requires stopping production repair on this newly demonstrated
regression defect. No framework or synthesis redesign was attempted. The
overall MIDI gate and combined F3/F4 classification are **FAIL**.

## Candidate and completed acceptance

Final private candidate (build002):
`179E6643FC4E6905E89EF8FB273382CCAB866BC145C24B3F2D9917B5EBFD0FC8`,
1,251,328 bytes. VS Community18 / MSBuild18.10.1 / Release x64 / v145;
pre/post build deployment disabled, including PostBuildEventUseInBuild=false.
Build001 is superseded diagnostic evidence. All final-candidate acceptance here
loads build002. Later edits are verification/docs only; the manifest checks the
actual production source hashes rather than equating two different whole-tree
fingerprints.

Final host state/runtime/controller/M2G/M2H/F2/roll/mute/internal-probe suites pass;
validator passes 47/47. REAPER profiles each pass 40 checks and report PDC26 at
48kHz. Independent full-bank/state comparisons pass. The rich two-instance case
passes 44 checks, all 245 controller values, twelve enabled routes per instance,
complete state identity and three identical matched MIDI renders.

Both previously outstanding snare regressions execute through approved VS LLVM:
snare render /Od 61/0, zero render allocations; snare/cymbal/hat /O2 47/0. Fresh
GCC counterparts also pass. Other fresh native and host artifacts remain
Application Control-denied; individual raw statuses and Code Integrity events
are retained. An aggregate transport runner labels its output FAIL despite a
WinError4551 launch denial; this is classified as policy BLOCKED, with its
passing O2 6,246 checks reported independently. No assertion is relaxed.

Final-DLL instrumentation measures zero plugin-controlled allocations across
4,000 varied callbacks (13,648-byte observed processing stack on a 1MiB thread).
A full-capacity 4,096-event callback independently measures zero allocations and
9,128 bytes. These measured scopes and PE/PDB unwind review are not a universal
allocation proof or exhaustive call-chain stack bound. Import/serialization
remain non-real-time operations. LXRSv3 remains 51,065 canonical bytes, bounded
at 65,536, with unchanged v1/v2/legacy readers and wire format.

## Outstanding gates and preserved inequalities

- MIDI CC: valid multiple points in a single existing CC queue still produce
  callback-dependent PCM, as independently reproduced above. Production work
  stopped under the explicit regression stop condition.
- Production Import: the exact private module is loaded in task-owned
  F4_IMPORT, but Computer Use app approval timed out after window selection.
  No click or typing occurred. Parser/import probes cannot replace this gate.
- Policy: denied exact artifacts and current variants remain BLOCKED. Current
  snare/percussion recovery does not relabel other denied executions.
- Stress: the active-routing fourteen-instance scenario completes two valid
  192,000-frame renders with identical PCM and fourteen distinct restored banks,
  full states and routes. An earlier OFF-route stress case first differed at
  frame0 despite identical state bytes. Its cause remains unresolved, so the
  stronger scenario does not erase that raw failure or close reconciliation.
- Cross-profile roll controls: selectors0 and12 retain PCM inequalities, with
  first bytes70,710 and719,931. Selector0 was already callback-quantized in F3;
  selector12 requires further localization. Neither is silently relabelled PASS.
- Live transport exercises play/stop/resume, nonzero start, seeks, tempo changes,
  seven loop wraps and held-roll stop release. Coarse host observations are not a
  sample-exact event trace. Remaining independent host/MIDI performance boundaries
  are explicitly incomplete in the matrix.
- Listening: historical seventeen-case acceptance is retained. Four new MIDI
  cases are available in `out/p16f4/listening/playlist.m3u`. The user's reply
  "approved you can keep going" authorizes continuation and supplies no auditory
  observations, player or output. Renewed acceptance remains
  **BLOCKED — PENDING_HUMAN_ACCEPTANCE**.

The protected installed DLL remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`,
with all seven resources unchanged. P16-F1 and F2 remain BLOCKED; combined F3/F4
and overall P16-F4 are FAIL because the CC regression is an executed defect.
Publication is BLOCKED/NOT ATTEMPTED. No commit titles or SHAs were created.
The next milestone is not ready. No presets, morphing, factory kits, graphical
editing, sample loading or unrelated instrument features were implemented.
