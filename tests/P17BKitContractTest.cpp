// Whole-kit contracts against independently compiled original firmware C.
// Original Copyright 2013 Julian Schmidt; see LICENSE-LXR.txt.
#include "../interfaces/KitContract.h"
#include "../dsp/RouteHostControls.h"
#include "../dsp/ParameterStateAuthority.h"
#include "reference/presets/original_kit_contract_adapter.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

static std::size_t allocations = 0;
void* operator new(std::size_t n) { ++allocations; if (void* p = std::malloc(n)) return p; std::abort(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using namespace lxr;
using namespace lxr::kit;
namespace mc = lxr::modulation_contract;
static unsigned passed = 0, failed = 0;
#define CHECK(x) do { if (x) ++passed; else { ++failed; if (failed < 15) std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); } } while (false)
static bool Same(const RawKit& a, const RawKit& b) { return a.name == b.name && a.values == b.values; }
static bool Same(const CanonicalKit& a, const CanonicalKit& b) {
    // Compare every logical field, not object padding.
    if (a.name != b.name) return false;
    for (std::uint16_t i = 0; i < 228; ++i) { std::uint16_t x=0,y=0; ReadField(a,i,x); ReadField(b,i,y); if (x!=y) return false; }
    return true;
}
static bool Same(const PersistentRouteRecord& a, const PersistentRouteRecord& b) {
    return a.owner==b.owner && a.source==b.source && a.flags==b.flags &&
        a.identityNamespace==b.identityNamespace && a.originalIndex==b.originalIndex &&
        a.firmwareParameter==b.firmwareParameter && a.valueType==b.valueType &&
        a.scope==b.scope && a.physicalKey==b.physicalKey && a.menuVoiceHint==b.menuVoiceHint && a.reserved==b.reserved;
}
static bool Same(const PatchCandidate& a, const PatchCandidate& b) {
    if (a.name!=b.name || a.patch!=b.patch) return false;
    for (unsigned i=0;i<12;++i) if (!Same(a.routes[i],b.routes[i])) return false;
    return true;
}
static RawKit varied() {
    RawKit raw {};
    for (unsigned i=0;i<8;++i) raw.name[i]=static_cast<std::uint8_t>(i*37);
    for (const auto& f:kFields) raw.values[f.id]=static_cast<std::uint8_t>((f.id*19+7)%(f.maximum+1));
    return raw;
}
static void structureDefaults() {
    P17OriginalContract c {}; p17_original_contract(&c);
    CHECK(c.sound_count==kFieldCount); CHECK(c.modulation_targets==205);
    CHECK(kOwnerCount==6); CHECK(sizeof(RawKit)==236); CHECK(sizeof(CanonicalKit)==236);
    CHECK(mc::voiceForTrack(mc::Track::ClosedHat)==mc::Voice::HiHat);
    CHECK(mc::voiceForTrack(mc::Track::OpenHat)==mc::Voice::HiHat);
    std::uint8_t original[273] {}; p17_original_defaults(original,sizeof(original));
    const auto raw=MakeFirmwareInitializationKit(); const auto canonical=MakeCanonicalInitializationKit();
    RawKit encoded {}; CHECK(EncodeRawKit(canonical,encoded)); CHECK(Same(raw,encoded));
    CHECK(ValidateRawKit(raw)); CHECK(ValidateCanonicalKit(canonical)); CHECK(ValidateForRuntime(canonical));
    PatchCandidate candidate {}; CHECK(BuildPatchCandidate(canonical,candidate));
    for (std::uint16_t i=0;i<228;++i) {
        const auto& f=kFields[i]; CHECK(p17_original_parameter_enum(i)==i);
        CHECK(static_cast<unsigned>(f.owner)==p17b_original_owner(i));
        CHECK(raw.values[i]==original[i]); CHECK(f.initializationDefault==original[i]);
        CHECK(candidate.patch[i]==original[i]);
        if (f.kind!=FieldKind::Reserved && f.kind!=FieldKind::OscillatorWave) CHECK(f.maximum==p17b_original_maximum(i));
        else CHECK(f.maximum==255);
    }
    CHECK(p17b_original_builtin_wave_count()==6);
    for (unsigned n=0;n<100;++n) {
        CHECK(Same(MakeFirmwareInitializationKit(),raw));
        CHECK(Same(MakeCanonicalInitializationKit(),canonical));
    }
    for (unsigned i=0;i<12;++i) CHECK(ValidatePersistentRoute(candidate.routes[i])==PersistentRouteStatus::Disabled);
    auto hats=raw; hats.values[60]=11;hats.values[61]=92;hats.values[226]=42;hats.values[227]=43;
    CanonicalKit k {}; CHECK(DecodeRawKit(hats,k));
    CHECK(k.hiHat.velod6_closed==11 && k.hiHat.velod6_open==92);
    CHECK(k.hiHat.midi_note6==42 && k.hiHat.midi_note7==43);
}
static void fieldsAndOracle() {
    // Exhaustive byte mutations; the original writer is exercised independently
    // for every admitted field value. Neither oracle consumes generated metadata.
    const auto sentinel=varied();CanonicalKit sentinelKit {};CHECK(DecodeRawKit(sentinel,sentinelKit));
    std::uint8_t values[273] {}, bytes[236] {};char filename[16] {};
    for (std::uint16_t id=0;id<228;++id) {
        for (unsigned v=0;v<256;++v) {
            auto raw=sentinel;raw.values[id]=static_cast<std::uint8_t>(v);
            auto k=sentinelKit;const auto result=DecodeRawKit(raw,k);
            const bool valid=v<=kFields[id].maximum;
            CHECK(static_cast<bool>(result)==valid);
            if (!valid) { CHECK(Same(k,sentinelKit));CHECK(result.field==id);continue; }
            std::uint16_t field=65535; CHECK(ReadField(k,id,field));CHECK(field==v);
            RawKit encoded {};CHECK(EncodeRawKit(k,encoded));CHECK(Same(raw,encoded));
            std::memcpy(values,raw.values.data(),228);
            CHECK(p17_original_write(P17_SOUND,values,273,raw.name.data(),bytes,sizeof(bytes),filename)==236);
            CHECK(std::memcmp(bytes,encoded.name.data(),8)==0);
            CHECK(std::memcmp(bytes+8,encoded.values.data(),228)==0);
        }
    }
    auto bad=sentinelKit;bad.hiHat.target_lfo6=StoredSelector::fromRaw(205);
    auto output=sentinel;CHECK(EncodeRawKit(bad,output).status==Status::Invalid);CHECK(Same(output,sentinel));
    CHECK(!StoredSelector::fromRaw(256).valid());CHECK(!StoredSelector::fromRaw(UINT64_MAX).valid());
    std::uint16_t unchanged=37;CHECK(ReadField(bad,228,unchanged).status==Status::Invalid);CHECK(unchanged==37);
    // Dirty placement storage demonstrates every byte of the canonical record
    // is initialized; the API still encodes named logical fields only.
    alignas(CanonicalKit) unsigned char storage[sizeof(CanonicalKit)];std::memset(storage,0xa5,sizeof(storage));
    auto* placed=new(storage) CanonicalKit{};
    for (auto b:storage) CHECK(b==0);
    placed->~CanonicalKit();
}
static unsigned supportedRoutes=0, wideRoutes=0, unsupportedRoutes=0;
static void selectors() {
    for (unsigned kind=0;kind<2;++kind) for (unsigned owner=0;owner<6;++owner) {
        const mc::SourceIdentity source {kind==0?mc::SourceKind::Velocity:mc::SourceKind::Lfo,mc::voiceFromZeroBased(owner)};
        for (unsigned index=0;index<205;++index) {
            const auto* t=mc::lookup(mc::OriginalTargetIndex::fromRaw(index));
            CHECK(t && t->parameter.value()==p17_original_target_parameter(index));
            for (unsigned hint=0;hint<=(kind==0?0u:6u);++hint) {
                OriginalSelector s {}; CHECK(ResolveOriginalSelector(source,index,hint,s));
                CHECK(s.index.value()==index);CHECK(s.voiceHint==hint);CHECK(ValidateOriginalSelector(s));
                auto raw=MakeFirmwareInitializationKit();raw.values[(kind==0?149:167)+owner]=static_cast<std::uint8_t>(index);
                if (kind) raw.values[161+owner]=static_cast<std::uint8_t>(hint);
                CanonicalKit k {};CHECK(DecodeRawKit(raw,k));RawKit out {};CHECK(EncodeRawKit(k,out));CHECK(Same(raw,out));
                if (hint==0) {
                    // Exercise the unchanged original reader AND route sender,
                    // including owner bits, PAR high bit and startup hint repair.
                    std::uint8_t bytes[236] {},base[228] {},target[228] {};
                    std::memcpy(bytes,raw.name.data(),8);std::memcpy(bytes+8,raw.values.data(),228);
                    CHECK(p17_original_read_sound(bytes,sizeof(bytes),0,base,target)==1);
                    P17Message messages[256] {};CHECK(p17_original_messages(messages,256)==240);
                    const auto message=messages[owner*2+kind];
                    CHECK(message.status==(kind==0?0xb8:0xb7));
                    CHECK((message.first>>1)==owner);
                    CHECK((((message.first&1u)<<7)|message.second)==p17_original_target_parameter(index));
                    CHECK(base[(kind==0?149:167)+owner]==index);
                    CHECK(base[161+owner]==1); // Original sender only; canonical retains0.
                }
                auto record=MakeDisabledRoute(1,5);const auto before=record;const auto r=ToTypedRoute(s,record);
                const auto host=route_host::decode(route_host::parameterForSlot(kind*6+owner),route_host::RouteToken16::fromRaw(index));
                CHECK(static_cast<bool>(r)==static_cast<bool>(host));
                if (r) {
                    CHECK(record.originalIndex==index);CHECK(ValidatePersistentRoute(record)==(index?PersistentRouteStatus::Ok:PersistentRouteStatus::Disabled));
                    OriginalSelector restored {};CHECK(FromTypedRoute(record,restored));CHECK(restored.index.value()==index);
                    CHECK(restored.voiceHint==(index?hint:0)); // Off hint retained in CanonicalKit, not LXRS.
                    if (hint==0) { if(index) {++supportedRoutes;if(index>127)++wideRoutes;} }
                } else {
                    CHECK(r.status==Status::Unsupported);CHECK(Same(record,before));
                    if (hint==0) ++unsupportedRoutes;
                }
                PatchCandidate candidate {};const auto prior=candidate;const auto built=BuildPatchCandidate(k,candidate);
                CHECK(static_cast<bool>(built)==static_cast<bool>(r));
                if (built) {
                    CHECK(candidate.patch[(kind==0?149:167)+owner]==0);
                    CHECK(candidate.routes[kind*6+owner].originalIndex==index);
                    CHECK(ValidatePersistentRoutes(candidate.routes)==PersistentRouteStatus::Ok);
                } else CHECK(Same(candidate,prior));
            }
        }
        for (std::uint64_t invalid:std::array<std::uint64_t,6>{{205,255,256,65535,65536,UINT64_MAX}}) {
            OriginalSelector s {};s.voiceHint=0;const auto before=s;
            CHECK(ResolveOriginalSelector(source,invalid,0,s).status==Status::Invalid);CHECK(s.index==before.index);
        }
        OriginalSelector s {};CHECK(ResolveOriginalSelector(source,0,7,s).status==Status::Invalid);
    }
    CHECK(supportedRoutes==214);CHECK(wideRoutes==78); //107 +39 per source.
    for (unsigned token=205;token<=65535;++token) {
        PersistentRouteRecord r {};r.owner=0;r.source=0;r.flags=1;r.identityNamespace=1;r.originalIndex=static_cast<std::uint16_t>(token);
        OriginalSelector s {};CHECK(FromTypedRoute(r,s).status==Status::Nonrepresentable);CHECK(s.index.value()==0);
    }
    auto r=MakeDisabledRoute(0,0);r.flags=1;r.identityNamespace=2;r.originalIndex=8;
    OriginalSelector s {};CHECK(FromTypedRoute(r,s).status==Status::Nonrepresentable);
    r=MakeDisabledRoute(0,7);CHECK(FromTypedRoute(r,s).status==Status::Invalid);
    r=MakeDisabledRoute(0,0);r.reserved=1;CHECK(FromTypedRoute(r,s).status==Status::Invalid);
    auto active=route_host::decode(route_host::parameterForSlot(0),route_host::RouteToken16::fromRaw(2)).record;
    CHECK(FromTypedRoute(active,s));
    for (unsigned corrupt=0;corrupt<9;++corrupt) {
        r=active;
        switch(corrupt) {
        case 0:r.flags=2;break;case 1:r.reserved=1;break;case 2:r.firmwareParameter=227;break;
        case 3:r.valueType=255;break;case 4:r.scope=2;break;case 5:r.physicalKey=227;break;
        case 6:r.menuVoiceHint=1;break;case 7:r.source=2;break;default:r.owner=6;break;
        }
        const auto previous=s;CHECK(FromTypedRoute(r,s).status==Status::Invalid);
        CHECK(s.index==previous.index && s.voiceHint==previous.voiceHint && s.source.owner==previous.source.owner);
    }
    r=MakeDisabledRoute(0,0);r.originalIndex=2;CHECK(FromTypedRoute(r,s).status==Status::Invalid);
    r=active;r.originalIndex=0;CHECK(FromTypedRoute(r,s).status==Status::Invalid);
    CHECK(ResolveOriginalSelector({mc::SourceKind::Velocity,mc::Voice::Invalid},0,0,s).status==Status::Invalid);
    CHECK(ResolveOriginalSelector({static_cast<mc::SourceKind>(2),mc::Voice::Drum1},0,0,s).status==Status::Invalid);
    CHECK(ResolveOriginalSelector({mc::SourceKind::Velocity,mc::Voice::Drum1},0,1,s).status==Status::Invalid);
}
static void runtimeAndIsolation() {
    auto raw=MakeFirmwareInitializationKit();CanonicalKit k {};PatchCandidate candidate {};
    for (const auto& f:kFields) {
        if (f.kind==FieldKind::OscillatorWave || f.kind==FieldKind::Reserved) {
            raw.values[f.id]=255;CHECK(DecodeRawKit(raw,k));CHECK(ValidateCanonicalKit(k));
            RawKit out {};CHECK(EncodeRawKit(k,out));CHECK(Same(raw,out));
            const auto before=candidate;CHECK(BuildPatchCandidate(k,candidate).status==Status::Unsupported);CHECK(Same(candidate,before));
            raw.values[f.id]=0;
        }
    }
    // All scalar patch candidates, including all seven note overrides and
    // nonzero reserved seven-bit provenance; source selectors stay off.
    for (const auto& f:kFields) if (f.kind!=FieldKind::Selector)
        raw.values[f.id]=static_cast<std::uint8_t>(f.kind==FieldKind::OscillatorWave?5:f.kind==FieldKind::Reserved?127:f.maximum);
    CHECK(DecodeRawKit(raw,k));CHECK(BuildPatchCandidate(k,candidate));CHECK(candidate.patch==raw.values);
    ParameterStateAuthority<228> patch;patch.initialize(raw.values);
    RoutingStateAuthority routes;routes.initialize();
    ParameterStateAuthority<228>::Values snapshot {};std::uint64_t generation=0;
    CHECK(patch.trySnapshot(snapshot,&generation));const auto old=snapshot;const auto epoch=generation;
    const auto allocationStart=allocations;
    for (unsigned n=0;n<100;++n) { CHECK(BuildPatchCandidate(k,candidate));CHECK(EncodeRawKit(k,raw));CHECK(DecodeRawKit(raw,k)); }
    CHECK(allocations==allocationStart);CHECK(patch.trySnapshot(snapshot,&generation));CHECK(snapshot==old && generation==epoch);
    RoutingStateAuthority::Guard guard;CHECK(routes.tryAcquire(guard));RoutingStateAuthority::Snapshot rs {};CHECK(routes.copyLocked(guard,rs));
    CHECK(rs.generation==0);CHECK(ValidatePersistentRoutes(rs.state.routes)==PersistentRouteStatus::Ok);
    for (const auto& route:rs.state.routes) CHECK(route.flags==0);
    // No Engine/PatternStateAuthority/HostTransport implementation is linked.
    // Candidate routines cannot reach them or any host callback/global owner.
}
static void completeKits() {
    std::uint32_t random=0x370017b;
    const auto next=[&]() {random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    for (unsigned n=0;n<1000;++n) {
        RawKit raw {};
        for (auto& b:raw.name) b=static_cast<std::uint8_t>(next());
        for (const auto& f:kFields) {
            const auto max=f.kind==FieldKind::OscillatorWave?5:f.kind==FieldKind::Reserved?127:f.maximum;
            raw.values[f.id]=static_cast<std::uint8_t>(next()%(max+1));
        }
        for (unsigned source=0;source<2;++source) for(unsigned owner=0;owner<6;++owner) {
            unsigned index=next()%205;
            while (!route_host::decode(route_host::parameterForSlot(source*6+owner),route_host::RouteToken16::fromRaw(index))) index=(index+1)%205;
            raw.values[(source?167:149)+owner]=static_cast<std::uint8_t>(index);
        }
        CanonicalKit k {};CHECK(DecodeRawKit(raw,k));CHECK(ValidateForRuntime(k));RawKit out {};CHECK(EncodeRawKit(k,out));CHECK(Same(raw,out));
        PatchCandidate a {},b {};CHECK(BuildPatchCandidate(k,a));CHECK(BuildPatchCandidate(k,b));CHECK(Same(a,b));
        for(const auto& f:kFields) CHECK(a.patch[f.id]==(f.kind==FieldKind::Selector?0:raw.values[f.id]));
        CHECK(ValidatePersistentRoutes(a.routes)==PersistentRouteStatus::Ok);
    }
}
static volatile std::uint64_t sink=0;
template<class F> static long long cost(F operation) {
    constexpr unsigned count=2000;const auto begin=std::chrono::steady_clock::now();
    for (unsigned i=0;i<count;++i) operation();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count()/count;
}
int main() {
    structureDefaults();fieldsAndOracle();selectors();runtimeAndIsolation();completeKits();
    const auto a=allocations;
    auto raw=MakeFirmwareInitializationKit();auto k=MakeCanonicalInitializationKit();PatchCandidate candidate {};
    const auto defaults=cost([&]{k=MakeCanonicalInitializationKit();sink+=k.drum1.vol1;});
    const auto validate=cost([&]{sink+=static_cast<unsigned>(ValidateCanonicalKit(k).status);});
    const auto decode=cost([&]{sink+=static_cast<unsigned>(DecodeRawKit(raw,k).status);});
    const auto encode=cost([&]{sink+=static_cast<unsigned>(EncodeRawKit(k,raw).status);});
    const auto build=cost([&]{sink+=static_cast<unsigned>(BuildPatchCandidate(k,candidate).status);});
    CHECK(allocations==a);
    std::printf("{\"pass\":%u,\"fail\":%u,\"raw_bytes\":%zu,\"canonical_bytes\":%zu,\"candidate_bytes\":%zu,\"conversion_allocations\":%zu,\"supported_routes_both_sources\":%u,\"wide_routes_both_sources\":%u,\"unsupported_owner_rows_both_sources\":%u,\"default_ns\":%lld,\"validate_ns\":%lld,\"decode_ns\":%lld,\"encode_ns\":%lld,\"candidate_ns\":%lld}\n",passed,failed,sizeof(RawKit),sizeof(CanonicalKit),sizeof(PatchCandidate),allocations-a,supportedRoutes,wideRoutes,unsupportedRoutes,defaults,validate,decode,encode,build);
    return failed?1:0;
}
