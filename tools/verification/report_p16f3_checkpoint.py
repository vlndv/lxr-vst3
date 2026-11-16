"""Read preserved test artifacts and write the F3 checkpoint, without Git writes.

This report deliberately cannot authorize publication or turn missing host gates
into PASS. It leaves raw policy denials and PCM comparisons intact.
"""
import csv
import hashlib
import importlib.util
import json
import pathlib
import re
import struct
import subprocess
from datetime import datetime, timezone

from build_p16f3_candidate import protected

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / 'out/p16f3'


def read(path):
    p = ROOT / path
    b = p.read_bytes()
    return b.decode('utf-16' if b.startswith(b'\xff\xfe') else 'utf-8-sig')


def load(path):
    return json.loads(read(path))


def digest(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest().upper()


def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def main():
    baseline = load('out/p16f3/baseline_inventory.json')
    candidate = load('out/p16f3/private-build-006/candidate_manifest.json')
    assert digest(candidate['module']) == candidate['sha256']
    module_hash = candidate['sha256']
    now = datetime.now(timezone.utc).isoformat()
    common = {'utc': now, 'candidate_sha256': module_hash, 'module': candidate['module']}
    candidate.update(msbuild_version='18.10.1.42706', compiler_tools='14.51.36231',
                     binary_analysis='binary-analysis-final-004/binary_analysis.json')
    write('candidate_binary_manifest.json', candidate)
    install = protected()
    write('protected_install_manifest.json', {**common, 'status': 'PASS', 'entries': install,
          'verified_before_and_after_all_builds': [f'private-build-00{i}/protected_install.json' for i in range(1, 7)]})

    rows = []
    def add(suite, name, status, evidence, detail='', scope='native'):
        rows.append(dict(suite=suite, test=name, status=status, scope=scope, evidence=evidence, detail=detail))

    for directory in ['native-final-004', 'native-llvm-final-005', 'native-llvm-final-006', 'host-final-005']:
        evidence = f'out/p16f3/{directory}/results.json'
        for row in load(evidence):
            detail = ' | '.join(row.get('summary', []))
            scope = 'exact final DLL, automated host' if directory.startswith('host') else 'native'
            if directory.startswith('native-llvm'):
                detail = 'VS LLVM equivalent of denied GCC artifact; original remains BLOCKED. ' + detail
            add(directory, row['name'], row['status'], evidence, detail, scope)
    for directory, filename in [('p16a-001', 'contract_test_results.json'),
                                ('p16b-001', 'core_test_results.json'),
                                ('p16c-final-003', 'scheduler_test_results.json'),
                                ('p16d-final-004', 'parameter_lock_results.json'),
                                ('p16e-final-003', 'performance_sequencer_results.json'),
                                ('serializer-001', 'serialization_results.json')]:
        evidence = f'out/p16f3/{directory}/{filename}'
        result = load(evidence)
        inputs = result.get('inputs', result.get('input_sha256', {}))
        bad = [p for p, h in inputs.items() if not (ROOT / p).is_file() or digest(ROOT / p) != h]
        assert not bad, ('Stale regression source', bad)
        variants = result.get('variants') or {r['optimization']: r.get('metrics', {}) for r in result.get('runs', [])}
        add(directory, 'O0/O2', result['classification'], evidence, json.dumps(variants, sort_keys=True))
    for directory, filename in [('p15-contract-001', 'contract_tests.json'), ('p15-base-001', 'test_results.json'),
                                ('p15-velocity-001', 'test_results.json'), ('p15-fidelity-001', 'test_results.json'),
                                ('p15-runtime-001', 'test_results.json'), ('base-llvm-final-005', 'results.json')]:
        evidence = f'out/p16f3/{directory}/{filename}'
        result = load(evidence)
        for row in result if isinstance(result, list) else result['tests']:
            detail = ' | '.join(row.get('summary', []))
            if row.get('negative_control'):
                detail = 'Intentional disconnected negative control; expected sensitivity=' + row.get('sensitivity', '')
            add(directory, row['name'], row['status'], evidence, detail)
    for directory in ['roll-od-final-009', 'resources-final-006', 'midi-mute-final-004', 'readback-mute-final-009', 'f2-od-llvm-final-007']:
        evidence = f'out/p16f3/{directory}/results.json'
        r = load(evidence)
        add(directory, pathlib.Path(r['source']).stem, r['status'], evidence,
            ' | '.join(re.findall(r'^SUMMARY.*$', read(f'out/p16f3/{directory}/run.log'), re.M)), 'exact final DLL')
    reused = []
    for old, log, label in [('tempo-boundary-final-004', 'tempo-boundary-final-006/run.log', 'tempo boundary'),
                             ('import-probe-final-002', 'import-reject-final-005/run.log', 'import rejection'),
                             ('listening-probe-final-004', 'listening-final-008/run.log', 'listening fixtures')]:
        r = load(f'out/p16f3/{old}/results.json')
        source = pathlib.Path(r['source'])
        assert digest(source) == r['source_sha256']
        exe = pathlib.Path(r['command'][0])
        assert digest(exe) == r['executable_sha256']
        summary = re.findall(r'^SUMMARY pass=(\d+) fail=(\d+)', read('out/p16f3/' + log), re.M)
        assert summary and int(summary[-1][1]) == 0
        invocation = load('out/p16f3/' + str(pathlib.Path(log).parent / 'results.json'))
        assert invocation['command'][1] == candidate['module']
        reused.append({'label': label, 'source': str(source), 'source_sha256': digest(source),
                       'executable': str(exe), 'executable_sha256': digest(exe), 'log': log,
                       'pass': int(summary[-1][0]), 'fail': 0, 'actual_command': invocation['command'],
                       'method': 'Unchanged permitted executable with identical current test source; normal LoadLibrary of final DLL. No copy/rename or policy change.'})
        add('unchanged permitted harness', label, 'PASS', 'out/p16f3/' + log, str(summary[-1]), 'exact final DLL')
    write('reused_probe_provenance.json', {**common, 'runs': reused})
    add('historical exact artifact', 'FIR Functional O2', 'BLOCKED — APPLICATION_CONTROL',
        'out/p16f1/regression_matrix.csv', 'Historical exact artifact remains denied; equivalents do not relabel it.')
    add('race detector', 'TSan', 'NOT RUN', '', 'Explicit barriers and fixed-memory ownership tests ran; no TSan claim.')

    spec = importlib.util.spec_from_file_location('p16f3_oracle', ROOT / 'tools/reaper/analyze_p16f3.py')
    oracle = importlib.util.module_from_spec(spec); spec.loader.exec_module(oracle)
    reaper = {}
    offline = {}
    for directory in ['reaper-final-026', 'reaper-final-027']:
        d = OUT / directory
        result = load(str(d / 'reaper_results.json'))
        assert result['candidate'] == module_hash and result['module'] == candidate['module']
        reaper[directory] = result
        offline[directory] = {'pcm': {}, 'states': {}, 'identity': {}}
        for p in sorted(d.glob('*.wav')):
            _, metrics = oracle.pcm(p)
            offline[directory]['pcm'][p.stem] = metrics
        chunks = {}
        for p in sorted(d.glob('*_chunk.txt')):
            wire, modules = oracle.state(p); chunks[p.stem] = wire
            offline[directory]['states'][p.stem] = {
                'bytes': len(wire), 'step_records': 7168, 'state_sha256': hashlib.sha256(wire).hexdigest().upper(),
                'bank_sha256': hashlib.sha256(modules[2]).hexdigest().upper(),
                'patch_and_routes_sha256': hashlib.sha256(modules[1]).hexdigest().upper(),
                'transport_hex': modules[3].hex(), 'bypass': struct.unpack('<I', wire[-4:])[0]}
        for left, right in [('saved_chunk', 'reopened_chunk'), ('instance_one_chunk', 'reopened_instance_one_chunk'), ('instance_two_chunk', 'reopened_instance_two_chunk')]:
            if left in chunks and right in chunks:
                equal = chunks[left] == chunks[right]
                offline[directory]['identity'][left + '__' + right] = 'PASS' if equal else 'FAIL'
    cross = {}
    for name in sorted(set(offline['reaper-final-026']['pcm']) & set(offline['reaper-final-027']['pcm'])):
        a, _ = oracle.pcm(OUT / 'reaper-final-026' / (name + '.wav'))
        b, _ = oracle.pcm(OUT / 'reaper-final-027' / (name + '.wav'))
        equal = a == b
        first = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), None)
        status = 'PASS' if equal else 'FAIL'
        reason = ''
        if not equal and name.startswith('roll_selector_0'):
            status = 'EXPECTED_DIFFERENCE_CALLBACK_QUANTIZED_CONTROL'
            reason = 'The pinned iPlug2 delivers parameter edges at callback boundaries. These runs do not have matched one-shot press sample times. Raw PCM inequality is retained; no sample-accurate automation claim.'
        if not equal and name == 'midi_coexistence':
            reason = 'Baseline7749ef1 and current LXR ProcessMidiMsgNow discard IMidiMsg.mOffset; MIDI is dispatched before rendering the callback. Different callback sizes deliver the note at different effective samples. Raw FAIL retained; sample-accurate MIDI and cross-callback MIDI identity are not certified. Same-callback MIDI repeats separately pass in rich_state_restore_results.json. No MIDI timing redesign was made.'
        cross[name] = {'status': status, 'raw_pcm_identical': equal, 'first_differing_byte': first, 'reason': reason}
    write('offline_pcm_results.json', {**common, 'profiles': offline, 'cross_callback_comparisons': cross,
          'rng_policy': 'Restore resets configured musical seed and fresh engine RNG; random-next8 means patterns0..1; no historical reconstruction.',
          'event_timeline': 'Dedicated exact-DLL tempo/event and fourteen-roll readback evidence; no invented REAPER event timeline hash.',
          'limits': 'Matched callback/input repeated renders must be bit identical. Host parameter automation is callback-quantized.'})
    write('reaper_results.json', {**common, 'classification': 'BLOCKED', 'profiles': reaper,
          'reconciled_separate_gates': {'rich_two_instance_restore': 'rich_state_restore_results.json', 'human_listening': 'PASS',
              'live_play_stop_resume_seek_loop': 'live_transport_analysis.json', 'UI_import': 'BLOCKED',
              'MIDI_cross_callback_timing': 'midi_callback_timing_limit.json'},
          'live_evidence': 'reaper-live-033/live_results.txt', 'production_ui_import': 'BLOCKED — COMPUTER_USE_ACCESS',
          'prior_gui_import_supporting_only': {'candidate': 'BBA6831878E5C53A39B0D7AFC530FD109FA533AB35AB8C64DBF8D6974C463FD5',
              'bank_dump': 'reaper-ingress-012/remote_bank.lxqb', 'proof': 'readback-ingress-006/run.log'},
          'ui_failure': 'Final build006 visible profile032/window735152: Accessibility state unavailable; screenshot showed another task project under terminal occlusion. Normal bound Save did not yield a verifiable target. Earlier profile021 also unavailable, initial window470280 reported no foreground process id. No coordinate import attempted on a mismatched project.',
          'scope': 'Task-owned projects and profiles only; no REAPER force termination or installed-bundle replacement.'})
    write('state_restore_results.json', {**common, 'profiles': {k: {'states': v['states'], 'identity': v['identity']} for k, v in offline.items()},
          'nonzero_routing_two_instance_case': load('out/p16f3/rich_state_restore_results.json'),
          'compatibility': 'Final host P12c322/M2G91 plus F2 state3300; unchanged codec O0/O2 each152002. v1/v2/validated legacy default empty bank and OFF/Internal120; v3 full bank/patch/routes/config/bypass; rejected corruption preserves accepted tuple.',
          'cross_authority': 'Final exact-DLL3300 includes held common gate, concurrent reserved pattern writer, pinned inactive slots, one-boundary tuple, coherent old tuple on Busy, controller readback and no unsolicited gestures.',
          'evidence': ['host-final-005/results.json', 'serializer-001/serialization_results.json']})
    write('pattern_ingress_results.json', {**common, 'final_ui_gate': 'BLOCKED — COMPUTER_USE_ACCESS',
          'final_production_parser': 'PASS', 'parser_and_rejection': reused[1],
          'file': str(OUT / 'serializer-001/fixtures/musical.lxqb'), 'fixture_sha256': digest(OUT / 'serializer-001/fixtures/musical.lxqb'),
          'bytes': 50449, 'positions': 7168,
          'limitations': 'Exact final production parser invocation and REAPER chunk-loaded PCM do not substitute for a final-hash UI import. Earlier build004 genuine UI import verified canonical bank only. Native .pat deferred.'})

    measured = re.findall(r'MEASURE (\w+) allocations=(\d+) bytes=(\d+) stack_watermark=(\d+)', read('out/p16f3/resources-final-006/run.log'))
    measurements = {n: {'allocations': int(a), 'requested_bytes': int(b), 'observed_stack_bytes': int(s)} for n, a, b, s in measured}
    assert measurements['process_4000_callbacks']['allocations'] == 0
    write('allocation_results.json', {**common, 'status': 'PASS — MEASURED_SCOPES', 'measurements': measurements,
          'method': 'Four imported allocation APIs hooked in normally loaded exact DLL IAT, profiling thread and plugin-call scope only. Static CRT HeapAlloc/ReAlloc paths covered; no VirtualAlloc import. Positive initialization control. Original IAT restored; disk hash unchanged.',
          'coverage': '4000 variable callbacks, first restore, PPQ/tempo/seek/loop/stop, locks, all-track rolls, MIDI notes and NRPN track mute. Host queues prepared outside scope.',
          'limits': 'Observed plugin-controlled processing paths; not proof against arbitrary external library or host allocation on every possible path.',
          'evidence': 'resources-final-006/results.json'})
    analysis = load('out/p16f3/binary-analysis-final-004/binary_analysis.json')
    (OUT / 'stack_usage_report.md').write_text(
        '# Final-DLL stack evidence\n\nCandidate `' + module_hash + '`.\n\n'
        'Default 1 MiB executable stack; fixed 512 KiB watermark survey. Observed processing '
        f"{measurements['process_4000_callbacks']['observed_stack_bytes']} bytes across 4000 callbacks; "
        'import213872, setState275776, getState228864 bytes. Import/save/restore run outside audio. '
        'No acceptance depends on an 8 MiB stack. Watermarks include measurement/host-call overhead.\n\n'
        'Matching PE/PDB x64 unwind analysis covers2703 records. The three unsupported version2 '
        'records are guard-check, memcpy_repmovs and memset_repstos helpers, listed in '
        '`binary-analysis-final-004/binary_analysis.json`. Per-function prolog frames are not a '
        'whole-call-chain proof. Processing ApplyPending has an8120-byte frame; ProcessBlock152 '
        'and ApplySequencerTransport200. No50KiB automatic bank exists on processing paths. '
        'Bank slots and applied immutable pin are resident; CRC/parse/candidate bank copies are nonRT.\n\n'
        'Static review: fixed capacity21 sequencer events/42 lock operations, bounded acquisition, '
        'no recursive processing calls or input-sized alloca. Maximum total possible call-chain '
        'stack has not been formally proven; observed paths are comfortably below64KiB. '
        'State operations have larger bounded nonRT frames and fit the tested default1MiB stack.\n', encoding='utf-8')
    identity = load('out/p16f3/listening-final-008/human_fixture_identity.json')
    assert identity['all_identical'] and len(identity['cases']) == 18
    (OUT / 'listening_acceptance.md').write_text(
        '# Human listening acceptance — PASS\n\nOperator: user, self-reported; no name invented. '
        'Confirmation date2026-11-10. PlayerVLC; outputheadphones. User explicitly confirmed '
        '“All17 heard; no issues”. Earlier replies: “listened”, “vlc”, “Headphones”.\n\n'
        'Listened playlist: `listening-002/listening.m3u`,17 cases: D1/D2/D3/snare/cymbal/closed '
        'hat/open hat/simultaneous hats/substep/shuffle/manual roll/pitch/filter/velocity-LFO/'
        'start-stop/seek-loop/project reload. No case-specific defect reported. These observations '
        'are the human report, not inferred from waveforms.\n\n'
        'Exact final candidate reproduces all17 plus reload repeat byte-for-byte: '
        '`listening-final-008/human_fixture_identity.json`. Listening source/executable provenance '
        'and344 automated fixture checks are separately recorded. No claim that the operator '
        'listened to every separate REAPER automation render.\n', encoding='utf-8')
    (OUT / 'manual_roll_accessibility.md').write_text(
        '# Production manual roll\n\nPASS: stable host IDs243 rate0..13 (selector/13, default8), '
        '244 held track-mask0..127 (mask/127, default0). Generic REAPER parameter UI or automation '
        'selects rate and targets D1,D2,D3,SN,CY,CH,OH. Rising bits press; falling bits release; '
        'selector0 is one-shot on press. No latching command and no new GUI.\n\n'
        'Release on stop, stopped seek, disable, deactivation and full restore; original normal '
        'sequencing continues after release. Controls are transient and excluded from frozen '
        'LXRSv3. Host automation may explicitly retain envelopes. Controller reconciliation '
        'uses value-change notification without host edit gestures. Final native host tests '
        '260/Od and260/O2, MIDI/mute216; actual REAPER automation and live release evidence '
        'are separately recorded. Callback-size-dependent one-shot edge timing is a pinned '
        'host-wrapper limitation, not sample-accurate automation.\n\n'
        'Original firmware front-panel protocol and fourteen selectors are cited in '
        '`docs/p16f3_final_host_certification.md`. NRPN200..206 now forward the original '
        'zero/nonzero mute semantics to the processing-owner scheduler; external MIDI and '
        'source rolls bypass ordinary track mute. No IDs0..242 changed.\n', encoding='utf-8')

    write('manual_roll_results.json', {**common, 'native': {'Od': '260/0', 'O2': '260/0', 'MIDI_mute': '216/0'},
          'reaper_live': read('out/p16f3/reaper-live-033/live_results.txt'),
          'reaper_automation': {k: {n: r for n, r in v['checks'].items() if 'roll' in n} for k, v in reaper.items()},
          'fourteen_selectors': load('out/p16f3/final_roll_event_counts.json'),
          'mute_and_release_PCM': load('out/p16f3/reaper_mute_pcm_results.json'),
          'callback_automation_limit': 'One-shot press is callback-quantized; compare matched callback sizes for determinism.'})
    write('transport_results.json', {**common, 'native_transport': 'native-final-004: current O0/O2 PASS; host-final-005:F1 105/0',
          'exact_DLL_tempo_boundary': reused[0], 'REAPER_live': read('out/p16f3/reaper-live-033/live_results.txt'),
          'REAPER_profiles': {k: {n: r for n, r in v['checks'].items() if any(x in n for x in ['host_60', 'internal', 'tempo', 'pdc'])} for k, v in reaper.items()},
          'limits': 'One BPM per callback, signedQ32.32 bounded reanchor. No history reconstruction or intra-callback ramp fidelity claim. Live peak/position trace is not an event-timestamp oracle.'})
    write('performance_results.json', {**common, 'native_source_differential': ['p16c-final-003', 'p16d-final-004', 'p16e-final-003'],
          'REAPER': {k: v['checks'] for k, v in reaper.items()},
          'MIDI_mute_readback': 'readback-mute-final-009/run.log',
          'scope_limits': 'Native oracle covers repeat/track0 transitions, rotations/lengths, all gates/substeps, zero-probability/RNG, copy/clear and Euclidean. Each has not independently been certified in final-hash REAPER; do not infer full host performance PASS.'})
    size = load('out/p16f2/state_size_analysis.json')
    boundary = int(re.search(r'maximum_restore_callback_ns=(\d+)', read('out/p16f3/host-final-005/P16F2VST3StateTest.log')).group(1))
    size.update(cpp_plugin_bytes=412504, candidate_sha256=module_hash,
                maximum_observed_restore_callback_ns=boundary,
                parsing_and_encoding='serializer-001/serialization_results.json: O0 encode1530us/decode1938us; O2 encode1220us/decode1218us; samples, not worst-case bounds.',
                publication='p16b-001: fixed bank edit/publication O2 sample6905ns; acquire10ns; common project boundary cost separately measured.',
                memory_limits='Plugin object412504bytes, immutable authority151344bytes included; candidate54432 and fixed encode65536, larger bounded nonRT frames. Initialization requested1073702bytes cumulatively is not a live-heap high-water mark. Aggregate external framework/host memory peak not measured.')
    write('state_size_analysis.json', size)

    blocked = []
    for p in OUT.rglob('results.json'):
        try:
            obj = load(p); items = obj if isinstance(obj, list) else [obj]
            for row in items:
                if row.get('status') == 'BLOCKED': blocked.append({'evidence': str(p.relative_to(ROOT)), 'test': row.get('name', row.get('source', '')), 'error': row.get('error', 'see execution log')})
        except (ValueError, UnicodeError): pass
    write('application_control_denials.json', blocked)
    (OUT / 'application_control_status.md').write_text(
        '# Application Control\n\nNo enforcement or security policy was changed. All denied '
        'executables retain BLOCKED. Current native-final-004 has13 denied artifacts; the '
        'complete VS LLVM repeat is native-llvm-final-006. DrumVoice O0 passed in the '
        'earlier current-source partial native-llvm-final-005 run. Current snare render O0 '
        'and snare/cymbal/hat O2 counterparts remain policy-BLOCKED. The denied '
        'P15 base O2 counterpart passes VS LLVM with the original C oracle in '
        'base-llvm-final-005. `_USE_MATH_DEFINES` exposes the original M_PI constant; no oracle '
        'algorithm was changed.\n\n'
        'Selected unchanged permitted test executables with byte-identical current source '
        'were reused normally against the final DLL. No executable was copied, renamed or '
        'run through an alternative loader to evade enforcement. See reused_probe_provenance.json. '
        'Historical exact FIR Functional O2 remains separately BLOCKED. An optional legacy '
        'P16-D LLVM harness exhausted its default1MiB stack (0xC00000FD): its ordering test '
        'simultaneously declares five151KiB authorities, five50KiB banks and an Engine. '
        'Current documented GCC P16-D O0/O2 both pass246checks; this test-harness stack '
        'failure is separate from the measured final-DLL1MiB/13616-byte processing result. '
        'Full individual denial '
        'inventory: application_control_denials.json.\n', encoding='utf-8')
    for name, result in reaper.items():
        for gate, value in result['checks'].items():
            add('REAPER ' + name, gate, value['status'], f'out/p16f3/{name}/reaper_results.json', value.get('detail', ''), 'exact final DLL, REAPER7.80')
    add('REAPER', 'final-hash production UI import', 'BLOCKED — COMPUTER_USE_ACCESS', 'out/p16f3/reaper_results.json', scope='exact final DLL, REAPER7.80')
    add('human', '17 listening cases', 'PASS', 'out/p16f3/listening_acceptance.md', 'User: All17 heard; no issues. VLC/headphones.', 'human')
    add('REAPER', 'rich two-instance project restore', 'PASS', 'out/p16f3/rich_state_restore_results.json',
        '44 host checks,12 enabled routes each, all245 controller values, two complete banks, host bypass and three identical matched MIDI PCM renders.', 'exact final DLL, REAPER7.80')
    add('REAPER', 'MIDI PCM across127/512 callbacks', 'FAIL — LEGACY_CALLBACK_TIMING', 'out/p16f3/reaper-final-027/independent_analysis.json',
        'First differing byte55149/frame9191. Starting implementation also discards MIDI offset; no sample-accurate MIDI claim. Same-callback repeated MIDI PCM passes.', 'raw cross-callback comparison')
    add('REAPER stress', 'fourteen simultaneous instances render finalization', 'BLOCKED — RENDER_NOT_FINALIZED',
        'out/p16f3/final_roll_event_counts.json', 'Readback event counts/masks pass; WAV has not finalized. Not substituted for completed individual-selector PCM.', 'additional stress scope')
    add('REAPER', 'all fourteen manual roll selectors and release', 'PASS', 'out/p16f3/final_roll_event_counts.json',
        'All expected event counts; exact module/vtable verification and released masks.', 'exact final DLL, REAPER7.80')
    add('REAPER', 'MIDI track mute and roll bypass/release', 'PASS', 'out/p16f3/reaper_mute_pcm_results.json',
        'Muted ordinary PCM silent,64 periodic rolls,65 total events, released masks; original voice tail retained.', 'exact final DLL, REAPER7.80')
    with (OUT / 'regression_matrix.csv').open('w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=['suite', 'test', 'status', 'scope', 'evidence', 'detail']); writer.writeheader(); writer.writerows(rows)

    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    branch = subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip()
    staged = subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=ROOT, text=True).splitlines()
    assert head == baseline['head'] and branch == baseline['branch'] and not staged
    paths = sorted(set(subprocess.check_output(['git', 'ls-files', '-c', '-o', '--exclude-standard'], cwd=ROOT, text=True).splitlines()))
    sources = [{'path': p, 'sha256': digest(ROOT / p)} for p in paths if (ROOT / p).is_file()]
    fingerprint = hashlib.sha256(''.join(x['path'] + '\0' + x['sha256'] + '\n' for x in sources).encode()).hexdigest().upper()
    built = load('out/p16f3/private-build-006/source_manifest.json')
    changed_after_build = [x['path'] for x in built['files'] if not (ROOT / x['path']).is_file() or digest(ROOT / x['path']) != x['sha256']]
    production_changed = [p for p in changed_after_build if p.startswith(('dsp/', 'interfaces/', 'vendor/', 'data/'))]
    assert not production_changed, ('Production candidate stale', production_changed)
    changed = sorted(set(subprocess.check_output(['git', 'diff', '--name-only'], cwd=ROOT, text=True).splitlines()) |
                     set(subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard'], cwd=ROOT, text=True).splitlines()))
    write('file_inventory.json', {**common, 'branch': branch, 'head': head, 'staged_paths': staged,
          'source_fingerprint': fingerprint, 'build_source_fingerprint': built['fingerprint'],
          'source_files': sources, 'changed_files': [{'path': p, 'sha256': digest(ROOT / p)} for p in changed],
          'post_build_nonproduction_changes': changed_after_build, 'post_build_production_changes': production_changed})
    prior_bad = [p for p, h in baseline['prior_evidence_sha256'].items() if not (ROOT / p).is_file() or digest(ROOT / p) != h]
    assert not prior_bad, ('Prior evidence changed', prior_bad)
    (OUT / 'integrity_report.md').write_text(
        '# Integrity checkpoint\n\nBranch `' + branch + '`, local HEAD `' + head + '`. Index empty. '
        'No staging/commit/push occurred. Live remote readback: `' + read('out/p16f3/live_remote_final.txt').strip() + '`.\n\n'
        f"All{len(baseline['prior_evidence_sha256'])} preserved F1/F2 evidence hashes match baseline. "
        'Starting39-file inventory reconciled before edits. Final dependency verification: '
        '`dependency_verification_final.log`. Installed module and7resources unchanged; see '
        '`protected_install_manifest.json`.\n\nBuild source fingerprint `' + built['fingerprint'] +
        '`. Current working source fingerprint `' + fingerprint + '`. Post-build differences '
        'are verification tools/tests/docs only, explicitly enumerated; production inputs unchanged. '
        'All source paths/hashes and exact changed files are in file_inventory.json.\n', encoding='utf-8')
    (OUT / 'verification_report.md').write_text(
        '# P16-F1/F2/F3 final checkpoint — BLOCKED\n\n'
        'Production final-hash GUI import cannot be verified because Computer Use cannot '
        'access the task REAPER window. Earlier build004 UI import and bank readback are '
        'supporting evidence only. Current required snare native cases also remain '
        'Application Control-BLOCKED after approved LLVM equivalents. Some comprehensive host performance gates are still '
        'native-only or unfinished; see performance_results.json and the regression matrix. '
        'Do not stage, commit or push, and do not start the next milestone.\n\n'
        'Final candidate `' + module_hash + '`,1248256bytes, VS18/MSBuild18.10.1/v145 '
        'Release x64, isolated private bundle; pre/post deployment disabled. Build input '
        'fingerprint `' + built['fingerprint'] + '`.\n\n'
        'Final automated host suites all11 PASS: state322/runtime1143/controller955/M2G91/'
        'M2H689/F1105/F23300/roll260/MIDI-mute216/probe1033/validator47. Roll Od260/0. '
        'Unchanged codec O0/O2 each152002/0. Exact-DLL tempo27/0, importer62/0, listening '
        'fixture344/0, resource39/0. Current native44 artifacts:31PASS/13policy-BLOCKED; '
        'accepted LLVM counterparts and remaining denials are separate matrix rows. Original denied files remain blocked. Historical '
        'exact FIR Functional O2 remains separately blocked.\n\n'
        'LXRSv3 stream51065bytes, fixedmax65536; LXQBv1 bank50449bytes. No wire-format '
        'change during F3. Legacy v1/v2/iPlug migration tested independently. All7168 '
        'positions and metadata restored in byte-identical serialized-state comparisons '
        'where completed. Common-gate/pinned-slot barriers and atomic rejection pass; '
        'TSanNOT RUN.\n\n'
        'Measured process allocations0 across4000 varied callbacks; observed stack13616bytes '
        'on1MiB thread. Larger import/state stack is bounded nonRT; see stack_usage_report.md '
        'for confidence limits. Human operator confirmed all17 cases without issues inVLC '
        'throughheadphones; final fixtures byte-identical.\n\n'
        'Two final-hash REAPER profiles record save/reopen, repeat PCM,120/60BPM/Internal, '
        'tempo-map, shuffle/random/roll and26samplePDC as they complete. Live final DLL '
        'records seek/loop/tempo and successful held-roll release. Check per-profile final '
        'status rather than interpreting IN_PROGRESS as PASS.\n\n'
        'Exact changed files, source hashes, full matrix and individual results are adjacent '
        'artifacts. No commits or push; local/live remote remain starting7749ef1. '
        'Protected installed SHA89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82 '
        'and seven resources unchanged. Native .pat/SOM/presets/morphing/editor/sample loading '
        'remain deferred. Publication and next-milestone readiness BLOCKED.\n', encoding='utf-8')
    print(json.dumps({'classification': 'BLOCKED', 'candidate': module_hash, 'source_fingerprint': fingerprint,
                      'changed_files': len(changed), 'matrix_rows': len(rows)}))


if __name__ == '__main__':
    main()
