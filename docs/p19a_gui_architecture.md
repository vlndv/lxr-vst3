# P19-A — complete GUI architecture and interaction specification

Status: **design and standalone-contract milestone only**. No production GUI,
DSP, sequencer, state-format, or host-operation implementation is changed.

## Baseline and authority

The verified baseline is branch `p12b-4c-fir-resampler` at
`be35649be6ae31c07ae5cbd6dbe225a20f4d5422`, matching local, tracking, and live
remote HEAD. Firmware is pinned to `dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`,
iPlug2 to `d54f69050f517e43b941d88c2a170f0a840b9ee4`, and the VST3 SDK to
`3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96` with its recorded submodules.

The design is derived from `LXRParamContract.h`, `LXR.cpp`, the P15 routing
contract, P16 pattern/transport authorities, and the P17/P18 production
operation interfaces. The original v0.37 firmware remains the behavioral
authority. Proposed GUI adapters are labelled as such and do not imply an
existing production operation.

## Current editor audit

The current editor is a fixed 600×600 iPlug2 `IGraphics` surface
(`PLUG_HOST_RESIZE=0`, 60 FPS). It contains whole-kit import/export, current kit
text, morph target load/clear/export, parameter 245, operation text, and LXQB
bank import. It does not expose synthesis, modulation routing, pattern editing,
or transport controls. The editor already delegates kit and morph actions to
the same production interfaces used by exact-DLL tests; that separation is the
model for the complete GUI.

The host registry contains exactly 246 parameters:

- 0–227: original patch bytes;
- 228–239: twelve validated 16-bit route projections;
- 240–242: sequencer enable, clock mode, and internal BPM;
- 243–244: roll rate and seven-track held mask; and
- 245: Morph Amount.

The design inventory assigns every ID a deterministic control ID, owner,
widget, validation rule, and accessibility name. Twenty-three registered fields
remain intentionally hidden: reserved/protocol bytes 0, 5, 97, 98, 127 and the
eighteen legacy destination/hint bytes 149–154 and 161–172. The latter are not
lost functionality: the GUI uses full-width typed IDs 228–239 instead.

## Application shell and information architecture

The shell has one persistent header, five primary views, and one contextual
status strip. Primary navigation order is Overview, Sound, Modulation,
Sequencer, Kits + Morph. Voice selectors D1, D2, D3, SN, CY, CH, and OH remain
in a stable location; CH and OH select articulation context for one shared HH
synthesis owner.

The header shows raw kit identity display, active/pending pattern, clock mode,
validated tempo readback, playback indication, and last operation severity.
Host playback state is read-only. The plugin never presents a control that
claims to start or stop the DAW.

### Overview

Overview is the performance dashboard. It contains:

- current kit identity and import/export shortcuts;
- sequencer enable, Internal/Host Sync, editable internal BPM only in Internal
  mode, and read-only valid host BPM/running state;
- eight pattern selectors showing active versus pending state;
- seven activity/mute/select strips and a compact 7×16 gate summary;
- roll rate and momentary track roll pads;
- Morph Amount, target identity/presence, and kit/morph operation status; and
- output/master voice levels represented by the existing voice parameters.

There is no new master-gain host parameter. “Main output” is a compact view of
the six existing volume controls plus the existing all-voice decimation field;
any future true master gain is an extension and is not shown as implemented.

### Sound

Sound is a selected-owner editor with stable sections: source, pitch/envelope,
transient, filter, and output. Controls that do not belong to the selected
engine are absent rather than disabled filler. Exact integer readback is always
shown. Waveform selectors offer implemented built-in values; source-valid but
unsupported user-SD waveform values remain visible as “Unsupported stored
value” when loaded and cannot be newly selected.

Drum 1–3 share one layout. Their per-owner IDs cover primary and mod oscillator
wave/pitch, FM amount/frequency and mix/PM mode, pitch envelope, amplitude
envelope, transient, filter/type/drive, volume/pan/distortion/decimation,
velocity behavior, LFO behavior, output route, and MIDI note.

Snare uses its actual tone oscillator, noise frequency/mix, pitch envelope,
repeatable amplitude envelope, noise-path filter, transient, and output fields.
Cymbal exposes three source oscillators and two FM gains, a repeatable amplitude
envelope, filter, transient, and output. Hi-hat exposes its shared three-
oscillator engine with separate closed/open decay and MIDI-note articulation
fields. CH/OH never imply two independent synthesis engines.

The exact per-ID ownership is the machine-readable inventory under
`design/p19a/parameter_control_inventory.csv`.

### Modulation

Modulation is six owner lanes, not an unrestricted matrix. Each lane exposes:

- velocity destination (IDs 228–233) and amount (143–148);
- LFO waveform (155–160), rate (115–120), amount (121–126), sync (179–184),
  retrigger (173–178), and phase offset (185–190); and
- LFO destination (234–239).

Selectors show a friendly label plus original target identity. OFF is explicit.
The list is generated from the 107 reviewed destination rows and filtered by
source and synthesis owner. Display order is never used as stored identity.
Invalid, cross-owner, global, reserved, or unavailable tokens retain the prior
accepted route and show a typed error. Values above 127 remain intact.

