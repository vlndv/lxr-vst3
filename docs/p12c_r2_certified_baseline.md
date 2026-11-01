# P12c-R2 certified baseline and controlled Git checkpoint

Current checkpoint status after **P12c-R3 (2026-10-31): READY for separate Git
execution authorization**. P12c-R2 functional certification remains PASS. No
staging, commit, push, installation or feature implementation has occurred.
Gate closure and fresh source-test evidence are in
[p12c_r3_checkpoint_gate_closure.md](p12c_r3_checkpoint_gate_closure.md); executable
instructions are in [build_and_test.md](build_and_test.md).

Sections 1-6 retain the original checkpoint audit's dated observations and
binary provenance, with the source-test limitation subsection updated for R3.
Sections 7-8 are the revised current plan. The original full document is
byte-preserved under `out/p12c-r3-20261031-142231/originals/docs/`.

## 1. Starting Git state and preservation

| Item | Observed value |
|---|---|
| Branch | `p12b-4c-fir-resampler` |
| HEAD | `280279debe94cb26810ea89b89f36b9bf965a747` |
| Upstream ref | `origin/p12b-4c-fir-resampler` |
| Upstream commit | `280279debe94cb26810ea89b89f36b9bf965a747` |
| Index changes | none |
| Tracked paths | 222 |
| Tracked modifications | 10 |
| Untracked files, before this document | 4,654 |
| Ignored files at initial inventory | 53,603 |
| Root submodules / gitlinks / `.gitmodules` | none |

The tracked modifications are exactly:

```text
docs/phase_log.md
dsp/Engine.cpp
dsp/LXRAudioAdapter.h
dsp/ParameterArray.cpp
dsp/PolyphaseResampler.h
dsp/Snare.cpp
vendor/iPlug2/Examples/LXR/LXR.cpp
vendor/iPlug2/Examples/LXR/LXR.h
vendor/iPlug2/Examples/LXR/config.h
vendor/iPlug2/Examples/LXR/scripts/package-resources.ps1
```

Untracked classification: 3 root build/preprocessor/validator reports; 6
engineering reports; 3 DSP headers; 17 test/harness files; 15 REAPER-tool files
(14 source files and one Python bytecode cache); 3,515 vendor paths; 1,095
validator-build paths. Vendor paths include the two missing LXR headers and six
LXR backups, besides unrelated upstream content. The exhaustive initial lists,
including ignored paths, are preserved in
`out/certified-baseline-audit-20261031-133054/initial-inventory.json`.
This document adds one untracked documentation file. Audit outputs are ignored.

Original R1/R2 evidence and reports were preserved. The audit directory contains
new logs, manifests, downloaded upstream archives, and isolated builds; it is
excluded from commits. Do not run the old certification scripts in place:
several write into their original dated evidence directories.

Root `.gitignore` excludes `out/`, `build/`, `ref/`, editor directories and
compiled executable/object extensions. It does **not** exclude
`vst3-validator-build/`, root reports, REAPER Python caches, the dependency
snapshot, or backups. The untracked vendor `.gitignore` additionally excludes
SDK contents, `Dependencies/Build/*`, and example build directories. No required
new LXR/DSP header or framework patch is currently ignored. All seven runtime
assets are already tracked in `data/`; their bundle copies are ignored.

`core.autocrlf=true` comes from Git's system configuration. No explicit
`core.eol`, `core.safecrlf`, or global excludes setting was returned. Root has
no `.gitattributes`. The untracked `vendor/iPlug2/.gitattributes` is nevertheless
active: `* -text`, C/C++ `text=auto`, Windows projects/scripts CRLF, shell and
Apple project files LF. New source byte hashes may change on checkout solely
through line endings; compare both byte hashes and normalized source when
assessing provenance. Do not renormalize existing source during checkpointing.

## 2. Certified feature inventory and binary identity

The certified implementation includes:

- Audible new-instance defaults: attack 0, per-voice/global decimation 127,
  open-hat decay 64; these are plugin patch choices, not extracted hardware kit
  defaults. Existing silent saved values remain meaningful.
- Seven-channel native MIDI routing, the existing native parameter mapping,
  and all 228 host slots (reserved 0 plus active 1-227).
- Original-rate synthesis at the float-derived 44002.7578125 Hz, 32-frame native
  blocks, 48-tap/512-phase FIR host resampling, invalid-rate silence, and dynamic
  integer host latency. At 48 kHz the report is 26 samples.
- Original-C-compatible snare rendering and both transient resources.
- LXRS v1: 24-byte little-endian header, 228-byte payload, CRC-32, 252-byte plugin
  state plus four-byte VST3 bypass; bounded complete validation and exact legacy
  228-double migration.
- One parameter-state authority, coherent bounded snapshots, generation-ticketed
  automation mailboxes, ordered deferred MIDI, and a preallocated 4096-event FIFO
  that rejects the newest event on overflow.
- Main-thread VST3 controller projection with generation/connection-checked
  acknowledgements and `kParamValuesChanged`; MIDI CC proxy canonicalization
  before the framework's truncating conversion. No automation gestures are
  manufactured for MIDI synchronization.

Certified installed executable (environment-relative location):

```text
%LOCALAPPDATA%/Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3
bytes: 729600
SHA-256: 89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82
```

The linker output and built bundle match that hash, before and after this audit.
The installed executable and resources were never replaced. The binary is not
described as built from HEAD alone: it was built from HEAD **plus the uncommitted
source inventory** and the dependencies below.

## 3. Dependency and framework audit

### Exact iPlug2 source baseline

Required upstream: `https://github.com/iPlug2/iPlug2`, commit
**`d54f69050f517e43b941d88c2a170f0a840b9ee4`**.
There is no iPlug2 `.git` metadata in `vendor/iPlug2/`; this is an unpacked source
snapshot, not a root submodule. The project currently has no machine-enforced
pin or acquisition recipe. This document records the recovered pin.

The audit downloaded the immutable commit archive and compared **all 3,552
upstream files** with the local tree. None is missing. After CRLF normalization,
only two paths differ:

1. `IPlug/VST3/IPlugVST3_Common.h`: the intended local state-reader patch.
2. `Dependencies/IPlug/VST3_SDK/README.md`: the upstream placeholder is replaced
   by the actual SDK's README. This is a dependency overlay, not a framework fix.

Downloaded archive SHA-256:
`DB587C465547A27EB166E95829E882500413217BC7E886E2686D589423DE51DA`.
Archive URL: `https://codeload.github.com/iPlug2/iPlug2/zip/d54f69050f517e43b941d88c2a170f0a840b9ee4`.

The historical report's **`d82b2e99b17c8f723d3f6e2a88ffd8b656f28e5d`
is a comparison revision, not this snapshot's baseline**. Against it, 97 files
differ beyond line endings, including Windows WDL inputs. Those differences
are explained by the older matching revision; do not import 96 unrelated
changes to make the local tree look like that newer revision. Comparison details
are `vendor-comparison.json` and `candidate-comparison.json` in the audit output.
WDL, NanoVG, NanoSVG, GLAD, STB and other bundled inputs come from the matching
iPlug2 archive, rather than separately guessed versions.

