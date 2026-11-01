# P12c state serialization

P12c-R2 follow-up (**PASS**, 2026-10-30): see
[controller synchronization repair](p12c_r2_controller_sync_repair.md) for the
latest acceptance result and exact binary identity. LXRS v1 encoding and legacy
migration are unchanged. Serialization still reads only the authority; main
idle projects acknowledged generations into the separate VST3 cache and uses
`kParamValuesChanged`, without automation gestures. Synthetic MIDI CC inputs are
canonicalized to seven-bit wire values before iPlug2's truncating dispatcher.
Host automation/restore reach native DSP at a processing boundary. MIDI save
checkpoints are taken before any subsequent host-generated reset messages.

Date: 2026-10-26  
Branch: `p12b-4c-fir-resampler`  
Starting HEAD: `280279debe94cb26810ea89b89f36b9bf965a747`  
Latest certification (2026-10-29): **FAIL — MIDI VST3 controller projection**.

The [final-hash certification](p12c_r1_final_certification.md) supersedes the
historical status statements below. Exact B170E009...93A6F953 REAPER state,
deterministic audio, transport and PDC checks now pass; both exact policy-denied
O2 files now run normally. However, a new installed-binary assertion finds
serialized MIDI values 77/45 while controller readback remains 100/0 after idle
notifications (34 pass, 2 fail). REAPER's desired-value MIDI save/reopen test
stops at controller agreement and is not claimed as passed. The LXRS v1 format
is unchanged; no production/framework repair was made in this verification
task. P12c is not DONE and P16 was not started. The remainder retains the
2026-10-26 evidence and chronology.

The state implementation, installed VST3 host tests, validator, and REAPER
save/reopen tests pass. P12c is not marked DONE. The final certification audit
found that the previously blocked O0 LFO test now executes normally, while two
O2 broad-regression executables remain blocked by Windows Code Integrity. The
audit also identified two concurrency gaps described below. No observed test
assertion failed.

## Framework findings

The installed iPlug2 default is not sufficient for this milestone. Its
`SerializeParams()` writes 228 native-endian `double` values without a magic,
version, bounded payload length, or checksum. `UnserializeParams()` applies each
value while decoding, so a truncated stream can partially change the current
patch. The installed VST3 wrapper appends a four-byte bypass value after plugin
state.

The VST3 SDK documents component `setState()`/`getState()` as UI-thread calls,
but permits them while the component is active or processing. `IParam` values in
this iPlug2 checkout are atomic. The custom restore therefore validates into a
local candidate first and publishes the complete candidate through fixed atomic
storage. The audio side consumes it at a processing boundary.

## Version 1 binary contract

All multibyte fields are little-endian. No C++ object representation is stored.

| Offset | Size | Field | Version 1 value |
|---:|---:|---|---|
| 0 | 4 | Magic | ASCII `LXRS` |
| 4 | 2 | Format version | `1` |
| 6 | 2 | Header size | `24` |
| 8 | 4 | Payload size | `228` |
| 12 | 2 | Native slot count | `228` |
| 14 | 2 | Flags | `0` |
| 16 | 4 | Extension size | `0` |
| 20 | 4 | CRC-32 | IEEE CRC-32 of the payload |
| 24 | 228 | Parameter payload | IDs 0 through 227, one bounded 7-bit byte each |

The LXR payload is 252 bytes. iPlug2 VST3 appends its documented four-byte
bypass field, so `IComponent::getState()` returns 256 bytes. Slot 0 remains
reserved and has no DSP effect, but it is preserved exactly as a bounded 7-bit
value so the host-visible, native, and serialized representations cannot
diverge. IDs 1-227 are the active instrument parameters.

Version 1 requires flags and extension size to be zero. This deliberately leaves
an extension point for future sequencer state without silently interpreting an
unknown layout. Unknown versions, invalid header values, non-7-bit parameter
bytes, inconsistent sizes, CRC failures, truncation, oversize input, and
unexpected trailing bytes are rejected.

The maximum accepted state stream is 4096 bytes. A future sequencer format must
increment the version, define its extension payload, include it in an explicitly
defined integrity scheme, and provide a migration path. This work does not claim
compatibility with LXR hardware kit files.

## Legacy compatibility

The reader accepts the exact historical iPlug2 stream produced by the previous
Windows VST3: 228 native-endian doubles, optionally followed by the VST3 bypass
integer. Every value must be finite, integral, and in 0-127. A successful legacy
load is reserialized in version 1 format. No heuristic partial or oversized
legacy input is accepted.

## Restore and synchronization model

Restore proceeds in this order:

1. Bound the stream and validate its format, version, lengths, CRC, and every
   parameter into a local 228-byte candidate.
2. Publish the complete candidate with an odd/even atomic sequence and update
   all iPlug2 `IParam` values.
