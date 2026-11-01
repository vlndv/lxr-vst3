# P12c-R1 state concurrency hardening

P12c-R2 follow-up (**PASS**, 2026-10-30): the controller projection and float-precision MIDI proxy
decoding repairs are documented in
[p12c_r2_controller_sync_repair.md](p12c_r2_controller_sync_repair.md). Its final
results supersede the historical certification status below. The R1 authority,
bounded snapshots, host mailboxes and deferred FIFO remain the concurrency
contract; controller acknowledgements now use durable per-slot authority
generations instead of the former exchanged MIDI notification mailboxes.

Date: 2026-10-26  
Branch: `p12b-4c-fir-resampler`  
Starting HEAD: `280279debe94cb26810ea89b89f36b9bf965a747`  
Latest certification (2026-10-29): **FAIL — MIDI VST3 controller readback diverges from serialized authority**.

The implementation evidence below is the historical 2026-10-26 record. The
[final-hash certification](p12c_r1_final_certification.md) supersedes its remaining
certification status: the exact B170E009...93A6F953 binary now passes fresh REAPER
456-slot persistence, exact tonal audio, two advancing transport cycles, and
48-kHz/26-sample PDC before and after reload. Both unchanged formerly denied O2
executables now execute normally under enforced Code Integrity.

A new installed-DLL verifier fails 2 controller-agreement assertions (34 pass):
CC88 and NRPN1 serialize 77/45, but VST3 controller readback remains 100/0 after
two idle edit notifications. REAPER exposes the same old values. The existing
MIDI serialization assertions did not cover this separate controller cache.
No production repair was made; the smallest proposed LXR-local controller
synchronization repair and complete reproduction are in the final report.
P12c-R1 is not DONE and P16 remains unauthorized by the strict acceptance gate.

P12c-R1 repairs the two state-concurrency findings from the P12c certification
audit. It does not change synthesis, FIR, MIDI mapping, state bytes, sequencer,
presets, morphing, UI, buses, or framework state handling.

## Original defects and deterministic reproductions

The former `SerializeState()` loaded 228 independent atomic `IParam` values.
There was no generation around those loads. The new concurrency test forces a
batch update after the first 114 old-style loads and before the remaining 114.
The resulting array is neither the old nor new correlated pattern. This is a
deterministic reproduction of Defect A, not a probabilistic stress result.

The former `ProcessMidiMsg()` called the deferred restore check, but continued
when that check returned because the restore sequence was odd. The reproduction
sets the restored `IParam` value, pauses the restore, applies MIDI to `IParam`
and native state, then completes and applies the restore candidate only to
native state. Host value 77 and native value 40 diverge. This forces the exact
Defect B ordering.

## Callback and thread audit

Installed iPlug2/VST3 call paths establish the following:

- `IPlugVST3::getState()` calls `IPlugVST3State::GetState()` and then the plugin
  `SerializeState()`. `setState()` follows the corresponding `SetState()` path.
  VST3 specifies component state callbacks on a host/UI-controlled thread, but
  permits them while processing is active. They are not assumed serialized
  with `process()`.
- `IPlugVST3ProcessorBase::Process()` executes parameter changes, MIDI input,
  and audio, in that order on the host processing thread.
- processing parameter points set atomic `IParam`, then call `OnParamChange()`;
  iPlug2 explicitly warns that this callback may be realtime.
- the non-distributed VST3 controller `setParamNormalized()` sets `IParam` and
  calls only `OnParamChangeUI()`. LXR now routes that producer into the same
  authority. iPlug2 documents this callback as low-priority/UI-thread.
- `ProcessMidiMsg()` is documented and implemented before `ProcessBlock()` on
  the processing thread. The deferred FIFO relies only on that installed VST3
  contract; it is not an MPMC container.
- `OnIdle()` is invoked by the API timer on the main thread.
- `OnReset()` is called from `setupProcessing()` and from
  `setProcessing(false)`. Its context is host-controlled; the code does not
  assume it can race safely with active audio.

Host implementations can use different UI/state threads. No correctness claim
depends on `SerializeState()`, `UnserializeState()`, `OnParamChange()`, and
`OnParamChangeUI()` being mutually serialized.

## Authoritative parameter-state model

`ParameterStateAuthority<228>` is the single logical LXRS parameter state.
Every slot is a bounded atomic byte. Slot 0 remains serialized and host-visible;
IDs 1-227 keep their established meanings.

