# P19-D — Modulation and Kits + Morph GUI

## Classification

P19-D engineering acceptance is **PASS**. Direct graphical REAPER acceptance is
**BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE** and is not counted as a failed
engineering gate. This remains a development checkpoint; P16-F release
certification is still independently blocked.

The certified starting point was
`f2592bb44e52dc5610b37c71c5c7961ce61b688c` on
`p12b-4c-fir-resampler`. The original firmware, iPlug2 and VST3 SDK pins were
verified before edits. The protected installed plugin and its seven resources
remained unchanged.

## Modulation editor contract

The accepted P19-A inventory contains exactly 54 Modulation rows. P19-D exposes
those rows as nine controls for each physical synthesis owner:

| Role | D1 | D2 | D3 | SN | CY | HH | Domain |
|---|---:|---:|---:|---:|---:|---:|---|
| Velocity amount | 143 | 144 | 145 | 146 | 147 | 148 | byte 0–127 |
| Velocity destination | 228 | 229 | 230 | 231 | 232 | 233 | typed identity 0–65535 |
| LFO waveform | 155 | 156 | 157 | 158 | 159 | 160 | stored byte; selectable 0–7 |
| LFO rate | 115 | 116 | 117 | 118 | 119 | 120 | byte 0–127 |
| LFO amount | 121 | 122 | 123 | 124 | 125 | 126 | byte 0–127 |
| LFO destination | 234 | 235 | 236 | 237 | 238 | 239 | typed identity 0–65535 |
| LFO retrigger | 173 | 174 | 175 | 176 | 177 | 178 | stored byte; selectable 0–6 |
| LFO sync | 179 | 180 | 181 | 182 | 183 | 184 | stored byte; selectable 0–11 |
| LFO phase offset | 185 | 186 | 187 | 188 | 189 | 190 | byte 0–127 |

The editor is a selected-owner detail surface inside the approved 1280 × 800
shell. This is the deliberate departure from the conceptual six-column mockup:
all six owner buttons remain visible, while the selected owner's complete nine
controls receive usable value and route-label widths. No control is shrunk into
an unreadable six-column cell.

The immutable production contract is
`LXRModulationEditorContract.h`. It records host ID, original selector and LFO
hint provenance, owner, role, raw range, default, control identity, automation
and persistence. It describes existing parameters only and does not add a GUI
state authority or expose transient DSP modulation outputs.

## Typed routing

Velocity parameters 228–233 retain original selector provenance 149–154. LFO
parameters 234–239 retain selector provenance 167–172 and voice-hint provenance
161–166. The widget stores and displays the original 16-bit identity, never its
visible option index. Identity zero is OFF.

The selectable sets are derived through `RouteHostControls::decode`, producing
the accepted 107 owner-specific rows (18, 18, 18, 18, 18 and 17). Identity 203
is the highest supported target and passes exact-DLL readback. Identity 204 is
a known original row outside the supported route set; 205 and above are
reserved. Invalid, reserved and cross-owner identities remain unselectable and
the routing authority republishes the previous accepted route. The control can
display an unsupported raw value without silently coercing it.

The eight waveform identities retain firmware order:
`sin`, `tri`, `sup`, `sdn`, `sqr`, `rnd`, `xup`, `xdn`. Long UI labels are
presentation-only. Rate, amount and phase preserve exact `raw / 127`
normalization; no invented hertz conversion is shown.

Mouse, wheel, keyboard and exact-entry changes use the established balanced
begin/edit/end path. View and owner navigation, readback and restoration do not
produce gestures. The exact private-DLL test verifies all six lanes, waveform
readback, velocity and LFO routes, identity 203, OFF wrapping, external
automation and multiple-instance isolation.

## Kits + Morph

The Kits + Morph view calls the accepted P17 and P18 production interfaces. It
does not parse or encode `.snd` data in GUI code.

The Kit section shows current embedded identity and distinguishes identity
present from absent. `FormatName` preserves all eight raw bytes, trims trailing
zeroes for display only, escapes nonprintable bytes as uppercase `\xNN`, and
uses `<unnamed>` for an all-zero identity. Filename and embedded identity are
never conflated. Import and export use the existing exactly-236-byte codec,
bounded validation, coherent publication, no-overwrite creation and atomic
commit.

