# Portable checkpoint builds and verification

P12c-R3 procedure. No command below stages Git changes, installs a VST3, deletes
outputs or changes Windows policy. Use a **new output directory** for each run.
The Python scripts resolve the repository from their own location; they can be
invoked from another working directory. Relative command-line paths are relative
to the invoking shell, so use absolute arguments when running elsewhere.

## Dependencies and prerequisites

- Python 3.10 or newer, standard library only.
- Native tests: GCC/G++ with C++17 and working `std::thread`; verified with
  MSYS2 UCRT64 GCC/G++ 16.1.0 on Windows x64. Both compiler executables must be
  available via PATH, `CC`/`CXX`, or `--cc`/`--cxx` (executable paths, not flags).
  The runner adds the G++ directory to the test process PATH for MinGW DLLs.
- VST3 host tests: Windows x64, Visual Studio C++ tools and CMake. Verified with
  VS Community 18, MSVC tools directory 14.51.36231, compiler file version
  19.51.36260.0, v145, Windows SDK 10.0.28000.0, VS generator
  `Visual Studio 18 2026`, and bundled CMake 4.3.1. `vswhere` discovers VS;
  `--vs-root`, `--tools-version`, `--toolset`, `--generator`, and `--cmake` allow
  explicit alternatives. Alternatives are not automatically certified.
- Path tests: Lua 5.4+ via `--lua` or `LUA`. The audit built Lua 5.4.8 from
  `https://www.lua.org/ftp/lua-5.4.8.tar.gz` into ignored output using GCC.
  That tarball SHA-256 was
  `4F18DDAE154E793E46EEAB727C59EF1C0C0C2B744E7B94219710D76F530629AE`.
  Lua is a verification tool, not a plugin dependency.
- Runtime source assets: the tracked `data/` directory, including the seven
  package resources hashed in `p12c_r2_certified_baseline.md`. Do not omit these
  when copying a source tree.
- Original-C comparisons: SonicPotions/LXR firmware at
  `dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`. Use this source, never Catalyst.
  Without it, the complete native procedure is unavailable; do not claim a
  fresh-clone test PASS when this prerequisite is absent.

Exact dependency pins and local patch identity are maintained in
`tools/verification/dependencies.json`. iPlug2 is
`d54f69050f517e43b941d88c2a170f0a840b9ee4`; SDK root is
`3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`, with every SDK submodule explicitly
pinned in that manifest. The later d82b iPlug2 commit was only a historical
comparison revision.

## Acquire or verify source dependencies

From the repository root, after the source checkpoint is available:

```powershell
python tools/verification/prepare_dependencies.py --firmware
python tools/verification/prepare_dependencies.py --firmware --verify-only
```

The first command downloads immutable commit archives into `out/dependency-cache`
and copies **only absent files**. It rejects differing existing files, preserves
the tracked LXR overlay and state-reader patch, verifies the iPlug2 archive hash,
and does not use Git checkout/submodule operations. Existing clean checkouts can
be verified in place. `--verify-only` uses cached archives without network or
source writes. `--root` and `--cache` support isolated destinations/caches.
For an empty destination, first materialize the project's source-controlled
overlay, including `vendor/iPlug2/IPlug/VST3/IPlugVST3_Common.h`; acquisition
requires its exact patched-header hash rather than silently using upstream.
The manifest records the certified raw header hash and a canonical LF hash;
patch verification accepts CRLF/LF checkouts with identical source content. Binary
resource identity remains a byte-hash contract.

Downloaded firmware without `.git` gets a generated source receipt. The test
runner requires either a clean Git checkout at the firmware pin or that receipt;
run archive verification to verify its contents, not merely the receipt's label.
All acquisition receipts are ignored build metadata. No topology conversion is
needed. The only shared iPlug2 modification is the narrow SetState failure-path
patch; the SDK README replaces an upstream placeholder as a dependency overlay.

Keep fetched upstream licences/notices. Original LXR code and exported assets
carry the existing custom non-commercial/full-source redistribution conditions
in `LICENSE-LXR.txt`. Acquiring ignored dependencies does not waive those terms.
No new production library or licence dependency was introduced.

## Rebuild and run the complete supported source matrix

```powershell
python tools/verification/run_checkpoint_tests.py --suite all --output "out/checkpoint verification 001"
```

Or select `--suite native` / `--suite host`. An existing output directory is
refused. Missing compilers, firmware, SDK, VS tools or plugin produce clear
nonzero errors. Build failure returns 2; failed executed tests return 1.
`tools/run_tests.bat` is an optional script-relative native entry point and now
propagates failures. `LXR_PYTHON` selects its Python executable.

