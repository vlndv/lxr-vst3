# P18-C morph target management and MorphSnd export

Status: engineering PASS; development publication pending the reviewed Git
checkpoint. Direct REAPER GUI acceptance is separately BLOCKED by unavailable
Computer Use access. The final verification classification is recorded in
`out/p18c/verification_report.md`.

## Scope and firmware boundary

P18-C exposes the P18-A/P18-B firmware transformation without creating another
morph engine. Source A is the accepted mutable whole-kit patch plus its twelve
typed routing records. Target B is one independently validated original 236-byte
`.snd` file: eight identity bytes followed by the original 228 parameter bytes.
Parameter 245 remains the sole Morph Amount authority and has the integer domain
0–255.

The scalar projection remains:

```text
fixed  = A * 256 + (B - A) * amount
result = fixed / 256 + ((fixed & 255) >= 127 ? 1 : 0)
```

The existing P18-B policy then restores source-owned typed route identities at
149–154, 161–166, and 167–172. Amount 255 retains the original `/256` endpoint
quirk. The operation is a parameter transformation, not an audio crossfade.

## Production operations

The editor and the exact-DLL verification harness both call the same production
entry points exposed by `IMorphManagement`:

- `importMorphSnd`: Load Morph Target.
- `clearMorphTarget`: Clear Morph Target.
- `exportMorphSnd`: Export Morphed Kit.
- `cancelMorphOperation`: report a cancelled file-dialog operation without state mutation.
- `getMorphState`: read accepted target presence, raw name, amount, generation,
  and the most recent typed operation result.

No editor callback contains codec, authority, or interpolation logic. Dialog
cancellation is reported as `Cancelled`; read, validation, publication, export,
busy, stale, and existing-destination failures have distinct statuses.

### Load Morph Target

The non-audio caller performs the P17-C exact-size reader and canonical decoder
before asking the P18-B morph authority to publish a target. Publication reserves
one global host-operation order and drains only older fixed-mailbox changes. A
failed read, unsupported sample, invalid selector/route, Busy result, or stale
publication leaves the prior target and its identity unchanged. A successful
replacement becomes one complete target generation. It does not write Source A,
reset the Engine, restart the sequencer, or alter project state.

### Target identity

The accepted target owns the displayed eight-byte identity. Display uses the
P17-F rules without changing the stored bytes: printable ASCII is copied,
nonprintable bytes become uppercase `\\xNN`, only trailing zero bytes are hidden,
and an all-zero identity is shown as `<unnamed>`. A failed load continues to show
the previously accepted identity.

### Clear Morph Target

Clear is an ordered, generation-checked morph-authority publication. It clears B,
its raw identity, and the target-present flag while retaining parameter 245.
With no target, effective rendering immediately selects Source A at the next
native 32-frame publication boundary. Source patch/routing authorities, locks,
velocity/LFO state, active voices, transport, and sequencer position are not
reset. A Busy or stale clear changes nothing.

## Coherent MorphSnd export

Export materializes this state only:

```text
accepted Source A + accepted typed routes + accepted Target B
                  + accepted Morph Amount
                  -> exact 228-byte effective morph projection
                  -> target B raw name8 + projection
                  -> exact 236-byte original .snd
```

Velocity, LFO, parameter-lock, PCM, pattern, and transport state are excluded.
Source-owned routing fields are reconstructed from typed routes and are never
seven-bit truncated or replaced by interpolated selector bytes. A route that
cannot be represented in the original `.snd` namespace rejects the operation.

Before snapshot acquisition, export reserves an operation order and drains only
older host mailboxes. It then acquires the routing, patch, and morph writer gates
once, copies one accepted generation, validates/materializes the projection, and
keeps those gates through the P17-C safe file commit. The output uses a bounded
temporary sibling, flush, and no-replace rename. Any later producer remains in a
fixed mailbox and wins after export; it cannot create a mixed file. Failed or
Busy acquisition does not spin and produces no destination file.

These gates never block the audio callback. Audio publication uses bounded
try-acquisition; while a non-audio export owns a gate, rendering continues with
the preceding coherent projection. Export performs no work on the audio thread.

The exported binary kit identity is Target B's original name. The chosen file
name is filesystem metadata and does not rewrite those eight bytes.

## DSP order and host behavior

The accepted order remains:

```text
Source A -> morph projection -> sequencer lock -> velocity -> LFO -> render
```

Parameter 245 remains the existing automatable integer parameter. The UI knob is
bound directly to that parameter; there is no second amount variable and no
unsolicited edit gesture. Host changes are normalized to exactly 0–255 and reach
the same bounded publication path used in P18-B. A target replacement at a
nonzero amount is projected coherently at the next native 32-frame boundary.

Source-kit imports and later patch automation update A without mutating B. Lock
release therefore reveals the latest projection of the latest accepted A.
Instances own independent target, amount, operation result, and generations.

## Persistence boundary

P18-C adds no LXRS module and changes no v1/v2/v3 encoding. Target B and the
morph-authority amount remain transient, as certified in P18-B. Component-state
save/reopen therefore restores neither a target nor a target-loaded display.
Hosts may separately restore automatable parameter 245, but a nonzero amount with
no target still renders Source A and must not be displayed as a loaded target.

Historical 51,065-byte and identity-bearing 51,089-byte states, plus the 65,536
byte maximum, remain unchanged. Target persistence is deferred.

## User surface

The existing compact editor adds only:

- Load Morph Target.
- Clear Morph Target.
- the existing Morph Amount parameter 245 control.
- loaded/no-target state and binary target identity.
- Export Morphed Kit.
- last operation result and typed error detail.

The whole-kit and sequencer controls remain present. This is not a preset
browser, animation system, factory library, or extensive editor redesign.

## Real-time and boundedness contract

- No audio-thread allocation, mutex, file I/O, log, wait, or dialog.
- Target validation and projection are fixed 228-field passes.
- Authority acquisition and snapshots have bounded attempts.
- No stale target pointers exist; target data are stored by value.
- Target replacement and clear publish one even generation.
- Export owns no heap-backed DSP state and does not mutate Source A.
- Existing P18-B native-boundary publication and PCM determinism are retained.

## Verification boundary

`P18CMorphManagementTest` independently covers projection/export bytes against
the unchanged original-C adapter, binary-name display, source-owned selectors,
no-target and nonrepresentable-route rejection, Busy/stale behavior, clear at
0/127/255, and allocation-free projection/publication. The P18-B exhaustive
16,777,216-input scalar parity suite is rerun before the P18-C assertions.

`P18CVST3MorphManagementTest` loads the exact private DLL and exercises the
production interface for valid/invalid/replacement loads, source preservation,
source-kit replacement while B remains loaded, host edits, amount endpoints,
safe export/no-overwrite, Busy clear, active rendering, transient restore,
multi-instance behavior inherited from P18-B, deterministic PCM, and 26-sample
48 kHz latency. Direct graphical interaction is a separate accessibility gate;
an unavailable automation attachment does not substitute blind clicks for these
production-path assertions.