### SDK revisions

`vendor/iPlug2/Dependencies/IPlug/VST3_SDK` is an independent nested Git checkout
from `https://github.com/steinbergmedia/vst3sdk.git`, clean at
**`3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`**. Its recorded submodules are:

| SDK path | Commit |
|---|---|
| `base` | `fcf9da0bd27a16f7f03773a3a39822f28f5c8477` |
| `cmake` | `054c9143cbb8d47fc4694e473f2ee3b4d951a8f5` |
| `doc` | `8bfca19d3b76a61d093951ba9297047f544caea1` |
| `pluginterfaces` | `4f547e8e102b47de4a8b8aaf343c73b700786372` |
| `public.sdk` | `586dc5e6c8012c3e4b01c79389375cbe96bdb1da` |
| `tutorials` | `33b73dfbb87f3fde3bce8c0a10cae934dc66ad34` |
| `vstgui4` | `5db272256172557818b6158cf0bb2c4410bddb25` |

Plugin compilation requires SDK root, base, pluginterfaces and public.sdk;
validator rebuilding also requires its CMake/hosting dependencies. The existing
validator is a disposable build, not a dependency to commit.

Original firmware remains read-only and excluded at
`ref/LXR-0.37`, clean commit
`dee49689f3f0cdf8f9b7b5e79628cfdca1b79394` from SonicPotions/LXR.
It is needed for original-C differential tests and data re-export, not plugin
compilation from the already tracked assets. Never substitute Catalyst.

### Complete local framework modification inventory

Only `vendor/iPlug2/IPlug/VST3/IPlugVST3_Common.h`, `IPlugVST3State::SetState()`
around upstream lines 67-88, changes framework behavior:

- Return failure for a negative plugin `UnserializeState()` position.
- Return failure if seeking to the bypass offset fails.
- Require both successful reading and exactly four bypass bytes.

Local patched header SHA-256:
`4AD9986250B87869160A5D49F0832FCD140F134026A8E99BF0EB00AD575A654C`.
It affects every VST3 built from this framework snapshot. Valid state and bypass
encoding are unchanged. The installed state suite and validator pass; dedicated
framework failed-seek/short-read branch tests remain absent. R2 adds **no** new
shared-framework change. The plugin's two class files are project code under
Examples/LXR, not additional upstream framework patches.

### Recommended acquisition strategy

Keep the existing tracked LXR overlay and one explicitly tracked framework
patched header. Acquire immutable iPlug2 and SDK sources into the same existing
paths from the pins above. Track this recipe and exact input hashes; exclude
downloaded dependency contents. This avoids a wholesale vendor import and
preserves layout. Do not convert `vendor/iPlug2` to a submodule: tracked LXR
files already live below that path, and conversion would require a separately
approved topology change.

For a **fresh checkout only**, acquisition must:

1. Download the pinned iPlug2 archive into a new ignored cache directory and
   verify its SHA-256 above before extraction.
2. Extract into a temporary directory. Copy archive files into `vendor/iPlug2`
   only when absent. Preserve tracked `Examples/LXR/**` and the patched
   `IPlug/VST3/IPlugVST3_Common.h`. Verify any other pre-existing collision by
   normalized content and stop on a mismatch. Skip the SDK placeholder directory.
   Never expand an archive over the present worktree or overwrite the patch.
3. In that fresh tree's absent SDK directory, clone the SDK, detach at the exact
   SDK commit, then initialize recursively at its recorded submodule revisions:

   ```powershell
   git clone --no-checkout https://github.com/steinbergmedia/vst3sdk.git vendor/iPlug2/Dependencies/IPlug/VST3_SDK
   git -C vendor/iPlug2/Dependencies/IPlug/VST3_SDK checkout --detach 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
   git -C vendor/iPlug2/Dependencies/IPlug/VST3_SDK submodule update --init --recursive
   ```

   These are future fresh-checkout acquisition instructions, not permission to
   switch any branch or modify the current dependency checkout during this task.
4. Verify pins, the patched-header hash, required resources, and the source
   manifest below before building. Keep upstream iPlug2, WDL and SDK notices
   with the fetched dependencies and any redistribution. `LICENSE-LXR.txt`
   covers the derived DSP/data; this remains non-commercial with full-source
   redistribution, with no GPL/AGPL or paid-store addition.

The isolated audit build used the pinned downloaded iPlug2 archive plus
`git archive` of the clean local SDK root/base/pluginterfaces/public.sdk commits.
It did not fetch a second independent network copy of the SDK or prove the
future bootstrap recipe end to end on another machine.

## 4. Certified source/build provenance and resources

R2's nine-entry `R2-change-inventory.json` matches current bytes **9/9**.
All **61** DSP sources/headers match the preserved R1 input manifest.
That manifest's other changes are the documented R2 class-file, snare-fixture,
and documentation edits. The required project/configuration/asset inputs exist.
There is no evidence of an omitted production source file. These are source
associations supported by preserved records, not an immutable original build
attestation or proof that every historical incremental object was rebuilt.

| Build item | Recorded/observed value |
|---|---|
| Visual Studio / MSBuild | Community 18 / `18.10.1-1.26427.6+3cd27c13e` |
| Platform / configuration | Windows x64 / Release |
| Toolset | `v145`; project defaults to v143, so override is required |
| Selected tools directory | MSVC `14.51.36231` |
| Current `cl.exe` file/product version | `19.51.36260.0` / `14.51.36260.0` |
| Windows SDK selected by audit build | `10.0.28000.0` |
| Language / optimization / runtime | C++17, `/O2 /Oi /GL /MT /fp:precise`, Release `NDEBUG` |
| Linking | `/LTCG:incremental /OPT:REF /OPT:ICF /DEBUG:FULL`, PDB paths embedded |
| Definitions | `VST3_API`, `IPLUG_EDITOR=1`, `IPLUG_DSP=1`, `NDEBUG`, `IGRAPHICS_NANOVG`, `IGRAPHICS_GL2`, Windows/MBCS definitions |
| Parameter mutex | `PARAMS_MUTEX` absent in the evaluated Release configuration |
| Plugin contract | `PLUG_CHANNEL_IO="0-2"`, state chunks enabled, initial latency 0 with dynamic reporting |

Historical build records identify the tools directory and configuration; no
historical `/Bv` attestation was found. The current compiler executable version
above is a fresh observation, not a substitute for a missing historical record.
The selected SDK/tool paths occur in both historical and audit build logs.

