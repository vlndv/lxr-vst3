# P16-F8 certification closure — BLOCKED

P16-F8 does not certify or publish the combined P16-F implementation. No new
confirmed implementation defect was found. Production Import remains
`BLOCKED — PRODUCTION_IMPORT_GUI_UNAVAILABLE`; complete exact host transition
and granular performance traces and historical raw PCM reconciliation remain
open. No staging, commit, push or successor milestone is authorized from this
checkpoint.

The original 451-file source inventory and 84 changed paths matched F7 exactly,
fingerprint `956DD73154D28B86FF80974103A6A2E4BE422A57457D7130E1CC2694674C2AC1`.
Branch is `p12b-4c-fir-resampler`, local/live starting HEAD
`7749ef1a0203f5ab92bf0e2f447b53bb95dadf68`, origin
`https://github.com/vlndv/lxr-vst3.git`, index empty. Firmware and all dependency
archives were verified against their pins. F8 changes documentation and ignored
verification artifacts only. Production sources, assets, interfaces, historical
fixtures and the private DLL were not changed or rebuilt.

The frozen module remains
`4591AB2EBFDD6FE738F4983B9FBBF292C4B4E06DA7E0F7528C1E9426E050BE1D`,
1,253,376 bytes, at
`out/p16f6/private-build-001/bundle/LXR.vst3/Contents/x86_64-win/LXR.vst3`.
All build-input and seven packaged-resource hashes match. Compiler/linker/build
provenance remains the F6 Release x64/v145 build with deployment disabled.

Protected preservation checks were executed again through the unchanged normal
test executables against that module:

- Original MIDI/CC: 1,660/0, including all eight former CC failures.
- Expanded CC/NRPN: 548,168/0.
- Frozen MIDI PCM: all 76 files bit-identical to original F4 references and F5
  frozen copies; no reference regeneration or overwrite.
- Poisoned-storage initialization: 16/0 at O0 and O2.
- Original-C filter differential: 80,000 blocks with zero bitwise buffer/state
  mismatches at each optimization. The `zi = 0.f` initializer remains unchanged;
  original filter equations and repeated-init/reset semantics are preserved.

The RuntimeSilence O2 gate is closed by ordinary execution **of the exact formerly
denied F7 GCC executable in place**, SHA-256
`CEC55265B1D23E0C8455ED3663C35AF2D39CCBFD7BAE59A1A932FCB97E4D7ECE`:
419 PASS, zero FAIL, zero render allocations. Prior Code Integrity 3033/3077
denials and policy GUID `{0283ac0f-fff1-49ae-ada1-8a933130cad6}` remain recorded.
The reason Windows now permits this file is not established. No policy, path,
certificate, PE header, registry or loader modification was made. A separate
fresh LLVM harness link attempt failed before entry because of its compiler
driver invocation; that failure remains separate and is not the gate result.

A new task-owned REAPER 7.80 project, PID20364, loaded the exact frozen module.
Process, command line, project title, module path and SHA-256 were verified.
Computer Use selected returned window1903050, `F8_IMPORT_4591AB`, but capture
showed the terminal/Codex application and returned no accessibility context.
Zero clicks or keystrokes were sent; graphical interaction stopped. The actual
production control could not be verified. A direct operator request identified
the exact project and valid musical.lxqb fixture. No operator import result is
credited without the observed bank and PCM evidence. GUI rejection, import
save/reopen and import PCM acceptance remain NOT RUN. Parser results are not
substitutes.

The F7 generated tempo script contained a corrupted `RENDER_SETTINGS` identifier,
preventing its enhanced timeline from executing. Only the copied F8 verification
Lua was corrected. F7 failures remain intact. REAPER's F8 timeline completed;
the unchanged read-only observer captured 75 event batches at 60/90/120/140 BPM.
All 75 match independently calculated native boundaries using the exact rate
5632353/128 Hz, 32-frame blocks, integer host demand points and Q32.32 PPQ.
Eight completed 96,000-frame stereo 48 kHz WAVs form four bit-identical
save/reopen pairs, closing the missing 90/140 repeat-render subset. These finite
snapshots do not prove every first event across seeks, loops, resume, live tempo
and map transitions. Full exact-host tempo acceptance remains BLOCKED.

