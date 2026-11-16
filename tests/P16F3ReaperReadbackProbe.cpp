// Read-only diagnostic for one explicitly identified task-owned REAPER PID.
// Normal LoadLibrary locally; target memory is never written or executed.
#include "../vendor/iPlug2/Examples/LXR/LXR.h"
#define main ExistingP12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main
#ifdef fopen
#undef fopen
#endif
#include <tlhelp32.h>
struct P16F3VST3CertificationProbe {
 static LXR& plugin(HostInstance& h){return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(static_cast<SingleComponentEffect*>(h.component)));}
 static void read(HANDLE process,uintptr_t remote,HostInstance& h,const char* output){
  const auto& local=plugin(h);const auto base=reinterpret_cast<uintptr_t>(&local);
  auto copy=[&](const auto& field,auto& dest){SIZE_T n=0;const auto offset=reinterpret_cast<uintptr_t>(&field)-base;require("read known plugin field",ReadProcessMemory(process,reinterpret_cast<void*>(remote+offset),&dest,sizeof(dest),&n)&&n==sizeof(dest));};
  lxr::sequencer::HostTransportContext ctx;lxr::sequencer::RollHostControls logical,audio;uint8_t applied=0;uint64_t cutoff=0;bool pending=false;
  copy(local.mHostProcessContext,ctx);copy(local.mRollControls,logical);copy(local.mAudioRollControls,audio);copy(local.mAppliedRollMask,applied);copy(local.mRollReleaseCutoff,cutoff);copy(local.mRollReleasePending,pending);
  lxr::sequencer::TriggerSchedulerDiagnostics diag;copy(local.mSequencer.diagnostics(),diag);
  std::printf("DIAGNOSTICS muted_ordinary_tracks=%llu ordinary_events=%llu\n",static_cast<unsigned long long>(diag.mutedOrdinaryTracks),static_cast<unsigned long long>(diag.emittedEvents));
  if(output){
   // Verification only: same exact architecture/PDB object representation.
   // Production persistence still uses explicit canonical encoding exclusively.
   std::array<uintptr_t,4> localPin{},pin{},after{};
   static_assert(sizeof(localPin)==sizeof(local.mAppliedPattern),"known x64 pin size");
   std::memcpy(localPin.data(),&local.mAppliedPattern,sizeof(localPin));
   require("verified local pin pointer offset",localPin[1]==reinterpret_cast<uintptr_t>(&local.mAppliedPattern.bank()));
   copy(local.mAppliedPattern,pin);require("bank pointer stays inside task plugin object",pin[1]>=remote&&pin[1]+sizeof(lxr::sequencer::PatternBank)<=remote+sizeof(LXR));
   lxr::sequencer::PatternBank bank{};SIZE_T n=0;require("read only pinned musical bank",ReadProcessMemory(process,reinterpret_cast<void*>(pin[1]),&bank,sizeof(bank),&n)&&n==sizeof(bank));copy(local.mAppliedPattern,after);require("pin generation stable across diagnostic copy",pin==after);
   lxr::sequencer::state::EncodedBank encoded;require("diagnostic bank canonical validation",lxr::sequencer::state::Encode(bank,encoded));
   auto* f=(std::fopen)(output,"wb");require("canonical diagnostic output",f!=nullptr);require("write canonical bank only",std::fwrite(encoded.data(),1,encoded.size(),f)==encoded.size());std::fclose(f);
  }
  std::printf("REMOTE playing=%d valid=%u sample=%lld ppq=%.17g bpm=%.17g frames=%d logical_mask=%u audio_mask=%u applied_mask=%u rate=%u press_ticket=%llu release_cutoff=%llu release_pending=%d host_stops=%llu seeks=%llu loops=%llu roll_events=%llu\n",ctx.playing,ctx.validFields,static_cast<long long>(ctx.projectSamplePosition),ctx.projectPpqPosition,ctx.tempoBpm,ctx.callbackFrames,logical.tracks,audio.tracks,applied,logical.rate,static_cast<unsigned long long>(logical.pressTicket),static_cast<unsigned long long>(cutoff),pending,static_cast<unsigned long long>(diag.hostStops),static_cast<unsigned long long>(diag.hostSeeks),static_cast<unsigned long long>(diag.hostLoops),static_cast<unsigned long long>(diag.emittedRollEvents));
 }
};
namespace {template<class T>T read(HANDLE p,uintptr_t at){T value{};SIZE_T n=0;require("bounded diagnostic read",ReadProcessMemory(p,reinterpret_cast<void*>(at),&value,sizeof(value),&n)&&n==sizeof(value));return value;}}
int main(int argc,char**argv){
 if(argc!=4&&argc!=5)return 2;auto module=LoadLibraryA(argv[1]);require("local exact DLL",module!=nullptr);auto init=reinterpret_cast<bool(*)()>(GetProcAddress(module,"InitDll"));auto end=reinterpret_cast<bool(*)()>(GetProcAddress(module,"ExitDll"));auto factoryFn=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(module,"GetPluginFactory"));require("exports",init&&end&&factoryFn&&init());auto* factory=factoryFn();PClassInfo info{};bool found=false;for(int i=0;i<factory->countClasses();++i)if(factory->getClassInfo(i,&info)==kResultOk&&std::strcmp(info.category,kVstAudioEffectClass)==0){found=true;break;}require("audio class",found);
 const DWORD pid=static_cast<DWORD>(std::strtoul(argv[2],nullptr,10));const uintptr_t timerRva=std::strtoull(argv[3],nullptr,10);
 auto process=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);require("read-only process access",process!=nullptr);
 wchar_t wanted[MAX_PATH]{};require("bounded module path",MultiByteToWideChar(CP_UTF8,0,argv[1],-1,wanted,MAX_PATH)>0);
 uintptr_t remoteModule=0;auto snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);require("target module inventory",snap!=INVALID_HANDLE_VALUE);MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);
 if(Module32FirstW(snap,&entry))do{if(!_wcsicmp(entry.szExePath,wanted))remoteModule=reinterpret_cast<uintptr_t>(entry.modBaseAddr);}while(Module32NextW(snap,&entry));CloseHandle(snap);require("exact target module path",remoteModule!=0);
 {
  HostInstance h(factory,info.cid);h.setup(48000);auto& local=P16F3VST3CertificationProbe::plugin(h);auto* api=static_cast<iplug::IPlugAPIBase*>(&local);
  const auto apiOffset=reinterpret_cast<uintptr_t>(api)-reinterpret_cast<uintptr_t>(&local);
  const auto localList=reinterpret_cast<uintptr_t>(module)+timerRva;
  const auto* localBuffer=*reinterpret_cast<uintptr_t**>(localList);const auto localSize=*reinterpret_cast<int*>(localList+12);require("one bounded local timer list",localSize==8&&localBuffer);
  const auto localTimer=localBuffer[0];size_t binderOffset=0;unsigned matches=0;
  for(size_t i=16;i<80;i+=8)if(*reinterpret_cast<uintptr_t*>(localTimer+i)==reinterpret_cast<uintptr_t>(api)){binderOffset=i;++matches;}
  require("unique local timer callback binding",matches==1);
  const auto remoteList=remoteModule+timerRva;const auto remoteBuffer=read<uintptr_t>(process,remoteList);const auto remoteSize=read<int>(process,remoteList+12);require("bounded task timer list",remoteSize>=8&&remoteSize<=14*8&&remoteSize%8==0);
  require("single bank dump requires one instance",argc!=5||remoteSize==8);
  for(int index=0;index<remoteSize/8;++index){
   const auto remoteTimer=read<uintptr_t>(process,remoteBuffer+index*8);const auto remoteApi=read<uintptr_t>(process,remoteTimer+binderOffset);const auto object=remoteApi-apiOffset;
   const auto expectedVtable=*reinterpret_cast<uintptr_t*>(&local)-reinterpret_cast<uintptr_t>(module)+remoteModule;
   require("remote LXR vtable identity",read<uintptr_t>(process,object)==expectedVtable);
   std::printf("TARGET pid=%lu instance=%d timer_rva=%llu binding_offset=%zu\n",pid,index,static_cast<unsigned long long>(timerRva),binderOffset);
   P16F3VST3CertificationProbe::read(process,object,h,argc==5?argv[4]:nullptr);
  }
 }
 CloseHandle(process);factory->release();end();FreeLibrary(module);std::printf("SUMMARY pass=%d fail=%d\n",passed,failed);return failed?1:0;
}
