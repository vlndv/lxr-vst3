/*
 * Independent host harness for the unmodified Sonic Potions LXR 0.37 DSP C
 * sources at ref/LXR-0.37 (commit dee4968). The firmware source files are
 * included directly below; this file only supplies Cortex-M4 intrinsics,
 * hardware services, assets, and a small C ABI for differential tests.
 */
#include "original_snare_harness.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SNARE_H_
#define OSCILLATOR_H_
#define RESONANTFILTER_H_
#define DISTORTION_H_
#define DECAY_H_
#define SLOPEEG2_H_
#define SNAPEG_H_
#define TRANSIENTGENERATOR_H_
#define TRANSIENTTABLES_H_
#define BUFFERTOOLS_H_
#define LFO_H_
#define RANDOM_H_
#define SAMPLES_H_
#define SQUAREROOTLUT_H_
#define VELOCITYMODULATION_H_
#define MIDIPARSER_H_
#define MIDINOTENUMBERS_H_
#define SEQUENCER_H_
#define SAMPLEMEMORY_H_
#define TRIGGEROUT_H_

#define INCCMZ
#define INCCM
#define TABLESIZE 4096
#define REAL_FS 44002.7573529412f
#define INTERPOLATE_OSC 1
#define INTERPOLATE_FM_OSC 1
#define NUM_TRANSIENTS 12
#define TRANSIENT_SAMPLE_LENGTH 2205
#define SEQ_DEFAULT_NOTE 63
#define SEMITONE_UP 1.0594630943592952645618252949463f

#define SINE 0
#define TRI 1
#define SAW 2
#define REC 3
#define NOISE 4
#define CRASH 5
#define OSC_SAMPLE_START 6

#define FILTER_LP 1
#define FILTER_HP 2
#define FILTER_BP 3
#define FILTER_UNITY_BP 4
#define FILTER_NOTCH 5
#define FILTER_PEAK 6
#define FILTER_NAIVE_2_POLE 7
#define FILTER_GAIN 0x70ff
#define ENABLE_NONLINEAR_INTEGRATORS 1
#define USE_SHAPER_NONLINEARITY 0

#define EG_STOPPED 0
#define EG_A 1
#define EG_D 2
#define EG_REPEAT 3

#define TRIGGER_4 3
#define TRIGGER_OFF 0

typedef struct OscStruct {
    int16_t output;
    uint32_t phaseInc;
    uint32_t phase;
    float freq;
    uint8_t waveform;
    uint8_t tableOffset;
    float pitchMod;
    float fmMod;
    float modNodeValue;
    uint16_t midiFreq;
    uint8_t baseNote;
    uint32_t startPhase;
} OscInfo;

typedef struct ResoFilterStruct {
    float f, g, q, s1, s2, a, b, zi, drive;
} ResonantFilter;

typedef struct DistStruct { float shape, inv_shape; } Distortion;
typedef struct Decay_EG_Struct { float decay, slope, value; } DecayEg;
typedef struct SLOPE_EG2_Struct {
    float attack, decay, slope, invSlope;
    uint8_t repeat, repeatCnt;
    float value;
    uint8_t state;
} SlopeEg2;
typedef struct Snap_EG_Struct { float value; } SnapEg;
typedef struct TransientGeneratorStruct {
    int16_t output;
    uint32_t phase;
    float pitch;
    uint8_t waveform;
    float volume;
    uint16_t midiFreq;
} TransientGenerator;
typedef struct LfoStruct { uint32_t unused; } Lfo;
typedef struct ModulationNodeStruct { float value; } ModulationNode;
typedef struct SampleInfoStruct { char name[3]; uint16_t size; uint32_t offset; } SampleInfo;

typedef struct SnareStruct {
    OscInfo osc;
    OscInfo noiseOsc;
    uint8_t filterType;
    float vol;
    uint8_t pan;
    float mix;
    float velo;
    int16_t oscSample;
    int32_t noiseSample;
    ResonantFilter filter;
    DecayEg oscPitchEg;
    float egPitchModAmount;
    Lfo lfo;
    TransientGenerator transGen;
    SlopeEg2 oscVolEg;
    float egValueOscVol;
    Distortion distortion;
    SnapEg snapEg;
    uint8_t volumeMod;
} SnareVoice;

