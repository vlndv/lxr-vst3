# P12c-R2 MIDI controller synchronization repair

Date: 2026-10-30. Final classification: **PASS**. P16, morphing,
preset-browser and graphical UI work are
outside this task.

## Baseline and preserved evidence

Branch `p12b-4c-fir-resampler`, HEAD
`280279debe94cb26810ea89b89f36b9bf965a747`, matched the requested baseline.
The working tree already contained ten tracked modifications and extensive
untracked implementation, framework, tests and REAPER tools. The initial status,
tracked diff, binary hashes, original source copies and subsequent logs are in
`out/p12c-r2-20261030/`. No staging, commit, push, branch switch, merge, reset,
clean, generated-evidence deletion, or firmware reference edit was performed.
Initial `git diff --check` passed, with existing line-ending conversion warnings.

The original linker, bundle and installed R1 binaries were 727552 bytes with
SHA-256 `B170E00911A4031B04616C0B75E2B2A7CE5902E5CF46F22A23F1D2DD93A6F953`.
The baseline DLL is additionally preserved as `LXR-R1-baseline.vst3` in the task
output directory. Historical R1 reports and `verification.json` remain intact.

## Exact reproduction and callback sequence

The original fixture is
`out/p12c-r1-final-20261029/controller-readback.cpp`. Its executable was run
unchanged against the installed R1 DLL before editing, yielding **34 PASS / 2
FAIL**, exit 1 (`original-controller-readback.log`). The two assertions are
`MIDI CC controller agrees` and `MIDI NRPN controller agrees`.

The test creates the component, initializes it with `HostApplication`, queries
`IAudioProcessor`, `IEditController` and `IMidiMapping` on that component, and
attaches a valid `IComponentHandler`. It sets native slot 88 to 100 and slot 128
to 0, sets up 48 kHz processing and activates the component and processor. Each
message below is delivered in its own VST3 processing call using the parameter
ID returned by `IMidiMapping::getMidiControllerAssignment(0,0,cc,tag)`:

| MIDI bytes (hex), channel 1 | Mapped VST3 MIDI ID | Native destination | Intended |
|---|---:|---:|---:|
| `B0 58 4D` (CC88=77) | 65626 | 88, D1 volume | 77 |
| `B0 63 00` (CC99=0) | 65637 | NRPN MSB selector | 0 |
| `B0 62 01` (CC98=1) | 65636 | NRPN LSB selector | 1 |
| `B0 06 2D` (CC6=45) | 65544 | 128, D1 filter drive, NRPN1 | 45 |

After 500 ms of actual Windows timer/message dispatch, LXRS v1 decodes to 77
and 45, but `IEditController::getParamNormalized(88/128)` returns
`100/127 = 0.78740157480314965` and `0`. The recording handler confirms this
callback order: `beginEdit(88)`, `performEdit(88,77/127)`, `endEdit(88)`, then
`beginEdit(128)`, `performEdit(128,45/127)`, `endEdit(128)`. It does not echo
`performEdit` back to the controller. The extended baseline callback trace is
`original-callback-sequence.log`; its intentionally failing checks are retained.

The harness lifecycle is valid. The installed adapter is a non-distributed
`SingleComponentEffect`; there is no separate controller class to instantiate
or processor/controller messaging connection to establish. The new verifier
checks canonical `FUnknown` identity through both interfaces. R1's independent
REAPER run also observed 100/0, so the failure is not missing host echo or idle
dispatch in the verifier.

## Confirmed causal chain

1. `IPlugVST3ProcessorBase::ProcessParameterChanges()`
   (`IPlugVST3_ProcessorBase.cpp:274`) translates synthetic MIDI parameter IDs
   into `IMidiMsg`, then calls `ProcessMidiMsg()` on the processing thread.
2. `LXR::ProcessMidiMsg()` applies pending authority state and replays deferred
   events. `ProcessMidiMsgNow()` obtains the bounded authority writer before
   passing CC/NRPN to `MidiParser::processEvent()`.
3. `MidiParser` selects the native ID. `ParameterArray::set()` stores its raw
   byte and immediately applies the existing Engine mapping. Slot 88 sets D1
   volume to `value/127.f`; slot 128 calls `filter.setDrive(value)`.
4. LXR publishes changed bytes to `ParameterStateAuthority<228>`, sets atomic
   processor `IParam`, then commits. `SerializeState()` snapshots this authority,
   explaining the correct serialized 77/45.
