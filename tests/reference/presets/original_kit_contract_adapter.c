/* Original firmware Copyright 2013 Julian Schmidt; see LICENSE-LXR.txt.
 * Reuses P17-A unchanged writers/defaults/sender, not generated C++ mappings.
 * Original parameter_dtypes declaration is extracted unchanged by the runner;
 * menu enumerations and menu-name counts are compiled from original headers.
 */
#include "original_kit_contract_adapter.h"
#include "original_preset_adapter.c"
#include P17_ORIGINAL_MENU_TEXT
#include P17_ORIGINAL_KIT_DTYPES

unsigned p17b_original_dtype(unsigned id) {
    return id < END_OF_SOUND_PARAMETERS ? (unsigned)parameter_dtypes[id] : 65535u;
}
unsigned p17b_original_builtin_wave_count(void) { return waveformNames[0][0]; }
unsigned p17b_original_maximum(unsigned id) {
    if (id >= END_OF_SOUND_PARAMETERS) return 65535u;
    unsigned dtype = parameter_dtypes[id];
    switch (dtype & 15u) {
    case DTYPE_TARGET_SELECTION_LFO:
    case DTYPE_TARGET_SELECTION_VELO: return getNumModTargets() - 1u;
    case DTYPE_VOICE_LFO: return 6;
    case DTYPE_ON_OFF:
    case DTYPE_MIX_FM: return 1;
    case DTYPE_MENU:
        switch (dtype >> 4) {
        case MENU_FILTER: return filterTypes[0][0] - 1u;
        case MENU_WAVEFORM: return waveformNames[0][0] - 1u; /* No external SD assets. */
        case MENU_AUDIO_OUT: return outputNames[0][0] - 1u;
        case MENU_SYNC_RATES: return syncRateNames[0][0] - 1u;
        case MENU_LFO_WAVES: return lfoWaveNames[0][0] - 1u;
        case MENU_RETRIGGER: return retriggerNames[0][0] - 1u;
        case MENU_TRANS: return transientNames[0][0] - 1u;
        default: abort();
        }
    default: return 127;
    }
}
unsigned p17b_original_owner(unsigned id) {
    /* Source ownership from compiled original table/ranges. Missing special
     * fields use actual original enum blocks, never the generated KitFields. */
    unsigned v, i;
    for (v = 0; v < 6; ++v)
        for (i = modTargetVoiceOffsets[v].start; i <= modTargetVoiceOffsets[v].end; ++i)
            if (modTargets[i].param == id) return v;
    if (id == PAR_VOICE_DECIMATION_ALL) return 6;
    if (id >= PAR_VOICE_LFO1 && id <= PAR_VOICE_LFO6) return id - PAR_VOICE_LFO1;
    if (id >= PAR_TARGET_LFO1 && id <= PAR_TARGET_LFO6) return id - PAR_TARGET_LFO1;
    if (id >= PAR_AUDIO_OUT1 && id <= PAR_AUDIO_OUT6) return id - PAR_AUDIO_OUT1;
    if (id == PAR_MIDI_NOTE7) return 5;
    return 7;
}