### Sequencer

The primary grid is seven rows by sixteen main steps. Each cell communicates
four independent facts without relying on color alone: main gate, stored detail,
current selection, and playhead. A small detail mark remains visible when the
main gate is off but any of its eight physical positions contains non-default
data. Turning off a gate never clears that data.

The view contains pattern 1–8, active/pending distinction, track length 1–16,
rotation 0–15, mute, roll, shuffle 0–127, repeat 0–127, next fixed pattern 1–8,
random-next codes 8–14 labelled Random 1–2 through Random 1–8, Euclidean
generation, copy, and clear. Copy and clear dialogs name source, destination,
and scope. Pattern copy explicitly states that destination chain metadata is
preserved by the source contract.

Selecting a main cell opens Step Detail. It exposes:

- independent main gate;
- eight physical-position toggles;
- note 0–127 with note-name readback;
- encoded active/velocity value 0–127;
- probability 0–127;
- two original automation selectors and absolute values 0–127.

Help text preserves source quirks: probability zero passes on random zero;
inactive main gates retain positions; slot 2 wins for duplicate targets; and an
accepted lock can persist until a later accepted command or defined reset
boundary. OFF is represented by source sentinel 0/255 as appropriate. Only
currently safe original runtime targets are offered; route proxies and host-only
IDs are not lock targets.

### Kits + Morph

Kit operations call `ImportKitFile` and `ExportKitFile`. The view displays the
coherent eight raw name bytes using printable ASCII, uppercase `\xNN`, trailing
zero display trimming, and `<unnamed>`. Standard VST3 host presets remain
host-owned; the UI offers explanatory help rather than a duplicate browser.

Morph actions call `LoadMorphTargetFile`, `ClearMorphTarget`, and
`ExportMorphedKitFile`; amount binds only to parameter 245. Target presence and
identity come from the accepted target snapshot, never source A. Typed Busy,
stale, cancellation, codec, route, no-overwrite, and I/O results appear in a
nonmodal status region. The view states plainly that target B and amount are
transient and a project reload may restore a nonzero amount with no loaded
target.

## Interaction contract

All parameter controls support click/tap, arrow-key adjustment, direct numeric
entry, and host automation readback. Drag is an optional acceleration path, not
the only path. Shift+arrow uses one integer step; Page Up/Down uses a documented
coarse step; Home/End selects bounds; double-click restores the registered
default after a normal host gesture. Enumerations use labelled menus and type-
ahead. Momentary rolls expose keyboard press/release semantics and always issue
a release on focus loss or editor close.

Tab order is header, primary navigation, contextual selector, main work area,
then status. Arrow keys move within tab lists, voice tabs, pattern tabs,
sequencer rows, and substep groups. Escape closes the current popup without
discarding unrelated edits. Destructive pattern operations require one bounded
confirmation; ordinary gates and values do not.

Every element uses a stable ASCII ID: `lxr.nav.*`, `lxr.voice.*`,
`lxr.param.NNN`, `lxr.seq.pP.tT.sSS`, `lxr.seq.detail.subN`, and
`lxr.operation.*`. Visible focus is a 3 px high-contrast ring. Status never
depends only on color or animation.

## Accessibility findings

Pinned iPlug2 provides control tags/groups, tooltips, keyboard callbacks,
text-entry facilities, DPI/backing scale, and vector rendering. Source search
found no complete Windows native accessibility-tree or screen-reader contract
for arbitrary `IGraphics` controls. P19 therefore must not claim that drawn
knobs automatically expose UI Automation roles, names, values, or state.

The minimum supported P19 implementation supplies complete keyboard operation,
visible focus, persistent text labels/value readback, ≥4.5:1 normal-text
contrast, scalable type, nonmodal error text, and a deterministic read-only GUI
test snapshot. Native screen-reader exposure remains a separately tested gate.
If pinned iPlug2 cannot expose the required native tree, the supported
alternative is platform-native child controls for focus/value proxies or a
small companion accessible list editor that invokes the same production
operations. A framework upgrade is not authorized by P19-A.

The GUI test surface is read-only. It reports instance token, selected view,
kit fingerprint, pattern generation/fingerprint, focused control ID, parameter
readback, and last operation. It cannot mutate DSP or bypass validation.

## Size and scaling policy

The concept canvas is 1280×800 logical pixels. The implementation target is:

- default: 1280×800;
- minimum if host resizing is enabled: 1024×640;
- preferred scale choices: 80%, 100%, 125%, and 150%;
- minimum normal text: 12 logical px; control labels 11 px only when secondary;
- minimum keyboard/touch target: 32 px with 44 px effective coarse-pointer
  target where the host supplies touch input.

Current production has `PLUG_HOST_RESIZE=0`; arbitrary host resize is therefore
not an existing capability. P19-B should first implement one fixed 1280×800
logical layout using vector controls and iPlug2 backing-scale support. Enabling
host resize and `IGraphics` layout-on-resize is a later P19-F gate after REAPER
window-mode testing. At 1024×640 the header and navigation remain fixed, Sound
groups wrap to fewer columns, and Sequencer retains all 16 columns while its
detail panel moves below; text is never scaled below its minimum.

