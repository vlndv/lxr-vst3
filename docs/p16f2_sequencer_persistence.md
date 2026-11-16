# P16-F2 sequencer persistence

Status: **BLOCKED; NOT CERTIFIED**. Starting HEAD
`7749ef1a0203f5ab92bf0e2f447b53bb95dadf68`. P16-F1 remains uncommitted
and BLOCKED pending combined production-host certification. No publication is
authorized before all mandatory gates pass.

## Pre-implementation format decision

Use LXRS v3, an explicitly new outer container. Embed the frozen LXRS v2 core
as required module 1, complete sequencer bank as required module 2, and plugin
transport configuration as required module 3. This preserves both historical
CRC definitions and routing/optional-extension semantics without teaching old
readers to accept a larger v2. Old readers reject v3. New readers retain the
unchanged bounded v1/v2/legacy decoders and supply empty-bank/default transport
on those branches. Every decode constructs a complete local candidate before
publication. No compiler object image, padding or bitfield is serialized.

All integers are little endian. The 32-byte v3 header is: magic `LXRS` (4),
version 3 (u16), header length 32 (u16), total core bytes (u32), payload bytes
(u32, total minus 32), module count (u16), flags zero (u16), reserved zero
(u32), IEEE CRC32 (u32 at offset 24), reserved zero (u32). Whole-core CRC treats
bytes 24..27 as zero. The existing four-byte VST3 bypass suffix is outside CRC;
zero is off, any nonzero input is on, canonical output is 0/1.

Modules use 16-byte headers: tag u32, version u16, flags u16 (1 required, 0
optional), body length u32, reserved zero u32. Tags strictly increase; duplicates,
zero tags, unsupported known versions, unknown required modules, reserved flags
or bytes reject. At most 16 modules. Unknown optional outer modules are rejected
in this initial profile: callers must never silently discard opaque data. A
future version may explicitly define bounded retention. Existing optional data
inside frozen v2 is preserved exactly by that module's original rules.

Module 1 is a complete v2 core (516 bytes plus up to 3576 preserved optional
bytes), without bypass. Module 2 is independently versioned `LXQB`:
32-byte header followed by 50,417 explicit payload bytes. Header: magic (4),
version 1 (u16), header length 32 (u16), module bytes 50,449 (u32), payload bytes
50,417 (u32), patterns 8 (u16), tracks 7 (u16), CRC32 (u32 at 20, zeroed for
whole-module CRC), positions 128 (u16), step bytes 7 (u16), reserved zero (u32).
Payload order: all steps in pattern/track/position order, seven named bytes per
step; all main-gate words in pattern/track order; all length codes; all rotations;
all pattern next/repeat pairs; shuffle byte. Source-backed data totals:
50,176 + 112 + 56 + 56 + 16 + 1 = 50,417. The original `.pat` omits rotations.

Module 3 has a 16-byte body: enabled u8, clock u8 (0 Internal, 1 Host Sync),
internal BPM u16 (1..255), selected pattern u8 (0..7), reserved zero u8,
reserved zero u16, musical PRNG seed u32, reserved zero u32. Seed is initial
musical configuration, never current RNG state. Mutes, held rolls, pending
requests, host PPQ/sample anchors, epochs, track positions, probability latches,
lock overlays, FIR history and voice state are transient and reset on restore.
Pattern selection is the configured starting pattern, not the runtime chain head.

Canonical v3 core: 32 + 3*16 + 516 + 50,449 + 16 = **51,061 bytes**.
Canonical VST3 stream: **51,065 bytes**. Fixed maximum stream **65,536 bytes**,
core at most 65,532, chosen for the 51,065-byte dense state, 3576 bytes of
historical optional routing data and 10,895 bytes of future format headroom.
No allocation is based on input lengths. Bounds use remaining-size subtraction
before addition, indexing or copying. Truncation and arbitrary trailing bytes
reject. Canonical encoding is deterministic and uncompressed.