The Morph section shows target presence and binary identity, and exposes Load,
Clear, Morph Amount and Export Morphed Kit. Parameter 245 remains the sole
Morph Amount authority with exact 0–255 normalization. Clearing B retains this
amount while returning effective DSP values to A. The UI explains—without
changing—the original denominator-256 endpoint quirk. MorphSnd export continues
to use P18-C's coherent source/target/amount projection and excludes locks,
velocity/LFO outputs, transport, PCM and note-trigger state.

Standard VST3 preset support is described as host-owned. There is no custom
preset browser, factory-preset claim or host-filesystem claim. The previous
sequencer-bank button was removed from this surface; no graphical sequencer was
implemented.

## Status, restoration and concurrency

The existing typed P17/P18 results remain authoritative. The surface presents
success, cancellation, invalid size/field/route, unsupported field/route,
read/write failure, existing destination, Busy, stale transaction, unavailable
host state and no-target results without collapsing them into a generic error.
Each completed or cancelled operation replaces the prior operation status; a
new failure cannot leave stale success visible.

File dialogs and operations run on the UI thread. Cancellation is explicitly
reported as cancellation. The exact-DLL test invokes the same P17/P18 callbacks
used by the buttons; it does not claim graphical dialog operation.

LXRS v1/v2/v3 and the 51,065/51,089-byte historical streams remain unchanged.
Neither morph target nor amount was added to the LXRS format. A host-applied
amount does not fabricate a target after restore. Readback retains P19-B's
three per-instance 64-bit atomics, even/odd generation, four bounded attempts
and 30 Hz refresh; P19-D adds no audio-thread GUI scan, dynamic menu, file I/O,
lock or allocation.

## Keyboard and accessibility

Tab order includes navigation, owner buttons, visible production operations and
the selected owner's nine modulation fields. Arrows adjust source-defined
identities; Home/End select exact endpoints; Enter/Space invokes navigation or
an operation and exact value prompting remains available. Focus is visible.
Operation buttons have stable control tags and are keyboard-triggerable.

Pinned iPlug2 does not establish native Windows screen-reader semantics for
these custom-drawn controls. Screen-reader, DPI and direct mouse/keyboard host
acceptance remain P19-F work and are not claimed here.

## Verification

Independent P19-D tests pass under GCC and VS LLVM at O0 and O2:

- Modulation: 3,160 assertions per configuration; 12,640 total.
- Kits/Morph: 66 assertions per configuration; 264 total.
- Exact private DLL: 254 assertions, 1280 × 800, 54 controls, six owners,
  production operations, restoration, lifecycle, isolation and 26-sample PDC.
- Steinberg validator: 47 passed, zero failed.

Fresh retained host suites pass P12c, P15, P16, P17, P18 and P19-B/C. Fresh
native O0/O2 suites pass synthesis, original-C filter/snare differential,
frozen PCM, resampler quality and latency. Dedicated P15 velocity/LFO, P17
codec/runtime/management/identity, P18 contract/runtime/management, P16-D lock
and P19-A/B/C contract campaigns also pass.

The private Release x64/v145 candidate was built with MSBuild 18.10.1 and both
pre/post build deployment events disabled. Its SHA-256 is
`AD103CFC7219B1EE9CAAD01B6EE4109F671FCBE849ED3EDAAFBE41B856995B49`
(1,396,224 bytes). The protected installed hash remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`.

Audio-path allocation probes remain zero; the retained exact-DLL processing
stack watermark is 20,288 bytes on a 1 MiB stack and bounded morph export uses
4,656 bytes. UI rendering uses pinned iPlug2's UI-thread allocation model and
was not instrumented for a total allocation count, so no zero-render-allocation
claim is made.

## Direct REAPER gate

An isolated REAPER 7.80 x64 profile pointing at the then-current private P19-D
bundle was launched. A unique task window was resolved, but computer-use attachment
timed out before a screenshot or accessible state. Zero clicks and zero keys
were sent. Direct layout, file-dialog, mouse, keyboard, DPI, audio and project
reopen observations are therefore **BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE**
and not claimed. The profile was then rebound to the final hash-tested candidate;
no final-candidate visual interaction is claimed.

Evidence is under `out/p19d/`. P19-E may now consume the stable shell,
selection/focus layer, parameter binding, operation-button pattern and bounded
readback; it must implement only the graphical sequencer and must not change the
P19-D modulation, kit, morph or DSP contracts.
