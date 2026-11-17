// P17-A audit harness only. No production preset codec or loading feature.
// Derived source contracts: Sonic Potions LXR 0.37; see LICENSE-LXR.txt.
#include "reference/presets/original_preset_adapter.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
unsigned passed=0,failed=0;
void check(bool ok,const char* name) {
    if(ok)++passed;else{++failed;std::printf("FAIL %s\n",name);}
}
std::vector<uint8_t> read(const std::string& p) {
    auto* f=std::fopen(p.c_str(),"rb");if(!f){check(false,"open independent fixture");return {};}
    std::vector<uint8_t> bytes;int c;while((c=std::fgetc(f))!=EOF)bytes.push_back(static_cast<uint8_t>(c));
    std::fclose(f);return bytes;
}
// Deliberately independent TEST-ONLY shape/range examiner. This is not a new
// hardware reader or a proposed production codec. Original permissive read
// semantics are tested separately below, including short-sound acceptance.
bool strictShape(const uint8_t* p,size_t n,unsigned kind) {
    static constexpr size_t sizes[]{236,50369,50946,50946,22};
    if(kind>4||!p||n!=sizes[kind])return false;
    if((kind==2||kind==3)&&p[8]!=2)return false;
    if(kind==1||kind==2||kind==3) {
        const size_t start=kind==1?8:585;
        // Steps occupy 7168 explicit seven-byte records; do not deserialize
        // PatternSet or the compiler-dependent LengthRotate union.
        for(size_t i=0;i<7168;++i) {
            const uint8_t* s=p+start+i*7;
            if(s[1]>127||s[2]>127||s[4]>127||s[6]>127)return false;
            if((s[3]>227&&s[3]!=255)||(s[5]>227&&s[5]!=255))return false;
        }
        const size_t chain=start+50176+112;
        for(size_t i=0;i<8;++i)if(p[chain+i*2]>14)return false;
        if(p[chain+16]>127)return false;
        for(size_t i=0;i<56;++i)if(p[chain+17+i]>15)return false;
    }
    return true;
}
void layout(const std::string& dir) {
    P17OriginalContract c{};p17_original_contract(&c);
    check(c.sound_count==228,"original sound boundary228");
    check(c.globals_begin==251&&c.parameter_count==273&&c.globals_count==22,"compiled global boundaries251..272");
    check(c.step_size==7,"unchanged AVR StepData seven byte fields on host");
    for(unsigned i=0;i<7;++i)check(c.step_offsets[i]==i,"each original StepData field offset");
    check(c.tracks==7&&c.patterns==8&&c.positions==128,"original pattern dimensions");
    check(c.version==2,"original compound writer version2");
    check(c.modulation_targets==205,"actual original modTargets count205");
    check(p17_original_target_parameter(0)==0,"original off target index");
    check(p17_original_target_parameter(204)==226,"last full-width target is MIDI_NOTE6, not open-hat override");
    for(unsigned i=0;i<228;++i)check(p17_original_parameter_enum(i)==i,"all228 original parameter enum ordinals");
    std::array<uint8_t,273> defaults{};defaults.fill(255);p17_original_defaults(defaults.data(),defaults.size());
    for(unsigned i=0;i<228;++i)check(defaults[i]==0,"original menu_init zero sound fallback, not factory kit");
    check(defaults[228]==8&&defaults[229]==0,"original roll and morph runtime defaults");
    check(defaults[234]==16&&defaults[235]==16&&defaults[236]==0,"source Euclidean menu defaults");
    check(defaults[251]==120,"source internal BPM120");
    auto parameters=read(dir+"/parameters.bin");check(parameters.size()==273,"independent parameter fixture273");
    const uint8_t name[8]{'A','U','D','I','T','0','3','7'};
    const char* files[]{"sound.snd","pattern.pat","all.all","performance.prf","globals.cfg"};
    const char* filenames[]{"p007.snd","p007.pat","p007.all","p007.prf","glo.cfg"};
    const size_t sizes[]{236,50369,50946,50946,22};
    std::array<uint8_t,65536> produced{};
    for(unsigned kind=0;kind<5;++kind) {
        char filename[16]{};
        auto n=p17_original_write(kind,parameters.data(),parameters.size(),name,produced.data(),produced.size(),filename);
        check(n==sizes[kind],"actual unchanged original writer exact size");
        check(std::strcmp(filename,filenames[kind])==0,"actual original filename extension; prf not perf");
        auto expected=read(dir+"/"+files[kind]);check(expected.size()==n,"independent Python fixture size");
        if(expected.size()!=n)continue;
        for(size_t i=0;i<n;++i)check(produced[i]==expected[i],"original-C byte matches independent field encoding");
        check(strictShape(produced.data(),n,kind),"writer fixture satisfies separate shape contract");
        for(size_t shortN=0;shortN<n;++shortN)check(!strictShape(produced.data(),shortN,kind),"every truncated fixture rejects in standalone examiner");
        check(!strictShape(produced.data(),n+1,kind),"oversized fixture rejects in standalone examiner");
        if(kind==2||kind==3) {
            produced[8]=3;check(!strictShape(produced.data(),n,kind),"future compound version rejects in standalone examiner");
        }
    }
    auto pat=read(dir+"/pattern.pat");check(pat.size()-8==50361,"exact pattern payload50361");
    check(!strictShape(pat.data(),pat.size()-56,P17_PATTERN),"strict profile rejects old missing-length form rather than guessing");
    pat[8+2]=255;check(!strictShape(pat.data(),pat.size(),P17_PATTERN),"invalid note field rejects in standalone examiner");
    // The two compound kinds are structurally indistinguishable by size/header:
    // correct scope must be supplied by extension/user selection.
    auto all=read(dir+"/all.all");check(strictShape(all.data(),all.size(),P17_PERFORMANCE),"no magic distinguishes all from prf");
    auto snd=read(dir+"/sound.snd");check(!strictShape(snd.data(),snd.size(),P17_PATTERN),"sound and pattern are not interchangeable");
}
void readSound(const std::string& dir) {
    auto snd=read(dir+"/sound.snd");
    std::array<uint8_t,228> base{},target{};std::array<P17Message,1024> msgs{};
    check(p17_original_read_sound(snd.data(),snd.size(),0,base.data(),target.data())==1,"original-C complete sound load");
    for(unsigned i=0;i<228;++i) {
        uint8_t expected=snd[8+i];
        if(i>=161&&i<=166&&(expected<1||expected>6))expected=1;
        check(base[i]==expected,"original sound bytes/voice-selector normalization");
    }
    auto count=p17_original_messages(msgs.data(),msgs.size());check(count==240,"twelve target messages then228 ordinary setters");
    for(unsigned i=0;i<12;++i)check(msgs[i].status==(i%2==0?0xb8:0xb7),"velocity and LFO target order before scalar replay");
    for(size_t i=0;i<count;++i)check(msgs[i].status!=0xb2,"kit path sends no sequencer stop/load command");
    // Full-width selector205 is invalid;204 is valid. Neither may be truncated
    // into the plugin's seven-bit parameter bank during eventual conversion.
    snd[8+149]=204;snd[8+167]=205;
    check(p17_original_read_sound(snd.data(),snd.size(),0,base.data(),target.data())==1,"full-width original target bytes load");
    check(base[149]==204&&base[167]==0,"original target normalization at205 boundary");
    count=p17_original_messages(msgs.data(),msgs.size());
    check(((msgs[0].first&1u)<<7|msgs[0].second)==226,"full-width AVR row204 translated through actual modTargets");
    check(p17_original_read_sound(snd.data(),7,0,base.data(),target.data())==0,"original rejects incomplete eight-byte name");
    check(p17_original_read_sound(snd.data(),8,0,base.data(),target.data())==1,"original accepts name-only sound and zero-fills body");
    check(base[1]==0&&base[227]==0&&base[161]==1,"zero-fill plus sender LFO-voice clamp");
    snd.push_back(99);
    check(p17_original_read_sound(snd.data(),snd.size(),0,base.data(),target.data())==1,"original ignores trailing sound byte");
    snd.pop_back();snd[8+149]=7;snd[8+167]=10;
    check(p17_original_read_sound(snd.data(),snd.size(),1,base.data(),target.data())==1,"original morph-target read uses same sound file");
    check(target[149]==7&&target[167]==10&&base[149]==41,"morph target storage separate from active kit");
    count=p17_original_messages(msgs.data(),msgs.size());
    check(((msgs[0].first&1u)<<7|msgs[0].second)==p17_original_target_parameter(41),"morph-target load sends active-kit route identities");
}
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    layout(argv[1]);readSound(argv[1]);
    std::printf("{\"pass\":%u,\"fail\":%u,\"original_c_writer_formats\":5,\"original_c_sound_reader\":true,\"pattern_payload\":50361,\"pat\":50369,\"compound_v2\":50946,\"snd\":236,\"globals\":22,\"avr_abi_certified\":false}\n",passed,failed);
    return failed?1:0;
}