Data flow is now:

1. host processing automation sets `IParam`; `OnParamChange()` publishes that
   one value as one authority generation;
2. controller/UI changes set `IParam`; `OnParamChangeUI()` routes them through
   the same publisher;
3. validated state restoration publishes all 228 values as one authority
   generation and mirrors the candidate to `IParam` before publication;
4. the processing boundary takes one coherent authority snapshot and projects
   it into `ParameterArray` and Engine in ascending ID order;
5. MIDI CC/NRPN takes the authority writer before changing `ParameterArray`,
   Engine, `IParam`, and the authoritative byte, then commits one generation;
6. serialization encodes one coherent authority snapshot, never an `IParam`
   scan;
7. `OnIdle()` sends a MIDI-originated host gesture only if that parameter's
   recorded generation is still its latest authoritative generation.

`IParam`, `ParameterArray`, and Engine are projections, not competing stores.
At a completed processing boundary they match the authority. Note events do not
change parameter state and preserve their existing seven-channel routing and
velocity behavior.

## Publication, ownership, and memory ordering

Writers attempt one `atomic_flag::test_and_set(acquire)`. The flag is guaranteed
lock-free by C++; the x64 target additionally requires lock-free 64-bit atomics
at compile time. A writer publishes an odd sequence with release semantics,
writes atomic payload bytes and per-slot generations with relaxed operations,
then publishes the next even generation with release semantics before clearing
the writer flag.

Readers make at most 16 attempts by default (serialization uses 32). Each reads
the sequence with acquire semantics, copies only atomic bytes, and reads the
sequence again with acquire semantics. Equality of the two even values defines
the snapshot linearization point. An overlap returns failure; there is no
unbounded spin. Atomic payload storage means the rejected attempt is still free
of an ISO C++ data race.

The writer gate excludes multiple host-side writers. A second restore returns
decode/publication failure without mutation if the gate is unavailable. An
audio-thread automation callback never waits: it records the latest 7-bit value
in a generation-ticketed per-parameter mailbox. The processing boundary drains
mailboxes as one documented batch generation. A later complete restore clears
only tickets ordered before its restore boundary; tickets claimed afterward
remain and apply after the restore.

The 64-bit generation wraps only after 2^63 committed publications. Equality
is required only across one bounded snapshot attempt. Practical wraparound is
not reachable in the product lifetime; the test exercises 10,000 successive
commits and even/stable invariants rather than adding a production test hook.

## Restore and MIDI ordering

The policy is:

- MIDI that acquires the writer before a restore is applied first; a later
  restore may supersede its parameter value.
- a restore that acquires the writer validates and publishes one complete
  patch. No partial candidate is visible.
- MIDI that encounters an in-progress or not-yet-applied restore is appended to
  a preallocated 4,096-event FIFO and replayed in arrival order after the
  complete restore at the next processing boundary.
- a CC/NRPN event takes the writer before touching host or native parameter
  state. It therefore cannot update one representation and be omitted from the
  other.
- rapid restores coalesce at the engine projection to the latest complete
  authoritative snapshot; every externally accepted restore remains a complete
  generation.

The FIFO has no allocation, lock, I/O, logging, or wait. If all 4,096 entries
are occupied, the newest event is rejected without changing host, authority, or
native state, and an atomic overflow counter is incremented. The production
FIFO itself is tested for capacity, rejection, and preservation of accepted
order. This is a bounded fail-closed policy; the VST3 MIDI callback has no
backpressure return channel. Queue exhaustion remains a low-probability risk
and is listed below rather than hidden.

## Host notification ordering

MIDI records the exact committed generation for each changed parameter.
`OnIdle()` atomically claims that notification and checks both the authority's
latest per-slot generation and the current `IParam` value. A later restore or
automation update makes the old notification stale and it is skipped. The
existing host gesture path is retained, but an obsolete notification cannot
resurrect an earlier patch value.

## Serialization compatibility and validation

LXRS v1 is unchanged: `LXRS`, version 1, 24-byte little-endian header, 228-byte
payload, 252-byte plugin state, four-byte VST3 bypass suffix, CRC-32, IDs 0-227,
and exact legacy 228-double migration. Identical authority values produce
identical bytes. Decode still validates the complete payload before attempting
publication. Empty, truncated, oversized, unsupported, corrupt, non-7-bit, and
unexpected trailing states leave the previous complete configuration intact.

