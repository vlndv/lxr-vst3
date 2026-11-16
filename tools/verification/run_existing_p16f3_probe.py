"""Reuse an unchanged, normally permitted current-source probe; never copy it.

Records the new actual DLL arguments. A denial is retained, never retried through
an alternative loader. Source and executable must match their original manifest.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--manifest', required=True)
    ap.add_argument('--output', required=True)
    ap.add_argument('arguments', nargs='*')
    a = ap.parse_args()
    old = json.loads((ROOT / a.manifest).read_text())
    assert old['status'] == 'PASS'
    source = Path(old['source'])
    exe = Path(old['command'][0])
    assert sha(source) == old['source_sha256']
    assert sha(exe) == old['executable_sha256']
    out = (ROOT / a.output).resolve()
    out.mkdir(parents=True, exist_ok=False)
    cmd = [str(exe), *[s if s.isdecimal() else str((ROOT / s).resolve()) for s in a.arguments]]
    result = {'source': str(source), 'source_sha256': sha(source), 'executable_sha256': sha(exe),
              'command': cmd, 'original_manifest': a.manifest,
              'method': 'Unchanged executable with identical current source; ordinary execution, no copy/rename/loader/policy changes.'}
    try:
        r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=110)
        (out / 'run.log').write_text(r.stdout + r.stderr)
        result.update(exit_code=r.returncode, status='PASS' if r.returncode == 0 else 'FAIL')
    except OSError as e:
        result.update(error=str(e), status='BLOCKED' if getattr(e, 'winerror', None) == 4551 else 'ERROR')
    (out / 'results.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result))
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
