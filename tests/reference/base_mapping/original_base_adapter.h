#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BaseState {
    float freq[3], pitch[3], gain[2];
    uint32_t phase[3], increment[3];
    uint16_t midi[3];
    uint8_t base[3];
    float cutoff, coefficient, resonance;
    float envelope, envelopeOutput, lastGain, targetGain;
    int envelopeState, repeatCount;
} BaseState;
int ref_snare_load_assets(const char*);
void base_ref_reset(uint32_t seed);
void base_ref_set(int id, int value);
void base_ref_trigger(int voice, int velocity, int note, int open);
void base_ref_async(int voice);
void base_ref_render(int voice, int16_t* output);
void base_ref_operator(int voice, int which, int16_t* output);
void base_ref_filter(int voice, int16_t* samples);
void base_ref_state(int voice, BaseState* output);
void base_ref_envelope(int voice, int state, float value, int repeat);
void base_ref_phase(int voice, int op, uint32_t phase);
void base_ref_checkpoint(void);
void base_ref_restore_checkpoint(void);
#ifdef __cplusplus
}
#endif
