"""Reconcile the stopped F4 checkpoint. Never stages, certifies or publishes."""
import csv
import hashlib
import json
import pathlib
import re
import subprocess
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / 'out/p16f4'


def read(path):
    b = pathlib.Path(path).read_bytes()
    return b.decode('utf-16' if b.startswith(b'\xff\xfe') else 'utf-8-sig')


def load(path):
    return json.loads(read(ROOT / path))


def sha(path):
    with pathlib.Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest().upper()


def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True).rstrip('\n')


def main():
    baseline = load('out/p16f4/baseline_inventory.json')
    candidate = load('out/p16f4/private-build-002/candidate_manifest.json')
    assert sha(candidate['module']) == candidate['sha256']
    assert git('branch', '--show-current') == baseline['branch']
    assert git('rev-parse', 'HEAD') == baseline['head']
    assert not git('diff', '--cached', '--name-status')
    now = datetime.now(timezone.utc).isoformat()
    common = dict(utc=now, candidate_sha256=candidate['sha256'], module=candidate['module'])
    protected = baseline['protected_entries']
    assert all(sha(x['path']) == x['sha256'] for x in protected)
    write('protected_install_manifest.json', {**common, 'status': 'PASS', 'entries': protected,
          'build_checks': ['private-build-001/protected_install.json', 'private-build-002/protected_install.json']})
    build_sources = load('out/p16f4/private-build-002/source_manifest.json')['files']
    production = [x for x in build_sources if x['path'].startswith(('dsp/', 'interfaces/', 'data/', 'vendor/'))]
    mismatches = [x['path'] for x in production if sha(ROOT / x['path']) != x['sha256']]
    assert not mismatches, mismatches
    candidate.update(msbuild_version='18.10.1.42706', compiler_tools='14.51.36231',
                     final_production_inputs_match_build=True, production_inputs=production,
                     certification='FAIL — MULTIPOINT_MIDI_CC_INGESTION',
                     binary_analysis='binary-analysis-001/binary_analysis.json',
                     environment='test_environment.json')
    write('candidate_binary_manifest.json', candidate)

    processes = load('out/p16f4/process_manifest.json')
    for row in processes:
        assert row['modules'] and all(x['sha256'] == candidate['sha256'] for x in row['modules']), row
    write('import_button_results.json', {**common, 'status': 'BLOCKED — HOST_ACCESS',
          'process': next(x for x in processes if x['profile'] == 'reaper-001'),
          'project': str(OUT / 'reaper-001/F4_IMPORT.rpp'),
          'window': 'F4_IMPORT - REAPER v7.80 - EVALUATION LICENSE',
          'computer_use': 'Initial splash handle changed. Refreshed exact task window selection; get_window/get_window_state returned Computer Use app approval timed out.',
          'graphical_inputs': 0, 'actual_control_verified': False, 'bank_imported_by_UI': False,
          'malformed_parser_probe': load('out/p16f4/import-rejection-001/results.json')['status'],
          'substitution_policy': 'No test setter/parser call credited as production Import-button acceptance.'})

    native = load('out/p16f4/native-001/results.json')
    host = load('out/p16f4/host-001/results.json')
    equivalents = load('out/p16f4/approved-native-001/results.json') + load('out/p16f4/approved-native-002/results.json')
    write('approved_regression_results.json', {**common, 'native': native, 'approved_VS_LLVM_equivalents': equivalents,
          'host': host, 'current_dedicated_runners': load('out/p16f4/current-suites-001/runs.json'),
          'equivalence': 'Same current DSP/test and unchanged pinned original C; VS18 clang-cl /Od or /O2, no exceptions/RTTI, ff-contract off and MSVC runtime. Established P16-E-R1 source-regression method. Raw denied artifacts remain BLOCKED.',
          'limits': 'This source-regression equivalence does not satisfy a separately required historical exact-artifact launch.'})
    denials = []
    for row in native + equivalents + host:
        if row['status'] == 'BLOCKED':
            executable = pathlib.Path(row['command'][0])
            denials.append({'name': row['name'], 'path': str(executable), 'sha256': sha(executable), 'error': 'WinError4551',
                            'command_and_provenance': row['command']})
    write('application_control_artifacts.json', denials)
    (OUT / 'application_control_diagnostics.md').write_text(
        '# Application Control diagnostics\n\nNormal process creation denied with WinError4551 before test entry. '
        'Code Integrity3033/3077 XML identifies VerifiedAndReputableDesktop, policy GUID '
        '{0283ac0f-fff1-49ae-ada1-8a933130cad6}, status0xc0e90002, requested signing level2 / validated1. '
        'Read-only evidence: code_integrity_events.xml, code_integrity_events_final.xml, artifact_signatures.json, '
        'application_control_artifacts.json and each runner commands/build log. No policy, registry, certificate, '
        'PE header, path identity or loader alteration. Unsigned/reputation decisions are not assertion failures.\n\n'
        'Current snare-render O0 passes61/0 and percussion O2 passes47/0 through normal VS LLVM; fresh GCC '
        'counterparts also pass. Other denied variants remain individually BLOCKED. Historical exact FIR '
        'Functional O2 remains BLOCKED and was not relabelled by a counterpart. No bypass attempted.\n', encoding='utf-8')

    profiles = {d: load('out/p16f4/' + d + '/reaper_results.json') for d in ['reaper-002', 'reaper-003']}
    analyses = {d: load('out/p16f4/' + d + '/independent_analysis.json') for d in profiles}
    rich = load('out/p16f4/rich_state_restore_results.json')
    assert rich['status'] == 'PASS' and rich['candidate_sha256'] == candidate['sha256']
    for result in profiles.values():
        assert result['candidate'] == candidate['sha256'] and all(x['status'] == 'PASS' for x in result['checks'].values())
    write('reaper_results.json', {**common, 'classification': 'FAIL — CC_REGRESSION; additional acceptance BLOCKED',
          'profiles': profiles, 'process_identity_and_module_hashes': processes, 'rich_state': rich,
          'production_import': 'BLOCKED — HOST_ACCESS', 'live_timing': 'host_timing_results.json',
          'stress': 'fourteen_instance_stress_results.json'})
    write('offline_pcm_results.json', {**common, 'profiles': analyses, 'rich_state': rich['pcm'],
          'original_failing_REAPER_MIDI_fixture_now_identical': analyses['reaper-003']['comparisons']['callback_sizes_midi_coexistence'],
          'unresolved_raw_roll_comparisons': {k: v for k, v in analyses['reaper-003']['comparisons'].items() if v['status'] != 'PASS'},
          'all_MIDI_partition_contract': 'FAIL — see midi_pcm_comparison.json CC regression',
          'assertion_policy': 'Retain every raw inequality; no tolerance or weaker contract substituted.'})
    write('state_restore_results.json', {**common, 'profiles': {d: {'states': a['states'], 'comparisons': {
          k: v for k, v in a['comparisons'].items() if not k.startswith('callback_sizes_')}} for d, a in analyses.items()},
          'rich': rich, 'final_exact_DLL_F2_transaction': next(x for x in host if x['name'] == 'P16F2VST3StateTest'),
          'canonical_stream_bytes': 51065, 'maximum_stream_bytes': 65536,
          'codec_O0_O2': load('out/p16f4/current-suites-001/sequencer_persistence/serialization_results.json'),
          'schema_changes': False})
    stress = {d: load('out/p16f4/' + d + '/independent_analysis.json') for d in ['stress-001', 'stress-002']}
    stress['stress-002']['process'] = next(x for x in processes if x['profile'] == 'stress-002')
    stress['stress-002']['scenario'] = '14 distinct eight-pattern banks, all seven tracks, shared120 BPM, transitions, shuffle, manual rolls, two-slot lock records, 12 active routes each, MIDI, save/reopen, two4-second offline renders.'
    write('fourteen_instance_stress_results.json', {**common, 'status': 'BLOCKED — EARLIER_RAW_PCM_INEQUALITY_UNRECONCILED',
          'active_route_scenario': 'PASS — completed,192000frames,bit-identical,14 full restored states and distinct banks',
          'runs': stress, 'old_F3_WAV': {'path': 'out/p16f3/reaper-rolls-029/fourteen_roll_rates.wav',
              'finding': 'Now finalized/valid192000frames at baseline inspection; exact reason for earlier unfinalized report not established.'},
          'observations': 'Hosts responding; no observed assertion/crash/truncation. Disk and process peaks recorded. No forced termination. Zero-allocation evidence is separate single-component measured coverage, not all14 hosts instrumented simultaneously.'})
    live = list(csv.DictReader((OUT / 'live-001/live_transport.csv').open()))
    live_result = read(OUT / 'live-001/live_results.txt')
    write('host_timing_results.json', {**common, 'classification': 'BLOCKED — COMPLETE_TIMING_ACCEPTANCE_UNFINISHED',
          'live_observations': live_result, 'row_count': len(live),
          'rows': live, 'final_profiles': {d: r['checks'] for d, r in profiles.items()},
          'limits': 'Live host API polling is a coarse trace; no sample-exact plugin event timeline inferred. Stop/seek/loop/tempo plus multi-CC equivalence are not all proven by the note fixtures. Independent rotation/length/copy/clear/Euclidean host boundaries remain incomplete.'})

    resource_log = read(OUT / 'resources-dense-003/run.log')
    measured = {name: {'allocations': int(a), 'requested_bytes': int(b), 'observed_stack_bytes': int(s)}
                for name, a, b, s in re.findall(r'MEASURE (\w+) allocations=(\d+) bytes=(\d+) stack_watermark=(\d+)', resource_log)}
    assert measured['process_4000_callbacks']['allocations'] == measured['process_4096_MIDI_events']['allocations'] == 0
    write('allocation_results.json', {**common, 'status': 'PASS — MEASURED_SCOPES', 'measurements': measured,
          'evidence': 'resources-dense-003/results.json', 'method': 'Four exact-DLL imported allocator IAT hooks; thread/plugin-call scope, positive initialization control, normal LoadLibrary. No disk binary patch; original IAT restored.',
          'coverage': '4000 varying callbacks, restore, MIDI/NRPN, host timing, locks and rolls; independent full4096-event sort/dispatch callback. Host event allocation outside measurement.',
          'limits': 'Observed scopes; no universal allocation proof or simultaneous14-instance allocation measurement.'})
    (OUT / 'stack_usage_report.md').write_text(
        '# Final candidate stack measurements\n\n1MiB processing thread. Observed varied4000-callback watermark13,648bytes; '
        'full4096-event callback9,128bytes. NonRT import213,872/setState275,776/getState228,864. '
        'Method: primed512KiB stack, ReadProcessMemory survey, including host-call overhead. '
        'Matching PE/PDB2716 unwind records; three unsupported helper records remain explicit in binary-analysis-001. '
        'No exhaustive call-chain maximum claimed. Resident callback queue4096 entries; std::sort uses bounded '
        'offset/sequence comparison in resident storage. No FIR/Engine algorithm changes.\n', encoding='utf-8')
    (OUT / 'listening_acceptance_status.md').write_text(
        '# BLOCKED — PENDING_HUMAN_ACCEPTANCE\n\nHistorical17 cases: user/VLC/headphones2026-11-10, no reported issues; '
        'preserved unchanged in F3. New timing affects delayed MIDI cases. Four final-candidate files and playlist '
        'are in listening/. User replied "approved you can keep going"; this authorizes continuation and '
        'does not report listening, player/output or auditory observations. No new listening PASS asserted.\n', encoding='utf-8')

    latest = {x['name']: x for x in native + host}
    for x in equivalents:
        if x['status'] == 'PASS' and latest.get(x['name'], {}).get('status') != 'PASS':
            latest[x['name']] = x
    dedicated = []
    for directory in ['base_mapping', 'velocity_routing', 'lfo_fidelity', 'lfo_runtime']:
        dedicated += load('out/p16f4/current-suites-001/' + directory + '/test_results.json')
    dedicated += load('out/p16f4/current-suites-001/modulation_contract/contract_tests.json')['tests']
    latest.update({x['name']: x for x in dedicated})
    latest['P15M2BBaseMappingTest_O2'] = latest['base-mapping-O2']
    latest['P16F3ExactDllResourcesTest'] = {
        'status': load('out/p16f4/resources-dense-003/results.json')['status'],
        'command': [str(OUT / 'resources-dense-003/results.json')]}
    prior = list(csv.DictReader((ROOT / 'out/p16f3/regression_matrix.csv').open(encoding='utf-8-sig')))
    assert len(prior) == 192
    rows = []
    suite_files = {'p16a-001': 'sequencer_contract/contract_test_results.json',
                   'p16b-001': 'sequencer_core/core_test_results.json',
                   'p16c-final-003': 'trigger_scheduler/scheduler_test_results.json',
                   'p16d-final-004': 'parameter_lock/parameter_lock_results.json',
                   'p16e-final-003': 'performance_sequencer/performance_sequencer_results.json',
                   'serializer-001': 'sequencer_persistence/serialization_results.json'}
    for index, row in enumerate(prior, 1):
        name = row['test']; status = 'INCOMPLETE'; evidence = ''; detail = 'Prior evidence retained; current acceptance not inferred.'
        if 'disconnected' in name:
            status = 'NOT_APPLICABLE — EXPECTED_NEGATIVE_CONTROL'
            detail = 'Intentional disconnected assertions are preserved, not production failures. Fresh raw launch/assertion result is listed separately.'
            evidence = latest.get(name, {}).get('command', [''])[0]
        elif name in latest:
            current = latest[name]; status = current['status']; evidence = current['command'][0]
            detail = 'Current source run; exact final DLL where host-scoped. Raw denied counterpart remains in current artifact rows.'
        elif row['suite'] in suite_files:
            p = OUT / 'current-suites-001' / suite_files[row['suite']]
            if p.exists():
                result = json.loads(p.read_text()); status = result['classification']; evidence = str(p)
                if status == 'FAIL' and '4551' in p.read_text(): status = 'BLOCKED — APPLICATION_CONTROL'
            else: status = 'BLOCKED — APPLICATION_CONTROL'; evidence = 'current-suites-001/trigger_scheduler.log'
        elif row['suite'].startswith('REAPER reaper-final-'):
            profile = 'reaper-002' if row['suite'].endswith('026') else 'reaper-003'
            status = profiles[profile]['checks'].get(name, {}).get('status', 'INCOMPLETE')
            evidence = profile + '/reaper_results.json'
        elif name == 'MIDI PCM across127/512 callbacks':
            status = 'PASS'; evidence = 'offline_pcm_results.json'; detail = 'Original two-note REAPER fixture repaired. General MIDI contract separately FAILs multipoint CC.'
        elif name == 'final-hash production UI import': status = 'BLOCKED — HOST_ACCESS'; evidence = 'import_button_results.json'
        elif name == 'rich two-instance project restore': status = 'PASS'; evidence = 'rich_state_restore_results.json'
        elif name == 'fourteen simultaneous instances render finalization':
            status = 'PASS'; evidence = 'stress-002/independent_analysis.json'; detail = 'Finalization/frame count and active-route repeat pass. Earlier exploratory PCM failure remains separate/unreconciled.'
        elif row['suite'] == 'human': status = 'BLOCKED — PENDING_HUMAN_ACCEPTANCE'; evidence = 'listening_acceptance_status.md'
        elif row['suite'] == 'historical exact artifact': status = 'BLOCKED — APPLICATION_CONTROL'; evidence = row['evidence']
        elif name == 'TSan': status = 'NOT RUN — OPTIONAL'; detail = 'No race-detector proof claimed.'
        elif name == 'import rejection':
            status = load('out/p16f4/import-rejection-001/results.json')['status']
            evidence = 'import-rejection-001/results.json'
        elif 'disconnected' in name: status = 'NOT_APPLICABLE — EXPECTED_NEGATIVE_CONTROL'; detail = 'Historical intentional FAIL retained; current negative-control launch status is listed separately.'
        rows.append({'prior_row': index, 'suite': row['suite'], 'test': name,
                     'prior_status': row['status'], 'prior_evidence': row['evidence'], 'status': status,
                     'required_binary': candidate['sha256'] if 'host' in row['scope'].lower() or 'dll' in row['scope'].lower() or 'REAPER' in row['suite'] else 'current source / explicit historical artifact where named',
                     'environment': 'Windows x64 normal enforcement; GCC or approved VS18 LLVM/MSVC; REAPER7.80 for host rows',
                     'expected': 'Original assertions/bit identity; no relaxation', 'evidence': evidence, 'detail': detail})
    for suite, values in [('current-native', native), ('approved-VS-LLVM', equivalents), ('current-final-DLL-host', host), ('current-dedicated-native', dedicated)]:
        for x in values:
            rows.append({'suite': suite, 'test': x['name'], 'status': 'NOT_APPLICABLE — EXPECTED_NEGATIVE_CONTROL' if 'disconnected' in x['name'] and x['status'] == 'FAIL' else x['status'],
                         'prior_status': x['status'] if 'disconnected' in x['name'] else '',
                         'evidence': x['command'][0], 'detail': ' | '.join(x.get('summary', []))})
    for name, status, evidence in [('note partition expanded1406', 'PASS', 'diagnostic-expanded-llvm-004/results.json'),
            ('multipoint existing MIDI CC partition regression', 'FAIL', 'diagnostic-cc-005/results.json'),
            ('stress preliminary OFF-route PCM identity', 'FAIL — CAUSE_UNRESOLVED', 'stress-001/independent_analysis.json'),
            ('roll selector12 cross-profile PCM identity', 'FAIL — CAUSE_UNRESOLVED', 'reaper-003/independent_analysis.json'),
            ('full4096 MIDI callback allocation/stack', 'PASS', 'resources-dense-003/results.json')]:
        rows.append({'suite': 'P16-F4', 'test': name, 'status': status, 'evidence': evidence})
    for row in rows:
        row.setdefault('required_binary', candidate['sha256'] if row['suite'] in ['P16-F4', 'current-final-DLL-host'] else 'current source; executable identity in linked result/command manifest')
        row.setdefault('environment', 'Windows x64, normal enforced policy; documented GCC/VS18 LLVM/MSVC or REAPER7.80 as scoped')
        row.setdefault('expected', 'Original assertions and matched-condition bit identity; no relaxation')
    fields = ['prior_row','suite','test','prior_status','prior_evidence','status','required_binary','environment','expected','evidence','detail']
    with (OUT / 'regression_matrix.csv').open('w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fields); writer.writeheader(); writer.writerows(rows)

    (OUT / 'midi_callback_partition_diagnostic.md').write_text(
        '# MIDI callback partition diagnosis — FAIL\n\nOriginal valid note fixture: event offsets discarded in LXR. '
        'Callback starts9525/9216; offsets75/384; native boundaries8736/8448. Native first difference8448 before FIR. '
        'Baseline microscopes reproduce real callback PCM exactly; clean resets repeat. Repair retains valid '
        'offsets, stable order and fixed capacity; note matrices248/0 and1406/0; REAPER original MIDI now identical.\n\n'
        'Final mandatory CC fixture: Note On9500; CC88=0 at9600; CC88=100 at9664; Note Off16800. '
        'Valid ascending same-ID IParamValueQueue points. Pinned ProcessParameterChanges reads only the last '
        'point, so the earlier MIDI CC never reaches LXR. First differences9600(disabled)/9601(sync); '
        'maximum0.04967037262126655.1652 checks pass/8 fail. Production stopped under user stop condition8. '
        'No shared framework modification or tolerance change. This is a separately demonstrated ingestion '
        'defect, not a FIR or synthesis diagnosis. Native pre-FIR microscope captures belong to the original '
        'note fixture; no unmeasured CC native waveform is fabricated.\n\n'
        'Exact payloads, callbacks, offsets, native-boundary CSVs, selected Engine phase/velocity state, '
        'resident native block PCM/FIR input and double host PCM are preserved under trace-baseline/, '
        'trace-candidate-002/, trace-expanded-003/ and trace-cc-regression-005/. See the two MIDI JSON reports.\n', encoding='utf-8')

    paths = sorted(set(git('ls-files', '-c', '-o', '--exclude-standard').splitlines()))
    sources = [{'path': p, 'sha256': sha(ROOT / p)} for p in paths if (ROOT / p).is_file()]
    fingerprint = hashlib.sha256(''.join(x['path']+'\0'+x['sha256']+'\n' for x in sources).encode()).hexdigest().upper()
    changed = [{'path': line[3:], 'sha256': sha(ROOT / line[3:])} for line in git('status', '--porcelain=v1').splitlines()]
    f4_changes = [x for x in changed if x['path'] not in {v['path'] for v in baseline['changed_files']} or
                  x['sha256'] != next(v['sha256'] for v in baseline['changed_files'] if v['path'] == x['path'])]
    write('file_inventory.json', {**common, 'source_fingerprint': fingerprint, 'source_files': sources,
          'changed_files': changed, 'F4_changed_or_added': f4_changes, 'staged_paths': [],
          'starting_source_fingerprint': baseline['source_fingerprint'], 'build_source_fingerprint': candidate['source_fingerprint']})
    remote = git('ls-remote', 'origin', 'refs/heads/p12b-4c-fir-resampler').split()[0]
    assert remote == baseline['head'], 'Remote changed; no publication permitted'
    report = [
        ('Overall classification', 'FAIL — remaining demonstrated multipoint MIDI CC ingestion defect; production stopped.'),
        ('P16-F1', 'BLOCKED overall; current native O2 6246/0, final host variant policy-denied, completed REAPER subsets.'),
        ('P16-F2', 'BLOCKED overall; final transaction3300/0, native codecO0 152002/0/O2 denied, production UI import blocked.'),
        ('P16-F3/F4', 'FAIL — note-offset correction passes; final CC extension1652 PASS/8 FAIL.'),
        ('Publication', 'BLOCKED / NOT ATTEMPTED. No staging or commit.'),
        ('Starting HEAD', baseline['head']), ('Final local HEAD', git('rev-parse','HEAD')), ('Final live remote HEAD', remote),
        ('Exact changed files', f"{len(changed)} combined paths; {len(f4_changes)} F4 changed/added paths. Full lists below and in file_inventory.json."),
        ('Source fingerprints', f"Starting working={baseline['source_fingerprint']}; F3 tested={baseline['tested_build_source_fingerprint']}; F4 build={candidate['source_fingerprint']}; final working={fingerprint}. All final production inputs match build002."),
        ('MIDI first divergence', 'Original: LXR discarded mOffset; native frame8448 before FIR. Remaining: pinned CC dispatcher discards earlier queue points.'),
        ('MIDI correction / fixture finding', 'Valid original fixture, narrow shell offset retention. Valid multi-CC fixture still fails; no fixture exception invented.'),
        ('Cross-callback PCM', 'Original/expanded note tests bit-identical at all8 partitions; REAPER127/512 MIDI identical. Multi-CC128/256/alternating/512 fail; first9600/9601, max0.04967037262126655.'),
        ('Production Import button', 'BLOCKED — Computer Use app approval timed out before input in exact task context. No successful click claimed.'),
        ('Application Control', 'Code Integrity3033/3077, VerifiedAndReputableDesktop,0xc0e90002; raw hashes/signatures/commands/events preserved. No bypass.'),
        ('Snare/percussion', 'Approved LLVM snareOd61/0 and percussionO247/0; current GCC counterparts also pass. Other blocks separate.'),
        ('Fourteen-instance stress', 'Active routes/full distinct banks: completed192000frames twice, PCM/state exact. Earlier OFF-route raw mismatch frame0 unresolved; gate reconciliation BLOCKED.'),
        ('Host performance matrix', '40 checks/profile plus live play/stop/resume/seeks/tempo/seven loop wraps/release. Sample-exact live/independent boundaries incomplete; host_timing_results.json.'),
        ('Project save/reopen', 'Full canonical banks/state pass in both profiles; rich44 checks/all245 controls/12 active routes each; fourteen full state snapshots exact.'),
        ('Offline render identity', 'Matched main/rich/active-stress repeats pass; selector0/12 cross-profile and preliminary stress inequalities retained. offline_pcm_results.json.'),
        ('Allocation and stack', '0 allocations across4000 callbacks;13648-byte stack on1MiB. Full4096-event callback0 allocations/9128bytes. Measured scopes, not universal proof.'),
        ('Listening', 'Historical17-case human acceptance retained. Four new MIDI cases pending; continuation approval supplies no auditory confirmation.'),
        ('LXRS v3 compatibility', '51065 canonical/65536 max, no schema change. Final state322/F23300/M2G91 plus codecO0 pass; O2 current codec policy-BLOCKED.'),
        ('Complete regression matrix', 'All192 prior rows individually retained/reconciled with current status, environment and evidence; current artifacts and new failures appended. No group PASS asserted.'),
        ('Private VST3 SHA-256', candidate['sha256'] + f"; {candidate['bytes']}bytes; private-build-002."),
        ('Steinberg validator', 'Exact final candidate47/47 PASS.'),
        ('Protected installed SHA-256', protected[0]['sha256'] + '; all7 resources unchanged before/after both builds and final checks.'),
        ('Commit SHAs/titles', 'None. Proposed three-commit plan not executed because gates fail/block.'),
        ('GitHub push', 'NOT ATTEMPTED. Origin vlndv/lxr-vst3; local/live remote unchanged; no force push, merge, tags or history rewrite.'),
        ('Evidence', 'out/p16f4/ and docs/p16f4_final_certification_recovery.md. F1/F2/F3 evidence integrity audit recorded separately.'),
        ('Remaining blockers', 'Demonstrated CC defect; production UI access; policy-denied mandatory variants/exact artifact; human acceptance; roll/stress raw inequalities; incomplete independent host timing coverage.'),
        ('Next milestone readiness', 'NOT READY. Stop at P16-F4. No presets/morphing/factory kits/editor/samples or unrelated features.')]
    lines = ['# P16-F4 final report — FAIL', '', '| # | Required item | Result |', '|---:|---|---|']
    lines += [f'| {i} | {name} | {result} |' for i, (name, result) in enumerate(report,1)]
    lines += ['', '## Exact F4 changed or added paths', ''] + ['- `'+x['path']+'`' for x in f4_changes]
    lines += ['', '## Complete combined changed-file inventory', ''] + ['- `'+x['path']+'` — `'+x['sha256']+'`' for x in changed]
    lines += ['', 'The failed CC comparison is an executed production defect. Environmental BLOCKED rows remain distinct. No acceptance or publication PASS is asserted.', '']
    (OUT / 'verification_report.md').write_text('\n'.join(lines), encoding='utf-8')
    write('final_status.json', {**common, 'classification': 'FAIL', 'production_stop': 'User stop condition8: regression reveals new genuine defect',
          'head': baseline['head'], 'remote_head': remote, 'source_fingerprint': fingerprint, 'commits': [], 'push': 'NOT ATTEMPTED',
          'prior_rows_reconciled': len(prior), 'total_matrix_rows': len(rows)})
    print(json.dumps({'classification':'FAIL', 'changed_files':len(changed),'F4_changes':len(f4_changes),'matrix_rows':len(rows),'final_source_fingerprint':fingerprint}))


if __name__ == '__main__':
    main()
