# P12c-R1 final-hash REAPER certification and policy-test closure

P12c-R2 follow-up (**PASS**, 2026-10-30): see
[the repair and final certification record](p12c_r2_controller_sync_repair.md).
It preserves this report's original 34 PASS / 2 FAIL reproduction and separately
documents the controller cache repair, host MIDI precision issue and results
on the rebuilt binary. The R1 history below is not rewritten as a prior PASS.

Date: 2026-10-29. Final classification: **FAIL — executed MIDI controller-readback assertions fail**.

The previous Windows launch restrictions are resolved for the two exact O2 files. Fresh REAPER state, deterministic audio, transport, and FIR/PDC checks pass on the expected final binary. Certification cannot pass because MIDI changes reach serialized state while the VST3 controller continues to return old values. No production repair was made. P12c-R1 is not DONE; P16 was not started.

## Baseline and preservation

Repository: `C:\Projects\lxr-vst3`  
Branch: `p12b-4c-fir-resampler`  
HEAD: `280279debe94cb26810ea89b89f36b9bf965a747`

The requested read-only Git baseline commands were run first. `git diff --check` exited 0; it emitted only the existing LF-to-CRLF conversion warnings. Branch and HEAD were rechecked unchanged. All starting tracked edits and untracked files were preserved. No reset, clean, stage, commit, push, merge, or branch switch occurred. No firmware, framework, DSP, FIR coefficient, sample-rate constant, or MIDI-mapping file was changed during this task.

The exact initial status is retained in [initial-status.txt](../out/p12c-r1-final-20261029/initial-status.txt), with a starting tracked diff in `initial-tracked.diff` and source/header hashes in `input-hashes.json`. Its complete contents are:

