# P16-F6 — frozen MIDI PCM divergence recovery

The requested local investigation and repair pass. This is not combined final
REAPER certification or publication. No commit or push is authorized in F6.

All work used `C:\Projects\lxr-vst3`, branch `p12b-4c-fir-resampler`,
HEAD `7749ef1a0203f5ab92bf0e2f447b53bb95dadf68`, with the actual uncommitted
F1–F5 changes. Starting447 source hashes and79 changed paths matched F5 exactly;
the Git index was empty. F4/F5 evidence, original commands and all76 original
F4 PCM references were accessible. The F5 frozen copies also match those original
F4 bytes. No clean clone or committed historical MIDI implementation was used.

The first verified divergence is **reset/initialization state**. An exact-DLL
probe applies the same LXRS candidate through ordinary VST3 processing, then
captures the effective patch/Engine state before native synthesis. The saved
state bytes and228 effective patch bytes are identical between F4 and F5.
However, all six native filters' input-delay `zi` fields are zero in F4 and contain
indeterminate nonzero storage in F5. At the first Sync native block, F5 produces
NaN integrator state in several voices. Its first native sample is-14460 versus
F4's-47, before any external MIDI event or FIR conversion.

The port's `ResonantFilter::init()` faithfully follows firmware `SVF_init()` and
does not assign `zi`. The firmware voices have `INCCMZ` storage: config.h:104
maps this to `.ccmz`, stm32_flash.ld:156 describes zero filling, and
startup_stm32f4xx.S:122–130 actually zeroes that segment. `StageCompleteRestore`
constructs a fresh `Engine{}` temporary in automatic storage. Its user-provided
constructor calls the startup initializers, but that does not supply a value
for this omitted scalar. F4's observed zero bytes were not guaranteed by the
C++ initialization contract. No precise optimizer/stack-layout explanation is
needed or claimed for why those particular F4 bytes happened to be zero.

The controlled experiments use preserved DLLs, not reconstructed approximations:

| Experiment | Result |
|---|---|
| A: actual F4 timing/DLL | Restored `zi` zero; first native block matches frozen F4. |
| B: actual F5 timestamped CC/DLL | Same persistent/effective patch bytes, dirty `zi`, first native sample differs. |
| C: F5 note callbacks with no CC queue object | All76 note files still match original F5 and47 still differ from F4; timestamps unchanged. |
| F5 test-only intervention: zero only six `zi` fields before rendering | First native block becomes bit-identical to F4. |
| Poisoned placement storage, unfixed filter |10 checks pass/6 fail; nonzero poison survives construction/startup init. |

No CC event precedes the first Sync divergence. C removes even the empty
parameter-change object on note-only callbacks, retaining the same event list,
timestamped note queue and per-sample dispatch. CC queue ordering and block-start
note handling therefore do not explain this first divergence.

The correction is one default member initializer in `dsp/ResonantFilter.h`:
`zi = 0.f`. It supplies the firmware's startup-memory value for fresh portable
objects. Filter equations, layout, `init()` and `reset()` bodies are unchanged.
In particular, the original repeated-`SVF_init` omission is preserved: the
regression sets an existing `zi` to0.25, calls init again and checks it remains
0.25. Explicit reset still clears it. LXR.cpp/LXR.h, note scheduling, MIDI CC
views, native32-frame quantization, PDC and serialization are unchanged in F6.

Private build001 SHA-256:
`4591AB2EBFDD6FE738F4983B9FBBF292C4B4E06DA7E0F7528C1E9426E050BE1D`
(1,253,376bytes), VS18/MSBuild18.10.1/Release x64/v145. Deployment events are
disabled; protected installed DLL and seven resources match before/after build.

Results on this exact candidate:

- Original CC fixture/suite:1660 PASS/0 FAIL, including all eight former failures.
- Expanded CC/NRPN/notes/automation/modulation/capacity suite:548168 PASS/0 FAIL.
- All76 frozen MIDI PCM comparisons: bit-identical, zero differences. References
  were neither regenerated nor overwritten.
- The restored scalar trace, patch/state bytes and first native block match F4.
- Poisoned-storage initialization atO0/O2:16/0 each.
- Original-C filter differential atO0/O2:80000 blocks, zero bitwise buffer/state
  mismatches each.
- Exact-DLL resource probe:8252/0; zero observed processing allocations across
 4000 variable callbacks,4096 notes,4097 CC points plus a note and recovery.
  Peak processing stack19616bytes on1MiB. These are measured scopes, not universal
  allocation/stack proofs.
- Steinberg validator:47/47.

The frozen references represent the already-corrected F4 timestamp contract.
Their original run argument0 disables the legacy block-start microscope. They
include nonzero callback offsets and are partition invariant. There is no
conflict between the references and the note-offset correction, and no rollback
to block-start note timing occurred.

Evidence and complete provenance: `out/p16f6/verification_report.md`,
`first_divergence_diagnosis.json`, `frozen_reference_manifest.json`,
`frozen_pcm_comparison.json`, the diagnostic snapshot files and per-run manifests.
F4/F5 reports and failure evidence remain historical and unchanged. Their other
final host/publication gates are outside this investigation and are not credited
to this new DLL. The index stays empty and HEAD remains unchanged.
