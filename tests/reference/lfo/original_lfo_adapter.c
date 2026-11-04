/* Included AFTER unchanged original DSP C and verbatim complete mixer functions.
 * Original Copyright 2013 Julian Schmidt; see LICENSE-LXR.txt.
 * Only test observation/setup and hardware-service boundaries live here. */
#include "original_lfo_adapter.h"
static Lfo* lfo_ref_owner(unsigned v) {
    return v<3?&voiceArray[v].lfo:v==3?&snareVoice.lfo:v==4?&cymbalVoice.lfo:&hatVoice.lfo;
}
void lfo_ref_get(unsigned v,LfoReferenceState* s) {
    Lfo* l=lfo_ref_owner(v);
    s->phase=l->phase;s->increment=l->phaseInc;s->offset=l->phaseOffset;
    s->frequency=l->freq;s->multiplier=l->modNodeValue;s->random=l->rnd;
    s->amount=l->modTarget.amount;s->held=l->modTarget.lastVal;
    s->waveform=l->waveform;s->retrigger=l->retrigger;s->sync=l->sync;
    s->destination=l->modTarget.destination;
}
void lfo_ref_put(unsigned v,const LfoReferenceState* s) {
    Lfo* l=lfo_ref_owner(v);
    l->phase=s->phase;l->phaseInc=s->increment;l->phaseOffset=s->offset;
    l->freq=s->frequency;l->modNodeValue=s->multiplier;l->rnd=s->random;
    l->modTarget.amount=s->amount;l->modTarget.lastVal=s->held;
    l->waveform=s->waveform;l->retrigger=s->retrigger;l->sync=s->sync;
    /* Route changes must go through the actual original function. */
}
void lfo_ref_init(unsigned v){lfo_init(lfo_ref_owner(v));}
float lfo_ref_calc(unsigned v){return lfo_calc(lfo_ref_owner(v));}
void lfo_ref_dispatch(unsigned v){lfo_dispatchNextValue(lfo_ref_owner(v));}
void lfo_ref_modulate(unsigned v,float x){modNode_updateValue(&lfo_ref_owner(v)->modTarget,x);}
void lfo_ref_route(unsigned v,unsigned p){modNode_setDestination(&lfo_ref_owner(v)->modTarget,p);}
void lfo_ref_velocity_route(unsigned v,unsigned p){modNode_setDestination(&velocityModulators[v],p);}
void lfo_ref_rate(unsigned v,float raw){lfo_setFreq(lfo_ref_owner(v),raw);}
uint32_t lfo_ref_increment(float f,unsigned s){return lfo_calcPhaseInc(f,s);}
void lfo_ref_sync(unsigned v,unsigned s){lfo_setSync(lfo_ref_owner(v),s);}
void lfo_ref_tempo(unsigned bpm){test_bpm=bpm;lfo_recalcSync();}
void lfo_ref_retrigger(unsigned t){lfo_retrigger(t);}
void lfo_ref_reset_targets(void){modNode_resetTargets();}
void lfo_ref_replay_velocity(void){modNode_reassignVeloMod();}
void lfo_ref_tick(int16_t* a,int16_t* b){mixer_calcNextSampleBlock(a,b);}
unsigned lfo_ref_type(unsigned p){return parameterArray[p].type;}
unsigned lfo_ref_has_field(unsigned p){return parameterArray[p].ptr!=0;}
float lfo_ref_field(unsigned p){
    Parameter* q=&parameterArray[p];if(!q->ptr)return 0;
    return q->type==TYPE_UINT8?*(uint8_t*)q->ptr:q->type==TYPE_UINT32?(float)*(uint32_t*)q->ptr:*(float*)q->ptr;
}
void lfo_ref_write(unsigned p,float x,unsigned notify){
    ptrValue v;v.flt=x;
    if(parameterArray[p].type==TYPE_UINT8||parameterArray[p].type==TYPE_UINT32)v.itg=x;
    paramArray_setParameter(p,v);if(notify)modNode_originalValueChanged(p);
}
float lfo_ref_saved(unsigned v,unsigned velocity){
    ModulationNode* n=velocity?&velocityModulators[v]:&lfo_ref_owner(v)->modTarget;
    return parameterArray[n->destination].type==TYPE_UINT8||parameterArray[n->destination].type==TYPE_UINT32?
        (float)n->originalValue.itg:n->originalValue.flt;
}
int16_t lfo_ref_sine(unsigned i){return sine_table[i];}
void lfo_ref_mixer_init(void){mixer_init();}