5. R1 called `SendParameterValueFromAPI()` on audio. That method only pushes an
   API parameter tuple. The installed `IPlugAPIBase::OnTimer()` VST3 branch does
   not drain that parameter queue (its parameter drain is in the non-VST3
   `#else` branch). It cannot synchronize this controller.
6. R1 also queued a generation for `LXR::OnIdle()`. Idle did run and sent the
   two gestures through `IPlugAPIBase::SetParameterValue()`
   (`IPlugAPIBase.cpp:117`). This sets `IParam`, calls `InformHostOfParamChange`
   and `OnParamChange`; it never updates the VST3 ParameterContainer.
7. `IPlugVST3ControllerBase::GetParamNormalized()`
   (`IPlugVST3_ControllerBase.h:194`) reads that separate container, whose cached
   `valueNormalized` remains 100/127 and 0. Neither `performEdit` nor an `IParam`
   write implicitly changes it. There is no evidence that host automation, bad
   normalization, wrong IDs, missing timers or dropped MIDI caused these two
   failures.
8. Restore works through a different route: `IPlugVST3State::SetState()` calls
   `UpdateParams()` (`IPlugVST3_ControllerBase.h:336`) after a valid decode. That
   copies `IParam` values into the controller container. The existing malformed
   state patch in `IPlugVST3_Common.h` is unchanged (SHA-256
   `4AD9986250B87869160A5D49F0832FCD140F134026A8E99BF0EB00AD575A654C`).

## Repair and explicit convergence contract

The repair is entirely in LXR's plugin class. No new vendor modification,
parameter ID, state format or synthesis mapping is introduced.

### Host MIDI precision boundary

The initial controller-only D7C2E76A build exposed a second issue during actual
REAPER NRPN certification. A task-only JSFX passthrough recorder confirmed
`B0 63 00`, `B0 62 01`, `B0 06 2D` before LXR. A read-only inspection of the
loaded component (`reaper-midi-trace5/parser.json`, using the exact DLL's class
layout) observed the NRPN LSB remain zero while the completion flag became
true, then clear on data entry. No process memory was modified. This locates
the selector corruption before native parameter mapping, independently of the
controller-cache problem.

`IMidiMsg::MakeControlChangeMsg()` (`IPlugMidi.h:186`) truncates `value*127` to
an integer. A host normalized value represented at float precision for wire
value 1 becomes `0.0078740157186985016`, whose product with 127 is
`0.9999999962747097`: truncation produces selector 0. The independent VST3
fixture reproduces this using a double containing a float-rounded normalized
value. Before this repair it fails both the NRPN1=45 assertion and an exhaustive
128-wire-value assertion (`host-float-repro.log`, 953 pass/2 fail). The exact
double above is measured in that independent reproduction; it was not captured
directly from REAPER's ProcessData. The REAPER selector observation and the
independent reproduction locate the practical host-delivery failure.

LXR now overrides the existing VST3 `process()` method with callback-scoped,
non-owning `IParameterChanges`/`IParamValueQueue` views. For configured synthetic
MIDI CC tags only, `getPoint()` rounds the clamped normalized value to the nearest
7-bit wire value and returns its canonical `wire/127.0` normalization. Native
parameter queues, queue count/order, point count/offsets, aftertouch and pitch
bend are forwarded unchanged. Non-finite MIDI values are rejected. The installed
framework reads one queue synchronously at a time; the two stack views borrow
the host interfaces only for that call, and are neither retained nor transferred.
They add no queue storage, allocation, ownership transfer, callback, lock or
event coalescing. This uses the installed VST3 input interface without changing
shared iPlug2 code or the native parser. The all-128-value and NRPN tests then
pass; this precision correction is part of the final certification build.

### Controller projection

The existing authority's per-slot value generation is the durable pending
record. Main-thread `mControllerSyncGenerations[228]` records only acknowledged
generations. `OnIdle()` tries the existing writer gate once, examines those
generations, and calls the supported protected
`SetVST3ParamNormalized(id, IParam::ToNormalized(authorityValue))` only for changed
slots. It cancels the unmodified writer transaction afterwards: controller
projection does not create another authoritative generation.

The gate excludes a newer writer between generation selection and controller
cache mutation. Busy audio producers take their existing mailbox/FIFO path;
they never wait. Idle does not call `SetParameterValue`, modify `IParam`, replay
an old MIDI value or call `OnParamChange`. A newer automation/restore generation
is projected instead of the obsolete MIDI generation.

