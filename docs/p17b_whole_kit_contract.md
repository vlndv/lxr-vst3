# P17-B whole-kit data contract

Development implementation and standalone engineering acceptance: **PASS**.
P16-F release certification remains **BLOCKED**. This phase introduces data,
validation and candidate construction only. The production Engine, host controls,
LXRS v3, parameter/routing/pattern authorities and transport are unchanged. No
VST3 was built or installed. Publication receipts are in
`out/p17b/publication_receipt.json`; the final inventory/report records whether
the authorized development push completed.

Baseline: `p12b-4c-fir-resampler`, local/live HEAD
`02c4005c91118d369473511e0d5a4b41e30202be`, origin
`https://github.com/vlndv/lxr-vst3.git`. All 468 P17-A source fingerprints,
eleven publication paths, 143 P17-A artifacts, 757 F13 artifacts and 235 existing
production inputs matched before implementation. Pinned firmware
`dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`, iPlug2
`d54f69050f517e43b941d88c2a170f0a840b9ee4`, SDK
`3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96` and SDK submodules passed cached-archive
verification. Catalyst was not used. Original copyright and the custom
non-commercial/full-source licence in `LICENSE-LXR.txt` apply to derived data.

## Representation and hierarchy

Accepted interface: `interfaces/KitContract.h`. Definitions: `dsp/KitTypes.h`,
`KitContract.h/.cpp`, `KitRecords.inc` and `KitFields.inc`.

| Record | Contents | Measured size |
|---|---|---:|
| `RawKit` | Eight uninterpreted name bytes and 228 interleaved source bytes | 236 |
| `CanonicalKit` | Eight name bytes; six named voice records; global and reserved records | 236 |
| `PatchCandidate` | Eight name bytes, 228 authority-compatible bytes, twelve typed route records | 428 |

Sizes agree under GCC UCRT64 and VS18 LLVM. These are logical records, never a
disk encoding or `memcpy` serialization contract. Canonical access/conversion
uses explicit named fields generated from the pinned original declarations.
Only fixed-width integers and fixed arrays are used. No owning pointers,
bitfields, runtime pointers, variable counts, dynamic storage or object-image
casts occur. The canonical record has alignment1 and no padding, enforced by
static assertions; construction over poisoned storage checks every byte.
`StoredSelector` validates a wide argument before storing a **full byte**, with
255 as an invalid in-memory value. It is distinct from `SevenBitValue` and from
the existing wider native route identities. Values128..204 are preserved.

The original hierarchy remains `.snd`=whole kit236, `.pat`=eight-pattern
bank50369, `.prf`=performance50946, `.all`=combined settings50946, globals22.
No patterns, BPM, bar-reset or device-global fields are embedded in the kit.
These format facts do not introduce any file parser/writer in this phase.

## Complete ownership and mappings

`kFields` is an immutable executable table for every ID0..227: source identity,
canonical member path, owner, kind, maximum, initialization default and native
disposition. The table is checked for contiguous IDs and exact owner counts.
`out/p17b/parameter_mapping_matrix.csv` additionally records original array slot,
MCU target, storage type, menu domain, conversion and shared/global status, with
source anchors. Generation reads original enums, `parameter_dtypes`, AVR target
table and STM32 pointer assignments; it does not start from VST3 names.

| Owner | Stored fields |
|---|---:|
| Drum1 / Drum2 / Drum3 | 37 each |
| Snare | 36 |
| Cymbal | 37 |
| Shared hi-hat | 38 |
| Kit-wide decimator, ID114 | 1 |
| Reserved/protocol IDs0,5,97,98,127 | 5 |
| Total | 228 |

Closed/open hats are trigger tracks5/6 and share synthesis owner5, one velocity
route and one LFO route. Decays60/61 and note overrides226/227 remain separate
named fields inside that shared record. They do not create a seventh oscillator,
filter, envelope owner or routing source. The seven MIDI note overrides are kit
fields despite their dispatch role. Source slot1 is the oscillator waveform
alias, not a new mod-wheel parameter. Original runtime/menu IDs228..250 and
device globals251..272 are separate domains from VST3 route IDs228..239.