The production MIDI NRPN200..206 mute and parameter243/244 roll scenario now
completes in REAPER. Read-only matching-module/vtable diagnostics record 1,641
muted ordinary track visits, 64 roll events (the source-defined one-second held
interval at 120 BPM/rate13), and all logical/audio/applied masks released.
Nonempty-bank PCM and the saved MIDI/automation project are preserved. Separate
fixed/random pattern renders have source-derived aggregate event/chain/lock
oracles. Aggregate counts are not complete per-event transition traces or
coverage of every unequal-length/rotation case. All fourteen production selector
counts and releases remain covered by the unchanged F7 evidence; F8 does not
claim fourteen new selector tests. SOM remains deferred.

Raw comparisons are individually retained in `out/p16f8/raw_pcm_reconciliation.csv`.
Current selector0 differs first at stereo frame11,785; ordinary host roll
automation has a callback-boundary contract, unlike timestamped MIDI. This
explains an incompatible timing assumption but does not turn unequal bytes into
a PASS. Historical F4 selector0 and selector12 failures remain traceable.
The historical OFF-route fourteen-instance pair differs in only 32 scalar
samples, confined to frames0–24, maximum 13 integer PCM units; all fourteen
persistent streams agree. Its original native/FIR transient startup state was
not recorded, and its cause remains Unresolved. No filter/FIR cause is inferred
and no baseline is changed. Current selector12 equality and final rich-bank
stress success do not erase those older inequalities.

The completed F7 fourteen-instance test was independently rechecked, not rerun:
fourteen distinct full 51,065-byte state streams restore exactly; both 192,000-frame
48 kHz stereo renders have PCM SHA-256
`56EDC4DDDFD410CCF6FEAA3299FDCD4C64598B2F4E25ADA12A31826D5D863923`.
Exact-binary LXRS v3/v1/v2/legacy compatibility, atomic rejection, allocation
probe 8,252/0 with zero measured processing allocations, observed 19,616-byte
processing stack, Steinberg validator47/47 and REAPER PDC26 at48 kHz remain
valid in their measured unchanged scopes. F7 operator approval of the fifteen
listening cases is retained with reverified playlist hashes; no new auditory
observation is invented.

