// Instrument the exact normally loaded module's IAT in memory. Disk image is unchanged.
#include "../vendor/iPlug2/Examples/LXR/LXR.h"
#define main ExistingP12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main
#include <intrin.h>
#include <string>

struct P16F3VST3CertificationProbe {
    static LXR& plugin(HostInstance& h) {return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(static_cast<SingleComponentEffect*>(h.component)));}
    static uint64_t dispatched(HostInstance& h){return plugin(h).mDispatchedMidiMessages;}
    static uint64_t rejected(HostInstance& h){return plugin(h).mRejectedMidiInputRecords.load(std::memory_order_relaxed);}
    static uint64_t overflow(HostInstance& h){return plugin(h).mDeferredMidiOverflows.load(std::memory_order_relaxed);}
    static uint32_t peak(HostInstance& h){return plugin(h).mCallbackMidiPeak;}
    // Exact matching final PDB supplies this private production function RVA.
    // It remains ordinary DLL execution; no alternate loader or file patch.
    static bool import(HostInstance& h,const char* path,HMODULE module,uintptr_t rva){
        auto fn=reinterpret_cast<bool(*)(LXR*,const char*)>(reinterpret_cast<uintptr_t>(module)+rva);
        return fn(&plugin(h),path);
    }
};
namespace {
DWORD measuredThread=0; bool measuring=false; uint64_t allocations=0,requested=0;
void count(SIZE_T n){if(measuring&&GetCurrentThreadId()==measuredThread){++allocations;requested+=n;}}
decltype(&HeapAlloc) heapAlloc=nullptr;decltype(&HeapReAlloc) heapRealloc=nullptr;
decltype(&GlobalAlloc) globalAlloc=nullptr;decltype(&CoTaskMemAlloc) taskAlloc=nullptr;
LPVOID WINAPI ha(HANDLE h,DWORD f,SIZE_T n){count(n);return heapAlloc(h,f,n);}
LPVOID WINAPI hr(HANDLE h,DWORD f,LPVOID p,SIZE_T n){count(n);return heapRealloc(h,f,p,n);}
HGLOBAL WINAPI ga(UINT f,SIZE_T n){count(n);return globalAlloc(f,n);}
LPVOID WINAPI ca(SIZE_T n){count(n);return taskAlloc(n);}
struct Slot{uintptr_t* address;uintptr_t original;};std::array<Slot,16> slots{};unsigned slotCount=0;
void hooks(HMODULE module){
    auto* base=reinterpret_cast<uint8_t*>(module);auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    auto rva=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    for(auto* d=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+rva);d->Name;++d){
        auto* names=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+d->OriginalFirstThunk);
        auto* addresses=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+d->FirstThunk);
        for(unsigned i=0;names[i].u1.AddressOfData;++i){
            if(IMAGE_SNAP_BY_ORDINAL64(names[i].u1.Ordinal))continue;
            auto* name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names[i].u1.AddressOfData)->Name;
            uintptr_t replacement=0;auto current=static_cast<uintptr_t>(addresses[i].u1.Function);
            if(!std::strcmp(name,"HeapAlloc")){heapAlloc=reinterpret_cast<decltype(heapAlloc)>(current);replacement=reinterpret_cast<uintptr_t>(&ha);}
            if(!std::strcmp(name,"HeapReAlloc")){heapRealloc=reinterpret_cast<decltype(heapRealloc)>(current);replacement=reinterpret_cast<uintptr_t>(&hr);}
            if(!std::strcmp(name,"GlobalAlloc")){globalAlloc=reinterpret_cast<decltype(globalAlloc)>(current);replacement=reinterpret_cast<uintptr_t>(&ga);}
            if(!std::strcmp(name,"CoTaskMemAlloc")){taskAlloc=reinterpret_cast<decltype(taskAlloc)>(current);replacement=reinterpret_cast<uintptr_t>(&ca);}
            if(replacement){
                auto* address=reinterpret_cast<uintptr_t*>(&addresses[i].u1.Function);DWORD old=0,ignored=0;
                require("writable measured IAT slot",VirtualProtect(address,sizeof(*address),PAGE_READWRITE,&old)!=0);
                slots[slotCount++]={address,*address};*address=replacement;VirtualProtect(address,sizeof(*address),old,&ignored);
                std::printf("HOOK %s\n",name);
            }
        }
    }
    require("all four imported allocators instrumented",slotCount==4&&heapAlloc&&heapRealloc&&globalAlloc&&taskAlloc);
}
void unhook(){for(unsigned i=0;i<slotCount;++i){DWORD old,ignored;auto& s=slots[i];VirtualProtect(s.address,8,PAGE_READWRITE,&old);*s.address=s.original;VirtualProtect(s.address,8,old,&ignored);}}
uintptr_t low=0,high=0;
__declspec(noinline) void prime(){volatile unsigned char buffer[512*1024];for(unsigned i=0;i<sizeof(buffer);++i)buffer[i]=0xa5;low=reinterpret_cast<uintptr_t>(&buffer[0]);high=low+sizeof(buffer);_ReadWriteBarrier();}
std::array<uint8_t,512*1024> survey{};
size_t watermark(){SIZE_T read=0;require("read stack watermark",ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(low),survey.data(),survey.size(),&read)!=0&&read==survey.size());
    size_t first=survey.size();for(size_t i=0;i<survey.size();++i)if(survey[i]!=0xa5){first=i;break;}return survey.size()-first;}
