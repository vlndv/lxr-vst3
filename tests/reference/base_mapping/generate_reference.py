"""Generate a host compatibility translation unit for pinned original LXR C.

Reuses the established snare shim and original-C extraction approach from M1.
No synthesis expression is rewritten. Complete CC/NRPN handlers and two buffer
gain functions are extracted verbatim, with source lines/body hashes retained.
The generated translation unit and hardware-only headers stay in ignored output.
Original Copyright 2013 Julian Schmidt; derived material: LICENSE-LXR.txt.
"""
from pathlib import Path
import re,json,argparse,hashlib
p=argparse.ArgumentParser();p.add_argument('--root',required=True);p.add_argument('--output',required=True);a=p.parse_args()
R=Path(a.root).resolve();O=Path(a.output).resolve();O.mkdir(parents=True,exist_ok=True)
F=R/'ref/LXR-0.37/mainboard/LxrStm32/src'
def read(p): return p.read_text(encoding='utf-8')
extracts=[]
def function(path,name):
    s=read(path); m=re.search(r'^[^\n;{}]*\b'+name+r'\([^;{}]*\)\s*\{',s,re.M)
    assert m,name
    start=m.start(); end=s.index('{',m.start())+1; depth=1
    while depth:
        if s[end]=='{': depth+=1
        if s[end]=='}': depth-=1
        end+=1
    body=s[start:end]
    extracts.append(dict(path=str(path.relative_to(R)),name=name,line=s[:start].count('\n')+1,body=body))
    return body
def typedef(path,tag,name):
    s=read(path); return re.search(r'typedef struct '+tag+r'\b.*?}\s*'+name+r';',s,re.S)[0]