typedef union APSR_Type {
    uint32_t w;
    struct { uint32_t pad : 28; uint32_t V : 1; uint32_t rest : 3; } b;
} APSR_Type;

/* Prototypes normally supplied by the firmware headers whose hardware-heavy
 * contents are intentionally suppressed above. */
void slopeEg2_setSlope(SlopeEg2* eg, uint8_t data2);
void SVF_directSetFilterValue(ResonantFilter* filter, float value);
void calcNoiseBlock(OscInfo* osc, int16_t* buf, uint8_t size, float gain);
int16_t calcNoise(OscInfo* osc);
void calcWavetableOscBlock(OscInfo* osc, const int16_t table[][1024],
                           int16_t* buf, uint8_t size, float gain);
int16_t calcWavetableOsc(OscInfo* osc, const int16_t table[][1024]);
void calcSampleOscFmBlock(OscInfo* osc, int16_t* mod, int16_t* buf,
                          uint8_t size, float gain);
int16_t calcSampleOscFm(OscInfo* osc, OscInfo* mod);
void calcUserSampleOscFmBlock(OscInfo* osc, int16_t* mod, int16_t* buf,
                              uint8_t size, float gain);
void calcUserSampleOscBlock(OscInfo* osc, int16_t* buf, uint8_t size,
                            float gain);
void calcSampleOscBlock(OscInfo* osc, int16_t* buf, uint8_t size, float gain);
int16_t calcSampleOsc(OscInfo* osc);

int16_t sine_table[4097];
int16_t sawTable[11][1024];
int16_t triTable[11][1024];
int16_t recTable[11][1024];
uint8_t crashSample[32769];
float MidiNoteFrequencies[128];
float transientVolumeTable[69];
int8_t transientData[NUM_TRANSIENTS][TRANSIENT_SAMPLE_LENGTH];
ModulationNode velocityModulators[6];

static uint32_t g_rng_state = 0x9E3779B9u;
static uint32_t GetRngValue(void) {
    uint32_t x = g_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng_state = x;
    return x;
}

static uint32_t ref_clz(uint32_t value) {
    uint32_t count = 0;
    if (!value) return 32;
    while (!(value & 0x80000000u)) { value <<= 1; ++count; }
    return count;
}
#define __CLZ(v) ref_clz((uint32_t)(v))
static uint32_t __get_APSR(void) { return 0; }
static int32_t ref_ssat(int32_t value, unsigned bits) {
    const int32_t hi = (int32_t)((1u << (bits - 1)) - 1u);
    const int32_t lo = -hi - 1;
    return value > hi ? hi : value < lo ? lo : value;
}
#define __SSAT(v, bits) ref_ssat((int32_t)(v), (bits))
#define __QADD16(a, b) ((int16_t)ref_ssat((int32_t)(int16_t)(a) + (int32_t)(int16_t)(b), 16))

static float midiParser_calcDetune(uint8_t value) {
    const float frac = value / 127.f - 0.5f;
    return 1.f + frac * (SEMITONE_UP - 1.f);
}
static uint8_t sampleMemory_getNumSamples(void) { return 0; }
static SampleInfo sampleMemory_getSampleInfo(uint8_t index) {
    (void)index;
    SampleInfo info = {{0, 0, 0}, 0, 0};
    return info;
}
static void lfo_init(Lfo* lfo) { lfo->unused = 0; }
static void lfo_retrigger(uint8_t voice) { (void)voice; }
static void modNode_updateValue(ModulationNode* node, float value) { node->value = value; }
static int trigger_isGateModeOn(void) { return 0; }
static void trigger_triggerVoice(int voice, int state) { (void)voice; (void)state; }
static void voiceControl_noteOff(int voice) { (void)voice; }
static void bufferTool_addBuffersSaturating(int16_t* lhs, int16_t* rhs, uint8_t size) {
    uint8_t i;
    for (i = 0; i < size; ++i) lhs[i] = __QADD16(lhs[i], rhs[i]);
}

