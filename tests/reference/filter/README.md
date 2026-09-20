# Filter reference vectors (P3)

`harness.c` compiles the ORIGINAL `ResonantFilter.c` (unchanged) with small stub headers and prints the expected values used in `prompts/P3_filter.md`.
`expected_output.txt` is the output I got with gcc x86-64.

Run from the repo root (adjust `REF` if your clone is elsewhere):
```
REF=ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio
gcc -O0 -ffp-contract=off -Itests/reference/filter/stub -I$REF -w -o harness tests/reference/filter/harness.c $REF/ResonantFilter.c -lm
./harness > out.txt
diff out.txt tests/reference/filter/expected_output.txt && echo SAME
```
Float digits can differ in the last places with other compilers; the int16 outputs should match.
Licence: the harness only calls the original code; do not commit `ref/`.

## Differential test (port vs original C, bitwise)
`diff_test.cpp` runs 20000 random configurations (80000 blocks, all types incl. invalid) through the original C and `dsp/ResonantFilter.cpp` and compares buffers and all state bitwise. Build from the repo root:
```
REF=ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio
gcc -O0 -ffp-contract=off -Itests/reference/filter/stub -I$REF -w -c $REF/ResonantFilter.c -o out/orig.o
g++ -std=c++17 -O0 -ffp-contract=off -Idsp tests/reference/filter/diff_test.cpp dsp/ResonantFilter.cpp out/orig.o -o out/diff_test
./out/diff_test
```
Expected last line: `differential: 80000 blocks, 0 mismatches ...`
