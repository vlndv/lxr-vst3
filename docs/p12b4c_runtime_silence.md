# P12b-4c runtime silence repair — 2026-10-21

Current classification: **REAPER PDC PASS; phase-wide BLOCKED** (updated
2026-10-26). Startup audio, FIR latency reporting, and automated REAPER offline
PDC/render verification pass at 44.1/48/96 kHz. Some debug regressions remain
blocked by Device Guard, and the independent inherited snare render defect
remains visible. The 2026-10-21 investigation below is historical; the final
sections record the subsequent latency and REAPER work and supersede their
pending host-PDC findings.

## Starting repository state

- Branch: `p12b-4c-fir-resampler` (unchanged).
- HEAD: `280279debe94cb26810ea89b89f36b9bf965a747` (unchanged).
- Only tracked modification initially: `vendor/iPlug2/Examples/LXR/LXR.cpp`.
  Its existing MIDI status/channel repair was preserved verbatim.
- Both diagnostic sources and `LXRParamContract.h` already existed untracked.
  The iPlug2 dependency tree, backups, validator build and other artifacts were
  already untracked; they were not removed or reorganized.
- Full initial status (4615 lines), file inventory (15294 paths), HEAD, branch and
  tracked diff were recorded before source edits in
  `C:\Users\valen\AppData\Local\Temp\lxr-p12b4c-20261021-012518\`.
  The inventory uses `rg --files --hidden`, so Git-ignored paths are excluded;
  the full Git status includes all nonignored untracked files.
- No staging, commit, push, branch change or merge was performed.

## Confirmed root cause and controlled experiment

The native defaults and host descriptors supplied two independent zero-gain gates:

1. PAR 49/51/53/55/57/59 = 127 maps to an attack increment of zero.
   With no repeat selected, a newly triggered amplitude envelope enters A with
   value zero and stays there. This is the firmware algorithm, not a mapping bug.
2. PAR 108–113 and global PAR 114 = 0 map to zero sample-and-hold increments.
   A freshly initialized decimator holds its initial zero forever, even when the
   envelope and oscillator are active.

All following measurements use D1, note 60, velocity 110, 100 native blocks
(3200 frames), with original assets. ST2 peak and nonzero ST2 frames were zero.

| State | Attack step | D1/global decimation | Final envelope value / output gain | ST1 peak | Nonzero ST1 frames |
|---|---:|---|---|---:|---:|
| Original defaults | 0 | 0 / 0 | 0 / 0 | 0 | 0 |
| Only PAR 49 → 64 | 0.00492227077 | 0 / 0 | 0.492227077 / 0.999742091 | 0 | 0 |
| Only PAR 108,114 → 127 | 0 | 1 / 1 | 0 / 0 | 0 | 0 |
| PAR 49 → 64 and PAR 108,114 → 127 | 0.00492227077 | 1 / 1 | 0.492227077 / 0.999742091 | 5826 | 3198 |
| Corrected initial patch | 1 | 1 / 1 | 0.9512766 / 0.00485924818 | 1639 | 3097 |

D1 decay step was `0.000492155552`, volume `0.787402`, waveform 1,
routing 0, velocity `0.866141737`, and oscillator increment `25530448`.
The original trigger state/value was A/0; after 100 blocks it was still A/0.
The corrected patch finished in D. Independent tests also verify that setting
either the voice decimator or global decimator alone to zero freezes output.

Source of truth inspected: SonicPotions/LXR 0.37 at `dee4968`, specifically
`DSPAudio/SlopeEg2.c` (trigger, A calculation, time mapping), `DSPAudio/mixer.c`
(initial full-rate decimation and hold loop), and `MIDI/MidiParser.c`
(decimation parameter mapping). Reference attack vectors at 0/64/127 are
`1`, `0.00492227077`, `0`. The mapping and envelope state machine agree with
these sources and were not changed. No Catalyst source was used.

## Smallest production repair

`ParameterArray::init()` and `LXRParamContract.h` now use attack **0** and
decimation **127** for a new instance. These are explicit plugin initial-patch
choices, not a claim that a hardware startup kit has been extracted. The native
open-hi-hat decay PAR 61 also now matches the host's existing value of 64;
the previous even-index loop left it zero. All 228 initial values now agree.

No parameter migration or forced patch replacement was added. The constructor
sets descriptor defaults for new instances. Existing state restoration and
`ResetEngineState()` still reapply the actual current `IParam` values, including
attack 127 and decimation 0. The first installed-binary host test saved these
deliberately silent values, restored them into a fresh component, verified the
values, and obtained peak zero. A new default does not repair an old saved patch.

The existing wrapper already combines `StatusMsg() << 4` with `Channel()`.
`MidiParser` reads the channel from the status nibble. No additional MIDI
production edit was needed. Native regressions confirm channels 1–7 select only
D1/D2/D3/SN/CY/HH-closed/HH-open, including distinct HH decay selection.
This routing issue is separate from default D1 silence.

## Resource and processing path verification

Installed binary:
`C:\Users\valen\AppData\Local\Programs\Common\VST3\LXR.vst3\Contents\x86_64-win\LXR.vst3`.

`GetPluginAssetDirectory()` resolves the loaded DLL, then its sibling
`Contents\Resources\LXR`, independently of the working directory.
Installed assets all matched repository SHA-256 hashes:

| Asset | Bytes |
|---|---:|
| sine_table.bin | 8194 |
| saw_table.bin | 22528 |
| tri_table.bin | 22528 |
| rec_table.bin | 22528 |
| crash_sample.bin | 32768 |

MSBuild's packaging step also verified all five files in both the generated and
installed bundles. Successful installed-plugin rendering demonstrates that its
asset and FIR-ready gates passed. The path exercised was VST3 note event → iPlug2
MIDI → wrapper → MidiParser → Engine trigger → native 32-frame Engine/Mixer
output → LXRAudioAdapter → polyphase FIR → stereo host buffers.

iPlug2 `SetupProcessing()` calls `SetSampleRate(setup.sampleRate)` then
`OnReset()`, which configures the adapter using `GetSampleRate()`. The first
installed-binary test exercised successive host-rate resets on the same instance:

| Host Hz | Stereo peak |
|---:|---:|
| 44100 | 0.0500060019 |
| 48000 | 0.0500246621 |
| 96000 | 0.0500246621 |
| 32000 | 0.0500159337 |
| 192000 | 0.0500298966 |

Output was finite. This was an actual loaded VST3 using 64-bit host buffers,
not a mock wrapper and not a REAPER listening test.

## Validation results

- Existing `tools\run_tests.bat`: **13/13 test programs passed**. Counts reported
  where available: distortion/decimator 23; drum 11; envelopes 31; LFO/modulation
  18; MIDI 5; mixer 28; parameter array 33; snare/cymbal/HH 47; transient 17.
  Oscillator, envelope mapping, miscellaneous mapping and filter also report zero
  failures. Log: `out/P12b4c-repair-dsp.log`.
- Original-C filter differential at `-O0` and `-O2`: **80000 blocks, zero
  mismatches each**, including buffers and all state. Original source unchanged;
  minimal hardware header stubs were generated under ignored `out/`.
  Logs: `out/P12b4c-filter-diff-O0.txt` and `out/P12b4c-filter-diff-O2.txt`.
- Both existing native diagnostics rebuilt at `-O2`: **PASS**, ST1 peak 1639,
  ST2 peak 0; default diagnostic reports 3097 nonzero ST1 frames.
- New `P12b4cRuntimeSilenceTest`: **418 passed, 1 failed**, exit 1. Checks include
  historical silence, isolated overrides, all 228 defaults, all 128 attack/decay
  mapping values, independent endpoint/midpoint vectors, MIDI routing, ST2,
  initial FIR-unready silence without advancing the Engine, initial invalid-rate
  rejection, seven supported rates and reset readiness. `renderAllocations=0`
  counts global C++ `new`/`new[]` during native/adapter rendering. This is not a
  full CRT allocation or framework lock audit. Rendering code review found no
  logging/file I/O in ProcessBlock or its synthesis/adapter/FIR call path; asset
  I/O remains in initialization. No production instrumentation was added.
- The one failure is channel 4 audio: existing `SnareVoice::calcSyncBlock()` is a
  silence stub. The routing assertion passes. Existing voice tests check state
  and formulas and did not catch missing rendering. The failing assertion is
  retained; implementing the snare signal chain is outside this startup repair.
- FIR functional tests: **7/7** at both `-O0` and `-O2`.
- FIR quality tests: **38/38** at both levels. 19 kHz input at 32 kHz output:
  **−77.74 dB** alias rejection. This is not complete FIR certification.
- First installed VST3 host test revision: **252 passed, 0 failed**, exit 0,
  including the rate table and silent restored patch above. These results were
  observed in tool output. Its redirected log was subsequently truncated when
  launching the expanded revision failed; the measurements are recorded here.
- The expanded `P12b4cVST3RuntimeTest.cpp` compiles, but **Windows Application
  Control blocked execution**. Extra D1-muted channel isolation, all-228-value
  state roundtrip, and a separate no-assets copy test are therefore **unexecuted**.
  No execution-policy or Device Guard bypass was attempted. A PowerShell native
  launch error can leave `$LASTEXITCODE` unchanged; it is not a passing test.
- The previously restricted TransientGen test executed successfully this run
  (17/17). The current policy block concerns the expanded MSVC host executable.

## MSBuild and IntelliSense

The user-supplied Release x64 build command, MSBuild 18.10 with v145 override,
completed with **exit 0**. Log: `out/P12b4c-repair-msbuild.log`, ending:

```text
PASS: crash_sample.bin -> C:\Users\valen\AppData\Local\Programs\Common\VST3\LXR.vst3
LXR resource packaging: PASS
MSBUILD_EXIT_CODE=0
```

Existing conversion/framework warnings remain; this was not a warning-free build.

`.vscode/c_cpp_properties.json` now uses evaluated Release/x64 MSBuild include
directories and defines (`VST3_API`, `IPLUG_EDITOR=1`, `IPLUG_DSP=1`, `NDEBUG`,
graphics and Windows definitions). The compiler is the actual selected v145
`14.51.36231/bin/Hostx64/x64/cl.exe`. The recursive repository search, unrelated
skill-directory include and Debug/Unicode assumptions were removed. C++17 and
x64 MSVC mode remain. An MSVC syntax-only compile of `LXR.cpp` using these
settings passed (exit 0); the VS Code diagnostics display has not been observed.
No valid iPlug2 API calls were rewritten. This config is already Git-ignored.

## REAPER verification — pending

No REAPER process was running during inspection; its config had empty
`lastproject`/`projecttab1`. Its VST cache contains LXR. There was no selected
saved project whose restored parameters could be inspected. Available computer
controls cannot operate the native REAPER app; no audible-playback claim is made.

Manual acceptance steps:

1. Restart REAPER to unload any old DLL. Open a new empty project and insert a
   fresh **VST3i: LXR (Sonic Potions)** instance from the local bundle above.
   Use 48 kHz, stereo main output and track/master routing at unity.
2. Inspect the generic parameter list: D1 waveform 1, attack 0, decay 64,
   volume 100, velocity volume modulation 1, filter type 0 (maps to LP),
   routing 0, D1 decimation 127 and global decimation 127. Disable bypass/mute.
3. Insert MIDI note 60, channel 1, velocity 110, spaced one second apart.
   Play with ordinary monitoring. Confirm an audible hit and nonzero track and
   master meters. Render a short stereo WAV and verify nonzero finite samples.
   The offline peak near 0.0500 (about −26 dBFS) is a useful scale, not a strict
   DAW threshold because host timing/velocity can differ.
4. Repeat at 44.1 and 96 kHz, then change back to 48 kHz on the same instance.
   Confirm output after each reset. Save/reopen a test project and verify values.
5. In an existing user project, first inspect the saved D1 values listed above.
   Old attack 127 and/or decimation 0 legitimately remain silent. Do not overwrite
   its patch. Make a separate project copy before intentionally editing it.
6. Test channels 2/3/5/6/7 for D2/D3/CY/closed/open HH. Channel 4's existing snare
   silence stub is a known unresolved failure. Capture REAPER rate, plugin path,
   relevant parameters and rendered peak with the acceptance result.

## Remaining risks and boundaries

- FIR group delay is **23.5 native frames**, approximately **0.534 ms** or
  **25.635 host frames at 48 kHz**. `PLUG_LATENCY` and actual VST3
  `getLatencySamples()` remain **0**. Latency reporting/compensation is unresolved.
- An invalid `PolyphaseResampler::configure()` after an earlier successful
  configuration retains the prior rate/readiness. The wrapper currently ignores
  the returned boolean. Unsupported-rate transitions therefore do not necessarily
  enter the documented silence gate. This was found by code inspection, is not
  the supported-rate startup-silence cause, and was left for explicit follow-up.
- Engine voice buffers and loaded asset storage use function-static arrays;
  concurrent-instance ownership and wider parameter-thread safety still require
  a separate audit. No broad real-time-safety certification is claimed.
- No oscillator, filter, envelope equation, mixer saturation, FIR coefficient,
  asset, sequencer or preset architecture was changed. No next phase was started.

## Exact edited source/config/document paths

1. `dsp/ParameterArray.cpp`
2. `vendor/iPlug2/Examples/LXR/LXRParamContract.h` (pre-existing untracked file)
3. `tests/P12b4cDefaultStateDiagnostic.cpp` (pre-existing untracked file)
4. `tests/P12b4cRuntimeSilenceTest.cpp` (new)
5. `tests/P12b4cVST3RuntimeTest.cpp` (new)
6. `.vscode/c_cpp_properties.json` (pre-existing ignored file)
7. `docs/p12b4c_runtime_silence.md` (new)
8. `docs/phase_log.md`

`LXR.cpp`'s existing tracked diff and `P12b4cNativeAudioDiagnostic.cpp` were
preserved without new edits. Test/build outputs were generated under ignored
`out/` and existing build directories, and the authorized build installed the
updated bundle. `git diff --check` passes. Untracked/ignored edited paths are
listed explicitly because they are absent from an ordinary tracked Git diff.

## FIR latency reporting and final validation — 2026-10-26

Scope: P12b-4c latency reporting only. **Implementation/offline latency checks
PASS; overall acceptance BLOCKED. P12b-4c is not DONE.** No next phase started.

### Starting state and preservation

- Branch `p12b-4c-fir-resampler`, HEAD
  `280279debe94cb26810ea89b89f36b9bf965a747`; neither changed.
- Initial tracked edits: `docs/phase_log.md`, `dsp/ParameterArray.cpp`,
  `vendor/iPlug2/Examples/LXR/LXR.cpp`. Initial `git diff --check` passed.
- There were 4617 pre-existing untracked files, including the five runtime/
  diagnostic test sources, this report, `LXRParamContract.h`, iPlug2 dependencies,
  backups, build reports and validator tree. All remain present. No cleanup,
  destructive restoration, staging, commit, push, merge or PR occurred.
- Initial full status (4620 lines), tracked diff and selected SHA-256 hashes:
  `C:\Users\valen\AppData\Local\Temp\lxr-latency-20261026-004516\`.
  `ParameterArray.cpp` still hash-matches its starting contents. The pre-existing
  MIDI-channel repair in `LXR.cpp` is preserved. Existing generated builds were
  rebuilt using the requested commands; backups/dependencies were not removed.

### Verified model, measurements and rounding

The original `config.h` defines `REAL_FS` with the float literal
`44002.7573529412f`; `Engine.h` represents this as **44002.7578125 Hz**.
The resampler now references that Engine constant instead of repeating a literal.
The 48-tap nominal group delay is `(48-1)/2 = 23.5` native frames, or
`23.5 / 44002.7578125 = 0.000534057435676` seconds.

For host frame n, the source position is s=n*Fs/Fh, base b=floor(s), and the
selected fraction is p=floor(512*(s-b))/512. The convolution reads x[b-k], and
the sinc center is k+p-23.5. Thus a source impulse at q has its nominal center
at n=(q+23.5)*Fh/Fs. No lookahead or extra delay line is present. Missing
pre-start samples are zero. Native blocks are rendered on demand; the cache
does not insert a 32-native-frame wait. Host/device buffer delay and native
MIDI/block scheduling are separate and are not added to FIR latency reporting.

At the native rate, the measured impulse response is symmetric and its signed
first moment is **23.500000000000 native frames**. At host rates, integer peaks
and signed moments below were measured from streamed impulses, independently
checked against direct finite convolution using absolute time. Tests also place
impulses at native frames 31, 32 and 137 to cover block boundaries and later phase.

| Host Hz | Nominal host frames | Measured startup peak | Reported VST3 samples | Nominal residual after PDC | Measured signed first moment |
|---:|---:|---:|---:|---:|---:|
| 32000 | 17.089837941620 | 17 | 17 | +0.089837941620 | 17.069540151752 |
| 44100 | 23.551932913296 | 24 | 24 | -0.448067086704 | 23.552680391294 |
| 48000 | 25.634756912431 | 26 | 26 | -0.365243087569 | 25.638537165735 |
| 88200 | 47.103865826591 | 47 | 47 | +0.103865826591 | 47.104388149552 |
| 96000 | 51.269513824861 | 51 | 51 | +0.269513824861 | 51.296150678085 |
| 192000 | 102.539027649723 | 103 | 103 | -0.460972350277 | 102.542837618075 |

VST3's installed `ivstaudioprocessor.h` requires a uint32 sample count and
`restartComponent(kLatencyChanged)` when latency changes; it specifies no
rounding direction. Nearest integer, ties upward, minimizes nominal PDC error
to at most 0.5 host frame. Residual = nominal minus reported: positive means
nominally late after compensation, negative means early. Ceiling would create a
larger error at 32/88.2/96 kHz without any corresponding physical delay.

The **0.534057435676 ms** figure is the nominal filter delay, not a claim of
constant measured delay at every frequency/phase. Phase selection has a source
fraction quantization below 1/512 frame; fractional-phase windowing and host
sampling also affect the sampled response. The signed moment is a separate
measurement, not an integer peak or proof of a perfectly linear-phase
time-invariant SRC at all ratios. No FIR design/coefficient equation was changed.

### iPlug2 integration and lifecycle evidence

- New `dsp/FIRLatency.h`: pure `noexcept`, allocation-free supported-rate,
  exact-host-frame, integer-host-sample and millisecond helpers. Rates in the
  existing continuous 32000–192000 Hz range are accepted; invalid, unsupported,
  NaN and infinite values return zero latency. Integer conversion is bounded.
- `PolyphaseResampler::configure` clears readiness/history before trying a new
  rate. A rejected reconfiguration no longer leaves an earlier rate active.
  The adapter exposes its configured rate for the wrapper's lifecycle check.
- The installed single-component VST3 implementation calls `SetSampleRate`,
  block setup and `OnReset` from `SetupProcessing`. SDK `setupProcessing` is a
  UI-thread, inactive-state operation. This is where a changed rate regenerates
  the preallocated coefficient bank; `ProcessBlock` is unchanged.
- iPlug2 also calls `OnReset` from `setProcessing(false)`, which the SDK permits
  on the processing thread. `OnReset` now retains a ready bank at the same rate,
  resets Engine/cache/history as before, and avoids redundant host notifications.
  Activation retains configuration; deactivation still discards cached audio.
  Lifecycle synchronization relies on the VST3 contract: configuration must not
  overlap `process`; no audio-thread lock was added.
- After configuration, the wrapper calls `SetLatency` only if the integer value
  changed. Failure yields readiness=false, silence and reported latency zero.
  The installed `IPlugVST3::SetLatency` stores the count via `IPlugProcessor`,
  then calls the attached handler's `restartComponent(kLatencyChanged)`.
  `getLatencySamples()` reads that stored count even without a handler.
- `PLUG_LATENCY` remains 0 for the initial/unconfigured instance, with an added
  explanatory comment. Runtime reporting owns the configured value. There is
  no competing static nonzero latency. iPlug2's bypass delay is created only
  for input-bearing plugins; LXR's `0-2` instrument has no such delay line.
- The actual installed DLL test attaches an `IComponentHandler` and checks the
  stored count, notification count and getter value inside the callback. All six
  rates, return to 48 kHz, same-rate reset/restart, invalid-rate zero/recovery and
  absence of process-time latency notifications passed. This proves the plugin/
  framework contract, not REAPER's internal mixer compensation.

### Executed tests and build

All results below are from this run; historical passes do not replace blocked
current execution. Logs are under ignored `out/`.

| Check | Actual result |
|---|---|
| New latency suite, `-O0 -g` and `-O2 -g`, C++17, `-fno-exceptions -fno-rtti -ffp-contract=off` | PASS: 220/220 at each level; normal standalone O2 build 214/214; exit 0; measured rendering C++ allocations 0 |
| FIR functional suite at O0/O2 | PASS: existing 7 cases, exit 0 at each level (the rate cases here cover five rates; the new latency/quality suites cover all six) |
| FIR quality suite at O0/O2 | PASS: 38/38 each, exit 0; 19 kHz to 32 kHz alias attenuation -77.74 dB |
| Native/adapter runtime silence suite at O0/O2 | FAIL: 418 pass / 1 fail each, exit 1; unchanged snare audibility assertion; renderAllocations=0 |
| `tools/run_tests.bat` | 11/13 programs PASS; 2 BLOCKED by Device Guard: LfoModulation and ParameterArray. The batch wrapper itself returns 0 despite its two failed launches; this is not an all-pass result |
| Original-C filter differential, O2 | PASS: 80000 blocks, zero mismatches; exit 0 |
| Original-C filter differential, O0 | BLOCKED: executable launch rejected by Application Control; not a test failure or pass |
| Loaded installed VST3 offline integration, MSVC O2 | PASS: 1101/1101, exit 0 |
| Same DLL in separate bundle without assets | PASS: 1101/1101, exit 0; silence at all rates, valid latency reporting |
| Steinberg VST 3.8.1 validator | PASS: 47 passed, 0 failed; exit 0 |
| MSBuild 18.10, VS Community 18, Release x64, v145 | PASS: exit 0; successful link, bundle creation and installed resource packaging |
| REAPER PDC/playback/reopen after this change | NOT RUN (UI interaction BLOCKED); see manual acceptance below |

Latency tests use independent `(47-0)/2` geometry, host time conversion, direct
impulse/PCM convolution and integer-neighbor selection. They cover invalid
rates/NaN/infinity, rate changes on one instance, reset replay, stereo isolation,
native PCM identity and no stale cached tail. Six additional audit checks compare
synthetic PCM bit-for-bit with the pre-task FIR snapshot in
`out/P12b4c-latency-original-resampler.h`; the normal standalone suite has 214
checks without `LXR_LATENCY_COMPARE_BASELINE`. That optional snapshot is a local
audit artifact, not a new production dependency.

Rendering allocation counts cover C++ `new/new[]`, not all CRT/OS allocation
APIs. Source/configuration review found no new allocation, lock, file I/O,
coefficient generation or logging in `ProcessBlock`/adapter/FIR. The evaluated
Release definitions omit `PARAMS_MUTEX`, so the framework's conditional parameter
mutex is not enabled in this configuration. `SetLatency` is outside processing.
This is scoped validation, not certification of all framework/UI/parameter
concurrency. Existing Engine/asset function-static storage still needs an
independent multi-instance/thread audit.

Principal logs: `P12b4cLatencyTest_O0.txt`, `P12b4cLatencyTest_O2.txt`,
`P12b4c{ResamplerTest,FIRQualityTest,RuntimeSilenceTest}_{O0,O2}_latency.txt`,
`P12b4c-latency-dsp.log`, `P12b4c-latency-filter-O2.txt`,
`P12b4c-latency-vst3-runtime.txt`, `P12b4c-latency-vst3-no-assets.txt`,
`P12b4c-latency-validator.txt`, `P12b4c-latency-msbuild.log`,
`P12b4c-latency-host-build.txt`, `P12b4c-latency-msbuild-config.json`.

Original-C compilation initially encountered its historical empty-parameter
prototype under GCC's current default C dialect; strict c11 then lacked M_PI.
Using `-std=gnu11` compiled the original unchanged. The O0 launch was then
blocked: "An Application Control policy has blocked this file". The batch DSP
launches explicitly reported "blocked by your organization's Device Guard
policy" for the two named programs. No renaming/recompilation to evade a block,
security-setting changes or other policy bypass was attempted. A stale native
exit variable after a PowerShell launch error is not accepted as a passing test.

Build/installation:

- DLL: `%LOCALAPPDATA%\Programs\Common\VST3\LXR.vst3\Contents\x86_64-win\LXR.vst3`.
- Built and installed DLL SHA-256 both:
  `106448A07F0CCB93DE821E6775F730C1F455A2E0A7DDF7ED6E6372C257952798`.
- All five original assets hash-match in generated and installed bundles.
  Existing warnings remain; no new latency compilation errors occurred.
- Evaluated project settings remain C++17, `ExceptionHandling=Sync`, with no
  explicit `RuntimeTypeInfo` value. The standalone tests disable exceptions and
  RTTI; aligning inherited vendor build flags with repository policy is a separate
  build-configuration follow-up, not a latency-source change.

Loaded-plugin D1 peaks remain identical to the prior recorded 44.1/48/96/32/192
kHz values: approximately 0.0500060019 / 0.0500246621 / 0.0500246621 /
0.0500159337 / 0.0500298966. New 88.2 kHz peak: 0.0500289385. Output is finite.
The expanded test now also executes D1-muted channel isolation, all-228-value
audible/silent state roundtrips, restart without MIDI (zero tail replay), and
missing-assets operation, which were previously blocked.

### REAPER status and precise remaining acceptance

The user confirmed audible sound before this task. REAPER v7.80 was launched
into a new unsaved project; one isolated empty track was added. Its FX chooser
listed `VST3i: LXR (Sonic Potions)`. Selecting it failed with:
`point (745, 466) is over reaper.exe "List1", not target window reaper.exe
"[unsaved project] - REAPER v7.80 - EVALUATION LICENSE"`.
The prescribed target activation/refresh and one retry returned the same error.
UI input was stopped. No loaded-instance PDC, new playback, listening, timing or
project-reopen result is claimed. The test project remains unsaved with the FX
chooser open; no existing user project/patch was edited. Offline VST3 acceptance
above is independent of this UI limitation.

Manual completion checklist (record host/device rate, raw plugin samples, host
graph PDC, rendered peak/timing and patch values for each step):

1. Unload old LXR instances/restart REAPER, use a separate test project, and load
   the installed VST3i from the path above. Keep ordinary PDC enabled, with no
   manual latency override or other track FX. Confirm successful load.
2. At actual processing rate **44100 Hz**, inspect the FX latency display: the
   raw plugin count must be **24 samples**. At **48000**, it must be **26**;
   at **96000**, **51**. Host graph/track compensation may be rounded to a
   processing block; record it separately from the raw plugin count. Do not use
   device input/output latency as the plugin measurement.
3. Change rates on that same instance in order 44100 -> 48000 -> 96000 -> 48000,
   stopping/restarting processing as required. Confirm each count updates and
   each rate still plays note 60/channel 1/velocity 110. Optional additional
   rates: 32000 -> 17, 88200 -> 47, 192000 -> 103 samples.
4. Use isolated MIDI hits at known one-second positions, then repeat playback
   and stop/start without a fresh note. Check track/master meters and capture a
   finite nonzero render. Confirm no previous hit or FIR tail replays on restart.
   Keep the current audible patch; an old intentionally silent patch must remain
   silent. Channel 4 snare is separately known to fail.
5. For timing, record/render equivalent MIDI on an LXR-only path and a reference
   path; inspect sample positions after PDC, distinguishing the synth patch's
   attack/native block scheduling from the FIR delay. If comparing PDC enabled
   versus disabled, the compensated shift must correspond to the reported count
   (subject to the host's documented graph/block policy), with the nominal
   sub-frame residual from the table. Do not infer FIR delay from the drum's
   waveform peak alone. Capture any host rounding or unexplained extra delay.
6. Save the test project with an audible patch, close/reopen it, verify all
   relevant parameter values remain, the current rate's count is restored, and
   the same MIDI produces sound. Preserve the test project/render as acceptance
   evidence. Mark the new REAPER acceptance PASS only after these observations.

### Independent snare blocker and exact changed paths

`SnareVoice::calcSyncBlock` still writes zero to every output sample. Commit
`c750a034ea043144e3bf43f98daa505c44d813a1` introduced the compiling stub;
`07d18be` is the latest snare edit. Both precede `main` at `a73f310` and the
P12b-4c branch commit. `git diff main -- dsp/Snare.cpp` is empty, as is the
working diff. Thus this is inherited from main, not introduced by the FIR branch
or this latency work. The channel-routing assertions pass; the channel-4
`MIDI startup hit audible` assertion is retained and fails at both O0/O2.
Implementing/verifying the original snare chain is a separate follow-up.

Files edited by this latency task (including pre-existing untracked sources):

1. `dsp/FIRLatency.h` (new)
2. `dsp/PolyphaseResampler.h`
3. `dsp/LXRAudioAdapter.h`
4. `vendor/iPlug2/Examples/LXR/LXR.cpp` (prior MIDI repair retained)
5. `vendor/iPlug2/Examples/LXR/config.h` (comment only)
6. `tests/P12b4cLatencyTest.cpp` (new)
7. `tests/P12b4cVST3RuntimeTest.cpp` (pre-existing untracked test extended)
8. `docs/phase_log.md` (prior entry retained)
9. `docs/p12b4c_runtime_silence.md` (this pre-existing untracked report)

No original DSP algorithm, FIR coefficient design, native rate/block size,
startup-default repair, MIDI routing, UI, sequencer or preset architecture was
changed. No commit is authorized: review/approval is needed for precisely the
nine task paths above before committing this work. Any separately desired
pre-existing defaults/diagnostics/dependencies require their own explicit
selection; do not stage the entire working tree. Phase-wide interface acceptance
and completion remain open with P12b-4c.

## Automated REAPER PDC and audio verification — 2026-10-26

Host-side PDC classification: **PASS**. REAPER 7.80/x64 executed
`tools/reaper/p12b4c_pdc_test.lua` in isolated new project instances. The
installed VST3 path and SHA-256 matched the earlier installed-binary validation.
No user project was opened or overwritten, and no DSP, MIDI, preset or UI source
was changed.

For each required rate, fresh LXR instances received one channel-1 note 60 at
velocity 110 (one note-on plus one note-off). REAPER rendered 2.5-second stereo
WAVs once with normal chain PDC and once with `chain_pdc_mode=2` (ignore PDC).
The WAV headers independently confirmed the offline processing/output rate.
`TrackFX_GetNamedConfigParm("pdc")` measured plugin reporting; identical-render
integer-lag RMS alignment measured host-applied compensation. This did not infer
PDC from the FIR calculator or from the synth waveform peak.

| Rate | plugin `pdc` | chain actual/reporting | measured normal-vs-ignore shift | nominal residual | alignment error | peak | RMS | frames | result |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 44100 | 24 | 1024 / 24 | 24 | -0.448067 | 0 | 0.0499936342 | 0.00120634971 | 110250 | PASS |
| 48000 | 26 | 1024 / 26 | 26 | -0.365243 | 0 | 0.0499974489 | 0.00120634792 | 120000 | PASS |
| 96000 | 51 | 1024 / 51 | 51 | +0.269514 | 0 | 0.0500279665 | 0.00120634948 | 240000 | PASS |

`chain_pdc_actual=1024` is REAPER's block-sized graph value and is recorded
separately from `chain_pdc_reporting` and the observed timeline shift. In the
ignore-PDC cases, `chain_pdc_actual=0` while `pdc` and chain reporting retained
24/26/51. The compensated and ignored renders were otherwise sample-identical
after the measured shift (waveform alignment RMS error zero). All audio was
finite and nonzero. The measured residual to the nominal 23.5-native-sample FIR
delay equalled the expected nearest-integer fractional residual and stayed within
the declared 0.25-frame measurement tolerance beyond that residual.

A separate fresh REAPER instance reopened the saved test project with six LXR
instances. All reloaded and reported 51 samples at the saved 96 kHz rate. Two
transport cycles each observed playing state 1 followed by stopped state 0;
the reopened project remained clean. The prior 418-pass/1-fail runtime evidence
was not altered or reclassified: the inherited snare renderer remains the only
known failure and is outside this PDC task.

Artifacts: `out/reaper-p12b4c/pdc_results.json`,
`out/reaper-p12b4c/pdc_report.md`, the paired matrix WAVs,
`out/reaper-p12b4c/p12b4c_pdc_matrix.rpp`, and
`out/reaper-p12b4c/transport_reload_probe.json`. Optional 32/88.2/192 kHz rates,
MIDI channels 2-7, and real-time audio-device rate switching were not run in
this matrix. The host PDC requirement is closed; phase-wide P12b-4c remains
blocked only by the already recorded independent issues.

## Snare renderer follow-up — 2026-10-26

The inherited snare silence blocker described above is repaired and verified;
see `docs/snare_renderer_repair.md`. The restored implementation is bit-exact to
the original firmware C across 4,864 deterministic samples at each of O0/O2,
the retained runtime assertion now passes (419/419 at both levels), and the
installed VST3 passes 1,143/1,143 plus Steinberg validator 47/47. A fresh REAPER
48 kHz channel-4 render produced finite nonzero audio (peak 0.410337090492249,
RMS 0.00756627536108755) while retaining the independently measured 26-sample
PDC shift with zero alignment error.

The strict snare task remains **BLOCKED** only because Windows Application
Control blocks the required O0 `Mixer_test.exe` and O2 FIR-quality executable;
no bypass was attempted. Subjective post-repair listening is NOT RUN. No prior
reports or render artifacts were overwritten.
