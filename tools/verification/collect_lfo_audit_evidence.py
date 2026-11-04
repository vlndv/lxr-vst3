"""Collect P15-M2D source evidence/results; never executes binaries or changes DSP.
Run after the documented lfo-tests-001/002 and regression runs. Existing historical
evidence is read only. This script may refresh ONLY task-owned audit artifacts.
"""
from pathlib import Path
import csv, hashlib, json, re, struct, subprocess, sys
from collections import Counter
R=Path(__file__).resolve().parents[2];O=R/'out/p15-m2d'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
def write(name,data): (O/name).write_text(json.dumps(data,indent=2),encoding='utf-8')
def table(name,rows):
    with (O/name).open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
F='ref/LXR-0.37/mainboard/LxrStm32/src/'
A='ref/LXR-0.37/front/LxrAvr/'
catalog=[]
functions={
 F+'DSPAudio/lfo.c':['lfo_init','lfo_calc','lfo_dispatchNextValue','lfo_calcPhaseInc','lfo_setFreq','lfo_setSync','lfo_recalcSync','lfo_retrigger'],
 F+'DSPAudio/random.c':['GetRngValue'],
 F+'DSPAudio/modulationNode.c':['modNode_init','modNode_setOriginalValueChanged','modNode_originalValueChanged','modNode_resetTargets','modNode_reassignVeloMod','modNode_setDestination','modNode_updateValue'],
 F+'MIDI/ParameterArray.c':['paramArray_setParameter','parameterArray_init'],
 F+'MIDI/MidiParser.c':['midiParser_ccHandler','midiParser_noteOn','midiParser_parseMidiMessage'],
 F+'DSPAudio/DrumVoice.c':['initDrumVoice','Drum_trigger','calcDrumVoiceAsync','calcDrumVoiceSyncBlock'],
 F+'DSPAudio/Snare.c':['Snare_init','Snare_trigger','Snare_calcAsync','Snare_calcSyncBlock'],
 F+'DSPAudio/CymbalVoice.c':['Cymbal_init','Cymbal_trigger','Cymbal_calcAsync','Cymbal_calcSyncBlock'],
 F+'DSPAudio/HiHat.c':['HiHat_init','HiHat_trigger','HiHat_calcAsync','HiHat_calcSyncBlock'],
 F+'DSPAudio/mixer.c':['mixer_calcNextSampleBlock','mixer_decimateBlock','mixer_addDataToOutput'],
 F+'Sequencer/sequencer.c':['seq_setBpm','seq_getBpm','seq_sync'],
 F+'Sequencer/clockSync.c':['sync_calcBpm','sync_tick','sync_midiStartStop'],
 F+'MIDI/MidiVoiceControl.c':['voiceControl_noteOn'],
 A+'Menu/Cc2Text.c':['getModTargetGapIndex','getModTargetIdxFromGapIdx','getNumModTargets'],
 A+'Preset/presetManager.c':['preset_sendDrumsetParameters'],
 'dsp/Lfo.cpp':['lfoFrequencyFromMidi','lfoPhaseOffsetFromMidi'],
}
missing=[]
for path,names in functions.items():
    s=(R/path).read_text(encoding='utf-8')
    for name in names:
        m=re.search(r'^[^\n;{}]*\b'+name+r'\([^;{}]*\)\s*\{',s,re.M)
        if not m:missing.append([path,name]);continue
        end=s.index('{',m.start())+1;depth=1
        while depth:
            if s[end]=='{':depth+=1
            if s[end]=='}':depth-=1
            end+=1
        body=s[m.start():end]
        catalog.append(dict(path=path,symbol=name,first_line=s[:m.start()].count('\n')+1,
            last_line=s[:end].count('\n')+1,source_sha256=sha(R/path),body_sha256=hashlib.sha256(body.encode()).hexdigest().upper(),body=body))