Raw-to-canonical preserves stored units. It does not convert an attack knob into
a DSP coefficient, add1 to filter enums, combine coarse/fine fields, or replace
their existing Engine setters. The source-target strings describe those later
setters/aliases, not pointers inside the kit.

## Defaults

`MakeFirmwareInitializationKit()` and `MakeCanonicalInitializationKit()` produce
the original **stored initialization**: every sound byte0, including all reserved
bytes, twelve OFF selectors, six LFO hint bytes0, pitch, envelope, filter, gain,
articulation and kit-wide decimation bytes. The eight-byte BSS name starts0.
Original `Menu/menu.c:545-580` zeroes all273 parameter bytes, then changes roll,
Euclidean fields and BPM outside0..227. These are not factory sounds.

The MCU oscillator/envelope/filter/voice initialization contains other values
and transient state; it is not an alternative 228-byte stored default kit.
Startup later attempts `glo.cfg` and `p000.snd`; no factory kit assets exist in
the pinned checkout. The original sender repairs LFO voice hints0 to1 during
publication. P17-B preserves the pre-send0 sentinel and tests that distinction
against the unchanged C reader/sender; it does not mutate provenance to imitate
that broadcast side effect. Hint0 is accepted explicitly, not an accidental
validation exemption.

All228 stored defaults match the executed original `menu_init` fixture, and
repeated construction is deterministic. Per-field results are in
`out/p17b/default_kit_comparison.json`.

## Structural, semantic and runtime validation

Fixed record types establish counts and ownership structurally: there is no
caller-controlled length, owner tag or extra voice to trust. Name bytes have no
terminator/text restriction. A `RawKit` can hold every byte sequence; its type
alone does not assert semantic validity. `ValidateRawKit` and
`ValidateCanonicalKit` apply the reviewed source semantic domains below.

| Domain | Accepted canonical values / policy |
|---|---|
| Ordinary knob/note fields | 0..127 |
| Mix/FM and velocity-amplitude switches | 0..1 |
| Filter type / LFO waveform | 0..7 |
| Retrigger / sync / transient waveform / audio route | 0..6 / 0..11 / 0..13 / 0..5 |
| Original destination selectors | 0..204;0 OFF |
| LFO advisory voice hint | 0 initialization sentinel,1..6 menu identities |
| Reserved/protocol slots | Preserve every uint8 byte0..255 as opaque provenance |
| Oscillator waveform | Preserve uint8;0..5 ROM,>=6 external-sample selection |

Menu ranges are independently compiled from original `parameter_dtypes` and
`MenuText.h`. They are meaningful editing domains, not a claim that the original
unsafe file reader enforced them. The original writer/reader can store/forward
out-of-domain bytes. Strict semantic conversion rejects such corrupt/noncanonical
knob/enumeration values instead of imitating unchecked dispatch or silently
clamping them. Reserved bytes have **no invented zero-only file rule** and are
never safe mutation targets. The runtime patch authority cannot represent their
values128..255, so those records remain source-representable but native candidates
return `Unsupported`. No reserved scalar meaning is invented.

External sample availability/count depends on hardware SD contents and is
UNKNOWN here. All byte identities>=6 remain representable for archival conversion;
native candidate construction returns `Unsupported/ExternalSample`. It neither
loads samples nor declares every such byte an available sound. Structurally real
global, cross-voice and unsupported modulation destinations also remain intact
in canonical kits; they are rejected by the separate runtime compatibility gate.

`Status` distinguishes `Ok`, `Invalid`, `Unsupported`, `Nonrepresentable`.
`Issue` and the affected field ID identify the first bounded failure. Successful
raw/canonical conversion is a source-semantic result, not permission to render.
Only `BuildPatchCandidate` passing the runtime gate yields a native candidate.
Every output-bearing API constructs locally and assigns on success; a failure
leaves the caller's accepted output unchanged. No normalization, host callbacks,
Engine mutation or authority publication occurs.

## Original and typed modulation routes

Original selector storage is uint8 table index0..204; OFF is0. The AVR has205
rows. A row is **not** its firmware PAR, physical destination key, a dense
per-voice menu position or a seven-bit compatibility byte. Coarse/fine alias
rows retain their distinct identities even when their physical target is shared.
The final row204 addresses firmware PAR226, not open-hat PAR227.

