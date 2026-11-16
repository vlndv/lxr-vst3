"""Reconcile P16-F5 evidence. Never stage, certify unresolved gates, or publish."""
import csv
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'out/p16f5'
sys.path.insert(0, str(ROOT / 'tools/reaper'))
from analyze_p16f3 import pcm, state


def read(path):
    b = Path(path).read_bytes()
    return b.decode('utf-16' if b.startswith(b'\xff\xfe') else 'utf-8-sig')


def load(path):
    return json.loads(read(ROOT / path))


def sha(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest().upper()


def write(name, data):
    (OUT / name).write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True).rstrip('\n')


def main():
    baseline = load('out/p16f5/baseline_inventory.json')
    candidate = load('out/p16f5/private-build-001/candidate_manifest.json')
    common = dict(utc=datetime.now(timezone.utc).isoformat(), candidate_sha256=candidate['sha256'], module=candidate['module'])
    assert sha(candidate['module']) == candidate['sha256']
    assert git('branch', '--show-current') == baseline['branch']
    assert git('rev-parse', 'HEAD') == baseline['head']
    assert not git('diff', '--cached', '--name-status')
    protected = baseline['protected_entries']
    assert all(sha(x['path']) == x['sha256'] for x in protected)
    assert load('out/p16f5/prior_evidence_integrity.json')['status'] == 'PASS'
    write('protected_install_manifest.json', dict(**common, status='PASS', entries=protected,
          build_checks=['private-build-001/protected_install.json']))
    production = [x for x in load('out/p16f5/private-build-001/source_manifest.json')['files']
                  if x['path'].startswith(('dsp/', 'interfaces/', 'data/', 'vendor/'))]
    assert all(sha(ROOT / x['path']) == x['sha256'] for x in production)
    candidate.update(final_production_inputs_match_build=True, production_inputs=production,
                     msbuild_version='18.10.1.42706', certification='FAIL — PROTECTED_NOTE_PCM_CHANGED',
                     binary_analysis='binary-analysis-001/binary_analysis.json',
                     note='Later test/documentation changes do not change compiled production inputs. One private DLL used for every final host test.')
    write('candidate_binary_manifest.json', candidate)

    # Exact original eight failures, each independently compared with reference.
    fail = load('out/p16f5/midi_cc_failure_reproducer.json')
    trace = OUT / 'cc-valid-host-002-trace'
    repaired = []
    for old in fail['failed_assertions']:
        p = trace / (old['test_id'] + '.f64')
        ref = trace / ('matrix4_' + old['sequencer'] + '_32.f64')
        a, b = p.read_bytes(), ref.read_bytes()
        assert a == b
        repaired.append(dict(test_id=old['test_id'], status='PASS', sha256=sha(p), reference_sha256=sha(ref),
                             first_differing_host_frame=None, maximum_abs_difference=0,
                             previous_first_frame=old['first_differing_host_frame'],
                             event_trace=str(p.with_suffix('.csv').relative_to(OUT))))
    note = load('out/p16f5/midi_note_regression_results.json')
    note['starting_private_sha256'] = note.pop('private_sha256', baseline['private_candidate']['sha256'])
    note['final_private_sha256'] = candidate['sha256']
    note['final_source_suite'] = 'cc-valid-host-002/results.json'
    final_notes = []
    for p in sorted((OUT / 'baseline-reproducer/trace').glob('*.f64')):
        if p.name.startswith('matrix4_'):
            continue
        q = trace / p.name
        final_notes.append(dict(file=q.name, sha256=sha(q), identical_to_frozen_F4=p.read_bytes()==q.read_bytes()))
    note['final_note_pcm'] = final_notes
    note['final_status'] = 'FAIL'
    note['cross_binary_failure'] = load('out/p16f5/midi_note_cross_binary_failure.json')
    write('midi_note_regression_results.json', note)
    expanded = OUT / 'cc-expanded-005-trace'
    pcms = []
    for rate in (32000, 48000, 96000):
        for mode in range(5):
            prefix = f'rate{rate}_mode{mode}'
            reference = (expanded / (prefix + '_profile0.f64')).read_bytes()
            for profile in range(9):
                p = expanded / (prefix + f'_profile{profile}.f64')
                assert p.read_bytes() == reference
                pcms.append(dict(rate=rate, mode=mode, profile=profile, sha256=sha(p), identical=True))
            repeat = expanded / (prefix + '_repeat.f64')
            assert repeat.read_bytes() == reference
            assert (expanded / (prefix + '_without_cc.f64')).read_bytes() != reference
    native = [dict(file=str(p.relative_to(OUT)), sha256=sha(p), bytes=p.stat().st_size)
              for p in expanded.glob('*.s16')]
    write('midi_cc_test_results.json', dict(**common, status='PASS', assertions=548168, failures=0,
          scope='CC history/ordering correction only; full F5 implementation FAILs frozen-note audio preservation',
          original_eight_assertions=repaired, original_suite=dict(pass_count=1660, fail_count=0, evidence='cc-valid-host-002/results.json'),
          expanded_suite='cc-expanded-006/results.json', trace_directory='cc-expanded-005-trace',
          features=['chronological CC88 values', 'identical repeated values retained', 'NRPN1 and interleaved CC',
                    'callback-level host automation before MIDI', 'idle readback and no unsolicited gestures',
                    'unadvertised channel rejected; seven note channels preserved', 'routing/instance isolation',
                    'invalid offsets and values', 'negative and INT32_MAX counts', '4095/4096/4097 admission',
                    'shared CC/note bound and overflow recovery', 'project restore resets partial NRPN'],
          expected_transitions=[dict(host_offset=n, raw88=v) for n, v in [(0, 20), (32, 60), (64, 90)]],
          capacity=dict(source_queues=376, visited_cc_points=4096, visited_events=4096, shared_messages=4096,
                        overflow='reject newest with separate observable counters; no coalescing'),
          compiler_denial='cc-expanded-005/results.json remains BLOCKED; unchanged assertion source normally executed with VS LLVM in 006'))
    write('cross_callback_pcm_results.json', dict(**common, status='PASS', comparisons=pcms,
          configurations=15, profiles=[1, 32, 64, 128, 256, 512, 8192, '32/256 alternating', '1/17/93/5/256/31/127/64/3'],
          matched_repeat_count=15, observable_controller_pcm_differences=15,
          native_reference_captures=native,
          native_limit='Resident native ST1 stereo blocks captured at one-host-frame reference cadence, before FIR. Other partitions verified by exact host PCM; no unmeasured per-block native trace is claimed.',
          original_eight=repaired, reaper=load('out/p16f5/reaper_cc_pcm_results.json'),
          cross_binary_preservation='FAIL — see midi_note_cross_binary_failure.json; within-candidate partition PASS does not satisfy frozen F4 audio preservation'))

    measurements = []
    text = read(OUT / 'resources-002/run.log')
    for name, allocations, size, stack in re.findall(r'MEASURE (\S+) allocations=(\d+) bytes=(\d+) stack_watermark=(\d+)', text):
        measurements.append(dict(scope=name, allocations=int(allocations), bytes=int(size), stack_watermark=int(stack)))
    write('allocation_results.json', dict(**common, status='PASS', measurements=measurements,
          resource_probe='resources-002/results.json', assertions=8252, failures=0,
          method='Normally loaded exact DLL, four imported allocator IAT slots measured in memory on the processing thread; initialization positive control; disk image unchanged.',
          limits='Observed calls in specified scopes, not universal zero-allocation proof. Host-owned input storage allocated before measured scopes. Initialization and production file import reported separately.'))
    (OUT / 'stack_usage_report.md').write_text(
        '# Final-DLL processing stack\n\n1,048,576-byte processing thread. A 512 KiB primed region is surveyed with ReadProcessMemory after each measured scope. '
        'Peak processing watermark: 19,616 bytes across 4,000 varying callbacks, transport, rolls, MIDI and NRPN. '
        '4,096-note callback: 15,096 bytes; 4,097 CC points plus one note: 15,096; overflow recovery: 14,840. All processing allocation counts zero.\n\n'
        'Import: 213,888 bytes and two allocations/4,232 bytes; setState: 275,792/zero observed allocations; getState: 228,880/zero. These are non-processing scopes. '
        'PDB/PE match; 2,719 unwind records and three unsupported helper records retained in binary-analysis-001. Watermarks are scenario observations, not exhaustive call-chain bounds.\n\n'
        'The first standalone LLVM lock harness used its linker default 1 MiB and exited 0xC00000FD before assertions. Original GCC standalone PE reserve is 2 MiB. '
        'A source rebuild/link with the same 2 MiB reserve passes 246/0 at Od/O2 (approved-locks-002). No PE header was patched. '
        'The exact-DLL probe still uses 1 MiB and passes 8,252/0. No production stack object was changed for this harness issue.\n', encoding='utf-8')

    profiles = {n: load('out/p16f5/' + n + '/reaper_results.json') for n in ('reaper-127-001', 'reaper-512-001')}
    write('reaper_results.json', dict(**common, profiles=profiles, actual_loaded_processes=load('out/p16f5/process_manifest.json'),
          checks_per_profile=[len(x['checks']) for x in profiles.values()], rich_state=load('out/p16f5/rich_state_restore_results.json'),
          real_midi_pcm=load('out/p16f5/reaper_cc_pcm_results.json'), overall='FAIL — PROTECTED_NOTE_PCM_CHANGED',
          import_gate='reaper_import_results.json', stress_gate='fourteen_instance_results.json'))
    live = list(csv.DictReader(read(OUT / 'reaper-live-001/live_transport.csv').splitlines()))
    write('reaper_transport_results.json', dict(**common, status='BLOCKED', offline_profiles=profiles,
          live_csv='reaper-live-001/live_transport.csv', live_rows=len(live),
          live_summary=read(OUT / 'reaper-live-001/live_results.txt'),
          final_readback=read(OUT / 'live-readback-001/run.log'),
          passed='Real playback at60/120/180, stop/resume, nonzero start, forward/back seeks, seven loop wraps, released roll masks, offline tempo map and26-sample PDC.',
          limitation='Polling and final readback establish execution/state, not sample-exact live transition acceptance. Dedicated exact-DLL tempo-boundary probe denied by Application Control under both approved compilers.',
          timing_probes=['tempo-boundary-001/results.json', 'tempo-boundary-002/results.json']))
    write('state_restore_results.json', dict(**common, status='PASS for executed scopes; production UI import remains BLOCKED',
          canonical_bytes=51065, maximum_bytes=65536, schema_changed=False,
          native_codec='current-suites-001/sequencer_persistence/serialization_results.json',
          final_host=['approved-host-001/P12cVST3StateTest/results.json', 'approved-host-001/P16F2VST3StateTest/results.json', 'host-001/results.json'],
          rich_state=load('out/p16f5/rich_state_restore_results.json'),
          legacy='v1/v2, legacy iPlug2, corrupt-state and coherent-authority tests executed unchanged in linked native/host suites'))
    offline = load('out/p16f5/reaper-512-001/independent_analysis.json')
    write('offline_pcm_results.json', dict(**common, raw_second_profile_analysis=offline,
          rich_state=load('out/p16f5/rich_state_restore_results.json'), midi=load('out/p16f5/reaper_cc_pcm_results.json'),
          raw_inequality='roll selector0 differs across native host-automation callback partitions; analyzer FAIL retained. Native automation remains callback-level last-point, distinct from timestamped MIDI equivalence.',
          historical_unresolved=['F4 preliminary OFF-route fourteen-instance inequality', 'F4 selector12 cross-profile inequality; current final DLL selector12 compares equal'],
          status='BLOCKED — historical reconciliation and full stress reopen pending'))

    stressroot = OUT / 'reaper-stress-001'
    expected = load('out/p16f5/reaper-stress-001/expected_banks.json')
    states = []
    for i in range(14):
        wire, modules = state(stressroot / f'initial_{i}_chunk.txt')
        states.append(dict(instance=i, canonical_bytes=len(wire), bank_sha256=hashlib.sha256(modules[2]).hexdigest().upper(),
                           patch_routes_sha256=hashlib.sha256(modules[1]).hexdigest().upper(), bank_nonempty=any(modules[2][32+50176:32+50176+112])))
    data, metrics = pcm(stressroot / 'stress_a.wav')
    periods = [0, 128, 64, 43, 32, 50, 16, 11, 8, 6, 4, 3, 2, 1]
    remote = re.findall(r'^REMOTE (.*)$', read(OUT / 'stress-readback-001/run.log'), re.M)
    rollrows = []
    for i, line in enumerate(remote):
        values = dict(p.split('=', 1) for p in line.split())
        count = 1 if i == 0 else sum((n % 128) % periods[i] == 0 for n in range(16, 160))
        released = all(int(values[k]) == 0 for k in ('logical_mask', 'audio_mask', 'applied_mask'))
        rollrows.append(dict(selector=i, observed_rate=int(values['rate']), observed=int(values['roll_events']), released=released,
                             empty_bank_reference_count=count, count_oracle_status='NOT APPLICABLE — rich banks have different track lengths/rotations/chaining/shuffle',
                             selector_release_status='PASS' if int(values['rate'])==i and released else 'FAIL'))
    assert len(rollrows) == 14
    write('fourteen_instance_results.json', dict(**common, status='BLOCKED', first_render=metrics,
          first_render_expected_frames=192000, instances=states, distinct_banks=len({x['bank_sha256'] for x in states}),
          expected_bank_manifest=expected, roll_events=rollrows, readback='stress-readback-001/results.json',
          progress=read(stressroot / 'progress.txt'),
          matched_rerender='NOT COMPLETED: no reopened snapshots, stress_b.wav or completion marker. Stop observed after valid first render; process responds, no forced termination.',
          diagnostics=['process_manifest.json', 'reaper-stress-001/thread_snapshot.json', 'host_application_errors.json'],
          resource_limit='No demonstrated disk or memory exhaustion: >532 GB disk free; sampled process private bytes946139136, working set406265856,64threads,643handles. These snapshots do not prove all resource limits absent.',
          cause='UNDETERMINED; UI capture unavailable. Do not infer plugin crash, deadlock or successful reopen from incomplete evidence.',
          previous_F4='Active-routing matched renders remain historical evidence; earlier OFF-route inequality remains unreconciled.'))

    # Collect every actual regression execution, preserving raw denied artifacts.
    raw = []
    def addrows(path):
        blob = load(path)
        def walk(x, opt=''):
            if isinstance(x, list):
                for v in x: walk(v, opt)
            elif isinstance(x, dict):
                opt = x.get('optimization', opt)
                cmd = x.get('command', [])
                if cmd and str(cmd[0]).lower().endswith('.exe') and 'status' in x and not x.get('name', '').startswith(('compile-', 'build-', 'link-')):
                    if not any(y in str(cmd[0]).lower() for y in ('gcc.exe', 'g++.exe', 'clang-cl.exe', 'cl.exe', 'cmake.exe', 'msbuild.exe')):
                        name = x.get('name') or Path(cmd[0]).stem + ('_' + opt if opt else '')
                        raw.append(dict(name=name, status=x['status'], command=cmd, evidence=str(path), summary=x.get('summary', [])))
                for k, v in x.items():
                    if isinstance(v, (list, dict)): walk(v, opt)
        walk(blob)
    resultfiles = ['native-001/results.json', 'approved-native-001/results.json', 'host-001/results.json',
                   'approved-additional-002/results.json', 'approved-locks-001/results.json', 'approved-locks-002/results.json',
                   'verified-source-reruns-001/results.json']
    resultfiles += [str(p.relative_to(OUT)) for p in (OUT / 'approved-host-001').glob('*/results.json')]
    resultfiles += [str(p.relative_to(OUT)) for p in (OUT / 'current-suites-001').glob('*/test_results.json')]
    resultfiles += ['current-suites-001/' + p for p in ('sequencer_contract/contract_test_results.json', 'sequencer_core/core_test_results.json',
                   'performance_sequencer/performance_sequencer_results.json', 'host_transport/transport_test_results.json',
                   'parameter_lock/parameter_lock_results.json', 'modulation_contract/contract_tests.json')]
    probes = ['cc-valid-host-002', 'cc-expanded-005', 'cc-expanded-006', 'resources-002', 'import-rejection-001',
              'tempo-boundary-001', 'tempo-boundary-002', 'stress-readback-001', 'live-readback-001',
              'listening-build-001', 'listening-cc-build-001']
    resultfiles += [p + '/results.json' for p in probes]
    for f in resultfiles: addrows('out/p16f5/' + f)
    write('approved_regression_results.json', dict(**common, executions=raw,
          equivalence='Approved VS LLVM strict FP/C++17/no RTTI/no exceptions; unchanged C compiled as C. Lock harness reserve matched original GCC2MiB. Two preserved approved artifacts freshly rerun only after all actual input/source/executable hashes verified; no old result transferred.',
          untouched_denials='Every denied counterpart remains BLOCKED. No policy, certificate, loader, PE header, or security setting changed.'))
    (OUT / 'application_control_status.md').write_text(
        '# Windows Application Control\n\nFresh compiler/executable identities and commands are in native-001, approved-native-001, host-001, approved-host-001 and approved-additional-002. '
        'Code Integrity3033/3077 and SAC3118 XML/messages are preserved in code_integrity_events.xml.txt and code_integrity_events.json. '
        'Example denied exact-DLL tempo LLVM SHA DE1AAAFA35664EDD1AF4FAC399CB0E38D083ADA5957937427F4DCCE10A1E0181; MSVC 1E58963F7D8FF760D90E9EF37459A29FB840AEF7C4D6F0CB6D18A0B4BDD48475; WinError4551 before entry. '
        'Unsigned status is recorded separately; it alone is not an explanation of every policy decision. Policy ID0283ac0f-fff1-49ae-ada1-8a933130cad6 appears in3077.\n\n'
        'Current snare-render Od61/0 and current percussion O247/0 execute normally; optimized original snare61/0 also passes through VS LLVM. '
        'Current FIR functionality/quality/latency pass through permitted executions. Historical exact FIR Functional O2 retains its separate restriction and is not relabelled by a source equivalent. '
        'The historical command names out/P12b4cResamplerTest_O2_snare.exe; that artifact is currently unavailable at this path, so no new exact-artifact PASS exists.\n\n'
        'No security bypass attempted. Both normal approved compiler builds of the tempo-boundary harness remain policy-BLOCKED. '
        'An initial native lock-harness stack overflow0xC00000FD was a distinct linker-reserve mismatch, not a4551 denial; its same-source2MiB rebuild passes.\n', encoding='utf-8')
    listening = list((OUT / 'listening').glob('*.wav'))
    write('listening_manifest.json', dict(**common, playlist='listening/playlist.m3u', files=[dict(path=str(p.relative_to(OUT)), sha256=sha(p)) for p in listening],
          generation_probes=['listening-build-001/results.json', 'listening-cc-build-001/results.json']))
    (OUT / 'listening_acceptance_status.md').write_text(
        '# Listening accepted for the seven F5 cases\n\nThe17 historical F3 cases and their human confirmation are preserved unchanged. '
        'F4 continuation approval did not supply auditory observations/player/output and cannot accept the changed F5 candidate. '
        'Seven current-DLL cases are in listening/playlist.m3u: multiple timed CCs, CC/velocity notes, MIDI/sequencer, manual rolls, shared hats, stop/resume, reload. '
        'Files were generated through the exact DLL; repeat PCM checks pass. The user replied "all is good" to the explicit seven-case listening question. '
        'This confirms no reported auditory issues for these files; player/output were not supplied. Listening is accepted for this DLL and these seven cases. '
        'It does not override the separately demonstrated frozen-note PCM regression.\n', encoding='utf-8')

    # Individual reconciliation of every F4 row, plus all new raw executions.
    latest = {}
    for x in raw:
        if x['name'] not in latest or x['status'] == 'PASS': latest[x['name']] = x
    for opt in ('O0', 'O2'):
        key='P15M2AModulationContractTest_'+opt
        if key in latest and latest[key]['status']=='PASS':latest['contract-'+opt]=latest[key]
    if 'base-mapping-O2' in latest:latest['P15M2BBaseMappingTest_O2']=latest['base-mapping-O2']
    dedicated = {'p16a-001': 'sequencer_contract/contract_test_results.json', 'p16b-001': 'sequencer_core/core_test_results.json',
                 'p16e-final-003': 'performance_sequencer/performance_sequencer_results.json',
                 'serializer-001': 'sequencer_persistence/serialization_results.json'}
    rows = []
    prior = list(csv.DictReader(read(ROOT / 'out/p16f4/regression_matrix.csv').splitlines()))
    for index, old in enumerate(prior, 1):
        name, suite = old['test'], old['suite']
        status, evidence, detail = 'NOT RUN', '', 'No unverified historical result transferred.'
        if 'disconnected' in name.lower(): status, evidence, detail = 'NOT APPLICABLE', 'out/p16f5/current-suites-001/lfo_runtime/test_results.json', 'Intentional disconnected negative control; raw assertion failure preserved, not a production defect.'
        elif name in latest:
            x = latest[name]; status, evidence = x['status'], x['evidence']; detail = 'Current source/exact DLL as scoped. Raw denied counterpart remains a separate row.'
        elif suite in dedicated:
            evidence = 'out/p16f5/current-suites-001/' + dedicated[suite]; status = load(evidence)['classification']
        elif suite.startswith('REAPER reaper-final-'):
            profile = 'reaper-127-001' if suite.endswith('026') else 'reaper-512-001'
            evidence = f'out/p16f5/{profile}/reaper_results.json'; status = profiles[profile]['checks'].get(name, {}).get('status', 'NOT RUN')
        elif suite == 'p16c-final-003': status, evidence = 'PASS', 'out/p16f5/approved-additional-002/results.json'
        elif suite == 'p16d-final-004': status, evidence = 'PASS', 'out/p16f5/approved-locks-002/results.json'
        elif 'resource' in suite or name == 'full4096 MIDI callback allocation/stack': status, evidence = 'PASS', 'out/p16f5/resources-002/results.json'
        elif name == 'multipoint existing MIDI CC partition regression' or name == 'note partition expanded1406': status, evidence = 'PASS', 'out/p16f5/cc-valid-host-002/results.json'
        elif name == 'P16F3ReaperReadbackProbe': status, evidence = 'PASS', 'out/p16f5/stress-readback-001/results.json'; detail = 'Read-only probe execution; independent event-count oracle separate.'
        elif 'all fourteen manual roll selectors' in name: status, evidence = 'BLOCKED', 'out/p16f5/fourteen_instance_results.json'; detail = 'Selectors/released masks observed; empty-bank event-count formula is inapplicable to rich rotated/chained/shuffled stress banks. No count PASS claimed.'
        elif name == 'tempo boundary': status, evidence = 'BLOCKED', 'out/p16f5/tempo-boundary-002/results.json'; detail = 'Both approved compilers denied before entry.'
        elif name == 'import rejection': status, evidence = 'PASS', 'out/p16f5/import-rejection-001/results.json'
        elif name == 'listening fixtures': status, evidence = 'PASS', 'out/p16f5/listening_manifest.json'; detail = 'Fixture generation only; human acceptance separate.'
        elif 'production UI import' in name: status, evidence = 'BLOCKED', 'out/p16f5/reaper_import_results.json'
        elif suite == 'human': status, evidence = 'PASS', 'out/p16f5/listening_acceptance_status.md'; detail='Historical17 accepted; user also reports all is good for seven F5 cases. Player/output not supplied.'
        elif name == 'rich two-instance project restore': status, evidence = 'PASS', 'out/p16f5/rich_state_restore_results.json'
        elif name == 'MIDI PCM across127/512 callbacks': status, evidence = 'PASS', 'out/p16f5/reaper_cc_pcm_results.json'
        elif 'fourteen simultaneous instances' in name: status, evidence = 'BLOCKED', 'out/p16f5/fourteen_instance_results.json'
        elif 'stress preliminary OFF-route' in name: status, evidence = 'BLOCKED', 'out/p16f5/offline_pcm_results.json'; detail = 'Historical executed inequality preserved; cause not established. Active-case old PASS cannot erase it.'
        elif 'roll selector12 cross-profile' in name: status, evidence = 'BLOCKED', 'out/p16f5/offline_pcm_results.json'; detail = 'Current exact-DLL profiles compare equal; earlier raw failure cause remains unreconciled. No unsupported repair claim.'
        elif name == 'MIDI track mute and roll bypass/release': status, evidence = 'NOT RUN', 'out/p16f5/reaper-mute-001'; detail = 'Final status separately reported in reaper_mute_results.json when available.'
        elif suite == 'historical exact artifact': status, evidence = 'BLOCKED', 'out/p16f5/application_control_status.md'
        elif name == 'TSan': status, detail = 'NOT RUN', 'Optional race detector; no proof claimed.'
        elif 'disconnected' in name: status, detail = 'NOT APPLICABLE', 'Expected negative control; actual launch/assertion results preserved below.'
        rows.append(dict(prior_row=index, suite=suite, test=name, prior_status=old['status'], prior_evidence=old['evidence'],
                         status=status, evidence=evidence, detail=detail))
    for x in raw:
        status = 'NOT APPLICABLE' if 'disconnected' in x['name'] and x['status'] == 'FAIL' else x['status']
        rows.append(dict(suite='F5 actual execution', test=x['name'], status=status, evidence=x['evidence'],
                         detail=' | '.join(x.get('summary', []))))
    for x in load('out/p16f5/midi_note_cross_binary_failure.json')['comparisons']:
        rows.append(dict(suite='F5 frozen note preservation',test=x['file'],status=x['status'],evidence='out/p16f5/midi_note_cross_binary_failure.json',detail='Same executable reproduces frozen F4 PCM with old DLL; new DLL comparison first frame='+str(x['first_host_frame'])))
    for x in rows:
        x['required_binary'] = candidate['sha256'] if any(k in x['suite'].lower() for k in ('host', 'reaper', 'resource', 'f4')) else 'Current source; exact executable/provenance in linked manifest'
        x['environment'] = 'Windows x64 normal enforcement; documented GCC or approved VS18 MSVC/LLVM; REAPER7.80 where scoped'
        x['expected'] = 'Original assertions, exact values and matched-condition PCM identity; no relaxation'
    fields = ['prior_row', 'suite', 'test', 'prior_status', 'prior_evidence', 'status', 'required_binary', 'environment', 'expected', 'evidence', 'detail']
    with (OUT / 'regression_matrix.csv').open('w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fields); w.writeheader(); w.writerows(rows)

    paths = sorted(set(git('ls-files', '-c', '-o', '--exclude-standard').splitlines()))
    sources = [dict(path=p, sha256=sha(ROOT / p)) for p in paths if (ROOT / p).is_file()]
    fingerprint = hashlib.sha256(''.join(x['path'] + '\0' + x['sha256'] + '\n' for x in sources).encode()).hexdigest().upper()
    changed = [dict(path=line[3:], sha256=sha(ROOT / line[3:])) for line in git('status', '--porcelain=v1').splitlines()]
    oldpaths = {x['path']: x['sha256'] for x in baseline['changed_files']}
    f5 = [x for x in changed if oldpaths.get(x['path']) != x['sha256']]
    write('file_inventory.json', dict(**common, source_fingerprint=fingerprint, source_files=sources, changed_files=changed,
          F5_changed_or_added=f5, staged_paths=[], starting_source_fingerprint=baseline['source_fingerprint'], build_source_fingerprint=candidate['source_fingerprint']))
    remote = git('ls-remote', 'origin', 'refs/heads/' + baseline['branch']).split()[0]
    assert remote == baseline['head'], 'Remote changed: publication prohibited'
    (OUT / 'integrity_report.md').write_text(
        '# Integrity PASS\n\nExpected root/branch/HEAD/origin and empty index verified. All74 starting changed paths and442 source hashes matched F4. '
        'All starting source bytes archived;141,348 earlier evidence files (8,757,476,588 bytes) rehashed unchanged. '
        'Pinned firmware/iPlug2/SDK unchanged. Protected DLL and seven resources match before/after private build and final audit. '
        'All final production source/data inputs match build001; later harness/docs differ from build whole-tree fingerprint and are explicitly inventoried. '
        'No reset/clean/stash/rebase/merge/amend/branch switch, staging, commit, push, tag, security bypass, installation or forced termination.\n\n'
        f'Starting={baseline["source_fingerprint"]}\n\nBuild={candidate["source_fingerprint"]}\n\nFinal working={fingerprint}\n\n'
        f'Local/live remote={remote}. No publication attempted while gates blocked.\n', encoding='utf-8')
    report = [
        ('Overall classification', 'FAIL — final audit finds47/76 frozen note PCM files changed, first native sync sample0 before FIR. Stop conditions6/12/13 applied; no production repair beyond CC scope attempted.'),
        ('P16-F1', 'BLOCKED final host certification; native O0/O2 6246/0 and exact-DLL105/0 pass; dedicated tempo-boundary executable denied.'),
        ('P16-F2', 'BLOCKED production Import-button acceptance; canonical serializer O0/O2 and exact-DLL3300/0 pass.'),
        ('P16-F3/F4', 'FAIL final preservation audit; note callback invariance passes within each DLL, but frozen cross-binary audio is not preserved.'),
        ('P16-F5 implementation classification', 'FAIL — original CC eight failures repaired (1660/0 original,548168/0 extended), but47/76 note-only frozen PCM comparisons fail. Do not certify based on CC suite alone.'),
        ('Publication classification', 'BLOCKED / NOT ATTEMPTED; index empty, no staging or commits.'),
        ('Starting HEAD', baseline['head']), ('Final local HEAD', git('rev-parse', 'HEAD')), ('Final live remote HEAD', remote),
        ('Exact changed files', f'{len(changed)} combined paths; {len(f5)} F5 changed/added paths. Complete lists below and file_inventory.json.'),
        ('Starting/final source fingerprints', f'{baseline["source_fingerprint"]} / {fingerprint}; build whole-tree {candidate["source_fingerprint"]}. Final production inputs exactly match build001.'),
        ('Original CC mechanism', 'Pinned iPlug2 ProcessParameterChanges takes getPoint(numPoints-1), losing earlier same-controller points before LXR MIDI collection.'),
        ('First divergence', 'Host queue contains both CC88 values9600/9664; only later one translated to MIDI. First old host PCM difference9600/9601, max0.04967037262126655. No FIR cause inferred.'),
        ('Correction', 'LXR-local bounded non-owning one-point views expose each accepted ordinary CC point to unchanged dispatcher. Existing F4 note sort/sample loop and source equations unchanged; observed cross-binary audio is not preserved.'),
        ('Eight failed assertions', 'All eight executed and PASS; each exact reference hash/first-difference-null recorded in midi_cc_test_results.json.'),
        ('Note regression', f'{len(final_notes)} frozen note PCM comparisons:29 identical/47 different. Same corrected host harness reproduces all76 original files when loading F4 DLL. Sync/native first sample0 differs -47 versus -14460; disabled original fixture remains identical.'),
        ('CC/NRPN timing', 'Three values20/60/90 at0/32/64 observed exactly; repeated values retained; CC88/NRPN1 ordering, automation/readback, reset and instance isolation pass.'),
        ('Cross-callback PCM', '135 exact-DLL renders across15 rate/mode configurations and9 profiles;15 repeat identities and15 observable CC differences. All6 real REAPER renders exact across127/512.'),
        ('Queue bounds/overflow', '376 source queues,4096 visited CC points,4096 input events,4096 shared accepted MIDI messages; reject-newest diagnostics, peak occupancy, huge/negative counts, overflow/reset/recovery tested.'),
        ('Native PCM', 'FAIL frozen F4 preservation: sync first native sample0 differs(-47 versus -14460), max14413 integer units in microscope capture; disabled original .s16 identical. Difference precedes FIR and external MIDI event; first differing internal input/state field not established. No synthesis/FIR diagnosis invented.'),
        ('REAPER Import', 'BLOCKED: exact process/project/path/hash verified; capture twice returned Computer Use app approval timed out. Zero input actions; actual control/import/malformed UI acceptance NOT RUN. Parser rejection62/0 separate.'),
        ('REAPER transport/performance', '40 checks/profile;44 rich checks; live60/120/180, play/stop/resume/seeks/seven wraps/release observed. Sample-exact live/tempo-boundary acceptance incomplete/policy-BLOCKED.'),
        ('Fourteen-instance stress', 'First4s/192000-frame WAV valid,14 nonempty distinct banks and14 roll selectors/released masks observed. Independent rich-bank event-count oracle incomplete; empty-bank formula inapplicable. Reopen/rerender incomplete after first render; host responsive. BLOCKED, no crash/limit cause invented.'),
        ('State save/reopen', 'Executed canonical/legacy/corrupt/coherent-authority tests pass; all245 controller values/12 routes per instance restored in REAPER rich test.'),
        ('Offline rendering', 'Matched main/rich/MIDI repeats identical; roll0 cross-profile native-automation inequality preserved; historical OFF-route stress/selector12 inequality reconciliation remains blocked.'),
        ('Allocations/stack', 'Zero measured processing allocations:4000 callbacks,4096 notes,4097 CC+note overflow and recovery. Peak19616 bytes on1MiB; initialization/import separate; measurement scope limits documented.'),
        ('Listening', 'User reports all is good for seven current-DLL cases; accepted with no reported issues. Player/output not supplied. Historical17-case acceptance preserved; listening does not override measured PCM failure.'),
        ('Application Control/regression matrix', f'Every one of {len(prior)} F4 rows individually reconciled; raw current executions appended. Snare/percussion current approved runs pass; exact tempo and historical FIR artifact restrictions remain separate.'),
        ('LXRS v3', '51065 canonical/65536 maximum unchanged;228 patch bytes,12 routes, transport,8 patterns/7 tracks, bypass, v1/v2 and legacy migration/corruption tests preserved.'),
        ('Private DLL SHA256', candidate['sha256'] + f';{candidate["bytes"]}bytes;Release x64/v145;deployment disabled.'),
        ('Steinberg validator', 'Exact final DLL47/47 PASS; host-001/results.json.'),
        ('Protected installed SHA256', '89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82;all7 resources unchanged.'),
        ('Commits/push', 'None; normal push not attempted because mandatory gates remain blocked. Local/live remote unchanged; no force push/history change.'),
        ('Evidence paths', 'out/p16f5/; docs/p16f5_midi_cc_timing_repair.md; all earlier evidence unchanged.'),
        ('Remaining limitations', 'Genuine unexplained frozen-note native PCM change; production UI access; sample-exact final tempo/live verification; completed14-instance matched reopen; prior raw inequalities; historical exact FIR artifact. Active certification stopped after native regression.'),
        ('Next milestone', 'NOT READY. Stop at P16-F5; no presets, morphing, GUI expansion, factory kits, sample loading or unrelated features.')]
    lines = ['# P16-F5 final checkpoint — FAIL', '', '| # | Required item | Result |', '|---:|---|---|']
    lines += [f'| {i} | {k} | {v} |' for i, (k, v) in enumerate(report, 1)]
    lines += ['', '## Exact F5 changed/added paths', ''] + ['- `' + x['path'] + '` — `' + x['sha256'] + '`' for x in f5]
    lines += ['', '## Complete combined changed-file inventory', ''] + ['- `' + x['path'] + '` — `' + x['sha256'] + '`' for x in changed]
    (OUT / 'verification_report.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(json.dumps(dict(overall='FAIL', original_eight='PASS', expanded_assertions=548168, frozen_note_failures=47, prior_rows=len(prior), matrix_rows=len(rows), changed_paths=len(changed), final_source_fingerprint=fingerprint)))


if __name__ == '__main__':
    main()
