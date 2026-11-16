"""Compile a current-source exact-DLL probe with the established VS host SDK libraries."""
import argparse, hashlib, json, pathlib, subprocess
from run_checkpoint_tests import msvc_environment, visual_studio
ROOT=pathlib.Path(__file__).resolve().parents[2]
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--source',required=True);ap.add_argument('--output',required=True)
    ap.add_argument('--host-build',default='out/p16f3/host-001');ap.add_argument('--opt',default='O2',choices=['Od','O2'])
    ap.add_argument('--compiler',choices=['msvc','llvm'],default='msvc')
    ap.add_argument('arguments',nargs='*');a=ap.parse_args()
    out=(ROOT/a.output).resolve();out.mkdir(parents=True,exist_ok=False)
    base=(ROOT/a.host_build).resolve(); commands=json.loads((base/'commands.json').read_text())
    row=next(r for r in commands if r['name']=='build-P16F3VST3CertificationTest')
    command=row['command']; oldsource=next(x for x in command if x.endswith('P16F3VST3CertificationTest.cpp'))
    source=(ROOT/a.source).resolve();exe=out/(source.stem+'.exe')
    command=[str(source) if x==oldsource else '/'+a.opt if x=='/O2' else '/Fo'+str(out/'probe.obj') if x.startswith('/Fo') else '/Fe'+str(exe) if x.startswith('/Fe') else x for x in command]
    command+=['/STACK:1048576']
    env=msvc_environment(visual_studio(), '14.51')
    compile_commands=[command]
    if a.compiler=='llvm':
        compiler=visual_studio()/'VC/Tools/Llvm/x64/bin/clang-cl.exe'
        source_index=command.index(str(source));link_index=command.index('/link')
        flags=command[1:source_index]
        objects=[x for x in command[source_index+1:link_index] if not x.startswith(('/Fo','/Fe'))]
        compile_commands=[[str(compiler),*flags,'/c',str(source),'/Fo'+str(out/'probe.obj')],
                          [str(compiler),str(out/'probe.obj'),*objects,'/Fe'+str(exe),*command[link_index:]]]
    build_log=''
    for actual in compile_commands:
        build=subprocess.run(actual,cwd=ROOT,env=env,capture_output=True,text=True);build_log+=build.stdout+build.stderr
        if build.returncode:break
    (out/'build.log').write_text(build_log)
    result={'source':str(source),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest().upper(),'compiler':a.compiler,'compile_commands':compile_commands,'compile_exit':build.returncode,'stack_reserve_bytes':1048576}
    if build.returncode==0:
        args=[str(exe),*[x if x.isdecimal() else str((ROOT/x).resolve()) for x in a.arguments]]
        result['command']=args; result['executable_sha256']=hashlib.sha256(exe.read_bytes()).hexdigest().upper()
        try:
            r=subprocess.run(args,cwd=ROOT,env=env,capture_output=True,text=True,timeout=110)
            (out/'run.log').write_text(r.stdout+r.stderr);result.update(exit_code=r.returncode,status='PASS' if r.returncode==0 else 'FAIL')
        except OSError as e:result.update(status='BLOCKED' if getattr(e,'winerror',None)==4551 else 'ERROR',error=str(e))
    else:result['status']='BUILD_FAIL'
    (out/'results.json').write_text(json.dumps(result,indent=2));print(json.dumps(result));return 0 if result['status']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
