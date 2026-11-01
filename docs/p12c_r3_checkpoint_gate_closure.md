# P12c-R3 checkpoint gate closure

Date: 2026-10-31. **READY for separate authorization of the bounded Git
checkpoint sequence.** P12c-R2 functional certification remains PASS. READY
does not mean staged, committed, pushed, or permission to begin another feature.
There is no new plugin release or replacement certified binary in R3.

## Starting state and preservation

| Item | Observed |
|---|---|
| Branch | `p12b-4c-fir-resampler` |
| HEAD and upstream commit | `280279debe94cb26810ea89b89f36b9bf965a747` |
| Upstream | `origin/p12b-4c-fir-resampler`; no ahead/behind difference |
| Tracked paths | 222 |
| Initial tracked modifications | 10 |
| Initial untracked files | 4655 |
| Initial ignored files | 63447, including prior audit outputs |
| Initial index changes | none |
| SDK checkout | clean at `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96` with recorded pinned submodules |
| Installed executable | 729600 bytes; SHA-256 below |

Certified installed VST3 SHA-256, unchanged:

`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`

Starting inventory, source hashes, and byte-preserved copies of relevant
pre-existing files are under `out/p12c-r3-20261031-142231/initial-inventory.json`,
`input-manifest.json`, and `originals/`. This includes the previous baseline
document before its required revision. Every pre-existing change was preserved.
No production DSP, plugin class, codec, authority, MIDI mapping, FIR equation,
resource asset or production project setting changed. No evidence was deleted,
no security policy changed, and no REAPER process was launched or terminated.

## Gate A — path portability: CLOSED

`tools/reaper/checkpoint_paths.lua` now discovers its repository from its own
location, checks dependencies, and supports explicit repository/helper/bundle/
output/project overrides. The historical Python driver supports plugin,
executable and validator overrides. The analyzer supports an evidence-directory
override and explicit historical-prefix remapping. Paths with spaces are
supported. Existing empty-project, exact saved-project and no-overwrite guards
remain; path tests never load the fake path fixture as a plugin.

Classification of absolute-path observations:

| Kind | Disposition |
|---|---|
| Active executable assumption | Removed from intended REAPER helpers; batch entry point is script-relative, tools are discovered or explicitly configured |
| Historical evidence path | Retained verbatim in reports; these describe the actual machine on which historical tests ran, not prerequisites for future execution |
| Illustrative example | The existing repository-path comment in `LXR-win.props` is non-executable and unchanged |
| Required platform-specific path | Windows per-user VST3 and VS/SDK locations are discovered through environment/VS tooling, with explicit overrides |
| Unrelated local artifact | Raw `out/`, editor, backup, dependency and build artifacts remain excluded; no bulk replacement was made |

The intended-file scan found 12 remaining literal historical path occurrences
in engineering reports (including the unchanged older project report) and one
non-executable build-property comment. The per-occurrence inventory is
`absolute-path-classification.json`. No intended helper has a literal username,
repository root, REAPER executable path or VS edition path as an active
assumption. Historical report paths need not be redacted to pass this gate;
the R3 instruction explicitly preserves their factual evidence.

Fresh portable-path acceptance: **32/32 PASS**. Tests use a copied repository
and outputs containing spaces, a different working directory, Lua syntax checks,
wrapper interception, missing-dependency diagnostics, existing-output refusal,
historical path remapping and Python variant generation. Generated Lua is parsed
without invoking REAPER. The R1 driver's default historical hash/labels remain
R1; configuration overrides do not relabel historical evidence as a current run.

## Gate B — source-test rebuilding: CLOSED

The supported procedure is documented in [build_and_test.md](build_and_test.md)
and implemented by an explicit JSON source manifest plus Python runner. It
reuses the existing GCC approach for native code and the SDK's own CMake build
for hosting libraries/validator. The LXR CMakeLists is an unconverted example
template; it was inspected and is not misrepresented as a working production
recipe. No replacement native build system was introduced.

Four small human-authored filter compatibility headers replace a missing local
stub prerequisite. The existing seven-file snare harness/compatibility layer is
included in the checkpoint. Firmware C is compiled as GNU11 C with GNU89 inline
semantics, strict contraction control and the original source; C++ is C++17.
Sources, include paths, definitions, optimization/warning flags, asset and
firmware requirements, linker libraries and tool discovery are explicit.

The final single `--suite all` run used an independently acquired source tree
with spaces in its path, no old objects, hosting libraries, MemoryStream object,
validator or test executables. It passed **47/47 source-built suite executions**:
42 native executions at O0/O2 and five Windows host/validator executions.