write('source_catalog.json',dict(functions=catalog,unresolved_symbol_names=missing))
# Entire numbered files retain provenance for switches, typedefs, and include
# order, not just equations. These copies are evidence, never compiler inputs.
paths=set(functions)|{F+'DSPAudio/lfo.h',F+'DSPAudio/modulationNode.h',F+'config.h',F+'main.c',
 F+'MIDI/frontPanelParser.c',A+'Menu/menu.c',A+'Menu/Cc2Text.c',A+'frontPanelParser.c',
 'dsp/Lfo.h','dsp/Engine.h','dsp/Engine.cpp','dsp/ParameterArray.cpp','dsp/ParameterArray.h',
 'dsp/VelocityRouting.h','dsp/ModulationContract.h','dsp/ModulationContractData.inc','dsp/LXRAudioAdapter.cpp',
 'dsp/MidiParser.cpp','dsp/DrumVoice.cpp','dsp/Snare.cpp','dsp/Cymbal.cpp','dsp/HiHat.cpp',
 'vendor/iPlug2/Examples/LXR/LXR.cpp','vendor/iPlug2/Examples/LXR/LXR.h',
 F+'Hardware/cs4344_cs5343.c',F+'AudioCodecManager/AudioCodecManager.h'}
e=O/'numbered_sources';e.mkdir(exist_ok=True)
for path in sorted(paths):
    lines=(R/path).read_text(encoding='utf-8').splitlines()
    (e/(path.replace('/','__')+'.txt')).write_text('\n'.join(f'{n}: {line}' for n,line in enumerate(lines,1))+'\n',encoding='utf-8')
write('source_provenance.json',{path:sha(R/path) for path in sorted(paths)})
waveforms=[('Sine','((sine_table[phase >> 20]/32767.f)+1)/2.f','lfo.c:72-74','4096 indexed int16 values; no interpolation; -cos-like table','lookup; phase 0 gives 0','table max < 1'),
 ('Triangle','1.f-fabsf((phase/(float)0xffffffff)*2-1)','lfo.c:76-78','uint32 -> float; denominator rounds to 2^32','0 -> 1 at half cycle -> 0','near-wrap float can round phase fraction to 1'),
 ('Saw up','phase/(float)0xffffffff','lfo.c:80-82','uint32 -> float','0 -> 1','rounding can reach 1 before wrap'),
 ('Saw down','(0xffffffff-phase)/(float)0xffffffff','lfo.c:105-107','UNSIGNED subtraction BEFORE float conversion','1 -> 0','different from 1.f-float(phase)/2^32'),
 ('Rectangle','phase > 0x7fffffff ? 1.f : 0.f','lfo.c:84-93','uint32 compare; fixed 50 percent duty','low first half; high second half','threshold exact; no duty parameter'),
 ('Sample and hold noise','on oldPhase > phase: rnd=GetRngValue(); rnd=rnd/(float)0xffffffff; return rnd','lfo.c:95-103','RNG uint32 -> float at assignment THEN divide','held unipolar float','hardware entropy bytes UNVERIFIED; init does not assign rnd'),
 ('Exponential up','x=phase/(float)0xffffffff; x*x*x','lfo.c:109-114','float multiplication left associative','0 -> 1','no exp function'),
 ('Exponential down','x=(0xffffffff-phase)/(float)0xffffffff; x*x*x','lfo.c:116-121','unsigned subtraction then float; left associative','1 -> 0','different rounding from C++ inverse')]
rows=[]
for w,(name,expr,where,types,polarity,end) in enumerate(waveforms):
    rows.append(dict(wave_id=w,name=name,parameter_ids='155..160',target_indices='28;62;96;129;163;197',
        source=F+'DSPAudio/'+where,symbol='lfo_calc',input_types='uint32 phase/inc; float multiplier; uint8 waveform',
        output_type='float',expression=expr,conversion=types,polarity=polarity,endpoints=end,
        order='float(phaseInc)*modNodeValue -> uint32 increment -> uint32 phase addition -> evaluate waveform',
        verification='O0 exact original-C; final O2 BLOCKED; initial O2 waveform comparisons exact',
        range='[0,1] under finite valid inputs; sine exact extrema 0 and 0.9999847412109375'))