**Inherited build-policy discrepancy:** production uses `/EHsc`, with no explicit
RTTI disable; native tests disable exceptions/RTTI. This was already recorded in
the latency report and was retained by certification. Do not silently change
compiler settings in a preservation checkpoint. Policy alignment needs a scoped
decision and subsequent recertification; the baseline does not satisfy the
repository's intended compiler-flag policy merely because no new throw or RTTI
use was added.

The historical compile command is recorded in
`out/p12c-r2-20261030/msbuild-float-compiled.log`:

```powershell
# From the repository root in an environment with the tools above.
$repoRoot = (Resolve-Path .).Path
$shellDir = Join-Path $repoRoot 'vendor/iPlug2/Examples/LXR/'
& $msbuild 'vendor/iPlug2/Examples/LXR/projects/LXR-vst3.vcxproj' /t:Build /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 "/p:SolutionDir=$shellDir" /p:PostBuildEventUseInBuild=false /v:minimal
```

`$msbuild` must name the matching MSBuild executable. Post-build is intentionally
disabled here to prevent installation. Historical final packaging/install is
separately recorded in `msbuild-acceptance.log`; do not repeat it on the certified
installation for this audit. For future independent builds, use a separate
source/output tree and preserve all generated outputs separately.

Package the build into a new private bundle:

```text
LXR.vst3/Contents/x86_64-win/LXR.vst3
LXR.vst3/Contents/Resources/LXR/<the seven files below>
```

Asset lookup is relative to the loaded DLL. A bare DLL without sibling resources
does not establish audible synthesis. The existing Windows post-build packager
copies/verifies these files from `data/` into both built and installed bundles;
its two transient additions belong to C. For an audit, copy only into a new
private bundle. No generated `moduleinfo.json` or generated DSP header is
required by this Windows project. `resources/main.rc`, `resources/resource.h`,
`resources/LXR.ico` and `resources/fonts/Roboto-Regular.ttf` are tracked source
inputs; `main.res`, object files and bundle copies are generated outputs.

| `data/` asset | Bytes | SHA-256 (source = built = installed) |
|---|---:|---|
| `sine_table.bin` | 8194 | `84BAE1556A743B86EA23F098B3E2329BBE260B5E6DC2030213F873437D4DD3F7` |
| `saw_table.bin` | 22528 | `50972E365198C9A768CCAD454AD93BFC12E691857BF2C138737F9F54606104C0` |
| `tri_table.bin` | 22528 | `591E0656F4A961157554AE2220055366487A744E273554D2120FA27001081B01` |
| `rec_table.bin` | 22528 | `0E0B93DAB16158A1AB7808170CEBF4878A80FD8AAEF4DA8B8810F4D2B112E509` |
| `crash_sample.bin` | 32768 | `440AC7DC5A43652A85EE8F01BB8CB9D4A0384ACE39E9785F23B2C448D7EC4365` |
| `transient_data.bin` | 26460 | `F120C68FCB1BBC3AFAA778F989712078D6A99E474202AAABD75CD34F82AB0C4C` |
| `transient_volume_table.bin` | 276 | `FA7E25307CCD3F86A5B0B39623F3E56617A9F72629D4A6E656869EE658CB98BE` |

Note frequencies and square-root LUTs are compiled from the DSP source; their
tracked export files need not be added as extra runtime resources. Do not commit
new generated assets or fixtures as a shortcut to missing test prerequisites.

### Independent build result and byte-reproduction limit

A new source tree was populated with the matching upstream archive, clean SDK
archives and current tracked/new production inputs, with **no old objects**.
The existing project built successfully with post-build disabled. Only local
source materialization and new audit outputs were written.

Independent DLL: 729600 bytes, SHA-256
`5C5E3EF40BC31F79105B12175D5C78878F0B1E337C9C71A00061DD7499B6F3BE`.
This is **not** a replacement certification hash. In its private seven-resource
bundle it passes state 322/322, runtime 1101/1101, original controller assertions
36/36, external controller 955/955, native/Engine/FIFO probe 1033/1033, and
validator 47/47.

PE comparison finds identical `.rsrc` and `.fptable`; `.text`, `.rdata`, `.data`,
`.pdata`, and `.reloc` differ. Timestamps differ and debug paths are embedded.
The clean link reports all 8052 functions compiled because no prior IPDB/IOBJ
exists; certification used an incremental link. Inputs and the meaningful
configuration match the recovered manifest/pins, but code-section differences
mean this audit does **not** attribute the entire difference to timestamps/PDBs.
Absolute source paths and incremental LTCG layout are plausible contributors.
A second clean build kept the original source paths and redirected all outputs
and intermediate files to a new audit directory. It also built successfully,
729600 bytes, SHA-256
`590F778687C6AE9F23F03A4454BE07404FCB30E1E5761879B278841A9CCEFD9E`.
Its `.data`, `.reloc`, `.rsrc` and `.fptable` match the certified binary. The
remaining **23 differing `.text` bytes** occur in 16 `lea` instructions pointing
to the same C++ exception/type-information and CRT runtime-check symbols at
different addresses. Symbol-resolved dumpbin disassembly agrees at **all
94,549 parsed instruction addresses**, with no symbolic instruction differences.
This narrows the code-byte changes to references affected by linker data layout;
it does not indicate a changed DSP instruction sequence. Timestamp, debug-path
and other data/unwind-section differences still prevent a byte-identical claim.
Full byte-for-byte attribution remains incomplete. Functional source rebuilding
is demonstrated on this host; byte-identical and another-machine validation are
not.
See `independent-build-result.json`, `independent-build-command.json`,
`independent-msbuild-detailed.log`, `pe-comparison.json` and
`independent-host-tests.json`, plus `original-path-build-comparison.json` and
`instruction-relocation-analysis.json` in the audit output. The second build
disabled both pre/post-build events and redirected BUILD_DIR, OutDir, IntDir,
PDB and project-extension paths; the certified installation was untouched.

## 5. Checkpoint A-H mapping and shared-file hunk audit

Paths below are exact; unchanged tracked prerequisites are explicitly marked and
must not be restaged merely to suggest new work. `LXR/` in the table means
`vendor/iPlug2/Examples/LXR/` only.

