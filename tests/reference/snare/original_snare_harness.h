#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RefSnareConfig {
    uint8_t tonal_waveform;
    uint16_t tonal_midi_freq;
    float noise_frequency;
    float mix;
    float volume;
    uint8_t volume_mod;
    uint8_t filter_type;
    float filter_frequency;
    float filter_resonance;
    float filter_drive;
    float amp_attack;
    float amp_decay;
    float amp_slope;
    float amp_inv_slope;
    uint8_t amp_repeat;
    float pitch_decay;
    float pitch_slope;
    float pitch_amount;
    uint8_t transient_waveform;
    float transient_volume;
    float transient_pitch;
    uint8_t distortion_shape;
} RefSnareConfig;

int ref_snare_load_assets(const char* data_dir);
void ref_snare_reset(uint32_t rng_seed);
void ref_snare_configure(const RefSnareConfig* config);
void ref_snare_trigger(uint8_t velocity, uint8_t note);
void ref_snare_process_block(int16_t* output, uint8_t size);
float ref_snare_envelope_value(void);
float ref_snare_tonal_frequency(void);
float ref_snare_noise_frequency(void);
uint32_t ref_snare_rng_state(void);

#ifdef __cplusplus
}
#endif
