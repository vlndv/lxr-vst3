/* Independent oracle: unchanged original AVR source is included via -I.
 * Copyright 2013 Julian Schmidt (included firmware). See LICENSE-LXR.txt.
 * No generated ModulationContractData file participates in this translation unit.
 */
#include "original_contract_fixture.h"
#include "Cc2Text.c"

unsigned lxr_ref_target_count(void) { return sizeof(modTargets) / sizeof(modTargets[0]); }
unsigned lxr_ref_parameter(unsigned i) { return i < lxr_ref_target_count() ? modTargets[i].param : 0xffffu; }
unsigned lxr_ref_label(unsigned i) { return i < lxr_ref_target_count() ? modTargets[i].nameIdx : 0xffffu; }
unsigned lxr_ref_voice(unsigned i) { return i < lxr_ref_target_count() ? voiceFromModTargValue((uint8_t)i) : 0xffffu; }
unsigned lxr_ref_range_start(unsigned v) { return v < 6 ? modTargetVoiceOffsets[v].start : 0xffffu; }
unsigned lxr_ref_range_end(unsigned v) { return v < 6 ? modTargetVoiceOffsets[v].end : 0xffffu; }
unsigned lxr_ref_gap_count(void) { return sizeof(modTargetGapMap); }
unsigned lxr_ref_gap(unsigned i) { return i < sizeof(modTargetGapMap) ? modTargetGapMap[i] : 0xffffu; }
unsigned lxr_ref_logical_slot(unsigned i) { return i < lxr_ref_target_count() ? getModTargetGapIndex((uint8_t)i) : 0xffffu; }
unsigned lxr_ref_remap(unsigned v, unsigned i) {
    return v < 6 && i < sizeof(modTargetGapMap) ? getModTargetIdxFromGapIdx((uint8_t)v, (uint8_t)i) : 0xffffu;
}
