# P15-M2D independent LFO reference

Original SonicPotions/LXR 0.37 at
dee49689f3f0cdf8f9b7b5e79628cfdca1b79394, never Catalyst. Original Copyright
2013 Julian Schmidt; all derived material retains LICENSE-LXR.txt's custom
non-commercial/full-source terms.

From the repository root, using the compilers documented in
`docs/build_and_test.md`, choose a new output directory:

```powershell
python tools/verification/run_lfo_fidelity_tests.py --output out/lfo-audit-new-run
```

The runner snapshots sources, includes unchanged original C using the existing
M2B compatibility generator, appends complete verbatim original mixer functions,
compiles C as gnu11 and the observer as C++17 at O0/O2, disables float contraction,
and records commands, compiler versions, hashes and all execution logs. It never
edits reference/production files, builds/installs a plugin or changes security
policy. A denied executable is BLOCKED; do not rename, relocate, sign or rebuild
it as a workaround. A new authorized environment must use its normal loader.

The adapter is setup/observation only. Original lfo_calc, lfo_setFreq,
lfo_calcPhaseInc, lfo_setSync, lfo_recalcSync, lfo_retrigger, ModulationNode,
ParameterArray, voices and the complete mixer execute directly. The test's
expected-value equations are independent assertions, not the reference engine.
GPIO jack availability and trigger IO are stubbed hardware services. RNG calls
use the established deterministic service; sine data are verified against the
original integer initializer before execution.

`comparisons.csv` separates exact original-C assertions from measured production
LFO divergences. A divergence finding does not authorize a production correction.
`timing.csv` labels generated native frames, not host/DAC arrival times. No
integrated C++ LFO PCM equality is claimed. Unsafe route addresses and undefined
high UInt32 conversions are not executed to fabricate deterministic semantics.

See `docs/p15_m2d_lfo_timing_and_ordering.md`, the shared-field state table and
M2E design under `out/p15-m2d/`. Those design files are evidence, not compiled
runtime code. The strict milestone remains BLOCKED until mandatory executions
are available through approved policy and pass.
