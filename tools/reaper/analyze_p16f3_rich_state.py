"""Independent complete-bank, nonzero-routing and matched MIDI PCM oracle."""
import hashlib
import argparse
import json
from pathlib import Path
import re
import struct
import zlib

from analyze_p16f3 import pcm, state

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'out/p16f3'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', type=Path, default=OUT / 'reaper-rich-state-034')
    parser.add_argument('--candidate', type=Path, default=OUT / 'private-build-006/candidate_manifest.json')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assert not args.output.exists(), 'Preserve existing evidence; choose a new output'
    d = args.directory
    result = {'candidate_sha256': json.loads(args.candidate.read_text())['sha256'],
              'host_status': (d / 'complete.txt').read_text(),
              'controller_checks': (d / 'rich_results.txt').read_text().splitlines(), 'instances': [], 'pcm': {}}
    assert result['host_status'].startswith('status=PASS')
    assert all(line.endswith('=PASS') for line in result['controller_checks'])
    for i in range(2):
        before_path = d / f'rich_before_{i}_chunk.txt'
        after_path = d / f'rich_after_{i}_chunk.txt'
        b, modules = state(before_path)
        a, _ = state(after_path)
        held, _ = state(d / f'rich_held_reopened_{i}_chunk.txt')
        assert b == a == held
        v2 = modules[1]
        tmp = bytearray(v2)
        crc = struct.unpack_from('<I', tmp, 24)[0]
        struct.pack_into('<I', tmp, 24, 0)
        assert zlib.crc32(tmp) == crc
        for index, value in [(0, 77-i*11), (8, 57+i*3), (9, 71-i*5), (37, 72), (43, 38)]:
            assert v2[32+index] == value
        routes = []
        for slot in range(12):
            fields = struct.unpack_from('<BBHHHHBBHBB', v2, 324+16*slot)
            assert fields[0] == slot % 6 and fields[1] == slot // 6 and fields[2] == fields[3] == 1
            routes.append({'owner': fields[0], 'source': fields[1], 'flags': fields[2], 'original_index': fields[4]})
        wrapper_before = int(re.search(r'(?m)^BYPASS (\d+)', before_path.read_text()).group(1))
        wrapper_after = int(re.search(r'(?m)^BYPASS (\d+)', after_path.read_text()).group(1))
        assert wrapper_before == wrapper_after == i
        result['instances'].append({'instance': i, 'status': 'PASS', 'all_state_bytes_identical': True,
            'canonical_state_bytes': len(b), 'positions': 7168,
            'bank_sha256': hashlib.sha256(modules[2]).hexdigest().upper(),
            'patch_sha256': hashlib.sha256(v2[32:260]).hexdigest().upper(), 'route_records': routes,
            'transport_hex': modules[3].hex(), 'plugin_bypass_suffix': struct.unpack('<I', b[-4:])[0],
            'REAPER_wrapper_bypass': wrapper_before})
    reference = None
    for name in ['rich_before', 'rich_after', 'rich_repeat']:
        raw, metrics = pcm(d / (name+'.wav'))
        assert metrics['nonzero']
        result['pcm'][name] = metrics
        if reference is None:
            reference = raw
        else:
            assert raw == reference
    result.update(status='PASS', same_callback_midi_PCM_identity=True,
        source='Actual REAPER245-controller readback; canonical bytes, independent inner v2 CRC, all12 enabled routes per instance, PCM payload equality. WAV metadata excluded.',
        bypass_distinction='REAPER TrackFX_SetEnabled preserves host wrapper BYPASS1 for the second instance; its internal plugin bypass suffix remains0, unchanged across restore. Native SDK state tests separately exercise a nonzero plugin bypass suffix.')
    args.output.write_text(json.dumps(result, indent=2))
    print(json.dumps({'status': 'PASS', 'host_checks': len(result['controller_checks']), 'complete_banks': 2,
                      'enabled_routes_each': 12, 'identical_MIDI_renders': 3}))


if __name__ == '__main__':
    main()
