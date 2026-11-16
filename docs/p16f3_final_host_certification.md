# P16-F3 final host certification

Status: **BLOCKED**, not certified or authorized for publication.
Baseline branch `p12b-4c-fir-resampler`, local/live remote HEAD
`7749ef1a0203f5ab92bf0e2f447b53bb95dadf68`. All 39 P16-F2 inventory hashes,
the empty index, firmware/dependency pins, private C2ECE0...B7C939 bundle and
protected installation/seven resources verified before production edits.
Original P16-F1/F2 evidence is read-only during this task.

## Manual-roll source audit and control decision

Pinned firmware `dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`:
`front/LxrAvr/buttonHandler.c:579-585,648-654` sends an ON message on a
performance-button press and an OFF message on release. The low nibble is the
track; the high nibble is the press state. STM32
`MIDI/frontPanelParser.c:857-866` decodes that front-panel protocol and calls
`seq_setRollRate` or `seq_setRoll`. It is not ordinary external MIDI CC input.
The production plugin has no existing path forwarding external MIDI to these
front-panel operations. Do not reinterpret a currently assigned MIDI CC.

`Sequencer/sequencer.c:973-988` sets/clears the track bit. Selector zero emits a
single trigger on press, not continuously while held. Periodic rolls use the
currently held bits, ignore main gates/mute/probability, and dispatch after the
ordinary event for that track. Tracks 5 and 6 target the shared hat voice in
ascending closed/open order. `seq_setRollRate:990-1067` gives selectors 0..13:
one-shot,128,64,43,32,50,16,11,8,6,4,3,2,1 physical positions. The selector-5
period 50 is retained. The eighth front button has no eighth logical track;
STM32 explicitly rejects track >=7.

Add only two host parameters, available in REAPER's generic parameter UI:

| ID | Name | Integer range | Default | Normalized value |
|---:|---|---|---:|---|
| 243 | Sequencer Roll Rate | 0..13 | 8 | selector / 13 |
| 244 | Sequencer Roll Tracks | 0..127 bit mask | 0 | mask / 127 |

Mask bits 0..6 mean D1,D2,D3,SN,CY,closed hat,open hat. A rising bit represents
button press; a falling bit represents release. Repeated delivery of a held bit
does not generate another one-shot. Several bits may be held simultaneously.
The mask is held-button state, not a toggle command. Rate is applied before mask
edges at a callback boundary. Changing a held roll's rate does not synthesize
another button press. Invalid normalized values/NaN/Inf reject before cache
mutation; ordinary normalized values quantize through the existing IParam rule.

Both controls are transient performance state. The original pattern-file stream
does not carry roll selection, and LXRS v3 remains frozen: 51065 canonical VST3
bytes, maximum65536. Rate defaults to8 and activation to0 on full project restore;
neither field consumes reserved wire bytes. Hosts may separately save explicit
automation lanes. No stale held bit is recalled by the component chunk.

The existing common one-attempt coordinator and bounded ticket mailboxes publish
accepted controls. The audio owner consumes a coherent control snapshot. Stop,
disable, deactivation and complete restore release held/pending rolls. A ticket
cutoff prevents an older pending press or older accepted mask from resurrecting
after release; a later explicit press is a new command. The release cutoff is a
plugin safety adaptation, not a claim that hardware stop clears its roll mask.
One-shot presses with sequencing enabled may trigger while Host Sync is stopped;
periodic rolls require advancing physical ticks. No host gesture is emitted by
stop/reload reconciliation. No new GUI or MIDI mapping is added.

## Measurement plan

One rebuilt final private hash will own every F3 host result. Exact-DLL allocation
measurement will intercept the module's imported allocation APIs in memory in a
normal test host, retain original functions, count only the profiling thread and
measured plugin-call scope, and use setup/state positive controls. The on-disk
DLL is never altered. Coverage/limits and every imported allocation API must be
reported; working-set stability is not an allocation measurement.