Standalone probe compilation required an include-context adaptation in its
disposable LXR header copy. Only the five-parent DSP include prefix is changed
to four parents, avoiding unintended header lookup outside the snapshot; class
declarations/layout and repository production headers are unchanged. Original
and adapted hashes are recorded. This is explicitly a test-build adaptation,
not a new production binary or runtime change.

The portable FIR latency source suite has **214** assertions. The preserved
historical 220-assertion executable also passed on rerun; its six extra
historical-snapshot checks are not claimed for the portable source build.
Likewise the freshly compiled runtime test has **1143** assertions while the
preserved matrix executable has **1101**. Both pass; test source was not changed.

All nine exported binary tables were regenerated from the independently
acquired pinned firmware into a new ignored directory and match tracked asset
hashes exactly. `data/` is still an explicit source prerequisite; absence of
assets/reference sources is an error, not a fresh-clone PASS.

Early runner-development attempts exposed missing fixture exception support,
the filter `globals.h` stub, Windows command quoting, an omitted explicit SDK
target, and the copied probe's include context. Those logs are preserved. These
were infrastructure setup errors, corrected before final acceptance; no
production test assertion failed and no assertion was removed or relaxed.

## Gate C — ignore rules: CLOSED

Applied the previously reviewed narrow root ignore additions: validator/build
reports, Python caches, unrelated downloaded iPlug2 content and LXR backups.
Explicit exceptions retain the LXR overlay and the single state-reader patch.
Existing tracked files remain tracked. No broad source or fixture exclusion was
introduced and no artifact was deleted.

All **70 exact checkpoint paths** pass `git check-ignore --no-index` eligibility
checks, including both LXR headers, every test manifest/compatibility header,
resource packager and engineering document. Representative dependency/build/
backup/cache paths are correctly ignored. Verification is in
`ignore-verification.json`. No generated binaries, test renders or reports are
in the proposed checkpoint manifest.

## Gate D — exception/RTTI policy: RETAIN, CLOSED

**RETAIN the certified production settings.** This is an explicit preservation
decision under R3, not a silent compiler change. The project inherits `/EHsc`
and enabled RTTI, uses C++17, and needs its existing v145 override. DSP/native
objects are separately built with exceptions and RTTI disabled.

Active framework paths use `dynamic_cast`, including iPlug parameter shapes,
VST3 state/controller restoration and NanoVG bitmap handling. A controlled
compile of unchanged `IPlugParameter.cpp` with its required `NOMINMAX` definition
passes with `/GR /EHsc`; `/GR- /we4541` fails with C4541 warnings promoted to
errors at its active shape casts. These diagnostics specifically warn of
unpredictable behavior with RTTI disabled. Keeping RTTI is supported by both
source and compilation evidence.

The same file compiles with `/GR /EHs-c-`; that single-file result does not
establish a supported exception-disabled build for the complete STL/framework/
SDK stack. No concrete exception-related defect or portability requirement was
found. Existing functional tests and source/build provenance support retaining
the certified configuration. SDK validator utilities also contain exception
handling. Compile success alone is not a proof of whole-plugin real-time safety.

The legacy `DrumVoiceTest` fixture contains `catch (...)`; only that test
translation unit has a documented `-fexceptions` override. Its DSP objects
remain exception-disabled. No new throws, catches or RTTI use were introduced
into DSP implementation. The repository design rule remains the rule for native
DSP; the inherited third-party shell configuration is a documented preservation
exception. A later change to production flags requires a scoped investigation
and full relevant recertification, not this checkpoint.

`flag experiments final/results.json` records the controlled comparisons. The
earlier experiment missing `NOMINMAX` was retained and is not cited as proof
that the retained configuration fails.

## Gate E — provenance/reproducibility: CLOSED WITH EXPLICIT LIMITS

| Property | Classification |
|---|---|
| Source reproducibility | Demonstrated on this host: immutable dependencies reconstructed in an empty isolated destination, overlay/patch preserved, source tests rebuilt, assets re-exported; prior independent plugin source builds retained |
| Functional reproducibility | Demonstrated for the rebuilt source-test matrix and preserved certified binary; prior independent rebuilt plugin host-suite results retained |
| Byte-identical VST3 reproduction | NOT ESTABLISHED; not a checkpoint requirement in repository policy |

No bitwise-PE requirement was found in AGENTS.md or the engineering policies.
Original-C DSP comparisons remain strict; that numerical fidelity requirement
does not imply identical Windows DLL metadata or link layout. R3 does not make
binary identity an arbitrary blocker for committing functionally certified source.

