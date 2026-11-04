"""P15-M2E original-C runtime differential. New output only; no production writes or policy bypass.

Compile unchanged lfo.c/modulationNode.c/voices/CC handlers through the existing
M2B compatibility generator. Extract COMPLETE mixer functions verbatim. Only
GPIO jack availability and trigger IO are replaced by disclosed host services.
"""
from pathlib import Path
import argparse, csv, hashlib, json, os, re, shutil, struct, subprocess, sys, time

R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',required=True);p.add_argument('--cc');p.add_argument('--cxx')
a=p.parse_args();O=Path(a.output).resolve();O.mkdir(parents=True,exist_ok=False)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
F=R/'ref/LXR-0.37'
assert subprocess.check_output(['git','-C',str(F),'rev-parse','HEAD']).decode().strip()=='dee49689f3f0cdf8f9b7b5e79628cfdca1b79394'
assert not subprocess.check_output(['git','-C',str(F),'status','--porcelain'])
cc=shutil.which(a.cc or os.getenv('CC','gcc'));cxx=shutil.which(a.cxx or os.getenv('CXX','g++'))
assert cc and cxx,'Use documented GCC/G++ compiler setup'
env=os.environ.copy();env['PATH']=str(Path(cxx).parent)+os.pathsep+env.get('PATH','')
commands=[];results=[]
def run(name,cmd,cwd=R,test=False):
    start=time.monotonic()
    try:
        r=subprocess.run(list(map(str,cmd)),cwd=cwd,env=env,capture_output=True,timeout=180)
        code=r.returncode;log=(r.stdout+r.stderr).decode(errors='replace');status='PASS' if code==0 else 'FAIL'
    except OSError as e:
        code=None;log=str(e);status='BLOCKED' if getattr(e,'winerror',0) in [5,577,1260,4551] else 'ERROR'
    row=dict(name=name,command=list(map(str,cmd)),cwd=str(cwd),exit_code=code,status=status,
             elapsed_seconds=time.monotonic()-start,log=name+'.log')
    (O/row['log']).write_text(log);commands.append(row)
    if test:row['executable_sha256']=sha(Path(cmd[0]));results.append(row)
    (O/'commands.json').write_text(json.dumps(commands,indent=2))
    (O/'test_results.json').write_text(json.dumps(results,indent=2))
    print(name,status,log[-240:] if test or code else '',flush=True)
    if not test and status!='PASS':raise RuntimeError('Compilation unavailable; see '+str(O/row['log']))

S=O/'source'
for d in ['dsp','tests','interfaces','data']:
    shutil.copytree(R/d,S/d,ignore=shutil.ignore_patterns('*.exe','*.o','__pycache__'))
for d in ['mainboard','front']:shutil.copytree(F/d,S/'ref/LXR-0.37'/d)
V=S/'vendor/iPlug2/Examples/LXR';V.mkdir(parents=True)
shutil.copy2(R/'vendor/iPlug2/Examples/LXR/LXRState.h',V/'LXRState.h')
run('generate',[sys.executable,R/'tests/reference/base_mapping/generate_reference.py','--root',S,'--output',O/'reference'])
ref=O/'reference';src=S/'ref/LXR-0.37/mainboard/LxrStm32/src';extracts=[]
def extract(rel,name):
    path=src/rel;s=path.read_text();m=re.search(r'^[^\n;{}]*\b'+name+r'\([^;{}]*\)\s*\{',s,re.M);assert m,name
    end=s.index('{',m.start())+1;depth=1
    while depth:
        if s[end]=='{':depth+=1
        if s[end]=='}':depth-=1
        end+=1
    body=s[m.start():end]
    extracts.append(dict(path='ref/LXR-0.37/mainboard/LxrStm32/src/'+rel,symbol=name,
        first_line=s[:m.start()].count('\n')+1,last_line=s[:end].count('\n')+1,
        sha256=hashlib.sha256(body.encode()).hexdigest().upper(),body=body))
    return body
extra='''
#define USE_DECIMATOR 1
#define MIXER_ROUTING_DAC1_STEREO 0
#define MIXER_ROUTING_DAC2_STEREO 1
#define MIXER_ROUTING_DAC1_L 2
#define MIXER_ROUTING_DAC1_R 3
#define MIXER_ROUTING_DAC2_L 4
#define MIXER_ROUTING_DAC2_R 5
float mixer_decimation_cnt[6];
int16_t mixer_voice_samples[6];
static uint8_t mixer_checkOutJackAvailable(uint8_t d){return d;}
static void trigger_tickPhaseCounter(void){}
'''
extra+='\n#include "'+(src/'DSPAudio/squareRootLut.c').as_posix()+'"\n'
for rel,name in [('DSPAudio/BufferTools.c','bufferTool_clearBuffer'),
                 ('DSPAudio/mixer.c','mixer_init'),('DSPAudio/mixer.c','mixer_decimateBlock'),
                 ('DSPAudio/mixer.c','mixer_addDataToOutput'),('DSPAudio/mixer.c','mixer_calcNextSampleBlock')]:
    extra+='\n'+extract(rel,name)+'\n'
