# P19-F complete GUI qualification

## Classification

- Engineering: **PASS**.
- Accessibility: **KEYBOARD_ONLY**. The deterministic keyboard campaign passes,
  but Microsoft UI Automation reaches only the plugin HWND and exposes zero
  semantic custom-control descendants. Native file dialogs and exact-entry edit
  fields remain native controls; the custom instrument surface is framework-limited.
- DPI: **PARTIALLY_VERIFIED**. Logical/physical mapping and VST3 content-scale
  handling pass at 100%, 125%, 150%, and 200%. Physical multi-monitor observation
  and REAPER popup/dialog placement remain unobserved.
- Direct REAPER GUI: **BLOCKED — COMPUTER_USE_SERVICE_UNAVAILABLE**.
- Full release: not claimed. P16-F release-certification blockers remain separate.

## Scope and source correction

The production audit covers all five views, six synthesis owners, 246 host IDs,
223 intentionally visible parameters, 23 intentionally hidden compatibility or
protocol fields, 162 Sound controls, 54 Modulation controls, seven sequencer
tracks, and Kits/Morph operations.

Qualification found one bounded source-confirmed DPI defect in the pinned iPlug2
Windows backend: `GetScaleForHWND()` returned a fractional `float`, but
`OpenWindow()` stored it in an `int`. That truncated 125% and 150% to 100% before
child-window sizing. The correction changes only that local variable to `float`
in an asserted, isolated P19-F build derivative. The pinned iPlug2 checkout remains
byte-for-byte unchanged; an MSBuild item override substitutes only the generated
translation unit for the private candidate.
The standalone regression checks the source signature and exact coordinate maps;
the exact-DLL host test calls `IPlugViewContentScaleSupport` at all four factors.
No DSP, scheduler, modulation, state format, parameter ID, or persistence contract
changed.

## Automated qualification

`tests/P19FGUIQualificationTest.cpp` is compiled and run with GCC O0/O2 and Visual
Studio LLVM O0/O2. Each build performs 3,950 checks (15,800 total), including
registry/visibility inventory, owner counts, geometry and hit targets, focus and
selection isolation, keyboard endpoints, normalization, invalid-route rejection,
sequencer endpoints, and fractional DPI mapping.

`tests/P19FVST3GUIQualificationTest.cpp` loads the exact private DLL and exercises
real VST3/Win32 production interfaces. It covers fixed editor construction,
five-view activation, six owners, content-scale calls, UI Automation inspection,
keyboard focus traversal, Sound endpoints, typed routes, sequencer gate edits,
Morph Amount 245, save/reopen, two instances, PDC 26, continuous processing during
GUI stress, render timing, and ten editor lifecycle cycles. Exact operation suites
from P17 through P19-E remain in the same host regression matrix and cover kit,
morph, pattern, automation, transport, controller, state, PCM, and clean-destruction
contracts.

## Accessibility findings

The instrument controls are custom-drawn iPlug2 controls rather than native HWND
controls. Direct UI Automation inspection returned a valid root element with zero
custom semantic descendants. Accordingly visible focus and complete keyboard
operation do not establish native screen-reader support. Custom controls are
classified `FRAMEWORK_LIMITED` with a keyboard path; native Windows edit fields
and file dialogs are `NATIVE_ACCESSIBLE`. No screen reader was installed, so
spoken-output acceptance is `NOT_TESTED`.

The deterministic campaign verifies Tab, Shift+Tab contract, arrows, Enter, Space,
Escape, Home, End, and exact-value entry contracts across the five views. Mouse
clicks deliberately do not claim keyboard focus; Tab is the authoritative focus
mechanism. Focus traversal excludes hidden controls after view/owner changes.

## DPI findings

The fixed logical canvas remains 1280 x 800. Framework and exact-DLL tests accept
1.0, 1.25, 1.5, and 2.0 scale factors and verify the host resize callback returns
to 1280 x 800 at 1.0. The source correction preserves fractional scale during
initial Win32 child sizing. Windows display settings were not changed. Physical
monitor screenshots, cross-monitor movement, REAPER popups, native file-dialog
placement, and focus-ring observation at every physical factor remain for manual
acceptance; they are not inferred from coordinate tests.

## Performance and real-time isolation

The exact-DLL test collects 60 forced-redraw samples per view and records median,
p95, p99, maximum, and count. It also runs continuous 64-frame production audio
callbacks during view switching, rendering, keyboard edits, route changes, and
sequencer edits. The recorded run completed thousands of callbacks with zero
processing failures. The exact accepted-DLL allocation/stack probe passes 8,250/0
with zero processing allocations and a 20,432-byte processing stack watermark.

Ten exact editor construction/destruction cycles record process-heap busy blocks
and bytes before, at peak, and after. These figures measure UI lifecycle behavior;
they do not claim zero UI allocations. Static graphics caches may retain bounded
initialization resources.

## Direct REAPER attempt

An isolated REAPER 7.80 x64 profile was created for the exact private bundle and
the uniquely titled `P19F_GUI_QUALIFICATION` project. The computer-control service
timed out while attaching to the initially verified window. During the single
allowed retry, the native handle had changed and the service refused the stale
handle while reporting the uniquely titled replacement. Per the bounded policy,
automation stopped with zero clicks and zero keystrokes. This is an environment
blocker, not a GUI defect. The pending manual procedure is
`docs/p19f_manual_reaper_acceptance.md`.

## Evidence

Machine-readable results and the final exact binary manifest are under
`out/p19f/`. The directory is evidence-only and remains excluded from Git. The
publication commit contains source, tests, runner updates, and these reviewed docs
only.
