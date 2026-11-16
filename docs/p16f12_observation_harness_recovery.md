# P16-F12 observation harness recovery — BLOCKED

P16-F12 does not certify or publish the combined P16-F implementation. The
observation failure is localized to acquisition of ephemeral scheduler batches.
The bounded evidence collector is repaired and tested, but lossless native/REAPER
acquisition remains unqualified. No production defect is independently established.
Production Import access, expanded CC execution and four historical PCM causes
remain blocked. No new instrument feature, production edit or VST3 rebuild occurs.

The original workspace has 454 source files with fingerprint
`3BDBF26421CB00A3A0C4140372EC1C879738B2D7A7876B4B8F1ED2E0FBF51283`,
exactly matching F11. Branch `p12b-4c-fir-resampler`, origin
`https://github.com/vlndv/lxr-vst3.git`, local/tracking/live HEAD
`7749ef1a0203f5ab92bf0e2f447b53bb95dadf68` and empty index verify.
Firmware, iPlug2, SDK, production build inputs, seven resources and 76 frozen
reference pairs verify. The final integrity report records evidence rehashes and
the separate documentation/harness-adjusted source fingerprint.

## Observer diagnosis and repair boundary

F11 used `out/p16f8/resume-001/P16F8CompleteHostObserver.cpp`, executable
`08BA2055A61A95FC669782BEBDEAFAD2F98F592C8E3B88AB82FF969A74D5598F`.
Its assertion is `accepted > 100 && events > 10`. It watches for 65 seconds,
opens one output stream and appends records. The ReaScript waits for its ready
flag before play. It does not exit on the first event. A unique PID/module/timer
binding/vtable identifies the actual instance.

`TriggerScheduler::processNativeBlock` clears `mLastBatch` at every native block,
including blocks with no event. It exposes the latest block, not history. An
external double-snapshot poll can miss the complete lifetime of a batch. The
observer further restricts reads by resampler-position/callback modulo, deduplicates
callback context before inspecting events, and requires the event to belong to
the latest native block. These rules cannot constitute a lossless event stream.
Whole-object `memcmp` also includes padding; a fieldwise alternative was prepared,
but its executable was policy-denied and its benefit is not claimed.

The original F11 Euclidean log records one event despite 44 emitted triggers.
A fresh task-owned F12 reproduction records 2,261 contexts and six of 44 events,
retaining the original **31 PASS / 1 FAIL** coverage assertion. Independent bank/
RNG/rotation/chain logic agrees with 2,259 state observations and all six captured
event/timing batches. The first unobserved expected trigger is track 0, pattern 0,
position 16, physical ordinal 8, expected native boundary 5,504. The ready handshake
and earlier snapshots precede it. F11's paired nonzero Euclidean PCM and canonical
restoration remain preserved supplementary evidence. Counts, expected sequence,
actual records and source-backed timestamps are in `out/p16f12/observer_diagnosis.json`.

Classification: **observer implementation/acquisition limitation**, not demonstrated
plugin event-generation failure. A counter records emissions after native dispatch;
it is supplementary and does not replace missing per-event timing evidence.

`tools/reaper/observation_capture.py` adds bounded append/deduplication, explicit
session/instance identity, lifecycle checks, cumulative-count reconciliation,
truncation/timeout reporting and independent exact-sequence comparison. Seven
synthetic tests pass, including repeated grouping, timestamps/order, stale sessions,
wrong instances, conflicting records, counter resets and refusing a one-event PASS.
This validates collection of supplied records, not their acquisition from REAPER.

A native exact-DLL qualification fixture covering two runs at callback sizes
1/32/127/512 compiles normally with VS LLVM O2 but receives WinError 4551 before
entry. A sampled-reader prototype removing modulo and adding native/emission
deduplication runs, but its actual Euclidean capture still has one event and
fails coverage. An initial stopped-project run yields no eligible contexts and
is retained as an invalid qualification context. The fieldwise MSVC reader is
also denied. No full host acceptance follows these unqualified readers. A bounded
reliable event-retention facility would require a separately approved diagnostic
binary/path; the frozen release is not rebuilt merely to add instrumentation.

## REAPER and production Import

F12 Import PID 30920, main HWND 924142, private module/hash and task-owned project
are verified. Action 40042 moves the cursor from 0.125 to 0 and preserves selected
track/FX GUID. Read-only canonical applied-bank probes before/final both give
Bank A `C305CC5B716B50996A8629E5CBF6D30B050B70D7AB5D50BF5F5B73EED1ED33F3`.
Target B remains
`51CF442EE979DE803CD4BB8A0CC6A5581DD3E093C90ABC0BB3B558E74DEC8C5D`.
No production file selection or A-to-B transition occurs.

