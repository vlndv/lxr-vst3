// Exact-DLL first-divergence probe. Access modifiers below change no ABI/code.
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
  static LXR& plugin(HostInstance& h) {
    return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(
      static_cast<SingleComponentEffect*>(h.component)));
  }
  static lxr::LXRAudioAdapter& adapter(HostInstance& h) { return plugin(h).mAudioAdapter; }
  static lxr::Engine& engine(HostInstance& h) { return plugin(h).mEngine; }
};
namespace {
constexpr int length = 48000;
struct Midi { int time, pitch, velocity, channel; bool off; bool control=false; };
std::vector<Midi> fixture {{9600,60,118,0,false},{16800,60,0,0,true}};
// Optional F6 diagnostic: note-only callbacks have no parameter-change object.
// This removes CC queue ingestion from that path without moving note offsets.
bool isolateEmptyCCQueues=false;
const std::array<std::vector<int>,8> partitions {{ {32},{64},{128},{256},
  {32,256},{1,17,93,5,256,31,127,64,3},{127},{512} }};
const char* names[] {"32","64","128","256","alternating","irregular","127","512"};
void dump(const std::string& name, const void* p, size_t size) {
  auto* f=(std::fopen)(name.c_str(),"wb");require("create diagnostic",f!=nullptr);
  require("write diagnostic",std::fwrite(p,1,size,f)==size);std::fclose(f);
}
int callbackStart(int time,const std::vector<int>& sizes) {
  int at=0;size_t i=0;while(at+sizes[i%sizes.size()]<=time){at+=sizes[i%sizes.size()];++i;}return at;
}
std::vector<double> render(HostInstance& h,std::vector<uint8_t>& state,
  size_t partition,bool microscope,bool legacy,const std::string& prefix) {
  require("clean project reset",h.loadState(state));
  std::vector<double> pcm;pcm.reserve(length*2);
  std::vector<int16_t> native;native.reserve(length*2);
  auto* trace=(std::fopen)((prefix+".csv").c_str(),"wb");require("trace",trace!=nullptr);
  std::fprintf(trace,"kind,host_start,frames,event_absolute,event_offset,native_next,phase,velocity\n");
  std::array<double,512> l{},r{};double* channels[]{l.data(),r.data()};
  AudioBusBuffers output{};output.numChannels=2;output.channelBuffers64=channels;
  bool ok=true;size_t callback=0;
  for(int at=0;at<length;){
    int n=std::min(microscope?1:partitions[partition][callback%partitions[partition].size()],length-at);
    EventList events(16);
    ParameterChanges controls(16);
    for(const auto& m:fixture){
      int effective=microscope&&legacy?callbackStart(m.time,partitions[partition]):m.time;
      if(effective<at||effective>=at+n)continue;
      if(m.control){
        ParamID id=0;require("existing MIDI CC assignment",h.midiMapping->getMidiControllerAssignment(
          0,m.channel,static_cast<CtrlNumber>(m.pitch),id)==kResultTrue);
        int32 queueIndex=0,pointIndex=0;auto* q=controls.addParameterData(id,queueIndex);
        require("preserve every timed CC point in host queue",q&&q->addPoint(effective-at,m.velocity/127.0,pointIndex)==kResultOk);
        std::fprintf(trace,"cc%d,%d,%d,%d,%d,%llu,0,%d\n",m.pitch,at,n,m.time,effective-at,
          static_cast<unsigned long long>(P16F3VST3CertificationProbe::adapter(h).nativeFrame()),m.velocity);
        continue;
      }
      Event e{};e.sampleOffset=effective-at;e.type=m.off?Event::kNoteOffEvent:Event::kNoteOnEvent;
      if(m.off){e.noteOff.pitch=m.pitch;e.noteOff.channel=m.channel;e.noteOff.noteId=-1;}
      else{e.noteOn.pitch=m.pitch;e.noteOn.velocity=m.velocity/127.0f;e.noteOn.channel=m.channel;e.noteOn.noteId=-1;}
      require("valid host event offset",e.sampleOffset>=0&&e.sampleOffset<n);
      require("queue event",events.addEvent(e)==kResultOk);
      auto& a=P16F3VST3CertificationProbe::adapter(h);auto& d=P16F3VST3CertificationProbe::engine(h).drums[0];
      std::fprintf(trace,"%s,%d,%d,%d,%d,%llu,%u,%.9g\n",m.off?"off":"on",at,n,m.time,e.sampleOffset,
        static_cast<unsigned long long>(a.nativeFrame()),d.osc.phase,d.velo);
    }
    ProcessContext c{};c.sampleRate=48000;c.tempo=120;c.projectTimeSamples=at;c.projectTimeMusic=at/24000.0;
    c.state=ProcessContext::kTempoValid|ProcessContext::kProjectTimeMusicValid|ProcessContext::kPlaying;
    ProcessData data{};data.processMode=kOffline;data.symbolicSampleSize=kSample64;data.numSamples=n;
    data.numOutputs=1;data.outputs=&output;data.inputEvents=&events;data.processContext=&c;
    data.inputParameterChanges=isolateEmptyCCQueues&&controls.getParameterCount()==0?nullptr:&controls;
    auto& a=P16F3VST3CertificationProbe::adapter(h);const auto before=a.nativeFrame();
    auto& d=P16F3VST3CertificationProbe::engine(h).drums[0];const auto phase=d.osc.phase;const auto velocity=d.velo;
    ok&=h.processor->process(data)==kResultOk;
    if(microscope&&before!=a.nativeFrame()){
      // A setState reset is applied inside the first callback, so use the
      // completed block's index rather than the pre-restore frame counter.
      std::fprintf(trace,"native,%d,%d,-1,-1,%llu,%u,%.9g\n",at,n,static_cast<unsigned long long>(a.nativeFrame()-32),phase,velocity);
      for(size_t i=0;i<32;++i){native.push_back(a.mST1Left[i]);native.push_back(a.mST1Right[i]);}
    }
    for(int i=0;i<n;++i){pcm.push_back(l[i]);pcm.push_back(r[i]);}
    at+=n;++callback;
  }
  std::fclose(trace);check("process completed",ok);
  dump(prefix+".f64",pcm.data(),pcm.size()*sizeof(double));
  if(microscope)dump(prefix+".s16",native.data(),native.size()*sizeof(int16_t));
  return pcm;
}
void compare(const char* label,const std::vector<double>& a,const std::vector<double>& b,bool mandatory){
  size_t first=0;double maximum=0;while(first<a.size()&&a[first]==b[first])++first;
  for(size_t i=0;i<a.size();++i)maximum=std::max(maximum,std::abs(a[i]-b[i]));
  std::printf("COMPARE %s equal=%d first_host_frame=%lld max_abs=%.17g\n",label,a==b,
    first==a.size()?-1:static_cast<long long>(first/2),maximum);
  if(mandatory)check(label,a==b);
}
}
int main(int argc,char** argv){
  if(argc!=5&&argc!=6)return 2;
  isolateEmptyCCQueues=argc==6;
  auto module=LoadLibraryA(argv[1]);require("module",module!=nullptr);
  auto init=reinterpret_cast<bool(*)()>(GetProcAddress(module,"InitDll"));
  auto end=reinterpret_cast<bool(*)()>(GetProcAddress(module,"ExitDll"));
  auto getFactory=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(module,"GetPluginFactory"));
  require("exports",init&&end&&getFactory&&init());auto* factory=getFactory();PClassInfo info{};bool found=false;
  for(int i=0;i<factory->countClasses();++i)if(factory->getClassInfo(i,&info)==kResultOk&&std::strcmp(info.category,kVstAudioEffectClass)==0){found=true;break;}
  require("audio class",found);
  { HostInstance h(factory,info.cid);h.setup(48000,512);
    auto* f=(std::fopen)(argv[2],"rb");require("fixture",f!=nullptr);std::vector<uint8_t> original(51065);
    require("fixture bytes",std::fread(original.data(),1,original.size(),f)==original.size());std::fclose(f);
    lxr::state::ProjectCandidate c;require("decode fixture",lxr::state::DecodeProject(original.data(),original.size(),c,true));
    for(int scenario=0;scenario<5;++scenario){
    if(scenario==1)fixture={{0,60,127,0,false}};
    if(scenario==2)fixture={{0,60,100,0,false},{31,60,0,0,true},{32,60,80,0,false},
      {63,60,0,0,true},{64,60,110,0,false},{127,60,40,0,false},
      {128,60,115,0,false},{255,60,55,0,false},{256,60,85,0,false},
      {511,60,0,0,true},{512,60,118,0,false},{9600,60,50,0,false}};
    if(scenario==3)fixture={{9600,60,100,0,false},{9600,60,110,1,false},
      {9600,60,70,2,false},{9600,60,120,3,false},{9600,60,80,4,false},
      {9600,60,90,5,false},{9600,60,127,6,false},{9635,60,0,5,true},
      {9669,60,60,5,false},{16800,60,0,0,true}};
    // Two points for one existing CC queue must survive callback grouping.
    if(scenario==4)fixture={{9500,60,118,0,false},{9600,88,0,0,false,true},
      {9664,88,100,0,false,true},{16800,60,0,0,true}};
    for(int sync=0;sync<2;++sync){
      c.transport.enabled=sync;c.transport.clock=1;lxr::state::ProjectBytes encoded;size_t size=0;
      require("encode fixture",lxr::state::EncodeProject(c,encoded,size,true));std::vector<uint8_t> state(encoded.begin(),encoded.begin()+size);
      const auto base=std::string(argv[3])+"/"+(scenario?"matrix"+std::to_string(scenario)+"_":"")+(sync?"sync_":"disabled_");std::vector<std::vector<double>> runs;
      for(size_t p=0;p<partitions.size();++p){runs.push_back(render(h,state,p,false,false,base+names[p]));
        compare(names[p],runs[0],runs.back(),true);}
      for(size_t p=6;scenario==0&&p<8;++p){auto micro=render(h,state,p,true,std::atoi(argv[4])!=0,base+names[p]+"_microscope");
        compare("microscope reproduces actual callback PCM",runs[p],micro,true);}
      auto repeat=render(h,state,6,false,false,base+"127_repeat");compare("clean reset repeat",runs[6],repeat,true);
    }
    }
  }
  factory->release();end();FreeLibrary(module);std::printf("SUMMARY pass=%d fail=%d\n",passed,failed);return failed?1:0;
}
