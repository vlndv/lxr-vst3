#!/usr/bin/env python3
"""P17-B standalone C++17 + independent unchanged original C at O0/O2.

Approved GCC UCRT64 and VS18 LLVM environments; no plugin build/deploy, file
codec, security changes, policy retries, or live Engine/state publication.
"""
import argparse
import hashlib
import json
import os
import pathlib
import subprocess
import time
from datetime import datetime, timezone
from run_preset_kit_contract_tests import enums, function_slice, sha, ROOT, AVR, REF
from extract_kit_contract import generate


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output', type=pathlib.Path, required=True)
    ap.add_argument('--compiler', choices=['gcc', 'vs-llvm'], default='gcc')
    ap.add_argument('--cc', default='C:/msys64/ucrt64/bin/gcc.exe')
    ap.add_argument('--cxx', default='C:/msys64/ucrt64/bin/g++.exe')
    a = ap.parse_args()
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    fields, records, mapping = generate()
    for path, expected in [('dsp/KitRecords.inc', records), ('dsp/KitFields.inc', mapping)]:
        assert (ROOT / path).read_text(encoding='utf-8') == expected, path + ' drift'
    values = enums(AVR / 'Parameters.h'); first = {}
    for name, number in values.items():
        if number < 228:
            first.setdefault(number, name)
    enumlist = out / 'original_enum_list.inc'
    enumlist.write_text(',\n'.join(first[i] for i in range(228))+'\n')
    fragments, receipts = [], []
    for name in ['menu_init', 'numtostrpu']:
        fragment, receipt = function_slice(AVR / 'Menu/menu.c', name)
        fragments.append(fragment); receipts.append(receipt)
    fragment = out / 'original_menu_functions.inc'
    fragment.write_text('\n\n'.join(fragments)+'\n')
    menu = (AVR / 'Menu/menu.c').read_text()
    start = menu.index('const enum Datatypes PROGMEM parameter_dtypes[NUM_PARAMS] = {')
    end = menu.index('};', start)+2
    dtypes = out / 'original_kit_dtypes.inc'
    dtypes.write_text(menu[start:end]+'\n')
    receipts.append(dict(source='ref/LXR-0.37/front/LxrAvr/Menu/menu.c',
                         start_line=menu[:start].count('\n')+1, end_line=menu[:end].count('\n')+1,
                         normalized_slice_sha256=hashlib.sha256(menu[start:end].encode()).hexdigest().upper()))
    sources = [ROOT/'dsp/KitTypes.h', ROOT/'dsp/KitContract.h', ROOT/'dsp/KitContract.cpp',
               ROOT/'dsp/KitRecords.inc', ROOT/'dsp/KitFields.inc', ROOT/'interfaces/KitContract.h',
               ROOT/'tests/P17BKitContractTest.cpp', ROOT/'dsp/ModulationContract.h',
               ROOT/'dsp/ModulationContractData.inc', ROOT/'dsp/VelocityRouting.h', ROOT/'dsp/PhysicalModulation.h',
               ROOT/'dsp/RoutingStateAuthority.h', ROOT/'dsp/RouteHostControls.h', ROOT/'dsp/ParameterStateAuthority.h',
               AVR/'Preset/presetManager.c', AVR/'Preset/SeqStep.h', AVR/'Preset/PresetManager.h',
               AVR/'Menu/menu.h', AVR/'Menu/menu.c', AVR/'Menu/MenuText.h', AVR/'Menu/Cc2Text.c',
               AVR/'Menu/CcNr2Text.h', AVR/'Parameters.h', AVR/'frontPanelParser.h', AVR/'config.h',
               pathlib.Path(__file__).resolve(), ROOT/'tools/verification/extract_kit_contract.py',
               ROOT/'tools/verification/extract_modulation_contract.py', ROOT/'tools/verification/run_preset_kit_contract_tests.py']
    sources += [p for p in REF.rglob('*') if p.is_file()]
    manifest = [dict(path=p.relative_to(ROOT).as_posix(), sha256=sha(p)) for p in sources]
    (out/'source_manifest.json').write_text(json.dumps(dict(files=manifest, unchanged_original_slices=receipts,
        scope='Original AVR writer, sender, initialization and compiled menu domains; not target ABI/hardware/factory assets'), indent=2))
    env = os.environ.copy(); env['PATH'] = str(pathlib.Path(a.cc).parent)+os.pathsep+env['PATH']
    clang = None
    if a.compiler == 'vs-llvm':
        from run_checkpoint_tests import msvc_environment, visual_studio
        vs = visual_studio(); env = msvc_environment(vs, '14.51'); clang = vs/'VC/Tools/Llvm/x64/bin/clang-cl.exe'
    commands, runs = [], []
    def command(label, args):
        row = dict(label=label, command=list(map(str,args)), cwd=str(ROOT)); begin=time.monotonic()
        try:
            p = subprocess.run(row['command'], cwd=ROOT, env=env, capture_output=True, text=True, timeout=55)
            row.update(status='PASS' if p.returncode==0 else 'FAIL', exit_code=p.returncode, stdout=p.stdout, stderr=p.stderr)
        except OSError as error:
            row.update(status='BLOCKED' if getattr(error,'winerror',None)==4551 else 'ERROR', error=str(error), assertions_completed=0)
        except subprocess.TimeoutExpired as error:
            row.update(status='BLOCKED', error=str(error))
        row['elapsed_seconds']=time.monotonic()-begin; commands.append(row)
        (out/'commands.json').write_text(json.dumps(commands,indent=2)); print(label,row['status'],flush=True)
        return row
    defines = {'P17_ORIGINAL_MENU_HEADER':AVR/'Menu/menu.h', 'P17_ORIGINAL_FRONT_HEADER':AVR/'frontPanelParser.h',
               'P17_ORIGINAL_PRESET_SOURCE':AVR/'Preset/presetManager.c', 'P17_ORIGINAL_MENU_FRAGMENT':fragment,
               'P17_ORIGINAL_ENUM_LIST':enumlist, 'P17_ORIGINAL_MENU_TEXT':AVR/'Menu/MenuText.h',
               'P17_ORIGINAL_KIT_DTYPES':dtypes}
    for opt in ['O0','O2']:
        build=out/opt;build.mkdir(); objects=[];stages=[]
        common=['/nologo','/Od' if opt=='O0' else '/O2','/MD','/I'+str(REF/'compat')] if clang else ['-'+opt,'-Wall','-Wextra','-ffp-contract=off','-I'+str(REF/'compat')]
        flags=[('/D' if clang else '-D')+key+'="'+path.as_posix()+'"' for key,path in defines.items()]
        for name, source in [('original',REF/'original_kit_contract_adapter.c'),('targets',AVR/'Menu/Cc2Text.c')]:
            obj=build/(name+('.obj' if clang else '.o'));objects.append(obj)
            args=[clang,*common,'/TC','/clang:-std=gnu11','/clang:-fgnu89-inline',*flags,'/c',source,'/Fo'+str(obj)] if clang else [a.cc,'-std=gnu11','-fgnu89-inline',*common,*flags,'-c',source,'-o',obj]
            stages.append(command(opt+'-compile-'+name,args))
        exe=build/'P17BKitContractTest.exe'
        if all(s['status']=='PASS' for s in stages):
            cpp=[ROOT/'tests/P17BKitContractTest.cpp',ROOT/'dsp/KitContract.cpp']
            if clang:
                for i,source in enumerate(cpp):
                    obj=build/('cpp'+str(i)+'.obj');objects.append(obj)
                    stages.append(command(opt+'-compile-cpp'+str(i),[clang,*common,'/std:c++17','/GR-','/EHs-c-','/fp:strict','/c',source,'/Fo'+str(obj)]))
                if all(s['status']=='PASS' for s in stages): stages.append(command(opt+'-link',[clang,*objects,'/Fe'+str(exe)]))
            else:
                stages.append(command(opt+'-link',[a.cxx,'-std=c++17','-fno-exceptions','-fno-rtti',*common,*cpp,*objects,'-o',exe]))
        if all(s['status']=='PASS' for s in stages):
            run=command(opt+'-execute',[exe]);run['executable_sha256']=sha(exe)
            if run['status']=='PASS':
                try:
                    run['metrics']=json.loads(run['stdout'].strip())
                    assert run['metrics']['fail']==0 and run['metrics']['conversion_allocations']==0
                except (ValueError,AssertionError): run['status']='FAIL'
            runs.append(dict(optimization=opt,**run))
        else: runs.append(dict(optimization=opt,status='BUILD_FAILED'))
    assert all(sha(ROOT/row['path'])==row['sha256'] for row in manifest),'Inputs changed during execution'
    result=dict(utc=datetime.now(timezone.utc).isoformat(),compiler=a.compiler,runs=runs,
                status='PASS' if all(r['status']=='PASS' for r in runs) else 'FAIL' if any(r['status'] in ['FAIL','BUILD_FAILED','ERROR'] for r in runs) else 'BLOCKED',
                original_c_field_value_writer_comparisons=sum(r['maximum']+1 for r in fields),
                source_manifest='source_manifest.json',limits=['No target AVR/ARM ABI or live audio claim','External sample availability unknown; retained, native candidate unsupported','No actual file I/O or host publication'])
    (out/'contract_test_results.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
    return 0 if result['status']=='PASS' else 1


if __name__=='__main__':
    raise SystemExit(main())