Session 1/WinSta0/Default identities match. Window ownership, plugin editor,
geometry, DPI and foreground metadata are recorded independently. They do not
prove capture/input routing. The sky surface reports null accessibility, then
app-access timeout; the second supported cua surface lists the exact F12 window
but times out obtaining app access. F12 makes no new verified screenshot-pixel
claim. Historical F11 terminal captures remain historical. Zero clicks/typing
are sent. The 600-second state watcher expires normally with one unchanged capture.

[Official ReaScript documentation](https://www.reaper.fm/sdk/reascript/reascripthelp.html)
describes `TrackFX_Show` and `Main_OnCommandEx`; the methods used are also proven
in the actual 7.80 runtime. The online documentation currently identifies 7.86,
so its version is not substituted for local runtime proof. Production Import is
an iPlug custom button/file-dialog callback (`LXR.cpp`), with no exposed host
parameter/action discovered that invokes it. Direct setters or chunk replacement
cannot substitute. All five production rejection cases and B PCM/imported
save/reopen remain NOT RUN, with GUI/app access as their precise blocker.

## Execution and historical PCM

Fresh ordinary original-path expanded CC execution under the existing VS18
environment is denied with WinError 4551, zero completed assertions. CodeIntegrity
3077 identifies VerifiedAndReputableDesktop policy
`{0283ac0f-fff1-49ae-ada1-8a933130cad6}`, status `0xc0e90002`.
Earlier 548,168/0 is retained separately. No policy, security service, certificate,
PE header, loader or executable relocation is changed.

The corrected preserved MSVC receipt replay executes freshly: **98/0**. It
reproduces callback PCM using one-frame replay of the actual callback-level control
receipt. It does not reproduce the historical 24-bit WAVs exactly after PDC26 and
fixed conversion experiments. Four raw FAILs are remeasured independently:

| Historical pair | First host frame/channel | Expected / actual | Classification |
|---|---:|---:|---|
| F7 selector 0 | 11785 / 0 | 0 / -1 | Unresolved |
| F4 selector 0 | 11785 / 0 | 0 / -1 | Unresolved |
| F4 selector 12 | 119988 / 1 | 11348 / 11347 | Unresolved |
| F4 OFF-route stress | 0 / 1 | -1738506 / -1738508 | Unresolved |

Selector-0 corresponding PCM is identical across F4 and F7, excluding the intervening
CC/zi changes as the explanation of that pair. The callback-receipt timing evidence
supports its onset difference, but residual conversion differences and absent
original callback/native startup history prevent exact causal reconciliation.
Selector-12 begins near release; its old 512-frame source cause is not proven.
OFF stress has only 32 differing scalar samples in frames 0–24, before external
MIDI. Its initial mismatch cannot originate in the later external note/CC delivery;
transient filter/FIR history remains unrecorded. No frozen reference is regenerated.
The ledger preserves original source/binary/fixture hashes, controllers, timing,
partition limitations and missing first-native samples explicitly.

## Preserved certification and stopping point

The missing 28-case first-event grid at 60/90/120/140 BPM is not run with an
unqualified observer. Earlier scoped loop/pattern witnesses and steady/map
observations remain distinct from a completed grid. Euclidean/copy-clear full
host gates stay BLOCKED; previously verified fixtures, canonical restoration and
paired PCM are preserved. No partial trace closes those requirements.

All production inputs remain frozen, including timestamped notes/CC, `zi=0.f`,
LXRS v3 canonical 51,065/max 65,536 and legacy readers. Prior exact-candidate
MIDI1660/0, PCM76/76, original-C filter80,000 blocks per O0/O2, fourteen-instance
restoration, zero measured processing allocations, 19,616-byte observed stack,
validator47/47, PDC26 and listening retain their original finite scopes.
No new processing-thread code runs; collector work is outside the audio process.

Private VST3 SHA-256 remains
`4591AB2EBFDD6FE738F4983B9FBBF292C4B4E06DA7E0F7528C1E9426E050BE1D`.
Protected installed SHA-256 remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`,
with all seven resources unchanged. Full gate/regression matrices, changed paths,
hashes, failed/denied attempts and required artifacts are under `out/p16f12/`.
No staging, commit, push, deployment, process termination or next milestone.
