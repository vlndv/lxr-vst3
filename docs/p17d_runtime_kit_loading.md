# P17-D transactional runtime whole-kit loading

Engineering classification: **PASS**. Development publication is conditional on
the reviewed commit and normal fast-forward push described at the end of this
phase. Release certification remains **BLOCKED** by the independent P16-F gates.

Baseline: branch `p12b-4c-fir-resampler`, local/tracking HEAD
`8a99b26edd356680912dca66de00f99af6b52584`, origin
`https://github.com/vlndv/lxr-vst3.git`, clean tree and empty index. P17-A/B/C,
firmware `dee49689f3f0cdf8f9b7b5e79628cfdca1b79394`, iPlug2
`d54f69050f517e43b941d88c2a170f0a840b9ee4`, SDK root
`3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`, all seven SDK module pins and the
protected DLL/resources were verified before production edits. Catalyst was not
used. The original custom non-commercial/full-source licence remains applicable.

## Ownership and candidate boundary

The input is the already decoded P17-B `CanonicalKit`. `BuildPatchCandidate`
reuses the P17-B runtime gate and produces one fixed 428-byte candidate: eight
name bytes, all 228 source-defined patch bytes and twelve typed route records.
External sample waves, invalid selectors, global/cross-voice destinations,
unsupported native targets, high reserved patch bytes and malformed route
metadata reject before live mutation. Selector identities remain 0..204 in
16-bit typed route records; values above 127 are not truncated or replaced by
OFF. P17-C file I/O and decoding remain non-audio services. P17-D adds no audio
thread file operation and no user-facing file dialog.

The kit transaction owns only its name provenance, 228 patch bytes and twelve
routes. It does not accept or replace the pattern bank, chain data, transport
configuration, host position/tempo, bypass, optional LXRS TLVs, rolls or other
project metadata. A complete LXRS restore remains the only operation that
replaces the mapped project domains together and deliberately clears transient
locks/runtime.

## Publication transaction

`RoutingStateAuthority` remains the existing outer patch/route coordinator.
`RuntimeKitLoad.h` does not introduce a parallel authority.

1. The non-audio producer validates/builds the complete candidate with no live
   mutation.
2. `LXR::LoadRuntimeKit` reserves one monotonic producer order, drains only
   older deferred host mailboxes and captures the expected joint generation.
3. One attempt acquires the per-instance routing gate. Busy returns immediately;
   there is no internal spin or retry.
4. Under that gate, all routes, reservation ordering, restore cutoff, expected
   generation and both authority generation ceilings are preflighted. One
   patch writer is then acquired and all 228 bytes are written.
5. The patch authority commits, then `commitValidatedKitLocked` copies the name
   and all twelve routes and release-stores the new routing generation. That
   routing generation is the logical cross-authority commit point. Every
   coordinated snapshot holds the same gate while snapshotting patch and
   routing state, so no new-patch/old-route or old-patch/new-route pair is
   observable.
6. Route tickets are advanced to the reservation. A newer project restore,
   route/host producer or competing kit makes an older reservation stale.
   Rejection occurs before either payload changes. Failed candidate validation,
   Busy and Stale leave all authorities and unrelated domains unchanged.

Individual authority generation counters alone are not treated as atomicity.
The outer gate and one release commit point provide that property. Controller
projection happens only if the accepted joint generation is still current;
otherwise `OnIdle` projects the newer authoritative snapshot. No edit gesture
is emitted.

## Native boundary and active voices

The processing owner takes one coordinated snapshot in
`ApplyPendingStateAndParameters`. It copies the currently playing Engine into
the resident staging Engine, updates only changed patch fields and routes, then
selects the staged Engine before the next native producer refill. A previously
rendered 32-frame native block can finish from the adapter cache; no block is
half-rendered with two kits. The recorded application boundary is the adapter's
next native frame and is always divisible by 32 at native rate
`5632353/128 Hz`.

There is no Engine, adapter, sequencer, MIDI parser, FIR, envelope, oscillator,
filter or LFO reset for kit loading. Active voice state, filter histories, LFO
phases/retrigger behavior, MIDI queues, sequencer position and FIR history
survive. This intentionally replaces the original firmware's message-by-message
UART visibility with one safe native-block boundary; the source-confirmed lack
of a sequencer/voice reset is preserved. Exact hardware UART timing and
click-free behavior are not claimed.

## Lock rebasing and modulation order

The Engine now retains each of the fourteen active lock nodes' raw value and
bounded application chronology in addition to its existing destination. After
a kit base replacement, active nodes are replayed in that chronology. Duplicate
destinations therefore retain their documented newest-node precedence. Clearing
a node still restores the newest authoritative raw byte, so no pre-kit base can
reappear. Two successive loads rebase again without clearing lock lifetime.

