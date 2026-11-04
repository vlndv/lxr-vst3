"""Extract pinned original modulation metadata; never edit firmware/runtime.

Outputs machine-readable evidence. --emit-data writes the standalone contract's
new .inc file; --check-data compares it without writing. No generated C oracle
uses this table: tests compile the original AVR and STM32 C independently.
"""
import argparse
import collections
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FW = "ref/LXR-0.37/"
AVR = FW + "front/LxrAvr/"
STM = FW + "mainboard/LxrStm32/src/"
PIN = "dee49689f3f0cdf8f9b7b5e79628cfdca1b79394"


def read(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace")


def without_comments(s):
    return re.sub(r"/\*.*?\*/|//[^\n]*", lambda m: "".join("\n" if c == "\n" else " " for c in m[0]), s, flags=re.S)


def location(path, position):
    return dict(file=path, line=read(path)[:position].count("\n") + 1)


def enum_values(path):
    s = without_comments(read(path))
    # menu.h has ten obsolete codec-label entries behind an undefined
    # USE_CODEC_EQ. Neither AVR config/build recipe defines it. The independent
    # unchanged-C oracle verifies the compiled labels, not this parser's output.
    if path == AVR + 'Menu/menu.h':
        s = re.sub(r'#if USE_CODEC_EQ\s*\n.*?#endif',
                   lambda m: ''.join('\n' if ch == '\n' else ' ' for ch in m[0]), s, flags=re.S)
    result = {}
    for block in re.finditer(r"enum\s*\w*\s*\{([^}]+)\}", s):
        n = -1
        for item in block[1].split(","):
            bits = item.strip().split("=")
            if not bits[0]:
                continue
            if len(bits) == 2:
                value = bits[1].strip()
                n = result[value] if value in result else int(value, 0)
            else:
                n += 1
            result[bits[0].strip()] = n
    return result


def semantic(parameter, pointer):
    # These groups describe the unchanged original mapped FIELD, not new DSP
    # arithmetic. The source setter and pointer references accompany every row.
    n = parameter
    if not pointer:
        return "None", "None", "NoDestination", "no target", "None"
    stages = "OrderedTick"
    policy = "TypedField"
    if n in range(149, 155):
        return "MutableDestinationLowByte", "MutableRoute", "RequiresDestinationMutationReview", "uint16 destination storage, TYPE_UINT8 accesses low byte; table/PAR identity not a 7-bit knob", "Stm32NoOpAvrRelayUnverified"
    if n in range(221, 228):
        return "MidiNoteFilter", "MidiDispatch", policy, "0..127;0 any note, nonzero matching note triggers default63", "DirectStm32"
    if n in range(215, 221):
        return "OutputRouting", "AudioSample", policy, "0..5 normal hardware routes; raw uint8 in firmware, no menu clamp in node", "DirectStm32"
    if "modNodeValue" in pointer:
        return "FrequencyMultiplier", stages, policy, "base1 multiplier; modulation scales mapped frequency/phase increment (coarse/fine pointer aliases)", "DirectStm32"
    if "waveform" in pointer:
        if ".lfo." in pointer:
            return "LfoWaveform", stages, policy, "0..7 named LFO waves; raw uint8 narrowing, no menu clamp in node", "DirectStm32"
        if ".transGen." in pointer:
            return "TransientWaveform", "OrderedTick | Consumption::AudioSample | Consumption::Trigger", policy, "raw setter0..13,>=14->0; node writes raw uint8 without this clamp", "DirectStm32"
        return "OscillatorWaveform", "OrderedTick | Consumption::AudioSample | Consumption::Trigger", policy, "0..5 ROM;>=6 user SD out of scope; node writes raw uint8", "DirectStm32"
    if ".sync" in pointer:
        return "SyncEnumeration", "SetterOrTempoRecalculation", policy, "0 free,1..11 sync; node byte update does not recalc phaseInc", "DirectStm32"
    if ".retrigger" in pointer:
        return "RetriggerVoice", "Trigger", policy, "0 off,1..6 source voice; raw uint8 comparison", "DirectStm32"
    if ".phaseOffset" in pointer:
        return "PhaseOffset", "Trigger", policy, "uint32 phase; raw knob127 float cast ARM saturation qualification", "DirectStm32"
    if ".repeat" in pointer:
        return "RepeatCount", "Trigger", policy, "0..127 from knob; uint8 node truncation per application", "DirectStm32"
    if ".decayOpen" in pointer or ".decayClosed" in pointer:
        return "EnvelopeStep", "Trigger", policy, "mapped decay step0..1 copied into active hat EG on trigger", "DirectStm32"
    if ".slope" in pointer:
        return "SignedSlopeCoefficient", stages, policy, "negative through positive rational coefficient; pitch endpoint127 inf; amp inverseSlope is a separate non-target field", "DirectStm32"
    if ".attack" in pointer or ".decay" in pointer:
        return "EnvelopeStep", stages, policy, "mapped step0..1 per native tick; raw127 step0", "DirectStm32"
    if ".filter.f" in pointer:
        return "CutoffCyclesPerSample", stages, policy, "mapped0..0.45 cycles/sample; coefficients recalculated after modulation", "DirectStm32"
    if ".filter.q" in pointer:
        return "Resonance", "AudioSample", policy, "mapped q=1-v/127; below0.1->0.02", "DirectStm32"
    if ".filter.drive" in pointer:
        return "FilterDrive", "AudioSample", policy, "mapped0.4..6.4 before modulation; no reapplication of knob floor", "DirectStm32"
    if ".filterType" in pointer:
        return "FilterEnumeration", "AudioSample", policy, "raw knob0..7 -> internal1..8; modulation uint8 may produce0 without +1", "DirectStm32"
    if ".volumeMod" in pointer or ".mixOscs" in pointer:
        return "Boolean", "AudioSample", policy, "uint8;0 false/nonzero true; menu0..1; narrowing after every node", "DirectStm32"
    if ".pan" in pointer:
        return "Pan", "AudioSample", policy, "0..127 unsigned despite centered UI display; sqrt LUT stereo only", "DirectStm32"
    if "decimation_rate" in pointer:
        return "DecimationRate", "AudioSample", policy, "mapped0..1; per-voice*global sample-hold rate", "DirectStm32"
    if ".distortion.shape" in pointer:
        return "DistortionCoefficient", "AudioSample", policy, "mapped0..254 rational coefficient, not raw0..127", "DirectStm32"
    if ".egPitchModAmount" in pointer:
        return "PitchEnvelopeAmount", stages, policy, "mapped(v/127)^2*32;0..32", "DirectStm32"
    if ".transGen.pitch" in pointer:
        return "TransientPitch", "OrderedTick | Consumption::AudioSample", policy, "mapped0.25..approximately3.9963; float phase promotion retained", "DirectStm32"
    if ".transGen.volume" in pointer:
        return "UnitFloat", "OrderedTick | Consumption::AudioSample | Consumption::Trigger", policy, "mapped0..1; snap amount/offset/sample gain depend on waveform", "DirectStm32"
    if "velocityModulators" in pointer:
        return "UnitFloat", "OrderedTick | Consumption::Trigger", policy, "mapped amount0..1; trigger and ordered held-velocity replay", "DirectStm32"
    if ".lfo.modTarget.amount" in pointer:
        return "UnitFloat", stages, policy, "mapped LFO target amount0..1; source order matters", "DirectStm32"
    return "UnitFloat", "AudioSample", policy, "mapped unit gain/mix0..1; float operation order unchanged", "DirectStm32"


def extract():
    revision = subprocess.check_output(["git", "-C", str(ROOT / (FW.rstrip('/'))), "rev-parse", "HEAD"]).decode().strip()
    dirty = subprocess.check_output(["git", "-C", str(ROOT / (FW.rstrip('/'))), "status", "--porcelain"]).decode().strip()
    if revision != PIN or dirty:
        raise RuntimeError("Firmware reference identity/integrity mismatch")
    params = enum_values(STM + "MIDI/ParameterArray.h")
    front = enum_values(AVR + "Parameters.h")
    names = enum_values(AVR + "Menu/menu.h")
    assert params['END_OF_SOUND_PARAMETERS'] == front['END_OF_SOUND_PARAMETERS'] == 228
    path = STM + "MIDI/ParameterArray.c"
    body = without_comments(read(path))
    pointers, types, locations = {}, {}, {}
    for m in re.finditer(r"parameterArray\[(PAR_\w+)\]\.ptr\s*=\s*([^;]+);", body):
        pointers[params[m[1]]] = m[2].strip()
        locations[params[m[1]]] = location(path, m.start())
    for m in re.finditer(r"parameterArray\[(PAR_\w+)\]\.type\s*=\s*(\w+)", body):
        types[params[m[1]]] = m[2]
    path = AVR + "Menu/Cc2Text.c"
    body = without_comments(read(path))
    begin = body.index("const ModTarg modTargets[]")
    finish = body.index("};", begin)
    table = list(re.finditer(r"\{\s*(TEXT_\w+)\s*,\s*(PAR_\w+)\s*\}", body[begin:finish]))
    assert len(table) == 205
    rbegin = body.index("const ModTargetVoiceOffset modTargetVoiceOffsets")
    ranges = [(int(a), int(b)) for a, b in re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*\}", body[rbegin:body.index("};", rbegin)])]
    assert ranges == [(2,35),(36,69),(70,103),(104,136),(137,170),(171,204)]
    gbegin = body.index("const uint8_t modTargetGapMap[]")
    gaps = [int(x,16) for x in re.findall(r"0x[0-9A-Fa-f]+", body[gbegin:body.index("};",gbegin)])]
    assert len(gaps) == 43
    rows = []
    canonical = {}
    for index, m in enumerate(table):
        label, symbol = m[1], m[2]
        parameter = params[symbol]
        assert front[symbol] == parameter
        voice = next((v for v, (first,last) in enumerate(ranges) if first <= index <= last), None)
        pointer = pointers.get(parameter, "")
        destination_symbol = canonical.setdefault(pointer, symbol) if pointer else "PAR_NONE"
        sem, consume, policy, domain, midi = semantic(parameter, pointer)
        setter_path = STM + "MIDI/MidiParser.c"
        # Individual source switch blocks are included in the full evidence file;
        # node destinations6 use the internal UART dispatch, not a direct setter.
        setter_line = read(setter_path).index('void midiParser_ccHandler')
        if parameter in range(149,155):
            setter_path = STM + "MIDI/frontPanelParser.c"
            setter_line = read(setter_path).index('case FRONT_CC_VELO_TARGET:')
        t = dict(index=index, original_symbol=symbol, label_symbol=label,
                 label_id=names[label], parameter_id=parameter,
                 destination_symbol=destination_symbol.removeprefix('PAR_'),
                 destination_id=params[destination_symbol],
                 voice=voice, scope='None' if index==0 else 'Global' if index==1 else 'Voice',
                 availability='Off' if index==0 else 'AutomationOnlyMenu' if index==1 else 'VoiceMenu',
                 firmware_type=types.get(parameter,'NONE'), semantic=sem, consumption=consume,
                 runtime_policy=policy, nominal_native_range=domain, midi_disposition=midi,
                 pointer=pointer, table_source=location(path,begin+m.start()),
                 pointer_source=locations.get(parameter,dict(file=STM+'MIDI/ParameterArray.c',line=0)),
                 setter_source=location(setter_path,setter_line),
                 front_panel_configurable=True,
                 normal_lfo_menu_selectable=index!=1,
                 normal_velocity_menu_scope='off' if index==0 else 'unavailable' if index==1 else 'own voice only',
                 normal_automation_menu_selectable=True, preset_index_allowed=True,
                 affects_rendering='off' if index==0 else 'route mutation requires verification' if policy=='RequiresDestinationMutationReview' else 'configuration/dispatch dependent' if consume in ['MidiDispatch','SetterOrTempoRecalculation','Trigger'] else 'mapped rendering/control field, source-order/trigger qualifications apply')
        rows.append(t)
    assert len({r['parameter_id'] for r in rows}) == 205
    assert len({r['pointer'] for r in rows if r['pointer']}) == 198
    internal=[]
    menu_parameters={r['parameter_id'] for r in rows}
    for parameter in sorted(pointers.keys()-menu_parameters):
        symbol=next(k for k,v in params.items() if v==parameter and k.startswith('PAR_'))
        pointer=pointers[parameter]
        sem,consume,policy,domain,midi=semantic(parameter,pointer)
        symbol=canonical.setdefault(pointer,symbol)
        voice=parameter-215 if 215<=parameter<=220 else 5
        internal.append(dict(index=None,original_symbol=symbol,label_symbol='',label_id=0xffff,
            parameter_id=parameter,destination_symbol=symbol.removeprefix('PAR_'),destination_id=params[symbol],
            voice=voice,scope='Voice',availability='InternalMessageOnly',firmware_type=types[parameter],
            semantic=sem,consumption=consume,runtime_policy=policy,nominal_native_range=domain,
            midi_disposition=midi,pointer=pointer,table_source=locations[parameter],pointer_source=locations[parameter],
            setter_source=location(STM+'MIDI/MidiParser.c',read(STM+'MIDI/MidiParser.c').index('void midiParser_ccHandler')),
            front_panel_configurable=False,normal_lfo_menu_selectable=False,
            normal_velocity_menu_scope='unavailable',normal_automation_menu_selectable=False,preset_index_allowed=False,
            affects_rendering='not an AVR modTargets entry; STM32 internal-message pointer exists'))
    assert [r['parameter_id'] for r in internal]==[215,216,217,218,219,220,227]
    return dict(firmware_revision=PIN,targets=rows,internal_only_parameters=internal,voice_ranges=ranges,gap_map=gaps,
                counts=dict(raw_entries=205,off=1,non_null_fields=204,normal_voice_menu_entries=203,
                            normal_automation_only_entries=1,distinct_storage_fields=198,
                            duplicate_storage_rows=6,requires_destination_mutation_review=6,
                            internal_only_pointer_targets=7,total_stm32_non_null_fields=211,
                            total_distinct_stm32_storage_fields=205,unknown_table_identities=0))


