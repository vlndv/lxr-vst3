"""Independent final-candidate REAPER roll/mute/readback evidence, no host writes."""
import csv
import hashlib
import json
from pathlib import Path
import re

from analyze_p16f3 import pcm

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'out/p16f3'


def text(path):
    b = path.read_bytes()
    return b.decode('utf-16' if b.startswith(b'\xff\xfe') else 'utf-8-sig')


def save(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2) + '\n')


def main():
    candidate = json.loads((OUT / 'private-build-006/candidate_manifest.json').read_text())
    common = {'candidate_sha256': candidate['sha256'], 'module': candidate['module']}
    periods = [0, 128, 64, 43, 32, 50, 16, 11, 8, 6, 4, 3, 2, 1]
    log = text(OUT / 'readback-rolls-final-014.log')
    lines = re.findall(r'^REMOTE (.*)$', log, re.M)
    assert len(lines) == 14 and 'SUMMARY pass=179 fail=0' in log
    rows = []
    for selector, line in enumerate(lines):
        values = dict(p.split('=', 1) for p in line.split())
        expected = 1 if selector == 0 else sum((n % 128) % periods[selector] == 0 for n in range(16, 160))
        actual = int(values['roll_events'])
        released = all(int(values[k]) == 0 for k in ['logical_mask', 'audio_mask', 'applied_mask'])
        rows.append({'selector': selector, 'period': periods[selector] if selector else 'one-shot',
                     'expected_events': expected, 'observed_events': actual, 'released': released,
                     'status': 'PASS' if actual == expected and released and int(values['rate']) == selector else 'FAIL'})
    metrics = {'status': 'BLOCKED — REAPER_RENDER_NOT_FINALIZED'}
    if (OUT / 'reaper-rolls-029/roll_counts_ready.txt').exists():
        _, metrics = pcm(OUT / 'reaper-rolls-029/fourteen_roll_rates.wav')
        assert metrics['nonzero'] and metrics['frames'] == 192000
    save('final_roll_event_counts.json', {**common, 'status': 'PASS' if all(r['status'] == 'PASS' for r in rows) else 'FAIL',
         'rows': rows, 'pcm': metrics, 'process_id': 33100, 'timer_list_rva': 1004400,
         'readback': 'readback-rolls-final-014.log', 'fixture': 'reaper-rolls-029/fourteen_roll_rates.rpp',
         'oracle': 'Press0.25s ordinal16 through release2.5s ordinal160 at120BPM. Period uses physical track position ordinal%128; selector5 retains50. Independent of observed counters.',
         'probe_provenance': 'readback-mute-final-009/results.json: same unchanged executable/source, normal LoadLibrary of matching final module and read-only target process access.'})

    raw, metrics = pcm(OUT / 'reaper-mute-031/midi_mute_roll.wav')
    samples = [int.from_bytes(raw[i:i+3], 'little', signed=True) / 8388608 for i in range(0, len(raw), 3)]
    windows = {}
    for label, start, end in [('before_mute', 0, .1), ('muted', 1.2, 1.45), ('roll_held', 1.6, 2.4), ('released', 3, 4)]:
        s = samples[int(start * 48000) * 2:int(end * 48000) * 2]
        windows[label] = {'peak': max(map(abs, s), default=0), 'rms': (sum(x*x for x in s) / len(s)) ** .5}
    prior = OUT / 'reaper_mute_pcm_results.json'
    original = OUT / 'reaper_mute_initial_zero_tail_assertion.json'
    if prior.exists() and not original.exists():
        old = json.loads(prior.read_text())
        assert old['status'] == 'FAIL'
        original.write_text(json.dumps(old, indent=2))
    log = text(OUT / 'readback-mute-final-009/run.log')
    roll = int(re.search(r'roll_events=(\d+)', log).group(1))
    # Probe's historical label ordinary_events prints diagnostics.emittedEvents,
    # which counts all dispatched events, including rolls. Do not call it an
    # ordinary-only counter in the acceptance result.
    total = int(re.search(r'ordinary_events=(\d+)', log).group(1))
    released = all(re.search(k + r'=0\b', log) for k in ['logical_mask', 'audio_mask', 'applied_mask'])
    accepted = roll == 64 and total == 65 and released and windows['muted']['peak'] == 0 and windows['roll_held']['peak'] > 0
    save('reaper_mute_pcm_results.json', {**common, **metrics, 'status': 'PASS' if accepted else 'FAIL', 'windows': windows,
         'expected_roll_events': 64, 'observed_roll_events': roll, 'expected_total_events': 65, 'observed_total_events': total,
         'released_masks': bool(released), 'readback': 'readback-mute-final-009/run.log',
         'production_interface': 'Actual REAPER MIDI NRPN200..206, host rate13 and held bit0; no Engine setter.',
         'source_contract': 'Pinned sequencer.c:973-988 clears roll bits on release; it does not cut already-triggered voices. One ordinary event precedes NRPN mute plus64 periodic events at ordinals96..159. All masks0 and no extra dispatched events.',
         'initial_analysis_error': 'The preliminary exact-zero post-release assertion was not the source contract. Its failure and the residual decaying one-shot PCM are retained in reaper_mute_initial_zero_tail_assertion.json. No PCM identity assertion was relaxed.'})

    path = OUT / 'reaper-live-033/live_transport.csv'
    with path.open(newline='') as f:
        live = list(csv.DictReader(f))
    result = text(OUT / 'reaper-live-033/live_results.txt')
    stages = set(r['stage'] for r in live)
    required = {'play_zero_120', 'stop_120', 'resume_120', 'resumed_120_pcm', 'play_nonzero_60', 'seek_forward_request', 'seek_backward_request', 'loop_enabled', 'manual_roll_release', 'stopped_readback'}
    resumed = [r for r in live if r['stage'] == 'resumed_120_pcm']
    resume_ok = len(resumed) == 1 and int(resumed[0]['play_state']) == 1 and float(resumed[0]['position']) > 0 and max(float(resumed[0][k]) for k in ['peak_left', 'peak_right']) > 0
    accepted = required <= stages and resume_ok and 'roll_released=true' in result
    save('live_transport_analysis.json', {**common, 'status': 'PASS — EXERCISED_TRANSITIONS' if accepted else 'FAIL', 'samples': len(live),
         'trace_sha256': hashlib.sha256(path.read_bytes()).hexdigest().upper(), 'stages': sorted(set(r['stage'] for r in live)),
         'peak': max(float(r[k]) for r in live for k in ['peak_left', 'peak_right']),
         'stop_release': 'roll_released=true' in result, 'play_stop_resume': resume_ok, 'result': result,
         'readback': 'readback-live-final-016.log',
         'limits': 'Trace establishes actual REAPER play states, cursor moves, tempo, peaks and release. It is not a sample-exact sequencer event timeline.'})
    print(json.dumps({'roll_counts': all(r['status'] == 'PASS' for r in rows), 'mute_and_release': accepted, 'live_rows': len(live)}))


if __name__ == '__main__':
    main()