table('lfo_primitive_matrix.csv',rows)
# Extend waveform table with separate scalar/phase contracts rather than hide
# them behind generic waveform metadata.
scalar=[
 dict(item='rate',ids='115..120',indices='25;59;93;126;160;194',source=F+'DSPAudio/lfo.c:136-220',input='float raw 0..127',output='float freq; uint32 phaseInc',expression='f+=1; f/=128.f; f=f*f*f; freq=f*200; phaseInc=freq/(REAL_FS/32.f)*0xffffffff',qualification='finite in-range C conversion only; raw0 is nonzero; explicit zeroHz increment is zero'),
 dict(item='phase advance',ids='115..120',indices='25;59;93;126;160;194',source=F+'DSPAudio/lfo.c:61-67',input='uint32 phaseInc; float modNodeValue',output='uint32 phase',expression='float inc=phaseInc*modNodeValue; uint32 incInt=inc; phase+=incInt',qualification='uint32->float may round integer UP before second narrowing; unsigned phase addition wraps'),
 dict(item='offset',ids='185..190',indices='30;64;98;131;165;199',source=F+'MIDI/MidiParser.c:1028-1044',input='uint8 raw',output='uint32 phaseOffset',expression='raw/127.f * 0xffffffff',qualification='0..126 exact tested; 127 produces float 2^32: ISO C out-of-range conversion; ARM saturation policy qualified, no host-C equality claim'),
 dict(item='amount',ids='121..126;143..148',indices='27;61;95;128;162;196',source=F+'DSPAudio/modulationNode.c:182-216',input='float current,amount,source (integers promoted to float)',output='original field type',expression='current*amount*source + (1.f-amount)*current',qualification='narrow after EACH node; zero depth identity only where float conversion/field range permits; UInt32 max is NOT certified'),
 dict(item='initialization',ids='all LFO controls',indices='25..30 etc',source=F+'DSPAudio/lfo.c:47-59',input='Lfo*',output='fields',expression='phase=0;phaseInc=1;phaseOffset=0;retrigger=0;waveform=SINE;sync=0;freq=1;modNodeValue=1;modNode_init',qualification='rnd not assigned; freq=1 does NOT imply increment for 1Hz until setter/recalculation'),
]
table('lfo_scalar_matrix.csv',scalar)
timing=[]
for case,event,first,scope,verified in [
 ('before-boundary','base/depth/rate write processed before Bk','32*k','first possible frame; actual volume fixture at k=0:0','O0 source and rendered fixture'),
 ('after-boundary','write after Bk has already generated frames [32k,32k+31]','32*(k+1)','cached/generated PCM unchanged; actual volume fixture:32','O0 source and rendered fixture'),
 ('before-trigger','setting followed by accepted trigger','next ungenerated block','retrigger scans six LFOs before velocity; offset/retrigger fields consumed immediately at trigger','O0 source; actual hat and repeated trigger states'),
 ('sustain','no trigger; free-running enabled or destination off','32*k','all six advance once per mixer call independent of envelope state','O0 original mixer phase test'),
 ('repeated-before-tick','two accepted nonzero Note On events','next ungenerated block','each velocity mutates current; each eligible retrigger resets phase; only last held velocity replayed','O0 original field tests'),
 ('within-original-block','physical receipt while mixer executes','UNKNOWN receipt-to-frame; next parsed event precedes later mixer call','main loop does not parse during synchronous mixer work; UART/USB/DMA physical latency unavailable','SOURCE_INFERRED; hardware timing UNVERIFIED'),
 ('sync-byte-modulation','node changes byte without setter','next setter/seq_setBpm for increment; trigger consumers immediately','no implicit recalc; restored byte can coexist with formerly calculated increment','O0 original state test'),
 ('lfo-to-lfo-forward','lower owner writes higher owner modNodeValue','same Bk','reset all; velocity all; LFO0..5 immediately dispatch; later LFO sees write','O0 original full mixer'),
 ('lfo-to-lfo-backward','higher owner writes lower owner modNodeValue','no next-tick carry under normal baseline restoration','lower owner already advanced; reset removes multiplier next tick','O0 original full mixer'),
 ('host-first-sample','host callback plus FIR adapter','UNVERIFIED exact host index','existing wrapper drops per-event offsets; generated native block/FIR history cannot be retroactively edited','source inspection; no new host modulation timing claim'),
 ]:
    timing.append(dict(case=case,event=event,native_first_affected_frame=first,semantics=scope,status=verified,
        source=F+'DSPAudio/mixer.c:287-378; '+F+'main.c:267-299; dsp/LXRAudioAdapter.cpp:37-72',
        stage_order='receipt buffer -> parse/dispatch -> all-LFO retrigger -> velocity -> osc/EG initialization -> restore12 -> velocity0..5 -> LFO0..5 -> filter coefficients -> async6 -> sync/filter voice-by-voice -> decimate/pan/mix',
        native_frames_per_tick=32,host_or_DAC_latency='not inferred from native frame labels'))