def cpp(data):
    lines = ['// Generated by tools/verification/extract_modulation_contract.py.',
             '// Sonic Potions LXR original C @ '+PIN,
             '// Copyright 2013 Julian Schmidt; see LICENSE-LXR.txt.',
             '// Include only inside lxr::modulation_contract after metadata declarations.',
             'enum class DestinationId : std::uint16_t {']
    seen=set()
    for t in data['targets']+data['internal_only_parameters']:
        symbol=t['destination_symbol']
        if symbol not in seen:
            lines.append('    '+symbol+' = '+str(t['destination_id'])+',');seen.add(symbol)
    lines += ['};','inline constexpr std::array<TargetMetadata, kOriginalTargetCount> kOriginalTargets {{']
    type_names=dict(TYPE_UINT8='UInt8',TYPE_FLT='Float',TYPE_SPECIAL_F='FrequencyMultiplier',TYPE_UINT32='UInt32',NONE='None')
    voices=['Drum1','Drum2','Drum3','Snare','Cymbal','HiHat']
    q=lambda s:json.dumps(s,ensure_ascii=True)
    source=lambda s:'{'+q(s['file'])+', '+str(s['line'])+'}'
    def row(t):
        index='OriginalTargetIndex{}' if t['index'] is None else 'OriginalTargetIndex::fromRaw('+str(t['index'])+')'
        return ['    {'+index+', FirmwareParameterId::fromRaw('+str(t['parameter_id'])+'),',
                  '     DestinationId::'+t['destination_symbol']+', Voice::'+('Invalid' if t['voice'] is None else voices[t['voice']])+', RoutingScope::'+t['scope']+',',
                  '     TargetAvailability::'+t['availability']+', ParameterValueType::'+type_names[t['firmware_type']]+', ValueSemantic::'+t['semantic']+',',
                  '     Consumption::'+t['consumption']+', RuntimePolicy::'+t['runtime_policy']+', OriginalMidiDisposition::'+t['midi_disposition']+',',
                  '     '+str(t['label_id'])+', '+q(t['original_symbol'])+', '+q(t['label_symbol'])+', '+q(t['pointer'])+',',
                  '     '+q(t['nominal_native_range'])+',',
                  '     '+source(t['table_source'])+', '+source(t['pointer_source'])+', '+source(t['setter_source'])+'},']
    for t in data['targets']:lines+=row(t)
    lines+=['}};','inline constexpr std::array<TargetMetadata, kInternalOnlyParameterCount> kInternalOnlyParameters {{']
    for t in data['internal_only_parameters']:lines+=row(t)
    lines+=['}};','inline constexpr std::array<VoiceTargetRange, kVoiceCount> kVoiceTargetRanges {{']
    for first,last in data['voice_ranges']:
        lines.append('    {OriginalTargetIndex::fromRaw('+str(first)+'), OriginalTargetIndex::fromRaw('+str(last)+')},')
    lines+=['}};','inline constexpr std::array<std::uint8_t, kLogicalSlotCount> kOriginalGapMap {{',
            '    '+', '.join(hex(x) for x in data['gap_map']),'}};','']
    return '\n'.join(lines)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--emit-data',type=Path)
    parser.add_argument('--check-data',type=Path)
    args=parser.parse_args();data=extract();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    generated=cpp(data)
    if args.emit_data:args.emit_data.write_text(generated,encoding='utf-8')
    if args.check_data:
        assert args.check_data.read_text(encoding='utf-8')==generated,'Contract data drift'
    (out/'extracted_original_contract.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
    rows=[]
    for t in data['targets']:
        row={k:(f"{v['file']}:{v['line']}" if k.endswith('_source') else v) for k,v in t.items()}
        row['aliases']=[r['index'] for r in data['targets'] if r['pointer']==t['pointer'] and r['index']!=t['index'] and t['pointer']]
        rows.append(row)
    with (out/'target_manifest.csv').open('w',newline='',encoding='utf-8') as f:
        writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    (out/'internal_only_targets.json').write_text(json.dumps(data['internal_only_parameters'],indent=2),encoding='utf-8')
    inputs=[AVR+'Menu/Cc2Text.c',AVR+'Menu/menu.h',AVR+'Parameters.h',AVR+'config.h',AVR+'Makefile',AVR+'DrumSynthFront.cproj',STM+'MIDI/ParameterArray.c',STM+'MIDI/ParameterArray.h',STM+'MIDI/MidiParser.c',STM+'MIDI/frontPanelParser.c']
    data_path=args.check_data or args.emit_data
    (out/'extraction_provenance.json').write_text(json.dumps(dict(firmware=PIN,inputs={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest().upper() for p in inputs},normalized_lf_data_sha256=hashlib.sha256(generated.encode()).hexdigest().upper(),data_file_sha256=hashlib.sha256(data_path.read_bytes()).hexdigest().upper() if data_path else None,conditional_labels='USE_CODEC_EQ undefined/disabled in AVR config and both build recipes; compiled original-C label test independently checks enum order'),indent=2))
    print(json.dumps(data['counts']))


if __name__=='__main__':main()