The certified installed hash remains the authority for that installed artifact.
The earlier audit's independent clean build was
`5C5E3EF40BC31F79105B12175D5C78878F0B1E337C9C71A00061DD7499B6F3BE`;
its original-source-path clean build was
`590F778687C6AE9F23F03A4454BE07404FCB30E1E5761879B278841A9CCEFD9E`.
Those are prior audit builds, not new R3 plugin builds or replacement certified
hashes. Symbol-resolved disassembly of the latter matches all 94549 parsed
instruction addresses; its 23 changed code bytes are address references to the
same relocated exception/CRT records. Timestamps, absolute debug paths,
incremental LTCG/IPDB state and data/unwind layout remain known non-bitwise
inputs. Full byte-for-byte attribution remains unclaimed.

The previous baseline's 63-row production manifest, seven resource hashes,
toolchain/flags and original build logs remain valid because production inputs
were not modified in R3. New runner source snapshots, command logs, dependency
receipts and test identities make the verification procedure auditable.

## Dependency strategy

Keep the current vendor layout with pinned immutable archive acquisition and
one explicit patched upstream header. No submodule conversion, vendor rewrite
or external dependency topology decision is required. `dependencies.json`
contains iPlug2, SDK root/all seven submodules, firmware and patched-header pins.
`prepare_dependencies.py` rejects differing existing files and never replaces
the LXR overlay/patch. Full acquisition and offline cached verification both
passed in `acquired dependencies with spaces/`.
The patch pin records both the certified raw byte hash and a canonical LF hash.
Offline verification also passes with an LF-only isolated patch copy, so Git
line-ending settings do not turn an identical source checkout into a false
dependency mismatch. The repository's actual patched header was not rewritten.

The required iPlug2 revision is
`d54f69050f517e43b941d88c2a170f0a840b9ee4`. The full comparison covers 3552
upstream files: 3550 acquired/verified by the script plus the separately pinned
state-reader header and intentionally skipped SDK placeholder README. The latter
is replaced by the pinned SDK's README. No unrelated local framework change is
required. Licences and attribution remain with fetched source; LXR remains
non-commercial with the existing full-source redistribution obligation.

## Verification evidence

All paths below are relative to `out/p12c-r3-20261031-142231/`; these artifacts
are preserved locally and excluded from commits.

| Evidence | Result |
|---|---|
| `isolated complete acceptance with spaces/results.json` | 47/47 newly source-built suites PASS |
| `final portable UTF8 acceptance/results.json` | 32/32 path checks PASS; no REAPER launch |
| `preserved-matrix-rerun/results.json` | 30/30 fresh executions of unchanged certified test executables PASS |
| `preserved-matrix-rerun/reaper-artifact-analysis.json` | Read-only re-analysis of original R2 state/MIDI/audio/PDC artifacts PASS |
| `asset-reexport-comparison.json` | 9/9 exported binary tables hash-identical |
| `ignore-verification.json` | 70 required paths eligible; disposable examples excluded |
| `dependency-cache/receipts-*.json` | Exact source revisions, downloaded archive hashes and verified-file counts |
| `flag experiments final/results.json` | Retained RTTI compile PASS; disabling RTTI rejected; exception-disabled single-file compile qualified above |

The rebuilt suites include codec 29/29 O0/O2, concurrency 35/35 O0/O2, native
synthesis 419/419 O0/O2, snare differential 61/61 O0/O2, filter differential
80000 blocks/zero mismatches at both levels, FIR quality 38/38 O0/O2, portable
latency 214/214 O0/O2, MIDI 5/5, mixer 28/28, filter/transient at both levels,
installed state 322/322, freshly compiled runtime 1143/1143, external controller
955/955, probe 1033/1033 and freshly built validator 47/47. The preserved matrix
separately retains original-controller 36/36 and historical latency 220/220.

The REAPER artifact analysis verifies 456-slot state and MIDI checkpoints,
CC88/NRPN1 values 77/45, CRC-valid saved states, exact deterministic tonal PCM,
and 26-sample PDC with zero aligned error. This is **not a fresh REAPER run**.
TSan, whole-DLL heap instrumentation, hardware RNG equivalence and literal GUI
refresh inspection remain unclaimed optional limitations.

## Revised bounded checkpoint plan

The authoritative exact path list is
`tools/verification/checkpoint_files.json`; the baseline document renders its
ordered staging commands. **No command in that plan was executed.** All changes
are staged as complete files after authorization; no interactive hunk staging is
necessary.