extra+='\n#include "'+(S/'tests/reference/lfo/original_lfo_adapter.c').as_posix()+'"\n'
extra+='\n#include \"'+(S/'tests/reference/velocity/original_velocity_adapter.c').as_posix()+'\"\n'
extra+='\n#include \"'+(S/'tests/reference/lfo_runtime/original_lfo_runtime_adapter.c').as_posix()+'\"\n'
with (ref/'oracle.c').open('a') as f:f.write(extra)
(ref/'mixer_extractions.json').write_text(json.dumps(extracts,indent=2))
# Verify the loaded sine binary is exactly the original integer initializer.
wt=(src/'DSPAudio/wavetable.c').read_text();m=re.search(r'sine_table\s*\[[^]]*\]\s*=\s*\{(.*?)\}',wt,re.S);assert m
values=[int(x) for x in re.findall(r'-?\d+',re.sub(r'/\*.*?\*/|//[^\n]*','',m[1],flags=re.S))]
assert struct.pack('<'+'h'*len(values),*values)==(S/'data/sine_table.bin').read_bytes()
(O/'sine_provenance.json').write_text(json.dumps(dict(entries=len(values),source_sha256=sha(src/'DSPAudio/wavetable.c'),binary_sha256=sha(S/'data/sine_table.bin')),indent=2))
run('gcc-version',[cc,'--version']);run('gxx-version',[cxx,'--version'])
manifest=json.loads((R/'tools/verification/test_manifest.json').read_text())
for opt in ['O0','O2']:
    B=O/opt;B.mkdir()
    cf=manifest['reference_c_flags']+['-'+opt]
    run('compile-reference-'+opt,[cc,*cf,'-I'+str(ref/'compat'),'-I'+str(S/'tests/reference/snare/compat'),
        '-I'+str(src/'DSPAudio'),'-I'+str(src/'MIDI'),'-c',ref/'oracle.c','-o',B/'reference.o'])
    run('compile-avr-'+opt,[cc,*cf,'-I'+str(S/'tests/reference/modulation/compat'),'-I'+str(S/'ref/LXR-0.37/front/LxrAvr/Menu'),
        '-c',S/'tests/reference/modulation/original_avr_targets.c','-o',B/'avr.o'])
    flags=manifest['native_flags']+['-'+opt,'-I'+str(S/'dsp')]
    run('compile-dsp-'+opt,[cxx,*flags,'-c',*[S/'dsp'/(n+'.cpp') for n in manifest['dsp_sources']]],cwd=B)
    exe=B/'P15M2ELfoRuntimeTest.exe'
    run('link-'+opt,[cxx,*flags,S/'tests/P15M2ELfoRuntimeTest.cpp',*[B/(n+'.o') for n in manifest['dsp_sources']],B/'reference.o',B/'avr.o','-o',exe])
    run('lfo-runtime-'+opt,[exe,B/'pcm_comparison.csv'],cwd=S,test=True)
    if results[-1]['status']=='FAIL':break
    # Test-only copied translation unit disconnects only dispatch. Comparator and
    # all production files remain unchanged. Expected failure is reported apart.
    negative=B/'Engine-disconnected.cpp'
    original=(S/'dsp/Engine.cpp').read_text()
    needle='applyModulationValue(mLfoRoutes[v],lfo.amount,lfo.output);'
    assert original.count(needle)==1
    negative.write_text(original.replace(needle,'/* TEST ONLY: dispatch disconnected */'))
    run('compile-disconnected-'+opt,[cxx,*flags,'-c',negative,'-o',B/'Engine-disconnected.o'])
    broken=B/'P15M2ELfoDisconnectedTest.exe'
    run('link-disconnected-'+opt,[cxx,*flags,S/'tests/P15M2ELfoRuntimeTest.cpp',*[B/(('Engine-disconnected' if n=='Engine' else n)+'.o') for n in manifest['dsp_sources']],B/'reference.o',B/'avr.o','-o',broken])
    run('lfo-disconnected-'+opt,[broken,B/'disconnected_comparison.csv'],cwd=S,test=True)
    results[-1]['negative_control']=True
    results[-1]['sensitivity']='DETECTED' if results[-1]['exit_code']==1 else 'BLOCKED' if results[-1]['status']=='BLOCKED' else 'UNDETECTED'
    (O/'test_results.json').write_text(json.dumps(results,indent=2))
(O/'source_manifest.json').write_text(json.dumps({str(f.relative_to(S)).replace('\\','/'):sha(f) for f in S.rglob('*') if f.is_file()},indent=2))
raise SystemExit(2 if any(x['status']=='BLOCKED' for x in results) else 1 if any(x['status']!='PASS' and not (x.get('negative_control') and x.get('sensitivity')=='DETECTED') for x in results) else 0)
