"""Permitted VS LLVM compiler equivalents. Never changes Application Control."""
import argparse,json,pathlib,subprocess,hashlib
from run_checkpoint_tests import msvc_environment,visual_studio
ROOT=pathlib.Path(__file__).resolve().parents[2]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',required=True);ap.add_argument('--only-base',action='store_true');ap.add_argument('--only-locks',action='store_true');ap.add_argument('--baseline',default='out/p16f3/native-001/results.json');a=ap.parse_args();out=(ROOT/a.output).resolve();out.mkdir(parents=True,exist_ok=False)
 vs=visual_studio();env=msvc_environment(vs,'14.51');cc=vs/'VC/Tools/Llvm/x64/bin/clang-cl.exe';manifest=json.loads((ROOT/'tools/verification/test_manifest.json').read_text())
 denied=[r for r in json.loads((ROOT/a.baseline).read_text()) if r['status']=='BLOCKED'];rows=[]
 def command(name,cmd):
  try:r=subprocess.run(list(map(str,cmd)),cwd=ROOT,env=env,capture_output=True,text=True,timeout=300);log=r.stdout+r.stderr;status='PASS' if r.returncode==0 else 'FAIL';code=r.returncode
  except OSError as e:log=str(e);status='BLOCKED' if getattr(e,'winerror',None)==4551 else 'ERROR';code=-1
  except subprocess.TimeoutExpired as e:log=str(e);status='TIMEOUT';code=-1
  (out/(name+'.log')).write_text(log);row={'name':name,'status':status,'exit_code':code,'command':list(map(str,cmd)),'summary':[s for s in log.splitlines() if 'SUMMARY' in s]};rows.append(row);(out/'commands.json').write_text(json.dumps(rows,indent=2));print(name,status,flush=True);return code
 for opt in (['O2'] if a.only_base else ['O0','O2']):
  folder=out/opt;folder.mkdir();obj=folder/'obj';obj.mkdir()
  flags=['/nologo','/std:c++17','/Od' if opt=='O0' else '/O2','/MD','/GR-','/EHs-c-','/clang:-ffp-contract=off','/I'+str(ROOT/'dsp')]
  sources=[ROOT/'dsp'/(n+'.cpp') for n in manifest['dsp_sources']]
  code=command('compile-dsp-'+opt,[cc,*flags,'/c',*sources,'/Fo'+str(obj)+'\\'])
  if code:continue
  objects=[obj/(n+'.obj') for n in manifest['dsp_sources']]
  if a.only_locks:
   firmware=ROOT/'ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio';compat=ROOT/'tests/reference/sequencer/parameter_lock_compat';reference=ROOT/'tests/reference/sequencer';extra=[];ok=True
   for name,source in [('original',firmware/'automationNode.c'),('adapter',reference/'original_parameter_lock_adapter.c')]:
    target=folder/(name+'.obj');extra.append(target)
    if command('compile-lock-'+name+'-'+opt,[cc,'/nologo','/TC','/Od' if opt=='O0' else '/O2','/MD','/D_USE_MATH_DEFINES','/clang:-std=gnu11','/clang:-fgnu89-inline','/clang:-ffp-contract=off','/I'+str(compat),'/I'+str(reference),'/I'+str(firmware),'/c',source,'/Fo'+str(target)]):ok=False
   testobj=folder/'lock-test.obj';exe=folder/'P16DParameterLocksTest.exe'
   # Match the original GCC standalone harness's 2 MiB PE stack reserve.
   # The exact-DLL processing resource probe independently retains 1 MiB.
   if ok and not command('compile-lock-test-'+opt,[cc,*flags,'/c',ROOT/'tests/P16DParameterLocksTest.cpp','/Fo'+str(testobj)]) and not command('build-lock-test-'+opt,[cc,testobj,*objects,*extra,'/Fe'+str(exe),'/link','/STACK:2097152']):command('P16DParameterLocksTest_'+opt,[exe])
   else:rows.append({'name':'P16DParameterLocksTest_'+opt,'status':'NOT RUN — BUILD FAILED','command':[]})
   continue
  if a.only_base:
   base=ROOT/'out/p16f3/p15-base-001';recipe=json.loads((base/'commands.json').read_text());original=next(r['command'] for r in recipe if r['name']=='compile-reference-O2')
   includes=['/I'+x[2:] for x in original if x.startswith('-I')];reference=base/'reference/oracle.c';refobj=folder/'original_base.obj';testobj=folder/'test.obj';exe=folder/'P15M2BBaseMappingTest.exe'
   if command('compile-base-original',[cc,'/nologo','/TC','/O2','/MD','/D_USE_MATH_DEFINES','/clang:-std=gnu11','/clang:-fgnu89-inline','/clang:-ffp-contract=off',*includes,'/c',reference,'/Fo'+str(refobj)]):continue
   if command('compile-base-test',[cc,*flags,'/c',ROOT/'tests/P15M2BBaseMappingTest.cpp','/Fo'+str(testobj)]):continue
   if command('build-base-test',[cc,testobj,*objects,refobj,'/Fe'+str(exe)]):continue
   command('P15M2BBaseMappingTest_O2',[exe,folder/'pcm_comparison.csv',folder/'pitch_timing.csv']);continue
  for old in denied:
   if not old['name'].endswith('_'+opt):continue
   name=old['name'][:-(len(opt)+1)];source=ROOT/('dsp' if name.removesuffix('Test') in manifest['core_tests'] else 'tests')/(name+'.cpp');exe=folder/(name+'.exe');extra_objects=[]
   if name in ['P12b4cSnareRenderTest','OriginalCFilter']:
    firmware=ROOT/'ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio';kind='snare' if name=='P12b4cSnareRenderTest' else 'filter';compat=ROOT/('tests/reference/'+kind+'/compat');reference=ROOT/'tests/reference/snare/original_snare_harness.c' if kind=='snare' else firmware/'ResonantFilter.c';refobj=folder/('original_'+kind+'.obj')
    if command('compile-original-'+kind+'-'+opt,[cc,'/nologo','/TC','/Od' if opt=='O0' else '/O2','/MD','/D_USE_MATH_DEFINES','/clang:-std=gnu11','/clang:-fgnu89-inline','/clang:-ffp-contract=off','/I'+str(compat),'/I'+str(firmware),'/c',reference,'/Fo'+str(refobj)]):
     rows.append({'name':old['name'],'status':'NOT RUN — ORIGINAL BUILD FAILED','command':[]});continue
    extra_objects=[refobj]
    if kind=='filter':source=ROOT/'tests/reference/filter/diff_test.cpp'
   extra=['/EHsc'] if name=='DrumVoiceTest' else []
   testobj=folder/(name+'.obj')
   code=command('compile-'+old['name'],[cc,*flags,*extra,'/c',source,'/Fo'+str(testobj)])
   if not code:code=command('build-'+old['name'],[cc,testobj,*objects,*extra_objects,'/Fe'+str(exe)])
   if not code:command(old['name'],[exe])
   else:rows.append({'name':old['name'],'status':'NOT RUN — BUILD FAILED','command':[]})
 result=[r for r in rows if not r['name'].startswith(('compile-','build-'))];(out/'results.json').write_text(json.dumps(result,indent=2));return 0 if all(r['status']=='PASS' for r in result) else 1
if __name__=='__main__':raise SystemExit(main())