```text
 M docs/phase_log.md
 M dsp/Engine.cpp
 M dsp/LXRAudioAdapter.h
 M dsp/ParameterArray.cpp
 M dsp/PolyphaseResampler.h
 M dsp/Snare.cpp
 M vendor/iPlug2/Examples/LXR/LXR.cpp
 M vendor/iPlug2/Examples/LXR/LXR.h
 M vendor/iPlug2/Examples/LXR/config.h
 M vendor/iPlug2/Examples/LXR/scripts/package-resources.ps1
?? LXR-build-latest.txt
?? LXR-preprocessed.xml
?? LXR-validator-report.txt
?? docs/p12b4c_runtime_silence.md
?? docs/p12c_r1_concurrency_hardening.md
?? docs/p12c_state_serialization.md
?? docs/snare_renderer_repair.md
?? dsp/FIRLatency.h
?? dsp/FixedFifo.h
?? dsp/ParameterStateAuthority.h
?? tests/P12b4cDefaultStateDiagnostic.cpp
?? tests/P12b4cLatencyTest.cpp
?? tests/P12b4cNativeAudioDiagnostic.cpp
?? tests/P12b4cRuntimeSilenceTest.cpp
?? tests/P12b4cSnareRenderTest.cpp
?? tests/P12b4cVST3RuntimeTest.cpp
?? tests/P12cR1ConcurrencyTest.cpp
?? tests/P12cStateSerializationTest.cpp
?? tests/P12cVST3StateTest.cpp
?? tests/reference/snare/
?? tools/reaper/
?? vendor/iPlug2/.clang-format
?? vendor/iPlug2/.claude/
?? vendor/iPlug2/.editorconfig
?? vendor/iPlug2/.gitattributes
?? vendor/iPlug2/.github/
?? vendor/iPlug2/.gitignore
?? vendor/iPlug2/CLAUDE.md
?? vendor/iPlug2/CMakeLists.txt
?? vendor/iPlug2/CMakePresets.json
?? vendor/iPlug2/Dependencies/
?? vendor/iPlug2/Documentation/
?? vendor/iPlug2/Examples/CMakeLists.txt
?? vendor/iPlug2/Examples/IPlugChunks/
?? vendor/iPlug2/Examples/IPlugCocoaUI/
?? vendor/iPlug2/Examples/IPlugControls/
?? vendor/iPlug2/Examples/IPlugConvoEngine/
?? vendor/iPlug2/Examples/IPlugDrumSynth/
?? vendor/iPlug2/Examples/IPlugEffect/
?? vendor/iPlug2/Examples/IPlugInstrument/
?? vendor/iPlug2/Examples/IPlugMidiEffect/
?? vendor/iPlug2/Examples/IPlugOSCEditor/
?? vendor/iPlug2/Examples/IPlugP5js/
?? vendor/iPlug2/Examples/IPlugReaperExtension/
?? vendor/iPlug2/Examples/IPlugReaperExtensionWebUI/
?? vendor/iPlug2/Examples/IPlugReaperPlugin/
?? vendor/iPlug2/Examples/IPlugResponsiveUI/
?? vendor/iPlug2/Examples/IPlugSideChain/
?? vendor/iPlug2/Examples/IPlugSurroundEffect/
?? vendor/iPlug2/Examples/IPlugSvelteUI/
?? vendor/iPlug2/Examples/IPlugSwiftUI/
?? vendor/iPlug2/Examples/IPlugVisualizer/
?? vendor/iPlug2/Examples/IPlugWebUI/
?? vendor/iPlug2/Examples/LXR/LXR.cpp.before-P12b4c-fix.bak
?? vendor/iPlug2/Examples/LXR/LXRParamContract.h
?? vendor/iPlug2/Examples/LXR/LXRState.h
?? vendor/iPlug2/Examples/LXR/config/LXR-win.props.before-xml-fix.bak
?? vendor/iPlug2/Examples/LXR/projects/LXR-vst3.vcxproj.before-P12b4b1.bak
?? vendor/iPlug2/Examples/LXR/projects/LXR-vst3.vcxproj.before-postbuild-fix.bak
?? vendor/iPlug2/Examples/LXR/projects/LXR-vst3.vcxproj.corrupted.bak
?? vendor/iPlug2/Examples/LXR/scripts/postbuild-win.bat.before-canonical-restore.bak
?? vendor/iPlug2/Examples/README.md
?? vendor/iPlug2/Examples/buildall-mac.sh
?? vendor/iPlug2/Examples/buildall-win.ps1
?? vendor/iPlug2/Examples/cleanup-mac.sh
?? vendor/iPlug2/Examples/duplicate.py
?? vendor/iPlug2/Examples/gitignore_template
?? vendor/iPlug2/IGraphics/
?? vendor/iPlug2/IPlug/
?? vendor/iPlug2/LICENSE.txt
?? vendor/iPlug2/README.md
?? vendor/iPlug2/Scripts/
?? vendor/iPlug2/Tests/
?? vendor/iPlug2/WDL/
?? vendor/iPlug2/azure-pipelines.yml
?? vendor/iPlug2/common-ios.xcconfig
?? vendor/iPlug2/common-mac.xcconfig
?? vendor/iPlug2/common-wasm.mk
?? vendor/iPlug2/common-web.mk
?? vendor/iPlug2/common-win.props
?? vendor/iPlug2/iPlug2.cmake
?? vendor/iPlug2/iPlug2.code-workspace
?? vst3-validator-build/
```

Prerequisites reviewed: AGENTS.md; architecture, parameter map, risks, phase log, P12c serialization, R1 hardening and snare reports; LXRState.h/LXR.h/LXR.cpp; ParameterStateAuthority.h/FixedFifo.h; all three P12c test sources; the R1, base state, FIR/PDC and underlying PDC Lua scripts. Historical reports were treated as historical, including older plugin hashes.

## Exact executable and resources

All three executable copies are **727,552 bytes**, SHA-256:

`B170E00911A4031B04616C0B75E2B2A7CE5902E5CF46F22A23F1D2DD93A6F953`

Paths:

- Built linker output: `vendor/iPlug2/Examples/LXR/build-win/vst3/x64/Release/LXR.vst3`.
- Built bundle executable: `vendor/iPlug2/Examples/LXR/build-win/LXR.vst3/Contents/x86_64-win/LXR.vst3`.
- Installed executable: `C:\Users\valen\AppData\Local\Programs\Common\VST3\LXR.vst3\Contents\x86_64-win\LXR.vst3`.

The initial identities are in `binaries-initial.json`; independent post-test rehashes are checked by `p12c_r1_analyze_certification.py` and recorded in `verification.json`. No rebuild or reinstall was necessary or attempted. No binary was overwritten in use. The seven required resources match in `data/`, the built bundle, and the installed bundle, including package-contract sizes. Full paths and hashes are in `resources.json`.