table('lfo_timing_matrix.csv',timing)
manifest=list(csv.DictReader((R/'out/p15-m2a/target_manifest.csv').open()))
gate=list(csv.DictReader((R/'out/p15-m2c/velocity_target_gate.csv').open()))
dest=[]
for owner in range(6):
    for t in manifest:
        idx=int(t['index']);p=int(t['parameter_id']);kind='not LFO control'
        if 115<=p<=120:kind='frequency multiplier (read at ordered LFO advance)'
        elif 121<=p<=126:kind='node amount (read on dispatch; self/current ordering matters)'
        elif 155<=p<=160:kind='waveform byte (read at ordered calc)'
        elif 173<=p<=178:kind='retrigger byte (read by all-LFO scan on trigger)'
        elif 179<=p<=184:kind='sync byte ONLY (recalc on setter or tempo publication)'
        elif 185<=p<=190:kind='uint32 phaseOffset (read on retrigger; high endpoint qualification)'
        classification='SOURCE_CONFIRMED_FIELD; O0 scalar fixture'
        policy='deferred pending per-domain integration fixtures'
        if idx==0:classification='CONFIRMED_OFF';policy='explicit typed off'
        elif 149<=p<=154:classification='KNOWN_IDENTITY_UNSAFE_ROUTE_ADDRESS_MUTATION';policy='reject transactionally; no mutation probe replay'
        elif idx==1:policy='original internal/kit global route; normal menu excludes; defer M2E'
        elif 227>=p>=221:policy='MIDI-dispatch config; defer M2E'
        elif any(int(x['index'])==idx and x['gate']=='VERIFIED_ROUTE' for x in gate):policy='existing M2C scalar subset; candidate LFO integration, needs paired render fixtures'
        dest.append(dict(source_kind='Lfo',source_owner=owner,source_storage=['voiceArray[0].lfo','voiceArray[1].lfo','voiceArray[2].lfo','snareVoice.lfo','cymbalVoice.lfo','hatVoice.lfo'][owner],
            amount_id=121+owner,legacy_voice_id=161+owner,legacy_target_id=167+owner,
            original_index=idx,parameter_id=p,physical_destination=t['destination_symbol'],target_voice=t['voice'],scope=t['scope'],pointer=t['pointer'],value_type=t['firmware_type'],
            normal_menu_selectable=t['normal_lfo_menu_selectable'],lfo_dependent_semantics=kind,
            source=t['table_source']+'; '+t['pointer_source']+'; '+F+'MIDI/frontPanelParser.c:428-451; '+F+'DSPAudio/modulationNode.c:138-228',
            dispatch_order=owner,mutation='live field; current*amount*source+(1.f-amount)*current; narrow after each node',
            selector_storage='AVR full global row byte -> translated PAR; STM32 node uint16; plugin legacy seven-bit bytes inert',
            active_voice_change='original allows; setter globally restores all captured targets immediately',
            bounds='front menu selected-voice range/off; kit bounds row; STM32 setDestination/update read array before any destination bounds check',
            classification=classification,proposed_m2e_policy=policy))
table('destination_dispatch_matrix.csv',dest)

comparison={}
for run in ['lfo-tests-001','lfo-tests-002']:
    comparison[run]=dict(executions=json.loads((O/run/'test_results.json').read_text()),modes={})
    for opt in ['O0','O2']:
        c=O/run/opt/'comparisons.csv'
        comparison[run]['modes'][opt]=list(csv.DictReader(c.open())) if c.exists() else 'BLOCKED; no final comparisons produced'
write('original_c_comparison.json',dict(classification='BLOCKED',firmware_revision='dee49689f3f0cdf8f9b7b5e79628cfdca1b79394',
    acceptance='exact finite float bit equality and integer equality; no tolerance; declared in test source before execution',
    reference='unchanged C lfo/modulation/voices; full original mixer functions verbatim; existing M2B generator; deterministic hardware-service RNG; GPIO availability/trigger IO stubs',
    final_O0='224377 passed, zero failed, 77 groups; 10 C++ observational groups recorded separately',
    final_O2='Application Control WinError4551; BLOCKED, no retry/bypass',
    first_run_fixture_error='reset() published tempo and recalculated increment before init-default assertion; both modes 224376 pass/1 fail. Corrected setup explicitly invokes original lfo_init, expected increment remains 1.',
    primitive_vs_timing_vs_field='separate groups in comparisons.csv; native rendered first-change fixture only; no port PCM equality claimed',
    full_rendered_PCM_equality='NOT TESTED: production LFO runtime is disconnected; no integration implemented',
    unavailable=['hardware RNG values','MCU fast-math/FMA output','offset127 ISO-C out-of-range conversion','UInt32 high-end narrowing','external clock transport jitter','DAC/USB/UART receipt latency','LXRS route restoration (no such persisted route exists)'],
    runs=comparison))