| Group | Files/hunks and objective |
|---|---|
| A: startup/MIDI | `dsp/ParameterArray.cpp` init hunk: attack, PAR61, decimation defaults; new `LXR/LXRParamContract.h` supplies all 228 descriptors/native defaults; `LXR/LXR.cpp` event.status channel composition; `tests/P12b4cDefaultStateDiagnostic.cpp`, `tests/P12b4cNativeAudioDiagnostic.cpp`, startup/routing parts of `tests/P12b4cRuntimeSilenceTest.cpp` and `tests/P12b4cVST3RuntimeTest.cpp`. Native `dsp/MidiParser.*` mapping is already tracked and unchanged. |
| B: FIR/latency | New `dsp/FIRLatency.h`; changes to `dsp/PolyphaseResampler.h`, `dsp/LXRAudioAdapter.h`; `LXR/LXR.cpp` FIR include and OnReset rate/readiness/SetLatency hunk; `LXR/config.h` latency comment only; new `tests/P12b4cLatencyTest.cpp`; FIR/lifecycle portions of the two shared runtime tests. FIR implementation, `dsp/LXRAudioAdapter.cpp`, `tests/P12b4cResamplerTest.cpp`, `tests/P12b4cFIRQualityTest.cpp`, project and build props are already in HEAD. Relevant scripts: `tools/reaper/p12b4c_pdc_test.lua`, `p12b4c_pdc_smoke.lua`, `p12b4c_transport_probe.lua`, `p12c_fir_pdc_regression.lua`. |
| C: snare | `dsp/Snare.cpp` init/trigger/render hunks; `dsp/Engine.cpp` include and transient loading; `LXR/scripts/package-resources.ps1` transient file additions; `tests/P12b4cSnareRenderTest.cpp`; seven `tests/reference/snare/` files listed in commit 3; snare checks in shared runtime tests. Both transient assets and `dsp/TransientTables.*` are already tracked. `docs/snare_renderer_repair.md` and six snare REAPER wrappers/probes are historical documentation/tools, historical paths retained as factual evidence under R3, with active helpers now portable. The snare test's three `{}` initializers are an R2 fixture correction, not a C renderer change. |
| D: serialization | New `LXR/LXRState.h`, `tests/P12cStateSerializationTest.cpp`, `tests/P12cVST3StateTest.cpp`; `LXR/config.h` state-chunks switch; class serialization declarations, codec calls, full-candidate validation, runtime reset and MIDI/parser reset in `LXR/LXR.{cpp,h}`. Final callbacks use E's authority, so D is not independently stageable from E in the current diff. `tools/reaper/p12c_state_test.lua`, `p12c_fir_pdc_regression.lua` and historical state report belong here. |
| E: concurrency | New `dsp/ParameterStateAuthority.h`, `dsp/FixedFifo.h`, `tests/P12cR1ConcurrencyTest.cpp`; class authority initialization, coherent reset/serialization, writer publication, host mailboxes, processing-boundary projection, MIDI FIFO/replay/overflow, UI callback and process-boundary hooks; `docs/p12c_r1_concurrency_hardening.md`, `docs/p12c_r1_final_certification.md`, `tools/reaper/p12c_r1_state_test.lua`, `p12c_r1_final_certification.py`, `p12c_r1_analyze_certification.py`. |
| F: R2 controller | `LXR/LXR.cpp` MIDI proxy view classes, process override, component-handler override, OnIdle projection/acknowledgement, removed R1 queue/gesture path; `LXR/LXR.h` proxy/handler declarations, friend probe, main-thread generation/connection fields; new `tests/P12cR2ControllerSyncTest.cpp`; corrected C snare fixture; `docs/p12c_r2_controller_sync_repair.md`. R2-only diff/inventory is preserved under the original R2 output. |
| G: framework/provenance | Only local shared-framework code is `vendor/iPlug2/IPlug/VST3/IPlugVST3_Common.h`. This document supplies pinned dependencies, prerequisites, packaging and manifests. Proposed root ignore rules below exclude fetched vendor/build content while retaining the overlay/patch. No unrelated vendor source is eligible for staging. |
| H: engineering documents | `docs/phase_log.md` entire appended historical sequence; six new milestone reports listed in commit 4/5; this new baseline. `docs/architecture.md`, `docs/risks.md`, `docs/param_map.md` and existing other docs are unchanged and are not staged. Their OPEN/UNVERIFIED items remain open. |

### Hunk boundaries against HEAD

- `LXR.cpp` old line 20 include hunk contains **B** FIR include plus **F** SDK
  includes and two proxy classes. Do not stage that hunk as B alone.
- Old line 154 constructor hunk is **E** initialization. Old lines 182-225
  ResetEngineState/added methods are **D+E**; Encode/Decode format is D, but
  snapshot/publication in those same functions is E.
- Old line 240 OnReset hunk is **B**. Old lines 287-325 parameter/MIDI hunk
  combines **D+E+F** and would require manual index patch editing to split.
- Old line 358 status assignment is **A**, now within ProcessMidiMsgNow from E.
  Old line 366 adds **D+E** MIDI mirroring/publication; old line 399 processing
  entry is **D+E**. Do not extract the A status line by replacing production
  source with an older version.
- `LXR.h` old line 2 combines **D** codec, **E** authority/FIFO includes and
  standard-library includes. Old line 35 combines **D/E** state/UI callbacks,
  **F** process/handler declarations and probe. Old line 50 combines **E**
  authority/mailbox/FIFO state with **F** acknowledgement fields. Declaration,
  initialization and implementation must land together.
- `config.h` old line 17 combines **B** comment and **D** state-chunk switch.
- `phase_log.md` is one HEAD diff hunk replacing the final newline and appending
  entries from startup through R2. Its sections map respectively to A, B, B,
  C, D, D/G audit, E, E/F failure evidence, and F PASS. Keep the whole historical
  progression in H. Earlier FAIL/BLOCKED entries are not current R2 failures.

Consequently, commit the remaining A-F production/test changes together after
the prerequisite header/dependency and framework commits. This avoids inventing
uncompiled D-only or E-only intermediate implementations. The header required by
HEAD itself is provisioned first; see the independently built foundation below.

## 6. Verification performed for this audit

The **exact 30 commands** from
`out/p12c-r2-20261030/acceptance-checks/regression-matrix.json` were run again,
with a 55-second per-process timeout and logs in the new audit directory.
All test-executable hashes still match the certified matrix. These are fresh
executions of preserved executables, not fresh native-test compilations.

| Check | Fresh result |
|---|---|
| `git diff --check` | exit 0; existing LF/CRLF warnings only |
| R1 concurrency | 35/35 O0 and O2 |
| Native serialization | 29/29 O0 and O2 |
| Original-C snare differential | 61/61 O0 and O2, zero mismatches/allocations |
| Runtime synthesis/routing | 419/419 O0 and O2, zero counted rendering allocations |
| FIR functional | zero failures O0/O2 |
| FIR quality | 38/38 O0/O2 |
| FIR latency | 220/220 O0/O2 |
| Original-C filter differential | 80000 blocks, zero mismatches O0/O2 |
| MIDI parser / mixer | 5/5 and 28/28, both O0/O2 |
| Filter / transient | zero filter failures/warnings and 17/17 transient, O0/O2 |
| Certified installed VST3 state / runtime | 322/322 and 1101/1101 |
| Original controller assertions | 36/36 |
| External controller / native Engine/FIFO probe | 955/955 and 1033/1033 |
| Certified installed Steinberg validator | 47/47 |
| Independent source build | exit 0; private-bundle six host suites also pass, as above |