3. At the next `ProcessBlock()` or MIDI boundary, take a stable candidate
   snapshot, reset Engine voices/modulation/mixer, clear the FIR history and
   native MIDI parser, initialize `ParameterArray`, and apply all 228 values.
4. Preserve loaded sample assets and the current host-rate FIR coefficient bank;
   no asset I/O or coefficient generation occurs in the audio callback.

The processing path adds no allocation, lock, file I/O, exception, or logging.
Normal host automation sets atomic `IParam` values and raises a single atomic
dirty flag. At the next processing boundary, only changed native values are
applied, so ordinary automation does not reset active voices. Native MIDI
CC/NRPN processing remains authoritative for its event, mirrors changed values
to `IParam`, and queues main-thread UI/host gestures through `OnIdle()`. This is
the intended convergence path; the final certification audit found overlap
conditions where the current sequencing is not sufficient, as recorded below.

The installed iPlug2 VST3 state reader was narrowly hardened so a negative plugin
decode result, failed seek, or missing/incomplete four-byte bypass suffix returns
failure instead of reporting corrupt state as restored.

## Automated results

### Pure codec and native Engine

- O0: 29/29, exit 0.
- O2: 29/29, exit 0.
- Repeated identical serialization is byte-identical.
- Every 7-bit value survives for every active ID; nonzero reserved slot 0 also
  round-trips.
- Empty, every truncation, oversize, invalid magic, future version, corrupt
  length, checksum failure, invalid parameter byte, and trailing data are
  rejected without changing the destination.
- One hundred save/load cycles are stable.
- Controlled D1-D3, snare, cymbal, closed-hat, and open-hat PCM is exact after
  restore. Aggregate peak is `19049`; RMS is `4330.850987486928`.

### Actual installed VST3

- Host-state suite: 322/322, exit 0.
- State size: 256 bytes (252-byte LXR payload plus four-byte bypass).
- Default, edited, legacy, corrupted, MIDI CC/NRPN, rate-change, and two-instance
  cases pass.
- Controlled restored PCM is exact; peak `0.622664248526`, RMS
  `0.302805758317`.
- Both differently configured host instances are audible and render differently.
- FIR latency remains 26 samples at 48 kHz and 51 at 96 kHz in this suite.

### REAPER 7.80/x64

The final isolated project used the exact installed binary:

`C:\Users\valen\AppData\Local\Programs\Common\VST3\LXR.vst3\Contents\x86_64-win\LXR.vst3`

SHA-256:

`617D4CFDF77AD8AB4B07177CB3C61D06824FD6B8304A59C64A15EF86F6750B5F`

Two distinct LXR instances were created. REAPER saved the project, both
instances were deliberately changed, and `Main_openProject()` reopened the saved
project. All 456 native slots checked across the two instances were restored
(454 active values plus both reserved slots), with zero mismatches. Reserved
slot values 77 and 0 were independently retained.

A deterministic channel-1 D1 path excluded free-running modulation and muted the
second track only for the audio comparison. The two 48 kHz, stereo, 67,200-frame
24-bit renders are sample-for-sample identical (maximum difference 0), finite,
and nonzero. Peak is `0.00064003467559814453`; RMS is
`0.00014255583587496562`. Two transport start/stop cycles passed. Multi-instance
audibility is covered separately by the installed VST3 host test.

Both REAPER instances report 26 samples through the supported `pdc` named
configuration parameter. A separate normal-versus-ignore-PDC render measured a
26-frame host-applied shift, zero alignment error, and the expected
`-0.365243087569262` frame nominal fractional residual. That render is 48 kHz,
stereo, 86,400 frames, peak `0.0499974489212036`, RMS
`0.00142169465432634`.

Authoritative final reports:

- `out/reaper-p12c/p12c_results_final.json`
- `out/reaper-p12c/p12c_report_final.md`
- `out/reaper-p12c/p12c_state_roundtrip_final.rpp`
- `out/reaper-p12b4c/p12c_fir_pdc_final_48000_results.json`
- `out/reaper-p12b4c/p12c_fir_pdc_final_48000_report.md`

Earlier failed comparison artifacts were preserved. They demonstrate that an
exact render comparison is invalid when a free-running modulation oscillator has
different pre-trigger history. The final test uses matched restored initial
conditions and disables that modulation on the deterministic comparison voice;
it does not weaken the equality requirement.

## Build, validator, and regression preservation

- Visual Studio Community 18 / MSBuild 18.10 / Release x64 / v145: compile,
  link, bundle copy, installed copy, and all seven resource checks pass, exit 0.
- Built and installed DLL hashes match the SHA-256 above.
- Steinberg validator: 47 passed, 0 failed, exit 0.
- Installed VST3 runtime: 1,143/1,143, including channel-4 snare, all latency
  rates, state, and channel isolation.
