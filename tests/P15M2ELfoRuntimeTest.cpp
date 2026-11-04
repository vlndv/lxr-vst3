// Original LXR v0.37 velocity route differential; LICENSE-LXR.txt.
#include "../interfaces/VelocityRouting.h"
#include "../dsp/Engine.h"
#include "../dsp/ParameterArray.h"
#include "../dsp/ParameterStateAuthority.h"
#include "../dsp/MidiParser.h"
#include "../dsp/LXRAudioAdapter.h"
#include "../vendor/iPlug2/Examples/LXR/LXRState.h"
#include "reference/lfo_runtime/original_lfo_runtime_adapter.h"
#include "reference/modulation/original_contract_fixture.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
namespace {
constexpr uint32_t seed=0x9e3779b9u;
int passed=0,failed=0,allocations=0,pcmCases=0;
int parameterCalls=0,triggerCalls=0,configureCalls=0;
long long samples=0,mismatches=0;
bool measured=false;
FILE* metrics=nullptr;

lxr::Engine engine;
lxr::ParameterArray params;
float originalNotes[128];
void check(const char* name,bool ok) {
    if(ok)++passed;
    else {++failed;if(failed<35)std::printf("FAIL %s\n",name);}
}
bool equal(float a,float b){return std::memcmp(&a,&b,sizeof(float))==0;}
lxr::OscInfo& osc(int v,int op=0) {
    if(v<3)return op?engine.drums[v].modOsc:engine.drums[v].osc;
    if(v==3)return op?engine.snare.noiseOsc:engine.snare.osc;
    if(v==4)return op==0?engine.cymbal.osc:op==1?engine.cymbal.modOsc:engine.cymbal.modOsc2;
    return op==0?engine.hihat.osc:op==1?engine.hihat.modOsc:engine.hihat.modOsc2;
}
lxr::ResonantFilter& filter(int v){return v<3?engine.drums[v].filter:v==3?engine.snare.filter:v==4?engine.cymbal.filter:engine.hihat.filter;}
lxr::OscRng& rng(int v){return v<3?engine.drums[v].rng:v==3?engine.snare.rng:v==4?engine.cymbal.rng:engine.hihat.rng;}
void set(int id,int value){const bool prior=measured;measured=true;++parameterCalls;params.set(static_cast<uint8_t>(id),static_cast<uint8_t>(value),engine);base_ref_set(id,value);measured=prior;}
void trigger(int v,int vel=127,int note=63,int open=0) {
    const bool prior=measured;measured=true;++triggerCalls;
    if(v<3)engine.triggerDrum(v,vel,note);else if(v==3)engine.triggerSnare(vel,note);
    else if(v==4)engine.triggerCymbal(vel,note);else engine.triggerHiHat(vel,open!=0,note);
    base_ref_trigger(v,vel,note,open);
    measured=prior;
}
void async(int v) {
    if(v<3)engine.drums[v].calcAsync(engine.noteFreq);else if(v==3)engine.snare.calcAsync(engine.noteFreq);
    else if(v==4)engine.cymbal.calcAsync(engine.noteFreq);else engine.hihat.calcAsync(engine.noteFreq);
    base_ref_async(v);
}
void renderPort(int v,int16_t* out) {
    if(v<3)engine.drums[v].calcSyncBlock(out,32,engine.tables);else if(v==3)engine.snare.calcSyncBlock(out,32,engine.tables);
    else if(v==4)engine.cymbal.calcSyncBlock(out,32,engine.tables);else engine.hihat.calcSyncBlock(out,32,engine.tables);
}
void pcm(const char* category,int v,int setting,int block,const int16_t* port,const int16_t* ref) {
    int count=0,first=-1,maximum=0;double squared=0;
    for(int j=0;j<32;j++){
        int d=static_cast<int>(port[j])-ref[j];if(d){++count;if(first<0)first=j;}
        maximum=std::max(maximum,std::abs(d));squared+=static_cast<double>(d)*d;
    }
    std::fprintf(metrics,"%s,%d,%d,%d,32,%d,%d,%d,%.12g\n",category,v,setting,block,count,first,maximum,std::sqrt(squared/32));
    samples+=32;mismatches+=count;++pcmCases;check(category,count==0);
}
void block(const char* category,int v,int setting,int n) {
    int16_t a[32],b[32];measured=true;engine.beginModulationTick();runtime_ref_tick();
    // Independent entropy service inputs for LFO and voice noise. The hardware
    // RNG is shared; the plugin deliberately owns a separate PRNG per source.
    runtime_ref_rng(rng(v).state);async(v);renderPort(v,a);base_ref_render(v,b);measured=false;
    pcm(category,v,setting,n,a,b);
}
void reset() {
    engine.init();lfo_ref_tempo(120);std::memcpy(engine.noteFreq,originalNotes,sizeof(originalNotes));base_ref_reset(seed);params.init();
    // Explicitly match zeroed firmware-global previous-input storage, as in
    // the established original-C snare fixture. No production reset change.
    for(int v=0;v<6;v++){filter(v).reset();rng(v).state=seed;}
    for(int id=1;id<228;id++){
        // Original CC protocol-only IDs are not native sound setters.
        if(id==5||id==97||id==98||id==127)continue;
        set(id,params.get(static_cast<uint8_t>(id)));
    }
    for(int id: {1,2,3,4,6,7,20,21,22,23,24,25,26})set(id,0);
    for(int id: {28,31,32,35,36,74,75,76,77,82,84,86})set(id,0);
    for(int id=88;id<=93;id++)set(id,127);
    for(int id=49;id<=59;id+=2)set(id,0);
    for(int id=62;id<=67;id++)set(id,64);
    for(int id=102;id<=107;id++)set(id,0);
    for(int id=191;id<=196;id++)set(id,7);
    for(int id=197;id<=202;id++)set(id,0);
    for(int id=203;id<=208;id++)set(id,1);
    set(27,64);set(28,96);set(6,4);set(7,4);
    // Match actual initialized pitch multipliers (no snap/EG in base fixture).
    for(int v=0;v<6;v++)osc(v).pitchMod=1;
}

#include "reference/velocity/port_field_observer.inc"

namespace mc=lxr::modulation_contract;
mc::SourceIdentity source(unsigned v){return {mc::SourceKind::Velocity,mc::voiceFromZeroBased(v)};}
mc::OriginalTargetIndex indexFor(unsigned id){return mc::targetForHost(mc::HostParameterId::fromRaw(id)).value;}
void route(unsigned v,mc::OriginalTargetIndex index) {
    const bool prior=measured;measured=true;++configureCalls;
    auto result=engine.configureVelocityDestination(source(v),index);
    measured=prior;
    check("configure verified destination",result==lxr::VelocityRouteStatus::Ok);
    velocity_ref_route(v,index.value());
}
std::array<uint8_t,228> rawPatch(){std::array<uint8_t,228> a{};std::copy(params.values,params.values+228,a.begin());return a;}
void fieldCheck(unsigned p){check("original-C effective field",equal(portField(p),velocity_ref_field(p)));}

void routeLfo(unsigned v,mc::OriginalTargetIndex index) {
    measured=true;++configureCalls;
    auto status=engine.configureLfoDestination({mc::SourceKind::Lfo,mc::voiceFromZeroBased(v)},index);
    measured=false;check("configure LFO",status==lxr::LfoRouteStatus::Ok);
    lfo_ref_route(v,mc::lookup(index)->parameter.value());
}
void state(unsigned v) {
    LfoReferenceState r{};lfo_ref_get(v,&r);const auto& p=*engine.nativeLfo(v);
    if(p.phase!=r.phase && failed<12)std::printf("STATE owner=%u phase=%u/%u increment=%u/%u freq=%g/%g sync=%u/%u multiplier=%g/%g\n",v,p.phase,r.phase,p.phaseInc,r.increment,p.frequency,r.frequency,p.sync,r.sync,p.modNodeValue,r.multiplier);
    check("phase exact",p.phase==r.phase);check("increment exact",p.phaseInc==r.increment);
    check("frequency exact",equal(p.frequency,r.frequency));
}
void primitives() {
    reset();
    for(unsigned v=0;v<6;++v) {
        engine.nativeLfo(v)->init();lfo_ref_init(v);state(v);
        engine.nativeLfo(v)->heldValue=.375f;
        LfoReferenceState r{};lfo_ref_get(v,&r);r.random=.375f;lfo_ref_put(v,&r);
        engine.nativeLfo(v)->init();lfo_ref_init(v);
        check("live init retains noise",equal(engine.nativeLfo(v)->heldValue,.375f));
        for(unsigned raw=0;raw<128;++raw) {
            set(115+v,raw);state(v);
        }
        for(unsigned bpm:{0u,1u,120u,300u}) for(unsigned sync=0;sync<14;++sync) {
            check("native BPM domain",engine.setNativeTempo(bpm));lfo_ref_tempo(bpm);
            set(179+v,sync);state(v);
            for(unsigned raw:{0u,64u,127u}) {set(115+v,raw);state(v);}
        }
        for(unsigned w=0;w<9;++w) for(unsigned i=0;i<4101;++i) {
            auto& p=*engine.nativeLfo(v);LfoReferenceState r{};lfo_ref_get(v,&r);
            uint32_t phase=i<4096?i<<20:i==4096?0xffffffffu:i==4097?0x7fffffffu:i==4098?0x80000000u:i==4099?0xfffffff0u:1;
            p.waveform=w;p.phase=phase;p.phaseInc=0;p.modNodeValue=1;p.heldValue=.375f;
            r.waveform=w;r.phase=phase;r.increment=0;r.multiplier=1;r.random=.375f;lfo_ref_put(v,&r);
            p.tick(engine.tables.sine);check("all table indices/edges",equal(p.output,lfo_ref_calc(v)));state(v);
        }
        for(uint32_t phase:{0u,0x7fffffffu,0xfffffff0u,0xffffffffu})
        for(uint32_t inc:{0u,1u,317u,0x12345678u,0x7fffffffu}) for(float multiplier:{0.f,.5f,1.f}) {
            auto& p=*engine.nativeLfo(v);LfoReferenceState r{};lfo_ref_get(v,&r);
            p.phase=phase;p.phaseInc=inc;p.modNodeValue=multiplier;p.waveform=2;
            r.phase=phase;r.increment=inc;r.multiplier=multiplier;r.waveform=2;lfo_ref_put(v,&r);
            p.tick(engine.tables.sine);check("wrap and float increment",equal(p.output,lfo_ref_calc(v)));state(v);
        }
        auto& p=*engine.nativeLfo(v);lfo_ref_get(v,&r);
        p.phase=0xffffffffu;p.phaseInc=1;p.modNodeValue=1;p.waveform=5;p.rng.state=seed;
        r.phase=p.phase;r.increment=1;r.multiplier=1;r.waveform=5;lfo_ref_put(v,&r);runtime_ref_rng(seed);
        for(int i=0;i<4;++i){p.tick(engine.tables.sine);check("controlled full-width RNG and hold",equal(p.output,lfo_ref_calc(v)));}
    }
}
void matrix() {
    unsigned rows=0,above127=0;
    for(const auto& t:mc::kOriginalTargets) {
        if(!lxr::velocityParameterSupported(t.parameter))continue;
        ++rows;if(t.index.value()>127)++above127;
        const unsigned v=static_cast<unsigned>(t.voice),p=t.parameter.value();
        check("independent AVR row",lxr_ref_parameter(t.index.value())==p);
        for(unsigned w=0;w<8;++w)for(int base:{0,64,127})for(int depth:{0,64,127})for(int vel:{0,1,64,127}) {
            reset();set(p,base);set(143+v,depth);set(121+v,127-depth/2);
            set(115+v,127);set(155+v,w);
            route(v,t.index);routeLfo(v,t.index);trigger(v,vel,63,v==5);
            auto patch=rawPatch();
            for(int b=0;b<(w==5?9:3);++b) {
                // Firmware shares a hardware RNG with oscillators. Supply the
                // same disclosed entropy input to each native LFO evaluation.
                engine.nativeLfo(v)->rng.state=seed;runtime_ref_rng(seed);
                block("all107_shared_eight_waves",v,p,w*3+b);fieldCheck(p);state(v);
                check("LFO held output exact",equal(engine.nativeLfo(v)->output,[] (unsigned n){LfoReferenceState r{};lfo_ref_get(n,&r);return r.held;}(v)));
                check("raw patch unchanged",rawPatch()==patch);
            }
        }
    }
    check("107 LFO rows",rows==107);check("full-width row coverage",above127>0);
    std::printf("COVERAGE rows=%u above127=%u waveforms=8 owners=6\n",rows,above127);
}
void ordering() {
    for(unsigned v=0;v<6;++v)for(unsigned p:{88+v,37+v,94+(v<3?v:v+2)}) {
        reset();set(p,64);set(143+v,127);set(121+v,64);set(155+v,2);set(115+v,127);
        route(v,indexFor(p));routeLfo(v,indexFor(p));
        for(int vel:{1,64,127,0}) {trigger(v,vel);fieldCheck(p);}
        block("repeated triggers replay",v,p,0);fieldCheck(p);
        trigger(v,64);fieldCheck(p); // velocity applies to LFO-effective field
        set(p,32);set(p,96);fieldCheck(p);block("new base wins",v,p,1);fieldCheck(p);
        set(p,32);trigger(v,1);fieldCheck(p);block("new base then trigger",v,p,2);fieldCheck(p);
        routeLfo(v,indexFor(88+v));fieldCheck(p);block("different physical fields",v,p,3);fieldCheck(p);fieldCheck(88+v);
        routeLfo(v,mc::OriginalTargetIndex::fromRaw(0));block("LFO off restoration",v,p,4);fieldCheck(p);
        route(v,mc::OriginalTargetIndex::fromRaw(0));block("all off",v,p,5);fieldCheck(p);
    }
    // Joining an existing multiplier uses physical base1, even after modulation.
    for(unsigned v=0;v<6;++v) {
        reset();const unsigned coarse=8+v*2;set(143+v,127);set(121+v,127);
        route(v,indexFor(coarse));trigger(v,1);routeLfo(v,indexFor(coarse+1));
        check("shared alias captures base1",equal(engine.lfoRoute(mc::voiceFromZeroBased(v))->baseFloat,1.f));
        block("aliases",v,coarse,0);fieldCheck(coarse);
        set(coarse,96);block("alias raw update",v,coarse,1);fieldCheck(coarse);
    }
    // Global scan, including other owners, and BOTH hi-hat articulations.
    for(unsigned track=0;track<6;++track)for(unsigned selected=0;selected<8;++selected)for(int open=0;open<2;++open) {
        reset();for(unsigned v=0;v<6;++v){set(173+v,selected);set(185+v,64);engine.nativeLfo(v)->phase=11;LfoReferenceState r{};lfo_ref_get(v,&r);r.phase=11;lfo_ref_put(v,&r);}
        trigger(track,64,63,open);
        for(unsigned v=0;v<6;++v)state(v);
        engine.beginModulationTick();runtime_ref_tick();for(unsigned v=0;v<6;++v)state(v);
    }
}
void lifecycleAndEngine() {
    reset();
    for(unsigned v=0;v<6;++v) {set(121+v,127);set(155+v,4);set(115+v,127);routeLfo(v,indexFor(88+v));}
    const auto raw=rawPatch();lxr::ParameterStateAuthority<228> authority;authority.initialize(raw);
    std::array<uint8_t,lxr::state::kEncodedSize> before{},after{};
    check("LXRS encode",lxr::state::Encode(raw,before));
    for(unsigned v=0;v<6;++v)trigger(v,127);
    auto disconnected=engine;
    for(unsigned v=0;v<6;++v)disconnected.configureLfoDestination({mc::SourceKind::Lfo,mc::voiceFromZeroBased(v)},mc::OriginalTargetIndex::fromRaw(0));
    int16_t a[32],b[32],c[32],d[32],x[32],y[32],z[32],w[32];
    measured=true;engine.processBlock(a,b,c,d);measured=false;
    runtime_ref_tick();for(unsigned v=0;v<6;++v){state(v);fieldCheck(88+v);}
    disconnected.processBlock(x,y,z,w);
    check("Engine actual PCM connection",std::memcmp(a,x,sizeof a)!=0||std::memcmp(b,y,sizeof b)!=0);
    check("authority remains raw",[] (auto& auth,const auto& r){typename lxr::ParameterStateAuthority<228>::Values snap{};return auth.trySnapshot(snap)&&snap==r;}(authority,raw));
    check("LXRS reencode",lxr::state::Encode(rawPatch(),after));check("LXRS bytes identical",before==after);
    for(unsigned v=0;v<6;++v)for(unsigned n=0;n<206;++n) {
        const auto index=mc::OriginalTargetIndex::fromRaw(n);const auto src=mc::SourceIdentity{mc::SourceKind::Lfo,mc::voiceFromZeroBased(v)};
        const auto expected=lxr::validateLfoDestination(src,index);
        if(expected==lxr::LfoRouteStatus::Ok)continue;
        const auto prior=engine.lfoRoute(src.owner)->target;const float priorField=portField(88+v);
        check("reject unsupported route",engine.configureLfoDestination(src,index)==expected);
        check("reject is transactional",engine.lfoRoute(src.owner)->target==prior&&equal(portField(88+v),priorField));
    }
    check("invalid owner safe",engine.nativeLfo(6)==nullptr);
    check("unsupported BPM transactional",!engine.setNativeTempo(301)&&engine.nativeTempo()==120);
    for(unsigned id=149;id<=172;++id)if(id<=154||id>=161) {
        set(id,127);for(unsigned v=0;v<6;++v)check("opaque selectors inert",engine.lfoRoute(mc::voiceFromZeroBased(v))->enabled);
    }
    engine.init();for(unsigned v=0;v<6;++v)check("full reset clears routes/noise",!engine.lfoRoute(mc::voiceFromZeroBased(v))->enabled&&!engine.velocityRoute(mc::voiceFromZeroBased(v))->enabled&&engine.nativeLfo(v)->heldValue==0);
    std::array<uint8_t,228> restored{};check("restore decode",static_cast<bool>(lxr::state::Decode(before.data(),before.size(),restored,false)));
    for(unsigned id=0;id<228;++id)params.set(id,restored[id],engine);
    for(unsigned v=0;v<6;++v)check("restore does not activate selectors",!engine.lfoRoute(mc::voiceFromZeroBased(v))->enabled);
    auto copy=engine;copy.drums[0].vol=.25f;copy.configureLfoDestination({mc::SourceKind::Lfo,mc::Voice::Drum1},indexFor(88));copy.nativeLfo(0)->amount=1;copy.nativeLfo(0)->waveform=4;
    const float other=engine.drums[0].vol;copy.beginModulationTick();check("Engine copy owns fields",equal(engine.drums[0].vol,other));
}
void capacityAndTiming() {
    reset();
    // Maximal twelve distinct physical fields, twelve active nodes. Silent
    // voices advance too; no trigger is needed to run LFOs.
    for(unsigned v=0;v<6;++v) {
        set(143+v,64);set(121+v,64);set(155+v,2);set(115+v,127);
        route(v,indexFor(88+v));routeLfo(v,indexFor(37+v));
    }
    for(int b=0;b<32;++b) {
        measured=true;engine.beginModulationTick();runtime_ref_tick();measured=false;
        for(unsigned v=0;v<6;++v){state(v);fieldCheck(88+v);fieldCheck(37+v);}
    }
    for(unsigned v=0;v<6;++v)trigger(v,1+20*v);
    measured=true;engine.beginModulationTick();runtime_ref_tick();measured=false;
    for(unsigned v=0;v<6;++v){fieldCheck(88+v);fieldCheck(37+v);}
    // The native adapter caches 32 PCM frames. A change just after rendering
    // cannot rewrite those frames or advance the LFO until frame32.
    reset();set(121,127);set(155,4);set(115,127);set(173,1);set(185,64);
    routeLfo(0,indexFor(88));trigger(0,127);
    lxr::LXRAudioAdapter adapter;int16_t left,right;
    measured=true;adapter.getNextST1Sample(engine,left,right);measured=false;
    runtime_ref_tick();state(0);const uint32_t afterTick=engine.nativeLfo(0)->phase;
    const auto unmodified=engine;auto counterfactual=unmodified;
    lxr::LXRAudioAdapter counterAdapter=adapter;
    trigger(0,64);state(0); // retrigger resets phase BEFORE velocity
    set(88,96); // accepted between boundaries, current effective updates now
    for(int frame=1;frame<32;++frame) {
        int16_t a,b;measured=true;adapter.getNextST1Sample(engine,left,right);measured=false;
        counterAdapter.getNextST1Sample(counterfactual,a,b);
        check("cached native PCM unaffected before frame32",a==left&&b==right);
        check("no additional tick in cached block",engine.nativeLfo(0)->phase==engine.nativeLfo(0)->phaseOffset);
    }
    measured=true;adapter.getNextST1Sample(engine,left,right);measured=false;
    runtime_ref_tick();state(0);fieldCheck(88);
    check("next native boundary advances",engine.nativeLfo(0)->phase!=engine.nativeLfo(0)->phaseOffset&&afterTick!=0);
    // Host resampling calls the SAME native block service; regardless of host
    // rate, observed phase deltas are integral native increments, never per-frame.
    for(double rate:{44100.,48000.,96000.}) {
        reset();set(115,64);set(155,2);
        lxr::LXRAudioAdapter host;check("configure host adapter",host.configure(rate));
        uint32_t prior=engine.nativeLfo(0)->phase;unsigned ticks=0;
        for(int frame=0;frame<300;++frame) {
            double l,r;measured=true;host.getNextHostST1Sample(engine,l,r);measured=false;
            const auto now=engine.nativeLfo(0)->phase;
            if(now!=prior) {
                check("host-independent native increment",now-prior==engine.nativeLfo(0)->phaseInc);
                ++ticks;prior=now;
            }
        }
        check("resampled audio advanced native blocks",ticks>0&&ticks<30);
    }
}
} // namespace
void* operator new(std::size_t n){if(measured)++allocations;void* p=std::malloc(n?n:1);if(!p)std::abort();return p;}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    metrics=std::fopen(argv[1],"w");if(!metrics)return 2;
    std::fprintf(metrics,"category,voice,setting,block,frames,mismatches,first_sample,max_difference,rms_difference\n");
    if(!engine.loadAssets("data")||!ref_snare_load_assets("data"))return 2;
    FILE* f=std::fopen("data/midi_note_frequencies.bin","rb");if(!f)return 2;
    if(std::fread(originalNotes,sizeof(float),128,f)!=128)return 2;
    std::fclose(f);
    primitives();matrix();ordering();lifecycleAndEngine();capacityAndTiming();
    check("zero allocations",allocations==0);std::fclose(metrics);
    std::printf("SUMMARY pass=%d fail=%d pcmCases=%d samples=%lld mismatches=%lld allocations=%d\n",passed,failed,pcmCases,samples,mismatches,allocations);
    std::printf("STORAGE Engine=%zu Lfo=%zu PhysicalBase=%zu Route=%zu\n",sizeof(lxr::Engine),sizeof(lxr::Lfo),sizeof(lxr::PhysicalModulationBase),sizeof(lxr::VelocityRouteState));
    return failed?1:0;
}
