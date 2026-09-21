## QWEN_SYSTEM.md Code Output Format

**Every phase must follow this format:**

1. **Each file gets its own separate code block** with a clear path comment on the first line
2. **No explanations before the code** — just the code blocks
3. **Maximum 5 lines of notes after the code** listing any UNSURE or QUIRK comments

### Example Format:

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

## Step-by-Step Workflow

### Step 1: Request a Phase
Specify which phase to work on (e.g., "Let's do P6 Transient Generator").
- Specify if you want direct code generation or a standalone prompt for local Qwen 2.5.

### Step 2: Code Generation (AI Output)
The AI generates the code with each file in its own separate Markdown code block, clearly labeled with the target path.

### Step 3: Local Testing (User Action)
1. Copy each code block into the corresponding file in the `lxr-vst3` repository.
2. Run the test harness (`run_tests.bat` or equivalent).
3. Verify all tests pass (`fail=0`).

### Step 4: Update Interfaces (User Action)
If tests pass, copy the generated header file(s) into the `interfaces/` directory.
- Example: Copy `dsp/ModuleName.h` to `interfaces/ModuleName.h`.
- This serves as the "accepted contract" for subsequent phases.

### Step 5: Update Documentation (User Action)
Update **`docs/phase_log.md`** with two changes:

**1. Update the Status Table:**
Change the phase status from `TODO` to `DONE`.

**2. Append the Phase Log Entry:**
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

## Documentation Files to Update

| File | When to Update |
|------|----------------|
| `docs/phase_log.md` | **Always** when a phase is completed |
| `docs/architecture.md` | Only if a new, previously undocumented quirk is discovered |
| `docs/risks.md` | Only if a major architectural decision is made |

## Code Generation Rules

1. **Faithful Port**: Keep constants, operation order, and types from the original C code.
2. **Quirk Marking**: Mark all original quirks with `// ORIGINAL QUIRK: <description>`.
3. **Uncertainty Marking**: Mark unclear points with `// UNSURE: <question>`.
4. **No Silent Fixes**: Never fix or guess silently.
5. **Real-Time Safe**: No heap allocation, locks, file I/O, or logging in audio thread code.
6. **C++17**: No exceptions, no RTTI.

## Testing Requirements

1. **Reference Values**: All expected test values must come from the original C code, not from the AI.
2. **Test Output**: Tests must print PASS/FAIL per line and a SUMMARY line.
3. **Zero Failures**: A phase is only DONE when `fail=0`.
4. **Bit-Identical Verification**: Where possible, verify bit-identical output against the original C (as done in P3 filter).

## Phase Dependencies

Before starting a phase, verify all dependencies are met:
- Check `phase_log.md` for prerequisite phases marked as DONE.
- Ensure all required interfaces exist in `interfaces/`.
- Confirm any open decisions (D1-D5) that block the phase are resolved.

## Out of Scope

Each phase prompt must explicitly list neighboring modules and features NOT to implement, preventing scope creep.

## File Organization

```
lxr-vst3/
  docs/           Reference documents and planning
  dsp/            Implementation files (.h, .cpp)
  interfaces/     Accepted headers (one copy per finished module)
  tests/          Reference harnesses and expected values
  data/           Generated binary/CSV data assets
  tools/          Build and test scripts
```

## Communication Protocol

- **One phase at a time** — do not combine multiple phases.
- **Standalone prompts** — each prompt contains everything needed, assuming no memory of earlier work.
- **Small scope** — one module or task per prompt (under ~6k tokens).
- **Paste code, not descriptions** — include the original C verbatim for DSP modules.

---

**Last updated**: 2026-09-22  
**Status**: Active workflow 


Done! Created `docs/workflow_rules.md` with all the rules and format requirements for working through the phases. This document captures:

- The exact code output format (separate blocks per file)
- Step-by-step workflow
- Documentation update requirements
- Testing and verification standards
- Code generation rules
- Phase dependencies
