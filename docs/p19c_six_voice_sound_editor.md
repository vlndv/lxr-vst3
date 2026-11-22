# P19-C six-voice Sound editor

## Classification

P19-C is a **development engineering PASS**. Direct REAPER graphical
interaction is separately **BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE** and is
not represented as visual acceptance. P16-F release certification remains
independently blocked.

The certified private Release x64/v145 module is
`D4D88B935CA308DA2024039B97C916BD8B55FD26DCA4AEFDA67DB50EDC92E2B1`
(1,387,008 bytes). It was built with post-build deployment disabled. The
protected installed module remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`.

## Source-grounded boundary

The production parameter registry remains exactly 246 host parameters, IDs
0–245. The original patch remains 228 bytes and no host or serialized GUI state
was added. Comparing the pinned firmware voice pages, `ParameterArray` ownership,
the production descriptors and the P19-A inventory identifies 162 Sound controls:

| Synthesis owner | Controls | Source distinction |
|---|---:|---|
| Drum 1 | 27 | Main/mod oscillator, FM frequency/amount and Mix/FM mode |
| Drum 2 | 27 | Own oscillator, envelopes, transient, filter and output bytes |
| Drum 3 | 27 | Own oscillator, envelopes, transient, filter and output bytes |
| Snare | 26 | Tone/noise source, repeat and P12-C snare signal path |
| Cymbal | 27 | Main plus two modulator oscillators and repeat |
| Hi-hat | 28 | One shared owner with closed/open decay and note articulation bytes |

The exact ID-by-owner matrix and every non-Sound classification are in
`out/p19c/sound_parameter_coverage.csv`. IDs 128–133 (filter drive), 134–136
(drum Mix/FM) and 137–142 (velocity-to-amplitude) are source-owned Sound
controls. P19-A had categorized these under the broader modulation concept; the
inventory and generated mockups are corrected. Fifty-four typed velocity/LFO
routing controls remain deferred to P19-D. Sequencer/performance, kit/morph and
compatibility fields are not duplicated in Sound.

Closed and open hi-hat remain two sequencer trigger articulations of one
physical synthesis owner. Decay IDs 60/61 and note IDs 226/227 are grouped in
the shared Hi-hat page. No seventh synthesis engine, authority or voice selector
was introduced.

## Production control architecture

`LXRSoundEditorContract.h` is the single fixed Sound binding and geometry
contract. Each row contains an existing parameter ID, stable control ID, owner,
logical section, control kind and optional enumeration domain. Compile-time
checks require 162 unique native IDs, exact per-owner counts, valid native
descriptors and all panel/control bounds inside the 1280 × 800 canvas.

`LXRSoundControl` is the reusable iPlug2 vector control. It presents continuous
integer knobs, level sliders, enumerated selectors, source-backed booleans and
MIDI-note values with an accepted-value display. Section headings and panel
backgrounds are shared. Unsupported restored enum bytes are rendered in the red
error treatment as `Unsupported [n]`; direct unsupported entry is rejected
without changing accepted state.

All interaction goes through the existing iPlug2 parameter path. Mouse editing
uses the framework gesture lifecycle; wheel, double-click entry and keyboard
adjustment explicitly balance begin/send/end. Construction, page selection,
voice selection, focus movement, state restoration and host readback do not
initiate edits. Widget-local values are never a competing musical authority.

The original integer contract is preserved:

`normalized = raw / 127.0`

`raw = floor(normalized * 127.0 + 0.5)`

All accepted values display as exact integers or source labels. Non-finite and
out-of-range normalized values reject. Exhaustive byte round trips, endpoints,
defaults, midpoint, adjacent values, repeated edits, automation, restore and
independent instances pass at GCC O0/O2 and VS LLVM Od/O2.

The source-backed selection domains are:

- waveform 0–5: Sin, Tri, Saw, Rec, Noi, Cym;
- filter 0–7: LP, HP, BP, UBP, Nch, Pek, LP2, off;
- transient 0–13: Snp, Ofs, Clk, Ck2, Tik, Kik, Rim, Drp, Hat, Clp, Kk2,
  Snr, Tom, Sp2;
- audio output 0–5: St1, St2, L1, R1, L2, R2;
- drum source mode: 0 FM, 1 Mix;
- velocity-to-amplitude: 0 Off, 1 On.

Restored values outside those user-selectable domains are retained exactly and
shown deterministically rather than silently normalized into a different stored
identity.

## Layout, lifecycle and accessibility

The fixed production editor remains 1280 × 800 and non-resizable. The selected
Sound page uses six panels between y=198 and y=684: Source, Pitch Envelope,
Amplitude Envelope, Transient, Filter and Output. Every one of the 162 runtime
control bounds is checked against its panel and the canvas. Overview no longer
attaches a second editable instance of the owner level controls, so each Sound
parameter has one visible editable binding.

All nine P19-A SVG concepts were regenerated with an explicit Segoe UI/Arial
font stack and audited for canvas bounds. The four voice concepts were compacted
into the approved height; the step-detail note was wrapped. The sequencer concept
keeps its 7 × 16 overview bounded and leaves per-step fields in the separate
detail panel for P19-E. Increasing the canvas height was not used as a workaround.

Tab traversal visits only the selected owner's controls in deterministic binding
order. Focus is visible. Arrow/wheel editing provides one-byte precision; coarse
keyboard adjustment and exact text entry are available without a drag-only
requirement. Labels and displayed accepted values remain visible. Pinned iPlug2
still does not source-prove a complete native Windows accessibility tree for
arbitrary vector controls, so screen-reader certification remains P19-F.

The P19-B readback design is unchanged: three per-instance lock-free 64-bit
atomics, even/odd generation, at most four snapshot attempts and 30 Hz idle
refresh for non-parameter status only. Parameter feedback remains event-driven.
No mutable Engine internals or pattern bank are read by the editor.

## Verification

`P19CSoundEditorTest.cpp` uses independent firmware/source-derived owner arrays,
not a second generated copy of the production binding registry. It passes 2,116
checks in each of GCC O0, GCC O2, VS LLVM Od and VS LLVM O2 (8,464 total).

`P19CVST3SoundEditorTest.cpp` loads the exact private DLL, attaches its real VST3
view to a native window and exercises the production navigation, owner selection
and Sound controls. It verifies the unchanged 0–245 parameter prefix,
GUI-to-controller edits, controller-to-widget readback, gestures, state restore,
reopen, deterministic audio, isolation and 26-sample PDC. It passes 721/0.

All retained exact-DLL state, runtime, routing, transport, sequencer, kit, preset,
morph, mute and controller-sync tests pass; Steinberg validator passes 47/47.
Relevant P15–P19 standalone/native suites pass. Original-C filter comparisons
remain bit-exact for 80,000 blocks per optimization, snare reference renders have
zero mismatched samples, and velocity/LFO PCM campaigns retain zero mismatch.
No frozen PCM reference was changed. LXRS v1/v2/v3, 51,065/51,089-byte streams,
the 65,536-byte limit, kit identity, morph semantics, transport and bypass remain
compatible.

P19-C changes only the GUI/control layer. Native processing allocation tests
remain zero and exact-DLL audio is deterministic across editor open, switch,
close, reopen and independent instances. The prior P19-B exact processing probe
remains applicable to the unchanged processing call graph: 8,252/0, zero observed
processing allocations and a 20,288-byte maximum observed stack watermark.

## REAPER and remaining work

An isolated REAPER 7.80 x64 profile loaded the then-current P19-C engineering
candidate and the unique `01_drum1` task window was resolved. Computer-use
attachment timed out twice before a screenshot or accessible UI state was
returned. Zero clicks and zero keys were sent. After the keyboard enum-endpoint
correction, `out/p19c/reaper-profile-002` was prepared against the final exact
candidate, but graphical access remained unavailable. Direct final-candidate
visual bounds, voice selection, manipulation, automation, save/reopen and DPI
checks are therefore NOT RUN and NOT CLAIMED.

P19-D retains the complete modulation editor. P19-E retains sequencer editing
and its bounded detail-panel behavior. Broader host visual/DPI and native
accessibility acceptance remain P19-F. No full Kits/Morph redesign, modulation
editor, sequencer facade, new synthesis feature or sample playback was added.

Evidence is under `out/p19c/`; the entry points are `verification_report.md`,
`integrity_report.md`, `sound_parameter_coverage.csv`,
`private_binary_manifest.json`, `protected_install_manifest.json` and
`regression_matrix.csv`.
