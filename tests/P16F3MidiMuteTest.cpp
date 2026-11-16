// Exact-DLL VST3 MIDI-controller path, not a test-only scheduler mute setter.
#include "../vendor/iPlug2/Examples/LXR/LXR.h"
#define main ExistingP12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main
struct P16F3VST3CertificationProbe {
 static LXR& plugin(HostInstance& h){return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(static_cast<SingleComponentEffect*>(h.component)));}
 static auto diagnostics(HostInstance& h){return plugin(h).mSequencer.diagnostics();}
};
namespace {
double process(HostInstance& h,int at,int n,bool play,IParameterChanges* changes=nullptr,IEventList* events=nullptr){
 std::array<double,256> l{},r{};double* channels[]{l.data(),r.data()};AudioBusBuffers output{};output.numChannels=2;output.channelBuffers64=channels;
 ProcessContext c{};c.sampleRate=48000;c.tempo=120;c.projectTimeSamples=at;c.projectTimeMusic=at/24000.0;c.state=ProcessContext::kTempoValid|ProcessContext::kProjectTimeMusicValid|(play?ProcessContext::kPlaying:0);
 ProcessData d{};d.processMode=kOffline;d.symbolicSampleSize=kSample64;d.numSamples=n;d.numOutputs=1;d.outputs=&output;d.processContext=&c;d.inputParameterChanges=changes;d.inputEvents=events;require("VST3 process",h.processor->process(d)==kResultOk);
 double peak=0;for(int i=0;i<n;++i)peak=std::max({peak,std::abs(l[i]),std::abs(r[i])});return peak;
}
void cc(HostInstance& h,unsigned number,unsigned value){
 ParamID id=0;require("production MIDI mapping",h.midiMapping->getMidiControllerAssignment(0,0,static_cast<CtrlNumber>(number),id)==kResultTrue);
 ParameterChanges changes(1);int32 qi=0,pi=0;auto* q=changes.addParameterData(id,qi);require("MIDI controller point",q&&q->addPoint(0,value/127.0,pi)==kResultOk);process(h,0,32,false,&changes);
}
void nrpn(HostInstance& h,unsigned number,unsigned value){cc(h,99,number>>7);cc(h,98,number&127);cc(h,6,value);}
}
int main(int argc,char**argv){if(argc!=2)return 2;auto m=LoadLibraryA(argv[1]);require("DLL",m!=nullptr);auto init=reinterpret_cast<bool(*)()>(GetProcAddress(m,"InitDll"));auto end=reinterpret_cast<bool(*)()>(GetProcAddress(m,"ExitDll"));auto factoryFn=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(m,"GetPluginFactory"));require("exports",init&&end&&factoryFn&&init());auto* factory=factoryFn();PClassInfo info{};bool found=false;for(int i=0;i<factory->countClasses();++i)if(factory->getClassInfo(i,&info)==kResultOk&&std::strcmp(info.category,kVstAudioEffectClass)==0){found=true;break;}require("audio class",found);
 {HostInstance h(factory,info.cid);h.setup(48000);auto state=h.saveState();lxr::state::ProjectCandidate c;require("decode state",lxr::state::DecodeProject(state.data(),state.size(),c,true));c.transport.enabled=1;c.transport.clock=1;c.bank=lxr::sequencer::MakeDefaultPatternBank();c.bank.patterns[0].mainGates[0]=0xffff;lxr::state::ProjectBytes b;size_t n=0;require("encode bank",lxr::state::EncodeProject(c,b,n,true));state.assign(b.begin(),b.begin()+n);require("production project loader",h.loadState(state));process(h,0,32,false);
  for(unsigned track=0;track<7;++track)nrpn(h,200+track,127);
  auto before=P16F3VST3CertificationProbe::diagnostics(h);double peak=0;for(int at=0;at<4800;at+=240)peak=std::max(peak,process(h,at,240,true));
  check("muted ordinary sequence silent",peak==0&&P16F3VST3CertificationProbe::diagnostics(h).emittedEvents==before.emittedEvents);
  const auto muted=P16F3VST3CertificationProbe::diagnostics(h);
  check("all seven logical mute bits reached scheduler",muted.mutedOrdinaryTracks-before.mutedOrdinaryTracks==7*(muted.duePhysicalPositions-before.duePhysicalPositions));
  require("set roll rate",h.controller->setParamNormalized(243,1)==kResultOk);require("hold D1 roll",h.controller->setParamNormalized(244,1.0/127)==kResultOk);
  peak=0;for(int at=4800;at<9600;at+=240)peak=std::max(peak,process(h,at,240,true));
  check("source roll ignores track mute and renders PCM",peak>0&&P16F3VST3CertificationProbe::diagnostics(h).emittedRollEvents>before.emittedRollEvents);
  require("release roll",h.controller->setParamNormalized(244,0)==kResultOk);process(h,9600,240,false);
  for(unsigned track=0;track<7;++track)nrpn(h,200+track,0);
  nrpn(h,207,127);before=P16F3VST3CertificationProbe::diagnostics(h);process(h,0,240,true);
  check("zero NRPN unmutes and unsupported NRPN cannot address eighth track",P16F3VST3CertificationProbe::diagnostics(h).mutedOrdinaryTracks==before.mutedOrdinaryTracks);
  nrpn(h,200,127);EventList notes(1);Event note{};note.type=Event::kNoteOnEvent;note.noteOn.pitch=63;note.noteOn.velocity=1;notes.addEvent(note);
  check("sequencer mute leaves external MIDI audible",process(h,0,256,false,nullptr,&notes)>0);
  check("no MIDI mute host gestures",h.handler.edits==0);
 }
 factory->release();end();FreeLibrary(m);std::printf("SUMMARY pass=%d fail=%d\n",passed,failed);return failed?1:0;
}