- Snare original-C differential: 61/61 at O0 and O2, zero sample differences.
- Runtime synthesis: 419/419 at O0 and O2.
- FIR functional: pass at O0/O2. FIR quality: 38/38 at O0/O2. FIR latency:
  214/214 at O0/O2. Original-C filter differential: 80,000 blocks with zero
  mismatches at O0/O2.
- Fresh broad DSP build/run: 12 of 13 O0 programs pass; 1 is policy-blocked.
  Eleven of 13 O2 programs pass; 2 are policy-blocked.

Windows Application Control blocked the freshly compiled O0 `LfoModulation`
executable and the freshly compiled O2 `ResonantFilter` and `TransientGen`
executables. No security policy was changed and no bypass was attempted. The
same source areas pass in other runnable optimization/configuration artifacts;
these launches are still classified BLOCKED, not PASS.

## Remaining limits and classification

- REAPER save/reopen was verified at an effective rendered rate of 48 kHz. The
  installed-host test separately verifies FIR reconfiguration at 96 kHz, and the
  existing runtime matrix covers all six required latency rates.
- Exact audio equivalence intentionally uses a deterministic tonal path.
  Noise-path state comparison remains covered by parameter equality and native
  controlled-RNG tests; hardware RNG runtime state is not serialized.
- Two final task-created REAPER processes may remain open behind REAPER's
  evaluation notice. Their state project is saved, and the PDC project/report/
  renders are preserved. No unrelated session was closed or altered.

The earlier P12c implementation and host verification passed, but the final
certification findings below supersede this preliminary classification.

## P12c-R1 concurrency hardening

The two concurrency findings in the final audit are repaired by the
plugin-local `ParameterStateAuthority`, bounded coherent snapshots, a
generation-ticketed automation mailbox, and ordered deferred MIDI FIFO. LXRS v1
bytes are unchanged. Focused concurrency tests pass 35/35 at O0 and O2; the
installed state suite passes 322/322 and validator passes 47/47. Full design,
memory ordering, callback audit, test evidence, and remaining certification
blockers are in `docs/p12c_r1_concurrency_hardening.md`.

P12c remains BLOCKED rather than DONE because the current-hash isolated REAPER
run could not complete through native UI automation and the two established O2
Windows Code Integrity denials remain. The concurrency findings themselves are
closed; no sequencer work is authorized from this status.

## Final certification audit — 2026-10-26

### Windows policy status

The exact O0 `out/LfoModulation_test.exe` (SHA-256
`66EA5075B1FF553A58DD0BE177A3F2AA6897F0BBB0FB4A00736ACF7476794301`)
now executes in place through the normal Windows process loader and passes
18/18. It was not renamed, relocated, resigned, or rebuilt for that retry.

Two exact files remain blocked:

- O2 `out/ResonantFilter_test_O2_p12c.exe`, SHA-256
  `94E53F9616522D6E01D2A5847756C3AC2263008D8AD94A6FCE1CAE3DAFC286A3`;
- O2 `out/TransientGen_test_O2_p12c.exe`, SHA-256
  `D34E2410BA5AF960AE11C1496879DF35F00356AEF472F36A5A94E4F3C0B3D7C7`.

Both are unsigned. Fresh normal launches produced Code Integrity events 3033
and 3077 (first fresh pairs: records 22123/22125 and 22135/22137). The 3077 XML
identifies policy `VerifiedAndReputableDesktop`, policy ID
`27555.1000.240208`, GUID
`{0283ac0f-fff1-49ae-ada1-8a933130cad6}`, policy hash
`2668895A5B233A80432D00D67251D7B7F52686A3FB13780F4B242C5A1F937A01`,
requested signing level 2, validated level 1, status `0xc0e90002`, and signing
scenario 1. `Win32_DeviceGuard` reports both Code Integrity and user-mode Code
Integrity enforcement status 2. AppLocker EXE/DLL logs contain no matching
event, and none of the files has a Zone.Identifier stream. This is therefore a
user-mode Windows Code Integrity/enterprise-signing policy denial, not evidence
of an AppLocker rule or Internet-zone/path restriction.

No compliant enterprise signing service, supplemental allow policy, or
policy-approved test host is available in this session. The two files remain
BLOCKED. An administrator must supply one of those approved mechanisms; the
policy must not be disabled or weakened.

### Framework modification audit

`vendor/iPlug2/IPlug/VST3/IPlugVST3_Common.h` differs from official iPlug2
upstream at `d82b2e99b17c8f723d3f6e2a88ffd8b656f28e5d` only in the shared
`IPlugVST3State::SetState()` error path. Upstream ignores a negative
`UnserializeState()` position, ignores `seek()` failure, and does not verify the
number of bypass bytes read. The local change rejects all three cases before
controller update and `OnRestoreState()`.

