# P17-F — kit identity persistence and host presets

## Classification and scope

P17-F is a development engineering PASS. It closes P17-E's raw-name
persistence limitation without adding a preset browser, factory library,
morphing, sample loading, or replacement synth editor. Direct REAPER graphical
interaction remains a separate BLOCKED gate because no uniquely identifiable
P17-F private-candidate window was available to the authorized control surface.

## Identity ownership

The eight original `.snd` name bytes and a one-bit provenance marker are owned
by `RoutingStateAuthority`. They share its outer gate, kit generation, and
associated patch generation. A whole-kit import commits patch, twelve routes,
name, and provenance at the existing route-generation release-store. A complete
project or preset restore commits identity through the same coordinated restore
cutoff as patch, routes, bank, transport, and bypass.

There is no second metadata authority. Ordinary automation, MIDI, parameter
locks, route edits, and transport changes do not rename a kit. Failed import,
export, or state restoration preserves the current identity. Loading a new
`.snd` replaces it. Export reads the identity from the same coherent snapshot
as the current patch and routes.

The provenance bit distinguishes an old state with no identity module from an
explicitly imported all-zero name. Both display `<unnamed>`, but the explicit
all-zero identity is serialized and round-trips as an imported identity.

## State-format decision

The outer format remains LXRS v3. Its ordered module mechanism carries a known
optional tag-4 module:

- tag: 4;
- module version: 1;
- flags: 0 (known optional);
- body length: 8;
- body: the eight uninterpreted name bytes;
- reserved field: zero.

The existing outer IEEE CRC-32 covers the complete tag-4 header and body while
continuing to treat outer CRC bytes 24–27 as zero. The four-byte VST3 bypass
suffix remains outside the CRC, unchanged. Tags 1–3 remain required and ordered.
Tag 4 may appear once after tag 3. Duplicate, reordered, unknown, wrong-version,
wrong-size, nonzero-reserved, or corrupt modules reject the whole candidate.

Exact VST3 stream sizes are:

- historical v3 without identity: 51,065 bytes;
- identity-bearing v3: 51,089 bytes;
- maximum accepted stream: unchanged at 65,536 bytes.

Historical v3 bytes remain accepted and reserialize byte-for-byte when no later
kit import establishes identity provenance. LXRS v1, v2, and validated legacy
iPlug2 states remain accepted and migrate with the documented absent/all-zero
identity default. The detailed pre-implementation gate is
`out/p17f/state_format_decision.md`.

## Host preset workflow

LXR does not define a `.vstpreset` container. Steinberg's pinned SDK
`PresetFile` implementation owns that standard host format. Its component chunk
contains the same production `IComponent::getState()` LXRS stream used for
project persistence. Loading calls the same production `setState()` decoder and
coordinated restoration path, followed by existing controller reconciliation.

The exact private DLL test saves and loads a standard VST3 preset through the
SDK, verifies the `VST3` container signature, restores all patch/routes/pattern/
transport/bypass/identity state, clears transient roll activation, produces
matched deterministic PCM, and retains 26-sample PDC at 48 kHz. Host-visible
preset names and preset-file placement remain host-owned.

## Import, export, display, and rename

P17-E import/export operations are unchanged. Import publishes the exact name
with the patch/routes transaction. Export uses the coherent current name and
retains embedded zeros, control values, extended bytes, trailing zeros, and an
all-zero name. Unsupported routes still reject without substitution or output.

Display remains printable ASCII, uppercase `\xNN` for nonprintable bytes,
display-only trailing-zero trimming, and `<unnamed>` for eight zeros.

No Rename Kit text control was added. The minimal editor has no byte-oriented
text-entry contract, and automatic Unicode conversion would violate the binary
identity requirement. A future explicit ASCII/raw-byte rename can reuse the
same authority transaction without changing this state module.

## Real-time behavior

Identity storage is one fixed eight-byte array plus a Boolean. File/state
parsing, CRC, VST3 preset I/O, and controller projection remain off audio. The
audio path consumes only the already coordinated authority generation. P17-F
adds no audio-thread allocation, mutex, I/O, logging, wait, retry loop, dynamic
storage, recursion, or variable-length stack object.

GCC and VS LLVM O0/O2 each pass 1,082 P17-F assertions with zero counted
publication allocations. Identity-only changes produce exact native PCM hash
`20667AF077E68561`; a volume-different state produces
`D4CB9DDB17B4D6B0` under all four builds.

## Verification summary

The exact private Release x64/v145 DLL passes the P17-F host preset suite
157/157, P17-E 191/191, prior VST3 state/runtime/controller/modulation/
transport/persistence/MIDI suites, ControllerSyncProbe 1057/1057, and Steinberg
validator 47/47. The 44-row current-source native checkpoint passes O0/O2,
including bit-exact 80,000-block original-C filter comparisons, original-C
snare cases, FIR quality/latency, MIDI, synthesis, state, and concurrency.

P17-A/B/C pass 306,037/817,873/594,156 assertions per optimization. P17-D and
P17-E pass 1,176 and 1,052 per optimization under both GCC and VS LLVM.

Private DLL SHA-256:
`F9F2C0E49BD19D9C5CFE8249E458A3758E82BBA8D2DC34388F2EBBB58339B174`.
The protected installed DLL remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`;
all seven resources are unchanged.

This is a development checkpoint, not release certification. P16-F release
blockers remain independent and open.