## Publication design

Extend the existing patch/routing outer one-attempt atomic coordinator to cover
bank publication and transport configuration. Parse, CRC, validation, bank copy
and pattern-slot reservation happen off audio. Reserve a P16-B write handle
before mutation. Under the outer gate, preflight all generations and acquire the
patch writer; commit prepared bank, patch, configuration and routing restore
epoch, with routing joint generation as the logical commit point. Failures
before commit cancel all reservations and preserve all authorities.

The audio owner must pin the bank while taking the same coordinated snapshot.
It stages patch/routes, selects that bank pin and transport configuration at one
processing boundary, then renders only that tuple. Retain the applied bank pin
when the coordinator is Busy. A scheduler using independently acquired latest
bank snapshots would violate this contract; it must consume the selected pin.
No full bank construction/copy/validation or CRC may occur on audio. Published
slots remain immutable and pinned until the audio owner replaces its selection.

Restore resets native/FIR/parser/transient sequencer state once. Host Sync then
reanchors from the next valid current host context, with no historical replay;
invalid context emits no sequence events. Internal starts at configured rotation
and BPM. Stopped, playing and looped hosts use the same reset/reanchor policy.
No controller projection uses unsolicited automation gestures.

## Production ingress decision

Provide a narrowly scoped host-accessible import action for an `LXQB` bank file.
It opens and parses the selected file on main-thread idle, uses the production
validator and three-slot publisher, and reports success, invalid input or Busy.
The startup bank stays the firmware-cleared bank. Fixtures are generated under
test/evidence directories. No factory bank, editor or preset browser is added.

## Original `.pat` audit

Pinned `front/LxrAvr/Preset/presetManager.c`, `preset_writePatternData`,
`preset_savePattern` and `preset_readPatternData` explicitly write/read eight
name bytes, 7168 seven-byte records, 56 little-endian main masks, eight next then
repeat pairs, shuffle, then 56 length bytes. Total 50,369. There is no magic,
version identifier or checksum. The old reader tolerates absent lengths and
defaults them to sixteen. Rotation is omitted. `.all/.perf` have separate
versioned wrappers and are not `.pat`. Native `.pat` import is **DEFERRED**;
no original-file compatibility claim is made without actual independent fixtures.

Detailed baseline and verification evidence: `out/p16f2/`. Prior evidence remains
unchanged. Certification results will be recorded only after execution.

## Implemented and executed results (2026-11-09)

The bounded explicit codecs, LXRS v3 VST3 integration, three-slot bank
publication, coordinated audio pin selection and minimal import-only editor are
implemented. IDs 0..242 retain their numbers and meanings; no additional host
parameters were registered. The editor imports `.lxqb` files on its main-thread
file-dialog callback. Busy or invalid import retains the previously accepted
project. No preset browser, factory bank, pattern editor, morph or sample loader
was added. Initial source-faithful empty patterns remain silent.

The serializer passes **152,002/0 at O0 and O2**, including independent Python
struct/zlib whole-project and bank goldens, all 7168 records, metadata, exact
sizes, full truncation rejection, repaired-CRC invalid fields, optional-module
policy, historical optional-byte retention, v1/v2/legacy migration and bypass.
Unknown outer optional modules reject under this initial profile. Nested v2
unknown optional records retain their original semantics and exact bytes.

The final private binary is **C2ECE09B0AA41EF6BBD13FADB4A4D8843654A2F84A242D0CE2510BA8C8B7C939**.
It was built with VS Community 18, MSBuild 18.10.1, Release x64/v145, both build
events disabled, and packaged privately with seven verified assets. It passes
state/runtime/controller/M2G/M2H/F1/F2/probe at 322/1143/955/91/687/105/3300/1033
checks respectively and Steinberg validator **47/47**. The expanded F2 barrier
and timing suite passes **3300/0 at MSVC /Od and /O2**. The final current-source
checkpoint generated its own fixtures and executed the /O2 suite successfully.
Earlier MSVC /O2 and LLVM /O2 executables were denied by Application Control and
retain **BLOCKED** status. No denied executable is relabelled PASS, and no
security policy or image was modified.