Independent re-analysis of the preserved R2 REAPER artifacts confirms both
456-slot state/MIDI checkpoints, slot 88=77 and slot 128=45, CRC-valid RPP
payloads, exact 67200-frame pre/post tonal PCM, finite/nonzero second-instance
audio, recorded advancing transport cycles, and 26-frame PDC with zero aligned
onset/waveform error including reload. Module identities and resource hashes
tie that evidence to the certified installed hash. No new REAPER session or
listening test was performed. The old GUI screenshot limitation, TSan and
whole-DLL heap-instrumentation qualifications remain unchanged.

Primary original evidence:

- `out/p12c-r2-20261030/verification.json`
- `out/p12c-r2-20261030/R2-change-inventory.json` and `R2-only.diff`
- `out/p12c-r2-20261030/acceptance-checks/`
- `out/p12c-r2-20261030/acceptance-state/`
- `out/p12c-r2-20261030/acceptance-pdc/`
- `out/p12c-r2-20261030/acceptance-module-identities.json`

Fresh audit evidence:

- `out/certified-baseline-audit-20261031-133054/regression-matrix.json`
- `out/certified-baseline-audit-20261031-133054/R2-source-match.json`
- `out/certified-baseline-audit-20261031-133054/reaper-artifact-recheck.json`
- `out/certified-baseline-audit-20261031-133054/source-manifest.json`
- `out/certified-baseline-audit-20261031-133054/independent-host-tests.json`

### R3 source-test workflow closure

The earlier missing local test prerequisites are now resolved by the portable
runner, explicit source/dependency manifests and four tracked filter compatibility
headers. See `docs/build_and_test.md`. A complete isolated run with independently
acquired dependencies passed **47/47 newly source-built suites**; native suites
run at O0/O2, hosting libraries/MemoryStream/validator are rebuilt from source,
and path tests pass 32/32. The preserved mandatory executable matrix separately
passed 30/30 again. Portable latency is 214 assertions; the historical 220 count
retains six optional snapshot comparisons and is not silently substituted.
The newly compiled runtime source passes 1143 assertions, while the preserved
older runtime executable passes 1101; neither test source nor assertions changed.

No production code, build flag, asset or installed VST3 changed during R3.
Historical reports retain factual machine paths; executable helpers now resolve
script-relative roots and explicit dependency/output overrides. Exception/RTTI
policy is explicitly **RETAIN** for the certified framework configuration,
with native DSP objects still exception/RTTI-disabled. Byte-identical Windows
binary reproduction is unestablished and is not a repository checkpoint
requirement. The earlier five preparation gates are closed, as recorded in R3.

## 7. Revised ordered commits and exact staging commands

**R3 preparation record; R4 execution authorized separately.** R4 completed
commits 1 and 2, then stopped before commit 3 on a dependency ordering error.
The R4 resume authorization moves only `dsp/FIRLatency.h` from commit 4 to
commit 3 because `tests/P12b4cSnareRenderTest.cpp` includes it directly.
The header's contents are unchanged (SHA-256
`0221E0572A25502B1438FDDB4F8D358AA0C913D3296B8E22C0BD51D120813FD6`).
The six-commit objective and every other path allocation remain unchanged;
this document and the revised manifest still belong to commit 6. The original
R3 manifest is preserved under
`out/p12c-r3-20261031-142231/final-checkpoint-source-manifest.json`, and the
original allocation is retained in `out/p12c-r4-20261101-005932/plan.json`.
The candidate commit-3 tree (HEAD plus its staged files and this header) passed
the original-C snare test 61/61 at O0 and O2 without later commit inputs.
Execution evidence remains outside the source inventory under the R4 out path.

**R4 resume 2 dependency correction (2026-11-01):** commit 5's
`tools/verification/test_portable_paths.py` unconditionally copies
`docs/p12c_state_serialization.md` and
`docs/p12c_r1_concurrency_hardening.md`; its historical Lua-generation helper
also hashes these inputs. Their original commit-6 allocation left required
test fixtures absent at commit 5. Separate user authorization moves both
unchanged reports to commit 5, preserving their historical content:

| Report | Unchanged SHA-256 |
|---|---|
| `docs/p12c_state_serialization.md` | `A1BC0B65262B0ED6C69E699908E75B8195C2155181811DEF45A7AB60650B96FE` |
| `docs/p12c_r1_concurrency_hardening.md` | `15395F08C0D70DE67998B6A602AC2ED36B917ACD81D141CBAD85E80D434E36E6` |

The revised groups contain 5/3/13/15/26/8 files, still 70 unique paths. This
document and the allocation manifest remain in commit 6. The preceding
allocation is preserved in `out/p12c-r4-20261101-005932/resume/plan.json`;
R3 manifests and both earlier execution reports remain unchanged.
The remaining dependency audit checked every commit-5/6 file, Python imports,
executable reads/copies, sibling Lua helpers, native source/fixture manifests,
external tool/dependency requirements and documentation references. An isolated
candidate-5 tree, containing none of commit 6's new files, passed all 32 portable
path checks. No further required execution input from commit 6 was found.
Historical evidence/executables are explicit external prerequisites for the
historical analyzer/regression modes; documentation links are references.
Certified production source, runtime settings and resource bytes are unchanged.
Detailed audit evidence is under the same R4 output path in `resume2/`.

The exact bounded list is
`tools/verification/checkpoint_files.json` (70 distinct paths). Review staged
names and `git diff --cached --check` before each commit. Do not use directory,
wildcard or bulk vendor staging. All listed changes are staged as whole files;
**no interactive hunk staging is needed**. A-H hunk ownership remains documented
in section 5; commit 4 combines the intertwined shell hunks for build integrity.

The dependency-bootstrap commit includes the narrow framework patch and missing
shell descriptor header together. Separating these would leave the documented
bootstrap unable to verify its required inputs. Snare and startup changes are
otherwise independently separated where their dependencies permit. Intermediate
commits are not individually recertified R2 milestones; acceptance belongs to
the complete reviewed sequence. No production source was edited for staging.

### Commit 1 - build: pin framework inputs and preserve state-reader hardening

Dependency commits: current HEAD.

Objective: provide a complete pinned framework/SDK acquisition foundation and the descriptor header already included by HEAD. The exact framework patch is confined to one file and its audited SetState hunk. It shares this bootstrap commit because acquisition requires the patched header, and HEAD needs the descriptor header. No unrelated vendor file is included. Evidence: independent acquisition/verification plus prior foundation compilation and current final integration suites. Risk: shared state error handling affects other iPlug2 VST3 projects; dedicated failed-seek/short-read framework unit coverage remains a later improvement.

Exact file list and proposed commands:

```powershell
git add -- .gitignore
git add -- tools/verification/dependencies.json
git add -- tools/verification/prepare_dependencies.py
git add -- vendor/iPlug2/Examples/LXR/LXRParamContract.h
git add -- vendor/iPlug2/IPlug/VST3/IPlugVST3_Common.h
```

### Commit 2 - fix(lxr): preserve certified startup defaults

Dependency commits: 1.