s=read(R/'tests/reference/snare/original_snare_harness.c')
prefix=s[:s.index('/* Exact firmware implementations')]
prefix=prefix.replace('#include "original_snare_harness.h"','#include "'+(R/'tests/reference/snare/original_snare_harness.h').as_posix()+'"\n#include <stdbool.h>')
prefix=prefix.replace('typedef struct LfoStruct { uint32_t unused; } Lfo;','REPLACE_LFO')
prefix=prefix.replace('typedef struct ModulationNodeStruct { float value; } ModulationNode;','')
prefix=prefix.replace('ModulationNode velocityModulators[6];','')
prefix=prefix.replace('static void lfo_init(Lfo* lfo) { lfo->unused = 0; }','')
prefix=prefix.replace('static void lfo_retrigger(uint8_t voice) { (void)voice; }','')
prefix=prefix.replace('static void modNode_updateValue(ModulationNode* node, float value) { node->value = value; }','')
header=read(F/'MIDI/ParameterArray.h')
enum=re.search(r'enum ParamEnums\s*\{.*?\n};',header,re.S)[0]
types=header[header.index('typedef union'):header.index('extern Parameter')]
node=typedef(F/'DSPAudio/modulationNode.h','ModulatorStruct','ModulationNode')
lfo=typedef(F/'DSPAudio/lfo.h','LfoStruct','Lfo')
prefix=prefix.replace('REPLACE_LFO',enum+'\n'+types+'\n'+node+'\n'+lfo)
extra=r'''
#define DRUMVOICE_H_
#define CYMBALVOICE_H_
#define HIHAT_H_
#define MIXER_H_
#define PARAMETERARRAY_H_
#define NUM_VOICES 3
#define OUTPUT_DMA_SIZE 32
#define ENABLE_DRUM_SVF 1
#define ENABLE_MIX_OSC 1
#define AMP_EG_SYNC 0
#define USE_FILTER_DRIVE 0
#define USE_PEAK 0
#define UNIT_GAIN_DRIVE 0
#define FILTER_SHAPER -0.9f
#define LFO_SR (REAL_FS/(float)OUTPUT_DMA_SIZE)
#define LFO_MAX_F 200
#define PITCH_AMOUNT_FACTOR 32
#define LFO_SINE 0
#define LFO_TRI 1
#define LFO_SAW_UP 2
#define LFO_SAW_DOWN 3
#define LFO_REC 4
#define LFO_NOISE 5
#define LFO_EXP_UP 6
#define LFO_EXP_DOWN 7
#define TYPE_UINT8 0
#define TYPE_FLT 1
#define TYPE_SPECIAL_F 2
#define TYPE_UINT32 3
#define TYPE_SPECIAL_P 4
#define TYPE_SPECIAL_FILTER_F 5
#define TRIGGER_1 0
#define TRIGGER_5 4
#define TRIGGER_6 5
#define TRIGGER_7 6
typedef struct { int unused; } Dither;
'''
extra+='\n'+typedef(F/'DSPAudio/DrumVoice.h','VoiceStruct','DrumVoice')
extra+='\n'+typedef(F/'DSPAudio/CymbalVoice.h','CymbalStruct','CymbalVoice')
extra+='\n'+typedef(F/'DSPAudio/HiHat.h','HiHatStruct','HiHatVoice')
extra+=r'''
extern DrumVoice voiceArray[3];
extern CymbalVoice cymbalVoice;
extern HiHatVoice hatVoice;
extern SnareVoice snareVoice;
Parameter parameterArray[END_OF_SOUND_PARAMETERS];
extern ModulationNode velocityModulators[6];
uint8_t midi_NoteOverride[7];
float mixer_decimation_rate[7];
uint8_t mixer_audioRouting[6];
uint8_t midiParser_originalCcValues[255];
uint16_t midiParser_activeNrpnNumber;
static float test_bpm=120;
static float seq_getBpm(void) { return test_bpm; }
static void seq_setMute(uint8_t v,uint8_t on) { (void)v;(void)on; }
void modNode_resetTargets(void);
void modNode_setDestination(ModulationNode*,uint16_t);
void modNode_init(ModulationNode*);
void modNode_updateValue(ModulationNode*,float);
void lfo_init(Lfo*);
void lfo_retrigger(uint8_t);
void lfo_setFreq(Lfo*,float);
void lfo_setSync(Lfo*,uint8_t);
static void uart_sendFrontpanelByte(uint8_t v) { (void)v; }
'''
# Existing harness supplies a saturating-add host service; add exact gain functions.
extra+='\n'+function(F/'DSPAudio/BufferTools.c','bufferTool_addGain')
extra+='\n'+function(F/'DSPAudio/BufferTools.c','bufferTool_addGainInterpolated')
prefix=prefix.replace('Parameter parameterArray[END_OF_SOUND_PARAMETERS];','')
# Remove stale conditional declarations? parameter array is defined by original source below.
extra=extra.replace('Parameter parameterArray[END_OF_SOUND_PARAMETERS];','extern Parameter parameterArray[END_OF_SOUND_PARAMETERS];')
source=prefix+'\n'+extra+'\n'
source+='#include "'+(F/'MIDI/MidiMessages.h').as_posix()+'"\n'
source+='void midiParser_ccHandler(MidiMsg,uint8_t);\n'
for name in ('Decay','SlopeEg2','snapEg','distortion','ResonantFilter','transientGenerator','Oscillator'):
    source+='#include "'+(F/f'DSPAudio/{name}.c').as_posix()+'"\n'
for rel in ('MIDI/ParameterArray.c','DSPAudio/modulationNode.c','DSPAudio/lfo.c','DSPAudio/DrumVoice.c','DSPAudio/CymbalVoice.c','DSPAudio/HiHat.c','DSPAudio/Snare.c'):
    source+='#include "'+(F/rel).as_posix()+'"\n'
source+='\n'+function(F/'MIDI/MidiParser.c','calcPitchModAmount')
source+='\n'+function(F/'MIDI/MidiParser.c','midiParser_nrpnHandler')
source+='\n'+function(F/'MIDI/MidiParser.c','midiParser_ccHandler')
footer=s[s.index('static int load_file'):]
# Existing ref_snare_reset uses memset and stubs only when no modulation is enabled.
source+='\n'+footer

source += '\n#include "' + (R/'tests/reference/base_mapping/original_base_adapter.c').as_posix() + '"\n'
(O/'oracle.c').write_text(source,encoding='utf-8')
for entry in extracts:
    entry['sha256']=hashlib.sha256(entry['body'].encode()).hexdigest().upper()
(O/'extracted_functions.json').write_text(json.dumps(extracts,indent=2),encoding='utf-8')
compat=O/'compat';compat.mkdir(exist_ok=True)
for name in ('stm32f4xx.h','config.h','MidiParser.h','sequencer.h','TriggerOut.h','MidiNoteNumbers.h'):
    (compat/name).write_text('/* Host services supplied by the independent oracle. */\n#include <stdint.h>\n')
print('Generated independent original-C base-mapping oracle; unchanged sources and extracted complete functions.')
