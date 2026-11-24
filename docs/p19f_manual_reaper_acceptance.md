# P19-F manual REAPER graphical acceptance

This package is a **pending manual acceptance procedure**, not evidence that the
graphical gate passed. Record every result and attach screenshots from the actual
plugin window. Do not substitute mockups or the standalone test host.

## Exact candidate

- Host: REAPER 7.80 x64.
- Plugin bundle: `out/p19f/candidate-certified/bundle/LXR.vst3`.
- DLL: `Contents/x86_64-win/LXR.vst3`.
- Expected DLL SHA-256: copy from
  `out/p19f/private_binary_manifest.json` immediately before the session.
- Editor: fixed 1280 x 800; host resizing disabled.
- PDC at 48 kHz: 26 samples.

Do not install this candidate or replace the protected installed plugin. Use an
isolated REAPER configuration whose `vstpath64` contains only the candidate
bundle directory. Confirm the hash before and after the session.

## Isolated launch

1. Close no existing REAPER process. Start a new instance with `-newinst` and a
   private `-cfgfile` under `out/p19f/reaper-profile-*`.
2. Set dummy audio to 48,000 Hz and a bounded block size. Do not change the
   normal REAPER profile or Windows display settings.
3. Rescan the isolated VST path and open the supplied
   `P19F_GUI_QUALIFICATION.rpp`, or create one instrument track and insert
   `LXR (Sonic Potions)` from that path.
4. Verify the loaded module path with Process Explorer, REAPER diagnostics, or
   another trustworthy module inventory. Hash the loaded file and compare it to
   `private_binary_manifest.json`.

## Screen-by-screen checklist

Record PASS or FAIL and a short observation for every item.

### Shell and Overview

- [ ] Window is the isolated REAPER process and exact candidate.
- [ ] Editor content is 1280 x 800 without clipping or corruption.
- [ ] Header, five navigation tabs, voice row, content, and footer are visible.
- [ ] Overview transport, tempo, output, morph, and status readbacks are legible.
- [ ] Focus indication is visible after pressing Tab.

### Sound

- [ ] D1, D2, D3, SN, CY, and HH each select a distinct owner panel.
- [ ] Representative waveform, pitch, envelope, transient, filter, level, pan,
      distortion, decimation, output, and MIDI-note controls render correctly.
- [ ] A mouse adjustment creates one balanced automation gesture.
- [ ] Tab/Shift+Tab traverse visible controls only.
- [ ] Arrow keys adjust; Home/End reach supported endpoints; Enter opens exact
      entry where supported; Escape cancels without changing the value.
- [ ] Unsupported restored waveform storage is labelled rather than silently
      remapped.

### Modulation

- [ ] Six owners are selectable.
- [ ] Velocity amount/destination, LFO waveform/rate/amount/destination,
      retrigger, sync, and phase offset are reachable.
- [ ] Typed route selection shows the original identity, including high values.
- [ ] An invalid destination is rejected and the prior accepted value remains.
- [ ] Host automation updates an open control and reopening shows the accepted
      controller value.

### Sequencer

- [ ] Seven tracks, eight patterns, sixteen main steps, and eight substeps render.
- [ ] Track, pattern, main-step, and substep selection are visually distinct.
- [ ] Main gates and stored substeps remain independent.
- [ ] Note, velocity, probability, two locks, length, and rotation edit correctly.
- [ ] Probability zero retains inclusive-zero semantics.
- [ ] Mute is transient; held roll clears on release and editor close.
- [ ] Shuffle, chaining, Euclidean generation, pending/active pattern, copy, and
      clear scopes behave as labelled; destructive actions require confirmation.
- [ ] Playhead/activity is visible during playback without drawing corruption.

### Kits + Morph

- [ ] Kit Import accepts an exact 236-byte `.snd`; Cancel leaves state unchanged.
- [ ] Kit Export refuses overwrite and reports a typed success/error status.
- [ ] Morph target load and clear update target identity without changing source A.
- [ ] Morph Amount spans exactly 0..255 and automates as parameter 245.
- [ ] MorphSnd export is no-overwrite; cancellation and errors are readable.
- [ ] Reopening or project restoration does not fabricate transient target B.

## Lifecycle, project, and two-instance checks

1. Start playback and repeatedly switch all views and owners.
2. Close and reopen the editor ten times. Audio must continue and the Engine must
   not reset or restart the sequencer.
3. Close the editor, automate representative native, typed-route, transport, and
   Morph Amount parameters, then reopen and verify readback.
4. Save as a new project in the isolated profile, close that project normally,
   reopen it, and verify accepted persistent state. Do not expect morph target B,
   transient mute, or held rolls to persist.
5. Add a second LXR instance. Edit different voices, patterns, kit identity, and
   Morph Amount values. Confirm no state crosses between instances.
6. Observe REAPER PDC: 26 samples at 48 kHz.

## Required screenshots

Capture the full verified plugin window for:

1. Overview and module-path/hash evidence.
2. Sound for each of the six owners (six images).
3. Modulation with a high typed destination selected.
4. Sequencer with track/step/substep selection and visible playhead.
5. Kits + Morph after target load and after clear.
6. Visible keyboard focus on a custom control.
7. Project reopened with restored values.
8. Two independent instances side by side.
9. REAPER PDC readback.

Use lossless PNG. Record monitor scale, resolution, REAPER version, UTC time,
candidate hash, and process ID in the session notes.

## Result template

```text
Tester:
UTC date/time:
Windows version:
Monitor(s), resolution(s), scaling:
REAPER version and process ID:
Candidate DLL path:
Candidate SHA-256:

Shell/Overview: PASS | FAIL —
Sound: PASS | FAIL —
Modulation: PASS | FAIL —
Sequencer: PASS | FAIL —
Kits + Morph: PASS | FAIL —
Keyboard-only: PASS | FAIL —
Mouse interaction: PASS | FAIL —
Focus indication: PASS | FAIL —
Save/reopen: PASS | FAIL —
Two instances: PASS | FAIL —
PDC 26: PASS | FAIL —
Visual clipping/corruption: PASS | FAIL —

Overall direct REAPER GUI: PASS | FAIL
Screenshot paths:
Defects/reproduction:
```
