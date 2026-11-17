# P17-C original whole-kit .snd codec

Implementation engineering acceptance: **PASS**. Development publication is
recorded in `out/p17c/publication_receipt.json` after the reviewed commit/push.
P16-F release certification remains **BLOCKED**. This phase supplies isolated
byte codecs and non-real-time Windows file helpers, with no live kit loading,
Engine/authority/host/GUI integration or VST3 rebuild/install.

Baseline branch `p12b-4c-fir-resampler`, local/live HEAD
`89c5eb61e89fa238b803aa94e83425cbce2f7c43`, origin
`https://github.com/vlndv/lxr-vst3.git`. Initial index/tree clean. All480 source
hashes,234 P17-B evidence hashes, both P17-A/B commits, protected DLL/seven
resources and pinned firmware/dependencies verified. Original SonicPotions/LXR
`dee49689f3f0cdf8f9b7b5e79628cfdca1b79394` is authoritative; Catalyst was not used.
Derived code/fixtures retain the copyright and custom non-commercial/full-source
licence in `LICENSE-LXR.txt`. No factory assets or new dependency were acquired.

## Exact original format

Pinned `front/LxrAvr/Preset/presetManager.c:57-115` implements
`preset_writeDrumsetData` and `preset_saveDrumset`. The latter writes exactly8
name bytes, then the former writes228 individual bytes in ID order. The reader
is `preset_loadDrumset`/`preset_readDrumsetData` at173-254. Full source hashes,
line receipts and compiled unchanged C are bound in the test manifests.

| Absolute offset | Length | Meaning | Validation/preservation |
|---:|---:|---|---|
| 0..7 | 8 | `preset_currentName[8]` | Arbitrary bytes; no terminator, text encoding or character restriction |
| 8..235 | 228 | `parameter_values[0..227]` | Individual uint8 values; byte at8+ID |
| 236 | 0 | End | Exact complete size required by this codec |

There is **no magic, version byte, checksum/CRC, count header, padding or
trailer** in `.snd`. `FILE_VERSION=2` in this C file is used for composite
`.all/.prf`, not for sound files. Byte8 is reserved patch ID0, not a version.
All stored elements are single bytes, so no multibyte endian rule or compiler
packing is involved. Any236-byte sequence has the raw framing; this format
cannot authenticate origin or detect every corruption. A changed name byte is
not an invalid header. `InvalidHeader`/`UnsupportedVersion` errors would invent
format fields and are deliberately absent.

`preset_makeFileName:1085-1110` uses `p` + three padded decimal digits + `.snd`
for a uint8 slot; tests independently observe `p007.snd`. The library accepts
an explicit caller path and does not generate slot names or derive paths from
kit names. `.pat`, composite `.prf/.all`, globals, sample/WAV files and executable
firmware images are not accepted as alternate sizes or guessed formats.

## Raw archival bytes, canonical meaning and firmware loading

These are three different contracts:

1. `snd::DecodeRaw`/`EncodeRaw` preserve **every byte** of an exact236-byte record,
   including unusual/reserved/unnormalized values and binary names. This is the
   correct representation for exact archival file round trips. Raw framing
   success is not a claim that a kit can be interpreted or played safely.
2. `snd::DecodeCanonical`/`EncodeCanonical` use the P17-B semantic field contract
   and strict route-admission policy. Every accepted byte is preserved exactly;
   failures leave accepted output unchanged. No source value is normalized to
   force a successful result. A rejected source record can still be archived
   losslessly through the explicitly raw API.
3. The original firmware loader accepts short bodies after a complete name,
   zero-fills missing bytes, ignores trailing bytes, normalizes selectors>=205
   to0, and its sender repairs LFO hints outside1..6 to1. P17-C neither imitates
   partial reads nor performs those mutations. Independent C tests observe
   selector255/hint255 becoming0/1 while raw codec bytes remain255/255.

No canonical data-model change was needed. `RawKit` already holds all original
bytes; `CanonicalKit` covers the reviewed source-semantic subset. Reserved
IDs0,5,97,98,127 remain full uint8 provenance, with no invented zero-only rule.
External oscillator-wave identities>=6 remain stored data, with availability
UNKNOWN; no sample loading is implemented. P17-C can archive/encode such source
identities and reserved bytes>=128 even though P17-B's **runtime** candidate
rejects them. A future live load must also call the full runtime compatibility
gate, not treat successful codec decoding as permission to render.

## Pure API and failure contract

Accepted facade `interfaces/KitFileCodec.h`; implementation
`dsp/KitFileCodec.h/.cpp`. Buffers have explicit sizes/capacities. All loops,
records and candidates have fixed bounds; there is no allocation, file I/O,
recursion, logging, lock or mutable global conversion state.

| Operation | Result |
|---|---|
| `DecodeRaw(input,size,RawKit&)` | Exact framing only; all8+228 bytes copied by explicit fields |
| `DecodeCanonical(input,size,CanonicalKit&)` | Framing, P17-B fields and strict route admission, then one output assignment |
| `EncodeRaw(raw,output,capacity)` | Explicit archival name/payload encoding |
| `CheckRepresentability(canonical)` | Strict field/route export preflight without output storage |
| `EncodeCanonical(canonical,output,capacity)` | Strict, deterministic whole-kit export |
| Typed-route overloads of preflight/encode | Export explicit current twelve assignments using canonical scalar/metadata provenance |