Objective: preserve the native attack/open-hat/decimation startup policy and its diagnostics. Host descriptors were supplied in commit 1. MIDI status/channel composition lands with the intertwined class in commit 4. Evidence: startup/default assertions in the 419-test runtime suite. Risk: defaults are a plugin policy, not extracted hardware kit values; existing restored silent patches remain unchanged.

Exact file list and proposed commands:

```powershell
git add -- dsp/ParameterArray.cpp
git add -- tests/P12b4cDefaultStateDiagnostic.cpp
git add -- tests/P12b4cNativeAudioDiagnostic.cpp
```

### Commit 3 - fix(snare): restore original renderer and transient resources

Dependency commits: 1, 2.

Objective: preserve original-compatible snare rendering, transient loading/packaging and the corrected original-C harness. Depends on startup defaults for engine-integration assertions. Evidence: 61/61 snare differential O0/O2, 419/419 runtime and exact resource hashes. Risk: hardware RNG/FPU and unsupported SD-sample qualifications remain; the R2 fixture zero-initialization correction is retained.

Exact file list and proposed commands:

```powershell
git add -- dsp/Snare.cpp
git add -- dsp/Engine.cpp
git add -- dsp/FIRLatency.h
git add -- vendor/iPlug2/Examples/LXR/scripts/package-resources.ps1
git add -- tests/P12b4cSnareRenderTest.cpp
git add -- tests/reference/snare/compat/MidiNoteNumbers.h
git add -- tests/reference/snare/compat/MidiParser.h
git add -- tests/reference/snare/compat/TriggerOut.h
git add -- tests/reference/snare/compat/config.h
git add -- tests/reference/snare/compat/sequencer.h
git add -- tests/reference/snare/original_snare_harness.c
git add -- tests/reference/snare/original_snare_harness.h
git add -- docs/snare_renderer_repair.md
```

### Commit 4 - fix(lxr): preserve certified FIR and P12c-R2 state pipeline

Dependency commits: 1, 2, 3.

Objective: preserve FIR lifecycle/latency, MIDI routing and the complete codec/authority/FIFO/controller pipeline as one compilable class/config change. Do not split declaration/definition or state-chunk/latency config hunks. Evidence: 47 source-built suites, 30 preserved suite reruns and read-only R2 REAPER analysis. Risk: bit-identical PE reproduction is not claimed; retain the documented framework flags and FIFO overflow contract.

Exact file list and proposed commands:

```powershell
git add -- dsp/LXRAudioAdapter.h
git add -- dsp/PolyphaseResampler.h
git add -- dsp/FixedFifo.h
git add -- dsp/ParameterStateAuthority.h
git add -- vendor/iPlug2/Examples/LXR/LXR.cpp
git add -- vendor/iPlug2/Examples/LXR/LXR.h
git add -- vendor/iPlug2/Examples/LXR/config.h
git add -- vendor/iPlug2/Examples/LXR/LXRState.h
git add -- tests/P12b4cLatencyTest.cpp
git add -- tests/P12b4cRuntimeSilenceTest.cpp
git add -- tests/P12b4cVST3RuntimeTest.cpp
git add -- tests/P12cStateSerializationTest.cpp
git add -- tests/P12cVST3StateTest.cpp
git add -- tests/P12cR1ConcurrencyTest.cpp
git add -- tests/P12cR2ControllerSyncTest.cpp
```

### Commit 5 - test: add portable source builds and REAPER helper configuration

Dependency commits: 1, 2, 3, 4.

Objective: supply repeatable native/host source builds, filter compatibility and portable REAPER helper configuration. Evidence: 47/47 isolated source suites, 32/32 path checks, new hosting libraries and validator. Risk: the copied probe header has a logged include-only adaptation; actual REAPER helpers require a new private project/bundle and are not run by path tests. Legacy R1 hash/label guards remain historical.

Exact file list and proposed commands:

```powershell
git add -- tools/run_tests.bat
git add -- tools/verification/run_checkpoint_tests.py
git add -- tools/verification/test_manifest.json
git add -- tools/verification/test_portable_paths.py
git add -- tests/reference/filter/README.md
git add -- tests/reference/filter/compat/config.h
git add -- tests/reference/filter/compat/datatypes.h
git add -- tests/reference/filter/compat/globals.h
git add -- tests/reference/filter/compat/stm32f4xx.h
git add -- tools/reaper/checkpoint_paths.lua
git add -- tools/reaper/p12b4c_pdc_smoke.lua
git add -- tools/reaper/p12b4c_pdc_test.lua
git add -- tools/reaper/p12b4c_snare_other_voice.lua
git add -- tools/reaper/p12b4c_snare_other_voice_final.lua
git add -- tools/reaper/p12b4c_snare_reaper.lua
git add -- tools/reaper/p12b4c_snare_reaper_final.lua
git add -- tools/reaper/p12b4c_snare_transport_probe.lua
git add -- tools/reaper/p12b4c_snare_transport_probe_final.lua
git add -- tools/reaper/p12b4c_transport_probe.lua
git add -- tools/reaper/p12c_fir_pdc_regression.lua
git add -- tools/reaper/p12c_r1_analyze_certification.py
git add -- tools/reaper/p12c_r1_final_certification.py
git add -- tools/reaper/p12c_r1_state_test.lua
git add -- tools/reaper/p12c_state_test.lua
git add -- docs/p12c_state_serialization.md
git add -- docs/p12c_r1_concurrency_hardening.md
```

### Commit 6 - docs: close P12c-R3 checkpoint gates and record provenance

Dependency commits: 1, 2, 3, 4, 5.

Objective: preserve truthful engineering history and record current gate closure, exact inputs, commands and exclusions. Evidence: production-byte preservation, pin/resource checks, audited 70-path inventory and diff checks. Historical machine paths are factual records, not executable assumptions. Risk: raw certification outputs remain separately preserved under ignored out/; no source commit alone archives every WAV/RPP/log.

Exact file list and proposed commands:

```powershell
git add -- docs/phase_log.md
git add -- docs/p12b4c_runtime_silence.md
git add -- docs/p12c_r1_final_certification.md
git add -- docs/p12c_r2_controller_sync_repair.md
git add -- docs/p12c_r2_certified_baseline.md
git add -- docs/p12c_r3_checkpoint_gate_closure.md
git add -- docs/build_and_test.md
git add -- tools/verification/checkpoint_files.json
```

## 8. Explicit exclusions and remaining authorization

Do not stage `out/`, `ref/`, downloaded dependency contents, build directories,
DLL/VST3 binaries, executables, objects, RES/PDB/IOBJ/IPDB files, renders, temporary
RPPs/REAPER configurations, caches, bytecode, root build reports/preprocessed XML,
backups, credentials or unrelated vendor files. Human-authored compatibility
headers, build manifests, resource scripts and finalized engineering reports are
explicitly eligible. Existing tracked assets remain tracked; no generated test
fixture was added. Historical paths in reports are preserved facts under R3's
instructions, not active workstation requirements or a redaction gate.