No additional iPlug2 framework change was needed. The existing narrow
`IPlugVST3_Common.h` error-propagation patch remains separate.

## Focused test results

`tests/P12cR1ConcurrencyTest.cpp` uses correlated patterns, forced writer
barriers, accepted-operation counts, and an independent ordering oracle.

- O0: 35/35, exit 0.
- O2: 35/35, exit 0.
- deterministic old Defect A and Defect B reproductions: PASS;
- forced half-publication rejects the snapshot: PASS;
- three simultaneous publishers plus serialization: no mixed pattern;
- bounded snapshot/publication failure paths: PASS;
- concurrent single-parameter automation: PASS;
- restore followed by CC/NRPN, MIDI followed by restore, and rapid restores:
  submitted/accepted order and final host/native/serialized state agree;
- full production FIFO rejects only the newest event and preserves accepted
  FIFO order;
- invalid candidate, CRC, truncation, version, and value leave state intact;
- identical logical state gives identical LXRS bytes and exact round trip;
- 10,000 measured authority update/snapshot iterations: zero counted C++
  allocations.

ThreadSanitizer was not executed: no compatible TSan test platform is available
in this Windows session. No TSan PASS is claimed.

## Regression, build, and host results

- native P12c codec: 29/29 at O0 and O2;
- installed VST3 state: 322/322, 256 bytes including bypass, exact restored PCM,
  legacy/corrupt/MIDI/multi-instance cases pass;
- installed VST3 runtime executable available in this checkout: 1,101/1,101;
- snare original-C differential: 61/61 at O0 and O2;
- native runtime synthesis: 419/419 at O0 and O2;
- FIR functional: PASS O0/O2; quality 38/38 O0/O2; latency 220/220 O0/O2;
- filter original-C differential: 80,000 blocks, zero mismatches, O0/O2;
- broad DSP: all 13 O0 pass; 11/13 O2 pass; two O2 launches blocked;
- MSBuild 18.10.1, Release x64, v145: compile/link/package/install and all seven
  resources pass. The first command-line attempts lacked `SolutionDir` and then
  the v145 override; the established command succeeded with zero errors;
- built and installed DLL SHA-256 match:
  `B170E00911A4031B04616C0B75E2B2A7CE5902E5CF46F22A23F1D2DD93A6F953`;
- Steinberg validator: 47 passed, 0 failed, exit 0.

The fresh isolated REAPER 7.80 run could not be completed. A new test instance
was launched against the pre-final R1 build only after confirming that no
REAPER process existed. Native UI automation timed out while importing the
unique P12c-R1 wrapper script; no test result file was created, no unrelated
process was touched, and the task-created REAPER instance was not terminated.
The subsequently rebuilt final hash therefore has no fresh REAPER result.
Prior P12c certification evidence remains valid for the earlier certified
binary, but it is not claimed for the current hash.

## Windows Application Control

All 13 O0 broad executables ran. O2 `ResonantFilter_test_O2_p12c.exe` and
`TransientGen_test_O2_p12c.exe` remain blocked by the previously identified
Windows Code Integrity `VerifiedAndReputableDesktop` enterprise-signing policy.
The launch errors explicitly report Application Control. No policy, signature,
path, name, or security setting was changed; these are BLOCKED, not FAIL.

## Remaining risks and classification

- Fresh current-hash REAPER save/reopen, deterministic render, MIDI persistence,
  live-render overlap, and PDC are unverified because the isolated UI harness
  could not be started to completion.
- The two required O2 executables remain unavailable under enterprise policy.
- A 4,096-event restore-overlap burst has a defined fail-closed overflow but no
  host backpressure mechanism. No overflow occurred in host tests.
- TSan remains unexecuted.

Both source-level concurrency defects are repaired and focused/installed-host
tests pass. Strict P12c-R1 certification is nevertheless **BLOCKED**, not PASS,
because the mandatory current-hash REAPER run and two O2 policy-controlled
launches are unavailable. There is no verified code assertion failure.

P16 is **not ready to begin under the task's strict gate**. Complete the fresh
REAPER run on the current binary and execute the two O2 tests on a compliant,
policy-approved host before reclassifying P12c-R1 as PASS.
