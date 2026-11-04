/* Included after unchanged STM32 C and the established base mapping shim.
 * Original Copyright 2013 Julian Schmidt; derived material LICENSE-LXR.txt. */
#include "original_velocity_adapter.h"
extern unsigned lxr_ref_parameter(unsigned index);
void velocity_ref_route(unsigned owner,unsigned index) {
    modNode_setDestination(&velocityModulators[owner],lxr_ref_parameter(index));
}
void velocity_ref_tick(void) {
    modNode_resetTargets();modNode_reassignVeloMod();
    /* LFO routes are off. Preserve the original subsequent coefficient stage. */
    for(int v=0;v<3;v++)SVF_recalcFreq(&voiceArray[v].filter);
    SVF_recalcFreq(&snareVoice.filter);SVF_recalcFreq(&cymbalVoice.filter);SVF_recalcFreq(&hatVoice.filter);
}
float velocity_ref_field(unsigned p) {
    const Parameter* value=&parameterArray[p];
    return value->type==TYPE_UINT8?*((uint8_t*)value->ptr):*((float*)value->ptr);
}
unsigned velocity_ref_type(unsigned p){return parameterArray[p].type;}
void velocity_ref_node(unsigned owner,float* amount,float* held,unsigned* dest){
    *amount=velocityModulators[owner].amount;*held=velocityModulators[owner].lastVal;*dest=velocityModulators[owner].destination;
}
