// Exact-DLL production controls and state-loader acceptance. No Engine setters.
#include "../vendor/iPlug2/Examples/LXR/LXR.h"
#define main ExistingP12cStateSuiteMain
#include "P12cVST3StateTest.cpp"
#undef main
#include <limits>

struct P16F3VST3CertificationProbe
{
    static LXR& plugin(HostInstance& h)
    { return *static_cast<LXR*>(static_cast<iplug::IPlugVST3*>(static_cast<SingleComponentEffect*>(h.component))); }
    static const lxr::sequencer::TriggerScheduler& seq(HostInstance& h)
    { return plugin(h).mSequencer; }
    static bool hold(HostInstance& h,lxr::RoutingStateAuthority::Guard& guard)
    {return plugin(h).mRoutingState.tryAcquire(guard);}
    static void release(HostInstance& h,lxr::RoutingStateAuthority::Guard& guard)
    {plugin(h).mRoutingState.release(guard);}
    static void idle(HostInstance& h){static_cast<iplug::IPlugAPIBase*>(&plugin(h))->OnIdle();}
    static void describe(HostInstance& h,const char* label){const auto& p=plugin(h);std::printf("CONTROL %s rate=%u logical=%u audio=%u applied=%u ticket=%llu cutoff=%llu rolls=%llu readback=%.17g\n",label,p.mRollControls.rate,p.mRollControls.tracks,p.mAudioRollControls.tracks,p.mAppliedRollMask,static_cast<unsigned long long>(p.mRollControls.pressTicket),static_cast<unsigned long long>(p.mRollReleaseCutoff.load()),static_cast<unsigned long long>(p.mSequencer.diagnostics().emittedRollEvents),h.controller->getParamNormalized(244));}
};
namespace {
using namespace lxr::sequencer;
std::vector<uint8_t> project(HostInstance& h, bool ordinary = false)
{
    auto saved = h.saveState(); lxr::state::ProjectCandidate c;
    require("decode production state", lxr::state::DecodeProject(saved.data(), saved.size(), c, true));
    c.bank = PatternBank{}; c.transport.enabled = 1; c.transport.clock = 1;
    for (unsigned p=0; p<8; ++p) c.bank.patterns[p].nextPattern = static_cast<uint8_t>(p);
    if (ordinary) {
        c.bank.patterns[0].mainGates[0] = 0xffff;
        for (auto& s : c.bank.patterns[0].steps[0]) { SetStepActive(s,true); s.note=63; s.probability=127; }
    }
    lxr::state::ProjectBytes bytes; std::size_t n=0;
    require("encode production state", lxr::state::EncodeProject(c,bytes,n,true));
    return {bytes.begin(),bytes.begin()+n};
}
void point(ParameterChanges& c, ParamID id, double v)
{
    int32 qi=0,pi=0; auto* q=c.addParameterData(id,qi);
    require("create automation point", q && q->addPoint(0,v,pi)==kResultOk);
}
struct Capture { uint64_t events=0,rolls=0,steps=0; double peak=0; bool ordered=true; uint64_t hash=1469598103934665603ULL; };
Capture process(HostInstance& h, int start, int frames, bool playing=true,
                IParameterChanges* changes=nullptr)
{
    Capture cap; std::array<double,1> l{},r{}; double* channels[]{l.data(),r.data()};
    AudioBusBuffers output{}; output.numChannels=2; output.channelBuffers64=channels;
    uint64_t last=UINT64_MAX;
    auto nativeBlocks=P16F3VST3CertificationProbe::seq(h).diagnostics().processedNativeBlocks;
    bool ok=true;
    for (int i=0;i<frames;++i) {
        ProcessContext ctx{}; ctx.sampleRate=48000; ctx.tempo=120;
        ctx.state=ProcessContext::kTempoValid|ProcessContext::kProjectTimeMusicValid|(playing?ProcessContext::kPlaying:0);
        ctx.projectTimeSamples=start+(playing?i:0); ctx.projectTimeMusic=ctx.projectTimeSamples/24000.0;
        ProcessData d{}; d.processMode=kOffline; d.symbolicSampleSize=kSample64;
        d.numSamples=1; d.numOutputs=1; d.outputs=&output; d.processContext=&ctx;
        if(i==0)d.inputParameterChanges=changes;
        ok &= h.processor->process(d)==kResultOk;
        for(double x:{l[0],r[0]}) { uint64_t bits; std::memcpy(&bits,&x,8);
            cap.peak=std::max(cap.peak,std::abs(x));
            for(unsigned b=0;b<8;++b){cap.hash^=(bits>>(b*8))&255;cap.hash*=1099511628211ULL;} }
        const auto blocks=P16F3VST3CertificationProbe::seq(h).diagnostics().processedNativeBlocks;
        if(blocks==nativeBlocks)continue;
        nativeBlocks=blocks;
        const auto& batch=P16F3VST3CertificationProbe::seq(h).lastBatch();
        for(const auto& e:batch) if(last==UINT64_MAX||e.stableOrder>last) {
            ++cap.events; cap.steps+=e.kind==TriggerKind::Step; cap.rolls+=e.kind!=TriggerKind::Step;
            last=e.stableOrder;
        }
    }
    check("production callbacks succeed",ok); return cap;
}
void controls(HostInstance& h,unsigned rate,unsigned mask)
{
    require("set roll rate",h.controller->setParamNormalized(243,rate/13.0)==kResultOk);
    require("set held track mask",h.controller->setParamNormalized(244,mask/127.0)==kResultOk);
}
}
int main(int argc,char**argv)
{
    if(argc!=2)return 2;
    auto module=LoadLibraryA(argv[1]); require("load exact module",module!=nullptr);
    auto init=reinterpret_cast<bool(*)()>(GetProcAddress(module,"InitDll"));
    auto end=reinterpret_cast<bool(*)()>(GetProcAddress(module,"ExitDll"));
    auto factoryFn=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(module,"GetPluginFactory"));
    require("module exports",init&&end&&factoryFn&&init());
    auto* factory=factoryFn(); PClassInfo info{}; bool found=false;
    for(int i=0;i<factory->countClasses();++i)if(factory->getClassInfo(i,&info)==kResultOk&&std::strcmp(info.category,kVstAudioEffectClass)==0){found=true;break;}
    require("audio class",found);
    {
        HostInstance h(factory,info.cid); h.setup(48000);
        ParameterInfo rate{},mask{};
        require("roll parameter metadata",h.controller->getParameterInfo(243,rate)==kResultOk&&h.controller->getParameterInfo(244,mask)==kResultOk);
        check("stable IDs and steps",rate.id==243&&mask.id==244&&rate.stepCount==13&&mask.stepCount==127);
        check("roll defaults",rate.defaultNormalizedValue==8.0/13.0&&mask.defaultNormalizedValue==0);
        auto empty=project(h); const unsigned expected[]{1,1,2,3,4,3,8,12,16,22,32,43,64,128};
        for(unsigned selector=0;selector<14;++selector){
            require("restore empty bank",h.loadState(empty)); controls(h,selector,1);
            const auto cap=process(h,0,96000);
            std::printf("ROLL selector=%u expected=%u observed=%llu peak=%.17g\n",selector,expected[selector],static_cast<unsigned long long>(cap.rolls),cap.peak);
            check("fourteen source-defined roll counts",cap.rolls==expected[selector]&&cap.steps==0);
            check("manual roll produces PCM",cap.peak>0);
            controls(h,selector,0); const auto before=P16F3VST3CertificationProbe::seq(h).diagnostics().emittedRollEvents;
            process(h,96000,4800);
            check("release stops roll-specific events",P16F3VST3CertificationProbe::seq(h).diagnostics().emittedRollEvents==before);
        }
        require("restore oneshot",h.loadState(empty));controls(h,0,1);process(h,0,1000);
        auto count=P16F3VST3CertificationProbe::seq(h).diagnostics().emittedRollEvents;
        controls(h,0,1);process(h,1000,1000);
        check("held one-shot does not retrigger",P16F3VST3CertificationProbe::seq(h).diagnostics().emittedRollEvents==count);
        controls(h,0,0);process(h,2000,64);controls(h,0,1);process(h,2064,64);
        check("new one-shot press retriggers",P16F3VST3CertificationProbe::seq(h).diagnostics().emittedRollEvents==count+1);
        require("restore automation case",h.loadState(empty)); ParameterChanges changes(2);point(changes,243,1);point(changes,244,1.0/127);
        auto automated=process(h,0,4800,true,&changes);check("host automation activates roll",automated.rolls>0);
        process(h,4800,64,false);process(h,4800,64,false);
        check("host stop projects released mask",h.controller->getParamNormalized(244)==0);
        count=P16F3VST3CertificationProbe::seq(h).diagnostics().emittedRollEvents;
        process(h,4800,4800);check("resume has no stale held roll",P16F3VST3CertificationProbe::seq(h).diagnostics().emittedRollEvents==count);
        require("restore stop contention case",h.loadState(empty));process(h,0,64);
        lxr::RoutingStateAuthority::Guard guard;
        require("hold explicit common publication barrier",P16F3VST3CertificationProbe::hold(h,guard));
        controls(h,13,1);process(h,64,64,false);
        P16F3VST3CertificationProbe::describe(h,"stop-gate-held");
        P16F3VST3CertificationProbe::release(h,guard);
        process(h,64,64,false);count=P16F3VST3CertificationProbe::seq(h).diagnostics().emittedRollEvents;
        P16F3VST3CertificationProbe::describe(h,"stop-gate-released");
        process(h,64,4800);P16F3VST3CertificationProbe::idle(h);check("older queued press cannot resurrect across stop barrier",P16F3VST3CertificationProbe::seq(h).diagnostics().emittedRollEvents==count&&h.controller->getParamNormalized(244)==0);
        process(h,4864,64,false);controls(h,0,1);process(h,4864,64,false);
        P16F3VST3CertificationProbe::describe(h,"new-stopped-oneshot");
        check("new stopped one-shot press accepted",P16F3VST3CertificationProbe::seq(h).diagnostics().emittedRollEvents==count+1);
        controls(h,0,0);process(h,4864,64,false);
        require("restore stop with rewind case",h.loadState(empty));controls(h,13,1);process(h,0,4800);
        process(h,0,64,false);P16F3VST3CertificationProbe::idle(h);
        count=P16F3VST3CertificationProbe::seq(h).diagnostics().emittedRollEvents;
        check("stop with position rewind releases controller mask",h.controller->getParamNormalized(244)==0);
        process(h,0,4800);check("resume after stopped seek cannot revive held roll",P16F3VST3CertificationProbe::seq(h).diagnostics().emittedRollEvents==count);
        controls(h,13,1);process(h,4928,64);
        auto saved=h.saveState();check("wire format frozen",saved.size()==51065);
        require("restore releases transient controls",h.loadState(saved));process(h,0,64);
        check("restore roll defaults",h.controller->getParamNormalized(243)==8.0/13.0&&h.controller->getParamNormalized(244)==0);
        check("invalid direct controls reject",h.controller->setParamNormalized(243,-.1)==kResultFalse&&h.controller->setParamNormalized(244,1.1)==kResultFalse&&h.controller->setParamNormalized(243,std::numeric_limits<double>::quiet_NaN())==kResultFalse);
        auto regular=project(h,true);require("restore sequenced roll case",h.loadState(regular));controls(h,13,1);
        auto mixed=process(h,0,4800);check("normal steps and rolls coexist",mixed.steps>0&&mixed.rolls>0);
        controls(h,13,0);mixed=process(h,4800,4800);check("normal sequence continues after release",mixed.steps>0&&mixed.rolls==0);
        require("restore deterministic case",h.loadState(empty));controls(h,9,96);auto hats=process(h,0,4800);
        require("repeat deterministic case",h.loadState(empty));controls(h,9,96);auto hats2=process(h,0,4800);
        check("shared hat rolls deterministic",hats.rolls>0&&hats.hash==hats2.hash);
        HostInstance other(factory,info.cid);other.setup(48000);auto otherEmpty=project(other);require("second instance restore",other.loadState(otherEmpty));
        check("second instance silent and released",process(other,0,4800).peak==0&&other.controller->getParamNormalized(244)==0);
        check("no unsolicited host gestures",h.handler.edits==0&&other.handler.edits==0);
    }
    factory->release();end();FreeLibrary(module);
    std::printf("SUMMARY pass=%d fail=%d\n",passed,failed);return failed?1:0;
}
