/* Test-only stage boundary observation. Unchanged algorithms execute in the
 * original C functions; order is mixer_calcNextSampleBlock (mixer.c:287-307).
 * Original Copyright 2013 Julian Schmidt; see LICENSE-LXR.txt. */
#include "original_lfo_runtime_adapter.h"
void runtime_ref_tick(void) {
    modNode_resetTargets();
    modNode_reassignVeloMod();
    for(unsigned v=0;v<6;++v) lfo_dispatchNextValue(lfo_ref_owner(v));
    for(unsigned v=0;v<3;++v) SVF_recalcFreq(&voiceArray[v].filter);
    SVF_recalcFreq(&snareVoice.filter);
    SVF_recalcFreq(&cymbalVoice.filter);
    SVF_recalcFreq(&hatVoice.filter);
}
void runtime_ref_rng(uint32_t seed) {g_rng_state=seed;}