1. Pin framework inputs, the required state-reader patch and the missing shell
   descriptor header. The patch is isolated by exact path/hunk from upstream;
   it shares this foundation commit because acquisition verifies it and HEAD
   itself needs the descriptor header. This avoids an incomplete bootstrap.
2. Preserve startup defaults and native diagnostics.
3. Restore snare renderer/transient loading/packaging plus its original-C harness.
   This depends on the startup defaults used by its integration assertions.
4. Commit intertwined FIR lifecycle, codec, authority/FIFO, MIDI routing and R2
   controller class changes together. Do not split class declaration/definition
   or config state-chunk/latency hunks into unbuildable intermediate states.
5. Commit portable source-test infrastructure, filter compatibility and REAPER
   helper configuration.
6. Commit finalized engineering history, provenance and gate-closure documents.

The A-H hunk audit from R2 remains applicable. MIDI status composition resides in
the same final class file as state/controller integration; its placement in
commit 4 is intentional. Historical reports retain historical FAIL/BLOCKED
labels and machine paths; current R2 PASS and R3 readiness supersede those
earlier states without rewriting them.

## Final state, remaining risks and authorization

The final bounded inventory and changed-file list are recorded below after
verification. Raw dependency/build output, RPP/WAV renders, caches, binaries,
objects, logs, backups and temporary reports remain excluded. No required source
or human-authored compatibility file is hidden by ignore rules.

No technical checkpoint gate remains open. Byte-identical DLL reproduction and
another-platform validation remain explicit limitations, not claimed passes or
requirements for this source checkpoint. Production compiler settings are an
explicit RETAIN decision, not deferred without disposition. Third-party
framework stream-error branch unit coverage remains a possible later improvement;
current malformed-state integration and validator checks pass.

**Checkpoint execution can now be separately authorized.** This task does not
authorize staging, committing, pushing, merging or switching branches. No such
operation was performed. There is no remaining topology approval gate.

P15-M1 is ready for separately scoped contract review after checkpoint execution:
the original destination mapping/no-op gaps and CC/NRPN convention discrepancy
remain documented in the baseline. R3 implements none of that work and does not
certify modulation completeness. P15-M1, P16, sequencing, morphing, presets and
GUI work have not begun.


## Final inventory and exact R3 edits

Final source-control inventory: **13 tracked modifications, 57 untracked files,
70 distinct proposed checkpoint paths, 86796 ignored files at final capture, empty Git index**. All 70 status paths
match the bounded checkpoint manifest exactly; the large dependency/build trees
are now correctly ignored. The historical 222 tracked paths are unchanged in
number because no staging/commit occurred.

R3 changed 18 pre-existing eligible source/documentation files and created 13
new eligible files (**31** R3 edits listed below). The remaining proposed paths
are pre-existing development work being checkpointed, not new R3 implementation.
Python also regenerated an ignored bytecode cache during verification; its
pre-existing bytes are preserved in the originals archive and it is not staged.

```text
.gitignore
docs/build_and_test.md
docs/p12c_r2_certified_baseline.md
docs/p12c_r3_checkpoint_gate_closure.md
docs/phase_log.md
tests/reference/filter/README.md
tests/reference/filter/compat/config.h
tests/reference/filter/compat/datatypes.h
tests/reference/filter/compat/globals.h
tests/reference/filter/compat/stm32f4xx.h
tools/reaper/checkpoint_paths.lua
tools/reaper/p12b4c_pdc_test.lua
tools/reaper/p12b4c_snare_other_voice.lua
tools/reaper/p12b4c_snare_other_voice_final.lua
tools/reaper/p12b4c_snare_reaper.lua
tools/reaper/p12b4c_snare_reaper_final.lua
tools/reaper/p12b4c_snare_transport_probe.lua
tools/reaper/p12b4c_snare_transport_probe_final.lua
tools/reaper/p12b4c_transport_probe.lua
tools/reaper/p12c_fir_pdc_regression.lua
tools/reaper/p12c_r1_analyze_certification.py
tools/reaper/p12c_r1_final_certification.py
tools/reaper/p12c_r1_state_test.lua
tools/reaper/p12c_state_test.lua
tools/run_tests.bat
tools/verification/checkpoint_files.json
tools/verification/dependencies.json
tools/verification/prepare_dependencies.py
tools/verification/run_checkpoint_tests.py
tools/verification/test_manifest.json
tools/verification/test_portable_paths.py
```
