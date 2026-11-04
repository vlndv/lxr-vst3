// Original LXR v0.37 velocity route differential; LICENSE-LXR.txt.
#include "../interfaces/VelocityRouting.h"
#include "../dsp/Engine.h"
#include "../dsp/ParameterArray.h"
#include "../dsp/ParameterStateAuthority.h"
#include "../dsp/MidiParser.h"
#include "../vendor/iPlug2/Examples/LXR/LXRState.h"
#include "reference/velocity/original_velocity_adapter.h"
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
    int16_t a[32],b[32];measured=true;engine.beginVelocityModulationTick();velocity_ref_tick();async(v);renderPort(v,a);base_ref_render(v,b);measured=false;
    pcm(category,v,setting,n,a,b);
}
void reset() {
    engine.init();std::memcpy(engine.noteFreq,originalNotes,sizeof(originalNotes));base_ref_reset(seed);params.init();
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
void matrix() {
    unsigned routes=0,above127=0,changed=0;
    for(const auto& target:mc::kOriginalTargets){
        const unsigned p=target.parameter.value();
        if(!lxr::velocityParameterSupported(target.parameter))continue;
        ++routes;if(target.index.value()>127)++above127;
        const unsigned v=static_cast<unsigned>(target.voice);
        check("independent AVR target identity",lxr_ref_parameter(target.index.value())==p);
        check("own scope",target.scope==mc::RoutingScope::Voice);
        for(int value:{0,64,127})for(int depth:{0,64,127})for(int vel:{0,1,64,127}){
            reset();set(p,value);set(143+v,depth);
            const float unmodulated=portField(p);const auto patch=rawPatch();
            route(v,target.index);
            check("actual original destination type",velocity_ref_type(p)==static_cast<unsigned>(target.valueType));
            measured=true;trigger(v,vel,63,v==5);measured=false;
            fieldCheck(p);
            if(value==64&&depth==127&&vel==64){check("real effective field changes",!equal(unmodulated,portField(p)));++changed;}
            for(int b=0;b<3;b++){
                block("all_verified_routes",v,p,b);fieldCheck(p);
                check("base patch unchanged",rawPatch()==patch);
                const auto* state=engine.velocityRoute(target.voice);
                check("route identity retained",state&&state->enabled&&state->target==target.index&&state->destination==target.destination);
            }
        }
    }
    check("107 supported rows",routes==107);check("all have effect proof",changed==routes);check("full-range routes exercised",above127>0);
    std::printf("COVERAGE routes=%u above127=%u effectProofs=%u\n",routes,above127,changed);
}
void retriggerAndEdits(){
    for(unsigned v=0;v<6;v++)for(unsigned p:{88+v,37+v,62+v}){
        reset();set(p,64);set(143+v,127);route(v,indexFor(p));
        const auto patch=rawPatch();const auto before=*engine.velocityRoute(mc::voiceFromZeroBased(v));
        // Original pre-tick compounding is observable but NEVER captured as base.
        for(int vel:{1,64,127,0}){measured=true;trigger(v,vel);measured=false;fieldCheck(p);}
        check("retrigger does not replace converted base",equal(before.baseFloat,engine.velocityRoute(mc::voiceFromZeroBased(v))->baseFloat));
        for(int b=0;b<3;b++){block("retrigger_replay",v,p,b);fieldCheck(p);}
        check("retrigger leaves persisted patch",rawPatch()==patch);
        set(p,96);fieldCheck(p);trigger(v,64);fieldCheck(p);block("base_edit_then_trigger",v,p,0);fieldCheck(p);
        set(p,32);fieldCheck(p);block("base_edit_held_velocity",v,p,1);fieldCheck(p);
        set(143+v,0);block("depth_zero_restores_base",v,p,2);fieldCheck(p);
        set(143+v,127);block("depth_edit_without_trigger",v,p,3);fieldCheck(p);
        route(v,indexFor(191+v));trigger(v,64);fieldCheck(191+v);block("active_destination_change",v,191+v,4);fieldCheck(191+v);
        route(v,mc::OriginalTargetIndex::fromRaw(0));fieldCheck(191+v);
        check("off disabled",!engine.velocityRoute(mc::voiceFromZeroBased(v))->enabled);
    }
    // Coarse/fine aliases preserve multiplier base1 and do not recapture effective.
    for(unsigned v=0;v<6;v++){
        reset();set(143+v,127);const unsigned coarse=v<3?8+v*2:14+(v-3)*2;
        route(v,indexFor(coarse));trigger(v,64);block("alias_initial",v,coarse,0);
        set(coarse+1,127);fieldCheck(coarse);route(v,indexFor(coarse+1));
        check("alias base remains one",equal(engine.velocityRoute(mc::voiceFromZeroBased(v))->baseFloat,1.f));
        trigger(v,1);block("alias_base_change",v,coarse,1);fieldCheck(coarse);
    }
}
void rejectsAndOwnership(){
    using S=lxr::VelocityRouteStatus;
    reset();set(143,127);route(0,indexFor(88));trigger(0,64);block("ownership_initial",0,88,0);
    const auto saved=*engine.velocityRoute(mc::Voice::Drum1);const float field=engine.drums[0].vol;
    for(unsigned n=0;n<206;n++){
        auto index=mc::OriginalTargetIndex::fromRaw(n);const auto* t=mc::lookup(index);
        auto expected=lxr::validateVelocityDestination(source(0),index);
        if(expected==S::Ok)continue;
        measured=true;const auto result=engine.configureVelocityDestination(source(0),index);measured=false;
        check("unsupported diagnostic",result==expected);
        check("reject preserves field/route",equal(field,engine.drums[0].vol)&&engine.velocityRoute(mc::Voice::Drum1)->target==saved.target);
        if(t&&t->runtimePolicy==mc::RuntimePolicy::RequiresDestinationMutationReview&&t->voice==mc::Voice::Drum1)
            check("mutable destination rejected",result==S::MutableDestinationUnsupported);
    }
    for(unsigned v=0;v<6;v++){
        const auto own=mc::voiceFromZeroBased(v);
        for(const auto& t:mc::kOriginalTargets){
            if(t.runtimePolicy==mc::RuntimePolicy::RequiresDestinationMutationReview&&t.voice==own)
                check("six mutation paths rejected",engine.configureVelocityDestination(source(v),t.index)==S::MutableDestinationUnsupported);
        }
        for(unsigned x=0;x<128;x++){
            check("legacy has no guessed meaning",engine.configureLegacyVelocitySelector(mc::HostParameterId::fromRaw(149+v),mc::SevenBitValue::fromRaw(x))==S::AmbiguousLegacySelector);
            const auto previous=engine.velocityRoute(own)->target;params.set(149+v,x,engine);
            check("legacy raw writes inert",engine.velocityRoute(own)->target==previous);
        }
    }
    check("invalid source",engine.configureVelocityDestination({mc::SourceKind::Lfo,mc::Voice::Drum1},indexFor(88))==S::InvalidSource);
    check("invalid owner",engine.configureVelocityDestination(source(7),indexFor(88))==S::InvalidSource);
    check("invalid index",engine.configureVelocityDestination(source(0),mc::OriginalTargetIndex::fromRaw(99999))==S::InvalidIndex);
    check("invalid seven bits",engine.configureLegacyVelocitySelector(mc::HostParameterId::fromRaw(149),mc::SevenBitValue::fromRaw(128))==S::InvalidLegacyValue);
    reset();engine.drums[0].vol=INFINITY;
    check("nonfinite candidate rejected",engine.configureVelocityDestination(source(0),indexFor(88))==S::NonFiniteBase);
    check("nonfinite rejection leaves route disabled",!engine.velocityRoute(mc::Voice::Drum1)->enabled);
    reset();engine.velocityModulators[0].amount=2;
    check("invalid depth rejected",engine.configureVelocityDestination(source(0),indexFor(88))==S::InvalidDepth);
    reset();set(143,127);route(0,indexFor(88));trigger(0,64);engine.beginVelocityModulationTick();
    const float secondVoice=engine.drums[1].vol;
    check("cross voice rejected",engine.configureVelocityDestination(source(0),indexFor(89))==S::VoiceMismatch);
    check("no cross voice mutation",equal(engine.drums[1].vol,secondVoice));
    engine.velocityModulators[0].amount=NAN;const float prior=engine.drums[0].vol;engine.triggerDrum(0,64,63);
    check("invalid active depth safe",equal(prior,engine.drums[0].vol)&&engine.velocityRoute(mc::Voice::Drum1)->status==S::InvalidDepth);
    engine.velocityModulators[0].amount=1;
    const auto invalidPhase=engine.drums[0].osc.phase;const auto invalidState=engine.drums[0].oscVolEg.state;
    engine.triggerDrum(0,255,63);
    check("invalid velocity diagnosed",engine.velocityRoute(mc::Voice::Drum1)->status==S::InvalidVelocity);
    check("invalid velocity leaves phase/envelope unchanged",engine.drums[0].osc.phase==invalidPhase&&engine.drums[0].oscVolEg.state==invalidState);
}
void amplitudeMidiAndPersistence(){
    for(unsigned v=0;v<6;v++)for(int amplitude:{0,1})for(int depth:{0,127})for(int vel:{0,1,64,127}){
        reset();set(137+v,amplitude);set(143+v,depth);route(v,indexFor(88+v));trigger(v,vel,63,v==5);
        block("independent_amplitude_velocity",v,amplitude*128+depth,vel);
    }
    for(unsigned v=0;v<6;v++){
        reset();set(143+v,127);route(v,indexFor(88+v));
        lxr::MidiParser parser;lxr::MidiEvent note{static_cast<uint8_t>(0x90|v),static_cast<uint8_t>(v),63,64};
        parser.processEvent(note,engine,params);base_ref_trigger(v,64,63,0);
        fieldCheck(88+v);block("actual_midi_trigger",v,88+v,0);
        const float held=engine.velocityModulators[v].modValue;
        note.data2=0;parser.processEvent(note,engine,params);
        check("MIDI zero velocity is no retrigger",equal(held,engine.velocityModulators[v].modValue));
        auto raw=rawPatch();lxr::ParameterStateAuthority<228> authority;authority.initialize(raw);
        std::array<uint8_t,lxr::state::kEncodedSize> before{},after{};
        check("encode before routing",lxr::state::Encode(raw,before));
        for(int b=0;b<3;b++)block("persisted_base_retained",v,88+v,b);
        lxr::ParameterStateAuthority<228>::Values snapshot{};check("authority snapshot",authority.trySnapshot(snapshot));
        check("authoritative base untouched",snapshot==raw&&rawPatch()==raw);
        check("encode after routing",lxr::state::Encode(rawPatch(),after));check("LXRS v1 bytes unchanged",before==after);
        engine.init();check("reset removes transient route",!engine.velocityRoute(mc::voiceFromZeroBased(v))->enabled);
        // Simulate the existing restore boundary: init then decode and raw replay.
        std::array<uint8_t,228> restored{};check("decode unchanged v1",static_cast<bool>(lxr::state::Decode(before.data(),before.size(),restored,false)));
        for(unsigned p=0;p<228;p++)params.set(p,restored[p],engine);
        check("restore does not activate saved selector bytes",!engine.velocityRoute(mc::voiceFromZeroBased(v))->enabled);
    }
    reset();for(unsigned v=0;v<6;v++){set(143+v,127);route(v,indexFor(88+v));trigger(v,1+v*20);}
    engine.beginVelocityModulationTick();velocity_ref_tick();
    for(unsigned v=0;v<6;v++)fieldCheck(88+v);
    // Exercise the ACTUAL Engine::processBlock connection and detector sensitivity.
    reset();set(143,127);route(0,indexFor(88));trigger(0,64);
    auto disconnected=engine;disconnected.configureVelocityDestination(source(0),mc::OriginalTargetIndex::fromRaw(0));
    int16_t a[32],b[32],c[32],d[32],x[32],y[32],z[32],w[32];
    measured=true;engine.processBlock(a,b,c,d);measured=false;
    disconnected.processBlock(x,y,z,w);
    check("disconnection changes actual rendered PCM",std::memcmp(a,x,sizeof(a))!=0||std::memcmp(b,y,sizeof(b))!=0);
}
void positiveFmAndCutoffDisable(){
    for(unsigned v:{4u,5u})for(unsigned op:{1u,2u})for(int value:{0,64,127})for(int depth:{0,64,127})for(int vel:{0,1,64,127}){
        reset();set(v==4?6:7,0);set(v==4?29:33,40);set(v==4?30:34,48);
        set(v==4?31:35,64);set(v==4?32:36,64);
        const unsigned p=v==4?(op==1?31:32):(op==1?35:36);
        set(p,value);set(143+v,depth);route(v,indexFor(p));trigger(v,vel);
        // Exactly the disclosed M2B positive-modulation comparison interval.
        for(int i:{1,2}){osc(v,i).phase=0x80000000u;base_ref_phase(v,i,0x80000000u);}
        block("positive_FM_velocity_gain",v,p,depth*128+vel);fieldCheck(p);
    }
    for(unsigned v=0;v<6;v++){
        reset();set(191+v,0);set(37+v,100);set(143+v,127);route(v,indexFor(37+v));trigger(v,64);
        block("cutoff_before_disable",v,37+v,0);route(v,mc::OriginalTargetIndex::fromRaw(0));
        block("cutoff_disabled_coefficient_restore",v,37+v,1);BaseState state{};base_ref_state(v,&state);
        check("disabled cutoff coefficient agrees",equal(filter(v).g,state.coefficient));
    }
}
} // namespace
void* operator new(std::size_t n){if(measured)++allocations;void* p=std::malloc(n?n:1);if(!p)std::abort();return p;}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    metrics=std::fopen(argv[1],"w");if(!metrics)return 2;
    std::fprintf(metrics,"category,voice,setting,block,frames,mismatches,first_sample,max_difference,rms_difference\n");
    if(!engine.loadAssets("data")||!ref_snare_load_assets("data"))return 2;
    FILE* f=std::fopen("data/midi_note_frequencies.bin","rb");if(!f)return 2;
    if(std::fread(originalNotes,sizeof(float),128,f)!=128)return 2;
    std::fclose(f);
    matrix();retriggerAndEdits();rejectsAndOwnership();amplitudeMidiAndPersistence();positiveFmAndCutoffDisable();
    check("zero configure/set/trigger/render allocations",allocations==0);std::fclose(metrics);
    std::printf("SUMMARY pass=%d fail=%d pcmCases=%d samples=%lld mismatches=%lld allocations=%d\n",passed,failed,pcmCases,samples,mismatches,allocations);
    std::printf("ALLOCATION_SCOPES parameterCalls=%d triggerCalls=%d configureCalls=%d renderCases=%d allocations=%d\n",parameterCalls,triggerCalls,configureCalls,pcmCases,allocations);
    return failed?1:0;
}