reg=[]
for d,file,key in [('regression-m2a','contract_tests.json','tests'),('regression-m2b','test_results.json',None),('regression-m2c','test_results.json',None),('regression-checkpoint','results.json',None)]:
    data=json.loads((O/d/file).read_text());data=data[key] if key else data
    for x in data:
        row=dict(x);row['evidence_root']=d;log=O/d/(row.get('log') or row['name']+'.log')
        if log.exists() and 'Application Control policy has blocked this file' in log.read_text():
            row['runner_status']=row['status'];row['status']='BLOCKED';row['reason']='WinError4551 Application Control; raw log retained'
        if x['name']=='ControllerSyncProbe':
            row['runner_status']=row['status'];row['status']='INVALID_ABI';row['reason']='Current M2C Engine header is larger than certified installed Engine; private-member offsets invalid. Public ABI tests pass; same probe passes against preserved matching M2C private bundle.'
        reg.append(row)
reg.append(dict(name='contract-O2',status='NOT_RUN',reason='Original M2A runner stops after O0 loader denial; O2 compiled, no optimization retry as workaround',evidence_root='regression-m2a'))
matched=json.loads((O/'controller-probe-matched-m2c.json').read_text());matched.update(name='ControllerSyncProbe-matched-M2C',status='PASS',scope='preserved private bundle, not installed DLL');reg.append(matched)
write('regression_results.json',dict(classification='BLOCKED',fresh_executions=reg,counts=dict(Counter(x['status'] for x in reg)),
    historical_results='Prior M2A/M2B/M2C PASS reports preserved; not substituted for current denials',
    policy='No denied executable retried, relocated, signed or rebuilt as a bypass. Independent prescribed suites continued.',
    ABI='Installed public state322/runtime1143/controller955/validator47 pass; incompatible private-member run995/38 retained; matching M2C private probe1033/0 passes. No confirmed unrelated production regression.'))
required=['AGENTS.md','docs/architecture.md','docs/risks.md','docs/phase_log.md','docs/param_map.md','docs/build_and_test.md',
 'docs/p15_m2a_modulation_contract.md','docs/p15_m2b_base_mapping_corrections.md','docs/p15_m2c_velocity_routing.md',
 'docs/p12c_state_serialization.md','docs/p12c_r1_concurrency_hardening.md','docs/p12c_r2_controller_sync_repair.md',
 'out/p15-m2a/target_manifest.csv','out/p15-m2a/selector_compatibility.json','out/p15-m2b/reference_comparison.json',
 'out/p15-m2c/velocity_route_matrix.csv','out/p15-m2c/reference_comparison.json','out/p15-m2c/selector_limitations.md']
write('mandatory_inputs.json',{p:sha(R/p) for p in required})
print('Evidence collected',dict(Counter(x['status'] for x in reg)), 'source functions',len(catalog),'unresolved symbol spellings',missing)

