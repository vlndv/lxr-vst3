# P20-A manual final-candidate REAPER acceptance

This is a **PENDING** procedure, not a graphical PASS. Record actual observations
and screenshots from the verified plugin window; leave unexecuted rows `NOT TESTED`.

## Fixed identity

- REAPER 7.80 x64.
- Bundle `out/p19f/candidate-certified/bundle/LXR.vst3`.
- DLL `Contents/x86_64-win/LXR.vst3`.
- SHA-256 `F8C8CBC816EAB5F51B79909DECD33808A0D24FD85ECD653B4EBD8091C398FA94`.
- Size 1,437,184 bytes.
- Source fingerprint `84AAA970E4EA4FBD24F69DF04D2F7CDD878E9467CF367B3678BA34715A10178C`.
- Fixed editor 1280 x 800; PDC 26 samples at 48 kHz.

Hash the loaded DLL before and after. Stop if it differs. Do not install the
bundle or overwrite the protected installed plugin.

## Isolated setup

1. Leave normal REAPER instances/profiles untouched.
2. Use `out/p20a/reaper-profile-001/config/REAPER.ini`, or a new profile below
   `out/p20a/` with only the candidate bundle parent in `vstpath64`.
3. Use dummy audio at 48,000 Hz and a bounded block size (prepared value 127).
   Do not change Windows display scaling.
4. Launch `-newinst -cfgfile <private-REAPER.ini>` and open
   `P20A_FINAL_CANDIDATE.rpp`.
5. Record process ID/title, REAPER version, loaded module absolute path/hash,
   resource manifest, and observation method.
6. Confirm the normal profile and protected VST3 remain unchanged.

## Five views, mouse, and keyboard

- [ ] The verified editor is 1280 x 800 with header, five tabs, owner row,
      content, status/footer, labels, and readbacks visible and unclipped.
- [ ] Overview, Sound, Modulation, Sequencer, and Kits + Morph render without
      stale controls or corruption; close/reopen preserves accepted state/audio.
- [ ] Adjust a Sound knob and slider with the mouse; hit targets align and each
      action produces one balanced host gesture.
- [ ] Select D1, D2, D3, SN, CY, HH; selection alone edits nothing and CH/OH
      remain one engine.
- [ ] Tab/Shift+Tab visit only visible controls with aligned focus indication.
- [ ] Test arrows, Enter, Space, Escape, Home, End, and exact entry; Escape cancels.
- [ ] Automate while open, then closed; reopen and verify controller readback.

## Sound and Modulation

- [ ] For all six Sound owners check waveform, pitch/filter, amp/pitch envelopes,
      MIDI note, level, exact entry, keyboard endpoints, and automation readback.
- [ ] Verify all eight original LFO waveform identities and rate/amount/phase.
- [ ] Select OFF zero and valid identity 203.
- [ ] Reject unsupported 204, reserved 205+, and cross-owner destinations while
      retaining the prior accepted route.
- [ ] Confirm menu positions are not mistaken for stored identities.

## Sequencer

- [ ] Select all seven tracks and representative entries in all eight patterns.
- [ ] Toggle main steps/select substeps; gate OFF must preserve physical records.
- [ ] Edit note, velocity, probability 0/127, and two lock slots; check duplicate
      precedence, length, rotation, shuffle, mute, held roll/release, chaining,
      Euclidean generation, and explicit copy/clear prompts/scopes.
- [ ] Observe active/pending pattern and playhead during audio.
- [ ] Save/reopen; held rolls and transient mute must not restore.

## Kits, Morph, and dialogs

Use disposable files under `out/p20a/manual/`.

- [ ] Kit Import is parented/in front; Cancel returns focus and changes nothing.
- [ ] Accept a valid 236-byte `.snd`, preserve its binary name, reject malformed
      input visibly, export exactly 236 bytes, and enforce no-overwrite.
