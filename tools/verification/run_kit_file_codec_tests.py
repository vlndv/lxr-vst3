#!/usr/bin/env python3
"""P17-C .snd codec and non-RT Windows I/O; approved standalone toolchains.

Compiles unchanged original firmware C. Generates independent explicit-byte
synthetic goldens. Refuses output reuse. No plugin build/install/Git changes.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
from run_preset_kit_contract_tests import ROOT, AVR, REF, enums, function_slice, sha
from extract_kit_contract import generate


def fixtures(folder):
    folder.mkdir()
    fields, _, _ = generate()
    examples = {}
    examples['default'] = (bytes(8), bytearray(228))
    drums = bytearray(228)
    for i,v in {1:2,8:42,9:64,37:100,49:3,50:87,88:101,149:2,161:1}.items(): drums[i]=v
    examples['drums'] = (b'P17CDRUM',drums)
    snare = bytearray(228)
    for i,v in {4:4,27:127,28:99,55:0,56:32,68:7,91:110,152:104,164:4,170:104}.items(): snare[i]=v
    examples['snare'] = (b'P17CSNAR',snare)
    hats = bytearray(228)
    for i,v in {7:3,42:80,59:0,60:10,61:122,93:109,154:171,166:6,172:203,226:42,227:46}.items(): hats[i]=v
    examples['hats'] = (b'P17CHATS',hats)
    boundary = bytearray(228)
    for i,v in {149:127,150:128,151:204,152:203,167:128,168:127,169:204}.items(): boundary[i]=v
    examples['selector-boundary'] = (b'IDX12780',boundary)
    populated = bytearray(f['maximum'] if f['kind']!='OscillatorWave' else 5 for f in fields)
    for source in (149,167):
        for owner,index in enumerate((2,36,70,104,137,171)): populated[source+owner]=index
    examples['populated'] = (bytes((0,255,65,0,66,127,128,32)),populated)
    manifest=[]
    for name,(label,values) in examples.items():
        data=label+values;assert len(data)==236
        path=folder/(name+'.snd');path.write_bytes(data)
        manifest.append(dict(name=name,file=path.name,bytes=236,sha256=sha(path),name_bytes=list(label),values=list(values),
            raw_decode='EXACT',canonical_admission='UnsupportedRoute' if name=='selector-boundary' else 'Ok',
            expected_canonical_values=list(values),provenance='Synthetic explicit Python name8+228 byte fixture; source-defined IDs/domains; not factory content'))
    return manifest


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--compiler',choices=['gcc','vs-llvm'],default='gcc')
    ap.add_argument('--sanitizer',choices=['none','address','undefined'],default='none')
    ap.add_argument('--cc',default='C:/msys64/ucrt64/bin/gcc.exe')
    ap.add_argument('--cxx',default='C:/msys64/ucrt64/bin/g++.exe')
    a=ap.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    golden=fixtures(out/'fixtures');(out/'golden_fixture_manifest.json').write_text(json.dumps(golden,indent=2))
    first={}
    for name,value in enums(AVR/'Parameters.h').items():
        if value<228:first.setdefault(value,name)
    enumlist=out/'original_enum_list.inc';enumlist.write_text(',\n'.join(first[i] for i in range(228))+'\n')
    fragments=[];receipts=[]
    for name in ['menu_init','numtostrpu']:
        body,receipt=function_slice(AVR/'Menu/menu.c',name);fragments.append(body);receipts.append(receipt)
    fragment=out/'original_menu_functions.inc';fragment.write_text('\n\n'.join(fragments)+'\n')
    # Include every direct/transitive repository test/contract header dependency.
    sources=[ROOT/'dsp'/n for n in ['KitTypes.h','KitRecords.inc','KitFields.inc','KitContract.h','KitContract.cpp',
        'KitFileCodec.h','KitFileCodec.cpp','KitFileIO.h','KitFileIO.cpp','KitFileIOWin32.cpp','ModulationContract.h',
        'ModulationContractData.inc','PhysicalModulation.h','VelocityRouting.h','RoutingStateAuthority.h','RouteHostControls.h']]
    sources += [ROOT/'interfaces/KitFileCodec.h',ROOT/'interfaces/KitFileIO.h',ROOT/'tests/P17CKitFileCodecTest.cpp',
                Path(__file__).resolve(),ROOT/'tools/verification/run_preset_kit_contract_tests.py',ROOT/'tools/verification/extract_kit_contract.py',ROOT/'tools/verification/extract_modulation_contract.py']
    sources += [p for p in REF.rglob('*') if p.is_file()]
    sources += [AVR/p for p in ['Preset/presetManager.c','Preset/PresetManager.h','Preset/SeqStep.h','Menu/menu.h',
                               'Menu/Cc2Text.c','Menu/CcNr2Text.h','Menu/menu.c','Parameters.h','frontPanelParser.h','config.h']]
    manifest=[dict(path=p.relative_to(ROOT).as_posix(),sha256=sha(p)) for p in sorted(set(sources))]
    (out/'source_manifest.json').write_text(json.dumps(dict(files=manifest,unchanged_original_slices=receipts,firmware='dee49689f3f0cdf8f9b7b5e79628cfdca1b79394'),indent=2))
    env=os.environ.copy();env['PATH']=str(Path(a.cc).parent)+os.pathsep+env['PATH'];clang=None
    if a.compiler=='vs-llvm':
        from run_checkpoint_tests import msvc_environment,visual_studio
        vs=visual_studio();env=msvc_environment(vs,'14.51');clang=vs/'VC/Tools/Llvm/x64/bin/clang-cl.exe'
        if a.sanitizer=='address':
            locations=list((vs/'VC/Tools/Llvm/x64/lib/clang').glob('*/lib/windows'))
            env['PATH']=os.pathsep.join(str(p) for p in locations)+os.pathsep+env['PATH']
    commands=[];runs=[]
    def command(label,args):
        row=dict(label=label,command=list(map(str,args)),cwd=str(ROOT));begin=time.monotonic()
        try:
            p=subprocess.run(row['command'],cwd=ROOT,env=env,capture_output=True,text=True,timeout=55)
            row.update(status='PASS' if p.returncode==0 else 'FAIL',exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr)
        except OSError as e:row.update(status='BLOCKED' if getattr(e,'winerror',None)==4551 else 'ERROR',error=str(e),assertions_completed=0)
        except subprocess.TimeoutExpired as e:row.update(status='BLOCKED',error=str(e))
        row['elapsed_seconds']=time.monotonic()-begin;commands.append(row)
        (out/'commands.json').write_text(json.dumps(commands,indent=2));print(label,row['status'],flush=True);return row
    defines={'P17_ORIGINAL_MENU_HEADER':AVR/'Menu/menu.h','P17_ORIGINAL_FRONT_HEADER':AVR/'frontPanelParser.h',
             'P17_ORIGINAL_PRESET_SOURCE':AVR/'Preset/presetManager.c','P17_ORIGINAL_MENU_FRAGMENT':fragment,'P17_ORIGINAL_ENUM_LIST':enumlist}
    for opt in ['O0','O2']:
        build=out/opt;build.mkdir();io=build/'filesystem';io.mkdir();objects=[];stages=[]
        # Bundled Windows UBSan uses the static MSVC runtime. ASan and ordinary
        # LLVM tests use the dynamic runtime; pass the same choice to the link
        # driver so it never selects an incompatible sanitizer thunk/CRT.
        crt='/MT' if a.sanitizer=='undefined' else '/MD'
        common=['/nologo','/Od' if opt=='O0' else '/O2',crt,'/D_WIN32_WINNT=0x0601','/I'+str(REF/'compat')] if clang else ['-'+opt,'-Wall','-Wextra','-ffp-contract=off','-D_WIN32_WINNT=0x0601','-I'+str(REF/'compat')]
        if a.sanitizer!='none':
            common += ['/fsanitize=address','/Zi'] if clang and a.sanitizer=='address' else ['/clang:-fsanitize=undefined','/clang:-fno-sanitize-recover=all'] if clang else ['-fsanitize='+a.sanitizer,'-fno-sanitize-recover=all','-g']
        flags=[('/D' if clang else '-D')+key+'="'+p.as_posix()+'"' for key,p in defines.items()]
        for name,source in [('original',REF/'original_preset_adapter.c'),('targets',AVR/'Menu/Cc2Text.c')]:
            obj=build/(name+('.obj' if clang else '.o'));objects.append(obj)
            args=[clang,*common,'/TC','/clang:-std=gnu11','/clang:-fgnu89-inline',*flags,'/c',source,'/Fo'+str(obj)] if clang else [a.cc,'-std=gnu11','-fgnu89-inline',*common,*flags,'-c',source,'-o',obj]
            stages.append(command(opt+'-compile-'+name,args))
        cpp=[ROOT/'tests/P17CKitFileCodecTest.cpp',*[ROOT/'dsp'/n for n in ['KitContract.cpp','KitFileCodec.cpp','KitFileIO.cpp','KitFileIOWin32.cpp']]]
        exe=build/'P17CKitFileCodecTest.exe'
        if all(s['status']=='PASS' for s in stages):
            if clang:
                for i,source in enumerate(cpp):
                    obj=build/('cpp'+str(i)+'.obj');objects.append(obj)
                    stages.append(command(opt+'-compile-cpp'+str(i),[clang,*common,'/std:c++17','/GR-','/EHs-c-','/fp:strict','/c',source,'/Fo'+str(obj)]))
                if all(s['status']=='PASS' for s in stages):
                    linkflags=['/fsanitize=address','/Zi'] if a.sanitizer=='address' else ['/clang:-fsanitize=undefined'] if a.sanitizer=='undefined' else []
                    stages.append(command(opt+'-link',[clang,crt,*linkflags,*objects,'/Fe'+str(exe)]))
            else:stages.append(command(opt+'-link',[a.cxx,'-std=c++17','-fno-exceptions','-fno-rtti','-pthread',*common,*cpp,*objects,'-o',exe]))
        if all(s['status']=='PASS' for s in stages):
            run=command(opt+'-execute',[exe,out/'fixtures',io]);run['executable_sha256']=sha(exe)
            if run['status']=='PASS':
                try:
                    run['metrics']=json.loads(run['stdout'].strip());assert run['metrics']['fail']==0 and run['metrics']['pure_allocations']==0
                except (ValueError,AssertionError):run['status']='FAIL'
            runs.append(dict(optimization=opt,**run))
        else:runs.append(dict(optimization=opt,status='BUILD_FAILED'))
    assert all(sha(ROOT/x['path'])==x['sha256'] for x in manifest),'Source changed during verification'
    result=dict(compiler=a.compiler,sanitizer=a.sanitizer,runs=runs,status='PASS' if all(x['status']=='PASS' for x in runs) else 'FAIL' if any(x['status'] in ['FAIL','BUILD_FAILED','ERROR'] for x in runs) else 'BLOCKED',source_manifest='source_manifest.json',golden_fixture_manifest='golden_fixture_manifest.json')
    (out/'codec_test_results.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2));return 0 if result['status']=='PASS' else 1


if __name__=='__main__':raise SystemExit(main())