Stack evidence will combine final-module unwind/frame metadata and bounded-source
inspection with stack-watermark probes on an ordinary default-size test thread.
No acceptance may depend on the old 8 MiB test-executable stack option. Import and
state parsing are non-real-time and use fixed buffers, never input-sized alloca.

The human operator confirmed all 17 controlled playlist cases, with no issues,
using VLC through headphones on 2026-11-10. Record the operator as the user,
self-reported; no personal name or case-specific observations were invented.
The final DLL reproduced all 17 WAVs and the reload repeat byte-for-byte against
the listened fixtures. See `out/p16f3/listening_acceptance.md` and
`out/p16f3/listening-final-008/human_fixture_identity.json`.

## Confirmed tempo-marker defect and narrow repair

Candidate26456B...409A61, exact-DLL reproduction in
`out/p16f3/tempo-boundary-001`: a120-to60 BPM marker at host frame48000
split exactly at the marker produces identical PCM at127/512 callback frames,
160 physical positions and33 triggers. Leaving the marker inside the callbacks
causes one spurious seek:127 frames yields159 positions/30 triggers,512 yields
160/33. The first PCM difference is frame48033 in this reproduction. The REAPER
tempo-map comparison first differs at frame96004. Earlier passing constant-tempo
results do not cover this defect.

The host reports one BPM per callback. The subsequent PPQ integral may therefore
lie between predictions using the old and new BPM for the previous callback.
When sample time is exactly continuous, BPM changed and PPQ is inside that
bounded interval (plus existing two-sample tolerance), classify the callback as
TempoChange. Keep the pending physical ordinal and RNG/chain history. Any
correction outside the interval remains a seek. This does not invent an
intra-callback tempo ramp or reconstruct already rendered history. Simultaneous
PPQ relocation and tempo change inside this indistinguishable interval follows
the explicitly defined tempo-correction policy; absent a host discontinuity flag
the two cases cannot be distinguished.

This production edit invalidates the earlier exact-binary certificate. Rebuild
and repeat host/validator, REAPER, allocation/stack and affected transport tests
on the next candidate before assigning final results. The original failing
inputs and outputs remain under `out/p16f3/`.

The real REAPER live fixture also found a held-roll release defect. REAPER stops
and rewinds to its edit cursor, so the transport's documented classification is
Seek with playing=false, rather than Stop. The new roll adaptation released on
Stop/invalid context only. A read-only probe used the matching PE/PDB timer
binding and verified the exact task process/module/vtable before reading only
known plugin fields: playing=false, sample/PPQ0, logical/audio/applied mask1,
hostStops0, seeks3, loops7. Evidence `out/p16f3/readback-live-003/run.log`.
No target process memory was written and no debugger/process termination used.
Release rolls on a Seek while stopped as well. This leaves the P16-F1
reset/reanchor classification and sequencing algorithms intact. Add a native
exact-DLL stop-plus-rewind/controller/resume regression and repeat the live host
fixture and final-binary gates on the next candidate.

The mandatory roll/mute interaction exposed the existing production NRPN200..206
no-op in `dsp/MidiParser.cpp`. `out/p16f3/midi-mute-defect-002` sends the real
VST3 MIDI-controller assignments and finds two failing gates: ordinary D1 events
remain audible and the scheduler sees no seven-track mute mask. Pinned firmware
`MidiParser.c:1067-1087` calls `seq_setMute(number-200, value!=0)`.
Connect this already-defined command to the existing processing-owner scheduler
through an optional `processEvent` sink; default-null standalone MIDI-only
callers retain their prior behavior. The VST3 supplies its existing scheduler.
No new host IDs, musical-bank fields, GUI, synthesis or NRPN selection semantics
are introduced. Muting remains transient; project restore resets it through the
existing scheduler reset. Periodic/one-shot rolls retain the original mute
bypass. Repeat MIDI/native regressions and final-hash certification after this
small integration repair.

## Final candidate and verified checkpoint

