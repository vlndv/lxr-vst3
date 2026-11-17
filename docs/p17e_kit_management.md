# P17-E — user-facing original `.snd` kit management

## Scope and classification

P17-E adds a deliberately small whole-kit panel to the existing LXR editor:
Import `.snd kit`, Export `.snd kit`, current kit identity, and the last typed
operation result. The existing LXQB sequencer-bank import remains available in
the same editor. There is no browser, factory content, name editor, morphing,
sample loader, or replacement synth editor.

The musical operation is not implemented in a GUI callback. Both buttons call
the same bounded non-real-time `LXR::ImportKitFile` and `LXR::ExportKitFile`
operations that are exposed to the exact-DLL production harness through the
instance-local, non-automatable `IKitManagement` VST3 interface. The interface
does not run on or participate in the audio callback.

## Import contract

The import operation performs this sequence:

1. P17-C validates the bounded absolute path and reads exactly 236 bytes.
2. `DecodeCanonical` validates all 228 source fields, selector identities,
   ownership, and twelve routes without touching live state.
3. P17-D builds and validates one complete `PatchCandidate`.
4. `LoadRuntimeKit` reserves and commits the patch and routing authorities as
   one transaction.
5. The existing main-thread reconciliation projects all 228 patch parameters
   and twelve route controls to the controller without edit gestures.
6. The audio callback observes and applies the accepted generation only at its
   existing 32-native-frame boundary.

Read, codec, validation, Busy, and stale failures return a typed operation
result and do not reach the publication step. Patterns, pattern chaining,
transport, bypass, host position, tempo, and active-lock lifetime are outside
the transaction. The P17-D ordering remains base patch, sequencer lock,
velocity, LFO, rendering.

## Export contract

Export first takes the existing coordinated patch/routing snapshot. It builds a
canonical source kit from the 228 patch values and the authority-owned raw name,
then supplies the twelve typed routes to the P17-C encoder. P17-C revalidates
the complete kit and overwrites selector bytes in the encoded candidate only
from the typed route records. The safe writer creates a temporary file, flushes
it, and publishes it with a no-replace rename. An existing destination is an
explicit rejection; the plugin never truncates or overwrites it.

`UnsupportedRoute`/`NonrepresentableRoute` maps to the P17-E
`UnsupportedRoute` result with the affected field and route. No route is
substituted with OFF and no partial `.snd` is emitted. The current routing
authority admits only source-representable original selectors 0–204, so normal
P17-D state is representable by construction. The rejection path is retained
and verified with an explicitly nonrepresentable typed route for compatibility
with any future extension; P17-E does not expand the routing authority.

## Kit names

The authoritative name remains exactly eight bytes. Import and export do not
interpret it as UTF-8 and do not edit it. Display rules are:

- trailing zero bytes are omitted from display only;
- bytes `0x20`–`0x7e` display as ASCII;
- every other displayed byte uses uppercase `\xNN` notation;
- eight zero bytes display as `<unnamed>`.

The raw bytes remain in the routing authority and round-trip exactly while that
kit is active. The fixed LXRS v3 51,065-byte contract has no kit-name field.
A complete project restore therefore preserves all musical kit state but resets
the display identity to `<unnamed>`. P17-E intentionally does not change LXRS
v3 merely to persist a label.

## Threading and ownership

The file dialog callback and production operation are main-thread/non-real-time
entry points. File I/O, decoding, validation, and display formatting never run
inside rendering. The audio path receives only the already validated P17-D
publication. It adds no allocation, mutex, file operation, logging, or retry
loop. Publication still has one bounded attempt and reports Busy or stale.

Operation presentation state is per `LXR` instance. Musical authority remains
`ParameterStateAuthority` plus `RoutingStateAuthority`; the UI status is not a
second sound-state authority.

## UI behavior

The two controls use the pinned iPlug2 file-dialog facility. Cancellation is a
typed result and clears the displayed destination. Success is shown only after
the import transaction commits or the export is published. Rejection displays
the typed description and, where applicable, the field or route number. The
dialog and filesystem work do not block the audio callback while playback is
active.

## Verification boundaries

`P17EKitManagementTest` exercises binary names, cancellation/error mapping,
real P17-C Windows I/O, exact round-trip, no-replace behavior,
nonrepresentable-route rejection, coherent publication, Busy/stale outcomes,
active-lock rebasing, running-sequencer/MIDI continuity, multi-instance state,
deterministic native PCM, and counted publication allocations under GCC and VS
LLVM O0/O2.

`P17EVST3KitManagementTest` loads the exact private DLL and invokes the same
production operations as the editor. It checks all 228 controller values,
twelve route controls, nondefault pattern/transport/bypass preservation,
51,065-byte LXRS v3 restoration, rejection rollback, no host edit gestures,
post-import automation, simultaneous instances, distinct actual VST3 PCM, and
26-sample PDC at 48 kHz. The normal checkpoint host suites and Steinberg
validator remain independent regressions.

Direct REAPER button interaction is a separate graphical gate. If the
authorized computer-control surface cannot reach the plugin editor, that gate
is reported BLOCKED without being substituted by the exact-DLL interface test.
This milestone remains a development checkpoint, not release certification.
