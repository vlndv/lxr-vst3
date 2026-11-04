from pathlib import Path
import os,json,subprocess,hashlib,datetime,time,sys,shutil,re
import argparse
a=argparse.ArgumentParser();a.add_argument('--output',required=True);args=a.parse_args()
R=Path(__file__).resolve().parents[3];O=Path(args.output).resolve();O.mkdir(parents=True,exist_ok=False)
cc=Path('C:/msys64/ucrt64/bin/gcc.exe');cxx=Path('C:/msys64/ucrt64/bin/g++.exe')
env=os.environ.copy();env['PATH']=str(cxx.parent)+os.pathsep+env.get('PATH','')
commands=[];results=[]
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
inputs=['tests/P15M2FStateContractTest.cpp','tests/reference/persistence/StateContractModel.h','interfaces/P15M2FPersistenceContract.h',
 'tests/reference/modulation/original_avr_targets.c','tests/reference/modulation/original_stm32_targets.c',
 'tests/reference/modulation/original_contract_fixture.h','dsp/ModulationContract.h','dsp/ModulationContractData.inc','dsp/VelocityRouting.h',
 'vendor/iPlug2/IPlug/IPlugParameter.h','vendor/iPlug2/IPlug/IPlugParameter.cpp','vendor/iPlug2/Examples/LXR/LXRState.h']
for d in ['tests/reference/persistence/fixtures','tests/reference/modulation/compat']:
 inputs.extend(p.relative_to(R).as_posix() for p in (R/d).rglob('*') if p.is_file())
manifest={p:sha(R/p) for p in inputs}
for p in inputs:
 q=O/'source'/p;q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(R/p,q)
(O/'source_manifest.json').write_text(json.dumps(manifest,indent=2))
def run(name,cmd,test=False):
 row=dict(name=name,command=list(map(str,cmd)),cwd=str(R),start_utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
 before=time.monotonic()
 try:
  process=subprocess.run(row['command'],cwd=R,env=env,capture_output=True,timeout=180)
  row.update(exit_code=process.returncode,status='PASS' if process.returncode==0 else 'FAIL')
  data=process.stdout+process.stderr
 except OSError as e:
  row.update(exit_code=None,status='BLOCKED',winerror=getattr(e,'winerror',None));data=str(e).encode()
 (O/(name+'.log')).write_bytes(data);row['log']=str(O/(name+'.log'));row['log_sha256']=sha(row['log']);row['elapsed_seconds']=time.monotonic()-before
 if test:
  row['executable_sha256']=sha(cmd[0]);m=re.search(rb'SUMMARY pass=(\d+) fail=(\d+)',data)
  if m:row.update(assertions_passed=int(m[1]),assertions_failed=int(m[2]))
  results.append(row)
 commands.append(row);(O/'commands.json').write_text(json.dumps(commands,indent=2));(O/'test_results.json').write_text(json.dumps(results,indent=2))
 print(name,row['status'],data.decode(errors='replace')[-1600:] if row['status']!='PASS' or test else '',flush=True)
 return row['status']
cf=['-std=gnu11','-fgnu89-inline','-ffp-contract=off','-Wall','-Wextra']
nf=['-std=c++17','-g','-fno-exceptions','-fno-rtti','-ffp-contract=off','-Wall','-Wextra','-pthread','-DNDEBUG','-DOS_WIN']
run('gcc-version',[cc,'--version']);run('gxx-version',[cxx,'--version'])
firmware=R/'ref/LXR-0.37'
includes=['-I'+str(R/'tests/reference/modulation/compat'),'-I'+str(firmware/'front/LxrAvr/Menu'),
 '-I'+str(firmware/'mainboard/LxrStm32/src/MIDI'),'-I'+str(firmware/'mainboard/LxrStm32/src/DSPAudio')]
for opt in ['O0','O2']:
 B=O/opt;B.mkdir()
 for stem in ['original_avr_targets','original_stm32_targets']:
  if run('compile-'+stem+'-'+opt,[cc,*cf,'-'+opt,*includes,'-c',R/'tests/reference/modulation'/(stem+'.c'),'-o',B/(stem+'.o')])!='PASS':sys.exit(2)
 exe=B/'P15M2FStateContractTest.exe'
 if run('compile-iplug-parameter-'+opt,[cxx,*nf,'-frtti','-'+opt,'-I'+str(R/'vendor/iPlug2/IPlug'),'-I'+str(R/'vendor/iPlug2/WDL'),
  '-c',R/'vendor/iPlug2/IPlug/IPlugParameter.cpp','-o',B/'IPlugParameter.o'])!='PASS':sys.exit(2)
 cmd=[cxx,*nf,'-'+opt,'-I'+str(R/'vendor/iPlug2/IPlug'),'-I'+str(R/'vendor/iPlug2/WDL'),
  R/'tests/P15M2FStateContractTest.cpp',B/'IPlugParameter.o',
  B/'original_avr_targets.o',B/'original_stm32_targets.o','-o',exe]
 if run('link-'+opt,cmd)!='PASS':sys.exit(2)
 status=run('state-contract-'+opt,[exe,R/'tests/reference/persistence/fixtures'],True)
 # An assertion failure is retained; the other independent optimization may run.
 # A policy-denied file is not retried or relocated.
sys.exit(1 if any(x['status']=='FAIL' for x in results) else 2 if any(x['status']=='BLOCKED' for x in results) else 0)
