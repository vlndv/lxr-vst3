#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "SlopeEg2.h"
#include "Decay.h"
#include "distortion.h"
#include "valueShaper.h"
#define PITCH_AMOUNT_FACTOR 32
#define SEQ_DEFAULT_NOTE 63
#define LFO_MAX_F 200
#define NUM_TRANSIENTS 12
#define TIME_AMOUNT_DECAY 0.999f
#define TIME_AMOUNT_ATTACK 0.99f
#define TIME_K_ATTACK_X (2*TIME_AMOUNT_ATTACK/(1.f-TIME_AMOUNT_ATTACK))
#define TIME_K_DECAY_X (2*TIME_AMOUNT_DECAY/(1.f-TIME_AMOUNT_DECAY))
#define SEMITONE_UP 1.0594630943592952645618252949463f
typedef struct { float freq; uint32_t phaseInc; uint8_t sync; } Lfo;
static uint32_t lfo_calcPhaseInc(float f, uint8_t s){ (void)f;(void)s; return 0; }
typedef struct { uint16_t midiFreq; uint8_t baseNote; float freq; } OscInfo;
typedef struct { uint8_t waveform; } TransientGenerator;
static float MidiNoteFrequencies[128];
static inline float calcPitchModAmount(uint8_t data2)
{
	const float val = data2/127.f;
	return val*val*PITCH_AMOUNT_FACTOR;
}
float midiParser_calcDetune(uint8_t value)
{
	//linear interpolation between 1(no change) and semitone up/down)
	float frac = (value/127.f -0.5f);
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
void lfo_setFreq(Lfo *lfo, float f)
{
	f += 1;
	f = f/128.f;
	f = f*f*f;
	lfo->freq = f*LFO_MAX_F;
	lfo->phaseInc = lfo_calcPhaseInc(lfo->freq,lfo->sync);
}
void osc_recalcFreq(OscInfo* osc)
 {
	 //get fine tune
	 const float cent = midiParser_calcDetune(osc->midiFreq&0xff);
	 //calc coarse tune
	 int16_t note =  (osc->midiFreq>>8) + (osc->baseNote-SEQ_DEFAULT_NOTE);

	 if(note>127)note=127;
 	 if(note<0)note=0;

	 osc->freq = MidiNoteFrequencies[note]*cent;
 }
void transient_setWaveform(TransientGenerator* transient, const uint8_t waveform)
{
	if(waveform < NUM_TRANSIENTS + 2)
		transient->waveform = waveform;
	else
		transient->waveform = 0;
}
static const uint8_t V[] = {0,1,2,10,32,63,64,65,96,126,127};
int main(void){
  for(unsigned k=0;k<sizeof V;k++){ uint8_t v=V[k];
    SlopeEg2 eg; memset(&eg,0,sizeof eg); slopeEg2_setAttack(&eg,v,0); float att=eg.attack; slopeEg2_setDecay(&eg,v,0); float dec=eg.decay;
    slopeEg2_setSlope(&eg,v);
    DecayEg pe; memset(&pe,0,sizeof pe); DecayEg_setDecay(&pe,v); DecayEg_setSlope(&pe,v);
    Distortion di; memset(&di,0,sizeof di); setDistortionShape(&di,v);
    printf("v=%d egA=%.9g egD=%.9g calcDecay=%.9g slopeA=%.9g invSlopeA=%.9g pDecay=%.9g slopeP=%.9g pAmt=%.9g\n",v,att,dec,slopeEg2_calcDecay(v),eg.slope,eg.invSlope,pe.decay,pe.slope,calcPitchModAmount(v));
    Lfo l; memset(&l,0,sizeof l); lfo_setFreq(&l,(float)v);
    float fv=v/127.f;
    printf("v=%d cutoffShaped=%.9g decim=%.9g dist=%.9g noiseF=%.9g lfoHz=%.9g trPitch=%.9g detune=%.9g\n",v,valueShaperF2F(fv,-0.9f),valueShaperI2F(v,-0.7f),di.shape,v/127.f*22000,l.freq,1.f + ((v/33.9f)-0.75f),midiParser_calcDetune(v));
    uint32_t off=(uint32_t)(v/127.f * 0xffffffff); if(v<127) printf("v=%d phaseOffset=%u\n",v,off);
    TransientGenerator t; transient_setWaveform(&t,v); printf("v=%d transWave=%d\n",v,t.waveform);
  }
  for(int i=0;i<128;i++) MidiNoteFrequencies[i]=1000.f+i;
  { uint16_t mf[]={ (63<<8)|64, (0<<8), (127<<8)|127, (100<<8)|0, (10<<8)|127, (127<<8)|0, (0<<8)|64 }; uint8_t bn[]={63,63,63,90,40,90,40};
    for(int k=0;k<7;k++){ OscInfo o; o.midiFreq=mf[k]; o.baseNote=bn[k]; o.freq=0; osc_recalcFreq(&o); printf("osc midiFreq=0x%04x base=%d freq=%.9g\n",mf[k],bn[k],o.freq);} }
  { int W[]={0,1,12,13,14,15,127}; for(int k=0;k<7;k++){ TransientGenerator t; transient_setWaveform(&t,W[k]); printf("tw v=%d -> %d\n",W[k],t.waveform);} }
  printf("K_att=%.9g K_dec=%.9g\n",(double)TIME_K_ATTACK_X,(double)TIME_K_DECAY_X);
  printf("SEMITONE_UP-1=%.9g\n",(double)(SEMITONE_UP-1));
  return 0; }