After releasing the gate, idle holds a temporary SDK `IPtr<IComponentHandler>`
and calls `restartComponent(kParamValuesChanged)`. The installed SDK contract
requires this main-thread callback and tells the host to invalidate cached
parameter values and query the controller. This creates **no**
`beginEdit`/`performEdit`/`endEdit` automation gestures.

Acknowledgements advance only after successful cache projection and a successful
host return on the same connection epoch. No handler, a rejected callback or an
unavailable writer leaves pending generations observable. A newer publication
during the host callback differs from the captured delivered generation and is
handled on the next idle. A main-thread reentry guard prevents recursive idle
notification. `setComponentHandler()` increments the connection epoch and requests
one complete reconciliation on attach/detach; reconnection refreshes all 228
slots once, then resumes generation-driven incremental updates. Periodic idle
scans compare generations; they do not blindly overwrite all controller values.

Boundary contract: after accepted MIDI is processed/replayed and a successful
main-thread idle projection/host notification, authority, ParameterArray,
Engine effect, processor IParam, VST3 controller and serialized native bytes
agree. Host automation and restore publish immediately to authority/IParam but
apply ParameterArray/Engine at the next processing boundary; their complete
convergence includes that boundary. If a writer or host callback overlaps, the
next uncontended processing/idle boundary completes delivery. There is no claim
of immediate cross-thread controller mutation inside the audio callback.

All descriptors retain `InitInt(min=0,max=127,linear)` and step count 127.
Normalization is `native/127`; conversion back constrains/rounds through the
installed `IParam` integer shape. Production uses `ToNormalized` rather than a
duplicated scaling formula. The verifier independently expects `77/127 =
0.60629921259842523` and `45/127 = 0.3543307086614173`, and checks both VST3
plain/normalized conversion methods.

## Ownership, capacity and thread safety

- Processing exclusively owns Engine, ParameterArray, parser and the 4096-entry
  deferred MIDI FIFO. Authority payload/generations and existing host mailboxes
  remain atomic; writer acquisition and snapshots stay bounded.
- Main thread exclusively owns controller-cache access, acknowledgement array,
  connection epoch and reentry flag. Host callback occurs after gate release.
- Pending capacity is one latest authoritative generation per each of 228 slots;
  notifications may coalesce, but accepted MIDI processing events still execute
  in FIFO order. There is no new notification queue to overflow.
- The original FIFO rejects the newest event and increments its atomic counter
  on exhaustion. No previous accepted event is discarded or overwritten.
- Audio removes the unused `SendParameterValueFromAPI` enqueue and obsolete
  MIDI notification-mailbox stores. It adds no heap allocation, mutex, file I/O,
  logging, controller access or host callback. Existing native tests count zero
  render/authority allocations; source review covers the changed DLL path.
  This is not a claim of whole-DLL heap instrumentation or a TSan run.
- A friend declaration adds test access without an exported runtime API, virtual
  function, object-layout change or production test branch. The optional probe
  uses the exact Release class headers and a static cast through the verified
  single-component inheritance; it inspects native projections and forces gate
  contention in the actual loaded binary.

## Test setup correction discovered during regression

The unchanged historical O0 snare executable initially failed 8 differential
checks (53/61 pass). This is retained in `P12b4cSnareRenderTest_O0.log` and
`snare-O0-retry.log`. Its SHA-256 was unchanged from R1. The standalone fixture
declared `SnareVoice port;`; `ResonantFilter::init()` does not initialize previous
input `zi`, while the original-C harness explicitly `memset`s firmware-global
storage before `Snare_init()`. Stack contents therefore make that comparison
unreliable. The fixture now value-initializes its three standalone voices with
`{}` to match the C harness's initialization. Assertions, oracle, strict bit
equality and synthesis code are unchanged. Fresh fixture builds pass 61/61 at
both O0 and O2 with zero render allocations. This is a test setup repair, not
a snare/filter algorithm change; the historical failed run is not erased.

The first optional native-probe attempt also tested host automation and restore
before their documented processing boundary. Its two native-only failures are
retained in `controller-sync-probe-built.log`. The fixture now supplies a real
empty process boundary for those two cases. The external controller assertions
and the original R1 mismatch assertions are unchanged.

