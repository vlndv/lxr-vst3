/* Reference values for P4 (oscillators): drives the ORIGINAL Oscillator.c (patched only for ARM float->uint32 semantics, see make_ref.sh). */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "Oscillator.h"
#include "MidiNoteNumbers.h"
#include "Samples.h"
/* xorshift32, same generator as lxr::OscRng in the port (replaces the hardware RNG) */
static uint32_t rng_state = 0x9E3779B9u;
uint32_t GetRngValue(void){ uint32_t x = rng_state; x ^= x << 13; x ^= x >> 17; x ^= x << 5; rng_state = x; return x; }
/* verbatim from MidiParser.c */
#define SEMITONE_UP 1.0594630943592952645618252949463f
float midiParser_calcDetune(uint8_t value)
{
  float frac = (value/127.f-0.5f);
  float cent = 1;
  if(cent>=0)
  {
    cent += frac*(SEMITONE_UP - 1);
  }
  else
  {
    cent += frac*(SEMITONE_UP - 1);
  }
  return cent;
}
typedef struct { const char* name; uint8_t wave; float freq; float pitchMod; float modNode; float fmMod; uint32_t phase; float gain; int fm; } Case;
static uint32_t fnv(uint32_t h, int16_t v){ h ^= (uint8_t)(v & 0xff); h *= 16777619u; h ^= (uint8_t)((v >> 8) & 0xff); h *= 16777619u; return h; }
static void mkmod(int16_t* m, int n, int blk){ for(int i=0;i<n;i++) m[i] = (int16_t)((((i + blk*32)*4099 + 7) % 2001 - 1000) * 30); }
static void runCase(const Case* c){
  OscInfo o; memset(&o, 0, sizeof o);
  o.waveform = c->wave; o.freq = c->freq; o.pitchMod = c->pitchMod; o.modNodeValue = c->modNode; o.fmMod = c->fmMod; o.phase = c->phase;
  osc_setFreq(&o);
  rng_state = 0x9E3779B9u;
  static const int sizes[3] = {32, 32, 20};
  int16_t all[84]; int n = 0;
  for(int b = 0; b < 3; b++){
    int16_t buf[64], mod[64]; mkmod(mod, sizes[b], b); memset(buf, 0x55, sizeof buf);
    if(c->fm) calcNextOscSampleFmBlock(&o, mod, buf, sizes[b], c->gain); else calcNextOscSampleBlock(&o, buf, sizes[b], c->gain);
    for(int i = 0; i < sizes[b]; i++) all[n++] = buf[i];
  }
  uint32_t h = 2166136261u; for(int i = 0; i < n; i++) h = fnv(h, all[i]);
  printf("R %s hash=%u o0=%d o1=%d o2=%d o3=%d o40=%d olast=%d phase=%u output=%d phaseInc=%u table=%d\n", c->name, h, all[0], all[1], all[2], all[3], all[40], all[n-1], o.phase, o.output, o.phaseInc, o.tableOffset);
}
int main(void){
  static const Case cases[] = {
    /* name        wave freq     pMod  mNode fmMod phase        gain fm */
    {"sine100",     0,  100.f,   1.f, 1.f, 0.f, 0u,            1.0f, 0},
    {"sine1000st",  0, 1000.f,   1.f, 1.f, 0.f, 1023u<<20,     0.5f, 0},
    {"sine10000",   0,10000.f,   1.f, 1.f, 0.f, 12345678u,     1.0f, 0},
    {"sinePitch",   0,  200.f,   2.5f,0.8f,0.f, 777u,          1.0f, 0},
    {"tri55",       1,   55.f,   1.f, 1.f, 0.f, 0xffu<<20,     1.0f, 0},
    {"tri440",      1,  440.f,   1.f, 1.f, 0.f, 0u,            1.0f, 0},
    {"tri1000",     1, 1000.f,   1.f, 1.f, 0.f, 5000000u,      0.7f, 0},
    {"saw100",      2,  100.f,   1.f, 1.f, 0.f, 0xffu<<20,     1.0f, 0},
    {"saw3000",     2, 3000.f,   1.f, 1.f, 0.f, 0u,            1.0f, 0},
    {"saw8000",     2, 8000.f,   1.f, 1.f, 0.f, 123456u,       1.0f, 0},
    {"saw20000",    2,20000.f,   1.f, 1.f, 0.f, 42u,           1.0f, 0},
    {"rec220",      3,  220.f,   1.f, 1.f, 0.f, 0xffu<<20,     1.0f, 0},
    {"rec9000",     3, 9000.f,   1.f, 1.f, 0.f, 500000u,       1.0f, 0},
    {"noise1000",   4, 1000.f,   1.f, 1.f, 0.f, 0u,            0.9f, 0},
    {"noise12000",  4,12000.f,   1.f, 1.f, 0.f, 0u,            1.0f, 0},
    {"crash500",    5,  500.f,   1.f, 1.f, 0.f, 0u,            1.0f, 0},
    {"crash2000",   5, 2000.f,   1.f, 1.f, 0.f, 1000000u,      1.0f, 0},
    {"fmSine200",   0,  200.f,   1.f, 1.f, 0.5f,0u,            1.0f, 1},
    {"fmSine1500",  0, 1500.f,   1.f, 1.f, 1.0f,555u,          0.7f, 1},
    {"fmTri100",    1,  100.f,   1.f, 1.f, 0.3f,0xffu<<20,     1.0f, 1},
    {"fmSaw400",    2,  400.f,   1.f, 1.f, 1.0f,0u,            1.0f, 1},
    {"fmRec2500",   3, 2500.f,   1.f, 1.f, 0.8f,31337u,        1.0f, 1},
    {"fmCrash800",  5,  800.f,   1.f, 1.f, 0.5f,0u,            1.0f, 1},
    {"fmNoise",     4, 3000.f,   1.f, 1.f, 0.5f,0u,            1.0f, 1},
  };
  for(unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) runCase(&cases[i]);
  /* frequency to phase increment / table offset */
  static const float fr[] = {0.f, 20.f, 100.f, 439.9f, 440.f, 879.9f, 880.f, 1760.f, 3520.f, 7040.f, 14080.f, 20000.f, 30000.f, 44000.f};
  static const uint8_t wv[] = {0, 2, 5};
  for(unsigned w = 0; w < 3; w++) for(unsigned k = 0; k < sizeof fr / sizeof fr[0]; k++){
    OscInfo o; memset(&o, 0, sizeof o); o.waveform = wv[w]; o.freq = fr[k]; o.pitchMod = 1.f; o.modNodeValue = 1.f; osc_setFreq(&o);
    printf("F %d %.9g phaseInc=%u table=%d\n", wv[w], fr[k], o.phaseInc, o.tableOffset);
  }
  /* base note / detune */
  static const uint16_t mf[] = {(63<<8)|64, (0<<8), (127<<8)|127, (100<<8)|0, (10<<8)|127, (70<<8)|33};
  static const uint8_t bn[] = {63, 63, 63, 90, 40, 80};
  for(unsigned k = 0; k < 6; k++){ OscInfo o; memset(&o, 0, sizeof o); o.midiFreq = mf[k]; osc_setBaseNote(&o, bn[k]); printf("B %u %d freq=%.9g base=%d\n", mf[k], bn[k], o.freq, o.baseNote);
    OscInfo p; memset(&p, 0, sizeof p); p.midiFreq = mf[k]; p.baseNote = bn[k]; osc_recalcFreq(&p); if(p.freq != o.freq) printf("MISMATCH recalc\n"); }
  /* host-only information for the differential test: values that the original reads one element past its arrays */
  printf("G saw=%d tri=%d rec=%d crash=%d\n", ((const int16_t*)sawTable)[11*1024], ((const int16_t*)triTable)[11*1024], ((const int16_t*)recTable)[11*1024], crashSample[32768]);
  return 0;
}
