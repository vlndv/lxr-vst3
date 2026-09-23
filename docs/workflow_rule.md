```markdown
// FILE: docs/workflow_rules.md
# Workflow Rules — LXR VST3 Port

This document defines the format, process, and conventions for building the LXR VST3 port phase-by-phase.
Every phase (P0–P17) must follow these rules.

## 1. Code Output Format

Every phase delivered by the AI must follow this format:

1. **Each file gets its own separate code block**, with a clear path comment on the first line:
   ```
   // FILE: dsp/ModuleName.h
   ```
2. **No explanations before the code** — just the code blocks.
3. **Maximum 5 lines of notes after the code**, listing every `UNSURE` and `QUIRK` comment.

### Example

```cpp
// FILE: dsp/ModuleName.h
#pragma once
// ... header content ...
```

```cpp
// FILE: dsp/ModuleName.cpp
#include "ModuleName.h"
// ... implementation ...
```

```cpp
// FILE: interfaces/ModuleName.h
#pragma once
// ... accepted interface copy ...
```

## 2. Step-by-Step Workflow

### Step 1 — Request a Phase
Specify which phase to work on (e.g., "Let's do P8 Drum Voice").
Specify whether you want:
- Direct code generation (AI writes the code), or
- A standalone prompt for the local Qwen 2.5 model.

### Step 2 — Code Generation (AI Output)
The AI generates the code with **each file in its own separate Markdown code block**, clearly labeled with the target path.

### Step 3 — Local Testing (User Action)
1. Copy each code block into the corresponding file in the `lxr-vst3` repository.
2. Compile and run the test harness:
   ```powershell
   g++ -std=c++17 -O0 -o <TestName>.exe dsp/<Test>.cpp dsp/<Module>.cpp -I dsp
   .\<TestName>.exe
   ```
3. Verify all tests pass (`fail=0`).

### Step 4 — Update Interfaces (User Action)
If tests pass, copy the generated header file(s) into the `interfaces/` directory:
```powershell
Copy-Item dsp\ModuleName.h interfaces\ModuleName.h
```
This serves as the "accepted contract" for subsequent phases.

### Step 5 — Update Documentation (User Action)
Update `docs/phase_log.md` with **two changes**:

**Change 1 — Status Table:**
Change the phase status from `TODO` to `DONE`.

**Change 2 — Append Phase Log Entry:**
Add this exact format to the bottom of the Log section:
```markdown
### P<n> <Phase Name> — DONE — <YYYY-MM-DD>
- Prompt file: <path or "N/A (Generated directly by AI assistant)">
- Output files: <list of generated files>
- Tests passed: <summary of test results>
- Quirks kept: <list of original quirks preserved>
- Interfaces saved: <list of interface headers copied>
- Open issues: <any unresolved issues or "None">
```

### Step 6 — Commit and Push (User Action)
```powershell
git add .
git commit -m "P<n>: <Phase Name> — DONE"
git push origin main
```

## 3. Documentation Files to Update

| File | When to Update |
|------|----------------|
| `docs/phase_log.md` | **Always** when a phase is completed |
| `docs/architecture.md` | Only if a new, previously undocumented quirk is discovered |
| `docs/risks.md` | Only if a major architectural decision is made |

## 4. Code Generation Rules

1. **Faithful Port**: Keep constants, operation order, and types from the original C code.
2. **Quirk Marking**: Mark all original quirks with `// ORIGINAL QUIRK: <description>`.
3. **Uncertainty Marking**: Mark unclear points with `// UNSURE: <question>`.
4. **No Silent Fixes**: Never fix or guess silently.
5. **Real-Time Safe**: No heap allocation, locks, file I/O, or logging in audio thread code.
6. **C++17**: No exceptions, no RTTI. Use `namespace lxr`.

## 5. Testing Requirements

1. **Reference Values**: All expected test values must come from the original C code, not from the AI.
2. **Test Output**: Tests must print `PASS`/`FAIL` per line and a `SUMMARY` line.
3. **Zero Failures**: A phase is only `DONE` when `fail=0`.
4. **Bit-Identical Verification**: Where possible, verify bit-identical output against the original C (as done in P3 filter).

## 6. Phase Dependencies

Before starting a phase, verify all dependencies are met:
- Check `phase_log.md` for prerequisite phases marked as `DONE`.
- Ensure all required interfaces exist in `interfaces/`.
- Confirm any open decisions (D1–D5) that block the phase are resolved.

## 7. Out of Scope

Each phase prompt must explicitly list neighboring modules and features **NOT** to implement, preventing scope creep.

## 8. File Organization

```
lxr-vst3/
  docs/           Reference documents and planning
  dsp/            Implementation files (.h, .cpp)
  interfaces/     Accepted headers (one copy per finished module)
  tests/          Reference harnesses and expected values
  data/           Generated binary/CSV data assets
  tools/          Build and test scripts
  ref/            Original LXR-0.37 source (git-ignored, read-only)
```

## 9. Communication Protocol

- **One phase at a time** — do not combine multiple phases.
- **Standalone prompts** — each prompt contains everything needed, assuming no memory of earlier work.
- **Small scope** — one module or task per prompt (under ~6k tokens).
- **Paste code, not descriptions** — include the original C verbatim for DSP modules.

## 10. Git Hygiene

- `*.exe`, `*.dll`, `*.o`, `*.obj` are ignored (see `.gitignore`).
- `out/` and `build/` directories are ignored.
- `ref/` (original firmware) is git-ignored to avoid licensing issues.
- Do not commit build artifacts.
- Commit messages follow the format: `P<n>: <Phase Name> — DONE`.

---

**Last updated**: 2026-09-23
**Status**: Active workflow for all phases
```

