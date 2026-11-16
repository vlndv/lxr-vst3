// Human acceptance material from the same normally loaded private VST3.
#define main ExistingP12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main
#ifdef fopen
#undef fopen
#endif
#include <string>

namespace {
void saveWave(const std::string& path,const std::vector<float>& samples){
    const uint32_t bytes=static_cast<uint32_t>(samples.size()*4);
    const uint32_t header[]{0x46464952,bytes+36,0x45564157,0x20746d66,16,0x00020003,48000,384000,0x00200008,0x61746164,bytes};
    auto* f=std::fopen(path.c_str(),"wb");require("create listening WAV",f!=nullptr);
    require("write WAV header",std::fwrite(header,1,sizeof(header),f)==sizeof(header));
    require("write listening PCM",std::fwrite(samples.data(),4,samples.size(),f)==samples.size());std::fclose(f);
}
std::vector<float> audio(HostInstance& h,unsigned which){
    std::array<double,8192> left{},right{};double* channels[]{left.data(),right.data()};
    AudioBusBuffers output{};output.numChannels=2;output.channelBuffers64=channels;
    std::vector<float> result;result.reserve(192000*2);
    ParamID volume=0;require("existing CC88 mapping",h.midiMapping->getMidiControllerAssignment(0,0,88,volume)==kResultTrue);
    for(int at=0;at<192000;){
        const int n=std::min(8192,192000-at);ParameterChanges changes(2);EventList events(8);
        int32 qi=0,pi=0;auto* q=changes.addParameterData(volume,qi);require("host CC queue",q!=nullptr);
        // Three accepted values of the same controller in one callback.
        const int positions[]{0,2731,5462};const int values[]{25,110,60};
        for(unsigned i=0;i<3;++i){if(positions[i]>=n)break;
            require("timed CC point",q->addPoint(positions[i],values[(i+at/8192)%3]/127.0,pi)==kResultOk);
            Event e{};e.type=Event::kNoteOnEvent;e.sampleOffset=positions[i]+17;
            if(e.sampleOffset>=n)continue;e.noteOn.channel=0;e.noteOn.pitch=60;
            e.noteOn.velocity=which==1?(i==0?.35f:i==1?1.f:.65f):.9f;e.noteOn.noteId=-1;
            require("timestamped note",events.addEvent(e)==kResultOk);
        }
        ProcessContext context{};context.sampleRate=48000;context.tempo=120;
        context.projectTimeSamples=at;context.projectTimeMusic=at/24000.0;
        context.state=ProcessContext::kTempoValid|ProcessContext::kProjectTimeMusicValid|ProcessContext::kPlaying;
        ProcessData data{};data.processMode=kOffline;data.symbolicSampleSize=kSample64;
        data.numSamples=n;data.numOutputs=1;data.outputs=&output;data.processContext=&context;
        data.inputParameterChanges=&changes;data.inputEvents=&events;
        require("listening processing",h.processor->process(data)==kResultOk);
        for(int i=0;i<n;++i){result.push_back(static_cast<float>(left[i]));result.push_back(static_cast<float>(right[i]));}at+=n;
    }
    return result;
}
}
int main(int argc,char** argv){
    if(argc!=4)return 2;auto module=LoadLibraryA(argv[1]);require("load exact listening DLL",module!=nullptr);
    auto init=reinterpret_cast<bool(*)()>(GetProcAddress(module,"InitDll"));
    auto end=reinterpret_cast<bool(*)()>(GetProcAddress(module,"ExitDll"));
    auto get=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(module,"GetPluginFactory"));
    require("factory exports",init&&end&&get&&init());auto* factory=get();PClassInfo info{};bool found=false;
    for(int i=0;i<factory->countClasses();++i)if(factory->getClassInfo(i,&info)==kResultOk&&std::strcmp(info.category,kVstAudioEffectClass)==0){found=true;break;}
    require("audio component",found);
    {
        HostInstance h(factory,info.cid);h.setup(48000,8192);
        auto* f=std::fopen(argv[2],"rb");require("listening state",f!=nullptr);std::vector<uint8_t> original(51065);
        require("complete canonical state",std::fread(original.data(),1,original.size(),f)==original.size());std::fclose(f);
        lxr::state::ProjectCandidate candidate;require("decode listening state",lxr::state::DecodeProject(original.data(),original.size(),candidate,true));
        const char* names[]{"F5_01_multiple_timed_CC","F5_02_CC_plus_velocity_notes","F5_03_MIDI_plus_sequencer"};
        for(unsigned which=0;which<3;++which){
            candidate.transport.enabled=which==2;candidate.transport.clock=1;
            lxr::state::ProjectBytes encoded;size_t n=0;require("encode listening configuration",lxr::state::EncodeProject(candidate,encoded,n,true));
            std::vector<uint8_t> state(encoded.begin(),encoded.begin()+n);require("fresh listening state",h.loadState(state));
            auto first=audio(h,which);require("reset for repeat",h.loadState(state));auto repeat=audio(h,which);
            check("listening repeat bit identity",first==repeat);
            check("nonempty audible listening fixture",std::any_of(first.begin(),first.end(),[](float x){return std::abs(x)>1e-5f;}));
            saveWave(std::string(argv[3])+"/"+names[which]+".wav",first);
        }
    }
    factory->release();end();FreeLibrary(module);std::printf("SUMMARY pass=%d fail=%d\n",passed,failed);return failed?1:0;
}
