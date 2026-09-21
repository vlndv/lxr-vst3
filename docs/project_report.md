# LXR VST3 port — project report (through P3)

Status date: 2026-09-20. Baseline: SonicPotions/LXR @ `dee4968` ("firmware image 0.37"). Repo: `https://github.com/vlndv/lxr-vst3.git`.
Everything below is from the source and tests we ran. Items I could not check are listed in section 9.

## 1. Summary

The goal is a VST3 emulation of the original Sonic Potions LXR drum synth (firmware 0.37), with the LXR-01 panel UI, MIDI/parameter compatibility, and easier replacements for hardware quirks.

| phase | what | status |
|---|---|---|
| P0 | Source review and project documents | DONE |
| P1 | Export data tables (wavetables, samples, note table) | DONE, run on your machine, nine tables OK |
| P2 | Parameter mappings (0..127 value to engine value) | DONE, tests pass on your machine |
| P3 | Nonlinear filter | DONE, tests pass on your machine |
| P4 to P13 | Oscillators, envelopes, transient, distortion/decimator, voices, LFO/modulation, mixer, plugin shell, verification harness | TODO |
| P14 to P17 | Sequencer, kits/presets, front-panel review, UI | TODO / BLOCKED until P14 |

Test totals: filter 130 checks (plus 80,000 random blocks bit-identical to the original C in my sandbox), envelope mappings 78 checks, other mappings 121 checks. All three test programs print `fail=0` on your laptop.

## 2. Tasks we did, in order

1. **Project instructions.** I wrote the text for the Claude Project instructions (goal, workflow, scope, rules).
2. **Context plan.** We decided which documents stay in project knowledge (parameter map, architecture, interface headers, phase log, prompt template, UI layout, risks) and what to leave out (the full repo, STM32 hardware code, Catalyst-only features).
3. **Source review.** I cloned `SonicPotions/LXR` and read the DSP, MIDI parser, parameter array, mixer, voices, filter, envelopes, LFO, modulation, distortion, transient generator and the AVR menu tables. I also cloned `brendanclarke/LXR` and `DoItYourSynth/LXRBrendanClarke` and compared them to the original.
4. **Documents.** I produced the reference documents (sections 4.1 to 4.3) and the workflow files (`AGENTS.md`, `CLAUDE.md`, `qwen_system.md`, `prompt_template.md`, `phase_log.md`, `risks.md`, `.gitignore`).
5. **Repo and environment setup.** Folder layout, GitHub repo, MSYS2 g++ on Windows, Ollama model `lxr-port` built from a `Modelfile`.
6. **P1.** A Python script exports the data tables from the C source. You ran it: all nine tables OK.
7. **P3.** I compiled the original filter C, produced reference values, and wrote a prompt. Qwen (14B coder) ported it. I verified the port and replaced Qwen's test file with a generated one.
8. **P2.** I analysed all mapping code, built reference values, wrote two Qwen prompts and the tests, and also wrote a tested implementation myself. Qwen's P2a answer was later verified bit-identical to mine.
9. **Tooling.** Code-block extractor (`extract_code.py`) and a one-command test runner (`run_tests.bat`).

## 3. Findings and corrections from the source review

| # | finding | consequence |
|---|---|---|
| 1 | `brendanclarke/LXR` is Catalyst v1.02. Its DSP differs from 0.37 in 23 of about 45 DSPAudio files (for example `Oscillator.c` differs by about 413 lines). `ResonantFilter.c` and `distortion.c` are identical. | The original `SonicPotions/LXR` HEAD (commit `dee4968`) is the reference. |
| 2 | `DoItYourSynth/LXRBrendanClarke` is an older mirror of the original (last commit merges SonicPotions/master). Its `mainboard` and `front` trees are identical to 0.37. | Not needed as a second reference. You decided to keep only `ref/LXR-0.37`. |
| 3 | The licence is custom non-commercial (no sale or commercial use; modified redistribution must include full source; keep the copyright notice). It is not GPL. | Derived code and exported data are covered. Incompatible with AGPL JUCE unless JUCE's commercial licence is bought, which would itself be commercial use. Decision D1 is open. |
| 4 | No reverb, delay, EQ, limiter or bit-depth reduction in 0.37. "Effects" are per-voice distortion, filter drive and sample-rate decimation. | Do not add these and call them faithful. |
| 5 | Engine block is 32 samples at REAL_FS = 44002.7578125 Hz (a float constant), so control-rate updates run at about 1375 Hz. `config.h` has a dead `OUTPUT_DMA_SIZE 16` branch; I preprocessed every translation unit with the Makefile flags and all resolve to 32. | Envelope times are per-tick, so the engine-rate decision (D2) affects every time-based module. |
| 6 | The time-constant K values are computed in float: 198.000198 (attack, pitch decay) and 1998.02576 (decay), not 198 and 1998. | Fixed in the documents and prompts. A test using 198 fails. |
| 7 | Amp envelope attack at value 127 is a step of 0. Pitch slope at value 127 divides by zero (+infinity). | Kept as original quirks; not clamped. |
| 8 | LFO phase offset at value 127 overflows the float-to-uint32 conversion (undefined in C). | Port decision: saturate to 0xFFFFFFFF (assumed ARM behaviour, unverified). |
| 9 | The sine table is a negative cosine shape, so the drum voice's sine start phase lands on the rising zero crossing, not a peak. | I corrected this claim in `architecture.md` after checking the exported table. |
| 10 | LFO targets, velocity destinations and voice-LFO selection cannot be set over MIDI in the original. | The plugin needs its own extension for these (risk S2). |

