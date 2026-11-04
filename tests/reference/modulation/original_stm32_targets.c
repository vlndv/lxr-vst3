/* Independent pointer/type oracle: unchanged STM32 ParameterArray.c.
 * Copyright 2013 Julian Schmidt (included firmware). See LICENSE-LXR.txt.
 * Mock voice storage supplies distinct fields only; no DSP is rendered. The
 * actual ModulationNode definition preserves the uint16 destination/type8 quirk.
 */
#include "original_contract_fixture.h"
#define DRUMVOICE_H_
#define SNARE_H_
#define CYMBALVOICE_H_
#define HIHAT_H_
#define MIDIPARSER_H_
#define MIXER_H_
#define INCCMZ
#include "ParameterArray.h"
#include "modulationNode.h"

typedef struct { uint8_t waveform; float modNodeValue; } MockOsc;
typedef struct { float f, q, drive; } MockFilter;
typedef struct { float attack, decay, slope; uint8_t repeat; } MockEnvelope;
typedef struct { float shape; } MockDistortion;
typedef struct { float volume, pitch; uint8_t waveform; } MockTransient;
typedef struct {
    float modNodeValue;
    ModulationNode modTarget;
    uint8_t waveform, retrigger, sync;
    uint32_t phaseOffset;
} MockLfo;
typedef struct {
    MockOsc osc, modOsc, modOsc2, noiseOsc;
    MockFilter filter;
    MockEnvelope oscVolEg, oscPitchEg;
    MockDistortion distortion;
    MockTransient transGen;
    MockLfo lfo;
    float mix, fmModAmount, fmModAmount1, fmModAmount2, egPitchModAmount, vol, decayClosed, decayOpen;
    uint8_t pan, mixOscs, volumeMod, filterType;
} MockVoice;
MockVoice voiceArray[3], snareVoice, cymbalVoice, hatVoice;
ModulationNode velocityModulators[6];
float mixer_decimation_rate[7];
uint8_t mixer_audioRouting[6], midi_NoteOverride[7];
#include "ParameterArray.c"

void lxr_ref_initialize_stm32_table(void) { parameterArray_init(); }
unsigned lxr_ref_parameter_type(unsigned p) {
    return p < END_OF_SOUND_PARAMETERS && parameterArray[p].ptr ? parameterArray[p].type : 0xffu;
}
int lxr_ref_same_storage(unsigned a, unsigned b) {
    return a < END_OF_SOUND_PARAMETERS && b < END_OF_SOUND_PARAMETERS &&
        parameterArray[a].ptr && parameterArray[b].ptr && parameterArray[a].ptr == parameterArray[b].ptr;
}
unsigned lxr_ref_destination_storage_bytes(void) { return sizeof(velocityModulators[0].destination); }
