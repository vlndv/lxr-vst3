# Original LXR 0.37 velocity routing comparison

```powershell
python tools/verification/run_velocity_routing_tests.py --output out/new-velocity-run
```

Use the GCC/G++ setup in docs/build_and_test.md; --cc/--cxx accept explicit
executables. The runner refuses existing output, snapshots source/assets, and
builds C GNU11 and C++17 at O0/O2 with -ffp-contract=off. DSP disables exceptions
and RTTI. No plugin install, Git operation or Windows-policy change occurs.

The unchanged M2B generator creates the original STM32 DSP/CC-handler oracle.
Its generated C includes original_velocity_adapter.c AFTER the original sources;
the actual unchanged AVR Cc2Text.c is compiled independently through the existing
M2A fixture. velocity_ref_route translates the table index using that compiled
AVR table, then calls actual modNode_setDestination. Triggers execute unchanged
original voice functions. velocity_ref_tick invokes original reset/replay then
coefficient recalculation, with LFO routes off. No synthesis expression is
rewritten or replaced. Existing hardware shims supply deterministic xorshift RNG,
intrinsics and resources, as in the established snare/M2B comparisons.

The C++ observer reads actual public voice/mixer fields separately from Engine's
private dispatcher. Independent C reads actual original parameter pointers/types.
All107 supported rows have effective-field change proofs, not just lookups.
Min/mid/max bases and depths, velocities0/1/64/127, source scopes, full indices,
base changes, integer narrowing, signed slopes, trigger/retrigger, off/change,
native tick consumption, MIDI zero-note behavior, amplitude gating and LXRS bytes
are tested. Original exported note/wave/transient assets and seed match both sides.
Zeroed filter previous-input fixture storage matches original firmware globals.

CY/HH broad matrix cases use noise carriers with controlled RNG, avoiding the
known negative-FM unsigned-cast difference between x64 C and Cortex-M4. Dedicated
actual FM cases use matched positive-crest operator phases and low frequencies
(coarse40/48) over one32-frame interval, as disclosed in M2B. No assertion tolerance
is relaxed. These vectors do not certify arbitrary negative-FM hardware behavior,
ARM fast-math/FMA, unknown ROM guards, SD samples or factory-kit bytes.

Per-block PCM CSV records first differing sample, count, max and RMS error.
The final test source measures operator-new allocations during raw parameter
updates, explicit configuration, triggers and native rendering. The task's
separate negative-control build replaces only applyVelocityValue's body in an
ignored Engine.cpp COPY; the same tests must detect disconnection. It is not a
production compile switch or an altered assertion/oracle.

Original Copyright2013 Julian Schmidt. Original/derived material remains subject
to LICENSE-LXR.txt; retain full-source/non-commercial terms and notices.
