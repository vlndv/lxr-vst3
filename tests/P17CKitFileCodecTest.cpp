// Original LXR .snd codec differential/property/I/O tests. LICENSE-LXR.txt.
#include "../interfaces/KitFileCodec.h"
#include "../interfaces/KitFileIO.h"
#include "../dsp/RouteHostControls.h"
#include "reference/presets/original_preset_adapter.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

static std::atomic<std::size_t> allocations {0};
void* operator new(std::size_t n) {allocations.fetch_add(1,std::memory_order_relaxed);if(void* p=std::malloc(n?n:1))return p;std::abort();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
using namespace lxr;
using namespace lxr::kit;
namespace file=lxr::kit::snd;
namespace mc=lxr::modulation_contract;
static std::atomic<unsigned> passed{0},failed{0};
#define CHECK(x) do {if(x)++passed;else {++failed;std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);}} while(false)
static bool same(const RawKit& a,const RawKit& b) {return a.name==b.name && a.values==b.values;}
static bool same(const CanonicalKit& a,const CanonicalKit& b) {
    if(a.name!=b.name)return false;
    for(std::uint16_t id=0;id<228;++id){std::uint16_t x=0,y=0;ReadField(a,id,x);ReadField(b,id,y);if(x!=y)return false;}return true;
}
static unsigned originalComparisons=0,roundtrips=0,fuzzCases=0;
static void original(const RawKit& raw,const file::Bytes& encoded) {
    std::array<std::uint8_t,273> values {};std::copy(raw.values.begin(),raw.values.end(),values.begin());
    file::Bytes out {};char name[16] {};
    CHECK(p17_original_write(P17_SOUND,values.data(),values.size(),raw.name.data(),out.data(),out.size(),name)==236);
    CHECK(std::strcmp(name,"p007.snd")==0);CHECK(encoded==out);++originalComparisons;
}
static void archive() {
    RawKit marker {};marker.values.fill(17);file::Bytes bytes {};bytes.fill(93);
    for(std::size_t n=0;n<236;++n){auto out=marker;CHECK(file::DecodeRaw(bytes.data(),n,out).error==file::Error::Truncated);CHECK(same(out,marker));}
    for(auto n:std::array<std::size_t,3>{{237,1000000,std::numeric_limits<std::size_t>::max()}}){auto out=marker;CHECK(file::DecodeRaw(bytes.data(),n,out).error==file::Error::Oversized);CHECK(same(out,marker));}
    auto out=marker;CHECK(file::DecodeRaw(nullptr,236,out).error==file::Error::InvalidArgument);CHECK(same(out,marker));
    CHECK(file::DecodeRaw(nullptr,0,out).error==file::Error::Truncated);
    auto can=MakeCanonicalInitializationKit();const auto saved=can;
    CHECK(file::DecodeCanonical(nullptr,236,can).error==file::Error::InvalidArgument);CHECK(same(can,saved));
    std::array<std::uint8_t,268> guarded {};guarded.fill(0xa5);
    for(std::size_t n=0;n<236;++n){const auto before=guarded;CHECK(file::EncodeRaw(marker,guarded.data()+16,n).error==file::Error::BufferTooSmall);CHECK(guarded==before);}
    CHECK(file::EncodeRaw(marker,nullptr,236).error==file::Error::InvalidArgument);
    CHECK(file::EncodeRaw(marker,guarded.data()+16,236));
    for(unsigned i=0;i<16;++i)CHECK(guarded[i]==0xa5 && guarded[252+i]==0xa5);
    // Every byte/position, including unrestricted name bytes, source-reserved
    // bytes and unnormalized values: independent original writer, not just self roundtrip.
    for(unsigned pos=0;pos<236;++pos)for(unsigned value=0;value<256;++value) {
        auto expected=MakeFirmwareInitializationKit();
        if(pos<8)expected.name[pos]=static_cast<std::uint8_t>(value);else expected.values[pos-8]=static_cast<std::uint8_t>(value);
        file::Bytes input {};input[pos]=static_cast<std::uint8_t>(value);RawKit raw {};
        CHECK(file::DecodeRaw(input.data(),input.size(),raw));CHECK(same(raw,expected));
        file::Bytes result {};CHECK(file::EncodeRaw(raw,result.data(),result.size()));CHECK(result==input);original(raw,result);++roundtrips;
    }
    // A .snd has no header/version/CRC: changing its first8bytes is name data.
    file::Bytes text {};text[0]='V';text[1]='S';text[2]='T';text[3]='3';text[7]=255;
    CHECK(file::DecodeCanonical(text.data(),text.size(),can));CHECK(can.name[7]==255);
}
static void canonical() {
    file::Bytes bytes {};auto initial=MakeCanonicalInitializationKit();
    CHECK(file::CheckRepresentability(initial));CHECK(file::EncodeCanonical(initial,bytes.data(),bytes.size()));
    CanonicalKit output {};CHECK(file::DecodeCanonical(bytes.data(),bytes.size(),output));CHECK(same(output,initial));
    for(const auto& f:kFields) {
        RawKit raw {};raw.values[f.id]=static_cast<std::uint8_t>(f.maximum);CanonicalKit source {};
        CHECK(DecodeRawKit(raw,source));
        if(f.kind==FieldKind::Selector)continue;
        CHECK(file::EncodeCanonical(source,bytes.data(),bytes.size()));CHECK(file::DecodeCanonical(bytes.data(),bytes.size(),output));CHECK(same(source,output));
        if(f.maximum<255) {
            file::Bytes bad {};bad[8+f.id]=static_cast<std::uint8_t>(f.maximum+1);const auto old=output;
            CHECK(!file::DecodeCanonical(bad.data(),bad.size(),output));CHECK(same(output,old));
        }
    }
    for(unsigned kind=0;kind<2;++kind)for(unsigned owner=0;owner<6;++owner)for(unsigned index=0;index<205;++index) {
        RawKit raw {};const unsigned id=(kind?167:149)+owner;raw.values[id]=static_cast<std::uint8_t>(index);
        for(unsigned hint=0;hint<=(kind?6u:0u);++hint) {
            raw.values[161+owner]=static_cast<std::uint8_t>(hint);CanonicalKit source {};CHECK(DecodeRawKit(raw,source));
            CHECK(file::EncodeRaw(raw,bytes.data(),bytes.size()));const auto input=bytes;
            const bool allowed=static_cast<bool>(route_host::decode(route_host::parameterForSlot(kind*6+owner),route_host::RouteToken16::fromRaw(index)));
            const auto before=output;const auto decoded=file::DecodeCanonical(bytes.data(),bytes.size(),output);
            CHECK(static_cast<bool>(decoded)==allowed);
            file::Bytes encoded {};encoded.fill(0xa7);const auto saved=encoded;
            const auto result=file::EncodeCanonical(source,encoded.data(),encoded.size());CHECK(static_cast<bool>(result)==allowed);
            if(allowed){CHECK(same(source,output));CHECK(encoded==input);original(raw,encoded);++roundtrips;}
            else {CHECK(result.error==file::Error::UnsupportedRoute && result.field==id);CHECK(encoded==saved);CHECK(same(output,before));}
        }
    }
    // The actual firmware normalizes on load. Archival decoding preserves the
    // pre-normalized bytes; strict interpretation rejects invalid selectors.
    file::Bytes bad {};bad[8+149]=255;bad[8+161]=255;
    std::uint8_t base[228] {},target[228] {};CHECK(p17_original_read_sound(bad.data(),bad.size(),0,base,target)==1);
    CHECK(base[149]==0 && base[161]==1);RawKit archived {};CHECK(file::DecodeRaw(bad.data(),bad.size(),archived));CHECK(archived.values[149]==255 && archived.values[161]==255);
    const auto old=output;auto result=file::DecodeCanonical(bad.data(),bad.size(),output);CHECK(result.error==file::Error::InvalidSelector && result.field==149);CHECK(same(output,old));
    auto routes=MakeDisabledRoutingState().routes;
    for(unsigned slot=0;slot<12;++slot)for(unsigned index=0;index<=204;++index) {
        const auto accepted=route_host::decode(route_host::parameterForSlot(slot),route_host::RouteToken16::fromRaw(index));if(!accepted)continue;
        auto active=routes;active[slot]=accepted.record;auto provenance=initial;provenance.hiHat.voice_lfo6=3;const auto before=provenance;
        CHECK(file::EncodeCanonical(provenance,active,bytes.data(),bytes.size()));CHECK(same(before,provenance));
        CHECK(bytes[8+(slot<6?149:167)+slot%6]==index);
        if(slot!=11 || index==0)CHECK(bytes[8+166]==3); // OFF has independent hint provenance.
    }
    for(unsigned slot=0;slot<12;++slot)for(std::uint16_t index:std::array<std::uint16_t,4>{{205,255,32768,65535}}) {
        auto active=routes;active[slot].flags=1;active[slot].identityNamespace=1;active[slot].originalIndex=index;
        bytes.fill(61);const auto before=bytes;const auto result=file::EncodeCanonical(initial,active,bytes.data(),bytes.size());
        CHECK(result.error==file::Error::NonrepresentableRoute);CHECK(result.field==(slot<6?149:167)+slot%6 && result.route==slot);CHECK(bytes==before);
    }
    auto active=routes;active[0].flags=1;active[0].identityNamespace=2;active[0].originalIndex=0x8008;
    bytes.fill(71);const auto before=bytes;CHECK(file::EncodeCanonical(initial,active,bytes.data(),bytes.size()).error==file::Error::NonrepresentableRoute);CHECK(bytes==before);
    active=routes;active[4].owner=5;CHECK(file::EncodeCanonical(initial,active,bytes.data(),bytes.size()).error==file::Error::InvalidRoute);CHECK(bytes==before);
    auto all=routes;RawKit expected {};
    for(unsigned slot=0;slot<12;++slot) {
        constexpr unsigned indices[]{2,36,70,104,137,171};const unsigned owner=slot%6;
        all[slot]=route_host::decode(route_host::parameterForSlot(slot),route_host::RouteToken16::fromRaw(indices[owner])).record;
        expected.values[(slot<6?149:167)+owner]=static_cast<std::uint8_t>(indices[owner]);
        if(slot>=6)expected.values[161+owner]=static_cast<std::uint8_t>(owner+1);
    }
    CHECK(file::CheckRepresentability(initial,all));CHECK(file::EncodeCanonical(initial,all,bytes.data(),bytes.size()));
    RawKit complete {};CHECK(file::DecodeRaw(bytes.data(),bytes.size(),complete));CHECK(same(expected,complete));original(expected,bytes);
    all[11].identityNamespace=2;bytes.fill(71);const auto rejected=bytes;
    const auto last=file::EncodeCanonical(initial,all,bytes.data(),bytes.size());
    CHECK(last.error==file::Error::NonrepresentableRoute && last.field==172 && last.route==11);CHECK(bytes==rejected);
    CHECK(file::EncodeCanonical(initial,nullptr,236).error==file::Error::InvalidArgument);
    bytes.fill(71);const auto shortBefore=bytes;
    CHECK(file::EncodeCanonical(initial,bytes.data(),235).error==file::Error::BufferTooSmall);CHECK(bytes==shortBefore);
    auto invalid=initial;invalid.drum1.vel_dest_1=StoredSelector::fromRaw(205);
    CHECK(file::EncodeCanonical(invalid,bytes.data(),bytes.size()).error==file::Error::InvalidSelector);CHECK(bytes==before);
    // Explicit rejection of overlapping output/source; no object-image export.
    const auto originalKit=initial;CHECK(file::EncodeCanonical(initial,initial.name.data(),236).error==file::Error::InvalidArgument);CHECK(same(initial,originalKit));
}
static std::uint32_t randomState=0x37c0de;
static std::uint32_t randomWord(){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;}
static void properties() {
    for(unsigned n=0;n<12000;++n) {
        std::array<std::uint8_t,256> bytes {};for(auto& b:bytes)b=static_cast<std::uint8_t>(randomWord());
        const std::size_t length=n%3==0?236:randomWord()%256;RawKit raw {};raw.values.fill(39);const auto old=raw;
        const auto a=file::DecodeRaw(bytes.data(),length,raw);RawKit again=old;const auto b=file::DecodeRaw(bytes.data(),length,again);
        CHECK(a.error==b.error);CHECK(same(raw,again));
        if(length==236){CHECK(a);file::Bytes encoded {};CHECK(file::EncodeRaw(raw,encoded.data(),encoded.size()));CHECK(std::equal(encoded.begin(),encoded.end(),bytes.begin()));++roundtrips;}
        else {CHECK(!a);CHECK(same(raw,old));}
        auto can=MakeCanonicalInitializationKit();const auto before=can;auto second=can;
        const auto x=file::DecodeCanonical(bytes.data(),length,can),y=file::DecodeCanonical(bytes.data(),length,second);
        CHECK(x.error==y.error && x.field==y.field && x.route==y.route);CHECK(same(can,second));if(!x)CHECK(same(can,before));++fuzzCases;
    }
    for(unsigned n=0;n<1000;++n) {
        RawKit raw {};for(auto& b:raw.name)b=static_cast<std::uint8_t>(randomWord());
        for(const auto& f:kFields)raw.values[f.id]=static_cast<std::uint8_t>(randomWord()%(f.maximum+1));
        for(unsigned slot=0;slot<12;++slot)raw.values[(slot<6?149:167)+slot%6]=0;
        CanonicalKit source {};CHECK(DecodeRawKit(raw,source));file::Bytes bytes {},second {};
        CHECK(file::EncodeCanonical(source,bytes.data(),bytes.size()));CHECK(file::EncodeCanonical(source,second.data(),second.size()));CHECK(bytes==second);
        CanonicalKit decoded {};CHECK(file::DecodeCanonical(bytes.data(),bytes.size(),decoded));CHECK(same(source,decoded));original(raw,bytes);++roundtrips;++fuzzCases;
    }
}
static void goldens(const char* folder) {
    for(const char* name:{"default","drums","snare","hats","selector-boundary","populated"}) {
        char path[2048] {};std::snprintf(path,sizeof(path),"%s/%s.snd",folder,name);auto* f=std::fopen(path,"rb");CHECK(f!=nullptr);if(!f)continue;
        file::Bytes input {};CHECK(std::fread(input.data(),1,input.size(),f)==236);CHECK(std::fgetc(f)==EOF);std::fclose(f);
        RawKit raw {};CHECK(file::DecodeRaw(input.data(),input.size(),raw));file::Bytes output {};CHECK(file::EncodeRaw(raw,output.data(),output.size()));CHECK(output==input);original(raw,output);
        CanonicalKit can {};const bool allowed=std::strcmp(name,"selector-boundary")!=0;const auto decoded=file::DecodeCanonical(input.data(),input.size(),can);
        CHECK(static_cast<bool>(decoded)==allowed);
        if(allowed){CHECK(file::EncodeCanonical(can,output.data(),output.size()));CHECK(output==input);}
    }
}
#include "reference/presets/kit_file_io_tests.h"
static volatile std::uint64_t sink=0;
template<class F> static long long cost(F fn){const auto t=std::chrono::steady_clock::now();for(unsigned i=0;i<2000;++i)fn();return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t).count()/2000;}
int main(int argc,char** argv) {
    if(argc!=3)return 2;const auto start=allocations.load();archive();canonical();properties();const auto pure=allocations.load()-start;
    CHECK(pure==0);goldens(argv[1]);faultFiles();const auto setup=allocations.load();nativeFiles(argv[2]);const auto io=allocations.load()-setup;
    auto raw=MakeFirmwareInitializationKit();auto can=MakeCanonicalInitializationKit();file::Bytes bytes {};
    const auto a=allocations.load();
    const auto decode=cost([&]{sink+=static_cast<unsigned>(file::DecodeRaw(bytes.data(),bytes.size(),raw).error);});
    const auto canonicalTime=cost([&]{sink+=static_cast<unsigned>(file::DecodeCanonical(bytes.data(),bytes.size(),can).error);});
    const auto encode=cost([&]{sink+=static_cast<unsigned>(file::EncodeCanonical(can,bytes.data(),bytes.size()).error);});
    const auto validation=cost([&]{sink+=static_cast<unsigned>(file::CheckRepresentability(can).error);});
    CHECK(allocations.load()==a);
    std::printf("{\"pass\":%u,\"fail\":%u,\"original_c_comparisons\":%u,\"roundtrips\":%u,\"fuzz_cases\":%u,\"pure_allocations\":%zu,\"io_setup_allocations\":%zu,\"raw_decode_ns\":%lld,\"canonical_ns\":%lld,\"encode_ns\":%lld,\"validation_ns\":%lld,\"raw_bytes\":%zu,\"canonical_bytes\":%zu,\"encoded_bytes\":%zu,\"file_result_bytes\":%zu}\n",passed.load(),failed.load(),originalComparisons,roundtrips,fuzzCases,pure,io,decode,canonicalTime,encode,validation,sizeof(raw),sizeof(can),sizeof(bytes),sizeof(file::FileResult));
    return failed.load()?1:0;
}