| Resource | Bytes | SHA-256, equal in all three locations |
|---|---:|---|
| sine_table.bin | 8194 | `84BAE1556A743B86EA23F098B3E2329BBE260B5E6DC2030213F873437D4DD3F7` |
| saw_table.bin | 22528 | `50972E365198C9A768CCAD454AD93BFC12E691857BF2C138737F9F54606104C0` |
| tri_table.bin | 22528 | `591E0656F4A961157554AE2220055366487A744E273554D2120FA27001081B01` |
| rec_table.bin | 22528 | `0E0B93DAB16158A1AB7808170CEBF4878A80FD8AAEF4DA8B8810F4D2B112E509` |
| crash_sample.bin | 32768 | `440AC7DC5A43652A85EE8F01BB8CB9D4A0384ACE39E9785F23B2C448D7EC4365` |
| transient_data.bin | 26460 | `F120C68FCB1BBC3AFAA778F989712078D6A99E474202AAABD75CD34F82AB0C4C` |
| transient_volume_table.bin | 276 | `FA7E25307CCD3F86A5B0B39623F3E56617A9F72629D4A6E656869EE658CB98BE` |

REAPER is **7.80/x64**, confirmed from file-version metadata and the running ReaScript API. `reaper-version-hash.txt` identifies its executable. The host-to-hash chain uses task launch PID/creation time/command line, isolated config and script path, REAPER splash/project-load logs, the process's loaded LXR module path, and pre/post hashes of that exact file. Module inventories are `processes-state.json`, `processes-mid.json`, `host-module-identities.json`, and `host-module-identities-final.json`. The final state PID was **26996**; the final PDC PID was **25524**. Both loaded the installed path/hash above, with the final inventory taken after test completion and before closing. A hardcoded Lua hash alone is not treated as proof.

## REAPER processes and safety

There were **zero reaper.exe processes at task start**, including no historical PID 15300. `processes-initial.json` is empty for that reason; `baseline.json` explicitly records the empty inventory. No pre-existing/user session was touched. Executable path, command line, creation time, parent and module details were collected for task instances. Parent process IDs identify task PowerShell launchers; exited parents are not assumed to remain available.

| Task PID | Isolated configuration directory relative to task outputs | Loaded-module evidence |
|---:|---|---|
| 28632 | reaper-config | LXR module verified |
| 34036 | . | no loaded LXR at capture |
| 13544 | run2\config | LXR module verified |
| 24004 | pdc2\config | LXR module verified |
| 27540 | run3\config | LXR module verified |
| 16836 | live-config | no loaded LXR at capture |
| 20352 | live-config-retry | LXR module verified |
| 19484 | run4\config | LXR module verified |
| 25524 | pdc-final\config | LXR module verified |
| 26996 | state-final\config | LXR module verified |

Projects, WAVs, scripts, reports and logs were preserved before any close requests. `artifacts-before-graceful-close.json` records hashes where files were readable; its own entry and active splash logs have null hashes because of open-file sharing restrictions. The project/render files were readable and retained. No save/discard dialog was answered automatically.

Normal `Process.CloseMainWindow()` requests closed seven task processes. Three refused that graceful request and remained at the final inventory: **34036**, **13544**, and **16836**. Their exact launch command lines/creation times are in `processes-after-close.json`; they correspond to the first cache-less PDC attempt, the run2 state attempt, and the initial Lua probe with an invalid file-open mode. No force termination was attempted. Cleanup of those three stopped here: force termination would require explicit user approval, or they can be closed manually after inspecting their dialogs. No remaining build/install operation depends on closing them.

## Harness timeout diagnosis and focused changes

The historical R1 attempt left no result artifact, and its process no longer existed. The available project report establishes a native UI script-import timeout, but does not prove whether a license modal, import dialog, or automation helper caused it. It would be inaccurate to invent a more specific historical root cause.

The installed REAPER `-help` output confirms direct `.lua` command-line execution and `-newinst`, `-new`, `-cfgfile`, and `-splashlog`. This task used those native capabilities, task-owned configurations, the existing VST cache, existing accepted render/state logic, unique paths and non-overwrite guards. No licensing dialog was bypassed and no registration data was copied. The evaluation-license title remained visible in process/window metadata.