## UI-to-authority architecture

Parameter widgets use the existing host parameter path and standard begin/edit/
end gestures. They never write Engine or `ParameterArray` directly. Route
selectors publish only validated tokens through IDs 228–239. Kit and morph file
dialogs invoke existing non-real-time production operations. Pattern editing
requires a narrow future `SequencerEditFacade` around
`PatternStateAuthority::tryBeginEdit/commit`; it returns Ok, Busy, Stale, or a
typed validation error and never exposes a mutable published bank.

P19-B/E must add a fixed, per-instance `GUIReadbackSnapshot` adapter. The audio
owner publishes only bounded scalar state: generation, active/pending pattern,
seven positions/activity bits, mute/roll masks, validated host tempo/running
flags, and operation generations. Kit/morph identity remains read through the
existing coherent non-audio snapshots. The UI never dereferences mutable
`TriggerScheduler`, Engine voices, or live pattern slots.

Publication order remains:

`base A → morph projection → sequencer locks → velocity → LFO → rendering`.

The GUI refreshes controller-driven parameters on framework notification and
polls readback snapshots at 30 Hz while visible. Playhead/activity may refresh
at 30 Hz; meters use 30 Hz fixed atomics. Hidden editors stop polling and audio
continues unchanged. A generation change invalidates only dependent controls.
No full 50,418-byte bank copy or serialization occurs in an idle callback.

## Visual system

The design is dark industrial without reproducing the original panel artwork.
Core colors are graphite `#101318`, panel `#181D24`, raised `#222934`, divider
`#353E4B`, primary text `#EDF2F7`, secondary `#9AA7B5`, amber action/step
`#F1B84B`, cyan modulation/detail `#49C6D4`, green success/playhead `#70C48F`,
red error/destructive `#EF6A67`, and pale focus `#F8DF8C`. Every semantic color
also has shape/text treatment.

Use Segoe UI or the shipped Roboto resource, 400/500 weights only, tabular
figures for changing values, an 8 px base spacing grid, 1 px dividers, 5–8 px
corner radii, and vector-drawn knobs/cells. No large bitmaps or continuous
decorative animation are required. Hover adds a subtle raised fill; focus uses
the explicit ring; automation uses a small `A` marker plus parameter-value
outline; disabled controls retain readable labels and explain why.

Nine reproducible SVG concept sheets are generated from
`design/p19a/mockup_spec.json` by `generate_mockups.py`. Each is visibly marked
as a design concept, not a product or REAPER screenshot.

## Performance budgets

- UI snapshot and dirty-control poll: 30 Hz, fixed work.
- Parameter feedback: event-driven, coalesced by control ID.
- Playhead/activity and meters: at most 30 Hz; no per-sample UI feed.
- Layout: editor open, explicit resize, or view change only.
- Pattern candidate copy: only on an edit transaction, never refresh.
- File dialogs/codecs: main/non-audio thread only.
- Hidden editor: no meter/playhead invalidation.
- Audio path: zero new allocation, mutex, I/O, logging, or UI callback.

P19 implementation must instrument maximum UI tick time and redraw count; an
average measurement cannot be reported as worst-case latency. The initial
engineering target is a bounded ≤4 ms UI update at 30 Hz on the certification
machine, with separately recorded maximum and percentile data.

## Roadmap

- **P19-B — shell and core controls:** fixed logical canvas, navigation,
  design tokens, overview, deterministic control IDs, host parameter widgets,
  and read-only GUI snapshot skeleton.
- **P19-C — six-voice editor:** all visible source controls, exact readback,
  supported enumeration menus, CH/OH shared ownership, automation feedback.
- **P19-D — modulation and Kits/Morph:** owner-filtered 107-row selectors,
  full-width tokens, existing P17/P18 production operations and typed status.
- **P19-E — sequencer editor:** edit facade, 7×16 grid, substeps, locks,
  length/rotation, chain, shuffle, mute, rolls, Euclidean, copy/clear.
- **P19-F — performance/accessibility/host acceptance:** keyboard completeness,
  scaling/optional resizing, readback/test surface, REAPER real-window testing,
  performance measurements, and accessibility limitation resolution.

P19-F does not inherit or relabel the outstanding P16-F release gates. Those
remain separately BLOCKED until their exact host observations are completed.

## P19-C source reconciliation

P19-C rechecked the conceptual inventory against the pinned firmware voice
menus and production ownership switch. IDs 128–133 (filter drive), 134–136
(Drum 1–3 Mix/FM) and 137–142 (velocity-to-amplitude) are synthesis-owner Sound
controls, not P19-D routing controls. The corrected Sound view contains 162
unique parameters across six owners (27/27/27/26/27/28); the deferred typed
velocity/LFO Modulation view contains 54. The total 246-parameter registry and
all host IDs remain unchanged. The regenerated SVG concepts also correct the
original 800-pixel overflow without increasing the canvas.
