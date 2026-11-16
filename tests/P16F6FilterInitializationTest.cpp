// A fresh portable filter must reproduce firmware zero-filled startup storage.
// Poisoned placement storage reproduces the complete-restore temporary defect.
#include "../dsp/ResonantFilter.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

int main(){
    int passed=0,failed=0;
    auto check=[&](const char* name,bool ok){if(ok)++passed;else{++failed;std::printf("FAIL %s\n",name);}};
    for(unsigned poison:{0u,0x55u,0xa5u,0xffu}){
        alignas(lxr::ResonantFilter) std::array<unsigned char,sizeof(lxr::ResonantFilter)> storage;
        storage.fill(static_cast<unsigned char>(poison));
        auto* f=new(storage.data()) lxr::ResonantFilter;
        // Read bytes, not an indeterminate float, in the unfixed negative run.
        uint32_t bits=0;std::memcpy(&bits,&f->zi,sizeof(bits));
        check("fresh filter has firmware zero startup input history",bits==0u);
        f->init();std::memcpy(&bits,&f->zi,sizeof(bits));
        check("startup init keeps defined zero input history",bits==0u);
        f->zi=.25f;f->init();
        check("repeat init retains original C omission semantics",f->zi==.25f);
        f->reset();check("explicit reset clears input history",f->zi==0.f);
        f->~ResonantFilter();
    }
    std::printf("SUMMARY pass=%d fail=%d\n",passed,failed);return failed?1:0;
}