## 4. File catalogue

### 4.1 Repo layout (your machine: `C:\Projects\lxr-vst3`)

```
lxr-vst3/
  AGENTS.md  CLAUDE.md  .gitignore  Modelfile
  docs/         architecture.md  param_map.md  risks.md  phase_log.md  prompt_template.md  qwen_system.md
  prompts/      P2a_env.md  P2b_misc.md  P3_filter.md
  dsp/          ResonantFilter.{h,cpp,Test.cpp}  ParamMapEnv.{h,cpp,Test.cpp}  ParamMapMisc.{h,cpp,Test.cpp}
  interfaces/   ResonantFilter.h  ParamMapEnv.h  ParamMapMisc.h
  tools/        export_data.py  extract_code.py  run_tests.bat
  tests/reference/filter/    harness.c  diff_test.cpp  README.md  expected_output.txt  stub/
  tests/reference/mappings/  harness.c  README.md  expected_output.txt  stub/
  data/         (generated) *.bin  *.csv  manifest.json
  ref/LXR-0.37/ (git clone of the original, git-ignored)
  out/          (build output, git-ignored)
  plugin/       (empty, for the framework shell)
```

### 4.2 Root files

| file | purpose | notes |
|---|---|---|
| `AGENTS.md` | Rules for any coding agent working in the repo: read-first list, source-of-truth order (original C, then docs, then own knowledge), C++17/real-time rules, quirk-marking convention, licence constraint, phase workflow. | Build commands stay "TBD" until the framework decision. |
| `CLAUDE.md` | Imports `AGENTS.md` (`@AGENTS.md`) and adds style lines (concise, flag flaws, ground DSP claims in the original C). | For Claude Code use in the repo. |
| `.gitignore` | Keeps `ref/` (non-commercial original source), build output, IDE files and generated audio out of git. | `data/` is tracked, and the licence covers it: keep the repo private or publish full source with the licence. |
| `Modelfile` | Ollama recipe for the `lxr-port` model: base `qwen2.5-coder:14b`, context 12288, prediction limit 6000, temperature 0.1, and the system prompt from `qwen_system.md`. | The larger context matters: the default would cut the prompts off. |

### 4.3 `docs/` (reference documents and planning)

| file | purpose | key contents |
|---|---|---|
| `architecture.md` | How the 0.37 engine works, verified against the source. | Timing model; the 6 voices (3 drum, snare, cymbal, hi-hat) and 7 tracks; oscillators; the filter; envelopes; transient generator; LFO and modulation; mixer, pan, decimator, routing; a table of ten fidelity quirks; unreviewed areas; data assets. |
| `param_map.md` | All 227 parameters with their MIDI address and how a 0..127 value becomes an engine value. | CC = index+1 for indices up to 127; NRPN = index-128 above that; per-parameter engine target; scale-tag legend with the formulas. |
| `risks.md` | Licence, fidelity, scope, real-time and workflow risks, each tagged DECIDED, OPEN or UNVERIFIED. | Top three to resolve first: framework/licence route, engine-rate strategy, fidelity verification. |
| `phase_log.md` | Phase table, decisions log, open decisions D1 to D5, and one log entry per finished phase. | Status: P0 to P3 DONE. |
| `prompt_template.md` | Skeleton for a standalone phase prompt to a local model, with a worked example and sizing advice. | Includes the lesson from P3: supply the test program yourself. |
| `qwen_system.md` | The fixed ROLE/RULES/OUTPUT FORMAT block used as the system prompt. | Short by design: small local models follow a short rule set better. |
| `project_report.md` | This report. | |

