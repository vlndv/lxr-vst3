#!/usr/bin/env python3
"""P17-A standalone source contracts; no plugin build, install or Git writes.

Compiles unchanged pinned presetManager.c and Cc2Text.c through bounded hardware
shims, plus exact menu_init/numtostrpu function slices (not edited algorithms).
Python struct fixtures are independent of the original writer and C++ reader.
"""
from __future__ import annotations
import argparse, hashlib, json, os, pathlib, re, struct, subprocess, sys
from datetime import datetime, timezone
ROOT=pathlib.Path(__file__).resolve().parents[2]
FW=ROOT/'ref/LXR-0.37'
AVR=FW/'front/LxrAvr'
REF=ROOT/'tests/reference/presets'

def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest().upper()
def clean(text):return re.sub(r'/\*.*?\*/|//[^\n]*','',text,flags=re.S)
def enums(path):
    text=path.read_text();body=clean(text).split('enum ParamEnums',1)[1].split('{',1)[1].split('}',1)[0]
    values={}; number=-1
    for item in body.split(','):
        item=item.strip()
        if not item:continue
        if '=' in item:
            name,expr=map(str.strip,item.split('=',1));number=values[expr] if expr in values else int(expr,0)
        else:name=item;number+=1
        values[name]=number
    return values
def function_slice(path,name):
    text=path.read_text();m=re.search(r'^void '+name+r'\([^\n]*\)\s*\{',text,re.M)
    if not m:raise ValueError(name)
    at=text.index('{',m.start());depth=1;end=at+1
    # These two selected functions contain no braces in literals/comments.
    while depth:
        if text[end]=='{':depth+=1
        elif text[end]=='}':depth-=1
        end+=1
    body=text[m.start():end]
    return body,dict(source=str(path.relative_to(ROOT)),function=name,start_line=text[:m.start()].count('\n')+1,end_line=text[:end].count('\n')+1,normalized_slice_sha256=hashlib.sha256(body.encode()).hexdigest().upper())