Overrides:

| Input | CLI / environment |
|---|---|
| Repository sources | `--root` (default: runner's repository) |
| GCC/G++ | `--cc`, `--cxx` / `CC`, `CXX` |
| Original firmware checkout | `--firmware` / `LXR_FIRMWARE_ROOT` |
| iPlug2 source root | `--iplug` / `LXR_IPLUG2_ROOT` |
| VST3 SDK source root | `--sdk` / `LXR_VST3_SDK` |
| Plugin executable inside bundle | `--plugin` / `LXR_VST3_BINARY` |
| VS/CMake | `--vs-root`, `--cmake` / `CMAKE` |

Without an explicit plugin argument, the Windows host tests use
`%LOCALAPPDATA%/Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3` and
require it to exist. The runner never creates or replaces that binary. For
another engineer without the certified installation, use a separately built
private bundle and report **that bundle's actual hash**, not the certified hash.

The runner snapshots DSP/test/asset inputs into its new output tree, copies
original reference sources there, builds new object files, runs native tests
with that source tree as their data-relative working directory, and records
commands, logs, hashes, results and tool versions. It does not reuse old test
executables, `memorystream.obj`, hosting libraries or validator binaries.

### Explicit manifest, flags and fixtures

`tools/verification/test_manifest.json` is the explicit source/test list. It
includes the 22 non-test DSP translation units, 13 core DSP test programs, six
additional native programs, original-C snare/filter comparisons, and the real
VST3 host/controller tests. No new native build system was introduced.

| Part | Build contract |
|---|---|
| Native C++ objects and tests | `-std=c++17 -g -O0` or `-O2`, `-fno-exceptions -fno-rtti -ffp-contract=off -Wall -Wextra -pthread`, `-I<snapshot>/dsp` |
| Legacy DrumVoice test translation unit | Additional `-fexceptions` because its existing fixture has `catch (...)`; DSP objects remain exception-disabled and test assertions are unchanged |
| Firmware C | Compile as C, `-std=gnu11 -fgnu89-inline -O0` or `-O2`, `-ffp-contract=off -Wall -Wextra`; do not compile unchanged C as C++ |
| Snare C oracle | `tests/reference/snare/original_snare_harness.c` and its seven tracked harness/compatibility files; directly includes unchanged firmware C and uses deterministic test RNG |
| Filter C oracle | Pinned `DSPAudio/ResonantFilter.c`, original headers, and four tracked headers under `tests/reference/filter/compat/`; no old `out/` stub is needed |
| VST3 host executables | MSVC C++17 `/O2 /MD /GR- /EHs-c- /DNDEBUG`, SDK include root; compile fresh SDK MemoryStream object |
| Hosting libraries | `sdk_hosting`, `sdk_common`, `sdk`, `base`, `pluginterfaces`, plus Windows `ole32`, `uuid`, `user32` |
| SDK/validator | Reuse the SDK's CMake build: Release/x64/v145, VSTGUI/plugin examples off, hosting examples on, plugin linking off; build `validator`, `sdk_hosting`, and `sdk` explicitly |
| Internal controller probe | Same source with `LXR_R2_INTERNAL_PROBE`, VST3/editor/DSP/NanoVG/GL2 and Windows definitions; explicit iPlug2 include roots are recorded in the runner |

The probe's disposable `LXR.h` copy changes only its five-parent DSP include
prefix to four parents. The production project normally supplies its `projects/`
include context. A standalone copied test has a different include stack and can
otherwise accidentally import headers outside the snapshot. This **test-only
include adaptation** is logged with original/adapted hashes; class declarations,
member layout and all repository production files remain unchanged. It is not a
plugin runtime patch or test-only field/API injection.

The portable native latency suite has **214** self-contained assertions. The
historical 220 executable has six extra comparisons against a pre-task FIR
snapshot. That optional audit fixture stays excluded; no generated fixture was
added or necessary for the 214 portable assertions. Report these counts
separately.

The supported matrix is 42 native suite executions (21 each at O0/O2) plus five
Windows host/validator executions: **47 source-built suites**. This is distinct
from the historical mandatory 30-suite executable matrix. The freshly compiled
runtime source has 1143 assertions; its preserved older executable has 1101.
Both were run and passed without changing the runtime test source.

## REAPER helper path configuration and safe verification

```powershell
python tools/verification/test_portable_paths.py --lua "<Lua executable>" --output "out/path verification 001" --plugin "<VST3 executable>"
```

`--plugin` additionally tests generation of historical Lua variants without
launching REAPER. The path tests otherwise use a fake path fixture which is
never loaded as a DLL. They cover syntax, copied repositories/output paths with
spaces, script-relative discovery outside the working directory, wrappers,
missing dependencies, output refusal, historical path remapping and generator
syntax. R3 acceptance: 32 checks pass. Lua does not need to be installed globally.

`tools/reaper/checkpoint_paths.lua` is test-harness-only configuration:

- `LXR_REPO_ROOT`: optional repository override; normally inferred from helper
  location, with an `AGENTS.md` existence check.
- `LXR_REAPER_HELPERS`: helper directory override for generated/relocated Lua.
- `LXR_VST3_BINARY`: explicit executable inside its resource-bearing bundle;
  otherwise the standard per-user Windows installation is discovered.
- `LXR_TEST_OUTPUT`: explicit output root. Existing artifact refusal remains.
- `LXR_TEST_PROJECT`: optional exact saved-project path for transport probes;
  the probe still refuses a different active project.
- `LXR_EXPECTED_PLUGIN_SHA256`: explicit expected-hash override for deliberate
  variant generation; default historical hash guards remain historical.

The Python R1 driver additionally supports `LXR_TEST_EXECUTABLES` and
`LXR_VALIDATOR`. Its default R1 hash and historical labels are retained; it is
not the current source-build driver. The artifact analyzer supports
`LXR_EVIDENCE_DIR` and JSON `LXR_EVIDENCE_PATH_MAP` (old prefix -> current prefix),
allowing historical report paths to be resolved without rewriting their facts.
Historical native-host ABI probes need the same pinned headers as their binary.

Only run actual REAPER harnesses in new isolated empty projects and new output
locations. Path regressions do not run REAPER or prove fresh DAW behavior.
Changing an environment variable does not prove a loaded module's identity;
retain actual module/hash verification for any new DAW certification.

## Production build configuration and byte reproducibility

Keep the certified production project settings: Release x64, v145 override,
MSVC C++17 `/O2 /Oi /GL /MT /fp:precise /EHsc`, inherited RTTI enabled, and
`/LTCG:incremental /OPT:REF /OPT:ICF /DEBUG:FULL`. The repository's no-exception/
RTTI design rule continues for native DSP implementation. R3 explicitly retains
the proven third-party framework build configuration; active iPlug2 casts require
RTTI. A later flag change needs a scoped change and recertification.

For a future non-installing build, discover MSBuild rather than hard-code a VS
edition path. This example is explicitly run from the source repository root:

```powershell
$sourceRoot = (Resolve-Path .).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$msbuild = Join-Path $vsRoot 'MSBuild/Current/Bin/MSBuild.exe'
$shellRoot = Join-Path $sourceRoot 'vendor/iPlug2/Examples/LXR/'
$outputRoot = Join-Path $sourceRoot 'out/private-plugin-build-001'
New-Item -ItemType Directory -Path $outputRoot -ErrorAction Stop | Out-Null
& $msbuild (Join-Path $shellRoot 'projects/LXR-vst3.vcxproj') /t:Build /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 "/p:SolutionDir=$shellRoot" "/p:BUILD_DIR=$outputRoot" "/p:OutDir=$outputRoot/bin/" "/p:IntDir=$outputRoot/int/" "/p:PDB_FILE=$outputRoot/pdb/LXR.pdb" "/p:BaseIntermediateOutputPath=$outputRoot/base-int/" "/p:MSBuildProjectExtensionsPath=$outputRoot/extensions/" /p:PostBuildEventUseInBuild=false /p:PreBuildEventUseInBuild=false /v:minimal
if ($LASTEXITCODE -ne 0) { throw 'Private plugin build failed' }
```

Package its DLL into a **new** private bundle's `Contents/x86_64-win/LXR.vst3`
and copy the seven named assets from `data/` to `Contents/Resources/LXR/`.
The normal post-build deploy script is deliberately disabled. Supply the private
executable to the host runner via `--plugin`; never overwrite the certified
installation for repository verification. The existing LXR CMakeLists is an
unconverted IPlugEffect template, so it is not the production recipe. The SDK's
CMake build is used only where it is already the supported dependency mechanism.

Source and functional reproducibility are demonstrated on this Windows host,
including an independently acquired source tree. No repository policy requires
byte-identical PE files. The preserved certified hash remains the installed
binary's identity, while prior clean-build hashes differ because of timestamps,
debug paths and linker data layout. Prior original-path clean-build disassembly
matches symbolically at 94549 instruction addresses; its 23 changed code bytes
are references to relocated exception/CRT records. Full bitwise reproduction is
not claimed. No plugin binary was rebuilt or installed during R3.
