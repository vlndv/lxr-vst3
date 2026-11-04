# P15-M2G production state integration

2026-11-04. **PASS — production LXRS v2, routing authority and atomic
restoration.** This milestone does not register host parameters 228–239 and
does not implement sequencing, morphing, presets or UI work.

The source of truth remains Sonic Potions/LXR 0.37 at
`dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`. The custom non-commercial and
source-redistribution licence continues to apply. The implementation follows
the frozen P15-M2F schema; it does not use Catalyst data or algorithms.

## Wire format

All integers are encoded explicitly in little endian. No C++ object, enum,
pointer, padding, runtime phase, random state or effective modulation value is
serialized.

| Absolute offset | Bytes | LXRS v2 field |
|---:|---:|---|
| 0 | 32 | `LXRS` header, version 2, lengths, count 228, flags/reserved zero, whole-container CRC-32 |
| 32 | 228 | original parameter IDs 0–227, one validated seven-bit byte each |
| 260 | 16 | required routing TLV: tag 1, record version 1, body 240 |
| 276 | 48 | routing module version/profile/table identity and pinned table SHA-256 |
| 324 | 192 | twelve ordered 16-byte records |
| 516 | 4 | existing VST3 signed-int32 bypass suffix |

The canonical core is 516 bytes and the canonical VST3 stream is 520 bytes.
The CRC is IEEE CRC-32 over the complete core with bytes 24–27 treated as zero.
Plugin bytes are limited to 4092 and VST3 input to 4096 bytes. Length arithmetic,
TLV order/count, reserved bytes, table digest, profile, route order and the
complete stream are validated before publication. Unknown required tags reject;
unknown optional TLVs are retained byte-for-byte in fixed per-instance storage.

Each route record preserves owner, source, enabled/off state, original full-width
AVR row, firmware parameter, value type, scope, physical alias key, LFO menu hint,
version context and reserved fields. Records are velocity D1/D2/D3/SN/CY/HH,
then LFO D1/D2/D3/SN/CY/HH. The production profile accepts the same 107 verified
own-voice scalar rows for each source. Thirty-nine supported identities are above
127 and the highest is 203. Row 204 remains identifiable but unsupported. Invalid,
global, cross-voice, mutable-storage and mismatched identities reject; they are
never remapped or silently converted to off.

## Compatibility and host boundary

The immutable LXRS v1 reader and the validated historical 228-double iPlug2
reader remain present. Both preserve all original patch values and supported
bypass semantics while creating twelve canonical OFF records. Opaque legacy
selector bytes are never interpreted as routes. New saves use LXRS v2.

The LXR-local VST3 `setState` reads into a fixed 4097-byte stack buffer, detects
oversize input and validates the suffix before changing plugin state. `getState`
writes one coherent v2 snapshot plus the existing bypass suffix. Parameter IDs
0–227, normalization and MIDI CC/NRPN addressing are unchanged. IDs 228–239 are
reserved for P15-M2H and are not registered here.

## Authorities and restoration

`ParameterStateAuthority<228>` remains the only patch authority. A separate
`RoutingStateAuthority` owns twelve fixed records, bounded optional extension
storage, route tickets, a joint generation and restore epoch/cutoff. Its
one-attempt atomic gate is the common coordinator around patch and route writes;
processing never spins or blocks. It performs no allocation, locking, I/O or
logging.

Restore proceeds as one logical transaction:

1. Decode and validate the entire state, bypass and every route into fixed
   candidates.
2. Under the common coordinator, preflight the patch publication and routing
   restore epoch, then commit both authorities as one logical transaction.
3. At the next processing boundary, reconstruct an inactive Engine and
   ParameterArray from one coherent post-publication snapshot.
4. Select the fully staged Engine/patch/routes, reset the adapter/parser state
   and retain the established MIDI ordering.
5. Reconcile the original 228 controller values on the main-thread boundary.

Host and MIDI publishers use the same outer gate. Deferred host mailboxes carry
monotonic tickets; the restore cutoff prevents an older notification from
resurrecting after restore. Route updates reject stale epochs/tickets. Complete
snapshots are copied only under the common gate, so serialization cannot pair a
new patch with unrelated routes. Each plugin instance owns all coordinator,
staging and extension storage.

Native application uses the existing typed Engine interfaces. The verified DSP
order remains converted-base restore, velocity replay, LFO dispatch, immediate
narrowing and filter recalculation. Authoritative patch bytes never contain
effective modulation values.

## Verification

The dedicated O0 and O2 integration executable each passed 1502 assertions. It
checks independent M2F fixtures, exact 516/520 sizes and offsets, CRC/endian and
extension policy, all twelve records, every verified destination, full-width
identities, v1/legacy migration, corruption rollback, contention/stale updates,
rapid restores, multi-instance isolation and deterministic native audio with
routes active and off.

The external production VST3 test passed 91 assertions against the private
Release x64 bundle: active twelve-route load, authority and processing-boundary
projection, byte-identical save/reload, identities 135/169/203, migration,
corruption rollback and instance isolation. Existing state (322), runtime (1143),
controller (955), internal controller probe (1033), and Steinberg validator
(47/47) all pass on the same binary.

The isolated Visual Studio Community 18 / MSBuild 18.10.1 / v145 Release x64
binary is 795648 bytes with SHA-256
`DC3356F7CA709ED1BA67351FA2A699CA7BE846E19AF950B998B08D90887A79A4`.
All seven required assets are packaged. It was not installed and REAPER was not
launched. The certified installed binary remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`.

Current-source regression counterparts pass for MIDI, synthesis, original-C
snare/filter, FIR functional/quality/latency, P12c state/concurrency/controller,
M2A–M2F and M2G. Windows Application Control independently denied several fresh
test executables; those rows are BLOCKED, not FAIL or PASS. The exact historical
FIR Functional O2 executable was not executed and retains its documented BLOCKED
classification. No security workaround was attempted.

Detailed machine-readable evidence is under `out/p15-m2g/`. That directory is
verification output and is intentionally not committed.

## Known limits and P15-M2H

This milestone supplies persistence and a private friend-only test probe, not
public route controls. P15-M2H may add exactly twelve stable VST3 controls at
IDs 228–239, convert their integer tokens through this typed contract, reconcile
their controller cache, and retain the same coordinator. It must not reinterpret
legacy selector bytes, expand profile 1, or change the v2 record identities.

Sequencer routes, morph behavior, preset browsing, physical AVR NRPN relay,
row 204 and unsupported mutable/global destinations remain outside this phase.
