#!/usr/bin/env python3
"""Documented VS private build, protected-install checks and source provenance."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
from datetime import datetime, timezone
from run_checkpoint_tests import visual_studio

ROOT = Path(__file__).resolve().parents[2]
EXPECTED = '89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82'
ASSETS = ['crash_sample.bin', 'rec_table.bin', 'saw_table.bin', 'sine_table.bin',
          'transient_data.bin', 'transient_volume_table.bin', 'tri_table.bin']


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def protected():
    module = Path(os.environ['LOCALAPPDATA']) / 'Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3'
    assert sha(module) == EXPECTED, 'Protected installed binary changed; STOP'
    entries = [{'path': str(module), 'bytes': module.stat().st_size, 'sha256': sha(module)}]
    for name in ASSETS:
        asset = module.parents[1] / 'Resources/LXR' / name
        assert sha(asset) == sha(ROOT / 'data' / name), 'Protected resource changed: ' + name
        entries.append({'path': str(asset), 'bytes': asset.stat().st_size, 'sha256': sha(asset)})
    return entries


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    allowed = ('out/p16f3', 'out/p16f4', 'out/p16f5', 'out/p16f6',
               'out/p17d', 'out/p17e', 'out/p17f', 'out/p18c', 'out/p19b',
               'out/p19e', 'out/p19f')
    assert any((ROOT / p).resolve() in out.parents for p in allowed), \
        'Private output must be under an approved milestone evidence directory'
    before = protected()
    out.mkdir(parents=True, exist_ok=False)
    override_properties = []
    source_override = None
    if (ROOT / 'out/p19f').resolve() in out.parents:
        # P19-F keeps the pinned iPlug2 checkout byte-for-byte intact. Build an
        # isolated derivative with exactly one asserted DPI type correction and
        # replace only that translation unit through a checked MSBuild override.
        pinned_igraphics = ROOT / 'vendor/iPlug2/IGraphics/Platforms/IGraphicsWin.cpp'
        old_dpi_line = '  int screenScale = GetScaleForHWND(mParentWnd);'
        new_dpi_line = '  float screenScale = GetScaleForHWND(mParentWnd);'
        pinned_text = pinned_igraphics.read_text(encoding='utf-8')
        assert pinned_text.count(old_dpi_line) == 1 and new_dpi_line not in pinned_text, \
            'Pinned IGraphicsWin.cpp no longer matches the reviewed P19-F correction'
        override_dir = out / 'source-overrides'
        override_dir.mkdir(parents=True)
        override_source = override_dir / 'IGraphicsWin.cpp'
        override_source.write_text(pinned_text.replace(old_dpi_line, new_dpi_line),
                                   encoding='utf-8')
        override_properties = [
            '/p:P19FIGraphicsWinSource=' + str(override_source),
            '/p:ForceImportAfterCppTargets=' + str(
                ROOT / 'tools/verification/p19f_iplug_override.targets'),
        ]
        source_override = {'kind': 'P19-F fractional DPI preservation',
                           'pinned_source_sha256': sha(pinned_igraphics),
                           'candidate_source': str(override_source),
                           'candidate_source_sha256': sha(override_source),
                           'changed_lines': 1}
    paths = sorted(set(subprocess.check_output(
        ['git', 'ls-files', '-c', '-o', '--exclude-standard'], cwd=ROOT, text=True).splitlines()))
    sources = [{'path': p, 'sha256': sha(ROOT / p)} for p in paths if (ROOT / p).is_file()]
    fingerprint = hashlib.sha256(''.join(x['path'] + '\0' + x['sha256'] + '\n' for x in sources).encode()).hexdigest().upper()
    (out / 'source_manifest.json').write_text(json.dumps({'fingerprint': fingerprint, 'files': sources}, indent=2) + '\n')
    vs = visual_studio()
    msbuild = vs / 'MSBuild/Current/Bin/MSBuild.exe'
    shell = ROOT / 'vendor/iPlug2/Examples/LXR'
    command = [str(msbuild), str(shell / 'projects/LXR-vst3.vcxproj'), '/t:Build',
               '/p:Configuration=Release', '/p:Platform=x64', '/p:PlatformToolset=v145',
               '/p:SolutionDir=' + str(shell) + '/', '/p:BUILD_DIR=' + str(out),
               '/p:OutDir=' + str(out / 'bin') + '/', '/p:IntDir=' + str(out / 'int') + '/',
               '/p:PDB_FILE=' + str(out / 'pdb/LXR.pdb'),
               '/p:BaseIntermediateOutputPath=' + str(out / 'base-int') + '/',
               '/p:MSBuildProjectExtensionsPath=' + str(out / 'extensions') + '/',
               *override_properties,
               '/p:PostBuildEventUseInBuild=false', '/p:PreBuildEventUseInBuild=false', '/v:minimal']
    (out / 'build_command.json').write_text(json.dumps(command, indent=2) + '\n')
    with (out / 'build.log').open('w', encoding='utf-8') as log:
        result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
    after = protected()
    assert before == after, 'Protected install differs after build; STOP'
    (out / 'protected_install.json').write_text(json.dumps({'before': before, 'after': after}, indent=2) + '\n')
    build_text = (out / 'build.log').read_text(encoding='utf-8', errors='replace')
    if 'MSB8027' in build_text or 'LNK4042' in build_text:
        print('BUILD FAIL: duplicate P19-F override translation unit')
        return 1
    if result.returncode:
        print('BUILD FAIL:', out / 'build.log')
        return result.returncode
    contents = out / 'bundle/LXR.vst3/Contents'
    (contents / 'x86_64-win').mkdir(parents=True)
    (contents / 'Resources/LXR').mkdir(parents=True)
    module = contents / 'x86_64-win/LXR.vst3'
    shutil.copy2(out / 'bin/LXR.vst3', module)
    resources = []
    for name in ASSETS:
        dest = contents / 'Resources/LXR' / name
        shutil.copy2(ROOT / 'data' / name, dest)
        assert sha(dest) == sha(ROOT / 'data' / name)
        resources.append({'name': name, 'bytes': dest.stat().st_size, 'sha256': sha(dest)})
    manifest = {'utc': datetime.now(timezone.utc).isoformat(), 'module': str(module),
                'bundle': str(contents.parent), 'sha256': sha(module), 'bytes': module.stat().st_size,
                'configuration': 'Release x64/v145', 'visual_studio': str(vs),
                'source_fingerprint': fingerprint, 'resources': resources, 'build_log': str(out / 'build.log'),
                'pdb': str(out / 'pdb/LXR.pdb'), 'pre_build_events': False, 'post_build_events': False,
                **({'source_override': source_override} if source_override else {}),
                'firmware': 'dee49689f3f0cdf8f9b7b5e79628cfdca1b79394',
                'iplug2': 'd54f69050f517e43b941d88c2a170f0a840b9ee4',
                'vst3sdk': '3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96'}
    (out / 'candidate_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
