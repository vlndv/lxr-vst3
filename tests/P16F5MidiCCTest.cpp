// Exact production DLL: timed CC/NRPN history, ordering, bounds and PCM.
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
#include <limits>
#include <string>

struct P16F3VST3CertificationProbe {
  static LXR& plugin(HostInstance& h) {
    return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(static_cast<SingleComponentEffect*>(h.component)));
  }
  static auto counters(HostInstance& h) {
    auto& p=plugin(h);
    return std::array<uint64_t,8>{{p.mRejectedParameterQueues.load(),p.mRejectedMidiInputRecords.load(),
      p.mMalformedMidiInputCounts.load(),p.mCallbackMidiPeak,p.mDispatchedMidiMessages,
      p.mDeferredMidiOverflows.load(),p.mInvalidMidiOffsets.load(),p.mDeferredMidi.size()}};
  }
  static int raw(HostInstance& h,int id){return plugin(h).mParams.values[id];}
  static int mirrored(HostInstance& h,int id){return plugin(h).GetParam(id)->Int();}
  static auto& adapter(HostInstance& h){return plugin(h).mAudioAdapter;}
};

namespace {
struct Point {int offset;double value;};
struct Queue final : IParamValueQueue {
  ParamID id=0;std::vector<Point> points;int32 reported=-2;
  std::array<int,4096> reads{};size_t readCount=0;int maxRead=-1;
  tresult PLUGIN_API queryInterface(const TUID,void** p)override{*p=nullptr;return kNoInterface;}
  uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
  ParamID PLUGIN_API getParameterId()override{return id;}
  int32 PLUGIN_API getPointCount()override{return reported==-2?static_cast<int32>(points.size()):reported;}
  tresult PLUGIN_API getPoint(int32 i,int32& at,ParamValue& value)override{
    maxRead=std::max(maxRead,static_cast<int>(i));
    if(readCount<reads.size())reads[readCount]=i;++readCount;
    if(i<0||static_cast<size_t>(i)>=points.size())return kResultFalse;
    at=points[i].offset;value=points[i].value;return kResultOk;
  }
  tresult PLUGIN_API addPoint(int32 at,ParamValue value,int32& i)override{
    i=static_cast<int32>(points.size());points.push_back({at,value});return kResultOk;
  }
};
struct Changes final : IParameterChanges {
  std::vector<Queue*> queues;int32 reported=-2;int getCalls=0;
  tresult PLUGIN_API queryInterface(const TUID,void** p)override{*p=nullptr;return kNoInterface;}
  uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
  int32 PLUGIN_API getParameterCount()override{return reported==-2?static_cast<int32>(queues.size()):reported;}
  IParamValueQueue* PLUGIN_API getParameterData(int32 i)override{
    ++getCalls;return i>=0&&static_cast<size_t>(i)<queues.size()?queues[i]:nullptr;
  }
  IParamValueQueue* PLUGIN_API addParameterData(const ParamID&,int32&)override{return nullptr;}
};
struct ReportedEvents final : IEventList {
  int32 count=0;int reads=0;
  tresult PLUGIN_API queryInterface(const TUID,void** p)override{*p=nullptr;return kNoInterface;}
  uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
  int32 PLUGIN_API getEventCount()override{return count;}
  tresult PLUGIN_API getEvent(int32 i,Event& e)override{
    ++reads;e={};e.type=Event::kNoteOnEvent;e.sampleOffset=i;e.noteOn.pitch=60;e.noteOn.velocity=.8f;e.noteOn.noteId=-1;return kResultOk;
  }
  tresult PLUGIN_API addEvent(Event&)override{return kNotImplemented;}
};
struct IO {
  std::array<double,8192> left{},right{};double* channels[2]{left.data(),right.data()};AudioBusBuffers output{};
  IO(){output.numChannels=2;output.channelBuffers64=channels;}
  void run(HostInstance& h,int at,int n,IParameterChanges* changes=nullptr,IEventList* events=nullptr,double rate=48000){
    ProcessContext c{};c.sampleRate=rate;c.tempo=120;c.projectTimeSamples=at;c.projectTimeMusic=at*120/(rate*60);
    c.state=ProcessContext::kTempoValid|ProcessContext::kProjectTimeMusicValid|ProcessContext::kPlaying;
    ProcessData d{};d.processMode=kOffline;d.symbolicSampleSize=kSample64;d.numSamples=n;
    d.numOutputs=1;d.outputs=&output;d.processContext=&c;d.inputParameterChanges=changes;d.inputEvents=events;
    require("valid configured callback size",n>=0&&n<=8192);
    require("process bounded MIDI callback",h.processor->process(d)==kResultOk);
  }
};
ParamID ccId(HostInstance& h,int cc){ParamID id=0;require("existing channel0 CC assignment",h.midiMapping->getMidiControllerAssignment(0,0,static_cast<CtrlNumber>(cc),id)==kResultTrue);return id;}
Queue queue(HostInstance& h,int cc,std::initializer_list<Point> points){Queue q;q.id=ccId(h,cc);q.points=points;return q;}
std::vector<uint8_t> wire(const lxr::state::ProjectCandidate& c){lxr::state::ProjectBytes b;size_t n=0;require("encode matched project",lxr::state::EncodeProject(c,b,n,true));return {b.begin(),b.begin()+n};}
void reset(HostInstance& h,std::vector<uint8_t>& state,IO& io){require("restore matched project",h.loadState(state));io.run(h,0,1);}
__declspec(noinline) void idle(iplug::IPlugAPIBase& p){p.OnIdle();}
void raw(HostInstance& h,int id,int value){check("native/processor parameter exact",P16F3VST3CertificationProbe::raw(h,id)==value&&P16F3VST3CertificationProbe::mirrored(h,id)==value);}

void semantics(HostInstance& h,HostInstance& second,std::vector<uint8_t>& state,const std::string& out){
  lxr::state::ProjectCandidate initial;
  require("decode semantic initial state",lxr::state::DecodeProject(state.data(),state.size(),initial,true));
  IO io;reset(h,state,io);reset(second,state,io);
  auto q=queue(h,88,{{0,20/127.0},{32,60/127.0},{64,90/127.0}});Changes c;c.queues={&q};
  const auto before=P16F3VST3CertificationProbe::counters(h);const int gestures=h.handler.edits;
  io.run(h,1,128,&c);const auto after=P16F3VST3CertificationProbe::counters(h);
  check("all three timed CC points read once",q.readCount==3&&q.reads[0]==0&&q.reads[1]==1&&q.reads[2]==2);
  check("all three CC messages dispatched",after[4]-before[4]==3);raw(h,88,90);
  raw(second,88,100);check("other instance receives no MIDI",P16F3VST3CertificationProbe::counters(second)[4]==0);
  idle(P16F3VST3CertificationProbe::plugin(h));
  check("idle controller matches final timed CC",std::abs(h.controller->getParamNormalized(88)-90/127.0)<1e-12);
  check("no unsolicited automation gesture",h.handler.edits==gestures);
  lxr::state::ProjectCandidate saved;auto bytes=h.saveState();require("decode final authority",lxr::state::DecodeProject(bytes.data(),bytes.size(),saved,true));
  check("serialized final value agrees",saved.patch[88]==90&&bytes.size()==51065);

  reset(h,state,io);q=queue(h,88,{{0,40/127.0},{32,40/127.0},{64,40/127.0}});c.queues={&q};
  auto count=P16F3VST3CertificationProbe::counters(h)[4];io.run(h,1,128,&c);
  check("identical values are not coalesced",q.readCount==3&&P16F3VST3CertificationProbe::counters(h)[4]-count==3);
  Queue host;host.id=88;host.points={{0,25/127.0},{31,35/127.0}};q=queue(h,88,{{32,77/127.0}});c.queues={&host,&q};
  io.run(h,129,128,&c);raw(h,88,77);
  check("native automation still reads only final point",host.readCount==1&&host.reads[0]==1);
  host.points={{0,22/127.0}};host.readCount=0;c.queues={&host};io.run(h,257,1,&c);raw(h,88,22);

  reset(h,state,io);auto msb=queue(h,99,{{0,0}}),lsb=queue(h,98,{{1,1/127.0}}),value=queue(h,6,{{3,45/127.0},{67,70/127.0}});
  msb.points.push_back({64,0});lsb.points.push_back({65,1/127.0});q=queue(h,88,{{2,77/127.0},{66,55/127.0}});
  // Deliberately not chronological queue order; timestamps must reconstruct it.
  c.queues={&value,&q,&lsb,&msb};io.run(h,1,128,&c);raw(h,128,70);raw(h,88,55);
  check("every NRPN data entry retained",value.readCount==2);
  const auto persistentRoutes=saved.routing;
  bytes=h.saveState();require("decode NRPN authority",lxr::state::DecodeProject(bytes.data(),bytes.size(),saved,true));
  bool same=true;for(size_t i=0;i<12;++i)same&=saved.routing.routes[i].originalIndex==persistentRoutes.routes[i].originalIndex&&saved.routing.routes[i].flags==persistentRoutes.routes[i].flags;
  check("CC/NRPN do not mutate routing records",same);

  reset(h,state,io);msb=queue(h,99,{{0,0}});lsb=queue(h,98,{{1,1/127.0}});c.queues={&msb,&lsb};io.run(h,1,2,&c);
  require("restore resets partial NRPN",h.loadState(state));value=queue(h,6,{{0,42/127.0}});c.queues={&value};io.run(h,0,1,&c);
  check("pre-restore NRPN selector cannot replay",P16F3VST3CertificationProbe::raw(h,128)==initial.patch[128]);
  reset(h,state,io);
  ParamID unsupported=0;check("unsupported CC channel stays unsupported",h.midiMapping->getMidiControllerAssignment(0,1,88,unsupported)!=kResultTrue);
  q=queue(h,88,{{-1,0},{128,0},{64,std::numeric_limits<double>::quiet_NaN()}});c.queues={&q};
  auto a=P16F3VST3CertificationProbe::counters(h);io.run(h,1,128,&c);auto b=P16F3VST3CertificationProbe::counters(h);
  raw(h,88,100);check("invalid offsets rejected observably",b[6]-a[6]==2);check("invalid value does not dispatch",b[4]==a[4]);
  auto* trace=(std::fopen)((out+"/state_transition_trace.csv").c_str(),"wb");require("create state trace",trace!=nullptr);
  std::fprintf(trace,"host_offset,expected_raw88,actual_raw88,next_native\n");
  for(int at=0;at<96;++at){Queue one;one.id=ccId(h,88);Changes oneChange;
    if(at==0||at==32||at==64){one.points={{0,(at==0?20:at==32?60:90)/127.0}};oneChange.queues={&one};}
    io.run(h,at,1,&oneChange);int expected=at<32?20:at<64?60:90;raw(h,88,expected);
    if(at%32==0)std::fprintf(trace,"%d,%d,%d,%llu\n",at,expected,P16F3VST3CertificationProbe::raw(h,88),static_cast<unsigned long long>(P16F3VST3CertificationProbe::adapter(h).nativeFrame()));
  }
  std::fclose(trace);
}

void capacity(HostInstance& h,std::vector<uint8_t>& state){
  IO io;
  for(int n:{4095,4096,4097}){
    reset(h,state,io);Queue q;q.id=ccId(h,88);for(int i=0;i<n;++i)q.points.push_back({i,(i%128)/127.0});Changes c;c.queues={&q};
    auto a=P16F3VST3CertificationProbe::counters(h);io.run(h,1,8192,&c);auto b=P16F3VST3CertificationProbe::counters(h);
    const int accepted=std::min(n,4096);check("CC capacity read bound exact",q.readCount==static_cast<size_t>(accepted)&&q.maxRead==accepted-1);
    check("CC capacity dispatch exact",b[4]-a[4]==static_cast<uint64_t>(accepted));
    check("CC overflow diagnosed exactly",b[1]-a[1]==static_cast<uint64_t>(n-accepted));raw(h,88,(accepted-1)%128);
    check("peak occupancy bounded",b[3]<=4096);
    std::printf("CAPACITY input=%d accepted=%d rejected=%llu peak=%llu\n",n,accepted,static_cast<unsigned long long>(b[1]-a[1]),static_cast<unsigned long long>(b[3]));
  }
  reset(h,state,io);Queue dense;dense.id=ccId(h,88);for(int i=0;i<4096;++i)dense.points.push_back({i,100/127.0});Changes c;c.queues={&dense};ReportedEvents events;events.count=1;
  auto a=P16F3VST3CertificationProbe::counters(h);io.run(h,1,8192,&c,&events);auto b=P16F3VST3CertificationProbe::counters(h);
  check("mixed notes/CC share one capacity",b[4]-a[4]==4096&&b[5]-a[5]==1);
  reset(h,state,io);auto recover=queue(h,88,{{0,77/127.0}});c.queues={&recover};io.run(h,1,1,&c);raw(h,88,77);
  check("no stale deferred events after overflow restore",P16F3VST3CertificationProbe::counters(h)[7]==0);
  reset(h,state,io);Changes huge;huge.reported=std::numeric_limits<int32>::max();a=P16F3VST3CertificationProbe::counters(h);io.run(h,1,1,&huge);b=P16F3VST3CertificationProbe::counters(h);
  check("huge parameter count scan bounded",huge.getCalls==376&&b[0]-a[0]==static_cast<uint64_t>(std::numeric_limits<int32>::max()-376));
  huge.reported=-1;a=b;io.run(h,2,1,&huge);b=P16F3VST3CertificationProbe::counters(h);check("negative parameter count diagnosed",b[2]-a[2]==1);
  events.count=-1;events.reads=0;a=b;io.run(h,3,1,nullptr,&events);b=P16F3VST3CertificationProbe::counters(h);check("negative event count rejected without reads",events.reads==0&&b[2]-a[2]==1);
  events.count=std::numeric_limits<int32>::max();events.reads=0;a=b;io.run(h,4,8192,nullptr,&events);b=P16F3VST3CertificationProbe::counters(h);
  check("huge event count read bound exact",events.reads==4096&&b[1]-a[1]==static_cast<uint64_t>(std::numeric_limits<int32>::max()-4096));
  reset(h,state,io);dense.reported=std::numeric_limits<int32>::max();dense.readCount=0;dense.maxRead=-1;c.queues={&dense};a=P16F3VST3CertificationProbe::counters(h);io.run(h,1,8192,&c);b=P16F3VST3CertificationProbe::counters(h);
  check("huge CC point count read bound exact",dense.readCount==4096&&dense.maxRead==4095&&b[1]-a[1]==static_cast<uint64_t>(std::numeric_limits<int32>::max()-4096));
  dense.reported=-1;c.queues={&dense};a=b;io.run(h,8193,1,&c);b=P16F3VST3CertificationProbe::counters(h);check("negative CC count diagnosed",b[2]-a[2]==1);
}

struct Timed {int at,kind,a,b,channel;}; // kind0 CC,1 NoteOn,2 NoteOff
const std::vector<Timed> timeline{{0,0,88,90,0},{1,1,60,118,0},{32,0,88,0,0},{64,0,88,100,0},{96,0,88,100,0},
  {127,0,94,0,0},{129,0,94,127,0},{256,0,88,0,0},{256,1,60,63,0},{257,0,88,100,0},
  {512,0,99,0,0},{513,0,98,1,0},{514,0,88,72,0},{515,0,6,45,0},
  {550,0,99,0,0},{551,0,98,1,0},{552,0,6,70,0},{640,0,88,0,0},{641,2,60,0,0},{673,0,88,100,0},
  {750,1,60,90,5},{750,1,60,127,6},{1024,0,0,0,0},{16383,0,88,20,0}};
void dump(const std::string& path,const void* p,size_t n){auto* f=(std::fopen)(path.c_str(),"wb");require("create PCM evidence",f!=nullptr);require("write PCM evidence",std::fwrite(p,1,n,f)==n);std::fclose(f);}
std::vector<double> pcm(HostInstance& h,std::vector<uint8_t>& state,const std::vector<int>& sizes,double rate,const std::string& prefix,bool withCC=true){
  require("restore PCM baseline",h.loadState(state));IO io;std::vector<double> result;result.reserve(32768);std::vector<int16_t> native;
  auto* trace=(std::fopen)((prefix+".csv").c_str(),"wb");require("create event trace",trace!=nullptr);std::fprintf(trace,"callback,host_start,frames,absolute,offset,kind,data1,data2,channel,next_native\n");
  size_t callback=0;
  for(int at=0;at<16384;){const int n=std::min(sizes[callback%sizes.size()],16384-at);ParameterChanges changes(16);EventList events(16);
    for(const auto& e:timeline)if(e.at>=at&&e.at<at+n){
      if(e.kind==0&&!withCC)continue;
      if(e.kind==0){int32 qi=0,pi=0;auto* q=changes.addParameterData(ccId(h,e.a),qi);require("timed CC accepted by host queue",q&&q->addPoint(e.at-at,e.b/127.0,pi)==kResultOk);}
      else{Event note{};note.sampleOffset=e.at-at;note.type=e.kind==1?Event::kNoteOnEvent:Event::kNoteOffEvent;
        if(e.kind==1){note.noteOn.pitch=e.a;note.noteOn.velocity=e.b/127.0f;note.noteOn.channel=e.channel;note.noteOn.noteId=-1;}
        else{note.noteOff.pitch=e.a;note.noteOff.channel=e.channel;note.noteOff.noteId=-1;}require("timed note accepted",events.addEvent(note)==kResultOk);}
      std::fprintf(trace,"%zu,%d,%d,%d,%d,%d,%d,%d,%d,%llu\n",callback,at,n,e.at,e.at-at,e.kind,e.a,e.b,e.channel,static_cast<unsigned long long>(P16F3VST3CertificationProbe::adapter(h).nativeFrame()));
    }
    auto& adapter=P16F3VST3CertificationProbe::adapter(h);const auto before=adapter.nativeFrame();io.run(h,at,n,&changes,&events,rate);
    if(sizes.size()==1&&sizes[0]==1&&before!=adapter.nativeFrame())for(size_t i=0;i<32;++i){native.push_back(adapter.mST1Left[i]);native.push_back(adapter.mST1Right[i]);}
    for(int i=0;i<n;++i){result.push_back(io.left[i]);result.push_back(io.right[i]);}at+=n;++callback;
  }
  std::fclose(trace);dump(prefix+".f64",result.data(),result.size()*8);if(!native.empty())dump(prefix+".s16",native.data(),native.size()*2);
  if(withCC){raw(h,88,20);raw(h,128,70);}return result;
}
}