Typed errors are `InvalidArgument`, `Truncated`, `Oversized`, `BufferTooSmall`,
`InvalidField`, `InvalidSelector`, `InvalidRoute`, `UnsupportedRoute` and
`NonrepresentableRoute`, with affected field, route slot where applicable and
P17-B detail. There is no lossy mode. Null/nonzero input rejects; null/zero
input is truncated. Wrong sizes are checked before reading any bytes. Output
capacity must be at least236; only236 bytes are written, preserving any tail.
Null, undersized and overlapping source/output buffers reject before mutation.
As with ordinary pointer/length APIs, supplied nonnull ranges must actually be
readable/writable for their declared extent. No untrusted length allocates or
sizes a stack object. Object representations are never serialized.

## Selector and typed-route export policy

Original selectors are full bytes0..204; OFF0. Every identity survives archival
decode/encode, including127/128/204 and unsupported/cross-voice rows. Strict
canonical admission/export rejects global, cross-voice, mutable-storage and
other unreviewed native destinations, using unchanged P17-B validators. The
accepted native subset remains107 rows/source family,39 above127, highest203.
No route is truncated, wrapped, remapped or silently replaced by OFF.

The twelve-record overload requires velocity owners0..5 then LFO owners0..5,
validating source/owner/order and exact persistent metadata. Full typed routes,
not a seven-bit patch candidate's inert selector bytes, are authoritative for
this explicitly requested export. Scalar/name/reserved data comes from the
canonical kit. Each accepted typed row replaces its original selector field;
enabled LFO records supply their exact hint0..6. An OFF record has no hint, so
that field stays with canonical provenance. Hints never retarget row identities.

Unknown namespaces or out-of-table typed identities reject the complete export
as Nonrepresentable, identifying the exact slot/field. Current host codes205..65535
are reserved/unavailable; this does not pretend that a plugin-only destination
is currently active. Future profiles still require explicit representability
checking. Invalid enabled-OFF records and mismatched metadata reject as invalid.
No extension bytes, profile change, host-ID change or LXRS v3 change was added.

## Non-real-time file boundary

Accepted facade `interfaces/KitFileIO.h`; `dsp/KitFileIO.h/.cpp` separates bounded
operations from `KitFileIOWin32.cpp` native services. These helpers must never
run on audio. `FileOps` is a borrowed synchronous OS boundary with independent
fault injection; it stores no owned heap object, kit state or global authority.
Windows is the implemented native platform; other builds return unavailable
operations, not an unverified portable filesystem implementation.

Read requires an explicit bounded absolute drive/UNC path, regular disk file,
exact file size236, exact236-byte read and EOF check, followed by close and
complete decoding before accepting output. Native read sharing excludes writers
and deletion during the read. Relative/device/ADS/reserved DOS paths, null paths,
trailing dot/space/separator and overlong input reject. Input path length is
less than944 wchar elements; the bound reserves space for a private suffix.
No directory is created and no path comes from the eight-byte kit name.

Export first encodes/validates the complete kit before any file operation. It
creates a temporary sibling with `CREATE_NEW`, exclusive sharing and DELETE
access, attempting at most16 collision names. Only a successfully created
handle becomes cleanup-owned; collision paths are never removed. It writes
exactly236 bytes and flushes before publication. The backend checks disk type
before sending data, including Windows-normalized device aliases.

Publication renames the opened object using `SetFileInformationByHandle` and
`FILE_RENAME_INFO` with replacement disabled. The handle remains exclusive;
there is no close/reopen temporary-path race. SDK header offsets are compile-time
checked; this OS ABI boundary is unrelated to `.snd` serialization. Existing
destinations, including ones created concurrently, cannot be overwritten. No
copy/replace fallback exists if the filesystem refuses the operation. The
documented no-replace and absolute-name behavior is defined by Microsoft's
[FILE_RENAME_INFO contract](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info).

Pre-publication failures mark **that owned handle** for deletion and close it,
using [FILE_DISPOSITION_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_disposition_info)
and [SetFileInformationByHandle](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfileinformationbyhandle).
No lexical path deletion can remove another process's replacement. Primary and
cleanup errors are retained separately; incomplete cleanup reports the private
temporary name. Tests inject write/short-write/flush/publish/disposition/close
failures and exhaustion. They do not change permissions or fill the disk.

The commit point is successful rename. `FileResult.published` reports it. A
subsequent close failure returns `FileError::Close` **with published=true**:
the complete, flushed file exists, and must not be deleted as temporary cleanup
or assumed absent. This is not a partial export; callers must inspect that flag
when reporting/retrying. Before commit, failures never publish a partial target.
Existing user files remain unchanged. Crash/power-loss durability, every remote
filesystem and malicious OS/kernel interference are not certified by finite
local tests; failed native operations propagate instead of weakening guarantees.

## Verification and reproducibility

