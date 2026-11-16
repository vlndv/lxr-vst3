// Actual exact-DLL production file parser. REAPER UI ingress is a separate gate.
#include "../vendor/iPlug2/Examples/LXR/LXR.h"
#define main ExistingP12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main
#ifdef fopen
#undef fopen
#endif
#include <string>
#include <chrono>
struct P16F3VST3CertificationProbe {
 static LXR& plugin(HostInstance& h){return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(static_cast<SingleComponentEffect*>(h.component)));}
 static bool import(HostInstance& h,const char* p,HMODULE m,uintptr_t r){return reinterpret_cast<bool(*)(LXR*,const char*)>(reinterpret_cast<uintptr_t>(m)+r)(&plugin(h),p);}
 static bool hold(HostInstance& h,lxr::RoutingStateAuthority::Guard& g){return plugin(h).mRoutingState.tryAcquire(g);}
 static void release(HostInstance& h,lxr::RoutingStateAuthority::Guard& g){plugin(h).mRoutingState.release(g);}
};
namespace {
void file(const std::string& p,const std::vector<uint8_t>& b){auto* f=(std::fopen)(p.c_str(),"wb");require("open bounded fixture file",f!=nullptr);require("write bounded fixture file",std::fwrite(b.data(),1,b.size(),f)==b.size());std::fclose(f);}
void checksum(std::vector<uint8_t>& b){lxr::sequencer::state::Put32(b.data()+20,lxr::sequencer::state::Crc(b.data(),b.size(),20));}
}
int main(int argc,char**argv){if(argc!=5)return 2;auto m=LoadLibraryA(argv[1]);require("exact DLL",m!=nullptr);auto init=reinterpret_cast<bool(*)()>(GetProcAddress(m,"InitDll"));auto end=reinterpret_cast<bool(*)()>(GetProcAddress(m,"ExitDll"));auto getFactory=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(m,"GetPluginFactory"));require("exports",init&&end&&getFactory&&init());auto* factory=getFactory();PClassInfo info{};bool found=false;for(int i=0;i<factory->countClasses();++i)if(factory->getClassInfo(i,&info)==kResultOk&&std::strcmp(info.category,kVstAudioEffectClass)==0){found=true;break;}require("audio class",found);
 {
  HostInstance h(factory,info.cid);h.setup(48000);const auto rva=std::strtoull(argv[4],nullptr,10);
  auto* f=(std::fopen)(argv[2],"rb");require("independent fixture input",f!=nullptr);std::vector<uint8_t> bank(50449);require("exact fixture input",std::fread(bank.data(),1,bank.size(),f)==bank.size()&&std::fgetc(f)==EOF);std::fclose(f);
  const auto t0=std::chrono::steady_clock::now();require("production valid bank import",P16F3VST3CertificationProbe::import(h,argv[2],m,rva));auto importUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-t0).count();
  const auto accepted=h.saveState();
  for(unsigned test=0;test<8;++test){auto bad=bank;
   if(test==0)bad.clear();if(test==1)bad.resize(50448);if(test==2)bad.push_back(0);if(test==3)bad[1000]^=1;
   if(test==4){bad[4]=2;checksum(bad);}if(test==5){bad[33]=255;checksum(bad);}if(test==6){bad[28]=1;checksum(bad);}if(test==7){lxr::sequencer::state::Put32(bad.data()+8,0xffffffff);checksum(bad);}
   const auto path=std::string(argv[3])+"/rejected_"+std::to_string(test)+".lxqb";file(path,bad);
   check("malformed file rejects",!P16F3VST3CertificationProbe::import(h,path.c_str(),m,rva));check("rejected import preserves complete project",h.saveState()==accepted);
  }
  lxr::RoutingStateAuthority::Guard guard;require("hold common authority barrier",P16F3VST3CertificationProbe::hold(h,guard));const bool busy=P16F3VST3CertificationProbe::import(h,argv[2],m,rva);P16F3VST3CertificationProbe::release(h,guard);check("busy file import returns without partial publication",!busy&&h.saveState()==accepted);
  std::printf("IMPORT bytes=50449 valid_import_us=%lld\n",static_cast<long long>(importUs));
 }
 factory->release();end();FreeLibrary(m);std::printf("SUMMARY pass=%d fail=%d\n",passed,failed);return failed?1:0;
}
