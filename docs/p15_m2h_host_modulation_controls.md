# P15-M2H host modulation controls

Status: **PASS** (2026-11-05).

P15-M2H was resumed from the interrupted working tree at
`bcf21a709044db33c8d079d9bc45461cc89ff82a`. The protected installed VST3 was
first restored to its certified SHA-256 and every resumed Release build used
`/p:PostBuildEventUseInBuild=false`. The controlled private build did not alter
the installed bundle.

## Implemented host contract

The interrupted implementation adds exactly twelve integer VST3 parameters:

| ID | Name |
|---:|---|
| 228 | Velocity Destination D1 |
| 229 | Velocity Destination D2 |
| 230 | Velocity Destination D3 |
| 231 | Velocity Destination SN |
| 232 | Velocity Destination CY |
| 233 | Velocity Destination HH |
| 234 | LFO Destination D1 |
| 235 | LFO Destination D2 |
| 236 | LFO Destination D3 |
| 237 | LFO Destination SN |
| 238 | LFO Destination CY |
| 239 | LFO Destination HH |

Each control is discrete over 0..65535 with OFF at zero. Codes retain original
firmware target identities; no seven-bit conversion exists. The controls are
projections of the P15-M2G `RoutingStateAuthority`, not another state owner.
Controller edits and processor automation validate the typed source/owner/target
combination before publication. Invalid values retain the accepted route and the
controller is reconciled without an automation gesture.

The LXRS v2 wire format remains unchanged: 516 plugin bytes and 520 VST3 stream
bytes. LXRS v1 and validated legacy iPlug2 states migrate with all twelve routes
OFF. IDs 0..227 and MIDI CC/NRPN behavior remain unchanged.

## Fresh standalone and VST3 results

- Host-control O0/O2: 131611 PASS, 0 FAIL per mode; all 65536 codes, twelve
  descriptors, 107 supported routes, 39 targets above 127, and zero measured
  allocations.
- Private VST3 controller: 684 PASS, 0 FAIL; twelve IDs, all 65536 selector
  codes at double and float-rounded precision, 107 route identities, automation,
  rejection, restore, concurrency and instance isolation.
- P15-M2G VST3 state: 91 PASS, 0 FAIL; twelve routes and 520-byte stream.
- Existing state/runtime/controller probes: 322/1143/955/1033 PASS, 0 FAIL.
- Steinberg validator: 47 PASS, 0 FAIL.

The private Release x64/v145 DLL is
`708EB4C1FEB55598762E1D4F9EE3C113D3C959FBCE325B580EDB8915371D1DD0`.
The installed certified DLL remains
`89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82`.

## REAPER certification

REAPER 7.80/x64 loaded the exact private DLL from the isolated bundle; the
running module path and hash were captured independently. Two instances, all
twelve controls, invalid row 204 rejection, controller readback, project
save/reopen, all 228 original parameters, 24 route values, bypass and automation
restoration passed. Velocity-only, LFO-only and combined active route renders
each differ from the separately saved/reopened OFF route render. Reopening the
active project produces bit-identical PCM. Both instances report 26 samples of
PDC at 48 kHz and repeated transport start/stop passes.

Earlier active/OFF attempts used a zero modulation depth (`VELO_MOD_AMT_1=0`
and LFO amount 0). Their identical PCM was correct native behavior, not a
production route defect. Those attempts remain preserved. The final fixture
sets both depths explicitly and proves each source independently without
changing production DSP or weakening an assertion.

## Environment restrictions

Windows Application Control blocked six freshly built native variants with
WinError 4551, including the historical FIR functional O2 executable. The
blocked binaries were not retried or bypassed. Passing counterpart variants are
reported independently; blocked results are not represented as PASS.

## Scope boundary

No sequencer, substep, morphing, preset-browser, kit-management or graphical UI
work is included. The twelve controls use the generic host parameter surface.