### 4.4 `prompts/` (standalone prompts for Qwen)

| file | purpose | outcome |
|---|---|---|
| `P3_filter.md` | Port the nonlinear filter. Contains the original C (preprocessed for the build configuration), the C++ interface, quirks to keep, and reference test values. About 3,000 tokens. | Run in Qwen. The port was correct. |
| `P2a_env.md` | Port the envelope mappings (attack, decay, slope, pitch decay, pitch slope, pitch amount). | Run in Qwen. The code was correct after removing terminal artefacts. Your repo uses my equivalent version. |
| `P2b_misc.md` | Port the other mappings (cutoff shape, decimation, distortion, noise frequency, transient pitch and waveform, LFO frequency and offset, filter type, oscillator pitch). | Not run in Qwen; my version is used. |

### 4.5 `dsp/` and `interfaces/` (the code)

| file | purpose | tests | author |
|---|---|---|---|
| `dsp/ResonantFilter.h/.cpp` | Nonlinear ZDF state-variable filter, 8 types (LP, HP, BP, unity BP, notch, peak, naive 2-pole LP, off/passthrough). int16 buffers in and out, float math inside. Keeps the original quirks (type 8 returns after one state update, R forced to 1 at high cutoff, LP scaled by 0x7fff and others by 0x70ff, q floor of 0.02). | `ResonantFilterTest.cpp`: 130 checks against the original C. | Qwen, verified by me; one edit (`M_PI` replaced by an equal constant for MSVC). |
| `dsp/ParamMapEnv.h/.cpp` | Envelope mappings: attack step, decay step, amp slope and inverse slope, pitch decay step, pitch slope (returns +inf at 127), pitch amount. | `ParamMapEnvTest.cpp`: 78 checks. | Me (Qwen's version verified identical). |
| `dsp/ParamMapMisc.h/.cpp` | Cutoff shaping, decimation rate, distortion shape, snare noise Hz, transient pitch and waveform, LFO Hz and phase offset, filter type, fine detune, oscillator frequency from coarse/fine and base note. | `ParamMapMiscTest.cpp`: 121 checks. | Me. |
| `dsp/*Test.cpp` | Test programs with `main()`, printing PASS/WARN/FAIL per line and a `SUMMARY` line. Expected values come from the original C. | | Generated by script from the reference harnesses. |
| `interfaces/*.h` | Accepted headers, one copy per finished module, to paste into later prompts. | | Copies of the three `.h` files. |

### 4.6 `tools/`

| file | purpose |
|---|---|
| `export_data.py` | P1: parses the C arrays and writes `.bin` (little-endian), `.csv` copies of the small tables, and `manifest.json` with element counts and hashes. Fails if any count is wrong. |
| `extract_code.py` | Writes each code block of a Qwen answer to the path in its first comment line. Skips `*Test.cpp`. Removes terminal word-wrap escape codes copied from a console. |
| `run_tests.bat` | Builds and runs every `dsp\*Test.cpp` against the module of the same name and prints the summaries. |

### 4.7 `tests/reference/` (ground truth from the original C)

| folder | contents | purpose |
|---|---|---|
| `filter/` | `harness.c`, `expected_output.txt`, `diff_test.cpp`, `README.md`, `stub/` | The harness compiles the unchanged original `ResonantFilter.c` and prints the expected values. `diff_test.cpp` compares the port with the original bitwise over 80,000 random blocks. |
| `mappings/` | `harness.c`, `expected_output.txt`, `README.md`, `stub/` | Links the original `SlopeEg2.c`, `Decay.c`, `distortion.c` and includes the other original functions extracted verbatim by script, then prints the expected values. |
| `stub/` | Minimal replacement headers (`config.h`, `globals.h`, `datatypes.h`, `stm32f4xx.h`, plus an empty `distortion.h` in `filter/`). | These were never sent as download cards. They are only needed to rerun the harnesses (needs `ref/` and gcc). |

### 4.8 `data/` (generated by `export_data.py`)

| table | count | type | sha256 prefix |
|---|---|---|---|
| `sine_table` | 4097 | int16 | 84bae1556a74 |
| `saw_table`, `tri_table`, `rec_table` | 11 x 1024 each | int16 | 50972e365198, 591e0656f4a9, 0e0b93dab161 |
| `crash_sample` | 32768 | uint8 | 440ac7dc5a43 |
| `transient_data` | 12 x 2205 | int8 | f120c68fcb1b |
| `transient_volume_table` | 69 | float32 | fa7e25307ccd |
| `midi_note_frequencies` | 128 | float32 | c53587ea03c6 |
| `sqrt_lut` | 128 | float32 | 079c0782a244 |

The last row of `transientData` lists 13 fewer values than its declared size, so the export zero-fills them as C does. Hex int8 literals above 0x7F are stored as negative numbers.

## 5. Verification summary

| item | how | result |
|---|---|---|
| Data export | Element counts, hashes | 9 of 9 OK (your machine) |
| Filter | 130 reference checks; 80,000 random blocks bitwise against the original C (my sandbox, -O0 and -O2) | fail=0; 0 mismatches. Your machine: fail=0 warn=0 |
| Envelope mappings | 78 checks within 4 ULP; a wrong constant (198) is detected | fail=0 (both machines) |
| Other mappings | 121 checks | fail=0 (both machines) |
| Qwen P2a answer | Cleaned of escape codes, tested, bitwise-compared for all 128 inputs | Identical |
| Code checked only with gcc on x86-64 (MSYS2 g++ 16.1.0 on your laptop, gcc on my side) | | MSVC and fused multiply-add builds not checked |

## 6. Qwen workflow assessment

- The 14B model ported the filter and the envelope mappings faithfully (line by line, quirks marked in the filter).
- Its own test files were unusable, so tests are now generated from the reference harness.
- Copying from the console injected wrap codes into the code; saving through `| Out-File` avoids that, and `extract_code.py` repairs affected files.
- On a CPU-only laptop a 3 to 4k-token answer takes many minutes. The interactive `>>>` prompt sends everything typed to the model, which caused three mix-ups.
- Current approach: I write mechanical ports directly and test them against the original C. Qwen stays available where offline or free use matters.

## 7. Environment

| item | state |
|---|---|
| Windows compiler | MSYS2 UCRT64 g++ 16.1.0 (`C:\msys64\ucrt64\bin` on PATH) |
| Local model | `lxr-port` created from `Modelfile` (Qwen2.5 Coder 14B) |
| Git | `https://github.com/vlndv/lxr-vst3.git` (private) |
| Not installed yet | Visual Studio Build Tools (MSVC) for the plugin build; CMake; the VST3/plugin framework |

## 8. Decisions

| id | decision | status |
|---|---|---|
| D1 | Framework and licence route (JUCE vs a permissive framework such as DPF or iPlug2) | OPEN |
| D2 | Engine rate: run at 44002.757 Hz and resample, or rescale the per-tick constants | OPEN |
| D3 | int16 semantics vs float: the filter keeps int16 in and out | Settled for the filter only |
| D4 | Keep or fix each fidelity quirk | OPEN |
| D5 | Fidelity verification method (hardware recordings, reference vectors) | OPEN |
| — | Base on 0.37; keep MIDI/parameter compatibility; per-parameter base plus modulation multiplier; keep per-block amp envelope; LFO phase offset saturates at 127 | Marked "proposed, not yet confirmed" where you have not explicitly agreed |

## 9. Not verified and assumptions

- ARM float-to-uint32 behaviour (saturation) is assumed for the LFO phase offset and is the likely reason FM may be half-wave. Not checked on hardware or by disassembly.
- The wavetable interpolation reading (quirk 1) and the low-frequency table index wrap (quirk 10) come from reading the code. `config.h` has `INTERPOLATE_OSC = 1`, so quirk 1 must be re-verified in P4.
- No reference recordings from a real LXR exist yet, so nothing is compared with real sound.
- CPU cost is unmeasured.
- The sequencer, kits/presets, SysEx, sync, and the front-panel layout are not reviewed.
- Trademark and design status of the LXR name and panel is unchecked. None of this is legal advice.
- The Windows `run_tests.bat` ran correctly on your machine. I could not run it myself.

## 10. Next steps

1. **P4 oscillators** (sine, wavetables, noise, crash sample, FM), starting with a re-check of the interpolation code and a keep-or-fix decision per quirk, then a port tested against the original C using the exported tables.
2. Then P5 (envelope state machines), P6 (transient), P7 (distortion and decimator), P8 and P9 (voices), P10 (LFO and modulation), P11 (mixer), P12 (plugin shell), P13 (comparison harness).
3. P14 (sequencer, presets, front panel review) can run in parallel and unblocks the UI and sequencer phases.
4. Decide D1 and D2 before P10 and P12.