The R3-authorized ignore edit and portability/test-build corrections are complete.
All five technical preparation gates are closed; the full checkpoint is **READY**
for review and separate Git execution authorization. There is no topology change
or additional technical approval required. Bitwise DLL identity remains an
explicit limitation, not a source-checkpoint blocker. Production exception/RTTI
flags have an explicit RETAIN disposition; future changes need their own scope
and recertification.

No staging, commit, push, merge, branch switch, reset, clean, evidence deletion,
production source/configuration change, installation or feature work occurred.
The original R2 audit's BLOCKED classification was correct at its time; R3 closes
that preparation status without changing P12c-R2 functional PASS or rewriting
historical failures. Preserve raw evidence separately from source commits.

## 9. P15-M1 preparation

P12c-R2 is a stable functional baseline for separately authorized modulation
contract work after checkpoint closure. P15-M1 implementation is not ready to
begin under this checkpoint task's boundary. The phase log currently calls P15
UI layout; record the explicitly requested P15-M1 contract scope before future
work rather than silently treating it as permission for UI/P16.

Evidence to resolve in that future contract review:

- `dsp/ParameterArray.cpp` stores but does not apply 149-154 (`VEL_DEST`), 161-166
  (`VOICE_LFO`) and 167-172 (`TARGET_LFO`). Original MIDI restrictions do not by
  themselves justify making host/native/internal destinations permanent no-ops.
- `dsp/Modulation.cpp::resetTargets()` remains a stub. Formula-level velocity/LFO
  tests are not proof that the end-to-end destination routing contract is done.
- The original CC/NRPN offsets documented in `docs/param_map.md` disagree with
  the certified parser's direct-CC / NRPN1-to-128 convention. Preserve current
  project compatibility while investigating original firmware addressing; do
  not silently shift certified host IDs or serialized slots.
- Re-audit original `modulationNode.c`, `velocityModulation.c`, `lfo.c`, native
  parameter tables and front-panel internal messages at the pinned firmware
  revision. Define target types/ranges, source/target selection, trigger/tick
  ordering, quantization, and accepted interface headers before implementation.
- Retain base value plus modulation multiplier evaluated once per native tick,
  audio-thread ownership and the authority/FIFO/controller contracts. Add
  original-C differential vectors and host/native/serialization integration
  coverage for any newly connected behavior.

Architecture/risk documents contain unresolved or historical statements; phase
log P14's DONE label does not supersede AGENTS.md's caution about unreviewed
sequencer/kit/SysEx/sync logic. Sequencing, morphing, preset browser, UI and user
SD samples remain outside this preparation. No claim is made that P12c-R2 PASS
certifies original modulation completeness or all hardware sound behavior.

## 10. Source manifest

The audit's full byte manifest covers the existing tracked files plus relevant
new source/tests/docs. The production input hashes below are captured from the
current **uncommitted** tree. Upstream framework/SDK input sets are fixed by
their commit pins above; resources are hashed in section 4. Git line-ending
conversion can change byte hashes without a source-content change.