The protected installed DLL remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`
and seven resources are unchanged. Earlier F1–F7 evidence is hashed and preserved.
The complete F7 matrix, raw policy denials and failed verification attempts remain
traceable in the F8 initial/final matrices. Source inventory, final integrity,
per-gate next actions and the requested final report are under `out/p16f8/`.
P16-F1/F2 and combined F3–F7 certification remain BLOCKED; historical F4/F5 FAIL
reports remain historical, and F6 repair preservation passes. Publication is
BLOCKED, no commits exist, and the next development milestone is not ready.

## F8 continuation — 2026-11-16

The repeated task resumes the preserved F8 checkpoint. Its 452 source files,
85 changed paths, empty index and local/live HEAD match the preceding F8 report.
Only the already recorded documentation differs from F7. All 10,117 preceding
F8 artifact hashes were reverified, and the previous root evidence and source
documents were archived under `out/p16f8/resume-001/prior_f8_checkpoint/` before
updating the report. New evidence is under `out/p16f8/resume-001/`.

Fresh preservation again passes: 76/76 frozen PCM files, original MIDI/CC
1,660/0, expanded CC 548,168/0, initialization 16/0 at O0/O2 and original-C
filter 80,000 exact blocks at each optimization. The production DLL, source
inputs, LXRS wire format, installed DLL and seven resources remain unchanged.

A fresh isolated Import project loads the exact candidate in REAPER 7.80,
PID16132. The returned window274254 was selected and activated, but capture
again showed the terminal overlay and no accessibility. No Import click or
typing occurred; graphical interaction stopped. The production Import gate
remains BLOCKED—PRODUCTION_IMPORT_GUI_UNAVAILABLE. A precise direct-operator
request did not yield a verified import result; the watcher expired with the
bank unchanged. Rejection and imported-bank reopen/PCM acceptance remain NOT RUN.

The new read-only host observer captures 3,879 callback contexts at an actual
32-frame callback size. There are 145 independently checked event-time batches
and 161 matching RNG/position/latch/bar-state observations at 60/90/120/140 BPM,
nonzero starts, seeks, actual loop contexts and tempo-map playback. The checked
native-frame errors range from -0.111 to 31.070; the small negative value reflects
integer host demand points, and exact expected native boundaries also agree.
This is the documented next-32-frame scheduling policy, not finer audio timing.
One actual first loop-boundary batch is captured. Some other first-transition
witnesses and live/map PCM capture remain incomplete; the complete exact-host
tempo gate stays BLOCKED. Earlier constant-tempo paired PCM passes remain valid.

Granular performance now has actual production-loaded host witnesses. A separate
canonical fixture supplies unequal track lengths, rotations, shuffle48, active
and inactive gates/substeps, probability0/63/127, fixed/random chaining and two
stored lock slots. Independent original-C source-range and deterministic-PRNG
models match 1,227 state observations, 63 complete event-batch observations and
58 timing observations. Actual observed events cover every track and every
substep, including probability-zero acceptance. A fixed transition from pattern6
to7 at ordinal352/native boundary242016 is observed exactly, and shorter-track
wraps leave the track0 bar/chain reference unchanged. Existing measured roll,
mute, lock, routing, shared-hat, MIDI and two-instance evidence is retained only
in its original scope. The expanded diagnostic's additional automation-operation
snapshots were unavailable and receive no credit.

Matched complete project restores produce two identical nonzero 192,000-frame,
48 kHz stereo performance renders, PCM SHA-256
`B57B74E1586D851A3B6EF2C0E3A2BAF9F797E5B9AD1DBD4AAA04602BF2E407C4`.
No production code or audio timing changed. F7's fourteen-instance stress,
LXRS compatibility, allocation/stack, validator, PDC and listening acceptance
therefore retain their correctly scoped exact-binary applicability.

Raw PCM reconciliation remains incomplete. A new ordinary-host-parameter replay
was prepared to test the selector timing explanation against the real WAVs.
Early compilation and sample-format setup failures are retained as invalid
verification attempts. The corrected sample32 setup compiles under MSVC and
the established VS LLVM toolchain, but both resulting executables are denied
before entry with WinError4551. Code Integrity events and hashes are retained.
No corrected replay assertions executed, no denied image was copied or modified,
and no security setting changed. The historical OFF-route startup and selector12
causes remain unresolved. Every raw inequality keeps its FAIL result; the prior
exact RuntimeSilenceO2 419/0 PASS remains a separate verified gate.

Overall and publication remain BLOCKED. No staging, commit, push, force
termination, production rebuild, new feature or next milestone occurred.


A second supported native app-control surface listed the exact isolated import
window274254, but app approval timed out before usable window state returned.
No graphical input was sent through that surface. The operator replied that they
would perform the exact Import. Its expired watcher requires restarting inside
the same project; completion has not yet been reported. Independent read-only
pre-operator bank fingerprint is
`084C0955DC7814CC5787AD13A2F88932B962DC8CD8222CFDD76455658AEAF008`,
which differs from the valid musical fixture. Import remains NOT RUN pending
actual operator completion. A follow-up render script and a separate corrupt-CRC
fixture are prepared only; they have not run and carry no acceptance credit.

A further unchanged-executable read-only transport capture contains4,760 actual
32-frame contexts,163 passing timing checks and188 passing state checks. The
requested first-callback transition flags were not captured despite96 ordinary
transport commands, so this finite capture does not close those mandatory rows.
Results and commands are preserved under resume-001/reaper-transition-004.
