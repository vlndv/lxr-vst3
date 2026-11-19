# P18-B firmware-faithful preset morphing

Classification: **development PASS**.

P18-B implements the source-confirmed Sonic Potions LXR v0.37 sound-preset
morph. It is a parameter projection, not an audio crossfade. Source A remains
the accepted mutable whole-kit state. Target B is the 228-parameter payload of
one validated original 236-byte `.snd` file (eight name bytes plus 228 parameter
bytes). Amount is an unsigned byte from 0 through 255.

The implementation deliberately does not add target or amount to LXRS, does not
add a target browser or animation, and does not change the installed VST3.

## Exact projection contract

For every source byte `a`, target byte `b`, and amount `x`, production uses the
same integer operation as pinned firmware `presetManager.c`:

```text
fixed = a*256 + (b-a)*x
q = fixed / 256
result = q + ((fixed & 255) >= 127 ? 1 : 0)
```

The denominator is 256. Amount zero is exact A; amount 255 is not guaranteed to
equal B. Remainders 127 through 255 round upward. Every update starts from A and
B, so repeated updates do not drift.

All 228 bytes undergo the original numeric sweep. The effective VST3 projection
then restores the eighteen source-owned route provenance bytes:

- velocity destination selectors 149--154;
- LFO voice hints 161--166; and
- LFO destination selectors 167--172.

The typed routing authority reconstructs selector identities across the full
original 0--204 domain. No seven-bit patch compatibility byte is treated as the
route identity, so selectors above 127 are not truncated. Disabled LFO routes
retain A's stored voice hint because the typed disabled record has no hint
provenance.

## Ownership and publication

`ParameterStateAuthority` and `RoutingStateAuthority` remain the only
authorities for persistent source A. `lxr::morph::StateAuthority` owns one
fixed-size, per-instance transient target, name, amount, order, and generation.
It contains no dynamic storage or pointers.

Target loading follows this transaction:

1. Decode the original `.snd` outside rendering through the established P17-C
   file codec.
2. Validate all fields and runtime support through the P17-B contract.
3. Reserve an order in the existing host-operation sequence and drain older
   source edits.
4. Attempt one bounded publication of the complete target.
5. Return `InvalidCandidate`, `Busy`, `Stale`, or `GenerationExhausted` without
   changing the accepted target when publication cannot commit.

Amount automation uses the existing fixed atomic mailbox pattern. Target and
amount snapshots use atomic payload fields plus an odd/even generation. Readers
reject overlap; writers make one `atomic_flag` attempt and never wait. Each LXR
instance owns its authority independently.

The non-automatable `IMorphManagement` VST3 interface is a narrow test/editor
ingress for loading target `.snd` files and reading transient status. Host
parameter 245 is the automatable integer `Morph Amount`; the project-state
parameter boundary remains 240, so it is excluded from LXRS serialization.

## Processing order and timing

The processing contract is:

```text
authoritative A -> morph projection -> sequencer locks
  -> velocity -> LFO -> rendering
```

At the start of processing, the owner snapshots A, typed routes, B, and amount,
builds a fixed 228-byte projection, and stages it into copies of the resident
`Engine` and `ParameterArray`. It validates all three authority generations
before one final assignment. A concurrent edit therefore leaves the previous
live engine intact and retries later.

The adapter may still contain PCM from the preceding native producer call. The
projection changes only the engine used for the next refill and never rewrites
cached PCM, so publication is coherent at one native 32-frame boundary. This
atomic boundary is the documented VST3 safety adaptation; original hardware
sent a sequential 684-byte UART sweep.

Staging copies preserve active oscillator, envelope, LFO, filter, MIDI, and
sequencer histories. No reset or retrigger is introduced. Active parameter-lock
nodes are replayed after projection; clearing a lock reveals the latest morph
projection. Target route bytes never replace source A's typed routes. Velocity
and LFO remain the existing downstream per-tick modulation layers.

When no target is present, the path uses the unchanged authoritative patch
bytes. This preserves historical P15/P17 controller and project-state behavior
exactly rather than reconstructing compatibility selector fields unnecessarily.

## Real-time and persistence properties

- no audio-thread allocation, mutex, blocking wait, file I/O, or logging;
- one bounded 228-byte projection and fixed staging copies per accepted change;
- no stale pointers or shared mutable target buffers;
- deterministic controlled-input PCM across compiler and optimization levels;
- target, name, amount, and target-present flag are transient;
- existing LXRS v1/v2/v3 sizes and migrations are unchanged;
- loading or restoring A retains B and amount in the same live instance, as the
  source firmware does; a new instance starts without B and at amount zero.

## Verification

The independent native test compiles the unchanged pinned-firmware
interpolation through the bounded P18-A C adapter and compares production for
all 16,777,216 `(a,b,x)` triples. It also checks all 228 projection positions,
the eighteen route exclusions, selectors above 127, invalid-target rollback,
Busy/stale ordering, multi-instance independence, active-voice continuity,
lock precedence/release, zero projection allocations, and deterministic PCM.

| Toolchain | Optimization | Result | Assertions | PCM hashes |
|---|---:|---:|---:|---|
| GCC | O0 | PASS | 16,781,360 | `7846F5C8D42FF227`, `4A9578ADC7822848`, `DD7D748062DE0326` |
| GCC | O2 | PASS | 16,781,360 | identical |
| VS 18 LLVM | O0 | PASS | 16,781,360 | identical |
| VS 18 LLVM | O2 | PASS | 16,781,360 | identical |

The private Release x64/v145 VST3 has SHA-256
`6262EE39AD1A2EC2F8578FA54B77668CD9478977F151FD3028892D6E619E23AE`.
It was built with pre/post deployment disabled and packaged only below
`out/p18b/`. Exact-DLL P18-B passes 309/0. State, runtime, controller,
modulation, transport, persistence, MIDI, P17 kit/preset, and controller-probe
suites all pass; Steinberg validator passes 47/47. The binary hash is unchanged
before and after host tests.

P16-D parameter locks pass at GCC O0/O2. P17-D transactional whole-kit loading
passes 1,176/0 at GCC O0/O2. The installed DLL remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`,
and all seven installed resources remain unchanged.

Evidence is under `out/p18b/`, principally `runtime-gcc/`,
`runtime-vs-llvm/`, `host-vs-release-r2/`,
`regression-parameter-locks-gcc/`, and `regression-runtime-kit-gcc/`.

## Explicit exclusions and next scope

P18-B has no target persistence, GUI browser, morph animation, target capture,
timed transition, direction control, factory morph content, new sample playback,
or change to the original `.snd` codec. A later milestone may add a user-facing
target chooser and explicit cancel/clear operation using the same ingress, but
must not silently persist the transient session or add a competing patch
authority. P16-F release certification remains separately BLOCKED.
