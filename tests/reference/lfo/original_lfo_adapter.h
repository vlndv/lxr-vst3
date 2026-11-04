#pragma once
/* Test ABI for unchanged Sonic Potions LXR 0.37 C. LICENSE-LXR.txt applies. */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct LfoReferenceState {
    uint32_t phase, increment, offset;
    float frequency, multiplier, random, amount, held;
    unsigned waveform, retrigger, sync, destination;
} LfoReferenceState;
void lfo_ref_get(unsigned owner, LfoReferenceState* state);
void lfo_ref_put(unsigned owner, const LfoReferenceState* state);
void lfo_ref_init(unsigned owner);
float lfo_ref_calc(unsigned owner);
void lfo_ref_dispatch(unsigned owner);
void lfo_ref_modulate(unsigned owner, float value);
void lfo_ref_route(unsigned owner, unsigned parameter);
void lfo_ref_velocity_route(unsigned owner, unsigned parameter);
void lfo_ref_rate(unsigned owner, float raw);
uint32_t lfo_ref_increment(float frequency, unsigned sync);
void lfo_ref_sync(unsigned owner, unsigned sync);
void lfo_ref_tempo(unsigned bpm);
void lfo_ref_retrigger(unsigned track);
void lfo_ref_reset_targets(void);
void lfo_ref_replay_velocity(void);
void lfo_ref_tick(int16_t* output1, int16_t* output2);
float lfo_ref_field(unsigned parameter);
void lfo_ref_write(unsigned parameter, float value, unsigned notify);
unsigned lfo_ref_type(unsigned parameter);
unsigned lfo_ref_has_field(unsigned parameter);
float lfo_ref_saved(unsigned owner, unsigned velocity);
int16_t lfo_ref_sine(unsigned index);
void lfo_ref_mixer_init(void);
#ifdef __cplusplus
}
#endif
