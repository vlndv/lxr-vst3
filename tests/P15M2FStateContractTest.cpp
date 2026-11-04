// Architecture/contract verification only. Production serialization is untouched.
// Original table semantics Copyright 2013 Julian Schmidt; LICENSE-LXR.txt.
#include "reference/persistence/StateContractModel.h"
#include "reference/modulation/original_contract_fixture.h"
#include "../vendor/iPlug2/Examples/LXR/LXRState.h"
#include "IPlugParameter.h"
#ifdef fopen
#undef fopen // WDL's Windows UTF-8 macro must not rewrite std::fopen in this test.
#endif
#include <cstdio>
#include <string>
#include <cstdlib>
using namespace lxr::m2f_test;
namespace {
int passed=0,failed=0;const char* group="startup";
void check(const char* what,bool ok){if(ok)++passed;else{++failed;std::printf("FAIL [%s] %s\n",group,what);}}
void status(const char* what,Status actual,Status expected){
 if(actual!=expected)std::printf("DETAIL expected=%s actual=%s\n",name(expected),name(actual));
 check(what,actual==expected);
}
std::vector<std::uint8_t> read(const std::string& p){
 FILE* f=std::fopen(p.c_str(),"rb");if(!f)return {};std::fseek(f,0,SEEK_END);long n=std::ftell(f);std::rewind(f);
 std::vector<std::uint8_t> b(static_cast<std::size_t>(n));if(std::fread(b.data(),1,b.size(),f)!=b.size())b.clear();std::fclose(f);return b;
}
std::array<std::uint8_t,32> hash{};
void recalc(std::vector<std::uint8_t>& b){put32(b.data()+24,crc(b.data(),u32(b.data()+8),true));}
bool same(const Snapshot& a,const Snapshot& b){std::vector<std::uint8_t> x,y;return encode(a,hash,x,true)==Status::Ok&&encode(b,hash,y,true)==Status::Ok&&x==y;}
void reject(const char* text,const std::vector<std::uint8_t>& b,Status expect,bool transport=false){
 Snapshot sentinel;sentinel.patch.fill(73);sentinel.routes[0]=route(0,0,2);Snapshot before=sentinel;
 status(text,decode(b,hash,sentinel,transport),expect);check("rejection leaves entire state intact",same(sentinel,before));
}
unsigned lastFor(unsigned v){unsigned found=0;for(unsigned i=1;i<205;++i)if(validate(route(0,v,i))==Status::Ok)found=i;return found;}
void wire(const std::string& dir){
 group="format";auto golden=read(dir+"/v2_active.bin");auto offBytes=read(dir+"/v2_off.bin");
 const std::uint8_t crcVector[]={'1','2','3','4','5','6','7','8','9'};
 check("published IEEE CRC check vector",crc(crcVector,9)==0xcbf43926u);
 check("independent golden lengths",golden.size()==520&&offBytes.size()==516);
 Snapshot s;for(unsigned i=0;i<228;++i)s.patch[i]=(i*37+11)%128;s.bypass=true;
 for(unsigned src=0;src<2;++src)for(unsigned v=0;v<6;++v)s.routes[src*6+v]=route(src,v,lastFor(v));
 check("highest profile-1 active identity 203",s.routes[11].index==203);
 std::vector<std::uint8_t> b;status("encode independent reference",encode(s,hash,b,true),Status::Ok);check("byte equality independent Python fixture",b==golden);
 std::vector<std::uint8_t> again;encode(s,hash,again,true);check("deterministic encoding",b==again);
 check("little-endian version and lengths",b[4]==2&&b[5]==0&&b[8]==4&&b[9]==2&&b[10]==0&&b[11]==0&&b[20]==0&&b[21]==1);
 Snapshot decoded;status("independent golden decode",decode(golden,hash,decoded,true),Status::Ok);check("all patch and route data",same(decoded,s));
 check("preserve LFO destination voice hint separately",decoded.routes[11].menuHint==6);
 Snapshot offState;for(unsigned i=0;i<228;++i)offState.patch[i]=(i*37+11)%128;
 encode(offState,hash,b);check("canonical off fixture",b==offBytes);
 status("off fixture decode",decode(offBytes,hash,decoded),Status::Ok);
 for(const auto& r:decoded.routes)check("off explicit and disabled",validate(r)==Status::Disabled);
 // A full-size optional record exercises the exact bounded stream ceiling.
 offState.optional.push_back({2,1,std::vector<std::uint8_t>(3560,0x5a)});
 status("maximum allowed plugin length",encode(offState,hash,b,true),Status::Ok);check("stream maximum 4096",b.size()==4096);
 status("maximum decode",decode(b,hash,decoded,true),Status::Ok);
 offState.optional[0].payload.push_back(0);status("one byte over ceiling",encode(offState,hash,b,true),Status::TooLarge);
 offState.optional[0].payload.pop_back();offState.optional.push_back({3,1,{}});
 status("no unsigned remaining-length underflow",encode(offState,hash,b),Status::TooLarge);
}
void legacy(const std::string& dir){
 group="legacy";
 for(const char* file:{"v1.bin","legacy.bin"}){
  auto b=read(dir+"/"+file);Snapshot s;status("independent old fixture",decode(b,hash,s,true),Status::Ok);
  std::array<std::uint8_t,228> production{};auto old=lxr::state::Decode(b.data(),b.size(),production,true);
  check("unchanged production v1/legacy reader agrees",static_cast<bool>(old)&&production==s.patch);
  check("legacy bypass preserved",s.bypass);
  for(unsigned i=0;i<228;++i)check("all 228 original bytes including opaque selectors",s.patch[i]==(i*37+11)%128);
  for(const auto& r:s.routes)check("no inferred routes",validate(r)==Status::Disabled);
  std::vector<std::uint8_t> v2;encode(s,hash,v2,true);Snapshot back;decode(v2,hash,back,true);check("migration to v2 internal roundtrip",same(s,back));
  put32(b.data()+b.size()-4,0);status("bypass off decode",decode(b,hash,s,true),Status::Ok);check("bypass remains off",!s.bypass);
 }
 auto doubles=read(dir+"/legacy.bin");
 for(double bad:{-.1,127.5,128.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
  auto b=doubles;std::memcpy(b.data()+149*8,&bad,8);reject("invalid legacy double",b,Status::InvalidParameter,true);
 }
 auto old=read(dir+"/v1.bin");old[24+149]=128;put32(old.data()+20,crc(old.data()+24,228));reject("v1 8-bit not accepted",old,Status::InvalidParameter,true);
 for(unsigned value=0;value<128;++value){Snapshot s;s.patch.fill(static_cast<std::uint8_t>(value));
  status("opaque selector has explicit diagnostic",legacySelector(value),Status::AmbiguousLegacySelector);
  std::array<std::uint8_t,lxr::state::kEncodedSize> b{};check("unchanged v1 encode",lxr::state::Encode(s.patch,b));
  std::vector<std::uint8_t> vec(b.begin(),b.end());Snapshot migrated;decode(vec,hash,migrated);
  check("all opaque byte values stay opaque",migrated.patch==s.patch&&std::all_of(migrated.routes.begin(),migrated.routes.end(),[](const Route& r){return validate(r)==Status::Disabled;}));
 }
 status("invalid opaque selector byte",legacySelector(128),Status::InvalidParameter);
}
void routing(){
 group="routing";lxr_ref_initialize_stm32_table();check("original table length",lxr_ref_target_count()==205);
 unsigned total=0,above=0;std::array<unsigned,6> ownerCounts{};
 for(unsigned src=0;src<2;++src)for(unsigned owner=0;owner<6;++owner)for(unsigned idx=0;idx<205;++idx){
  auto r=route(src,owner,idx);auto actual=validate(r);
  check("original PAR not reordered",idx==0||r.parameter==lxr_ref_parameter(idx));
  check("original type not reinterpreted",idx==0||r.type==lxr_ref_parameter_type(r.parameter));
  const auto native=lxr::validateVelocityDestination({mc::SourceKind::Velocity,mc::voiceFromZeroBased(owner)},mc::OriginalTargetIndex::fromRaw(idx));
  check("standalone profile exactly native verified subset",accepted(actual)==(native==lxr::VelocityRouteStatus::Ok));
  if(src==0&&idx&&actual==Status::Ok){++total;++ownerCounts[owner];if(idx>127)++above;}
  if(actual==Status::Ok){
   Snapshot s;s.routes[src*6+owner]=r;std::vector<std::uint8_t> bytes;Snapshot out;
   check("all accepted full-width records encode",encode(s,hash,bytes)==Status::Ok);
   check("all accepted full-width records decode",decode(bytes,hash,out)==Status::Ok&&out.routes[src*6+owner].index==idx);
   check("physical alias key matches original C storage",lxr_ref_same_storage(r.parameter,r.physicalKey)!=0);
  }
 }
 check("107 supported rows",total==107);check("39 indices over 127",above==39);
 for(unsigned v=0;v<6;++v)check("six-voice coverage",ownerCounts[v]==(v==5?17u:18u));
 auto r=route(0,0,2);r.owner=6;status("bad voice",validate(r),Status::InvalidVoice);
 r=route(0,0,2);r.source=2;status("bad source",validate(r),Status::InvalidSource);
 r=route(0,0,205);status("index 205",validate(r),Status::InvalidTarget);
 r=route(0,1,2);status("cross voice",validate(r),Status::VoiceMismatch);
 r=route(0,0,1);status("global rejected",validate(r),Status::GlobalUnsupported);
 for(unsigned i=1;i<205;++i){const auto& t=*mc::lookup(mc::OriginalTargetIndex::fromRaw(i));
  if(t.runtimePolicy==mc::RuntimePolicy::RequiresDestinationMutationReview){
   auto q=route(0,static_cast<unsigned>(t.voice),i);status("unsafe pointer storage",validate(q),Status::MutableDestinationUnsupported);}}
 r=route(0,0,2);r.nameSpace=2;status("future STM32 not silently enabled",validate(r),Status::UnsupportedNamespace);
 r=route(0,0,2);r.parameter=99;status("metadata mismatch",validate(r),Status::MetadataMismatch);
 r=off(0,0);r.index=2;status("off cannot retain active identity",validate(r),Status::InvalidOff);
 r=route(0,0,2);r.flags=2;status("reserved flags",validate(r),Status::InvalidFlags);
 r=route(0,0,25);status("LFO source-control target deferred",validate(r),Status::SourceIncompatible);
 Snapshot s;s.routes[1]=s.routes[0];status("duplicate source-owner",validateSnapshot(s),Status::DuplicateRecord);
 std::swap(s.routes[0],s.routes[6]);status("source position must be canonical",validateSnapshot(s),Status::RecordOrder);
}
void corrupt(const std::string& dir){
 group="corruption";auto g=read(dir+"/v2_off.bin");reject("empty",{},Status::Empty);
 for(unsigned n=4;n<8;++n)reject("short prefix",std::vector<std::uint8_t>(g.begin(),g.begin()+n),Status::InvalidLength);
 for(unsigned n=8;n<g.size();++n)reject("every truncated header/payload",std::vector<std::uint8_t>(g.begin(),g.begin()+n),Status::InvalidLength);
 reject("oversize",std::vector<std::uint8_t>(4097,0),Status::TooLarge);
 auto b=g;put16(b.data()+4,3);reject("unknown top-level version",b,Status::UnsupportedVersion);
 b=g;b[32]^=1;reject("payload CRC",b,Status::BadCrc);
 b=g;b[14]=1;reject("reserved header flags",b,Status::InvalidHeader);
 b=g;put32(b.data()+20,257);reject("wrong extension length",b,Status::InvalidLength);
 b=g;b.push_back(0);reject("trailing byte",b,Status::TrailingBytes);
 b=g;b[32]=128;recalc(b);reject("non seven-bit patch",b,Status::InvalidParameter);
 b=g;put16(b.data()+264,2);recalc(b);reject("known TLV version mismatch",b,Status::UnsupportedVersion);
 b=g;put16(b.data()+276,2);recalc(b);reject("module version mismatch",b,Status::UnsupportedVersion);
 b=g;put16(b.data()+282,2);recalc(b);reject("unsupported profile",b,Status::UnsupportedProfile);
 b=g;b[288]^=1;recalc(b);reject("table identity mismatch",b,Status::TableMismatch);
 b=g;b[320]=1;recalc(b);reject("module reserved",b,Status::ReservedField);
 b=g;b[272]=1;recalc(b);reject("TLV reserved",b,Status::ReservedField);
 b=g;put32(b.data()+268,239);recalc(b);reject("known TLV length",b,Status::InvalidLength);
 b=g;std::copy(b.begin()+324,b.begin()+340,b.begin()+340);recalc(b);reject("duplicate route record",b,Status::DuplicateRecord);
 b=g;b[324]=6;recalc(b);reject("bad voice in wire",b,Status::InvalidVoice);
 b=g;b[325]=2;recalc(b);reject("bad source in wire",b,Status::InvalidSource);
 b=g;b[339]=1;recalc(b);reject("reserved route field",b,Status::ReservedField);
 b=g;put32(b.data()+268,0xffffffffu);recalc(b);reject("length overflow guarded",b,Status::InvalidLength);
 b=g;b.resize(260);put32(b.data()+8,260);put32(b.data()+20,0);recalc(b);reject("missing routing module",b,Status::MissingRoute);
}
void extensions(){
 group="extensions";Snapshot s;s.optional={{2,77,{1,2,3,4}},{500,9,{5,6}}};std::vector<std::uint8_t> b;
 status("multiple optional extensions",encode(s,hash,b),Status::Ok);Snapshot out;
 status("unknown optional accepted",decode(b,hash,out),Status::Ok);std::vector<std::uint8_t> again;encode(out,hash,again);
 check("unknown optional bytes preserved",b==again&&out.optional[0].version==77);
 auto invalid=b;put16(invalid.data()+516+6,1);recalc(invalid);reject("unknown mandatory",invalid,Status::UnknownMandatory);
 invalid=b;put16(invalid.data()+516+6,2);recalc(invalid);reject("unknown reserved flags",invalid,Status::ReservedField);
 invalid=b;put32(invalid.data()+516+12,1);recalc(invalid);reject("unknown reserved fields",invalid,Status::ReservedField);
 invalid=b;put32(invalid.data()+536,2);recalc(invalid);reject("duplicate optional tag",invalid,Status::DuplicateRecord);
 s.optional.clear();for(unsigned i=2;i<17;++i)s.optional.push_back({i,1,{}});
 check("16 total TLVs permitted",encode(s,hash,b)==Status::Ok&&decode(b,hash,out)==Status::Ok);
 s.optional.push_back({17,1,{}});status("17 TLVs rejected",encode(s,hash,b),Status::InvalidLength);
}
void host(){
 group="host normalization";iplug::IParam p;p.InitInt("Proposed route",0,0,65535);
 check("actual pinned IParam range",p.GetRange()==65535);
 for(unsigned code=0;code<65536;++code){
  auto raw=static_cast<std::uint16_t>(code);double x=normalized(raw);std::uint16_t out=0;
  check("exact model double roundtrip",plain(x,out)&&out==code);
  check("actual pinned IParam double roundtrip",p.FromNormalized(p.ToNormalized(code))==code);
  double floatNormalized=static_cast<float>(x);
  check("float32 normalized host roundtrip",plain(floatNormalized,out)&&out==code&&p.FromNormalized(floatNormalized)==code);
  if(code)check("no public selector alias",x>normalized(static_cast<std::uint16_t>(code-1)));
 }
 for(unsigned code:{0u,127u,128u,204u,205u,32768u,65535u}){std::uint16_t back;
  check("critical full range values",plain(normalized(static_cast<std::uint16_t>(code)),back)&&back==code);}
 std::uint16_t sentinel=42;
 for(double x:{-.1,1.1,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
  check("nonfinite/outside normalized rejects before framework clamp",!plain(x,sentinel)&&sentinel==42);
 for(unsigned code:{127u,128u,203u,204u}){
  auto r=route(1,5,code);check("numeric conversion does not grant availability",code==203?validate(r)==Status::Ok:!accepted(validate(r)));}
 for(unsigned s=0;s<2;++s)for(unsigned v=0;v<6;++v)check("new stable IDs preserve legacy IDs",228+s*6+v>=228&&228+s*6+v<=239);
}
void consistency(){
 group="publication consistency";
 Publication p;Snapshot first;first.patch.fill(11);first.routes[0]=route(0,0,lastFor(0));auto r=p.ticket();
 status("whole restore",p.restore(first,r),Status::Ok);check("complete patch and routes",same(p.state,first));
 auto bad=first;bad.patch[227]=128;auto before=p.state;auto gen=p.generation;
 status("invalid candidate rejected before mutation",p.restore(bad,p.ticket()),Status::InvalidParameter);check("no partial candidate",same(p.state,before)&&p.generation==gen);
 auto oldEpoch=p.epoch;auto oldTicket=p.ticket();Snapshot second;second.patch.fill(99);auto restoreTicket=p.ticket();
 status("newer reload",p.restore(second,restoreTicket),Status::Ok);
 status("old route cannot resurrect",p.setRoute(first.routes[0],oldTicket,oldEpoch),Status::Stale);
 check("reload still all off",same(p.state,second));
 auto newer=p.ticket();auto earlier=newer-1;status("post restore edit",p.setRoute(first.routes[0],newer,p.epoch),Status::Ok);
 status("older same-field ticket",p.setRoute(off(0,0),earlier,p.epoch),Status::Stale);
 before=p.state;gen=p.generation;p.busy=true;
 status("contended callback no spin",p.setRoute(off(0,0),p.ticket(),p.epoch),Status::Busy);
 status("restore busy no mutation",p.restore(second,p.ticket()),Status::Busy);
 check("busy retains complete state",same(p.state,before)&&p.generation==gen);p.busy=false;
 Publication other;check("multi-instance isolation",other.state.patch[0]==0&&p.state.patch[0]==99);
 PendingRoutes<4> queue;
 for(unsigned i=0;i<4;++i)status("bounded pending acceptance",queue.enqueue(off(0,0),p.ticket(),p.epoch),Status::Ok);
 status("overflow rejects newest",queue.enqueue(route(0,0,2),p.ticket(),p.epoch),Status::QueueFull);
 p.busy=true;status("busy drain leaves pending item",queue.drainOne(p),Status::Busy);check("pending count retained",queue.count==4);p.busy=false;
 status("restore supersedes pending route edits",p.restore(second,p.ticket()),Status::Ok);
 for(unsigned i=0;i<4;++i)status("pending old epoch cannot resurrect",queue.drainOne(p),Status::Stale);
 check("all old pending edits discarded",queue.count==0&&same(p.state,second));
 // Enumerate all six orders of restore/old edit/new edit and verify ticket-
 // cutoff semantics without pretending to prove C++ memory ordering.
 std::array<unsigned,3> order{{0,1,2}};
 do{
  Publication q;auto old=q.ticket(), restore=q.ticket(), newerTicket=q.ticket();Snapshot fresh;fresh.patch.fill(17);
  for(auto op:order){if(op==0)q.setRoute(route(0,0,lastFor(0)),old,0);
   if(op==1)q.restore(fresh,restore);
   if(op==2)q.setRoute(route(0,0,2),newerTicket,1);}
  check("restore always supersedes pre-reload identity",q.state.patch[0]==17&&q.state.routes[0].index!=lastFor(0));
 }while(std::next_permutation(order.begin(),order.end()));
}
}
int main(int argc,char** argv){
 if(argc!=2)return 2;std::string dir=argv[1];auto h=read(dir+"/table_sha256.bin");if(h.size()!=32)return 2;std::copy(h.begin(),h.end(),hash.begin());
 wire(dir);legacy(dir);routing();corrupt(dir);extensions();host();consistency();
 std::printf("SUMMARY pass=%d fail=%d original_rows=205 supported_rows=107 supported_above127=39 owners=6 routes=12 host_codes=65536\n",passed,failed);
 return failed?1:0;
}