If host/MIDI automation is published after the kit but before the audio owner
observes it, per-parameter authority generations identify those newer bytes and
replay them after lock rebasing. The existing rule that a newer base edit is
immediately authoritative is preserved. The native tick order remains:

`authoritative kit/host base -> sequencer lock -> velocity -> LFO -> rendering`.

Six physical synthesis owners, the shared closed/open hi-hat owner, twelve
velocity/LFO routes, 0..204 selector identity, own-voice checks, physical-base
aliasing and LFO retrigger semantics are unchanged.

## Verification results

`P17DRuntimeKitLoadTest` passes **1,176/0** in each of GCC O0, GCC O2, VS LLVM
O0-equivalent and VS LLVM O2. It covers complete candidate conversion, coherent
snapshots, field/route diagnostics, Busy/Stale/restore rollback, two producers,
multi-instance isolation, pattern/transport sentinels, active and duplicate
locks, successive loads, post-kit base edits, deterministic rerendering and
zero counted publication allocations. The four accepted runs are under
`out/p17d/runtime-*-accepted3/`. They include an active MIDI-triggered voice
and an advancing sequencer across kit application, with neither runtime reset.

Fresh deterministic native PCM hashes agree across all four builds. Baseline D1
is `850773D76B4E2809`; pitch `6A3EE683D2C7B65C`, volume
`76C93D91CFBB3C5B`, snare `A39FF50747192E80`, cymbal
`674B90F4523F42BE`, open/closed hi-hat comparison
`4F9EB953D2A471F6`, filter `DF55EFC98E4ED74B`, velocity route
`7D1341CC054E30B4`, and LFO route `BEBD9F425ED0D3A4`. First differing frames are
1,0,0,0,32,0,0,0 respectively; the matched repeat is exact and has no differing
frame. These are deterministic software-native hashes, not hardware recordings.
Each scenario record also contains the tested 236-byte logical input SHA-256,
field-wise candidate fingerprint, isolated publication generations, frame-zero
native boundary, trigger event and expected/observed comparison.

Finite 1,000-publication measurements include harness/OS scheduling overhead.
Observed maximum/mean nanoseconds are recorded per compiler in evidence; O2
means were 2,798 ns (GCC) and 1,883 ns (VS LLVM). They are not hard real-time
worst-case guarantees. Publication and rendering scopes counted zero C++ heap
allocations. Exact private-DLL instrumentation passes 8,252/0: 4,000 varied
callbacks and full 4,096-event callbacks allocate zero; observed processing
stack watermark is 19,744 bytes on a 1 MiB thread. PE unwind inspection reports
an 8,136-byte fixed prolog for `ApplyPendingStateAndParameters`; unknown unwind
records and dynamic/callee stack remain documented limits.

Relevant fresh regressions pass: P17-A 306,037/0, P17-B 817,873/0, P17-C
594,156/0 per optimization; P15 contract 121,781/0, velocity 61,395/0,
LFO fidelity 224,377/0 and LFO runtime 1,772,097/0 per optimization; P16-D
246/0 per optimization; the full supported native checkpoint passes all 44
executions including original-C filter 80,000 exact blocks per optimization.
The exact private VST3 state/runtime/controller/M2G/M2H/transport/F2/
certification/mute/probe counts are 322/1,143/955/91/689/105/3,300/260/216/
1,057, all with zero failures. Steinberg validator passes 47/47.

The isolated Release x64/v145 private DLL is
`658272C716FDB1A7C42848B9F073888118F825D4F61C9730FBA3D7F8C2DDFC5F`
(1,255,424 bytes). Both pre/post build deployment events were disabled,
including mandatory `/p:PostBuildEventUseInBuild=false`. It was not installed.
The protected installed DLL remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`,
and all seven resources remain identical to tracked data.

## Scope and next phase

P17-D provides a programmatic non-audio canonical-kit ingress and coherent DSP/
host publication. It does not provide a preset browser, `.snd` Import UI,
factory content, morphing, new graphics, user-sample loading or complete
performance import. P17-E may add the user-facing `.snd` action by calling the
already bounded P17-C read/decoder and this transaction; it must not move file
I/O onto audio or broaden kit ownership.

P16-F release certification remains blocked/open exactly as recorded in
`p16f13_development_checkpoint.md`; P17-D development PASS does not satisfy the
outstanding Import/timing/performance/raw-PCM/exact-artifact/observer release
matrix and does not authorize a release.
