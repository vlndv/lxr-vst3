// Independent original-C verification, not a production LFO implementation.
// Original Copyright 2013 Julian Schmidt; derivative terms: LICENSE-LXR.txt.
#include "../dsp/Lfo.h"
#include "../dsp/ModulationContract.h"
#include "reference/lfo/original_lfo_adapter.h"
#include "reference/base_mapping/original_base_adapter.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <algorithm>
extern "C" unsigned lxr_ref_parameter(unsigned);
extern "C" unsigned lxr_ref_target_count(void);

// PREDECLARED acceptance: bitwise float equality for deterministic finite
// arithmetic, exact integer equality. No tolerance and no rendered-port PCM claim.
struct Stats { unsigned count=0, mismatches=0; long first=-1; double max=0,sum=0; };
static std::map<std::string,Stats> stats;
static unsigned pass=0,fail=0;
static void observe(const std::string& group,double a,double b,bool equal) {
    auto& s=stats[group];double e=std::fabs(a-b);
    if(!equal){if(s.first<0)s.first=s.count;++s.mismatches;}
    ++s.count;s.max=std::max(s.max,e);s.sum+=e*e;
}
static void eq(const std::string& group,float a,float b) {
    bool same=std::memcmp(&a,&b,sizeof a)==0;observe(group,a,b,same);
    if(same)++pass;else{if(fail<20)std::printf("FAIL %s got=%.9g expected=%.9g\n",group.c_str(),a,b);++fail;}
}
static void integer(const std::string& group,uint32_t a,uint32_t b) {
    observe(group,a,b,a==b);if(a==b)++pass;else{if(fail<20)std::printf("FAIL %s got=%u expected=%u\n",group.c_str(),a,b);++fail;}
}
static LfoReferenceState get(unsigned v=0){LfoReferenceState s{};lfo_ref_get(v,&s);return s;}
static void reset(){base_ref_reset(0x12345678);lfo_ref_mixer_init();lfo_ref_tempo(120);}
static void fixed(unsigned v,unsigned wave=1,uint32_t phase=0x40000000u) {
    auto s=get(v);s.phase=phase;s.increment=0;s.waveform=wave;s.multiplier=1;s.random=.375f;lfo_ref_put(v,&s);
}
static float operation(float x,float depth,float value){return x*depth*value+(1.f-depth)*x;}
static float expectedWave(unsigned w,uint32_t phase,float held) {
    // Expected values derived separately from the source contract. The oracle
    // executes the original C; this function is ONLY the independent expectation.
    float x=phase/4294967296.f;
    switch(w){
        case 0:return ((lfo_ref_sine(phase>>20)/32767.f)+1)/2.f;
        case 1:return 1.f-std::fabs(x*2-1);
        case 2:return x;
        case 3:return (0xffffffffu-phase)/4294967296.f;
        case 4:return phase>0x7fffffffu?1.f:0.f;
        case 5:return held;
        case 6:return x*x*x;
        case 7:{float y=(0xffffffffu-phase)/4294967296.f;return y*y*y;}
        default:return 0;
    }
}
static void primitive() {
    // reset() also invokes the tempo-publication service, which recalculates
    // all increments. Exercise lfo_init itself before asserting its defaults.
    reset();lfo_ref_init(0);auto s=get();integer("init/phase",s.phase,0);integer("init/increment",s.increment,1);
    eq("init/frequency",s.frequency,1);eq("init/multiplier",s.multiplier,1);
    // lfo_init deliberately does not reset rnd, unlike a zeroed engine image.
    s.random=.375f;lfo_ref_put(0,&s);lfo_ref_init(0);eq("init/random-retained",get().random,.375f);
    for(unsigned v=0;v<128;++v){
        lfo_ref_rate(0,float(v));s=get();float x=(v+1)/128.f;float hz=x*x*x*200;
        eq("rate/original-hz",s.frequency,hz);
        uint32_t inc=uint32_t(hz/(44002.7573529412f/32.f)*4294967296.f);
        integer("rate/original-increment",s.increment,inc);
        lxr::Lfo cpp;cpp.init();cpp.setRate(lxr::lfoFrequencyFromMidi(uint8_t(v)));
        observe("cpp/rate-increment",cpp.phaseInc,inc,cpp.phaseInc==inc);
        eq("rate/cpp-hz-helper",lxr::lfoFrequencyFromMidi(uint8_t(v)),hz);
        if(v<127){base_ref_set(185,v);uint32_t offset=uint32_t((v/127.f)*4294967296.f);
            integer("offset/defined-original",get().offset,offset);
            integer("offset/cpp-defined",lxr::lfoPhaseOffsetFromMidi(uint8_t(v)),offset);}
    }
    integer("rate/explicit-zero-hz",lfo_ref_increment(0,0),0);
    for(unsigned bpm:{0u,1u,120u,300u})for(unsigned sync=0;sync<14;++sync){
        lfo_ref_tempo(bpm);lfo_ref_rate(0,64);lfo_ref_sync(0,sync);s=get();
        const float scalers[]={.25f,.5f,1,2,3,4,6,8,12,16,32};
        float hz=sync>=1&&sync<=11?(bpm/60.f/4)*scalers[sync-1]:s.frequency;
        integer("sync/increment",s.increment,uint32_t((hz/(44002.7573529412f/32))*4294967296.f));
        integer("sync/phase-retained",s.phase,0);
    }
    for(unsigned owner=0;owner<6;++owner)for(unsigned w=0;w<9;++w){
        for(unsigned i=0;i<4101;++i){
            uint32_t phase=i<4096?uint32_t(i<<20):i==4096?0xffffffffu:i==4097?0x7fffffffu:i==4098?0x80000000u:i==4099?0xfffffff0u:1u;
            fixed(owner,w,phase);float actual=lfo_ref_calc(owner);float wanted=expectedWave(w,phase,.375f);
            eq("wave/original-"+std::to_string(w),actual,wanted);
            lxr::Lfo cpp;cpp.init();cpp.waveform=uint8_t(w);cpp.phase=phase;cpp.heldValue=.375f;cpp.tick();
            observe("cpp/wave-"+std::to_string(w),cpp.output,actual,std::memcmp(&cpp.output,&actual,4)==0);
        }
    }
    for(uint32_t phase:{0u,0x7fffffffu,0xfffffff0u,0xffffffffu})
    for(uint32_t inc:{0u,1u,317u,0x12345678u,0x7fffffffu})
    for(float mult:{0.f,.5f,1.f}){
        fixed(0,2,phase);s=get();s.increment=inc;s.multiplier=mult;lfo_ref_put(0,&s);
        uint32_t next=phase+uint32_t(float(inc)*mult);
        eq("phase/wrapped-output",lfo_ref_calc(0),expectedWave(2,next,.375f));
        integer("phase/two-stage-increment",get().phase,next);
    }
    // Hardware RNG is replaced at its service boundary by the established
    // deterministic xorshift fixture; assert WHEN it is sampled, not MCU bytes.
    reset();fixed(0,5,0xffffffffu);s=get();s.increment=1;lfo_ref_put(0,&s);
    uint32_t rng=0x12345678;rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
    float random=float(rng)/4294967296.f;
    eq("noise/wrap-service",lfo_ref_calc(0),random);
    eq("noise/held-between-wraps",lfo_ref_calc(0),random);
    // Parameter changes preserve phase; offset takes effect only on retrigger.
    fixed(0);uint32_t phaseBefore=get().phase;lfo_ref_rate(0,127);lfo_ref_sync(0,6);base_ref_set(185,64);
    integer("changes/phase-retained",get().phase,phaseBefore);
    for(unsigned track=0;track<7;++track)for(unsigned selected=0;selected<8;++selected){
        reset();for(unsigned owner=0;owner<6;++owner){s=get(owner);s.phase=11;s.offset=0x12340000u+owner;s.retrigger=selected;lfo_ref_put(owner,&s);}
        lfo_ref_retrigger(track);
        for(unsigned owner=0;owner<6;++owner)integer("retrigger/global-compare",get(owner).phase,selected==track+1?0x12340000u+owner:11);
    }
    // Actual closed/open hat triggers both pass 5, so selection 7 does not fire.
    for(int open=0;open<2;++open)for(unsigned selected:{6u,7u}){
        reset();s=get();s.phase=11;s.offset=123;s.retrigger=selected;lfo_ref_put(0,&s);
        base_ref_trigger(5,127,63,open);integer("retrigger/actual-hat",get().phase,selected==6?123:11);
    }
}
static void destinations() {
    using namespace lxr::modulation_contract;
    integer("destinations/avr-count",lxr_ref_target_count(),205);
    for(unsigned index=0;index<205;++index){
        unsigned p=lxr_ref_parameter(index);integer("destinations/typed-id",kOriginalTargets[index].parameter.value(),p);
        reset();if(index==0){lfo_ref_route(0,0);lfo_ref_dispatch(0);integer("destinations/off",get().destination,0);continue;}
        integer("destinations/actual-pointer",lfo_ref_has_field(p),1);
        integer("destinations/actual-type",lfo_ref_type(p),unsigned(kOriginalTargets[index].valueType));
        // Never mutate a live route address exposed as a low byte of uint16.
        if(p>=149&&p<=154)continue;
        float initial=lfo_ref_type(p)==0?64.f:lfo_ref_type(p)==3?1024.f:.75f;
        lfo_ref_write(p,initial,1);lfo_ref_route(0,p);
        auto s=get();s.amount=.5f;lfo_ref_put(0,&s);
        // amount targeting itself overwrites .5 during the setup above.
        initial=lfo_ref_field(p);
        float want=operation(initial,.5f,.5f);
        if(lfo_ref_type(p)==0||lfo_ref_type(p)==3)want=float(uint32_t(want));
        lfo_ref_modulate(0,.5f);eq("destinations/node-update",lfo_ref_field(p),want);
        lfo_ref_reset_targets();
        float restored=kOriginalTargets[index].valueType==ParameterValueType::FrequencyMultiplier?1.f:
            p==121?.75f:initial;
        eq("destinations/restoration",lfo_ref_field(p),restored);
    }
    // Modulation writes sync storage only; setter/tempo changes recalculate.
    reset();lfo_ref_rate(1,64);uint32_t increment=get(1).increment;
    lfo_ref_write(180,6,1);lfo_ref_route(0,180);auto s=get();s.amount=1;lfo_ref_put(0,&s);
    lfo_ref_modulate(0,.5f);integer("sync/target-storage",get(1).sync,3);
    integer("sync/target-no-recalc",get(1).increment,increment);
    lfo_ref_tempo(120);integer("sync/tempo-recalculates-live-byte",get(1).increment,lfo_ref_increment(get(1).frequency,3));
    lfo_ref_reset_targets();integer("sync/restored-byte",get(1).sync,6);
    integer("sync/restoration-no-recalc",get(1).increment,lfo_ref_increment(get(1).frequency,3));
    // Lower owner modulates a later LFO's frequency before that LFO advances.
    for(bool forward:{false,true}){
        reset();unsigned source=forward?0:1,target=forward?1:0;
        lfo_ref_route(source,115+target);fixed(source,4,0);s=get(source);s.amount=1;lfo_ref_put(source,&s);
        fixed(target,2,0);s=get(target);s.increment=100;lfo_ref_put(target,&s);
        int16_t a[64],b[64];lfo_ref_tick(a,b);
        integer("ordering/lfo-to-lfo-frequency",get(target).phase,forward?0:100);
        lfo_ref_tick(a,b);integer("ordering/backward-restores-before-use",get(target).phase,forward?0:200);
    }
}
static void shared() {
    for(unsigned owner=0;owner<6;++owner)for(float vd:{0.f,.5f,1.f})for(float ld:{0.f,.5f,1.f}){
        reset();unsigned p=88+owner;lfo_ref_write(p,1,1);base_ref_set(143+owner,vd==0?0:vd==1?127:64);
        float depth=vd==.5f?64/127.f:vd;
        lfo_ref_velocity_route(owner,p);lfo_ref_route(owner,p);fixed(owner);
        auto s=get(owner);s.amount=ld;lfo_ref_put(owner,&s);
        float velocity=64/127.f;
        base_ref_trigger(owner,64,63,0);float first=operation(1,depth,velocity);
        eq("shared/trigger-current",lfo_ref_field(p),first);
        lfo_ref_dispatch(owner);float second=operation(first,ld,.5f);
        eq("shared/lfo-after-velocity",lfo_ref_field(p),second);
        base_ref_trigger(owner,64,63,0);float repeated=operation(second,depth,velocity);
        eq("shared/repeated-trigger",lfo_ref_field(p),repeated);
        int16_t a[64],b[64];lfo_ref_tick(a,b);
        eq("shared/full-mixer-restore-replay",lfo_ref_field(p),second);
        lfo_ref_tick(a,b);eq("shared/no-block-accumulation",lfo_ref_field(p),second);
        base_ref_set(p,64);float base=64/127.f;
        eq("shared/new-base-immediate",lfo_ref_field(p),base);
        eq("shared/new-base-velocity-snapshot",lfo_ref_saved(owner,1),base);
        eq("shared/new-base-lfo-snapshot",lfo_ref_saved(owner,0),base);
        lfo_ref_tick(a,b);eq("shared/new-base-replay",lfo_ref_field(p),operation(operation(base,depth,velocity),ld,.5f));
        lfo_ref_route(owner,0);lfo_ref_tick(a,b);
        eq("shared/off-restores-with-velocity",lfo_ref_field(p),operation(base,depth,velocity));
    }
    // Narrow at each node, and preserve exact node order (not factor product).
    for(unsigned base=1;base<128;++base){
        reset();lfo_ref_write(94,float(base),1);base_ref_set(143,64);lfo_ref_velocity_route(0,94);lfo_ref_route(0,94);
        fixed(0);auto s=get();s.amount=64/127.f;lfo_ref_put(0,&s);base_ref_trigger(0,64,63,0);
        float depth=64/127.f,v=64/127.f;float first=float(uint8_t(operation(float(base),depth,v)));
        lfo_ref_dispatch(0);eq("shared/byte-each-node",lfo_ref_field(94),float(uint8_t(operation(first,depth,.5f))));
    }
    // Two LFO owners can address one field in original C; ascending owner order.
    reset();lfo_ref_write(88,1,1);lfo_ref_route(0,88);lfo_ref_route(1,88);fixed(0);fixed(1);
    auto s=get();s.amount=.5f;lfo_ref_put(0,&s);s=get(1);s.amount=1;lfo_ref_put(1,&s);
    int16_t a[64],b[64];lfo_ref_tick(a,b);eq("shared/two-lfo",lfo_ref_field(88),.375f);
    // Signed coefficient, alias multiplier, independent fields, and engine reset.
    reset();lfo_ref_write(62,-4,1);lfo_ref_route(0,62);fixed(0);s=get();s.amount=.5f;lfo_ref_put(0,&s);
    lfo_ref_dispatch(0);eq("shared/signed-slope",lfo_ref_field(62),-3);
    reset();lfo_ref_velocity_route(0,8);lfo_ref_route(0,9);base_ref_set(143,127);fixed(0);s=get();s.amount=1;lfo_ref_put(0,&s);
    base_ref_trigger(0,64,63,0);lfo_ref_tick(a,b);eq("shared/coarse-fine-alias",lfo_ref_field(8),(64/127.f)*.5f);
    base_ref_set(8,50);lfo_ref_tick(a,b);eq("shared/pitch-edit-base-one",lfo_ref_field(9),(64/127.f)*.5f);
    reset();lfo_ref_write(88,1,1);lfo_ref_write(89,.75f,1);lfo_ref_velocity_route(0,88);base_ref_set(143,127);
    lfo_ref_route(0,89);fixed(0);s=get();s.amount=1;lfo_ref_put(0,&s);base_ref_trigger(0,64,63,0);
    lfo_ref_tick(a,b);eq("shared/different-velocity-field",lfo_ref_field(88),64/127.f);eq("shared/different-lfo-field",lfo_ref_field(89),.375f);
    reset();integer("reset/destination-off",get().destination,0);integer("reset/phase-zero",get().phase,0);
}
static void timing(const std::string& output) {
    FILE* f=std::fopen((output+"/timing.csv").c_str(),"w");std::fprintf(f,"case,first_changed_native_frame,status\n");
    // A rendered block is immutable; changes between calls first affect the next
    // block. Absolute native frame labels: before B0 -> 0; after B0 -> 32.
    for(unsigned boundary:{0u,1u}){
        int16_t reference[64],changed[64],dummy[64],other[64];
        for(unsigned variant=0;variant<2;++variant){
            reset();for(unsigned v=0;v<6;++v)base_ref_set(88+v,0);
            base_ref_set(88,127);base_ref_set(191,7);base_ref_set(49,0);base_ref_set(50,127);
            base_ref_set(74,0);base_ref_set(82,0);base_ref_set(197,0);base_ref_set(203,0);
            base_ref_set(8,50);base_ref_set(9,64);base_ref_trigger(0,127,63,0);
            lfo_ref_route(0,88);fixed(0,4,0);auto s=get();s.amount=0;lfo_ref_put(0,&s);
            if(boundary)lfo_ref_tick(dummy,other);
            if(variant){s=get();s.amount=1;lfo_ref_put(0,&s);}
            lfo_ref_tick(dummy,variant?changed:reference);
            integer("timing/phase-one-update-per-block",get().phase,0);
        }
        int first=-1;for(int i=0;i<32;++i)if(reference[2*i]!=changed[2*i]||reference[2*i+1]!=changed[2*i+1]){first=i;break;}
        integer("timing/first-rendered-frame",uint32_t(first),0);
        std::fprintf(f,"%s,%d,VERIFIED_ORIGINAL_C_FIXTURE\n",boundary?"edit-after-B0":"edit-before-B0",first<0?-1:int(boundary*32)+first);
    }
    std::fclose(f);
    reset();lfo_ref_rate(0,64);auto s=get();uint32_t inc=uint32_t(float(s.increment));
    base_ref_async(0);integer("timing/async-does-not-advance-lfo",get().phase,0);
    int16_t a[64],b[64];lfo_ref_tick(a,b);integer("timing/full-mixer-one-step",get().phase,inc);
    lfo_ref_tick(a,b);integer("timing/full-mixer-two-steps",get().phase,inc+inc);
}
int main(int argc,char** argv){
    if(argc!=2||!ref_snare_load_assets("data"))return 2;
    primitive();destinations();shared();timing(argv[1]);
    FILE* f=std::fopen((std::string(argv[1])+"/comparisons.csv").c_str(),"w");
    std::fprintf(f,"group,count,mismatches,first_divergence,max_abs_error,rms_error,classification\n");
    for(const auto& entry:stats){const auto& s=entry.second;bool cpp=entry.first.rfind("cpp/",0)==0;
        std::fprintf(f,"%s,%u,%u,%ld,%.17g,%.17g,%s\n",entry.first.c_str(),s.count,s.mismatches,s.first,s.max,
            std::sqrt(s.sum/s.count),cpp?(s.mismatches?"CONFIRMED_CPP_DIVERGENCE":"EQUAL_IN_FIXTURE"):(s.mismatches?"FAIL":"VERIFIED_ORIGINAL_C"));}
    std::fclose(f);std::printf("SUMMARY pass=%u fail=%u groups=%zu (C++ divergences recorded separately)\n",pass,fail,stats.size());
    return fail?1:0;
}
