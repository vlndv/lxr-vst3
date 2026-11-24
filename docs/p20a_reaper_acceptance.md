# P20-A final-candidate REAPER acceptance

## Classification

- Exact candidate integrity: **PASS**.
- Automated engineering: **PASS**.
- Direct REAPER GUI: **BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE**.
- Physical DPI: **NOT TESTED**. Retained 100%, 125%, 150%, and 200%
  coordinate/content-scale tests pass, but no physical editor was observable.
- Accessibility: **KEYBOARD_ONLY**. Exact-candidate UI Automation inspection
  finds the plugin HWND but zero semantic custom-control descendants.
- Manual acceptance: **PENDING**.
- Full release: **NOT CERTIFIED**. P16-F remains separately open.

P20-A did not rebuild, install, or modify the certified P19-F candidate. No GUI
defect was observed because the graphical surface could not be attached.

## Exact candidate and source

The target is
`out/p19f/candidate-certified/bundle/LXR.vst3/Contents/x86_64-win/LXR.vst3`:

- SHA-256 `F8C8CBC816EAB5F51B79909DECD33808A0D24FD85ECD653B4EBD8091C398FA94`.
- 1,437,184 bytes.
- Source fingerprint `84AAA970E4EA4FBD24F69DF04D2F7CDD878E9467CF367B3678BA34715A10178C`.
- Source reconciliation 583/583, with no mismatch or unexpected file.
- Source commit `66a6181219cb0c23eab7826ccab266e0e6b68ac2`.
- Firmware/iPlug2/VST3 SDK pins `dee49689...`, `d54f6905...`, and
  `3cdf9ca5...` respectively.

All seven bundle resources match the certified manifest. The protected installed
DLL remains `89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`,
and its seven resources remain unchanged. Nothing was installed.

## Isolated REAPER environment

REAPER 7.80 x64 was launched with `-newinst` and a private `-cfgfile` below
`out/p20a/reaper-profile-001`. Its only 64-bit VST scan path is the certified
bundle parent. Dummy audio is 48 kHz with a 127-frame block. The project is
`P20A_FINAL_CANDIDATE.rpp`.

PID 7384 was independently identified with title
`P20A_FINAL_CANDIDATE - REAPER v7.80 - EVALUATION LICENSE`. Read-only module
enumeration found the exact candidate path loaded, with the certified hash. No
existing REAPER process was closed, no normal profile was changed, and PID 7384
was not terminated. This proves process/profile/module identity, not visual
interaction.

## Computer-control preflight

The supported desktop/app inventory timed out initially, after the single
controlled wait/retry, and on the final post-reset check. The service never
returned observable target state, so a supported window attachment could not be
made. P20-A stopped graphical automation with zero clicks and zero keystrokes.

The exact module subsequently loaded in the uniquely titled process, but the
bounded allowance was exhausted. Direct five-view rendering, mouse adjustment,
live keyboard focus, editor close/reopen, dialog placement, save/reopen visuals,
and screenshots are therefore **BLOCKED**, not failed or inferred.

## Automated production-path results

The fresh exact-DLL matrix passes all 20 rows and 10,993 assertions, plus
Steinberg validator 47/47. It covers VST3 state/runtime/controller, typed routes,
LXRS compatibility, transport, restoration, MIDI mute, kits, presets, morphing,
all five views, automation/readback, lifecycle, and two-instance isolation.

Production inventory remains 246 IDs (0–245), 223 visible and 23 intentionally
hidden fields, 162 Sound controls/six synthesis owners, 54 Modulation controls/six
owners, and seven sequencer tracks/eight patterns. Editor construction remains
1280 x 800 and PDC remains 26 samples at 48 kHz.

The exact-resource probe rerun passes 8,250/0. It measured 2,170 bounded
initialization allocations totaling 1,362,524 bytes, zero allocations in 4,000
processing callbacks, and a 20,432-byte observed processing stack watermark.
State get/set and event-capacity paths also recorded zero processing allocations.
These finite measurements do not claim an absolute worst-case guarantee.

## Feature dispositions

Sound contracts pass for Drum 1/2/3, Snare, Cymbal, and the shared closed/open
Hi-hat engine. Owner selection remains non-mutating; ranges, exact entry, keyboard
endpoints, automation readback, and PDC pass. Actual mouse/visual checks remain
blocked.

Modulation contracts pass for six owners, velocity/LFO routing, eight original
LFO waves, OFF zero, identity 203, and rejection of 204, reserved 205+, and
cross-owner destinations without seven-bit truncation. Actual menu observation
remains blocked.

Sequencer contracts pass for all tracks/patterns/steps, mute/roll,
length/rotation/shuffle, chaining, Euclidean generation, and copy/clear scopes.
Main-gate OFF preserves records, probability zero remains inclusive, and CH/OH
share one engine. Playhead rendering and graphical edits remain blocked.

Kit/Morph production paths pass exact 236-byte `.snd`, binary identity,
atomic/no-overwrite operations, typed errors/cancellation, target replace/clear,
Source A preservation, transient Target B, Morph Amount 245, and MorphSnd export.
Physical dialog parenting, z-order, placement, and focus return remain not tested.

## DPI and accessibility

Windows reported one 1920 x 1080 display at 60 Hz with `AppliedDPI=120` (nominal
125%). No display setting or monitor connection changed. Because the editor was
not observable, even 125% is not credited as a physical pass; multi-monitor and
edge behavior are not tested. P19-F's exact 100/125/150/200% coordinate/content-
scale results remain engineering evidence only.

Exact-candidate automated UI Automation inspection reaches the plugin HWND and
finds zero semantic custom-control descendants. Custom iPlug2 controls are
`FRAMEWORK_LIMITED` with tested keyboard paths; native exact-entry/file-dialog
controls can expose native semantics when instantiated. No screen reader or live
REAPER accessibility tree was observed. Classification remains **KEYBOARD_ONLY**.

## Manual closure and remaining blockers

The pending exact-candidate procedure is
`docs/p20a_manual_reaper_acceptance.md`. It must record actual visual,
mouse/keyboard, physical DPI, popup/dialog, project-reopen, multi-instance, and
accessibility observations without changing the certified hash.

P20-B should remain limited to unresolved P16-F gates: production `.lxqb` Import
GUI acceptance, granular event timing/observation, Euclidean/copy-clear host-event
observation, historical PCM discrepancies, and exact-artifact closure. P20-A
does not claim them resolved. Evidence is under `out/p20a/`; full release is not
certified.