| Path | SHA-256 |
|---|---|
| `dsp/Cymbal.cpp` | `2273F21750E558EA22402CE666DF189DF27A96D28EEE75ABFA0321D1D33A224B` |
| `dsp/Cymbal.h` | `7B5A12154366A66B21C8322F06C148C49B66356C06D0724777C7395BDC4B8111` |
| `dsp/Decimator.cpp` | `9C1EC11E00AFDD1AAA0DD2FB925F3738A0520F96CA7FB152547A1CF848CC1811` |
| `dsp/Decimator.h` | `25419E50C311FC8441D6A7C1E8E9BB2AF6153E18928F351400D462F29C8C8E2A` |
| `dsp/Distortion.cpp` | `1106F3B7F2835EE60731FFC425AE146CF37C011A589426DE56C4B2A8D08F7859` |
| `dsp/Distortion.h` | `6D0A7355A7E15721070FCB1289FAA2F125176522552613BF5EE6EFF0049B05A4` |
| `dsp/DrumVoice.cpp` | `C15A1FE28E00600B8D77BFE3B52E329BB3749D508CADE6FC1A3FA399A4D472F1` |
| `dsp/DrumVoice.h` | `CDED629C8E4D75714049AB182873B364AB7EBCA217865475E1F2C9745047A2BF` |
| `dsp/Engine.cpp` | `F4408A11110BF0958FC73FDC73238C761FD5E264421F338BBC3CFEAAF053DE51` |
| `dsp/Engine.h` | `7ED5ED22483C80BDB5682B6C53A0FAAE594121ACB2773A88A656556E1CD346FB` |
| `dsp/Envelopes.cpp` | `5EABB63C2A2BE1F6F23A2B6AB8F255B0B9F9F4AD172180A4BBC69902999A2A2F` |
| `dsp/Envelopes.h` | `60103DDDB65B154FF49C7522429AEB097ED534E41EF5BC21193D114477265750` |
| `dsp/FIRLatency.h` | `0221E0572A25502B1438FDDB4F8D358AA0C913D3296B8E22C0BD51D120813FD6` |
| `dsp/FixedFifo.h` | `CBF735D5E3DA5058A14B7781906808F225C946268640A57B9E379E802E249633` |
| `dsp/HiHat.cpp` | `D880E9F74DFAE202077FA94A1DE60F06550309A16B2AEBB307CDFFE84B54E62F` |
| `dsp/HiHat.h` | `047E5C664DFB5117134F0F7F0A78F2A2869F267C8C3A5BB418454B62E3BC14AD` |
| `dsp/LXRAudioAdapter.cpp` | `2BC2FC29BC33B67DA7F729D3F7FF62E32E1828E6425BD4D8BC03D4612FA25A4D` |
| `dsp/LXRAudioAdapter.h` | `5AA843583F81E2D8FF8EFD94E994CCC2DE73021689E6B4894CF676AECF9BC807` |
| `dsp/LXRResourceLocator.cpp` | `59705C6F934AB2FE3F7ECB854F26A01ED074104C6DBC84492E605F3F6C19B996` |
| `dsp/LXRResourceLocator.h` | `5EE210B8437B29AC7A31B1D95C7EC0F5256633588252C1D348C104446755408A` |
| `dsp/Lfo.cpp` | `2CD701E97F650638E5A6F7F2A7FAFDCEED11CD68D475D68DC1FE2DB7B5E5D917` |
| `dsp/Lfo.h` | `B5AFA15DE0D98514C41455F2EEFB4F291A2999371C49133D3F126C005D0E2F77` |
| `dsp/MidiParser.cpp` | `B74BD691CCB4F1365484CFC7198215AD22AD4031305FC9BEB3447A5F538B1A59` |
| `dsp/MidiParser.h` | `647E51FEDA7C7F90C58E51A341AC8D56E4340EBB45D68800E99B95686C4E7562` |
| `dsp/Mixer.cpp` | `57EC6177BC1FD1DCF22842F9EB3F69623FC05CB57883E96FE3A4B47B39B7A189` |
| `dsp/Mixer.h` | `96951C2235EAA17E300196EDC3B6D638903DBCBABD248AC398A9C48B5C7A8F2C` |
| `dsp/Modulation.cpp` | `2A72D0871F91E5778B84F4B7C9371EA84AD8AE35AF96961AB8184D27ED496C35` |
| `dsp/Modulation.h` | `EA0E65074D0F2EB0F179356AC501BEF6D8AB6B879110DE6AB0CE36AE560B53AD` |
| `dsp/OscTables.cpp` | `981FD8684FC408BFE1D953D228F21A2ECBEA72FBF2925086DA7085F187EC91F1` |
| `dsp/OscTables.h` | `207A6E04172B0BFCCFD701E9BBA2DD7D824F4B7AD51C68BB6DE5E2F3C4FE6FE6` |
| `dsp/Oscillator.cpp` | `656E4B7A2ECC7A7394CDF182226D79FB2697932D5E7B8C1802E41B3E852F75E3` |
| `dsp/Oscillator.h` | `247476BCB9FFE70DBBB9CEECE4CA7CE56A0AC04C317C469745E3FA9D904F985A` |
| `dsp/ParamMapEnv.cpp` | `BBC0C2A97E4A78DD5FB864AF4D03CEB948E8566FD31C8D87FFEDF8FB28BDCE35` |
| `dsp/ParamMapEnv.h` | `329776DFD3B2ADF573DCD326C256673CA2096F22A9CA5D1A3DE3B7D967D6B276` |
| `dsp/ParamMapMisc.cpp` | `FD9CF511F6C91AAFD5393032E86319A70B07368372F344DBA0D347A8981E8D08` |
| `dsp/ParamMapMisc.h` | `6E50544CDF4521FE2CC2C715BC75EF561AE4494F0E23BD29FCB9B353E7588DF5` |
| `dsp/ParameterArray.cpp` | `2EEC4E988EB1AF0F3F885D46C0D0306957800D9427745664E8DB30E3465CB251` |
| `dsp/ParameterArray.h` | `B88ACAF8531655A66A95A8133AB8C42941A61CEA766A20E990C0924FC586B3ED` |
| `dsp/ParameterStateAuthority.h` | `0CC2653623205D5D1EFF4449771AB34E363CC02FFB6442F73F3E4CB8EFF5ED67` |
| `dsp/PolyphaseResampler.h` | `10A7741B31CC2897A02EB57249813B2497D65A506879C17B7155C732CCD16A5B` |
| `dsp/ResonantFilter.cpp` | `6D2187F009A697C90AAAB3321E693E90D0560CA321F65CE9663C286FA8A03885` |
| `dsp/ResonantFilter.h` | `87918237A3387D66A1470E731C34C3FC900E3768A087A5F9220021A2DC0D453A` |
| `dsp/Snare.cpp` | `778746CEC2BC24538E0F6CD8F29DD38D8C790F1CFBF9FF9EA4EE5791EF133593` |
| `dsp/Snare.h` | `E130418E0F4A6682FE5370D5D1BD8668EE274932B49BC5BE1D82397038AA3476` |
| `dsp/TransientGen.cpp` | `2F0FCCD75CF5917B37129F5629B00E8D5690E30B1E895693B713D61566AF7D78` |
| `dsp/TransientGen.h` | `A8797324AAF61FDE819FD5E0F602FBC0BBECE9183D7A8801106A3976B66568FC` |
| `dsp/TransientTables.cpp` | `D07EA6BADE1D8D925126BFB39ED8524248AA7FE48B91CB3BDD313EF6A1C4759D` |
| `dsp/TransientTables.h` | `26FA55FFA9D394BA6620887C3C29D42084EB92E1A00BF35300B06A6E5152DC2A` |
| `vendor/iPlug2/Examples/LXR/LXR.cpp` | `29AF0B72A411B1B796B71AC766D776F3F0133680E90B1F3772C9A6679765F161` |
| `vendor/iPlug2/Examples/LXR/LXR.h` | `17831A714B9E3667640A99745239C63B7E9B2B4181D5002830C9C31BDA82F29D` |
| `vendor/iPlug2/Examples/LXR/LXRParamContract.h` | `E51C5062D864B439C775F52B70BFB2061317D7990897E3DD364F4F56E2C07EAD` |
| `vendor/iPlug2/Examples/LXR/LXRState.h` | `6C73C5E69D8B595812B845B05CCB7A746F9678BBAA57877F96AA446ECEF78EA7` |
| `vendor/iPlug2/Examples/LXR/config.h` | `C3C2318B4F0D7795F47BE565C86DC7F0B1CB88406825563C0C5103F281017692` |
| `vendor/iPlug2/Examples/LXR/config/LXR-win.props` | `226105E39B1CCF9F3FE094CDDC067DCA60D52D5184298D4F2F689EC7C73F0704` |
| `vendor/iPlug2/Examples/LXR/projects/LXR-vst3.vcxproj` | `5DD4FF5FAC2A06F612388E677F115C1504D6938F44BE79AF0B13FDC4C535AA0D` |
| `vendor/iPlug2/Examples/LXR/resources/LXR.ico` | `AC4D43ADC17341179169762A62828AF644F5A6463F669D2C0A1EF58D4A602BC5` |
| `vendor/iPlug2/Examples/LXR/resources/fonts/Roboto-Regular.ttf` | `B2EFABCA5EA4BC56EEA829713706B5CD0788B82ACA153BD4ADDE9B1573933B4F` |
| `vendor/iPlug2/Examples/LXR/resources/main.rc` | `EB27073D3E43477C50DF6AA53F744E8C5C77993B87E23375F8A7CE473ECF6837` |
| `vendor/iPlug2/Examples/LXR/resources/resource.h` | `663D09CB807FC85D82FEC5ECBFAC383F809EC847883E40BF5D3418C172A033F5` |
| `vendor/iPlug2/Examples/LXR/scripts/package-resources.ps1` | `DFBC367A4C48566D3771C091C84B86B2B9BBA5CF18AB853B37073CEB4DCAF905` |
| `vendor/iPlug2/Examples/LXR/scripts/postbuild-win.bat` | `E1E4FA745EEDF11045E0BCC092A120A270003CEE81C94EBE9B210EE50FF84713` |
| `vendor/iPlug2/Examples/LXR/scripts/prebuild-win.bat` | `D946D20B0ECED9527D1E7199B58A5DDBE1930B7B59B60220B6467EF633BF7C6A` |
| `vendor/iPlug2/IPlug/VST3/IPlugVST3_Common.h` | `4AD9986250B87869160A5D49F0832FCD140F134026A8E99BF0EB00AD575A654C` |