The change is required if corrupt LXR state must make VST3 `setState()` return
failure: the plugin callback can return a negative position, but the unmodified
shared wrapper discards that result. There is no LXR-only hook in the installed
adapter that can change the wrapper's final VST3 result. A custom LXR VST3
adapter would be a larger and riskier fork.

The change is global to every iPlug2 VST3 built from this checkout. It does not
change `GetState()`, valid plugin bytes, bypass encoding, bypass offset, or
valid historical project migration. It only changes malformed-state behaviour
from possible false success to failure, which is consistent with the callback
and `IBStream` contracts. Valid LXR state, legacy state, corruption tests,
REAPER reload, and validator all pass. The failed-seek and short-bypass branches
do not yet have a dedicated framework-level unit test, so the framework patch
should remain an isolated commit and the vendor revision should be pinned.

### Concurrency audit

Complete validation before publication ensures a malformed state cannot
partially alter `IParam`, pending restore storage, `ParameterArray`, or Engine.
The odd/even restore sequence also prevents the audio side from consuming a
half-copied restore candidate. No allocation, lock, I/O, or logging was found in
the added processing-boundary path.

Two unresolved overlap conditions remain:

1. `SerializeState()` reads 228 individually atomic `IParam` values without a
   shared generation. If processing-thread automation changes multiple
   parameters while the UI thread is saving state, the encoded chunk can be a
   valid CRC-protected mixture of values from different instants. Severity:
   **medium**; reproduction requires project save/state capture to overlap a
   multi-parameter automation update.
2. `ProcessMidiMsg()` calls `ApplyPendingStateAndParameters()`, but if restore
   publication is odd that function returns and MIDI processing continues. A
   simultaneous UI-thread `setState()` and processing-thread CC/NRPN event can
   therefore leave the later restored Engine/`ParameterArray` value different
   from an `IParam` value written by MIDI, depending on ordering. Severity:
   **medium-high**; VST3 explicitly permits UI-thread state calls while the
   component is processing. The deterministic single-thread and REAPER tests
   do not exercise this exact overlap.

These are synchronization findings, not corrupt-payload failures. A correction
needs a single plugin-local authoritative snapshot/publication protocol shared
by host automation, MIDI, serialization, and restore, followed by a deliberate
concurrency stress test. It must remain lock-free on the audio thread.

### Fresh build and host evidence

MSBuild 18.10.1, Release x64, platform toolset v145, `/t:Rebuild` completed with
exit 0. Compilation, linking, packaging, installation, and all seven source-to-
installed resource hash comparisons passed. The built and installed DLLs are
726016 bytes and hash identically:

`45750AA46CCFBD3BBF080DBE81B682874038CC24C27B82AEE85A419A721B494F`

Fresh results against that exact binary:

- native codec: 29/29 at O0 and O2;
- installed VST3 state: 322/322;
- installed runtime: 1143/1143;
- Steinberg validator: 47/47;
- broad DSP: 13/13 O0 PASS, 11/13 O2 PASS, 2/13 O2 BLOCKED;
- snare differential: 61/61 at O0 and O2, bit exact;
- runtime synthesis: 419/419 at O0 and O2;
- FIR functional, quality (38/38), and latency (214/214): PASS at O0/O2;
- original-C filter comparison: 80000 blocks, zero mismatches at O0/O2.

REAPER 7.80/x64 freshly restored 456/456 slots across two different instances,
retained reserved values 77 and 0, rendered exact pre/post samples, produced
finite nonzero audio, completed two transport cycles, and reported 26 samples
at 48 kHz. The independent PDC run measured a 26-frame shift with zero alignment
error and zero aligned-waveform RMS error. Evidence:

- `out/reaper-p12c/p12c_results_cert_45750aa4.json`;
- `out/reaper-p12c/p12c_report_cert_45750aa4.md`;
- `out/reaper-p12c/p12c_state_roundtrip_cert_45750aa4.rpp`;
- `out/reaper-p12b4c/p12c_cert_45750aa4_48000_results.json`;
- `out/reaper-p12b4c/p12c_cert_45750aa4_48000_report.md`.

The older final reports and binary hash remain prior evidence and were not
overwritten. The earlier REAPER PIDs 22188 and 27720 were absent at audit start.
Fresh isolated test instances PIDs 24868 and 18056 remain open; no process was
terminated.

### Certification result

P12c functional behaviour, codec validation, build, installed-host behaviour,
REAPER persistence, and PDC pass. Strict acceptance remains **BLOCKED** for two
independent reasons: the two O2 Windows Code Integrity denials and the unresolved
state/automation/MIDI overlap conditions above. P12c is not DONE. Do not start
sequencer, kit-file, preset-browser, or UI work from this result.