- [ ] Load/replace target B; invalid replacement retains accepted B; clear keeps A.
- [ ] Sweep Morph Amount 245 through exact 0–255 and automate it.
- [ ] Export MorphSnd with original interpolation/no-overwrite; reopen confirms
      transient B was not fabricated.
- [ ] For all four dialogs record parent, z-order, coordinates, focus return,
      edge behavior, selected path, and success/error status.

## Restoration, transport, two instances, and audio

1. Check internal/Host Sync, BPM, play, sequencer enable, pattern/pending,
   playhead, and reanchor readbacks.
2. Save below `out/p20a/manual/`, reopen, and verify parameters 0–245,
   patch/routes/patterns/transport/kit identity/bypass.
3. Close/reopen/re-instantiate: no Engine reset, sequencer restart, unwanted edit,
   stale readback, or callback after destruction.
4. Give a second instance different kits, target, amount, view, owner, patterns,
   transport, and status; confirm no cross-instance state/file/UI effects.
5. Confirm PDC 26 at 48 kHz and record any dropout/callback/reset indicator during
   continuous playback and all representative UI/file actions.

## Physical DPI

Do not change active scaling. Test only already available or separately authorized
isolated displays/VMs. For each physically observed 100/125/150/200% configuration
record monitor identity, resolution/scale/primary status, Windows build, logical
and physical editor size, text, hit/focus alignment, popup/dropdown/dialog and
embedded-window placement, every screen edge, and cross-monitor coordinates.
Mark unavailable factors and multi-monitor checks `NOT TESTED`; do not extrapolate
from coordinate/content-scale tests.

## Accessibility

- [ ] Record REAPER/plugin HWND hierarchy and enumerate semantic descendants,
      roles, names, value patterns, and focus events.
- [ ] Inspect representative knob, slider, enum, toggle, step, and operation button.
- [ ] Inspect native exact-entry fields and file dialogs separately.
- [ ] Complete keyboard-only navigation and record focus behavior.
- [ ] If a supported screen reader is already available, record spoken output
      only if actually heard.

Expected current limitation: zero semantic custom-control UIA descendants, hence
`FRAMEWORK_LIMITED`/`KEYBOARD_ONLY`. Record `NATIVE_PASS` only after semantic
discovery and supported interaction actually work.

## Screenshots and result template

Use lossless PNG with PID, REAPER version, hash, scale, resolution, and UTC notes:
Overview/identity; six Sound owners; Modulation identity 203/rejection; Sequencer
selection/playhead; target loaded/cleared; four dialogs; keyboard focus/exact entry;
restored project; two instances; each physical DPI; PDC; UIA/screen-reader evidence.

```text
Tester / UTC / Windows build:
Monitor(s), resolution(s), scaling:
REAPER path/version/PID and private profile:
Candidate DLL path and SHA-256 before/after:
Module-identity observation method:

Overview: PASS | FAIL | NOT TESTED —
Sound: PASS | FAIL | NOT TESTED —
Modulation: PASS | FAIL | NOT TESTED —
Sequencer: PASS | FAIL | NOT TESTED —
Kits + Morph: PASS | FAIL | NOT TESTED —
Mouse / keyboard-focus: PASS | FAIL | NOT TESTED —
Automation / save-reopen / two instances: PASS | FAIL | NOT TESTED —
Continuous audio / PDC 26: PASS | FAIL | NOT TESTED —
Dialogs: PASS | FAIL | NOT TESTED —
DPI 100 / 125 / 150 / 200 / multi-monitor: record each separately —
Native accessibility / screen reader: PASS | FAIL | NOT TESTED —
Visual clipping/corruption: PASS | FAIL | NOT TESTED —

Direct REAPER GUI: PASS | FAIL | BLOCKED
Physical DPI: PASS | PARTIAL | NOT TESTED | FAIL
Accessibility: NATIVE_PASS | KEYBOARD_ONLY | PARTIAL | BLOCKED | FAIL
Screenshot/evidence paths:
Defects and exact reproduction:
```