## Final certification results

The final linker output, built bundle and installed executable are all
**729600 bytes**, SHA-256:

`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`

VS Community 18 / MSBuild **18.10.1** / Release x64 / **v145** compilation and
linking succeeded (`msbuild-float-compiled.log`); the normal final build and
seven-resource package/install succeeded (`msbuild-acceptance.log`, exit 0).
Only LXR required recompilation; the framework was not rebuilt unnecessarily.
All seven source/built/installed asset hashes match. All 61 DSP files match the
R1 input manifest; no oscillator, snare, filter, FIR or native mapping change is
hidden in the shell repair.

Final exact-installed-binary results are in `acceptance-checks/`:

| Suite | O0 | O2 / installed result |
|---|---|---|
| R1 concurrency / coherent snapshot / writer and FIFO ordering | PASS 35/35 | PASS 35/35 |
| Native LXRS codec / legacy / nonmutation / determinism | PASS 29/29 | PASS 29/29 |
| Original-C snare differential, corrected initialization fixture | PASS 61/61 | PASS 61/61 |
| Native runtime synthesis / routing / zero velocity / allocations | PASS 419/419 | PASS 419/419 |
| Original-C filter differential | PASS 80000 blocks, zero mismatches | PASS 80000 blocks, zero mismatches |
| FIR functional | PASS | PASS |
| FIR quality | PASS 38/38 | PASS 38/38 |
| FIR latency | PASS 220/220 | PASS 220/220 |
| MIDI parser | PASS 5/5 | PASS 5/5 |
| Mixer | PASS 28/28 | PASS 28/28 |
| ResonantFilter | PASS, fail=0/warn=0 | PASS, fail=0/warn=0 |
| TransientGen | PASS 17/17 | PASS 17/17 |
| Installed VST3 state | n/a | PASS 322/322 |
| Installed VST3 runtime | n/a | PASS 1101/1101 |
| Unchanged original controller mismatch verifier | n/a | PASS **36/36** |
| New external controller verifier | n/a | PASS **955/955** |
| Same verifier with native/Engine/FIFO probe | n/a | PASS **1033/1033** |
| Steinberg validator | n/a | PASS **47/47** |

All **30 executable suite runs PASS**, zero FAIL, BLOCKED or NOT RUN mandatory
suites. The exact two original failing assertions are retained and pass.
`focused-verification.json` records intended/actual raw and normalized values,
IDs, actual MIDI bytes/proxy IDs, all assertion counts and binary identity.
`verification.json` independently checks final results, resource identities,
all 228-byte LXRS payloads/CRCs in saved RPPs, decoded WAV samples, PDC lag and
unchanged DSP hashes. The earlier controller-only D7C2E76A results and failure
logs remain historical and are not cited as proof of this final binary.

New coverage includes CC IDs 88-93, NRPN IDs 128-133, all 128 float-precision
wire values, before/after idle, repeated/coalesced and distinct edits, later
automation/restore supersession, host acknowledgement rejection/retry, a newer
MIDI update during acknowledgement, disconnection/reconnection and all-slot
reconciliation, pending destruction, real process/save/restore/UI overlap,
forced deferred CC/NRPN replay across restore, 4096 accepted FIFO events plus
one rejected newest event, invalid and legacy restore followed by MIDI, two
independent instances and absence of repeated notification/automation gestures.
The probe checks authority, ParameterArray, processor IParam and original
Engine volume/drive effects in addition to genuine IEditController readback.

### REAPER 7.80 / exact final installed binary

Fresh task PIDs **33316** (state/MIDI) and **34172** (PDC) both loaded the canonical
installed path and exact hash above, independently recorded in
`acceptance-module-identities.json`. `acceptance-state/` and `acceptance-pdc/`
contain the final scripts, launch identities, projects, reports and WAVs.
An earlier private-package run on the same hash also passed; final acceptance
uses the canonical installed path, not that earlier run.

- Two distinct instances: **456/456** intended values after recreation/reopen,
  reserved slots 77/0, zero mismatches. Native LXRS bytes independently match
  both full expected arrays.
- MIDI CC88=77 and NRPN1=45: live readback **77/45**, save while processing,
  reopen, **456/456** MIDI checkpoint slots agree across both instances. The
  saved RPP's two CRC-validated LXRS payloads independently match those arrays.