`ResolveOriginalSelector` preserves source, full original index and LFO hint.
Every original row can round-trip through the canonical source kit for every
source owner, including source-valid cross-voice/global/unimplemented routes.
Velocity has no stored destination-voice hint; a supplied nonzero hint is invalid.
LFO hint0..6 is advisory and never retargets an index, even if it names another
voice. Unknown selectors205..255 reject rather than becoming OFF.

`ToTypedRoute` reuses the current persistent route metadata and validator. It
permits OFF or the existing107 own-voice scalar rows for each source family:
18 per D1/D2/D3/SN/CY,17 for shared HH. Exactly39 of those identities exceed127;
the highest is203. Global rows, cross-voice destinations, mutable destination
storage, LFO destinations and other unreviewed native targets return Unsupported
without touching the route output. No support profile or LXRS semantics changes.

`FromTypedRoute` validates owner/source, flags, reserved bytes and exact existing
route metadata before returning a source selector. Unknown active namespaces or
indices205..65535 return Nonrepresentable; an enabled OFF record is Invalid.
This is deliberate export refusal,
not byte truncation or extension-byte invention. Currently **no accepted native
plugin-only destination exists**: host tokens205..65535 and the proposed STM32
namespace are reserved/unavailable. Tests cover the entire reserved token range;
they do not claim those tokens are supported routes. A future expanded profile
must keep export refusal for identities absent from the original table.

The existing disabled LXRS record has no hint. Converting an OFF selector to
that record retains OFF identity but cannot retain an advisory hint there.
CanonicalKit continues to preserve the original hint independently. Exact source
export must use that provenance, rather than reconstructing a source record
solely from twelve LXRS routes. Enabled typed routes preserve their hint0..6.

## Candidate construction and ownership boundary

`BuildPatchCandidate` validates the entire canonical source kit, checks native
wave/route/patch compatibility, then produces all228 patch bytes plus twelve
ordered `PersistentRouteRecord`s: velocity owners0..5, LFO owners0..5. Candidate
construction replaces all twelve route assignments; it does not merge an old
assignment when the new source says OFF. This is only candidate data, not an
implemented live application.

The twelve old inert selector slots149..154/167..172 receive **zero compatibility
bytes**, for low and high selections alike. Their exact routes are in typed
records; hints161..166 and all other accepted bytes are preserved. This deliberate
projection prevents seven-bit truncation and does not redefine those opaque host
slots as a new encoding. CanonicalKit remains the lossless original source
record. Candidate alone is not an archival `.snd` round-trip representation.

Future publication must use the existing patch and routing authorities together,
then reconcile controllers. It must preserve patterns, parameter-lock overlays,
transport configuration/anchor, roll/mute state, bypass, external MIDI state,
optional LXRS data and unrelated host project fields. Neither a PatternStateAuthority
reference nor a HostTransport/Engine reference is accepted by any P17-B API.
Full-project restore cannot be substituted unchanged: it resets those domains.
P17-A's kit-kind coordinator, stale-edit ordering, active-lock rebasing to the new
base and runtime continuity proofs remain future work. No publication method or
synthetic automation-recording gesture was added here.

## Independent verification and regressions

Final P17-B runs: **817,873 PASS / 0 FAIL each**, GCC O0/O2 and VS18 LLVM O0/O2.
All tests use C++17 with exceptions/RTTI disabled; C is compiled as C. Source
manifests bind the same final inputs across runs. The C adapter includes the
unchanged original P17-A writer/reader/sender, original headers/menu tables and
exact `menu_init`/dtype slices with hash/line receipts. It does not include the
generated C++ field table or implement a duplicate C++ writer.

Coverage includes all228 owners/defaults/domains;58,368 byte mutations with
26,274 source-writer comparisons;all205 original selector identities across12
source/owner pairs;all0..6 LFO hints;2,460 original reader/sender translations;
all reserved host codes205..65535;metadata/owner/cross-voice corruption;OFF,
127/128/204 boundaries;unsupported waves/reserved provenance;rollback;
1,000 complete deterministic random kits;poisoned initialization;unchanged patch
and route authorities;zero counted conversion `new/new[]` allocations. No
Engine/PatternStateAuthority/HostTransport implementation is linked, so candidate
construction has no live state or host inlet. Source review finds no malloc,
I/O, locks, logging, mutable global tables or unsafe casts in these functions.

