// Read-only state snapshots around ordinary processing of preserved DLLs.
// Only common-prefix LXR fields (unchanged F4/F5 offsets) are inspected.
#include "../dsp/Engine.h"
#include "../dsp/PolyphaseResampler.h"
#define private public
#include "../dsp/LXRAudioAdapter.h"
#undef private
#include "../vendor/iPlug2/Examples/LXR/LXR.h"
#define main ExistingP12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main
#ifdef fopen
#undef fopen
#endif
#include <string>

struct P16F3VST3CertificationProbe {
  static LXR& plugin(HostInstance& h){return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(static_cast<SingleComponentEffect*>(h.component)));}
  static auto& engine(HostInstance& h){return plugin(h).mEngine;}
  static auto& adapter(HostInstance& h){return plugin(h).mAudioAdapter;}
  static auto& params(HostInstance& h){return plugin(h).mParams;}
  static auto& scheduler(HostInstance& h){return plugin(h).mSequencer;}
};
namespace {
void bytes(const std::string& name,const void* p,size_t n){auto* f=std::fopen(name.c_str(),"wb");require("open new diagnostic",f!=nullptr);require("write new diagnostic",std::fwrite(p,1,n,f)==n);std::fclose(f);}
FILE* fields=nullptr;const unsigned char* origin=nullptr;
template<class T>void field(const char* name,const T& value){std::fprintf(fields,"%s,%zu,%zu,%.17g\n",name,static_cast<size_t>(reinterpret_cast<const unsigned char*>(&value)-origin),sizeof(value),static_cast<double>(value));}
#define F(x) field(#x,x)
void osc(const char* name,const lxr::OscInfo& o){std::fprintf(fields,"SECTION %s\n",name);F(o.output);F(o.phaseInc);F(o.phase);F(o.freq);F(o.waveform);F(o.tableOffset);F(o.pitchMod);F(o.fmMod);F(o.modNodeValue);F(o.midiFreq);F(o.baseNote);F(o.startPhase);}
void filter(const lxr::ResonantFilter& o){F(o.f);F(o.g);F(o.q);F(o.s1);F(o.s2);F(o.a);F(o.b);F(o.zi);F(o.drive);}
void amp(const AmpEg& o){F(o.attack);F(o.decay);F(o.value);F(o.slope);F(o.invSlope);F(o.state);F(o.repeat);F(o.repeatCnt);}
void transient(const lxr::TransientGen& o){F(o.output);F(o.phase);F(o.pitch);F(o.waveform);F(o.volume);}
void lfo(const lxr::Lfo& o){F(o.phase);F(o.phaseInc);F(o.frequency);F(o.modNodeValue);F(o.waveform);F(o.retrigger);F(o.sync);F(o.phaseOffset);F(o.amount);F(o.output);F(o.heldValue);F(o.lastPhase);F(o.rng.state);}
template<class Voice>void common(const Voice& v){osc("main",v.osc);F(v.vol);F(v.pan);F(v.velo);F(v.filterType);filter(v.filter);amp(v.oscVolEg);transient(v.transGen);lfo(v.lfo);F(v.volumeMod);F(v.egValueOscVol);F(v.snapEg.value);F(v.rng.state);}
void snapshot(HostInstance& h,const std::string& prefix){
  auto& e=P16F3VST3CertificationProbe::engine(h);auto& a=P16F3VST3CertificationProbe::adapter(h);
  origin=reinterpret_cast<const unsigned char*>(&e);fields=std::fopen((prefix+".csv").c_str(),"wb");require("state field trace",fields!=nullptr);
  std::fprintf(fields,"field,engine_offset,bytes,value\n");
  for(int i=0;i<3;++i){std::fprintf(fields,"VOICE drum%d\n",i);auto& v=e.drums[i];common(v);osc("mod",v.modOsc);F(v.fmModAmount);F(v.oscSample);F(v.offset);F(v.oscPitchEg.decay);F(v.oscPitchEg.value);F(v.oscPitchEg.slope);F(v.egPitchModAmount);F(v.lastGain);F(v.targetGain);F(v.mixOscs);F(v.decimationCnt);F(v.decimationRate);for(auto& x:v.volEgValueBlock)F(x);}
  std::fprintf(fields,"VOICE snare\n");common(e.snare);osc("noise",e.snare.noiseOsc);F(e.snare.noiseSample);F(e.snare.oscSample);F(e.snare.mix);F(e.snare.oscPitchEg.decay);F(e.snare.oscPitchEg.value);F(e.snare.oscPitchEg.slope);F(e.snare.egPitchModAmount);
  std::fprintf(fields,"VOICE cymbal\n");common(e.cymbal);osc("mod",e.cymbal.modOsc);osc("mod2",e.cymbal.modOsc2);F(e.cymbal.fmModAmount1);F(e.cymbal.fmModAmount2);F(e.cymbal.noiseSample);
  std::fprintf(fields,"VOICE hihat\n");common(e.hihat);osc("mod",e.hihat.modOsc);osc("mod2",e.hihat.modOsc2);F(e.hihat.fmModAmount1);F(e.hihat.fmModAmount2);F(e.hihat.noiseSample);F(e.hihat.decayClosed);F(e.hihat.decayOpen);F(e.hihat.isOpen);
  std::fprintf(fields,"MIXER\n");for(auto& x:e.mixer.routing)F(x);for(auto& x:e.mixer.pan)F(x);for(auto& x:e.mixer.decimationRate)F(x);for(auto& x:e.noteFreq)F(x);
  std::fclose(fields);bytes(prefix+".engine",&e,sizeof(e));bytes(prefix+".params",P16F3VST3CertificationProbe::params(h).values,228);
  auto state=h.saveState();bytes(prefix+".lxrs",state.data(),state.size());
  bytes(prefix+".native",a.mST1Left.data(),a.mST1Left.size()*sizeof(int16_t));
  std::printf("SNAPSHOT %s native_next=%llu events=%zu tempo=%u\n",prefix.c_str(),static_cast<unsigned long long>(a.nativeFrame()),P16F3VST3CertificationProbe::scheduler(h).lastBatch().size(),e.nativeTempo());
}
#undef F
void process(HostInstance& h,int n,IEventList* events){
  std::array<double,1> l{},r{};double* ch[]{l.data(),r.data()};AudioBusBuffers output{};output.numChannels=2;output.channelBuffers64=ch;
  ProcessContext ctx{};ctx.sampleRate=48000;ctx.tempo=120;ctx.projectTimeSamples=0;ctx.projectTimeMusic=0;
  ctx.state=ProcessContext::kPlaying|ProcessContext::kTempoValid|ProcessContext::kProjectTimeMusicValid;
  ProcessData data{};data.processMode=kOffline;data.symbolicSampleSize=kSample64;data.numSamples=n;data.numOutputs=1;data.outputs=&output;data.processContext=&ctx;data.inputEvents=events;
  require("ordinary process",h.processor->process(data)==kResultOk);
}
}
int main(int argc,char** argv){
 if(argc!=5)return 2;auto module=LoadLibraryA(argv[1]);require("preserved DLL loads normally",module!=nullptr);
 auto init=reinterpret_cast<bool(*)()>(GetProcAddress(module,"InitDll"));auto end=reinterpret_cast<bool(*)()>(GetProcAddress(module,"ExitDll"));auto get=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(module,"GetPluginFactory"));require("factory exports",init&&end&&get&&init());
 auto* factory=get();PClassInfo info{};bool found=false;for(int i=0;i<factory->countClasses();++i)if(factory->getClassInfo(i,&info)==kResultOk&&std::strcmp(info.category,kVstAudioEffectClass)==0){found=true;break;}require("audio component",found);
 {
  HostInstance h(factory,info.cid);h.setup(48000,512);
  auto* f=std::fopen(argv[2],"rb");require("preserved input",f!=nullptr);std::vector<uint8_t> input(51065);require("exact canonical bytes",std::fread(input.data(),1,input.size(),f)==input.size());std::fclose(f);
  lxr::state::ProjectCandidate c;require("decode preserved state",lxr::state::DecodeProject(input.data(),input.size(),c,true));c.transport.enabled=1;c.transport.clock=1;
  lxr::state::ProjectBytes wire;size_t n=0;require("encode sync fixture",lxr::state::EncodeProject(c,wire,n,true));std::vector<uint8_t> state(wire.begin(),wire.begin()+n);
  const std::string prefix=argv[3];bytes(prefix+"_input.lxrs",state.data(),state.size());require("stage complete restore",h.loadState(state));
  if(std::atoi(argv[4])!=0){EventList ev(1);Event e{};e.type=Event::kPolyPressureEvent;e.sampleOffset=0;e.polyPressure.channel=0;e.polyPressure.pitch=60;e.polyPressure.pressure=.5f;ev.addEvent(e);process(h,0,&ev);snapshot(h,prefix+"_restored");}
  // Test-only intervention, outside processing: isolate the six input-delay
  // scalars without changing scheduling, patch/routing or filter equations.
  if(std::atoi(argv[4])==2){auto& e=P16F3VST3CertificationProbe::engine(h);for(auto& d:e.drums)d.filter.zi=0;e.snare.filter.zi=0;e.cymbal.filter.zi=0;e.hihat.filter.zi=0;snapshot(h,prefix+"_zi_zeroed");}
  process(h,1,nullptr);snapshot(h,prefix+"_first");
 }
 factory->release();end();FreeLibrary(module);std::printf("SUMMARY pass=%d fail=%d\n",passed,failed);return failed?1:0;
}
