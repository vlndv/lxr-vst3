// Controlled listening material from the exact DLL, actual file importer and
// ordinary VST3 state/automation. This program cannot certify human listening.
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
    static bool import(HostInstance& h,const char* path,HMODULE module,uintptr_t rva){return reinterpret_cast<bool(*)(LXR*,const char*)>(reinterpret_cast<uintptr_t>(module)+rva)(&plugin(h),path);}
};
namespace {
void write(const std::string& path,const void* data,size_t n){auto* f=(std::fopen)(path.c_str(),"wb");require("open task fixture",f!=nullptr);require("write task fixture",std::fwrite(data,1,n,f)==n);std::fclose(f);}
void wav(const std::string& path,const std::vector<float>& pcm){
    const uint32_t data=static_cast<uint32_t>(pcm.size()*4);const uint32_t header[]{0x46464952,data+36,0x45564157,0x20746d66,16,0x00020003,48000,48000*8,0x00200008,0x61746164,data};
    auto* f=(std::fopen)(path.c_str(),"wb");require("open controlled WAV",f!=nullptr);require("write WAV header",std::fwrite(header,1,sizeof(header),f)==sizeof(header));require("write WAV PCM",std::fwrite(pcm.data(),4,pcm.size(),f)==pcm.size());std::fclose(f);
}
void automation(ParameterChanges& changes,ParamID id,double value){int32 qi=0,pi=0;auto* q=changes.addParameterData(id,qi);q->addPoint(0,value,pi);}
std::vector<float> render(HostInstance& h,unsigned which){
    std::vector<float> pcm;pcm.reserve(48000*3*2);std::array<double,128> l{},r{};double* channels[]{l.data(),r.data()};AudioBusBuffers output{};output.numChannels=2;output.channelBuffers64=channels;
    bool ok=true;
    for(int at=0;at<144000;){int frames=std::min(128,144000-at); // exact control edges at half-second boundaries
        frames=std::min(frames,24000-at%24000);
        ProcessContext c{};c.sampleRate=48000;c.tempo=120;c.state=ProcessContext::kTempoValid|ProcessContext::kProjectTimeMusicValid|ProcessContext::kPlaying;
        c.projectTimeSamples=at;c.projectTimeMusic=at/24000.0;
        ParameterChanges changes(3);
        if(which==10){if(at==24000){automation(changes,243,11.0/13);automation(changes,244,1.0/127);}if(at==72000)automation(changes,243,13.0/13);if(at==96000)automation(changes,244,0);}
        if(which==11&&at%24000==0)automation(changes,8,(at/24000)%2?70.0/127:42.0/127);
        if(which==12&&at%24000==0)automation(changes,37,(at/24000)%2?32.0/127:110.0/127);
        if(which==14&&at>=48000&&at<72000)c.state&=~ProcessContext::kPlaying;
        if(which==15){
            if(at>=48000&&at<72000)c.projectTimeSamples=at+48000;
            else if(at>=72000&&at<96000)c.projectTimeSamples=at-72000;
            else if(at>=96000){c.state|=ProcessContext::kCycleActive|ProcessContext::kCycleValid;c.cycleStartMusic=0;c.cycleEndMusic=1;c.projectTimeSamples=(at-96000)%24000;}
            c.projectTimeMusic=c.projectTimeSamples/24000.0;
        }
        ProcessData d{};d.processMode=kOffline;d.symbolicSampleSize=kSample64;d.numSamples=frames;d.numOutputs=1;d.outputs=&output;d.processContext=&c;d.inputParameterChanges=&changes;
        ok&=h.processor->process(d)==kResultOk;
        for(int i=0;i<frames;++i){pcm.push_back(static_cast<float>(l[i]));pcm.push_back(static_cast<float>(r[i]));}at+=frames;
    }
    check("controlled processing succeeded",ok);return pcm;
}
}
int main(int argc,char**argv){
    if(argc!=4)return 2;std::string root=argv[2];auto module=LoadLibraryA(argv[1]);require("exact DLL load",module!=nullptr);
    auto init=reinterpret_cast<bool(*)()>(GetProcAddress(module,"InitDll"));auto end=reinterpret_cast<bool(*)()>(GetProcAddress(module,"ExitDll"));auto getFactory=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(module,"GetPluginFactory"));require("module exports",init&&end&&getFactory&&init());
    auto* factory=getFactory();PClassInfo info{};bool found=false;for(int i=0;i<factory->countClasses();++i)if(factory->getClassInfo(i,&info)==kResultOk&&std::strcmp(info.category,kVstAudioEffectClass)==0){found=true;break;}require("audio component",found);
    const char* names[]{"01_drum1","02_drum2","03_drum3","04_snare","05_cymbal","06_closed_hat","07_open_hat","08_simultaneous_hats","09_substep_retrigger","10_shuffle","11_manual_roll","12_pitch_automation","13_filter_automation","14_velocity_lfo","15_start_stop_resume","16_seek_loop","17_project_reload"};
    std::string playlist="#EXTM3U\n";
    {
        HostInstance h(factory,info.cid);h.setup(48000);auto initial=h.saveState();
        for(unsigned scenario=0;scenario<17;++scenario){
            require("fresh initial state",h.loadState(initial));
            lxr::sequencer::PatternBank bank=lxr::sequencer::MakeDefaultPatternBank();for(unsigned p=0;p<8;++p)bank.patterns[p].nextPattern=static_cast<uint8_t>(p);
            unsigned track=scenario<7?scenario:0;bank.patterns[0].mainGates[track]=0x1111;
            if(scenario==7){bank.patterns[0].mainGates[0]=0;bank.patterns[0].mainGates[5]=bank.patterns[0].mainGates[6]=0x5555;}
            if(scenario==8){for(unsigned s:{1u,2u,3u,5u,6u,7u})lxr::sequencer::SetStepActive(bank.patterns[0].steps[0][s],true);}
            if(scenario==9){bank.shuffle=87;bank.patterns[0].mainGates[0]=0xffff;}
            if(scenario==10)bank.patterns[0].mainGates[0]=0x1111;
            if(scenario==13){bank.patterns[0].mainGates[0]=0xffff;for(unsigned s=0;s<128;++s)lxr::sequencer::SetStepVelocity(bank.patterns[0].steps[0][s],static_cast<uint8_t>(s%16<8?35:115));}
            lxr::sequencer::state::EncodedBank bankBytes;require("canonical listening bank validates",lxr::sequencer::state::Encode(bank,bankBytes));
            std::string name=root+"/"+names[scenario];write(name+".lxqb",bankBytes.data(),bankBytes.size());
            require("production file import",P16F3VST3CertificationProbe::import(h,(name+".lxqb").c_str(),module,std::strtoull(argv[3],nullptr,10)));
            auto bytes=h.saveState();lxr::state::ProjectCandidate c;require("decode imported candidate",lxr::state::DecodeProject(bytes.data(),bytes.size(),c,true));
            c.transport.enabled=1;c.transport.clock=1;
            // Keep every original voice audible at a controlled mixer level.
            for(unsigned id=88;id<=93;++id)c.patch[id]=100;
            c.patch[60]=48;c.patch[61]=95; // source closed/open amp decay IDs
            if(scenario==13){c.patch[115]=37;c.patch[121]=90;c.patch[143]=100;
                for(unsigned slot:{0u,6u}){
                    const auto decoded=lxr::route_host::decode(lxr::route_host::parameterForSlot(slot),lxr::route_host::RouteToken16::fromRaw(2));c.routing.routes[slot]=decoded.record;}}
            lxr::state::ProjectBytes wire;size_t n=0;require("encode listening project",lxr::state::EncodeProject(c,wire,n,true));bytes.assign(wire.begin(),wire.begin()+n);write(name+".lxrs",bytes.data(),bytes.size());
            require("load complete musical state",h.loadState(bytes));auto pcm=render(h,scenario);
            require("repeat complete musical state",h.loadState(bytes));auto repeat=render(h,scenario);
            check("matched-condition PCM bit identity",pcm==repeat);
            double peak=0,sum=0;for(float x:pcm){peak=std::max(peak,std::abs(static_cast<double>(x)));sum+=x*x;}
            check("controlled voice audible PCM",peak>0&&std::isfinite(sum));
            std::printf("FIXTURE %s frames=%zu peak=%.17g rms=%.17g\n",names[scenario],pcm.size()/2,peak,std::sqrt(sum/pcm.size()));
            wav(name+".wav",pcm);if(scenario==16)wav(name+"_repeat.wav",repeat);playlist+=std::string(names[scenario])+".wav\n";
        }
    }
    write(root+"/listening.m3u",playlist.data(),playlist.size());factory->release();end();FreeLibrary(module);std::printf("SUMMARY pass=%d fail=%d\n",passed,failed);return failed?1:0;
}
