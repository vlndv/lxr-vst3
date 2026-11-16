#!/usr/bin/env python3
"""P16-F2 independent fixtures and bounded codec verification; new outputs only."""
import argparse
import hashlib
import json
import pathlib
import subprocess
import sys
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--cxx', default='C:/msys64/ucrt64/bin/g++.exe')
    a = parser.parse_args()
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    subprocess.run([sys.executable, str(ROOT / 'tools/verification/generate_sequencer_fixtures.py'),
                    '--output', str(out / 'fixtures')], check=True, cwd=ROOT)
    runs = []
    for opt in ('O0', 'O2'):
        exe = out / ('P16F2SequencerStateTest-' + opt + '.exe')
        command = [a.cxx, '-std=c++17', '-fno-exceptions', '-fno-rtti', '-Wall', '-Wextra', '-' + opt,
                   str(ROOT / 'tests/P16F2SequencerStateTest.cpp'), str(ROOT / 'dsp/PatternBank.cpp'), '-o', str(exe)]
        row = {'optimization': opt, 'compile_command': command}
        build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
        (out / (opt + '-build.log')).write_text(build.stdout + build.stderr)
        row['compile_exit'] = build.returncode
        row['status'] = 'FAIL'
        if build.returncode == 0:
            row['executable_sha256'] = hashlib.sha256(exe.read_bytes()).hexdigest().upper()
            try:
                result = subprocess.run([str(exe), str(out / 'fixtures')], cwd=ROOT, capture_output=True, text=True)
                (out / (opt + '-run.log')).write_text(result.stdout + result.stderr)
                row['exit_code'] = result.returncode
                if result.returncode == 0:
                    row['metrics'] = json.loads(result.stdout)
                    row['status'] = 'PASS'
            except OSError as error:
                row['status'] = 'BLOCKED' if getattr(error, 'winerror', None) == 4551 else 'ERROR'
                row['error'] = str(error)
        runs.append(row)
    inputs = ['dsp/SequencerState.h', 'dsp/PatternBank.h', 'dsp/PatternBank.cpp',
              'vendor/iPlug2/Examples/LXR/LXRProjectState.h', 'vendor/iPlug2/Examples/LXR/LXRState.h',
              'tests/P16F2SequencerStateTest.cpp', 'tools/verification/generate_sequencer_fixtures.py',
              'tools/verification/run_sequencer_persistence_tests.py']
    result = {'utc': datetime.now(timezone.utc).isoformat(), 'runs': runs,
              'classification': 'PASS' if all(r['status'] == 'PASS' for r in runs) else 'BLOCKED' if any(r['status'] == 'BLOCKED' for r in runs) else 'FAIL',
              'input_sha256': {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest().upper() for p in inputs}}
    (out / 'serialization_results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
    return 0 if result['classification'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
