#pragma once
// Standalone architecture/reference model only; no production includes this file.
// Original LXR table semantics Copyright 2013 Julian Schmidt; LICENSE-LXR.txt.
#include "../../../interfaces/P15M2FPersistenceContract.h"
#include "../../../dsp/ModulationContract.h"
#include "../../../dsp/VelocityRouting.h"
#include <array>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <limits>
namespace lxr::m2f_test {
using namespace m2f_proposal;
namespace mc=modulation_contract;
enum class Status {
 Ok, Disabled, Empty, TooLarge, InvalidMagic, UnsupportedVersion, InvalidHeader,
 InvalidLength, TrailingBytes, BadCrc, InvalidParameter, InvalidVoice, InvalidSource,
 InvalidTarget, InvalidOff, InvalidFlags, MetadataMismatch, VoiceMismatch,
 GlobalUnsupported, MutableDestinationUnsupported, SourceIncompatible,
 UnsupportedTarget, UnsupportedNamespace, UnsupportedProfile, TableMismatch,
 DuplicateRecord, MissingRoute, RecordOrder, UnknownMandatory, ReservedField,
 AmbiguousLegacySelector, Busy, Stale, QueueFull
};
inline const char* name(Status s) {
#define M2F_NAME(x) case Status::x:return #x
 switch(s){M2F_NAME(Ok);M2F_NAME(Disabled);M2F_NAME(Empty);M2F_NAME(TooLarge);
 M2F_NAME(InvalidMagic);M2F_NAME(UnsupportedVersion);M2F_NAME(InvalidHeader);
 M2F_NAME(InvalidLength);M2F_NAME(TrailingBytes);M2F_NAME(BadCrc);M2F_NAME(InvalidParameter);
 M2F_NAME(InvalidVoice);M2F_NAME(InvalidSource);M2F_NAME(InvalidTarget);M2F_NAME(InvalidOff);
 M2F_NAME(InvalidFlags);M2F_NAME(MetadataMismatch);M2F_NAME(VoiceMismatch);
 M2F_NAME(GlobalUnsupported);M2F_NAME(MutableDestinationUnsupported);M2F_NAME(SourceIncompatible);
 M2F_NAME(UnsupportedTarget);M2F_NAME(UnsupportedNamespace);M2F_NAME(UnsupportedProfile);
 M2F_NAME(TableMismatch);M2F_NAME(DuplicateRecord);M2F_NAME(MissingRoute);M2F_NAME(RecordOrder);
 M2F_NAME(UnknownMandatory);M2F_NAME(ReservedField);M2F_NAME(AmbiguousLegacySelector);
 M2F_NAME(Busy);M2F_NAME(Stale);M2F_NAME(QueueFull);}
#undef M2F_NAME
 return "Unknown";
}
inline bool accepted(Status s){return s==Status::Ok||s==Status::Disabled;}
inline std::uint16_t physicalKey(const mc::TargetMetadata& t) {
 std::uint16_t key=t.parameter.value();
 for(const auto& row:mc::kOriginalTargets)
  if(row.destination==t.destination)key=std::min(key,row.parameter.value());
 for(const auto& row:mc::kInternalOnlyParameters)
  if(row.destination==t.destination)key=std::min(key,row.parameter.value());
 return key;
}
inline Route off(unsigned source,unsigned owner) {
 Route r;r.source=static_cast<std::uint8_t>(source);r.owner=static_cast<std::uint8_t>(owner);return r;
}
inline Route route(unsigned source,unsigned owner,unsigned index) {
 Route r=off(source,owner);if(index==0)return r;
 r.flags=1;r.nameSpace=1;r.index=static_cast<std::uint16_t>(index);
 const auto* t=mc::lookup(mc::OriginalTargetIndex::fromRaw(index));
 if(t){r.parameter=t->parameter.value();r.type=static_cast<std::uint8_t>(t->valueType);
 r.scope=static_cast<std::uint8_t>(t->scope);r.physicalKey=physicalKey(*t);
 r.menuHint=source==1?static_cast<std::uint8_t>(owner+1):0;}
 return r;
}
inline Status validate(const Route& r) {
 if(r.owner>=6)return Status::InvalidVoice;
 if(r.source>1)return Status::InvalidSource;
 if(r.reserved)return Status::ReservedField;
 if(r.flags>1)return Status::InvalidFlags;
 if(r.flags==0){
  return r.nameSpace==0&&r.index==0&&r.parameter==0&&r.type==255&&r.scope==0&&r.physicalKey==0&&r.menuHint==0
   ?Status::Disabled:Status::InvalidOff;
 }
 if(r.nameSpace!=1)return Status::UnsupportedNamespace;
 if(r.index==0||r.index>204)return Status::InvalidTarget;
 const auto& t=*mc::lookup(mc::OriginalTargetIndex::fromRaw(r.index));
 if(r.parameter!=t.parameter.value()||r.type!=static_cast<unsigned>(t.valueType)||r.scope!=static_cast<unsigned>(t.scope)||r.physicalKey!=physicalKey(t))return Status::MetadataMismatch;
 if((r.source==0&&r.menuHint!=0)||r.menuHint>6)return Status::MetadataMismatch;
 if(t.scope!=mc::RoutingScope::Voice)return Status::GlobalUnsupported;
 if(static_cast<unsigned>(t.voice)!=r.owner)return Status::VoiceMismatch;
 if(t.runtimePolicy==mc::RuntimePolicy::RequiresDestinationMutationReview)return Status::MutableDestinationUnsupported;
 const auto p=t.parameter.value();
 if((p>=115&&p<=126)||(p>=155&&p<=190))return Status::SourceIncompatible;
 return velocityParameterSupported(t.parameter)?Status::Ok:Status::UnsupportedTarget;
}
struct Optional {std::uint32_t tag=0;std::uint16_t version=0;std::vector<std::uint8_t> payload;};
struct Snapshot {
 std::array<std::uint8_t,228> patch{};
 std::array<Route,12> routes{};
 std::vector<Optional> optional;
 bool bypass=false;
 Snapshot(){for(unsigned s=0;s<2;++s)for(unsigned v=0;v<6;++v)routes[s*6+v]=off(s,v);}
};
inline std::uint16_t u16(const std::uint8_t* p){return std::uint16_t(p[0])|(std::uint16_t(p[1])<<8);}
inline std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(u16(p))|(std::uint32_t(u16(p+2))<<16);}
inline void put16(std::uint8_t* p,std::uint16_t n){p[0]=n&255;p[1]=(n>>8)&255;}
inline void put32(std::uint8_t* p,std::uint32_t n){put16(p,n&65535);put16(p+2,n>>16);}
inline std::uint32_t crc(const std::uint8_t* p,std::size_t n,bool v2=false){
 std::uint32_t c=~0u;
 for(std::size_t i=0;i<n;++i){c^=(v2&&i>=24&&i<28)?0:p[i];
  for(unsigned b=0;b<8;++b)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}
 return ~c;
}
inline void writeRoute(std::uint8_t* p,const Route& r){
 p[0]=r.owner;p[1]=r.source;put16(p+2,r.flags);put16(p+4,r.nameSpace);put16(p+6,r.index);
 put16(p+8,r.parameter);p[10]=r.type;p[11]=r.scope;put16(p+12,r.physicalKey);p[14]=r.menuHint;p[15]=r.reserved;
}
inline Route readRoute(const std::uint8_t* p){
 Route r;r.owner=p[0];r.source=p[1];r.flags=u16(p+2);r.nameSpace=u16(p+4);r.index=u16(p+6);
 r.parameter=u16(p+8);r.type=p[10];r.scope=p[11];r.physicalKey=u16(p+12);r.menuHint=p[14];r.reserved=p[15];return r;
}
inline Status validateSnapshot(const Snapshot& s){
 for(auto v:s.patch)if(v>127)return Status::InvalidParameter;
 std::array<bool,12> seen{};
 for(unsigned i=0;i<12;++i){const auto& r=s.routes[i];auto status=validate(r);if(!accepted(status))return status;
  unsigned slot=r.source*6+r.owner;if(seen[slot])return Status::DuplicateRecord;seen[slot]=true;
  if(slot!=i)return Status::RecordOrder;}
 std::uint32_t last=1;std::size_t total=516;
 if(s.optional.size()>15)return Status::InvalidLength;
 for(const auto& e:s.optional){if(e.tag<=last)return e.tag==last?Status::DuplicateRecord:Status::RecordOrder;
  if(total>kMaximumPluginBytes-16||e.payload.size()>kMaximumPluginBytes-total-16)return Status::TooLarge;
  last=e.tag;total+=16+e.payload.size();}
 return Status::Ok;
}
inline Status encode(const Snapshot& s,const std::array<std::uint8_t,32>& tableHash,std::vector<std::uint8_t>& out,bool transport=false){
 auto status=validateSnapshot(s);if(status!=Status::Ok)return status;
 std::size_t total=516;for(const auto& e:s.optional)total+=16+e.payload.size();
 if(total>kMaximumPluginBytes)return Status::TooLarge;
 std::vector<std::uint8_t> bytes(total+(transport?4:0),0);
 auto* p=bytes.data();std::memcpy(p,"LXRS",4);put16(p+4,2);put16(p+6,32);put32(p+8,static_cast<std::uint32_t>(total));
 put16(p+12,228);put32(p+16,228);put32(p+20,static_cast<std::uint32_t>(total-260));
 std::copy(s.patch.begin(),s.patch.end(),p+32);
 put32(p+260,1);put16(p+264,1);put16(p+266,1);put32(p+268,240);
 put16(p+276,1);put16(p+278,16);put16(p+280,12);put16(p+282,1);put32(p+284,1);
 std::copy(tableHash.begin(),tableHash.end(),p+288);
 for(unsigned i=0;i<12;++i)writeRoute(p+324+i*16,s.routes[i]);
 std::size_t pos=516;
 for(const auto& e:s.optional){put32(p+pos,e.tag);put16(p+pos+4,e.version);put32(p+pos+8,static_cast<std::uint32_t>(e.payload.size()));
  std::copy(e.payload.begin(),e.payload.end(),p+pos+16);pos+=16+e.payload.size();}
 put32(p+24,crc(p,total,true));if(transport)put32(p+total,s.bypass?1:0);
 out=std::move(bytes);return Status::Ok;
}
inline Status decode(const std::vector<std::uint8_t>& bytes,const std::array<std::uint8_t,32>& hash,Snapshot& output,bool transport=false){
 if(bytes.empty())return Status::Empty;if(bytes.size()>4096)return Status::TooLarge;
 const auto* p=bytes.data();const std::size_t suffix=transport?4:0;Snapshot candidate;
 if(bytes.size()<4||std::memcmp(p,"LXRS",4)!=0){
  if(bytes.size()!=1824+suffix)return Status::InvalidMagic;
  static_assert(sizeof(double)==8&&std::numeric_limits<double>::is_iec559,"legacy x64 ABI");
  for(unsigned i=0;i<228;++i){double x;std::memcpy(&x,p+i*8,8);
   if(!std::isfinite(x)||x<0||x>127||std::floor(x)!=x)return Status::InvalidParameter;
   candidate.patch[i]=static_cast<std::uint8_t>(x);}
  if(transport)candidate.bypass=u32(p+1824)!=0;output=std::move(candidate);return Status::Ok;
 }
 if(bytes.size()<8)return Status::InvalidLength;
 auto version=u16(p+4);
 if(version==1){
  if(bytes.size()<24)return Status::InvalidLength;
  if(u16(p+6)!=24||u32(p+8)!=228||u16(p+12)!=228||u16(p+14)||u32(p+16))return Status::InvalidHeader;
  if(bytes.size()<252+suffix)return Status::InvalidLength;
  if(bytes.size()!=252+suffix)return Status::TrailingBytes;
  if(crc(p+24,228)!=u32(p+20))return Status::BadCrc;
  std::copy(p+24,p+252,candidate.patch.begin());
  for(auto x:candidate.patch)if(x>127)return Status::InvalidParameter;
  if(transport)candidate.bypass=u32(p+252)!=0;output=std::move(candidate);return Status::Ok;
 }
 if(version!=2)return Status::UnsupportedVersion;
 if(bytes.size()<32)return Status::InvalidLength;
 if(u16(p+6)!=32||u16(p+12)!=228||u16(p+14)||u32(p+16)!=228||u32(p+28))return Status::InvalidHeader;
 const std::uint32_t total=u32(p+8), ext=u32(p+20);
 if(total>4092||ext>3832)return Status::TooLarge;
 if(total<260||ext!=total-260)return Status::InvalidLength;
 if(bytes.size()<std::size_t(total)+suffix)return Status::InvalidLength;
 if(bytes.size()!=std::size_t(total)+suffix)return Status::TrailingBytes;
 if(crc(p,total,true)!=u32(p+24))return Status::BadCrc;
 std::copy(p+32,p+260,candidate.patch.begin());
 for(auto x:candidate.patch)if(x>127)return Status::InvalidParameter;
 bool hasRoute=false;std::uint32_t last=0;unsigned count=0;std::size_t pos=260;
 while(pos<total){
  if(++count>16||total-pos<16)return Status::InvalidLength;
  auto tag=u32(p+pos);auto ver=u16(p+pos+4);auto flags=u16(p+pos+6);auto n=u32(p+pos+8);
  if(!tag)return Status::ReservedField;
  if(flags>1||u32(p+pos+12))return Status::ReservedField;
  if(n>total-pos-16)return Status::InvalidLength;
  if(tag==last)return Status::DuplicateRecord;if(tag<last)return Status::RecordOrder;last=tag;
  const auto* body=p+pos+16;
  if(tag==1){
   if(ver!=1)return Status::UnsupportedVersion;
   if(flags!=1||n!=240)return Status::InvalidLength;
   if(u16(body)!=1)return Status::UnsupportedVersion;
   if(u16(body+2)!=16||u16(body+4)!=12)return Status::InvalidLength;
   if(u16(body+6)!=1)return Status::UnsupportedProfile;
   if(u32(body+8)!=1||!std::equal(hash.begin(),hash.end(),body+12))return Status::TableMismatch;
   if(u32(body+44))return Status::ReservedField;
   std::array<bool,12> seen{};
   for(unsigned i=0;i<12;++i){auto r=readRoute(body+48+i*16);auto status=validate(r);if(!accepted(status))return status;
    unsigned slot=r.source*6+r.owner;if(seen[slot])return Status::DuplicateRecord;seen[slot]=true;
    if(slot!=i)return Status::RecordOrder;candidate.routes[i]=r;}
   hasRoute=true;
  } else {
   if(flags&1)return Status::UnknownMandatory;
   Optional e;e.tag=tag;e.version=ver;e.payload.assign(body,body+n);candidate.optional.push_back(std::move(e));
  }
  pos+=16+n;
 }
 if(!hasRoute)return Status::MissingRoute;
 if(transport)candidate.bypass=u32(p+total)!=0;
 output=std::move(candidate);return Status::Ok;
}
inline double normalized(std::uint16_t code){return static_cast<double>(code)/65535.;}
inline bool plain(double x,std::uint16_t& code){
 if(!std::isfinite(x)||x<0.||x>1.)return false;
 code=static_cast<std::uint16_t>(std::round(x*65535.));return true;
}
// Sequential publication reference, NOT a production lock-free implementation.
// Ticket/cutoff/epoch rules are executable specifications for later concurrency tests.
struct Publication {
 Snapshot state;std::uint64_t epoch=0,cutoff=0,generation=0,next=1;
 std::array<std::uint64_t,12> routeTickets{};bool busy=false;
 std::uint64_t ticket(){return next++;}
 Status restore(const Snapshot& candidate,std::uint64_t order){
  auto s=validateSnapshot(candidate);if(s!=Status::Ok)return s;
  if(busy)return Status::Busy;if(order<=cutoff)return Status::Stale;
  Snapshot staged=candidate;state=std::move(staged);cutoff=order;++epoch;++generation;routeTickets.fill(order);return Status::Ok;
 }
 Status setRoute(const Route& r,std::uint64_t order,std::uint64_t expectedEpoch){
  auto s=validate(r);if(!accepted(s))return s;if(busy)return Status::Busy;
  unsigned slot=r.source*6+r.owner;
  if(expectedEpoch!=epoch||order<=cutoff||order<=routeTickets[slot])return Status::Stale;
  state.routes[slot]=r;routeTickets[slot]=order;++generation;return Status::Ok;
 }
};
template <std::size_t Capacity> struct PendingRoutes {
 struct Item {Route value;std::uint64_t ticket=0,epoch=0;};
 std::array<Item,Capacity> items{};std::size_t read=0,write=0,count=0;
 Status enqueue(const Route& r,std::uint64_t ticket,std::uint64_t epoch){
  auto s=validate(r);if(!accepted(s))return s;
  if(count==Capacity)return Status::QueueFull;
  items[write]={r,ticket,epoch};write=(write+1)%Capacity;++count;return Status::Ok;
 }
 Status drainOne(Publication& p){
  if(!count)return Status::Empty;auto& i=items[read];auto result=p.setRoute(i.value,i.ticket,i.epoch);
  if(result==Status::Busy)return result;read=(read+1)%Capacity;--count;return result;
 }
};
inline Status legacySelector(unsigned value){return value<=127?Status::AmbiguousLegacySelector:Status::InvalidParameter;}
} // namespace lxr::m2f_test