The focused helper `tools/reaper/p12c_r1_final_certification.py` creates task Lua variants and runs preserved regression executables. Changes include explicit writes to all 228 slots per instance, progress logs, coroutine/deferred scheduling, a 90-second cooperative deadline, transport position checks, separate B audibility, MIDI diagnostics, and strict PDC rechecks after reload. Console regressions have 55-second timeouts. Synchronous REAPER API calls cannot be interrupted by that cooperative deadline; it does not authorize killing REAPER.

Attempts and limitations were preserved:

- The first MIDI assertion sampled host values after an offline render; subsequent live playback and an independent installed-binary test were needed to distinguish host-state behavior from that method.
- The first separate PDC config lacked the VST scan cache and could not instantiate LXR. Copying the existing cache into a new task config resolved instantiation.
- A generated path lacked a separator and wrote task artifacts to `C:\Projects\lxr-vst3out\p12c-r1-final-20261029`. The generator was corrected. Copies are preserved under `path-error-preserved/`; the original task-created sibling files were left intact. Final evidence paths are all under the repository's `out/`.
- Initial transport requests made during command-line startup were stopped before processing progressed. A two-second deferred harness start, task-local 48-kHz dummy audio, explicit audio initialization, and playback-position assertions resolved this. The native live probe records the earlier play=1 to play=0 transition.
- An initial standalone probe used unsupported Lua `io.open(...,"wx")`; its corrected variant used the supported mode. The original is preserved.
- Native UI inspection was unreliable: state/keypress calls delayed or failed, and a later screenshot captured unrelated foreground content instead of the hidden task window. No action was taken on that content. No particular modal cause is claimed.
- The extra controller-readback verifier initially had an incorrect include path, then missing MemoryStream/User32 link dependencies. Both failed build logs were retained; the final build exited 0 and its actual assertions ran.

## Two-instance save/reopen and audio

The state-only final checkpoint created two distinct non-default LXR instances, explicitly set **all 228 slots** in each (using safe native defaults plus the established patch edits), saved a dedicated project, recreated both instances, changed parameters deliberately, reopened again, and compared **456/456** slots exactly. There were zero mismatches. Reserved slot 0 restored to **77 and 0**. Both complete expected and actual arrays are in [state-checkpoint.json](../out/p12c-r1-final-20261029/state-final/state-checkpoint.json) and [verification.json](../out/p12c-r1-final-20261029/verification.json). The independent analyzer also decoded both saved LXRS payloads and verified version/header/CRC/value bounds and equality with those arrays.

Project: `out/p12c-r1-final-20261029/state-final/p12c_state_roundtrip_cert_b170e009.rpp`.

| Measurement | Before reload A | After reload A | Independent B |
|---|---:|---:|---:|
| Frames / sample count | 67200 / 134400 | 67200 / 134400 | 67200 / 134400 |
| Sample rate / channels | 48000 / 2 | 48000 / 2 | 48000 / 2 |
| PCM | 24-bit | 24-bit | 24-bit |
| Peak | 0.0006400346755981445 | 0.0006400346755981445 | 0.1301330327987671 |
| RMS | 0.00014255583587496562 | 0.00014255583587496562 | 0.007745851477042285 |
| Finite / nonzero | yes / yes | yes / yes | yes / yes |

A maximum absolute sample difference = **0**; RMS error = **0**. Exported PCM samples are exactly equal. Whole WAV hashes differ because container metadata differs; equality was checked on decoded sample arrays. The contract was not weakened. A uses the established deterministic tonal path and matched restored initial histories; noise/RNG runtime-state serialization is not claimed. B's separate audible render comes from `run4/instance_b.wav` using the same hash and patch; the final checkpoint records A and the 456-slot comparison.

Two transport cycles passed with play state 1, advancing positions **0.42666666666666658** and **0.4479999999999999** seconds, then stop state 0. Both instances reported 26-sample PDC. The checkpoint's PASS is scoped to state/audio/transport; it is not a complete milestone PASS.

## MIDI failure and smallest proposed repair