def fixtures(folder):
    folder.mkdir();params=bytes((i*11+3)%128 for i in range(273));name=b'AUDIT037'
    steps=bytes(b for i in range(7168) for b in ((i*17+128)&255,i%128,(i*3)%128,i%228,(i+7)%128,(i+11)%228,(i+13)%128))
    masks=b''.join(struct.pack('<H',(0x1234+i*257)&65535) for i in range(56))
    chain=bytes(b for i in range(8) for b in ((i+1)%8,i+2))
    body=steps+masks+chain+params[242:243]+bytes(i%16 for i in range(56))
    assert len(body)==50361
    files={'sound.snd':name+params[:228],'pattern.pat':name+body,
           'all.all':name+b'\x02'+params[251:273]+b'\xff'*42+params[:228]+b'\xff'*284+body,
           'performance.prf':name+b'\x02'+params[251:252]+params[271:272]+b'\xff'*62+params[:228]+b'\xff'*284+body,
           'globals.cfg':params[251:273],'parameters.bin':params}
    assert [len(files[n]) for n in ['sound.snd','pattern.pat','all.all','performance.prf','globals.cfg']]==[236,50369,50946,50946,22]
    for n,b in files.items():(folder/n).write_bytes(b)
    return [dict(file=n,bytes=len(b),sha256=sha(folder/n),provenance='Independent Python explicit-field synthetic source-contract fixture, not factory/hardware content') for n,b in files.items()]
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',required=True,type=pathlib.Path)
    ap.add_argument('--cc',default='C:/msys64/ucrt64/bin/gcc.exe');ap.add_argument('--cxx',default='C:/msys64/ucrt64/bin/g++.exe')
    ap.add_argument('--compiler',choices=['gcc','vs-llvm'],default='gcc',help='Approved native compiler environments; no trust/policy changes')
    a=ap.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    values=enums(AVR/'Parameters.h');assert values['END_OF_SOUND_PARAMETERS']==228 and values['PAR_BEGINNING_OF_GLOBALS']==251 and values['NUM_PARAMS']==273
    first={}
    for name,value in values.items():
        if value<228:first.setdefault(value,name)
    assert set(first)==set(range(228))
    enumlist=out/'original_enum_list.inc';enumlist.write_text(',\n'.join(first[i] for i in range(228))+'\n')
    fragments=[];receipts=[]
    for name in ['menu_init','numtostrpu']:
        text,receipt=function_slice(AVR/'Menu/menu.c',name);fragments.append(text);receipts.append(receipt)
    fragment=out/'original_menu_functions.inc';fragment.write_text('\n\n'.join(fragments)+'\n')
    golden=fixtures(out/'fixtures')
    sources=[REF/'original_preset_adapter.c',REF/'original_preset_adapter.h',ROOT/'tests/P17APresetKitContractTest.cpp',AVR/'Preset/presetManager.c',AVR/'Preset/SeqStep.h',AVR/'Preset/PresetManager.h',AVR/'Menu/menu.h',AVR/'Menu/menu.c',AVR/'Menu/Cc2Text.c',AVR/'Menu/CcNr2Text.h',AVR/'Parameters.h',AVR/'frontPanelParser.h',AVR/'config.h',pathlib.Path(__file__).resolve()]
    sources+=list((REF/'compat').rglob('*.h'))
    manifest=[dict(path=p.relative_to(ROOT).as_posix(),sha256=sha(p)) for p in sources]
    (out/'source_manifest.json').write_text(json.dumps(dict(files=manifest,unchanged_original_slices=receipts,fixture_manifest=golden,scope='Host boundary shims; no AVR hardware/ABI or genuine factory-file claim'),indent=2))
    env=os.environ.copy();env['PATH']=str(pathlib.Path(a.cc).parent)+';'+env['PATH']
    clang=None
    if a.compiler=='vs-llvm':
        from run_checkpoint_tests import msvc_environment,visual_studio
        vs=visual_studio();env=msvc_environment(vs,'14.51');clang=vs/'VC/Tools/Llvm/x64/bin/clang-cl.exe'
    commands=[];runs=[]
    def command(label,args):
        row=dict(label=label,command=list(map(str,args)))
        try:
            p=subprocess.run(row['command'],cwd=ROOT,env=env,capture_output=True,text=True,timeout=60)
            row.update(exit_code=p.returncode,status='PASS' if p.returncode==0 else 'FAIL',stdout=p.stdout,stderr=p.stderr)
        except OSError as e:row.update(status='BLOCKED' if getattr(e,'winerror',None)==4551 else 'ERROR',error=str(e),assertions_completed=0)
        except subprocess.TimeoutExpired as e:row.update(status='BLOCKED',error=str(e))
        commands.append(row);(out/'commands.json').write_text(json.dumps(commands,indent=2));print(label,row['status'],flush=True);return row
    defines={'P17_ORIGINAL_MENU_HEADER':AVR/'Menu/menu.h','P17_ORIGINAL_FRONT_HEADER':AVR/'frontPanelParser.h','P17_ORIGINAL_PRESET_SOURCE':AVR/'Preset/presetManager.c','P17_ORIGINAL_MENU_FRAGMENT':fragment,'P17_ORIGINAL_ENUM_LIST':enumlist}
    for opt in ['O0','O2']:
        build=out/opt;build.mkdir();common=['-'+opt,'-Wall','-Wextra','-I'+str(REF/'compat')]
        flags=['-D'+k+'="'+p.as_posix()+'"' for k,p in defines.items()]
        stages=[]
        original_object=build/('original.obj' if clang else 'original.o')
        targets_object=build/('targets.obj' if clang else 'targets.o')
        if clang:
            common=['/nologo','/Od' if opt=='O0' else '/O2','/MD','/I'+str(REF/'compat')]
            flags=['/D'+k+'="'+p.as_posix()+'"' for k,p in defines.items()]
            cflags=['/TC','/clang:-std=gnu11','/clang:-fgnu89-inline']
            stages.append(command(opt+'-compile-original-adapter',[clang,*common,*cflags,*flags,'/c',REF/'original_preset_adapter.c','/Fo'+str(original_object)]))
            stages.append(command(opt+'-compile-original-modtargets',[clang,*common,*cflags,'/c',AVR/'Menu/Cc2Text.c','/Fo'+str(targets_object)]))
        else:
            stages.append(command(opt+'-compile-original-adapter',[a.cc,'-std=gnu11',*common,*flags,'-c',REF/'original_preset_adapter.c','-o',original_object]))
            stages.append(command(opt+'-compile-original-modtargets',[a.cc,'-std=gnu11',*common,'-c',AVR/'Menu/Cc2Text.c','-o',targets_object]))
        exe=build/'P17APresetKitContractTest.exe'
        if all(x['status']=='PASS' for x in stages):
            cmd=[clang,*common,'/std:c++17','/GR-','/EHs-c-','/c',ROOT/'tests/P17APresetKitContractTest.cpp','/Fo'+str(build/'test.obj')] if clang else [a.cxx,'-std=c++17','-fno-exceptions','-fno-rtti',*common,ROOT/'tests/P17APresetKitContractTest.cpp',original_object,targets_object,'-o',exe]
            stages.append(command(opt+'-compile-test',cmd))
            if clang and stages[-1]['status']=='PASS':
                stages.append(command(opt+'-link-test',[clang,build/'test.obj',original_object,targets_object,'/Fe'+str(exe)]))
        if all(x['status']=='PASS' for x in stages):
            run=command(opt+'-execute',[exe,out/'fixtures']);run['executable_sha256']=sha(exe)
            if run['status']=='PASS':
                try:run['metrics']=json.loads(run['stdout'].strip())
                except json.JSONDecodeError:run['status']='FAIL'
            runs.append(dict(optimization=opt,**run))
        else:runs.append(dict(optimization=opt,status='BLOCKED',reason='Build did not complete; see commands.json'))
    assert all(sha(ROOT/x['path'])==x['sha256'] for x in manifest),'Source changed during reference execution'
    result=dict(utc=datetime.now(timezone.utc).isoformat(),compiler=a.compiler,firmware='dee49689f3f0cdf8f9b7b5e79628cfdca1b79394',status='PASS' if len(runs)==2 and all(x['status']=='PASS' for x in runs) else 'FAIL' if any(x['status']=='FAIL' for x in runs) else 'BLOCKED',runs=runs,source_manifest='source_manifest.json',limits=['Host structure layout is not exact AVR/ARM ABI evidence','Original global/all readers have an uninitialized bytesRead condition and are not executed','Fixtures are synthetic; no genuine factory SD pack acquired','Strict standalone parser tests are an audit policy, not production format support'])
    (out/'contract_test_results.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2));return 0 if result['status']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
