#include "../dsp/Engine.h"
#include "../dsp/MidiParser.h"
#include "../dsp/ParameterArray.h"
#include "../dsp/TriggerScheduler.h"
#include "reference/sequencer/original_parameter_lock_adapter.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <thread>

namespace
{
using namespace lxr;
using namespace lxr::sequencer;
namespace mc = lxr::modulation_contract;

std::atomic<std::uint64_t> allocations {0};
std::atomic<bool> measured {false};
std::uint64_t passed=0,failed=0,pcmCases=0,pcmDifferences=0;
int firstPcmDifference=-1;

void check(bool value,const char* label)
{
    if(value) ++passed;
    else {++failed;std::fprintf(stderr,"FAIL: %s\n",label);}
}
template<class A,class B> void equal(A a,B b,const char* label)
{check(a==static_cast<A>(b),label);}
bool close(float a,float b,float epsilon=1.e-6f)
{return std::fabs(a-b)<=epsilon;}

AutomationSlotCommand command(std::uint8_t selector,std::uint8_t value)
{
    const auto decoded=DecodeAutomationSlot(selector,value);
    check(static_cast<bool>(decoded),"decode test command");
    return decoded.command;
}

void initialize(Engine& engine,ParameterArray& parameters)
{
    parameters.init();
    parameters.applyAllToEngine(engine);
}

PatternBank blankBank()
{
    PatternBank bank=MakeDefaultPatternBank();
    for(auto& pattern:bank.patterns) {
        for(auto& gate:pattern.mainGates) gate=0u;
        for(auto& track:pattern.steps)
            for(auto& step:track) SetStepActive(step,false);
    }
    return bank;
}

StepData lockedStep(std::uint8_t p1,std::uint8_t v1,
                    std::uint8_t p2=kNoAutomation,std::uint8_t v2=0u)
{
    StepData step=MakeDefaultStep(false);
    SetStepActive(step,true);SetStepVelocity(step,100u);
    step.probability=127u;step.note=63u;
    step.parameter1=p1;step.value1=v1;
    step.parameter2=p2;step.value2=v2;
    return step;
}

void testIdentity()
{
    auto d=DecodeAutomationSlot(0u,12u);
    check(d.status==AutomationValidationStatus::Clear,"front off clears");
    d=DecodeAutomationSlot(0xffu,12u);
    check(d.status==AutomationValidationStatus::Clear,"sentinel clears");
    d=DecodeAutomationSlot(1u,12u);
    check(d.status==AutomationValidationStatus::ModWheelNoOp,"selector one rejected");
    d=DecodeAutomationSlot(2u,127u);
    check(d&&d.command.target.value()==1u,"lower selector subtracts one");
    d=DecodeAutomationSlot(127u,0u);
    check(d&&d.command.target.value()==126u,"lower selector upper boundary");
    d=DecodeAutomationSlot(128u,0u);
    check(d&&d.command.target.value()==128u,"upper selector direct mapping");
    d=DecodeAutomationSlot(149u,0u);
    check(!d,"mutable velocity destination rejected");
    d=DecodeAutomationSlot(215u,5u);
    check(d&&d.command.target.value()==215u,"internal direct output target represented");
    d=DecodeAutomationSlot(221u,0u);
    check(!d,"unimplemented MIDI note target rejected");
    d=DecodeAutomationSlot(227u,0u);
    check(!d,"runtime-unimplemented upper target rejected");
    d=DecodeAutomationSlot(228u,0u);
    check(!d,"host routing identity rejected");
    d=DecodeAutomationSlot(89u,128u);
    check(d.status==AutomationValidationStatus::InvalidValue,"non-seven-bit value rejected");
    check(!mc::HostParameterId::fromRaw(127u).valid() ||
          DecodeAutomationSlot(128u,1u).command.target.value()!=127u,
          "parameter 127 has no stored selector");
}

void testCapacity()
{
    AutomationEventBatch batch;
    for(std::uint8_t i=0;i<15u;++i) {
        AutomationEvent event {};event.track=static_cast<std::uint8_t>(i/2u);event.slot=i%2u;
        check(batch.push(event)==(i<14u),"automation reject-newest capacity");
    }
    equal(batch.size(),14u,"automation capacity fourteen");
    equal(batch.rejected(),1u,"automation overflow diagnosed");
}

void testOriginalNodeDifferential()
{
    Engine engine;ParameterArray parameters;initialize(engine,parameters);
    parameters.set(88u,100u,engine);
    std::array<std::uint8_t,P16D_ORIGINAL_PARAMETERS> base {};
    for(std::size_t i=0;i<base.size();++i) base[i]=parameters.get(static_cast<std::uint8_t>(i));
    p16d_original_init(base.data());

    auto a=command(89u,30u),b=command(89u,60u);
    check(engine.applySequencerAutomationPair(0u,&a,&b),"apply duplicate pair");
    p16d_original_pair(0u,89u,30u,89u,60u);
    equal(p16d_original_effective(88u),60u,"original duplicate slot2 wins");
    check(close(engine.drums[0].vol,60.f/127.f),"engine duplicate slot2 wins");
    equal(p16d_original_write_count(),2u,"first pair writes two values");

    a=command(89u,70u);b=command(0xffu,0u);
    check(engine.applySequencerAutomationPair(0u,&a,&b),"change and clear pair");
    p16d_original_pair(0u,89u,70u,0xffu,0u);
    equal(p16d_original_write_count(),3u,"restore restore value ordering");
    equal(p16d_original_write(0).value,100u,"slot1 old destination restores base");
    equal(p16d_original_write(1).value,100u,"slot2 old destination restores base");
    equal(p16d_original_write(2).value,70u,"slot1 value follows both restores");
    check(close(engine.drums[0].vol,70.f/127.f),"engine matches restore/write order");

    parameters.set(88u,110u,engine);
    p16d_original_update_base(88u,110u);
    check(close(engine.drums[0].vol,110.f/127.f),"new base wins while lock active");
    equal(parameters.get(88u),110u,"base byte changed only by base producer");
    a=command(0xffu,0u);
    check(engine.applySequencerAutomationPair(0u,&a,nullptr),"clear uses newest base");
    p16d_original_pair(0u,0xffu,0u,0xffu,0u);
    equal(p16d_original_effective(88u),110u,"original release uses newest base");
    check(close(engine.drums[0].vol,110.f/127.f),"engine release uses newest base");

    engine.init();
    check(!engine.sequencerAutomationDestination(0u,0u).valid(),"engine reset clears slot");
    parameters.applyAllToEngine(engine);
    check(!engine.sequencerAutomationDestination(0u,0u).valid()&&
          close(engine.drums[0].vol,110.f/127.f),
          "complete restore replays base without transient lock");
}

void testMidiAndInstances()
{
    Engine first,second;ParameterArray firstParameters,secondParameters;
    initialize(first,firstParameters);initialize(second,secondParameters);
    firstParameters.set(88u,90u,first);secondParameters.set(88u,90u,second);
    MidiParser parser;
    parser.processEvent({0xb0u,0u,88u,105u},first,firstParameters);
    equal(firstParameters.get(88u),105u,"MIDI updates authoritative base before lock");
    auto lock=command(89u,25u);
    check(first.applySequencerAutomationPair(0u,&lock,nullptr),"same-boundary MIDI then lock");
    check(close(first.drums[0].vol,25.f/127.f),"sequencer lock follows MIDI base update");
    check(close(second.drums[0].vol,90.f/127.f),"other Engine instance remains isolated");
    auto clear=command(kNoAutomation,0u);
    check(first.applySequencerAutomationPair(0u,&clear,nullptr),"release after MIDI base update");
    check(close(first.drums[0].vol,105.f/127.f),"release restores MIDI-updated base");
}

void testSchedulerOrderingAndLifetime()
{
    Engine engine;ParameterArray parameters;initialize(engine,parameters);
    parameters.set(88u,100u,engine);
    TriggerScheduler scheduler(1u);
    PatternBank bank=blankBank();
    for(std::uint8_t track=0;track<7u;++track) {
        bank.patterns[0].mainGates[track]=1u;
        bank.patterns[0].steps[track][0]=lockedStep(89u,static_cast<std::uint8_t>(20u+track));
    }
    check(scheduler.patternAuthority().tryPublish(bank)==Status::Ok,"publish multitrack locks");
    check(scheduler.start()==Status::Ok,"start multitrack locks");
    check(scheduler.processNativeBlock(0u,engine)==Status::Ok,"process multitrack locks");
    equal(scheduler.lastBatch().size(),7u,"seven triggers at boundary");
    equal(scheduler.lastAutomationBatch().size(),14u,"two operations per accepted track");
    for(std::size_t i=0;i<14u;++i) {
        equal(scheduler.lastAutomationBatch()[i].track,i/2u,"automation track ordering");
        equal(scheduler.lastAutomationBatch()[i].slot,i%2u,"automation slot ordering");
    }
    check(close(engine.drums[0].vol,26.f/127.f),"later track wins same target");
    equal(parameters.get(88u),100u,"sequencer leaves patch base unchanged");

    TriggerScheduler mixed(1u);PatternBank mixedBank=blankBank();
    mixedBank.patterns[0].mainGates[0]=1u;
    mixedBank.patterns[0].steps[0][0]=lockedStep(1u,12u,89u,77u);
    check(mixed.patternAuthority().tryPublish(mixedBank)==Status::Ok,"publish mixed-validity locks");
    check(mixed.start()==Status::Ok,"start mixed-validity locks");
    (void)mixed.processNativeBlock(0u,engine);
    equal(mixed.lastAutomationBatch().size(),1u,"invalid slot rejected while valid slot retained");
    equal(mixed.lastAutomationBatch()[0].slot,1u,"valid slot2 retains identity");
    equal(mixed.diagnostics().rejectedAutomationEvents,1u,"invalid slot diagnosed");
    check(close(engine.drums[0].vol,77.f/127.f),"valid slot applies beside invalid slot");

    TriggerScheduler invalid(1u);PatternBank invalidBank=blankBank();
    invalidBank.patterns[0].mainGates[0]=1u;
    invalidBank.patterns[0].steps[0][0]=lockedStep(1u,12u,227u,44u);
    check(invalid.patternAuthority().tryPublish(invalidBank)==Status::Ok,"publish invalid locks");
    check(invalid.start()==Status::Ok,"start invalid locks");
    (void)invalid.processNativeBlock(0u,engine);
    equal(invalid.lastAutomationBatch().size(),0u,"both invalid slots emit no automation");
    equal(invalid.lastBatch().size(),1u,"invalid automation does not suppress trigger");
    check(close(engine.drums[0].vol,77.f/127.f),"invalid automation leaves effective state unchanged");

    TriggerScheduler lifetime(1u);PatternBank life=blankBank();
    life.patterns[0].mainGates[0]=1u;
    life.patterns[0].steps[0][0]=lockedStep(89u,40u);
    life.patterns[0].steps[0][1]=lockedStep(89u,90u);
    life.patterns[0].steps[0][1].probability=0u;
    check(lifetime.patternAuthority().tryPublish(life)==Status::Ok,"publish lifetime bank");
    check(lifetime.start()==Status::Ok,"start lifetime bank");
    check(lifetime.processNativeBlock(0u,engine)==Status::Ok,"apply persistent lock");
    check(close(engine.drums[0].vol,40.f/127.f),"initial lock effective");
    std::uint64_t prior=lifetime.diagnostics().duePhysicalPositions;
    for(std::uint64_t frame=32u;frame<2048u && lifetime.diagnostics().duePhysicalPositions==prior;++frame)
        if((frame%32u)==0u) (void)lifetime.processNativeBlock(frame,engine);
    equal(lifetime.lastBatch().size(),0u,"probability rejection emits no trigger");
    check(close(engine.drums[0].vol,40.f/127.f),"probability rejection preserves lock");
    lifetime.stop();
    check(close(engine.drums[0].vol,40.f/127.f),"stop preserves firmware lock state");
    check(lifetime.start()==Status::Ok,"restart scheduler");
    check(close(engine.drums[0].vol,40.f/127.f),"restart preserves firmware lock state");

    TriggerScheduler gated(1u);PatternBank gatedBank=blankBank();
    gatedBank.patterns[0].mainGates[0]=1u;
    gatedBank.patterns[0].steps[0][0]=lockedStep(89u,50u);
    gatedBank.patterns[0].steps[0][1]=lockedStep(89u,90u);
    check(gated.patternAuthority().tryPublish(gatedBank)==Status::Ok,"publish gate bank");
    check(gated.start()==Status::Ok,"start gate bank");
    (void)gated.processNativeBlock(0u,engine);
    gatedBank.patterns[0].mainGates[0]=0u;
    check(gated.patternAuthority().tryPublish(gatedBank)==Status::Ok,"publish gate close");
    prior=gated.diagnostics().duePhysicalPositions;
    for(std::uint64_t frame=32u;frame<2048u && gated.diagnostics().duePhysicalPositions==prior;frame+=32u)
        (void)gated.processNativeBlock(frame,engine);
    equal(gated.lastBatch().size(),0u,"main gate suppresses automation and trigger");
    check(close(engine.drums[0].vol,50.f/127.f),"main gate suppression preserves lock");
}

void testLayering()
{
    Engine engine;ParameterArray parameters;initialize(engine,parameters);
    parameters.set(88u,100u,engine);parameters.set(143u,127u,engine);
    parameters.set(121u,64u,engine);parameters.set(155u,2u,engine);
    const auto target=mc::targetForHost(mc::HostParameterId::fromRaw(88u));
    check(static_cast<bool>(target),"volume target exists");
    check(engine.configureVelocityDestination({mc::SourceKind::Velocity,mc::Voice::Drum1},target.value)==VelocityRouteStatus::Ok,"velocity route configured");
    check(engine.configureLfoDestination({mc::SourceKind::Lfo,mc::Voice::Drum1},target.value)==LfoRouteStatus::Ok,"lfo route configured");
    const auto velocityRouteBefore=*engine.velocityRoute(mc::Voice::Drum1);
    const auto lfoRouteBefore=*engine.lfoRoute(mc::Voice::Drum1);
    auto lock=command(89u,80u);
    check(engine.applySequencerAutomationPair(0u,&lock,nullptr),"layered lock accepted");
    check(engine.velocityRoute(mc::Voice::Drum1)->target==velocityRouteBefore.target&&
          engine.velocityRoute(mc::Voice::Drum1)->destination==velocityRouteBefore.destination&&
          engine.velocityRoute(mc::Voice::Drum1)->enabled==velocityRouteBefore.enabled,
          "lock leaves velocity routing identity unchanged");
    check(engine.lfoRoute(mc::Voice::Drum1)->target==lfoRouteBefore.target&&
          engine.lfoRoute(mc::Voice::Drum1)->destination==lfoRouteBefore.destination&&
          engine.lfoRoute(mc::Voice::Drum1)->enabled==lfoRouteBefore.enabled,
          "lock leaves LFO routing identity unchanged");
    engine.triggerDrum(0u,64u,63u);
    engine.beginModulationTick();
    const float velocity=64.f/127.f;
    const float lfoDepth=engine.nativeLfo(0u)->amount;
    const float lfoValue=engine.nativeLfo(0u)->output;
    const float afterVelocity=(80.f/127.f)*velocity;
    const float expected=afterVelocity*lfoDepth*lfoValue+(1.f-lfoDepth)*afterVelocity;
    check(close(engine.drums[0].vol,expected,2.e-6f),"base overlay then velocity then lfo");
    equal(parameters.get(88u),100u,"layering leaves base byte unchanged");
}

struct Pcm {std::array<std::int16_t,32> l{},r{},l2{},r2{};};
Pcm render(Engine& engine)
{
    Pcm pcm;engine.processBlock(pcm.l.data(),pcm.r.data(),pcm.l2.data(),pcm.r2.data());return pcm;
}

void testPcmAndAllocations()
{
    Engine source;ParameterArray baseParameters;initialize(source,baseParameters);
    check(source.loadAssets("data"),"load PCM assets");
    baseParameters.set(49u,0u,source);baseParameters.set(50u,64u,source);
    baseParameters.set(37u,100u,source);baseParameters.set(88u,110u,source);
    auto scheduled=[&](std::uint8_t p1,std::uint8_t v1,
                       std::uint8_t p2=kNoAutomation,std::uint8_t v2=0u) {
        Engine engine=source;TriggerScheduler scheduler(1u);PatternBank bank=blankBank();
        bank.patterns[0].mainGates[0]=1u;
        bank.patterns[0].steps[0][0]=lockedStep(p1,v1,p2,v2);
        check(scheduler.patternAuthority().tryPublish(bank)==Status::Ok,"publish PCM case");
        check(scheduler.start()==Status::Ok,"start PCM case");
        (void)scheduler.processNativeBlock(0u,engine);
        return render(engine);
    };
    measured.store(true,std::memory_order_relaxed);
    const Pcm baseline=scheduled(kNoAutomation,0u);
    const Pcm pitch=scheduled(9u,30u);
    const Pcm cutoff=scheduled(38u,20u);
    const Pcm volume=scheduled(89u,20u);
    const Pcm twoSlots=scheduled(9u,30u,89u,20u);
    const Pcm duplicate=scheduled(89u,10u,89u,70u);
    const Pcm slot2Only=scheduled(kNoAutomation,0u,89u,70u);
    const Pcm deterministic=scheduled(89u,20u);
    measured.store(false,std::memory_order_relaxed);
    auto differs=[&](const Pcm& a,const Pcm& b) {
        bool any=false;for(int i=0;i<32;++i)
            if(a.l[i]!=b.l[i]||a.r[i]!=b.r[i]||a.l2[i]!=b.l2[i]||a.r2[i]!=b.r2[i]) {
                any=true;++pcmDifferences;if(firstPcmDifference<0) firstPcmDifference=i;
            }
        ++pcmCases;return any;
    };
    check(differs(baseline,pitch),"drum pitch lock changes native PCM");
    check(differs(baseline,cutoff),"filter cutoff lock changes native PCM");
    check(differs(baseline,volume),"volume lock changes native PCM");
    check(differs(baseline,twoSlots),"two separate lock targets change native PCM");
    check(duplicate.l==slot2Only.l&&duplicate.r==slot2Only.r&&
          duplicate.l2==slot2Only.l2&&duplicate.r2==slot2Only.r2,
          "duplicate target PCM equals slot2 value");
    check(volume.l==deterministic.l&&volume.r==deterministic.r&&
          volume.l2==deterministic.l2&&volume.r2==deterministic.r2,
          "controlled reset rerenders deterministically");
    check(pcmDifferences>0u,"audible volume lock changes native PCM");
    check(firstPcmDifference>=0,"first affected PCM sample recorded");
    equal(baseParameters.get(88u),110u,"PCM lock leaves base parameter unchanged");
    equal(allocations.load(),0u,"scheduler overlay and render allocate zero");
}

void testConcurrentPublication()
{
    Engine engine;ParameterArray parameters;initialize(engine,parameters);
    TriggerScheduler scheduler(0u);PatternBank a=blankBank(),b=blankBank();
    for(auto* bank:{&a,&b}) {
        bank->patterns[0].mainGates[0]=0xffffu;
        for(auto& step:bank->patterns[0].steps[0]) step=lockedStep(89u,10u,90u,20u);
    }
    for(auto& step:b.patterns[0].steps[0]) {step.value1=100u;step.value2=110u;}
    check(scheduler.patternAuthority().tryPublish(a)==Status::Ok,"publish concurrency initial");
    check(scheduler.start()==Status::Ok,"start concurrency scheduler");
    std::atomic<bool> start{false},done{false};
    std::thread writer([&]{while(!start.load(std::memory_order_acquire)){}
        for(unsigned i=0;i<200u;++i) (void)scheduler.patternAuthority().tryPublish((i&1u)?a:b);
        done.store(true,std::memory_order_release);});
    start.store(true,std::memory_order_release);
    std::uint64_t frame=0u;
    while(!done.load(std::memory_order_acquire)||frame<20000u) {
        (void)scheduler.processNativeBlock(frame,engine,1u);frame+=32u;
        const auto& batch=scheduler.lastAutomationBatch();
        if(batch.size()>=2u) {
            const auto x=batch[0].command.value.value(),y=batch[1].command.value.value();
            check((x==10u&&y==20u)||(x==100u&&y==110u),"coherent automation snapshot pair");
        }
        if(frame>40000u) break;
    }
    writer.join();
    check(scheduler.diagnostics().snapshotBusyBoundaries<1000u,"bounded Busy publication handling");
}
} // namespace

void* operator new(std::size_t size)
{
    if(measured.load(std::memory_order_relaxed)) allocations.fetch_add(1u);
    void* value=std::malloc(size?size:1u);if(!value) std::abort();return value;
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* value) noexcept{std::free(value);}
void operator delete[](void* value) noexcept{std::free(value);}
void operator delete(void* value,std::size_t) noexcept{std::free(value);}
void operator delete[](void* value,std::size_t) noexcept{std::free(value);}

int main()
{
    testIdentity();testCapacity();testOriginalNodeDifferential();
    testMidiAndInstances();testSchedulerOrderingAndLifetime();testLayering();testPcmAndAllocations();
    testConcurrentPublication();
    std::printf("{\"pass\":%llu,\"fail\":%llu,\"pcm_cases\":%llu,"
                "\"pcm_differences\":%llu,\"first_pcm_difference\":%d,"
                "\"allocations\":%llu}\n",
                static_cast<unsigned long long>(passed),
                static_cast<unsigned long long>(failed),
                static_cast<unsigned long long>(pcmCases),
                static_cast<unsigned long long>(pcmDifferences),firstPcmDifference,
                static_cast<unsigned long long>(allocations.load()));
    return failed?1:0;
}