Complete restoration now constructs fresh transient Engine storage while
retaining resident non-owning assets. The former copy-then-init path retained
voice oscillator RNG state: the first multi-voice PCM test failed exact reload
and callback-invariance comparisons. That failure is preserved in
`out/p16f2/host-focused-002/`. The corrected loader passes both comparisons
through the actual final VST3 `setState`. No oscillator, filter, envelope or
modulation equation was changed. Bypass is captured inside the shared snapshot
coordinator so a concurrent restore cannot supply a newer suffix to an older
patch/bank/config snapshot.

The shared routing generation is the logical commit point under the outer
try-gate. The audio owner pins a matching bank while snapshotting the other
domains, stages the native projection, then swaps the retained pin and transport
configuration before rendering. Contention retains the complete old audio tuple.
Tests force the coordinator barrier, pinned inactive slots and a reserved
concurrent pattern writer. No validation/CRC or full-bank copy occurs on audio.
Controller readback is projected without gestures. Project restore discards
pending pre-cutoff transport edits and resets transient clock/queues/locks/RNG;
Host Sync reanchors to the current valid host context. The configured initial
pattern is persisted; the runtime random-chain head is not.

REAPER 7.80 loaded the preceding private binary
`CC4934CA013A51A335D40104EB064D0DEE21F304CAD5D9C81748DB04F3097AD7`.
The user completed the actual editor file dialog. Independent extraction of
REAPER's plugin state proves the imported and reopened bank are both exactly
the fixture, SHA-256
`74A5A73030212A1F3641CD6C5BB86AB8290DB269530F9E6E43D2F68D988A1D90`.
Host Sync 120/60, Internal, MIDI coexistence, all 228 patch values, twelve routes,
three transport controls, bypass, project reopen, repeated offline PCM and PDC 26
pass in that run. The two 120-BPM 96,000-frame stereo 24-bit renders are identical:
PCM SHA-256 `8355E00B9D6A1D0F8EE395213996EA1D85737B1AAAB1E588D6AC4F8E0D67516A`,
peak 0.4454512596130371, RMS 0.01183707821449973.

The final-hash REAPER run independently proves the exact same bank and produces
the identical first 120-BPM PCM hash above. It restores the bank through
the real project loader, with a configured 512-frame dummy device. It has not
completed its modal render/reopen workflow: Computer Use timed out waiting for
app access to the new instance. This is **not** complete exact-final-hash host
certification. Source-identical functionality or the preceding hash's results
are not substituted for that gate.

## Remaining mandatory gates

Combined P16-F1/F2, F2 implementation certification and publication remain
**BLOCKED**. In addition to the final-hash REAPER workflow, mandatory REAPER live play/stop/resume, seek, loop,
tempo-change timing, verified callback-size changes, shuffle isolation, manual
roll, probability, two-slot lock isolation, fixed/random chain replay and two
different nonempty instances remain uncertified. The existing three production
host controls do not expose manual roll; this report does not count substep
retriggers as manual-roll acceptance. Exact-DLL processing allocation-call
instrumentation and a measured complete concurrent stack high-water mark are
also outstanding. Native bounded loops measure zero processing allocations;
that is not a blanket claim about every host/framework callback.

The historical exact FIR Functional O2 executable retains its independent
BLOCKED classification. TSan was not run. Native `.pat`, SOM, morphing, presets,
factory-kit management, graphical editing and user samples remain deferred.
Physical listening through an audio output was not performed; REAPER used its
Dummy Audio device and the PCM files were independently measured.
No files were staged, committed or pushed. This checkpoint is not ready for a
successor milestone until the remaining mandatory gates close.