int main(int argc,char** argv){
  if(argc!=4)return 2;auto module=LoadLibraryA(argv[1]);require("load final private module",module!=nullptr);
  auto init=reinterpret_cast<bool(*)()>(GetProcAddress(module,"InitDll"));auto end=reinterpret_cast<bool(*)()>(GetProcAddress(module,"ExitDll"));auto get=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(module,"GetPluginFactory"));require("exports",init&&end&&get&&init());auto* factory=get();PClassInfo info{};bool found=false;for(int i=0;i<factory->countClasses();++i)if(factory->getClassInfo(i,&info)==kResultOk&&std::strcmp(info.category,kVstAudioEffectClass)==0){found=true;break;}require("audio class",found);
  {HostInstance h(factory,info.cid),second(factory,info.cid);h.setup(48000,8192);second.setup(48000,8192);
    auto* f=(std::fopen)(argv[2],"rb");require("canonical fixture",f!=nullptr);std::vector<uint8_t> original(51065);require("canonical fixture size",std::fread(original.data(),1,original.size(),f)==original.size());std::fclose(f);
    lxr::state::ProjectCandidate candidate;require("decode fixture",lxr::state::DecodeProject(original.data(),original.size(),candidate,true));candidate.transport.enabled=0;auto disabled=wire(candidate);
    semantics(h,second,disabled,argv[3]);capacity(h,disabled);
    const std::array<std::vector<int>,9> partitions{{{1},{32},{64},{128},{256},{512},{8192},{32,256},{1,17,93,5,256,31,127,64,3}}};
    for(double rate:{32000.0,48000.0,96000.0}){
      h.setup(rate,8192);
      for(int mode=0;mode<5;++mode){auto c=candidate;c.transport.enabled=mode==1||mode==4;c.transport.clock=1;
        if(mode>=2){auto vel=lxr::route_host::decode(lxr::route_host::RouteHostParamId::fromRaw(228),lxr::route_host::RouteToken16::fromRaw(34));auto lfo=lxr::route_host::decode(lxr::route_host::RouteHostParamId::fromRaw(234),lxr::route_host::RouteToken16::fromRaw(34));require("verified modulation identities",static_cast<bool>(vel)&&static_cast<bool>(lfo));
          if(mode==2||mode==4){c.routing.routes[0]=vel.record;c.patch[143]=127;}
          if(mode==3||mode==4){c.routing.routes[6]=lfo.record;c.patch[121]=90;c.patch[115]=40;c.patch[155]=2;}}
        auto state=wire(c);std::vector<double> reference;const auto base=std::string(argv[3])+"/rate"+std::to_string(static_cast<int>(rate))+"_mode"+std::to_string(mode);
        for(size_t p=0;p<partitions.size();++p){auto current=pcm(h,state,partitions[p],rate,base+"_profile"+std::to_string(p));if(p==0)reference=current;size_t first=0;while(first<reference.size()&&reference[first]==current[first])++first;
          std::printf("PCM rate=%.0f mode=%d profile=%zu equal=%d first_frame=%lld\n",rate,mode,p,reference==current,first==reference.size()?-1:static_cast<long long>(first/2));check("CC/NRPN/note/modulation PCM partition identity",reference==current);}
        auto repeat=pcm(h,state,partitions[3],rate,base+"_repeat");check("matched offline repeat exact",reference==repeat);
        auto noCC=pcm(h,state,partitions[3],rate,base+"_without_cc",false);
        check("timed controller target changes actual PCM",reference!=noCC);
        check("matched controller PCM is observably active",std::any_of(reference.begin(),reference.end(),[](double x){return std::abs(x)>1e-8;}));
      }
    }
  }
  factory->release();end();FreeLibrary(module);std::printf("SUMMARY pass=%d fail=%d\n",passed,failed);return failed?1:0;
}