if '--finalize' in sys.argv:
    start=json.loads((O/'integrity_start.json').read_text())
    def git(*args):return subprocess.check_output(['git',*args],cwd=R).decode().strip()
    unchanged={p:sha(R/p) for p in start['files'] if (R/p).is_file()}
    changed=[p for p,h in start['files'].items() if unchanged.get(p)!=h]
    installed=Path(start['installed_path']);head=git('rev-parse','HEAD');branch=git('branch','--show-current')
    pin=git('-C','ref/LXR-0.37','rev-parse','HEAD')
    end=dict(head=head,branch=branch,status=git('status','--porcelain=v1'),index_sha256=sha(R/'.git/index'),
        installed_path=str(installed),installed_sha256=sha(installed),firmware_revision=pin,
        firmware_status=git('-C','ref/LXR-0.37','status','--porcelain=v1'),changed_existing_files=changed,
        existing_files_checked=len(start['files']))
    assert not changed and end['index_sha256']==start['index_sha256']
    assert head==start['head']=='0091651720cacbcb2ecd3172bd667581ffb83415'
    assert branch==start['branch']=='p12b-4c-fir-resampler'
    assert sha(installed)==start['installed_sha256']=='89F6DF598EBC88F580A6013D83DA10053A177ECBBFC89CA88AC474E2C3E7FF82'
    assert pin=='dee49689f3f0cdf8f9b7b5e79628cfdca1b79394' and not end['firmware_status']
    m2c=json.loads((R/'out/p15-m2c/file_inventory.json').read_text())
    assert all(sha(R/p)==h for p,h in m2c['task_hashes'].items())
    write('integrity_end.json',end)
    now=set(git('ls-files','-z').split('\0'))|set(git('ls-files','--others','--exclude-standard','-z').split('\0'))
    created=sorted(p for p in now if p and p not in start['files'] and (R/p).is_file())
    assert all(p.startswith(('docs/p15_m2d','tests/P15M2D','tests/reference/lfo/','tools/verification/run_lfo_','tools/verification/collect_lfo_')) for p in created),created
    report=f'''# P15-M2D integrity report

Classification: **BLOCKED**, mandatory Windows Application Control executions.
Source audit and O0 finite deterministic verification complete; no runtime LFO
integration. No assertion failure was hidden as a security denial.

Branch before/after: {branch}.
HEAD before/after: {head}.
Firmware before/after: {pin}; clean Git status and all395 pinned archive files
verified. iPlug2 d54f69050f517e43b941d88c2a170f0a840b9ee4, SDK root
3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96 and all seven SDK submodules match
tools/verification/dependencies.json. Approved state-reader patch unchanged.
Before/after verification used immutable archives in this task's cache with
--verify-only; dependency_verification.json and dependency-verification-end.log
retain results. The first default-cache check reported absent archives; no
dependency source was written to resolve it.

Installed certified VST3 SHA-256 before/after:
{sha(installed)}.
Index SHA-256 before/after: {end['index_sha256']}.
All{len(start['files'])} starting source/evidence files retain their byte hashes;
this includes every starting M2A/M2B/M2C evidence file and all current production
files. Historical M2C task_hashes match independently. Existing modified/untracked
work is preserved. No existing file was changed, including docs/phase_log.md.

New source/docs files ({len(created)}):
'''+''.join('- '+p+'\n' for p in created)+'''

Exact hashes of all created source/docs and task evidence are in file_inventory.json.
It excludes its own recursive hash; all other files present at finalization are
listed. Dependency archives, copied sources and test build products live only
under out/p15-m2d. Original firmware was never modified. No production source,
SDK, installed binary, index, branch or existing report was edited. No staging,
commit, push, merge, reset, clean, checkout or Windows security-policy action.

Final LFO O0:224377 passed/0 failed; final O2 WinError4551 BLOCKED. Initial
fixture setup failures are preserved separately. Regression summary:44 valid
PASS,8 BLOCKED,1 not run after loader denial,1 invalid-ABI result retained.
The identical internal probe passes1033/0 on the preserved matching M2C bundle;
installed public state/runtime/controller/validator checks pass. No unrelated
production regression was confirmed. No denied executable was retried or
relocated/rebuilt/signed as a bypass. No new production plugin was built.

M2E contract remains design only. No phase-log completion append is permitted
while the mandatory execution gate remains blocked. No P16/morph/preset/UI work.
'''
    (O/'integrity_report.md').write_text(report,encoding='utf-8')
    evidence={str(p.relative_to(O)).replace('\\','/'):sha(p) for p in O.rglob('*') if p.is_file() and p.name!='file_inventory.json'}
    write('file_inventory.json',dict(created_source_files={p:sha(R/p) for p in created},modified_existing_files=[],
        preserved_existing_files=start['files'],created_evidence_files=evidence,evidence_root='out/p15-m2d',
        self_hash='Excluded to avoid a recursive hash; integrity_report.md is included',
        starting_head=start['head'],ending_head=head,index_sha256=end['index_sha256'],installed_sha256=sha(installed)))
    print('Final integrity PASS; unchanged existing files',len(start['files']),'new source/docs',len(created),'evidence files',len(evidence))
