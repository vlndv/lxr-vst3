// dsp/ParamMapMisc.h
#pragma once
#include <cstdint>
namespace lxr {
constexpr uint8_t kSeqDefaultNote = 63;   // SEQ_DEFAULT_NOTE
float    unitFromParam(uint8_t v);         // v/127.f
float    cutoffShape(uint8_t v);           // valueShaperF2F(v/127.f, FILTER_SHAPER)
float    decimationRate(uint8_t v);        // valueShaperI2F(v, -0.7f)
float    distortionShape(uint8_t v);       // setDistortionShape
float    noiseFrequencyHz(uint8_t v);      // v/127.f*22000
float    transientPitch(uint8_t v);        // 1.f + ((v/33.9f)-0.75f)
uint8_t  transientWaveform(uint8_t v);     // transient_setWaveform
float    lfoFrequencyHz(uint8_t v);        // lfo_setFreq, frequency only
uint32_t lfoPhaseOffset(uint8_t v);        // v/127.f * 0xffffffff
uint8_t  filterTypeFromParam(uint8_t v);   // v + 1
float    fineDetune(uint8_t v);            // midiParser_calcDetune
float    oscFrequencyHz(uint16_t midiFreq, uint8_t baseNote, const float* noteTable); // osc_recalcFreq
}
