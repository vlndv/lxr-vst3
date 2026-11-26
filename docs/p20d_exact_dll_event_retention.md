# P20-D exact-DLL native-batch retention and transition copy

Date: 2026-11-26
Starting source HEAD: `0155e153ae34ff2fb5740ea7bfe6855ef5d1c220`
Focused engineering classification: **PASS**
Full release: **NOT_CERTIFIED**

## Baseline and scope

The ten intentional P20-B/P20-C paths were reconciled before editing as `ACCEPTED_INTENTIONAL_P20B_P20C_WORKTREE`. The branch, local/tracking/live remote HEAD, empty index, repaired Import control, private production candidate and protected installation all matched their preserved evidence. No unrelated starting modification was present.

P20-D does not change `TriggerScheduler` event generation, transport scheduling, synthesis, MIDI timing, host parameter IDs or LXRS serialization. The existing production candidate remains SHA-256 `FB1045BD5716282607487F6A70A6082206309EE741C8F751768AE5C8F6F7650D` with historical build-time source fingerprint `80DB83C0C8F79020B06580A54E5627537CE35C5B3E1FBE57DEE33413C0113400`.

## Observation failure and capture boundary

The P20-C limitation was visibility, not event loss. A host callback requests resampled output and can exhaust/refill the adapter's 32-native-frame buffer multiple times. Each refill calls `TriggerScheduler::processNativeBlock()`, which completes one trigger/lock batch, then `Engine::processBlock()`. A later refill clears and replaces the scheduler's `lastBatch()` and `lastAutomationBatch()`. Consequently a consumer reading only after the host callback sees the final completed batch; the engine has already dispatched and rendered every earlier batch.

P20-D adds a compile-time diagnostic hook immediately after `Engine::processBlock()` and before the native-frame/read-index advance. It copies the just-completed scheduler batches before another refill can overwrite them. The hook is guarded by `LXR_P20D_DIAGNOSTIC_CAPTURE`; ordinary production builds contain neither the capture storage nor its interface. The diagnostic DLL is a separate, non-installable test artifact and its authoritative identity is in `out/p20d/diagnostic_binary_manifest.json`.

The controlled host is synchronous: one thread calls `process()`, then reads records only after that call returns. A ring buffer and cross-thread atomics would add complexity without serving the proven topology. The selected per-instance bounded array resets its publication window at callback entry and publishes a record only after every field and copied event is complete.

## Record and capacity contract

Each fixed record contains capture sequence, adapter instance identity, callback identity/size and refill offset, native batch/frame, active pattern, published and event-consumed generations, trigger/lock counts, and fixed copied trigger/lock arrays. No pointer to mutable scheduler storage is retained.

At 48 kHz, 1,024 host frames demand fewer than 939 native frames, therefore at most 30 32-frame refills in the measured adapter phase. Capacity is 64 records per callback, greater than twice that bound. A record is 3,432 bytes and the complete capture object is 219,704 bytes. This memory exists only in the diagnostic candidate.

Normal qualification retained every completed record. An explicit capacity-two run generated 30 records, retained the oldest two in order, reported 28 drops and set overflow. Its PCM matched an uncapped peer, demonstrating bounded observation without an out-of-range write or audio consequence.

## Exact-DLL results

The callback matrix is 1, 16, 31, 32, 33, 64, 127, 128, 256, 512 and 1,024 frames. Each partition ran 384,000 host frames/four complete pattern cycles with the same callback-independent transport/control timeline. Every partition produced and retained 11,001 native batches, 448 triggers and 896 locks with zero drops. The independent P16/P20-C rational oracle reports exact native-boundary identity, event order, lock order, alternating pattern transitions and generation consumption.

All partitions have the same interleaved PCM hash, accepted-state hash and RNG state. Enabled and disabled diagnostic instances also match host/native PCM, state, parameters, event totals and scheduling. IAT allocation instrumentation reports zero callback allocations. The measured 1,024-frame diagnostic callback stack watermark is 20,416 bytes on a 1 MiB stack. Capture cost observations are diagnostic wall-clock samples, not a claimed worst-case real-time bound.

Exact-DLL transition capture passes stopped/start, playing/stop, restart, tempo changes while stopped and playing, seeks forward/backward, loop wrap, host/internal clock switching and near-native-boundary reanchor. Every completed batch in these callbacks is retained with zero drops. Pending-pattern activation is additionally exercised by the transition-copy scenarios.

## Copy at pending-to-active transition

The test uses the actual P19-E `SequencerEditFacade` and a known pattern-1 track-0 source. It copies to track 1 while pattern 1 is pending and pattern 0 is active. A deliberately different track-2 sentinel prevents recurrence of P20-B's wrong-destination probe error.

Copy is observed before transition, at the last native refill before transition, on the representable transition callback, immediately after transition, across a tempo change and across seek/reanchor. Every accepted edit publishes exactly one generation, preserves chain and unrelated track data, becomes the processing generation, and yields complete paired track-0/track-1 triggers and locks. The immediately-after case correctly observes the remaining 30 paired triggers after the already scheduled first boundary; scheduler behavior was not changed to force retroactive suppression. Stale and Busy requests preserve the newest accepted state.

Fresh P16-E Euclidean/copy/clear qualification passes 9,869/0 at O0 and O2. The retained P20-B exact-host regression passes 833/0, including Euclidean, copy, clear, production Import and multi-instance behavior. The original wrong-destination diagnostic remains preserved in P20-B evidence.

## Regression, external gates and publication

The fresh checkpoint passes 64/64 commands, including native O0/O2, original-C filter 80,000 exact blocks per optimization, snare, FIR/latency, MIDI/controller/state, P15-P19 suites, exact host tests and Steinberg validator 47/47. Host IDs 0-245, LXRS v1/v2/v3, 51,065/51,089-byte streams, 65,536-byte maximum state and 26-sample PDC remain covered. The protected installed DLL remains `89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`; all seven resources remain unchanged.

P20-D controlled-host retained capture and pattern-transition copy are **PASS**. The production Import path is **PRODUCTION_PASS**. Actual REAPER GUI and event acceptance remain **BLOCKED** because no new supported desktop-control capability became available; no new attachment campaign or blind input was attempted. Historical PCM remains **UNRESOLVED**, and the historical FIR image remains **HISTORICAL_IDENTITY_UNPROVABLE**.

Development publication remains **BLOCKED** under the existing checkpoint policy: P20-B/P20-C required the actual REAPER and original historical acceptance gates before publication, and P20-D does not authorize their waiver or replacement. No files were staged, committed or pushed. P16-F final certification remains blocked/open.

Machine evidence is under `out/p20d/`; `out/p20d/verification_report.md` is the concise checkpoint report.
