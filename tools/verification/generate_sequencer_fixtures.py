#!/usr/bin/env python3
"""Independent struct/zlib wire oracle; evidence fixtures, never factory presets."""
import argparse
import hashlib
import json
import pathlib
import struct
import zlib


def bank(populated=False, musical=False):
    steps = bytearray()
    for p in range(8):
        for t in range(7):
            for s in range(128):
                if populated:
                    i = (p * 7 + t) * 128 + s
                    steps.extend((i % 256, i % 128, (i * 3) % 128,
                                  255 if i % 3 == 0 else i % 228,
                                  (i * 5) % 128, 255 if i % 5 == 0 else (i * 7) % 228,
                                  (i * 11) % 128))
                else:
                    steps.extend((228 if s % 8 == 0 else 100, 127, 63, 255, 0, 255, 0))
    masks = [((i * 1009) & 65535) if populated else 0 for i in range(56)]
    lengths = bytes(i % 16 if populated else 0 for i in range(56))
    rotations = bytes((i * 3) % 16 if populated else 0 for i in range(56))
    chain = bytes(x for p in range(8) for x in ((p * 2) % 15 if populated else p, p * 13 if populated else 0))
    if musical:
        for p in (0, 1):
            masks[p * 7] = 0x1111
            masks[p * 7 + 3] = 0x1010
            masks[p * 7 + 5] = 0x5555
            masks[p * 7 + 6] = 0xaaaa
            # A three-position substep figure and probability-controlled D2.
            masks[p * 7 + 1] = 0x0101
            for s in (3, 6):
                steps[((p * 7) * 128 + s) * 7] = 228
            steps[((p * 7 + 1) * 128 + 0) * 7 + 1] = 63
            # Two native selector slots: coarse1 and cutoff1.
            at = ((p * 7) * 128 + 32) * 7
            steps[at + 3:at + 7] = bytes((9, 48 + p * 12, 38, 90))
        chain = bytes((1, 0, 0, 0)) + chain[4:]
    payload = steps + b''.join(struct.pack('<H', m) for m in masks) + lengths + rotations + chain + bytes((87 if populated else 0,))
    assert len(payload) == 50417
    h = bytearray(struct.pack('<4sHHIIHHIHHI', b'LXQB', 1, 32, 50449, 50417, 8, 7, 0, 128, 7, 0))
    wire = h + payload
    struct.pack_into('<I', wire, 20, zlib.crc32(wire))
    return wire


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--output', type=pathlib.Path, required=True)
    a = ap.parse_args()
    a.output.mkdir(parents=True, exist_ok=False)
    manifest = {}
    for name, options in [('default', {}), ('dense', {'populated': True}), ('musical', {'musical': True})]:
        data = bank(**options)
        (a.output / (name + '.lxqb')).write_bytes(data)
        manifest[name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest().upper()}
    root = pathlib.Path(__file__).resolve().parents[2]
    old = bytearray((root / 'tests/reference/persistence/fixtures/v2_off.bin').read_bytes())
    assert len(old) == 516
    old[32:260] = bytes(i % 128 for i in range(228))
    struct.pack_into('<I', old, 24, 0)
    struct.pack_into('<I', old, 24, zlib.crc32(old))
    transport = struct.pack('<BBHBBHII', 1, 1, 60, 7, 0, 0, 0x12345678, 0)
    modules = b''.join(struct.pack('<IHHII', tag, 1, 1, len(body), 0) + body
                       for tag, body in ((1, old), (2, bank(populated=True)), (3, transport)))
    project = bytearray(struct.pack('<4sHHIIHHIII', b'LXRS', 3, 32, 32 + len(modules),
                                   len(modules), 3, 0, 0, 0, 0)) + modules
    assert len(project) == 51061
    struct.pack_into('<I', project, 24, zlib.crc32(project))
    project += struct.pack('<I', 1)
    (a.output / 'dense_project.lxrs').write_bytes(project)
    manifest['dense_project'] = {'bytes': len(project), 'sha256': hashlib.sha256(project).hexdigest().upper()}
    (a.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest))


if __name__ == '__main__':
    main()