Final private SHA-256:
`28F3639D2AB00FFBF34857BDC1B2ED89260F1360E7D2973C00209B8F90AC654A`.
Module: `out/p16f3/private-build-006/bundle/LXR.vst3/Contents/x86_64-win/LXR.vst3`,
1,248,256 bytes. Build input fingerprint:
`DD32D5822B61FB20491BE1C7ADE785E53BB69B71F0C0ACC4D20043EE4D0C8C39`.
VS Community 18, MSBuild 18.10.1.42706, toolset v145, compiler tools 14.51.36231,
Release x64. Pre/post build deployment events disabled. The matching PE/PDB and
seven private resources are recorded in the candidate manifest. No production
source changed after this build; subsequent edits concern verification/docs.

Final exact-DLL automated host suites all pass: state 322, runtime 1,143,
controller 955, M2G 91, M2H 689, F1 105, F2 transaction/PCM 3,300, F3 roll 260,
MIDI/mute 216, internal probe 1,033, validator 47/47. F3 roll also passes 260
checks with an Od harness. Focused exact-DLL tempo-boundary 27, production import
and rejection 62, listening-fixture 344 and resource instrumentation 39 checks
pass. Some unchanged permitted harnesses were reused with identical current
source against the new DLL; their provenance is explicit, and denied artifacts
remain BLOCKED. Codec O0/O2 each pass 152,002 checks with unchanged input hashes.

Current native checkpoint: 31 PASS and 13 Application Control-denied GCC
artifacts. Approved VS LLVM counterparts and their remaining denials are recorded
separately; current snare-render O0 and snare/cymbal/hat O2 remain BLOCKED. The
original-C base-mapping O2 equivalent also passes 30,169 checks. No Windows
security policy changed. The historical exact FIR Functional O2 artifact remains
separately BLOCKED. Native C/D/E original-source differential suites pass at
O0/O2, with all recorded inputs still matching. TSan was not run.

Final-DLL IAT instrumentation covers all four imported allocation APIs, with an
initialization positive control. Across 4,000 variable processing callbacks,
including restored state, tempo/seek/loop/stop, locks, rolls, notes and NRPN mute,
plugin-controlled allocation count is zero. Observed processing stack watermark
is 13,616 bytes on a 1 MiB thread. NonRT import/setState/getState watermarks are
213,872/275,776/228,864 bytes. Unwind analysis and source review complement these
observations; they do not prove an exhaustive whole-call-chain maximum. See the
allocation and stack reports for scope and unsupported helper unwind records.

Real REAPER 7.80 final-DLL profiles 026/027 exercise callback sizes 127/512,
save/reopen, controller/bypass restoration, 120/60 BPM, Internal, tempo maps,
shuffle, random chaining, rolls and two instances. Their individual completed
checks and independent full-state/PCM comparisons are authoritative; an
IN_PROGRESS profile is not a completed certificate. Live profile 028 records
nonzero peaks, seeks, loop wraps and held-roll release after stopped rewind.
The fourteen-instance profile 029 exercises all source roll selectors; independent
readback records exact module/vtable identity, per-selector event totals and
released masks. Native-only algorithm cases are explicitly distinguished from
REAPER coverage. No unavailable event timeline or listening result is fabricated.

## Remaining certification barrier

The final production file parser accepts/rejects banks correctly and the final
DLL plays production-restored banks in REAPER. A genuine production Import
button action on the preceding build004 was verified independently by a complete
canonical bank readback. That earlier hash is supporting evidence only.

Repeating the UI import on final build006 is BLOCKED by Computer Use access:
the first initializing window reported no foreground process ID; later ready
windows returned "Accessibility state unavailable". Even the explicitly visible
`FINAL_IMPORT_032` window returned a screenshot of a different task project under
the terminal overlay. A normal bound Save did not yield a verifiable target.
The user's textual access approval did not produce reliable control of the named
window. No Import click was made on the mismatched project.
No security setting, terminal window or unrelated application was manipulated.
The final-hash UI action is not replaced with a test-only setter or credited
from the preceding binary.

