"""Build/run P15-M2A metadata-only C++ tests with independent original C.

Uses the GCC flags from docs/build_and_test.md. Does not build/install the VST3
or compile rendering DSP. Refuses existing output, records all command/source/
executable identities, and stops execution on a Windows loader restriction.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT=Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--cc',default=shutil.which('gcc'))
    parser.add_argument('--cxx',default=shutil.which('g++'))
    args=parser.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    if not args.cc or not args.cxx:raise RuntimeError('GCC/G++ required; use --cc/--cxx')
    fw=ROOT/'ref/LXR-0.37'
    revision=subprocess.check_output(['git','-C',str(fw),'rev-parse','HEAD']).decode().strip()
    dirty=subprocess.check_output(['git','-C',str(fw),'status','--porcelain']).decode().strip()
    if revision!='dee49689f3f0cdf8f9b7b5e79628cfdca1b79394' or dirty:
        raise RuntimeError('Pinned clean original firmware required')
    fixture=ROOT/'tests/reference/modulation'
    include=[fixture/'compat',fw/'front/LxrAvr/Menu',fw/'mainboard/LxrStm32/src/MIDI',fw/'mainboard/LxrStm32/src/DSPAudio']
    inputs=[ROOT/'interfaces/ModulationContract.h',ROOT/'dsp/ModulationContract.h',ROOT/'dsp/ModulationContractData.inc',ROOT/'tests/P15M2AModulationContractTest.cpp',
            *[p for p in fixture.rglob('*') if p.is_file()],
            fw/'front/LxrAvr/Menu/Cc2Text.c',fw/'front/LxrAvr/Menu/menu.h',fw/'front/LxrAvr/Parameters.h',
            fw/'mainboard/LxrStm32/src/MIDI/ParameterArray.c',fw/'mainboard/LxrStm32/src/MIDI/ParameterArray.h',
            fw/'mainboard/LxrStm32/src/DSPAudio/modulationNode.h',ROOT/'dsp/MidiParser.h',
            ROOT/'vendor/iPlug2/Examples/LXR/LXRParamContract.h',ROOT/'vendor/iPlug2/Examples/LXR/LXRState.h']
    identities={str(p.relative_to(ROOT)):sha(p) for p in inputs}
    (out/'source_manifest.json').write_text(json.dumps(identities,indent=2))
    env=os.environ.copy();env['PATH']=str(Path(args.cxx).resolve().parent)+os.pathsep+env.get('PATH','')
    commands=[];tests=[]
    def save():
        (out/'commands.json').write_text(json.dumps(commands,indent=2))
        (out/'contract_tests.json').write_text(json.dumps(dict(firmware_revision=revision,tests=tests,
            classification='FAIL' if any(t['status']=='FAIL' for t in tests) else 'BLOCKED' if any(t['status']=='BLOCKED' for t in tests) else 'PASS' if len(tests)==2 else 'INCOMPLETE'),indent=2))
    def command(name,args,test=False):
        args=list(map(str,args));start=time.monotonic()
        row=dict(name=name,command=args,cwd=str(ROOT),fresh_source_build=True)
        if test:row['executable_sha256']=sha(args[0])
        try:
            process=subprocess.run(args,cwd=ROOT,env=env,capture_output=True,timeout=55)
            log=(process.stdout+process.stderr).decode(errors='replace')
            row.update(status='PASS' if process.returncode==0 else 'FAIL',exit_code=process.returncode)
        except (OSError,subprocess.TimeoutExpired) as error:
            log=str(error);row.update(status='BLOCKED',exit_code=None,error=str(error))
        row.update(elapsed_seconds=time.monotonic()-start,log=name+'.log',summary=[s for s in log.splitlines() if s.startswith(('FAIL','SUMMARY'))])
        (out/row['log']).write_text(log,encoding='utf-8')
        commands.append(row)
        if test:
            tests.append(row)
            assert row['executable_sha256']==sha(args[0])
        save();print(name,row['status'],row['summary'][-1:],flush=True)
        return row['status']
    executables=[]
    for opt in ('O0','O2'):
        folder=out/opt;folder.mkdir()
        objects=[]
        cflags=['-std=gnu11','-fgnu89-inline','-ffp-contract=off','-Wall','-Wextra','-'+opt,*['-I'+str(p) for p in include]]
        for name in ('original_avr_targets','original_stm32_targets'):
            obj=folder/(name+'.o')
            if command('compile-'+name+'-'+opt,[args.cc,*cflags,'-c',fixture/(name+'.c'),'-o',obj])!='PASS':
                raise RuntimeError('C oracle compilation unavailable; inspect recorded log')
            objects.append(obj)
        exe=folder/'P15M2AModulationContractTest.exe'
        flags=['-std=c++17','-'+opt,'-g','-fno-exceptions','-fno-rtti','-ffp-contract=off','-Wall','-Wextra','-pthread']
        if command('link-contract-'+opt,[args.cxx,*flags,ROOT/'tests/P15M2AModulationContractTest.cpp',*objects,'-o',exe])!='PASS':
            raise RuntimeError('Contract compilation unavailable; inspect recorded log')
        executables.append((opt,exe))
    # Compile both prescribed configurations before execution. A loader denial
    # stops execution; no path/name/optimization retry is used as a workaround.
    for opt,exe in executables:
        if command('contract-'+opt,[exe],test=True)!='PASS':break
    for p in inputs:assert sha(p)==identities[str(p.relative_to(ROOT))],'Input changed during tests'
    save()
    return 0 if len(tests)==2 and all(t['status']=='PASS' for t in tests) else 1


if __name__=='__main__':raise SystemExit(main())
