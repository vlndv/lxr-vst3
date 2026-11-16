// Reproduce exact-DLL context differences around a known tempo-map marker.
#include "../vendor/iPlug2/Examples/LXR/LXR.h"
#define main ExistingP12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main
#ifdef fopen
#undef fopen
#endif
struct P16F3VST3CertificationProbe {
 static LXR& plugin(HostInstance& h){return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(static_cast<SingleComponentEffect*>(h.component)));}
 static const lxr::sequencer::TriggerSchedulerDiagnostics& diag(HostInstance& h){return plugin(h).mSequencer.diagnostics();}
};
namespace {
std::vector<double> render(HostInstance& h,std::vector<uint8_t>& state,int block,bool split){
 require("load matched project",h.loadState(state));auto before=P16F3VST3CertificationProbe::diag(h);
 std::vector<double> pcm;pcm.reserve(384000);std::array<double,512> l{},r{};double* channels[]{l.data(),r.data()};AudioBusBuffers output{};output.numChannels=2;output.channelBuffers64=channels;
 bool ok=true;
 for(int at=0;at<192000;){int n=std::min(block,192000-at);if(split&&at<48000)n=std::min(n,48000-at);
  ProcessContext c{};c.sampleRate=48000;c.tempo=at<48000?120:60;c.projectTimeSamples=at;c.projectTimeMusic=at<48000?at/24000.0:2+(at-48000)/48000.0;
  c.state=ProcessContext::kTempoValid|ProcessContext::kProjectTimeMusicValid|ProcessContext::kPlaying;
  ProcessData d{};d.processMode=kOffline;d.symbolicSampleSize=kSample64;d.numSamples=n;d.numOutputs=1;d.outputs=&output;d.processContext=&c;
  ok&=h.processor->process(d)==kResultOk;for(int i=0;i<n;++i){pcm.push_back(l[i]);pcm.push_back(r[i]);}at+=n;
 }
 check("processing",ok);auto after=P16F3VST3CertificationProbe::diag(h);
 std::printf("TEMPO block=%d split=%d seeks=%llu tempo_changes=%llu positions=%llu events=%llu\n",block,split,static_cast<unsigned long long>(after.hostSeeks-before.hostSeeks),static_cast<unsigned long long>(after.hostTempoChanges-before.hostTempoChanges),static_cast<unsigned long long>(after.duePhysicalPositions-before.duePhysicalPositions),static_cast<unsigned long long>(after.emittedEvents-before.emittedEvents));return pcm;
}
void compare(const char* name,const std::vector<double>& a,const std::vector<double>& b,bool requireIdentity){
 size_t i=0;for(;i<a.size()&&a[i]==b[i];++i);std::printf("COMPARE %s equal=%d first_frame=%zu\n",name,a==b,i/2);if(requireIdentity)check(name,a==b);
}
}
int main(int argc,char**argv){if(argc!=3)return 2;auto module=LoadLibraryA(argv[1]);require("module",module!=nullptr);auto init=reinterpret_cast<bool(*)()>(GetProcAddress(module,"InitDll"));auto end=reinterpret_cast<bool(*)()>(GetProcAddress(module,"ExitDll"));auto getFactory=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(module,"GetPluginFactory"));require("exports",init&&end&&getFactory&&init());auto* factory=getFactory();PClassInfo info{};bool found=false;for(int i=0;i<factory->countClasses();++i)if(factory->getClassInfo(i,&info)==kResultOk&&std::strcmp(info.category,kVstAudioEffectClass)==0){found=true;break;}require("audio class",found);
 {HostInstance h(factory,info.cid);h.setup(48000,512);auto* f=(std::fopen)(argv[2],"rb");require("fixture",f!=nullptr);std::vector<uint8_t> state(51065);require("fixture exact size",std::fread(state.data(),1,state.size(),f)==state.size()&&std::fgetc(f)==EOF);std::fclose(f);
  auto split127=render(h,state,127,true),split512=render(h,state,512,true);compare("marker at callback boundary",split127,split512,true);
  auto unsplit127=render(h,state,127,false),unsplit512=render(h,state,512,false);compare("marker inside different callbacks",unsplit127,unsplit512,true);
 }
 factory->release();end();FreeLibrary(module);std::printf("SUMMARY pass=%d fail=%d\n",passed,failed);return failed?1:0;}