REAPER `run4` played MIDI item events CC88=77 and NRPN selectors CC99=0, CC98=1, CC6=45 on channel 1. Audio was running at 48 kHz; playback reached **0.9599999999999995 s**. Nevertheless, exposed slots 88/128 remained **100/0** while playing and after stopping. Its raw failure, observations and saved diagnostic project are preserved in `run4/`. Desired-value REAPER MIDI save/reopen was **NOT COMPLETED** because the controller agreement assertion failed. No MIDI persistence PASS is claimed.

That REAPER attempt also includes host stop/reset behavior and tightly spaced NRPN events. An independent installed-binary reproduction therefore delivered each CC in a separate valid VST3 processing call and dispatched the Windows message loop for 500 ms to allow production idle notifications. It uses the existing installed-state host helper and an ordinary component handler that records edits without echoing them back to the controller.

Reproduction source and build command: `out/p12c-r1-final-20261029/controller-readback.cpp` and `build-controller-readback.bat`. The build uses the established VS18 vcvars64 environment, C++17, `/O2 /MD /GR- /EHs-c-`, local Steinberg hosting libraries, the existing MemoryStream object, and User32 for timer dispatch. The final build exits 0. Verifier SHA-256:

`5F87C6E70422F69BBF26E367F0934D2E634085C37C14A57E7FA3EC6A07AD89BD`

Exact test command:

```powershell
& 'C:\Projects\lxr-vst3\out\p12c-r1-final-20261029\controller-readback.exe' 'C:\Users\valen\AppData\Local\Programs\Common\VST3\LXR.vst3\Contents\x86_64-win\LXR.vst3'
```

Observed result, `controller-readback.log`: **34 passed, 2 failed, exit 1**.

| Slot/event | Expected | Serialized authoritative value | VST3 IEditController readback |
|---|---:|---:|---:|
| 88 / CC88 | 77 | 77 | 100 |
| 128 / NRPN1 | 45 | 45 | 0 |

The handler received **2** edit notifications. This is an executed plugin integration failure, independent of REAPER startup and MIDI timing. The existing installed-state suite checks serialized MIDI values, but lacks these controller-agreement assertions, explaining why its 322/322 can still pass.

Source-level explanation: `LXR::OnIdle()` (`LXR.cpp:540`, value call at line 567) generation-checks MIDI notifications and calls `IPlugAPIBase::SetParameterValue()`. That method (`IPlugAPIBase.cpp:117`) updates `IParam`, informs the host, and calls OnParamChange, but does not update the separate VST3 `mParameters` cache. `IPlugVST3ControllerBase::GetParamNormalized()` (line 194) reads that cache. The existing VST3 UI route explicitly calls `SetVST3ParamNormalized()` (`IPlugVST3.cpp:245`), which illustrates the missing projection update. The measured disagreement is between serialized authority and the VST3 controller projection; private native ParameterArray/Engine fields were not inspected in REAPER, and no new claim of an Engine race is made.

**Proposed repair only, not implemented:** after the existing generation/value checks, synchronize the VST3 controller projection on the main thread before the MIDI host edit notification, using the established LXR-local/API route that updates the controller cache. Retain generation validation and bounded audio behavior. Add this readback assertion plus a real REAPER MIDI save/reopen assertion to the required regression contract. Review ordering before choosing the exact call; do not broaden the iPlug2 fork or change MIDI mapping/DSP/state bytes. A production repair would create a new final hash and require recertification.

The original two R1 concurrency reproductions still pass; this newly exposed controller projection defect prevents acceptance. Forced live restore/MIDI overlap and a separate REAPER automation stress run were **NOT RUN** after this failure. No unsupported interleaving coverage is claimed.

## FIR/PDC at 48 kHz

Final PDC evidence: `out/p12c-r1-final-20261029/pdc-final/pdc_b170e009_48000_results.json`, its Markdown report, RPP, paired WAVs and `pdc_reload_0.wav` / `pdc_reload_1.wav`.

Both WAV headers confirm 48,000 Hz, stereo, 86,400 frames / 172,800 samples, 24-bit PCM. Both are finite/nonzero, peak **0.04999744892120361**, RMS **0.0014216946543263442**. REAPER's direct `TrackFX_GetNamedConfigParm(...,"pdc")` reports **26** samples. The installed native-host runtime/latency tests independently confirm the plugin's 26-sample report at 48 kHz. REAPER chain actual latency is **1024** and chain reporting latency is **26**; block-rounded chain actual latency is not mistaken for direct plugin latency.

