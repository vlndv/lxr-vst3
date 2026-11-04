/* Test ABI only. Included after unchanged original DSP C by generate_reference.py.
 * Original Copyright 2013 Julian Schmidt; see LICENSE-LXR.txt. */
#include "original_base_adapter.h"

void base_ref_reset(uint32_t seed) {
    memset(voiceArray,0,sizeof(voiceArray)); memset(&snareVoice,0,sizeof(snareVoice));
    memset(&cymbalVoice,0,sizeof(cymbalVoice)); memset(&hatVoice,0,sizeof(hatVoice));
    memset(parameterArray,0,sizeof(parameterArray)); memset(velocityModulators,0,sizeof(velocityModulators));
    parameterArray_init(); initDrumVoice(); Snare_init(); Cymbal_init(); HiHat_init();
    for(int i=0;i<6;i++) modNode_init(&velocityModulators[i]);
    memset(midiParser_originalCcValues,0,sizeof(midiParser_originalCcValues));
    midiParser_activeNrpnNumber=0; g_rng_state=seed;
}
void base_ref_set(int id,int value) {
    MidiMsg m={0}; m.status=id<128?MIDI_CC:MIDI_CC2;
    /* Translate numeric PAR to ORIGINAL wire addressing, not plugin addressing. */
    m.data1=id<128?id+1:id-128; m.data2=value; midiParser_ccHandler(m,1);
}
void base_ref_trigger(int v,int vel,int note,int open) {
    if(v<3)Drum_trigger(v,vel,note);else if(v==3)Snare_trigger(vel,note);
    else if(v==4)Cymbal_trigger(vel,note);else HiHat_trigger(vel,open,note);
}
void base_ref_async(int v) {
    if(v<3)calcDrumVoiceAsync(v);else if(v==3)Snare_calcAsync();
    else if(v==4)Cymbal_calcAsync();else HiHat_calcAsync();
}
void base_ref_render(int v,int16_t* out) {
    if(v<3)calcDrumVoiceSyncBlock(v,out,32);else if(v==3)Snare_calcSyncBlock(out,32);
    else if(v==4)Cymbal_calcSyncBlock(out,32);else HiHat_calcSyncBlock(out,32);
}
static OscInfo* base_ref_osc(int v,int op) {
    if(v<3)return op?&voiceArray[v].modOsc:&voiceArray[v].osc;
    if(v==3)return op?&snareVoice.noiseOsc:&snareVoice.osc;
    if(v==4)return op==0?&cymbalVoice.osc:op==1?&cymbalVoice.modOsc:&cymbalVoice.modOsc2;
    return op==0?&hatVoice.osc:op==1?&hatVoice.modOsc:&hatVoice.modOsc2;
}
static SlopeEg2* base_ref_eg(int v) {
    return v<3?&voiceArray[v].oscVolEg:v==3?&snareVoice.oscVolEg:v==4?&cymbalVoice.oscVolEg:&hatVoice.oscVolEg;
}
static ResonantFilter* base_ref_filter_ptr(int v) {
    return v<3?&voiceArray[v].filter:v==3?&snareVoice.filter:v==4?&cymbalVoice.filter:&hatVoice.filter;
}
void base_ref_operator(int v,int op,int16_t* out) {
    float gain=v==4?(op==1?cymbalVoice.fmModAmount1:cymbalVoice.fmModAmount2):
                     (op==1?hatVoice.fmModAmount1:hatVoice.fmModAmount2);
    calcNextOscSampleBlock(base_ref_osc(v,op),out,32,gain);
}
void base_ref_filter(int v,int16_t* samples) {
    int type=v<3?voiceArray[v].filterType:v==3?snareVoice.filterType:v==4?cymbalVoice.filterType:hatVoice.filterType;
    SVF_calcBlockZDF(base_ref_filter_ptr(v),type,samples,32);
}
void base_ref_state(int v,BaseState* s) {
    memset(s,0,sizeof(*s));
    const int n=v<4?2:3;
    for(int i=0;i<n;i++) {
        OscInfo* o=base_ref_osc(v,i);s->freq[i]=o->freq;s->pitch[i]=o->pitchMod;
        s->phase[i]=o->phase;s->increment[i]=o->phaseInc;s->midi[i]=o->midiFreq;s->base[i]=o->baseNote;
    }
    if(v==4){s->gain[0]=cymbalVoice.fmModAmount1;s->gain[1]=cymbalVoice.fmModAmount2;}
    if(v==5){s->gain[0]=hatVoice.fmModAmount1;s->gain[1]=hatVoice.fmModAmount2;}
    ResonantFilter* f=base_ref_filter_ptr(v);s->cutoff=f->f;s->coefficient=f->g;s->resonance=f->q;
    SlopeEg2* eg=base_ref_eg(v);s->envelope=eg->value;s->envelopeState=eg->state;s->repeatCount=eg->repeatCnt;
    s->envelopeOutput=v<3?voiceArray[v].ampFilterInput:v==3?snareVoice.egValueOscVol:v==4?cymbalVoice.egValueOscVol:hatVoice.egValueOscVol;
    if(v<3){s->lastGain=voiceArray[v].lastGain;s->targetGain=voiceArray[v].ampFilterInput;}
}
void base_ref_envelope(int v,int state,float value,int repeat) {
    SlopeEg2* eg=base_ref_eg(v);eg->state=state;eg->value=value;eg->repeatCnt=repeat;
}
void base_ref_phase(int v,int op,uint32_t phase) { base_ref_osc(v,op)->phase=phase; }
/* Matched counterfactual snapshots for measuring the FIRST affected sample.
 * All routes are off in these tests; no live original-value pointer is moved. */
static DrumVoice saved_drums[3];
static SnareVoice saved_snare;
static CymbalVoice saved_cymbal;
static HiHatVoice saved_hat;
static uint32_t saved_rng;
void base_ref_checkpoint(void) {
    memcpy(saved_drums,voiceArray,sizeof(saved_drums));saved_snare=snareVoice;
    saved_cymbal=cymbalVoice;saved_hat=hatVoice;saved_rng=g_rng_state;
}
void base_ref_restore_checkpoint(void) {
    memcpy(voiceArray,saved_drums,sizeof(saved_drums));snareVoice=saved_snare;
    cymbalVoice=saved_cymbal;hatVoice=saved_hat;g_rng_state=saved_rng;
}