/* Exact firmware implementations, compiled unchanged from the reference tree. */
#include "../../../ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio/Decay.c"
#include "../../../ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio/SlopeEg2.c"
#include "../../../ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio/snapEg.c"
#include "../../../ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio/distortion.c"
#include "../../../ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio/ResonantFilter.c"
#include "../../../ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio/transientGenerator.c"
#include "../../../ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio/Oscillator.c"
#include "../../../ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio/Snare.c"

static int load_file(const char* dir, const char* name, void* dst, size_t bytes) {
    char path[512];
    FILE* file;
    if (snprintf(path, sizeof(path), "%s/%s", dir, name) <= 0) return 0;
    file = fopen(path, "rb");
    if (!file) return 0;
    const size_t count = fread(dst, 1, bytes, file);
    fclose(file);
    return count == bytes;
}

int ref_snare_load_assets(const char* data_dir) {
    memset(crashSample, 127, sizeof(crashSample));
    return load_file(data_dir, "sine_table.bin", sine_table, 4097u * sizeof(int16_t)) &&
           load_file(data_dir, "saw_table.bin", sawTable, sizeof(sawTable)) &&
           load_file(data_dir, "tri_table.bin", triTable, sizeof(triTable)) &&
           load_file(data_dir, "rec_table.bin", recTable, sizeof(recTable)) &&
           load_file(data_dir, "crash_sample.bin", crashSample, 32768u) &&
           load_file(data_dir, "midi_note_frequencies.bin", MidiNoteFrequencies,
                     sizeof(MidiNoteFrequencies)) &&
           load_file(data_dir, "transient_volume_table.bin", transientVolumeTable,
                     sizeof(transientVolumeTable)) &&
           load_file(data_dir, "transient_data.bin", transientData, sizeof(transientData));
}

void ref_snare_reset(uint32_t rng_seed) {
    memset(&snareVoice, 0, sizeof(snareVoice));
    memset(velocityModulators, 0, sizeof(velocityModulators));
    g_rng_state = rng_seed;
    Snare_init();
}

void ref_snare_configure(const RefSnareConfig* c) {
    snareVoice.osc.waveform = c->tonal_waveform;
    snareVoice.osc.midiFreq = c->tonal_midi_freq;
    snareVoice.noiseOsc.freq = c->noise_frequency;
    snareVoice.mix = c->mix;
    snareVoice.vol = c->volume;
    snareVoice.volumeMod = c->volume_mod;
    snareVoice.filterType = c->filter_type;
    snareVoice.filter.f = c->filter_frequency;
    snareVoice.filter.q = c->filter_resonance;
    snareVoice.filter.drive = c->filter_drive;
    SVF_recalcFreq(&snareVoice.filter);
    snareVoice.oscVolEg.attack = c->amp_attack;
    snareVoice.oscVolEg.decay = c->amp_decay;
    snareVoice.oscVolEg.slope = c->amp_slope;
    snareVoice.oscVolEg.invSlope = c->amp_inv_slope;
    snareVoice.oscVolEg.repeat = c->amp_repeat;
    snareVoice.oscPitchEg.decay = c->pitch_decay;
    snareVoice.oscPitchEg.slope = c->pitch_slope;
    snareVoice.egPitchModAmount = c->pitch_amount;
    snareVoice.transGen.waveform = c->transient_waveform;
    snareVoice.transGen.volume = c->transient_volume;
    snareVoice.transGen.pitch = c->transient_pitch;
    setDistortionShape(&snareVoice.distortion, c->distortion_shape);
}

void ref_snare_trigger(uint8_t velocity, uint8_t note) {
    Snare_trigger(velocity, note);
}

void ref_snare_process_block(int16_t* output, uint8_t size) {
    Snare_calcAsync();
    Snare_calcSyncBlock(output, size);
}

float ref_snare_envelope_value(void) { return snareVoice.egValueOscVol; }
float ref_snare_tonal_frequency(void) { return snareVoice.osc.freq; }
float ref_snare_noise_frequency(void) { return snareVoice.noiseOsc.freq; }
uint32_t ref_snare_rng_state(void) { return g_rng_state; }