The independent Python stereo-PCM lag scan over 0..64 frames finds shift **26**, aligned waveform RMS error **0**, normal onset **47108**, ignored-PDC onset **47134**, and aligned onset error **0**. The established Lua comparison independently agrees. Reload recreated both plugin instances, re-rendered normal/ignore paths and again measured **26**, aligned onset error **0**, and aligned waveform RMS error **0**, with direct PDC 26. The original strict waveform assertion was strengthened explicitly, never relaxed. No resampler/PDC regression was observed.

The named `pdc`, chain PDC settings and render-stat APIs are described in the
[official REAPER ReaScript reference](https://www.reaper.fm/sdk/reascript/reascripthelp.html).
Support and actual results were verified on installed 7.80 rather than assumed
from the newer online reference.

## Fresh regression matrix

Every row below is a **fresh execution during this task**, not a copied historical result. Except for the new readback verifier, preserved existing test executables were rerun without recompilation; their exact hashes, commands, timestamps, exit codes and logs are in `regression-matrix.json` and per-test JSON/log files. Source and header hashes were captured separately. The installed suites and validator loaded the exact installed final DLL. Standalone native tests are not presented as REAPER tests or as renewed plugin-build provenance.

| Executable/configuration | Status | Fresh result |
|---|---|---|
| P12cR1ConcurrencyTest_O0 | PASS | SUMMARY pass=35 fail=0 |
| P12cR1ConcurrencyTest_O2 | PASS | SUMMARY pass=35 fail=0 |
| P12cStateSerializationTest_O0 | PASS | SUMMARY pass=29 fail=0 |
| P12cStateSerializationTest_O2 | PASS | SUMMARY pass=29 fail=0 |
| P12b4cSnareRenderTest_O0 | PASS | SUMMARY pass=61 fail=0 renderAllocations=0 |
| P12b4cSnareRenderTest_O2 | PASS | SUMMARY pass=61 fail=0 renderAllocations=0 |
| P12b4cRuntimeSilenceTest_O0 | PASS | SUMMARY pass=419 fail=0 renderAllocations=0 |
| P12b4cRuntimeSilenceTest_O2 | PASS | SUMMARY pass=419 fail=0 renderAllocations=0 |
| P12b4cResamplerTest_O0 | PASS | SUMMARY fail=0 |
| P12b4cFIRQualityTest_O0 | PASS | SUMMARY pass=38 fail=0 |
| FIRLatency_O0 | PASS | SUMMARY pass=220 fail=0 renderAllocations=0 |
| OriginalCFilter_O0 | PASS | differential: 80000 blocks, 0 mismatches (bitwise on buffers and all state) |
| P12b4cResamplerTest_O2 | PASS | SUMMARY fail=0 |
| P12b4cFIRQualityTest_O2 | PASS | SUMMARY pass=38 fail=0 |
| FIRLatency_O2 | PASS | SUMMARY pass=220 fail=0 renderAllocations=0 |
| OriginalCFilter_O2 | PASS | differential: 80000 blocks, 0 mismatches (bitwise on buffers and all state) |
| P12cVST3StateTest | PASS | SUMMARY pass=322 fail=0 |
| P12b4cVST3RuntimeTest | PASS | SUMMARY pass=1101 fail=0 |
| SteinbergValidator | PASS | Result: 47 tests passed, 0 tests failed |
| ResonantFilter_O2_policy | PASS | SUMMARY fail=0 warn=0 |
| TransientGen_O2_policy | PASS | SUMMARY pass=17 fail=0 |
| Added controller-readback verifier / O2 | FAIL | 34 passed, 2 failed, exit 1 |
| REAPER final state checkpoint | PASS | 456/456, exact A PCM, two advancing transport cycles |
| REAPER MIDI controller agreement | FAIL | expected 77/45, exposed 100/0 |
| REAPER desired-value MIDI save/reopen | NOT RUN | earlier assertion failed; no PASS claim |
| REAPER final PDC and reload | PASS | direct 26, shift 26, aligned onset/RMS errors 0 |
| Separate broad 13-program O0/O2 batch | NOT RUN | focused required matrix and the two formerly denied files were run |
| REAPER forced restore/MIDI overlap / separate automation stress | NOT RUN | stopped after confirmed controller defect |
| ThreadSanitizer | NOT RUN | no compatible TSan session established |

## Windows Code Integrity closure

Current `Win32_DeviceGuard` reports Code Integrity enforcement **2** and user-mode Code Integrity enforcement **2**, before and after testing (`deviceguard.json`, `deviceguard-final.json`). Existing 3033/3077 event XML was re-read and preserved as `ci-event-*.xml`; it identifies the historical `VerifiedAndReputableDesktop` policy and the exact previously denied files. Read-only `CiTool -lp -json` returned access denied **0x80070005**, so a new full policy inventory/allow-rule explanation is unavailable. Current policy identity/hash is not assumed from old events.

The two exact files now execute normally in place under the enforced policy:

| File | SHA-256 | Fresh result |
|---|---|---|
| `out/ResonantFilter_test_O2_p12c.exe` | `94E53F9616522D6E01D2A5847756C3AC2263008D8AD94A6FCE1CAE3DAFC286A3` | exit 0, fail=0, warn=0 |
| `out/TransientGen_test_O2_p12c.exe` | `D34E2410BA5AF960AE11C1496879DF35F00356AEF472F36A5A94E4F3C0B3D7C7` | exit 0, 17/17 |

Both remain unsigned (`policy-signatures.json`). The helper first used ordinary process creation, then both were rerun by **direct PowerShell invocation**, the route previously denied. Exact commands are their absolute paths; `policy-powershell-retry.json` records both exit codes 0 and the timestamp. Logs are `ResonantFilter-O2-powershell.log` and `TransientGen-O2-powershell.log`. No new matching 3033/3077 denial was found for this task. The mechanism by which the current policy accepts the files was not established; no signing/allow-policy change is attributed to this task.

No Device Guard/WDAC/Application Control setting, policy, executable name/location, signature, loader, or security control was changed to obtain these runs. There was no alternate test binary substituted, O0 inference, security bypass, or requested exception. These two previously blocked test items are now **PASS** through normal enforcement. No administrator action is needed to execute these exact two files on the currently observed host; the remaining acceptance failure is code behavior.

## Out-of-scope observation

`docs/param_map.md` documents original firmware addressing as PAR=CC-1 and
PAR=NRPN+128. The current `dsp/MidiParser.h` instead maps CC directly to PAR
and NRPN1 to PAR128. This certification used the pre-existing installed-test
contract (CC88/NRPN1) without changing that mapping. Reconciling that documented
original/native difference is outside this verification task.

## Artifacts, documentation and final gate

- Main machine-readable report: [verification.json](../out/p12c-r1-final-20261029/verification.json).
- Human-readable host evidence: [reaper-verification.md](../out/p12c-r1-final-20261029/reaper-verification.md).
- Exact state arrays/project/paired renders: `out/p12c-r1-final-20261029/state-final/`.
- PDC original/reload measurements: `out/p12c-r1-final-20261029/pdc-final/`.
- Failed full MIDI host attempt: `out/p12c-r1-final-20261029/run4/`.
- Minimal installed-DLL failure: `controller-readback.cpp`, `build-controller-readback.bat`, `controller-readback.log`, `controller-readback-result-final.json` under the task output root.
- Scoped reusable verification helpers: `tools/reaper/p12c_r1_final_certification.py`, `tools/reaper/p12c_r1_analyze_certification.py`.

Documentation updates are limited to this report and current-status addenda in `docs/p12c_r1_concurrency_hardening.md`, `docs/p12c_state_serialization.md`, and `docs/phase_log.md`. Historical evidence remains present. Production/source preservation and final `git diff --check` are recorded in `closure.json` / `git-diff-check-final.txt` after these edits. The final check exits 0 with the pre-existing conversion warnings only.

**FAIL** is required by the acceptance gate because an executed controller-agreement test fails due to plugin behavior. The security execution gap is closed; final-hash state/audio/PDC evidence is now available. MIDI convergence/persistence acceptance remains unsatisfied, and forced-overlap coverage is missing. Three task REAPER processes remain a cleanup/authorization issue, not a basis for ignoring the code failure. No phase was declared DONE and no P16 or other feature work began.