Combined F1/F2/F3 and publication remain BLOCKED. Additional comprehensive host
performance cases remain native-only or unfinished as detailed in the evidence
matrix. There is no staging, commit or push while a required gate is blocked.
The source and prior evidence are preserved for continuation; do not start the
next milestone. Native `.pat`, SOM, morphing, presets, graphical editing and user
samples remain deferred.

## Signed host sample endpoint repair

The final review found unchecked `previousSample + previousFrames` in continuity
classification. With previous sample `INT64_MAX`, one frame and current sample
`INT64_MIN`, the previous implementation evaluates signed overflow before it can
classify a seek. An approved VS LLVM `-ftrapv` reproduction exits with
`0xC000001D`; see `out/p16f3/endpoint-llvm-001`. GCC UBSan is unavailable because
its runtime library is absent, and the fixed standalone LLVM trap executable is
Application Control-denied; neither is reported PASS.

The narrow repair checks representability before evaluating the sum. A boundary
that ends exactly at `INT64_MAX` remains continuous; an unrepresentable next
sample endpoint is a bounded seek. Three added assertions pass in the complete
transport suite: O0/O2 each 6,246 checks, zero measured processing allocations.
No wire field or normal transport timing changes. This edit supersedes build005;
its reports are preserved in `out/p16f3/checkpoint-build005/`. All replacement
candidate results must identify build006's full SHA above.

An optional P16-D LLVM test exhausted its default 1 MiB executable stack. Its
ordering function places five 151 KiB authorities and five 50 KiB banks plus an
Engine on the test stack. The documented current GCC O0/O2 P16-D suite passes
246 checks each in `out/p16f3/p16d-final-004`. This harness limitation is separate
from final-DLL processing measured at 13,616 bytes on a 1 MiB thread. No production
stack acceptance relies on the legacy 8 MiB test setting.

## Completed final-hash host subsets and timing limits

Profiles 026/027 each complete 40 checks on the replacement hash, at 127/512
callback frames: nonzero sequence, 120/60 BPM and Internal, tempo-map marker,
random-next/shuffle/roll rendering, all fourteen roll selectors, two independent
banks, project reopen, all controller values, bypass and MIDI coexistence.
Independent canonical state comparisons preserve all 7,168 positions, gates and
metadata in each stored bank. Runtime bank dumps from both processes match the
musical fixture SHA `74A5A73030212A1F3641CD6C5BB86AB8290DB269530F9E6E43D2F68D988A1D90`.

Profile 034 adds the necessary nonzero-routing case: two banks, five changed
patch values per instance, all twelve routes enabled per instance, distinct
transport configuration and host bypass. All 245 controller values and complete
serialized bytes survive reopen and a held-roll save; three matched MIDI-bearing
renders are bit-identical. Its 44 host checks and independent inner-v2 CRC/route
record oracle pass. REAPER's outer `BYPASS` wrapper and the plugin's internal
bypass suffix are checked separately; native SDK tests independently exercise a
nonzero internal suffix. Final F2 transaction/PCM also passes 3,300 checks with a
current-source LLVM Od harness; the denied MSVC Od artifact remains BLOCKED.

Cross-callback comparisons retain two raw inequalities: selector-zero one-shot
control timing and MIDI coexistence. The MIDI first difference is PCM byte 55,149,
stereo frame 9,191. Both the starting commit and current plugin discard
`IMidiMsg::mOffset` and dispatch MIDI before rendering the callback. The existing
MIDI timing behavior was preserved. No sample-accurate MIDI or cross-callback MIDI
PCM identity is certified, and the failed raw comparison remains in the matrix.
Matched-callback repeat/reopen MIDI PCM is identical. The one-shot parameter edge
is also callback-quantized by the pinned host dispatcher; this is not claimed to
be sample-accurate automation. Pattern-driven transport/tempo-map comparisons
without those varying control-delivery times are reported separately.

The separate fourteen-instance stress render has not finalized its WAV, although
read-only event counts and released masks match the source oracle. It is not a
completed PCM PASS. The fourteen individual selector renders in each completed
main profile are independent evidence; the unfinished stress render is retained.
