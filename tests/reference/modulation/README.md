# Original modulation metadata oracle

Pinned source: SonicPotions/LXR `dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`.
The original custom non-commercial/full-source licence applies to derived data;
see `LICENSE-LXR.txt`. Never substitute Catalyst.

`original_avr_targets.c` directly includes the unchanged original
`front/LxrAvr/Menu/Cc2Text.c` and its actual headers. It exposes the table's
parameter IDs, label IDs, range bounds, gap bitmap and original mapping functions.

`original_stm32_targets.c` directly includes unchanged
`mainboard/LxrStm32/src/MIDI/ParameterArray.c`, the actual parameter enum and
ModulationNode definition. Mock voice storage makes pointer equality and type
metadata observable without compiling/rendering DSP. It does not establish DSP
arithmetic, exact original struct layout or hardware behavior. In particular the
original destination is uint16, while its table metadata declares UINT8.

Both files compile as C, not C++. Host-only `compat/` headers replace AVR flash
reads and the STM32 integer-type include. No firmware source is copied/edited,
and the oracle does not include the generated contract metadata.

Build at O0 and O2 with GNU11/`-fgnu89-inline -ffp-contract=off`, include roots:

- `tests/reference/modulation/compat`
- `ref/LXR-0.37/front/LxrAvr/Menu`
- `ref/LXR-0.37/mainboard/LxrStm32/src/MIDI`
- `ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio`

Link both C objects with `tests/P15M2AModulationContractTest.cpp` using the C++17
flags documented in `docs/build_and_test.md`. Keep all objects/executables/logs
in a new ignored output directory. This test does not build/install a VST3.
