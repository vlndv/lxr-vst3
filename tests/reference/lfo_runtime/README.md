# P15-M2E original-C runtime adapter

Run from the repository root:

```
python tools/verification/run_lfo_runtime_tests.py --output out/new-lfo-runtime-run
```

The runner snapshots inputs and compiles unchanged original LXR 0.37 C through
the established base-mapping compatibility generator. Complete original mixer
functions are extracted verbatim with source lines/body hashes. This adapter
exposes the modulation-stage boundary (`DSPAudio/mixer.c`,
`mixer_calcNextSampleBlock`, lines287-307) for comparison with
`Engine::beginModulationTick`. It calls the original restore, velocity replay,
LFO dispatch and filter-coefficient functions. It does not implement their math.

The oracle and implementation are different translation units, C and C++.
Original AVR table lookup independently checks the typed route identities. The
existing field observer reads public DSP storage independently of dispatch code.
Every supported row compares rendered native voice PCM and effective fields.
This is strict host arithmetic fidelity for the declared fixtures, not hardware
entropy/FMA equality or complete mixer/host PCM certification.

Deterministic fixtures match original note-frequency assets, filter previous
input initialization and the established M2C safe FM domain. LFO noise and voice
noise receive explicitly controlled, independent service inputs: the original
hardware shares an entropy service, while the plugin has a PRNG per source.
The test resets the oracle RNG at those service boundaries, without editing
original C. Wrap conditions and full-width normalization are compared exactly.

Acceptance is exact float bit comparison, integer equality and exact int16 PCM;
no numeric tolerance is introduced. Negative control replaces only dispatch in
a copied Engine translation unit; the same comparator must fail. Its expected
assertion failures are sensitivity evidence, never production regression FAILs.

Outputs record source hashes, commands, flags, executable hashes, raw output,
per-PCM-case first divergence, maximum error and RMS. An execution denial stays
BLOCKED; do not rename, relocate, rebuild or retry a binary to evade policy.
Only a new authorized verification run may execute that exact artifact again.

Original Copyright2013 Julian Schmidt. LICENSE-LXR.txt applies to derived code.