P17-A fresh GCC O0/O2:306,037/0 each. Forty relevant original-path regression
executions pass: P15-M2A..M2H, P16-B/C/D/E, host transport and bank/project codec,
P12c serialization/concurrency, native MIDI and parameter maps at both levels.
Existing input hashes are verified before reusing those preserved executables;
test-output CSVs are redirected into new P17-B evidence. This is fresh execution
of unchanged-source binaries, not a fresh rebuild of every regression suite.
No broad P16-F release campaign, host GUI acceptance or instrument build occurred.

The initial P17-B build used a reference instead of the existing authority
snapshot API's pointer argument; compilation stopped before tests. That harness
error was corrected. Raw failed build and earlier passing narrower tests remain
in `out/p17b/`; no original-C comparison failed and no assertion was weakened.
Review also corrected enabled-OFF typed metadata from Nonrepresentable to
Invalid, with an added assertion and all four final configurations rerun.
The staged whitespace check also removed a generated trailing blank line;
the generator and all four publication configurations were reverified.
All final runs executed normally; earlier historical Application Control denials
retain their original status. No policy, signing, image or loader workaround.

## Memory, cost and limits

All functions use bounded loops over228 fields,12 sources and fixed205-row
metadata. There is no recursion, heap allocation or mutable shared conversion
storage. Failure-atomic local candidates are at most428 bytes per routine,
with bounded call nesting and metadata lookup. This is a source storage bound,
not a measured complete callback stack or wait-free/audio scheduling claim.

| Average ns/operation, this machine | GCC O0 | GCC O2 | VS LLVM O0 | VS LLVM O2 |
|---|---:|---:|---:|---:|
| Canonical default | 47 | 4 | 49 | 14 |
| Canonical validation | 18904 | 790 | 9670 | 1068 |
| Raw to canonical | 5998 | 950 | 6424 | 169 |
| Canonical to raw | 31052 | 4031 | 12413 | 5687 |
| Native candidate | 58817 | 8521 | 32132 | 3988 |

Finite2000-iteration means include timing/harness overhead and are not worst-case
latency guarantees. Conversion/default/validation measured allocation delta0;
the test intercepts C++ new/new[], with source audit covering absence of C
allocator calls. Speed and fixed memory alone do not certify a future audio
thread kit-publication path. Keep conversion off audio until that phase is
independently designed and tested.

## Evidence, publication and next phase

`out/p17b/` contains structure/mapping/route matrices, per-default comparisons,
validation/conversion/original-C results, allocation/memory report, regression
matrix, source/executable/command manifests, inventories, integrity and full
verification reports. Generated binaries and evidence remain excluded from Git.
The protected installed plugin remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`;
all seven resources remain unchanged. Historical P17-A/P16 evidence content is
preserved. The metadata check found29 historical REAPER hardware-cache files
with timestamp changes predating this task; every size and content hash matches
the F13 ledger. The original metadata failure and content reconciliation are
recorded separately in `out/p17b/historical_metadata_discrepancies.json`; no
historical timestamp or content was rewritten.

The authorized checkpoint uses two commits: the unchanged eleven P17-A paths
(including its phase-log append), then P17-B source/test/docs and its separate
append. Each index is explicitly staged, fully diff-reviewed and blob-verified
against tested/reviewed content. Only a normal fast-forward push to the existing
development branch is permitted. Publication is conditional on all engineering
gates, clean reviewed index, unchanged installation and matching live baseline;
receipts identify the actual commit SHAs and remote result.

P17-C is ready for a separately authorized bounded236-byte `.snd` reader/writer
using these contracts: exact size, explicit8+228 field encoding, atomic error
handling, semantic/runtime policy reports and lossless provenance. Do not dump
C++ objects or extend original bytes. Export from current authorities needs a
deliberate provenance policy for reserved bytes, inert selector slots and OFF
hints; canonical archival conversion is already lossless. Composite legacy
files, target ABI, genuine factory content/licensing, external sample inventory
and hardware timing remain UNKNOWN/deferred. These do not block this fixed
whole-kit data contract. Live switching, browser, morph, factory kits, GUI and
sample loading are not implemented. P16-F release blockers remain open.
