# prompt_template.md — standalone phase prompt for local Qwen 2.5

Use one prompt per module. Fill every `<...>`. Delete sections that do not apply. Paste the result into Qwen as a single message.
Target size: under ~6k tokens per prompt. If it is larger, split the task.

## Author checklist (before pasting)
- [ ] One module, one task, one to three output files.
- [ ] Original C pasted verbatim for anything DSP (do not paraphrase it).
- [ ] Only the `param_map.md` rows this module needs (not the whole table).
- [ ] Any header this module depends on is pasted in full (from `interfaces/`).
- [ ] Acceptance tests use numbers computed from the original C, not from Qwen.
- [ ] "Out of scope" lists neighbouring modules so Qwen does not invent them.

## Template (copy from here)

```
ROLE
You are a C++17 audio DSP engineer porting one module of the Sonic Potions LXR drum synth (firmware 0.37) to a VST3 plugin.
You have no memory of earlier work. Everything you need is in this message.

GOAL
<one sentence: what this module does and where it sits in the signal flow>

RULES (apply to all code)
- C++17. No exceptions, no RTTI.
- No heap allocation, locks, file I/O or logging in any function called from the audio thread. Preallocate in prepare().
- Port faithfully from the ORIGINAL C below. Keep constants, operation order and integer/float types. Do not replace with textbook algorithms.
- If the original has a bug or quirk, keep it and mark it with a comment `// ORIGINAL QUIRK: <what>`. Do not fix silently.
- If something is unclear, write `// UNSURE: <question>` and continue. Do not guess silently.
- Guard against NaN/inf and denormals only where stated under CONSTRAINTS.
- Output only the files requested, each in its own code block with its path as the first line comment. No explanations before the code. After the code, add at most 5 lines listing every UNSURE and QUIRK comment.

INPUTS
1. Original C (verbatim):
<paste full function(s) and struct(s); include relevant #defines>

2. Parameter rows (from param_map.md):
<paste only the needed rows>

3. Dependencies (existing headers, already written; do not rewrite):
<paste header contents or "none">

OUTPUT FILES
- <path/Module.h>: <what it declares>
- <path/Module.cpp>: <what it implements>
- <path/ModuleTest.cpp>: <test main, see ACCEPTANCE>

INTERFACE (must match exactly)
<paste the header signature block: class/struct, method names, argument types, units, thread-safety notes>

BEHAVIOUR
- <numbered list of what each method does; reference original function names>
- Sample rate: engine constant REAL_FS = 44002.7573529412 Hz, block = 32 samples, unless told otherwise.
- <ranges, units, defaults>

CONSTRAINTS
- <e.g. float vs int16 semantics, saturation points, clamps, table sizes>

ACCEPTANCE (test program must print PASS/FAIL per line)
1. <input> -> <expected value> (tolerance <x>)
2. <input> -> <expected value> (tolerance <x>)
3. <edge case: v=0, v=127, NaN guard, etc.>
Expected values come from the original C, not from you. Do not adjust expected values to make tests pass.

OUT OF SCOPE
<list neighbouring modules and features not to implement>
```

## Worked example (filled, short)

```
ROLE
You are a C++17 audio DSP engineer porting one module of the Sonic Potions LXR drum synth (firmware 0.37) to a VST3 plugin.
You have no memory of earlier work. Everything you need is in this message.

GOAL
Implement the mapping from a 0..127 decay parameter to the per-tick decay step used by the amp envelope.

RULES
<as above>

INPUTS
1. Original formula: x = v/127; K = 2 * 0.999 / (1 - 0.999); step = 1 - ((1 + K) * x) / (1 + K * |x|).
2. Parameter rows: VELOD1..VELOD6 -> tag egD (v = 127 gives step 0, meaning it never decays).

OUTPUT FILES
- dsp/EgTime.h: `float egDecayStep(uint8_t v);`
- dsp/EgTime.cpp
- dsp/EgTimeTest.cpp

ACCEPTANCE
1. v=0   -> 1.0
2. v=1   -> 0.059294 (tol 1e-5)
3. v=10  -> 0.005819 (tol 1e-5)
4. v=64  -> 0.000492 (tol 1e-5)
5. v=127 -> 0.0 exactly
6. Full ramp time at v=64 = 1 / (step * (44002.7573529412 / 32)) = ~1.48 s (tol 0.01)

OUT OF SCOPE
Attack mapping, slope warp, the envelope state machine.
```

## Sizing guidance for Qwen 2.5
- One function family per prompt (e.g. "EG time mappings", not "all envelopes").
- Paste code, not descriptions. Quote line-level facts from `architecture.md` only as hints, not as the source of truth.
- Ask for tests in the same prompt; run them yourself. If a test fails, send back only: the failing line, expected vs actual, and the relevant function.
- After each phase, save the accepted headers to `interfaces/` and add one line to `phase_log.md`.
