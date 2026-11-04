"""P15-M2B independent original-C mapping/timing tests; never installs a plugin.

All generated translation units, snapshots, objects and results use a NEW output
directory. Normal Windows policy applies; denied executions are recorded BLOCKED.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--root',default=str(R));p.add_argument('--cc');p.add_argument('--cxx');a=p.parse_args()
R=Path(a.root).resolve();O=Path(a.output).resolve();O.mkdir(parents=True,exist_ok=False)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest().upper()
cc=shutil.which(a.cc or os.getenv('CC','gcc'));cxx=shutil.which(a.cxx or os.getenv('CXX','g++'))
if not cc or not cxx:raise SystemExit('Missing GCC/G++; use documented compiler setup.')
env=os.environ.copy();env['PATH']=str(Path(cxx).parent)+os.pathsep+env.get('PATH','')
pin=subprocess.check_output(['git','-C',str(R/'ref/LXR-0.37'),'rev-parse','HEAD']).decode().strip()
dirty=subprocess.check_output(['git','-C',str(R/'ref/LXR-0.37'),'status','--porcelain'])
assert pin=='dee49689f3f0cdf8f9b7b5e79628cfdca1b79394' and not dirty
S=O/'source'
for directory in ['dsp','tests','data']:
    shutil.copytree(R/directory,S/directory,ignore=shutil.ignore_patterns('__pycache__','*.exe','*.o'))
shutil.copytree(R/'ref/LXR-0.37/mainboard',S/'ref/LXR-0.37/mainboard')
commands=[];results=[]
def save():
    (O/'commands.json').write_text(json.dumps(commands,indent=2))
    (O/'test_results.json').write_text(json.dumps(results,indent=2))
def run(name,cmd,cwd=S,test=False):
    t=time.monotonic();blocked=False
    try:
        r=subprocess.run(list(map(str,cmd)),cwd=cwd,env=env,capture_output=True,timeout=180)
        code=r.returncode;log=r.stdout.decode(errors='replace')+r.stderr.decode(errors='replace')
    except OSError as e:
        code=-1;log=str(e);blocked=getattr(e,'winerror',None) in [5,577,1260,4551]
    row=dict(name=name,command=list(map(str,cmd)),cwd=str(cwd),exit_code=code,status='BLOCKED' if blocked else 'PASS' if code==0 else 'FAIL',elapsed_seconds=time.monotonic()-t,log=name+'.log',summary=[l for l in log.splitlines() if 'SUMMARY' in l])
    if test:row['executable_sha256']=sha(Path(cmd[0]));results.append(row)
    commands.append(row);(O/(name+'.log')).write_text(log);save()
    if test:print(name,row['status'],row['summary'],flush=True)
    elif code:raise RuntimeError('Build failed: '+name+'; see '+str(O/(name+'.log')))
run('generate-reference',[sys.executable,S/'tests/reference/base_mapping/generate_reference.py','--root',S,'--output',O/'reference'])
run('gcc-version',[cc,'--version']);run('gxx-version',[cxx,'--version'])
manifest=json.loads((R/'tools/verification/test_manifest.json').read_text())
for opt in ['O0','O2']:
    B=O/opt;B.mkdir();flags=manifest['native_flags']+['-'+opt,'-I'+str(S/'dsp')]
    run('compile-dsp-'+opt,[cxx,*flags,'-c',*[S/'dsp'/(n+'.cpp') for n in manifest['dsp_sources']]],cwd=B)
    ref=O/'reference'
    run('compile-reference-'+opt,[cc,*manifest['reference_c_flags'],'-'+opt,'-I'+str(ref/'compat'),'-I'+str(S/'tests/reference/snare/compat'),'-I'+str(S/'ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio'),'-I'+str(S/'ref/LXR-0.37/mainboard/LxrStm32/src/MIDI'),'-c',ref/'oracle.c','-o',B/'reference.o'])
    exe=B/'P15M2BBaseMappingTest.exe'
    run('link-'+opt,[cxx,*flags,S/'tests/P15M2BBaseMappingTest.cpp',*[B/(n+'.o') for n in manifest['dsp_sources']],B/'reference.o','-o',exe])
    run('base-mapping-'+opt,[exe,B/'pcm_comparison.csv',B/'pitch_timing.csv'],test=True)
    if results[-1]['status']=='BLOCKED': print('Execution denied; no bypass attempted.',flush=True)
inputs={str(q.relative_to(S)).replace('\\','/'):sha(q) for q in S.rglob('*') if q.is_file()}
(O/'source_manifest.json').write_text(json.dumps(inputs,indent=2))
save()
raise SystemExit(2 if any(t['status']=='BLOCKED' for t in results) else 1 if any(t['status']=='FAIL' for t in results) else 0)
