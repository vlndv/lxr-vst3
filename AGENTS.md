# AGENTS.md — LXR VST3 port

Emulation of the Sonic Potions LXR drum synth, firmware 0.37 (SonicPotions/LXR @ dee4968), as a VST3 plugin.
NOT the brendanclarke/Catalyst fork: its DSP differs. Never use it as reference.

## Read first
- `docs/architecture.md`: voice architecture, timing, quirks (verified against 0.37).
- `docs/param_map.md`: all 227 parameters, MIDI CC/NRPN, engine targets, scaling formulas.
- `docs/risks.md`: licence, fidelity and real-time risks. Check before making design choices.
- `docs/phase_log.md`: current phase, decisions, open questions.
- Sections marked UNVERIFIED or OPEN in these files are not settled. Do not present them as facts.

## Source of truth
1. The original C in `ref/LXR-0.37/` (read-only, git-ignored, do not commit or edit).
2. The docs above.
3. Your own knowledge, last. If it disagrees with the original C, the original C is correct.
Do not assume anything about the sequencer, kits/presets, SysEx, sync or front-panel logic: not reviewed yet (see `docs/phase_log.md` P14).

## Rules
- C++17. No exceptions, no RTTI.
- Audio thread: no heap allocation, locks, file I/O or logging. Preallocate in `prepare()`. Parameter changes reach DSP through atomics or a lock-free queue.
- Port faithfully: keep constants, operation order, integer/float types and clip points. Do not substitute textbook algorithms (especially the filter: copy `SVF_calcBlockZDF` logic).
- Keep original quirks and mark them `// ORIGINAL QUIRK: <what>`. Unclear points get `// UNSURE: <question>`. Never fix or guess silently.
- Modulation: per-parameter base value + modulation multiplier evaluated once per tick, not in-place mutation.
- Guard NaN/inf and denormals where `docs/risks.md` says so.
- Do not add features the original lacks (no reverb, delay, EQ, limiter, bit-depth crush) unless marked as an extension.
- User SD samples (wave >= 6) are out of scope.

## Licence constraint
Original source has a custom non-commercial licence (not GPL). Derived code and exported data assets are covered: no sale or commercial use, modified redistribution ships full source, keep copyright notices. Do not add AGPL/GPL dependencies or paid-store metadata. Framework choice is open (`docs/phase_log.md` D1).

## Proposed layout (not created yet)
```
docs/        the .md files above
ref/         LXR-0.37 checkout (git-ignored)
data/        exported wavetables, samples, tables (binary/CSV)
interfaces/  accepted headers per phase
dsp/         engine modules
plugin/      framework shell, parameters, MIDI
tests/       one test per module, expected values from the original C
```

## Workflow
- One phase at a time, in the order in `docs/phase_log.md`.
- A phase is done only when its tests pass against values derived from the original C, its headers are in `interfaces/`, and `phase_log.md` has an entry.
- Build and test commands: TBD (depends on framework decision D1). Do not invent them; ask.
- Keep changes small and scoped to the current phase. List anything out of scope you noticed instead of doing it.