void scope(){allocations=0;requested=0;measuring=true;}
void report(const char* name,size_t stack){measuring=false;std::printf("MEASURE %s allocations=%llu bytes=%llu stack_watermark=%zu\n",name,static_cast<unsigned long long>(allocations),static_cast<unsigned long long>(requested),stack);}
}
int main(int argc,char**argv){
    // P19-E's whole-program build can inline the non-real-time bank-import
    // helper completely. The original four-argument P16 probe remains intact;
    // a module-only invocation retains all state/publication and processing
    // measurements without depending on that optional private symbol.
    if(argc!=2&&argc!=4)return 2;measuredThread=GetCurrentThreadId();
    ULONG_PTR stackLow=0,stackHigh=0;GetCurrentThreadStackLimits(&stackLow,&stackHigh);
    std::printf("STACK reserved=%llu\n",static_cast<unsigned long long>(stackHigh-stackLow));
    auto module=LoadLibraryA(argv[1]);if(!module){std::printf("LOAD_ERROR %lu\n",GetLastError());return 2;}hooks(module);
    auto init=reinterpret_cast<bool(*)()>(GetProcAddress(module,"InitDll"));auto end=reinterpret_cast<bool(*)()>(GetProcAddress(module,"ExitDll"));
    auto getFactory=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(module,"GetPluginFactory"));require("exports",init&&end&&getFactory&&init());
    auto* factory=getFactory();PClassInfo info{};bool found=false;
    for(int i=0;i<factory->countClasses();++i)if(factory->getClassInfo(i,&info)==kResultOk&&std::strcmp(info.category,kVstAudioEffectClass)==0){found=true;break;}
    require("audio class",found);
    {
        scope();HostInstance h(factory,info.cid);h.setup(48000,8192);measuring=false;
        auto initAlloc=allocations;report("initialization",0);check("allocation positive control",initAlloc>0);
        if(argc==4){prime();scope();bool imported=P16F3VST3CertificationProbe::import(h,argv[2],module,std::strtoull(argv[3],nullptr,10));measuring=false;auto importStack=watermark();report("production_file_import",importStack);require("real production import",imported);}
        auto bytes=h.saveState();lxr::state::ProjectCandidate c;require("decode setup",lxr::state::DecodeProject(bytes.data(),bytes.size(),c,true));
        c.transport.enabled=1;c.transport.clock=1;lxr::state::ProjectBytes encoded;std::size_t n=0;require("encode setup",lxr::state::EncodeProject(c,encoded,n,true));bytes.assign(encoded.begin(),encoded.begin()+n);
        prime();scope();bool loaded=h.loadState(bytes);measuring=false;auto restoreStack=watermark();report("component_setState",restoreStack);require("restore",loaded);
        MemoryStream stream;prime();scope();auto saved=h.component->getState(&stream);measuring=false;auto saveStack=watermark();report("component_getState",saveStack);require("serialize",saved==kResultOk);
        std::array<double,8192> l{},r{};double* channels[]{l.data(),r.data()};AudioBusBuffers output{};output.numChannels=2;output.channelBuffers64=channels;
        ProcessContext ctx{};ctx.sampleRate=48000;ctx.tempo=120;ctx.state=ProcessContext::kTempoValid|ProcessContext::kProjectTimeMusicValid|ProcessContext::kPlaying;
        ProcessData d{};d.processMode=kOffline;d.symbolicSampleSize=kSample64;d.numSamples=127;d.numOutputs=1;d.outputs=&output;d.processContext=&ctx;
        // Include first restore boundary, varying callback sizes, seeks, loops,
        // tempo changes, parameter locks, held rolls and direct MIDI events.
        ParameterChanges changes(2);int32 qi=0,pi=0;auto* q=changes.addParameterData(243,qi);q->addPoint(0,1,pi);q=changes.addParameterData(244,qi);q->addPoint(0,127.0/127,pi);
        EventList midi(4);Event e{};e.type=Event::kNoteOnEvent;e.noteOn.pitch=60;e.noteOn.velocity=.8f;midi.addEvent(e);
        ParameterChanges msb(1),lsb(1),dataEntry(1);IParameterChanges* nrpn[]{&msb,&lsb,&dataEntry};
        unsigned ccNumbers[]{99,98,6};double ccValues[]{1.0/127,72.0/127,1.0};
        for(unsigned i=0;i<3;++i){ParamID id=0;require("measured NRPN controller assignment",h.midiMapping->getMidiControllerAssignment(0,0,static_cast<CtrlNumber>(ccNumbers[i]),id)==kResultTrue);int32 qi=0,pi=0;auto* queue=static_cast<ParameterChanges*>(nrpn[i])->addParameterData(id,qi);require("prepare host-owned NRPN queue",queue&&queue->addPoint(0,ccValues[i],pi)==kResultOk);}
        bool ok=true;prime();scope();
        for(unsigned i=0;i<4000;++i){d.numSamples=1+(i*37)%256;ctx.projectTimeSamples+=d.numSamples;ctx.projectTimeMusic=ctx.projectTimeSamples*(i<2000?120:60)/(48000.0*60);ctx.tempo=i<2000?120:60;
            d.inputParameterChanges=i==2?&changes:nullptr;d.inputEvents=i%100==0?&midi:nullptr;
            if(i>=100&&i<103)d.inputParameterChanges=nrpn[i-100];
            if(i==3000){ctx.state|=ProcessContext::kCycleActive|ProcessContext::kCycleValid;ctx.cycleStartMusic=0;ctx.cycleEndMusic=4;ctx.projectTimeSamples=0;ctx.projectTimeMusic=0;}
            if(i==3500)ctx.state&=~ProcessContext::kPlaying;
            ok&=h.processor->process(d)==kResultOk;
        }
        measuring=false;auto audioAlloc=allocations;auto processingStack=watermark();report("process_4000_callbacks",processingStack);
        check("processing completed",ok);check("exact DLL processing allocations zero",audioAlloc==0);
        check("audio stack below 64KiB observed",processingStack<65536);
        // F4: exercise the full resident callback-MIDI sort and dispatch bound.
        // The host-owned list is allocated before the measured plugin scope.
        EventList dense(4096);
        for(unsigned i=0;i<4096;++i){Event note{};note.type=Event::kNoteOnEvent;
            note.sampleOffset=static_cast<int32>((i*73)%256);note.noteOn.channel=static_cast<int16>(i%7);
            note.noteOn.pitch=60;note.noteOn.velocity=.8f;note.noteOn.noteId=-1;
            require("prepare full callback MIDI capacity",dense.addEvent(note)==kResultOk);}
        d.numSamples=256;d.inputEvents=&dense;d.inputParameterChanges=nullptr;
        ctx.projectTimeSamples=0;ctx.projectTimeMusic=0;
        prime();scope();auto denseResult=h.processor->process(d);measuring=false;
        const auto denseAlloc=allocations;const auto denseStack=watermark();
        report("process_4096_MIDI_events",denseStack);
        check("full capacity callback completes",denseResult==kResultOk);
        check("full capacity callback allocations zero",denseAlloc==0);
        check("full capacity callback stack below 64KiB observed",denseStack<65536);
        // F5: timestamped CC point expansion reaches the shared fixed bound.
        // All host storage is prepared outside the measured plugin scope.
        ParameterChanges timed(1);ParamID cc88=0;
        require("CC88 mapped for resource measurement",h.midiMapping->getMidiControllerAssignment(0,0,88,cc88)==kResultTrue);
        auto* timedQueue=timed.addParameterData(cc88,qi);require("timed host queue",timedQueue!=nullptr);
        for(int i=0;i<4097;++i)require("prepare timed CC capacity plus one",timedQueue->addPoint(i,static_cast<double>(i%128)/127,pi)==kResultOk);
        EventList extra(1);e.sampleOffset=8191;require("prepare mixed overflow note",extra.addEvent(e)==kResultOk);
        const auto beforeDispatch=P16F3VST3CertificationProbe::dispatched(h);
        const auto beforeReject=P16F3VST3CertificationProbe::rejected(h);
        const auto beforeOverflow=P16F3VST3CertificationProbe::overflow(h);
        d.numSamples=8192;d.inputParameterChanges=&timed;d.inputEvents=&extra;
        ctx.projectTimeSamples=256;ctx.projectTimeMusic=256.0/24000;
        prime();scope();auto timedResult=h.processor->process(d);measuring=false;
        const auto timedAlloc=allocations;const auto timedStack=watermark();
        report("process_4097_CC_points_plus_note",timedStack);
        check("timed capacity callback completes",timedResult==kResultOk);
        check("timed capacity allocations zero",timedAlloc==0);
        check("timed capacity stack below 64KiB observed",timedStack<65536);
        check("exactly 4096 timed values dispatched",P16F3VST3CertificationProbe::dispatched(h)-beforeDispatch==4096);
        check("one excess CC point rejected",P16F3VST3CertificationProbe::rejected(h)-beforeReject==1);
        check("mixed note rejected at shared bound",P16F3VST3CertificationProbe::overflow(h)-beforeOverflow==1);
        check("peak occupancy fixed at 4096",P16F3VST3CertificationProbe::peak(h)==4096);
        d.inputParameterChanges=nullptr;d.inputEvents=nullptr;ctx.projectTimeSamples+=8192;ctx.projectTimeMusic=ctx.projectTimeSamples/24000.0;
        const auto afterDispatch=P16F3VST3CertificationProbe::dispatched(h);
        prime();scope();auto recoveryResult=h.processor->process(d);measuring=false;
        const auto recoveryAlloc=allocations;const auto recoveryStack=watermark();report("process_overflow_recovery",recoveryStack);
        check("overflow recovery completes",recoveryResult==kResultOk);
        check("overflow recovery allocations zero",recoveryAlloc==0);
        check("overflow recovery stack below 64KiB observed",recoveryStack<65536);
        check("no stale event replay after overflow",P16F3VST3CertificationProbe::dispatched(h)==afterDispatch);
    }
    factory->release();end();unhook();FreeLibrary(module);std::printf("SUMMARY pass=%d fail=%d\n",passed,failed);return failed?1:0;
}