- REAPER's formatted FX parameter-value API reports **"77" / "45"** after
  reopen. Standalone checks directly exercise IEditController; the formatted
  value API is not presented as a screenshot of a refreshed GUI window.
- Deterministic tonal audio: **67200 stereo frames at 48 kHz**, sample-for-sample
  exact before/after reload, maximum and RMS error **0**. A peak
  `0.00064003467559814453`; B separately finite/nonzero. RNG runtime-state
  serialization is not claimed.
- Two advancing transport start/stop cycles: both positions
  **0.4266666666666666 s**, playing=1, stopped=0.
- FIR report **26 samples** at 48 kHz; independently measured host PDC shift
  **26**, aligned onset error **0**, aligned waveform RMS error **0**, including
  a fresh reopen/re-render. Audio header rates are independently checked.
- No automation envelope was generated in the MIDI roundtrip project; the
  recording handler reports zero begin/perform/end gestures.

The initial REAPER fixture failures are preserved. Task MIDI tracing established
that stopping sends additional `CC6=0`, `CC88=0`, `CC98=0` messages. These are new
accepted inputs, not stale notifications or a failure to retain the earlier
patch. The final fixture uses a fresh long MIDI item, sends the sequence once
through live transport and saves after 77/45 converges but **before** subsequent
stop/reset messages. It records the stop values separately, then reopens the
77/45 save and checks every slot. This is the explicit save boundary; the
plugin does not ignore, filter or special-case REAPER resets. No assertion in
the original 36-assertion verifier was removed or weakened. REAPER's documented
[playback reset settings](https://dlz.reaper.fm/userguide/ReaperUserGuide740c.pdf)
give context; the actual reset bytes are established by task traces, not assumed
from that documentation. A task-local `midisendflags` experiment did not prevent
the observed resets and is not used as acceptance evidence.

The historical three REAPER processes were checked by PID, executable path,
command line and creation time before further launches. All declined normal
closure. User-assisted closure removed PID 34036, but 13544 and 16836 remained
at the next inventory and the installed R1 DLL remained locked. Force
termination was requested explicitly, and the user authorized **only 13544 and
16836**. Both identities were rechecked and only those two were terminated.
The subsequent controller-only install and host investigation are retained.
Six later diagnostic task processes also remained after `CloseMainWindow`; a second
force-termination request became unnecessary. The documented
[Main_SaveProjectEx options](https://www.reaper.fm/sdk/reascript/reascripthelp.html#Main_SaveProjectEx)
provided a normal-exit route: create a unique additional saved project with
option 8, verify a clean project, then issue REAPER's normal quit action. That
closed all eight remaining diagnostic/private-package instances without further
force termination or discarded dialogs. `graceful-exit.log` preserves each
additional saved project path. The same safe route is used after final evidence
capture. No unrelated project or process was manipulated.
The final process inventory records **zero remaining REAPER processes**.

R2-only changes are the two LXR class files, the new controller verifier, the
three-voice initialization fix in the existing snare fixture, this report and
the four requested documentation updates. `R2-only.diff` and
`R2-change-inventory.json` separate these from the pre-existing working tree;
Git's overall diff/stat still includes the preserved earlier work. Final
`git diff --check` passes. Branch/HEAD are unchanged; nothing was staged,
committed or pushed.

No Windows security policy was changed. Read-only DeviceGuard reports kernel
and user-mode enforcement status 2. Both formerly blocked exact O2 filter and
transient executables run normally and pass in this task. Computer-use window
inspection encountered an app-approval timeout; it did not act on a dialog.

Remaining qualifications: TSan, whole-DLL heap instrumentation and literal FX
window inspection are **NOT RUN** optional checks. Native measured allocation
checks and changed-processing-path source audit pass; formatted FX value API
and genuine IEditController checks pass. The installed framework's existing
queue-point/timing policy is unchanged; the repair adds no MIDI event loss or
new scheduling semantics. No hardware RNG runtime-state serialization; exact
project audio uses the established deterministic tonal path. User SD samples
and later feature phases remain outside scope. The existing project/framework
compiler configuration was retained; R2 adds no throws or RTTI use.

P12c-R2 is **PASS** under this task's acceptance criteria. This closes the two
controller assertions and the host MIDI precision finding. P12 remains the
larger active phase; no new DSP interface header is introduced by this private
plugin-shell repair. The next milestone can be reviewed separately. P16,
morphing, preset browser and UI development have not begun.