All final source-built configurations pass **594,156 assertions /0 failures**:
GCC O0/O2, VS18 LLVM O0/O2, AddressSanitizer O0/O2 and UndefinedBehaviorSanitizer
O0/O2. Sanitizers actually executed with no findings. This is finite exercised
coverage, not proof of all future inputs, absence of all data races or leak
coverage. No sanitizer checks were suppressed to pass the suite.

Each run compares62,327 outputs byte-for-byte against the **unchanged original
C writer**, with zero mismatches;66,351 round trips and13,000 deterministic
fuzz/property cases pass. Coverage includes every value of every236 byte
position, all228 payload fields, all original selectors/source owners/hints,
metadata corruption, all shorter sizes and selected huge/oversize sizes,
buffer canaries/null/overlap, strict routing/export rollback and independent
multi-instance/concurrent I/O. Original normalization is independently observed.
Golden source files are synthetic, not factory content: default, drum-focused,
snare-focused, shared-hat, selector-boundary and fully populated/boundary/reserved.
Independent Python explicit byte vectors, SHA256, expected field values and
canonical admission are in `golden_fixture_manifest.json`. The selector-boundary
fixture is archival-valid but deliberately fails strict route admission.

The initial sanitizer link recipes selected inconsistent CRTs and stopped before
execution. Those failures remain in evidence. Address builds now use matching
/MD compile/link; bundled Windows UBSan uses /MT. Ordinary native tests use the
documented toolchains; these diagnostic CRT choices do not change the plugin.
Earlier narrower passing runs are preserved. No original-C mismatch, memory
unsafety, assertion failure or policy denial was hidden or relabeled.

Fresh original-path regressions of hash-verified unchanged-source binaries:
40 relevant P15-M2A..H/P12c/P16 suites pass, plus P17-A GCC O0/O2 and P17-B
GCC/LLVM O0/O2. Existing output destinations are redirected into new evidence;
old reports/fixtures are not rewritten. These six kit and40 other executions
are not claimed as newly compiled regressions. No full P16-F release campaign.

```powershell
python tools/verification/run_kit_file_codec_tests.py --output out/new-snd-gcc
python tools/verification/run_kit_file_codec_tests.py --compiler vs-llvm --output out/new-snd-llvm
python tools/verification/run_kit_file_codec_tests.py --compiler vs-llvm --sanitizer address --output out/new-snd-asan
python tools/verification/run_kit_file_codec_tests.py --compiler vs-llvm --sanitizer undefined --output out/new-snd-ubsan
```

Each output directory must be new. The runner regenerates all fixtures, records
source/command/executable hashes and compiles original C as C through disclosed
host-only FatFs/UART boundaries. No generated C++ codec/mapping participates in
the C oracle. Denied images must remain BLOCKED, never retried through a bypass.

## Allocation, memory and scope

Pure codec/conversion counted new/new[] allocations:0 in every final run,
including the timed operations. Production pure source contains no C allocation
calls. Raw/canonical/encoded storage is236 bytes each. `FileResult` is2076 bytes
on tested Windows x64; paths are fixed1024-wchar buffers and reads never allocate
from file sizes. Bounded native rename storage uses the same fixed path cap.
Source review shows fixed call nesting and no recursion, untrusted-size stack
allocation or hidden mutable state. Complete callback stack/latency is not claimed.

File-I/O test setup allocations are measured separately (2..31 in the final
ordinary configurations,31 in diagnostics), including test path strings/threads;
they are not audio or pure-conversion allocations, and do not measure Windows
kernel allocations. Operation means over2000 iterations and all exact compiler
results are in `out/p17c/allocation_report.md`; they include harness overhead and
system-load effects and are not worst-case scheduling guarantees.

## Evidence, publication and next phase

Every requested artifact is under `out/p17c/`: exact layout/API contracts,
goldens, differential/round-trip/representability/malformed/I/O/fuzz results,
allocation report, regression matrix, source/artifact inventories, integrity,
complete verification and development-publication receipts. Only explicitly
reviewed source/test/docs paths are eligible for the development commit. No
out evidence, binaries, DLLs, WAVs, temporary projects, backups or credentials.

The protected installed plugin remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`;
all seven resources and existing235 production inputs are unchanged. Original
firmware and prior P16-F/P17 evidence remain preserved. Source accepts no Engine,
authority, HostTransport, MIDI queue, FIR adapter or host-parameter object. It
cannot publish a kit or change LXRS state. No VST3 compatibility build is needed.

P17-D can now be separately designed around accepted candidates and existing
joint patch/routing authority: preserve unrelated project domains and ongoing
voice/runtime state, rebase held parameter-lock bases, order newer automation,
reconcile controllers and prove atomic publication without calling full-project
restore unchanged. It also needs full runtime compatibility checks beyond codec
success. None of that is implemented here. REAPER `.snd` Import GUI, browsing,
morphing, factory kits, graphics and samples remain absent. Legacy composites,
factory inventory/licensing, external samples and hardware timing stay UNKNOWN
or deferred. There is no unresolved current236-byte format ambiguity. P16-F
Import/timing/performance/raw-PCM/exact-artifact/observer release gates remain open.
