# P17-A original storage reference adapter

Test-only hardware boundary adapter. See `docs/p17a_preset_kit_architecture.md` for findings and scope; no production preset codec is added.

The runner includes unchanged pinned AVR `presetManager.c` and `Cc2Text.c` and real enum/protocol/StepData headers, plus exact source-extracted `menu_init`/`numtostrpu` bodies. Original copyright/licence notices remain in those inputs. See `LICENSE-LXR.txt`: derived work is subject to the original custom non-commercial terms. No original source is copied into this directory.

Compatibility headers emulate only AVR integer/header access. Memory FatFs and deterministic UART responses exercise original writers without a card or hardware. Explicit counter adaptation avoids host-width pointer writes. The global/composite original reader's uninitialized count is documented and deliberately not executed. Host structure checks do not certify AVR/ARM object ABI.

Independent Python fixtures use explicit byte fields and LE masks. The C++ strict shape checker is a standalone test examiner, not a production reader and not a complete validator. Fixtures are synthetic original-source-backed data, not factory kits or acquired hardware-card files.

Use `python tools/verification/run_preset_kit_contract_tests.py --help`. Both optimization levels are attempted; image-policy denials stay BLOCKED and must not be bypassed. No VST3 is built or installed. Record each run in a fresh output directory to preserve prior evidence.
