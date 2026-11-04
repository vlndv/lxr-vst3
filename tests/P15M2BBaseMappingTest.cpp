// Firmware 0.37 mapping/timing differential. Original C remains the oracle.
// Copyright 2013 Julian Schmidt (reference); derived material: LICENSE-LXR.txt.
#include "../dsp/Engine.h"
#include "../dsp/ParameterArray.h"
#include "reference/base_mapping/original_base_adapter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
constexpr uint32_t seed=0x9e3779b9u;
int passed=0,failed=0,allocations=0,pcmCases=0;
long long samples=0,mismatches=0;
bool measured=false;
FILE* metrics=nullptr;
FILE* timingMetrics=nullptr;
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
AmpEg& eg(int v){return v<3?engine.drums[v].oscVolEg:v==3?engine.snare.oscVolEg:v==4?engine.cymbal.oscVolEg:engine.hihat.oscVolEg;}
lxr::ResonantFilter& filter(int v){return v<3?engine.drums[v].filter:v==3?engine.snare.filter:v==4?engine.cymbal.filter:engine.hihat.filter;}
lxr::OscRng& rng(int v){return v<3?engine.drums[v].rng:v==3?engine.snare.rng:v==4?engine.cymbal.rng:engine.hihat.rng;}
float gain(int v,int op){return v==4?(op==1?engine.cymbal.fmModAmount1:engine.cymbal.fmModAmount2):(op==1?engine.hihat.fmModAmount1:engine.hihat.fmModAmount2);}
void set(int id,int value){params.set(static_cast<uint8_t>(id),static_cast<uint8_t>(value),engine);base_ref_set(id,value);}
void trigger(int v,int vel=127,int note=63,int open=0) {
    if(v<3)engine.triggerDrum(v,vel,note);else if(v==3)engine.triggerSnare(vel,note);
    else if(v==4)engine.triggerCymbal(vel,note);else engine.triggerHiHat(vel,open!=0,note);
    base_ref_trigger(v,vel,note,open);
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
    int16_t a[32],b[32];measured=true;async(v);renderPort(v,a);base_ref_render(v,b);measured=false;
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
    // Match actual initialized pitch multipliers (no snap/EG in base fixture).
    for(int v=0;v<6;v++)osc(v).pitchMod=1;
}
void stateCheck(int v) {
    BaseState s{};base_ref_state(v,&s);
    for(int i=0;i<(v<4?2:3);i++){
        check("frequency",equal(osc(v,i).freq,s.freq[i]));
        check("midiFreq",osc(v,i).midiFreq==s.midi[i]);
        check("baseNote",osc(v,i).baseNote==s.base[i]);
        check("phase",osc(v,i).phase==s.phase[i]);
        check("phaseIncrement",osc(v,i).phaseInc==s.increment[i]);
    }
}
void mappingA() {
    const int ids[]={29,30,33,34,31,32,35,36};
    for(int id:ids)for(int note:{0,63,127}){
        reset();const int v=(id==29||id==30||id==31||id==32)?4:5;
        trigger(v,127,note);async(v);
        const int op=(id==29||id==33||id==31||id==35)?1:2;
        for(int value=0;value<128;value++){
            const auto phase=osc(v,op).phase,increment=osc(v,op).phaseInc;
            const auto fine=osc(v,op).midiFreq&255;
            const float d0=engine.drums[0].modOsc.freq,d1=engine.drums[1].modOsc.freq,d2=engine.drums[2].modOsc.freq;
            measured=true;set(id,value);measured=false;
            BaseState s{};base_ref_state(v,&s);
            check("A field",equal(osc(v,op).freq,s.freq[op]));
            check("A coarse/gain",osc(v,op).midiFreq==s.midi[op]&&equal(gain(v,op),s.gain[op-1]));
            check("A preserves fine",(osc(v,op).midiFreq&255)==fine);
            check("A no early phaseInc or phase change",phase==osc(v,op).phase&&increment==osc(v,op).phaseInc);
            check("A no drum frequency writes",equal(d0,engine.drums[0].modOsc.freq)&&equal(d1,engine.drums[1].modOsc.freq)&&equal(d2,engine.drums[2].modOsc.freq));
        }
    }
    // Independent operator PCM includes extreme pitch/gain, live edits and
    // trigger/retrigger. It avoids the unrelated ARM negative-FM cast boundary.
    for(int v:{4,5})for(int op:{1,2})for(int value:{0,64,127}){
        reset();const int freqId=v==4?(op==1?29:30):(op==1?33:34);
        const int gainId=v==4?(op==1?31:32):(op==1?35:36);
        set(freqId,value);set(gainId,value);
        for(int b=0;b<12;b++){
            if(b==0||b==5)trigger(v,127,b==5?45:63,v==5);
            if(b==8)set(freqId,127-value);
            async(v);int16_t a[32],r[32];
            measured=true;lxr::calcNextOscSampleBlock(&osc(v,op),a,32,gain(v,op),engine.tables,rng(v));base_ref_operator(v,op,r);measured=false;
            pcm("A_operator",v,value,b,a,r);stateCheck(v);
        }
    }
    // Matched positive-modulator initial phases exercise actual combined FM
    // with original assets and defined unsigned casts. No C source is patched.
    for(int v:{4,5})for(int a:{0,64,127})for(int b:{0,64,127}){
        reset();set(v==4?29:33,40);set(v==4?30:34,48);
        set(v==4?31:35,a);set(v==4?32:36,b);
        for(int hit=0;hit<2;hit++){
            trigger(v,127,63,hit);
            for(int op:{1,2}){osc(v,op).phase=0x80000000u;base_ref_phase(v,op,0x80000000u);}
            block("A_combined_positive_FM",v,a*128+b,hit);
        }
    }
}
void cutoffB() {
    for(int v=0;v<6;v++){
        reset();for(int x=0;x<128;x++){
            measured=true;set(37+v,x);measured=false;BaseState s{};base_ref_state(v,&s);
            check("B cutoff field",equal(filter(v).f,s.cutoff));check("B coefficient",equal(filter(v).g,s.coefficient));
        }
        for(int type=0;type<8;type++)for(int resonance:{0,127}){
            reset();set(191+v,type);set(43+v,resonance);trigger(v);
            for(int b=0;b<9;b++){
                set(37+v,b%3==0?0:b%3==1?64:127);
                if(b==4)trigger(v);
                int16_t a[32],r[32];for(int j=0;j<32;j++)a[j]=r[j]=static_cast<int16_t>(((j+b*32)*997)%30001-15000);
                measured=true;filter(v).calcBlockZDF(type+1,a,32);base_ref_filter(v,r);measured=false;
                pcm("B_filter_input_PCM",v,type*128+resonance,b,a,r);
            }
        }
        reset();set(191+v,0);set(43+v,127);trigger(v);
        for(int b=0;b<9;b++){set(37+v,b%3==0?0:b%3==1?64:127);if(b==4)trigger(v);block("B_voice_PCM",v,0,b);}
    }
}
void timingC() {
    for(int v:{3,4,5})for(int fine:{0,1})for(int note:{0,63,127}){
        reset();trigger(v,127,note);async(v);int id=14+(v-3)*2+fine;
        for(int x=0;x<128;x++){
            const auto phase=osc(v).phase,increment=osc(v).phaseInc;
            measured=true;set(id,x);measured=false;BaseState s{};base_ref_state(v,&s);
            check("C immediate freq",equal(osc(v).freq,s.freq[0]));
            check("C phase/phaseInc deferred",phase==osc(v).phase&&increment==osc(v).phaseInc);
            async(v);check("C next tick phaseInc",osc(v).phaseInc==([](int voice){BaseState q{};base_ref_state(voice,&q);return q.increment[0];})(v));
        }
    }
    for(int v:{3,4,5})for(int value:{0,64,127})for(int fine:{0,1}){
        reset();int id=14+(v-3)*2+fine;set(id,value);trigger(v);
        for(int b=0;b<8;b++){
            if(b==1||b==5)set(id,127-value);
            if(b==4)trigger(v,127,74,v==5);
            block("C_live_pitch_PCM",v,value*2+fine,b);stateCheck(v);
        }
        // A write after async changes Hz but not the already computed increment:
        // remaining cached native samples stay identical; next async consumes it.
        async(v);const auto phase=osc(v).phase,increment=osc(v).phaseInc;set(id,value);
        check("C after-async phase continuity",phase==osc(v).phase&&increment==osc(v).phaseInc);
        int16_t a[32],r[32];renderPort(v,a);base_ref_render(v,r);pcm("C_after_async_same_increment",v,value,8,a,r);
        block("C_next_async_consumes_edit",v,value,9);
    }
    for(int v:{3,4,5})for(int fine:{0,1})for(int afterAsync:{0,1}){
        reset();trigger(v);block("C_timing_warmup",v,fine,0);
        if(afterAsync)async(v);
        // Both independent implementations fork from identical unedited state.
        const lxr::Engine prior=engine;base_ref_checkpoint();
        int16_t oldPort[64],oldRef[64],newPort[64],newRef[64];
        if(!afterAsync)async(v);
        renderPort(v,oldPort);base_ref_render(v,oldRef);
        async(v);renderPort(v,oldPort+32);base_ref_render(v,oldRef+32);
        pcm("C_counterfactual_unchanged",v,fine,0,oldPort,oldRef);
        pcm("C_counterfactual_unchanged",v,fine,1,oldPort+32,oldRef+32);
        engine=prior;base_ref_restore_checkpoint();
        const int id=14+(v-3)*2+fine;set(id,fine?127:76);
        if(!afterAsync)async(v);
        renderPort(v,newPort);base_ref_render(v,newRef);
        async(v);renderPort(v,newPort+32);base_ref_render(v,newRef+32);
        pcm("C_counterfactual_edited",v,fine,0,newPort,newRef);
        pcm("C_counterfactual_edited",v,fine,1,newPort+32,newRef+32);
        int firstPort=-1,firstRef=-1;
        for(int j=0;j<64;j++){
            if(firstPort<0&&oldPort[j]!=newPort[j])firstPort=j;
            if(firstRef<0&&oldRef[j]!=newRef[j])firstRef=j;
        }
        check("C first affected sample agrees",firstPort==firstRef&&firstRef>=0);
        check("C cached block is not prematurely retuned",afterAsync?firstRef>=32:firstRef<32);
        std::fprintf(timingMetrics,"%d,%d,%d,%d,%d,%d\n",v,id,afterAsync,firstRef,firstPort,afterAsync?32:0);
    }
}
void envelopeD() {
    for(int v=0;v<3;v++)for(int attack:{0,1,127})for(int vel:{0,127}){
        reset();set(49+v*2,attack);set(50+v*2,0);trigger(v,vel);
        for(int b=0;b<10;b++){
            if(b==3||b==7)trigger(v,vel);
            block("D_attack_trigger_PCM",v,attack*128+vel,b);
            BaseState s{};base_ref_state(v,&s);
            check("D envelope state",eg(v).state==s.envelopeState&&equal(eg(v).value,s.envelope));
            check("D last/new gains",equal(engine.drums[v].lastGain,s.lastGain)&&equal(engine.drums[v].targetGain,s.targetGain));
        }
    }
    // Exercise the exact branch's REPEAT path and negative controls without
    // inventing a new drum repeat parameter. This is matched initial state.
    for(int v=0;v<3;v++)for(int state:{0,1,2,3})for(int attack:{0,1}){
        reset();set(49+v*2,attack);trigger(v);
        eg(v).state=state;eg(v).value=0.5f;eg(v).repeatCnt=2;base_ref_envelope(v,state,0.5f,2);
        for(int b=0;b<5;b++){
            block("D_state_branch_PCM",v,state*128+attack,b);BaseState s{};base_ref_state(v,&s);
            check("D state transitions",eg(v).state==s.envelopeState&&eg(v).repeatCnt==s.repeatCount&&equal(eg(v).value,s.envelope));
            check("D interpolation endpoints",equal(engine.drums[v].lastGain,s.lastGain)&&equal(engine.drums[v].targetGain,s.targetGain));
        }
    }
}
}
void* operator new(std::size_t n){if(measured)++allocations;void* p=std::malloc(n?n:1);if(!p)std::abort();return p;}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
int main(int argc,char**argv) {
    if(argc!=3)return 2;
    metrics=std::fopen(argv[1],"w");timingMetrics=std::fopen(argv[2],"w");if(!metrics||!timingMetrics)return 2;
    std::fprintf(metrics,"category,voice,setting,block,frames,mismatches,first_sample,max_difference,rms_difference\n");
    std::fprintf(timingMetrics,"voice,id,written_after_async,first_reference_changed_sample,first_port_changed_sample,native_block_offset\n");
    if(!engine.loadAssets("data")||!ref_snare_load_assets("data"))return 2;
    FILE* f=std::fopen("data/midi_note_frequencies.bin","rb");if(!f)return 2;
    if(std::fread(originalNotes,sizeof(float),128,f)!=128)return 2;
    std::fclose(f);
    // engine.init() generates its own table. The independent comparison uses
    // the original exported note table on both sides, as explicitly required.
    // Install it after every reset by preserving a test-only reference copy.
    std::memcpy(engine.noteFreq,originalNotes,sizeof(originalNotes));
    mappingA();cutoffB();timingC();envelopeD();
    check("zero parameter/render allocations",allocations==0);std::fclose(metrics);std::fclose(timingMetrics);
    std::printf("SUMMARY pass=%d fail=%d pcmCases=%d samples=%lld mismatches=%lld allocations=%d\n",passed,failed,pcmCases,samples,mismatches,allocations);
    return failed?1:0;
}
