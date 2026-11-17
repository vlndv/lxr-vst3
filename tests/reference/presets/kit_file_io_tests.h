#pragma once
// Test-only independent I/O fault model; no production feature flag or state.
#include <cwchar>
#include <string>
#include <thread>

struct MockFiles final {
    enum class Fault { None, Open, Size, Read, ShortRead, Extra, Create, Write, ShortWrite, Flush, Publish } fault = Fault::None;
    file::Bytes source {}, temporaryBytes {}, destination {};
    std::uint64_t size = 236;
    unsigned creates=0, reads=0, discards=0, closes=0, publishes=0, operations=0, collisions=0;
    bool existing=false, temporary=false, deletePending=false, opened=false;
    bool cleanupFailure=false, closeFailure=false;
    static MockFiles& state(void* p) { return *static_cast<MockFiles*>(p); }
    static bool fail(file::IoFailure& e, bool exists=false) { e={123,exists};return false; }
    static file::FileHandle open(void* p,const wchar_t*,file::IoFailure& e) noexcept {
        auto& s=state(p);++s.operations;if(s.fault==Fault::Open){fail(e);return nullptr;}s.opened=true;return p;
    }
    static file::FileHandle create(void* p,const wchar_t*,file::IoFailure& e) noexcept {
        auto& s=state(p);++s.operations;++s.creates;
        if(s.creates<=s.collisions){fail(e,true);return nullptr;}
        if(s.fault==Fault::Create){fail(e);return nullptr;}
        s.temporary=true;s.opened=true;return p;
    }
    static bool getSize(void* p,file::FileHandle,std::uint64_t& n,file::IoFailure& e) noexcept {
        auto& s=state(p);++s.operations;if(s.fault==Fault::Size)return fail(e);n=s.size;return true;
    }
    static bool read(void* p,file::FileHandle,std::uint8_t* b,std::uint32_t n,std::uint32_t& count,file::IoFailure& e) noexcept {
        auto& s=state(p);++s.operations;++s.reads;if(s.fault==Fault::Read)return fail(e);
        if(s.reads==1){count=s.fault==Fault::ShortRead?n-1:n;for(unsigned i=0;i<count;++i)b[i]=s.source[i];}
        else {count=s.fault==Fault::Extra?1:0;if(count)b[0]=17;}
        return true;
    }
    static bool write(void* p,file::FileHandle,const std::uint8_t* b,std::uint32_t n,std::uint32_t& count,file::IoFailure& e) noexcept {
        auto& s=state(p);++s.operations;if(s.fault==Fault::Write)return fail(e);
        count=s.fault==Fault::ShortWrite?n-1:n;for(unsigned i=0;i<count;++i)s.temporaryBytes[i]=b[i];return true;
    }
    static bool flush(void* p,file::FileHandle,file::IoFailure& e) noexcept {
        auto& s=state(p);++s.operations;return s.fault==Fault::Flush?fail(e):true;
    }
    static bool publish(void* p,file::FileHandle,const wchar_t*,file::IoFailure& e) noexcept {
        auto& s=state(p);++s.operations;++s.publishes;
        if(s.existing)return fail(e,true);
        if(s.fault==Fault::Publish)return fail(e);
        s.destination=s.temporaryBytes;s.existing=true;s.temporary=false;return true;
    }
    static bool discard(void* p,file::FileHandle,file::IoFailure& e) noexcept {
        auto& s=state(p);++s.operations;++s.discards;
        if(s.cleanupFailure)return fail(e);s.deletePending=true;return true;
    }
    static bool close(void* p,file::FileHandle,file::IoFailure& e) noexcept {
        auto& s=state(p);++s.operations;++s.closes;if(s.closeFailure)return fail(e);
        s.opened=false;if(s.deletePending)s.temporary=false;return true;
    }
    static std::uint64_t token(void*) noexcept {return 0x370017c;}
    file::FileOps ops() noexcept {return {this,open,create,getSize,read,write,flush,publish,discard,close,token};}
};

static void faultFiles() {
    const auto init=MakeFirmwareInitializationKit();RawKit marker=init;marker.values[1]=3;
    file::Bytes bytes {};CHECK(file::EncodeRaw(init,bytes.data(),bytes.size()));
    for(auto fault:{MockFiles::Fault::Open,MockFiles::Fault::Size,MockFiles::Fault::Read,MockFiles::Fault::ShortRead,MockFiles::Fault::Extra}) {
        MockFiles s;s.fault=fault;s.source=bytes;auto out=marker;
        CHECK(!file::ReadFile(L"C:\\fixture\\input.snd",out,s.ops()));CHECK(same(out,marker));
        CHECK(s.closes==(fault==MockFiles::Fault::Open?0u:1u));
    }
    for(std::uint64_t size:std::array<std::uint64_t,5>{{0,1,235,237,UINT64_MAX}}) {
        MockFiles s;s.size=size;auto out=marker;const auto r=file::ReadFile(L"C:\\fixture\\input.snd",out,s.ops());
        CHECK(r.error==file::FileError::Codec);CHECK(r.codec.error==(size<236?file::Error::Truncated:file::Error::Oversized));
        CHECK(s.reads==0 && s.closes==1);CHECK(same(out,marker));
    }
    MockFiles readClose;readClose.closeFailure=true;auto out=marker;
    CHECK(file::ReadFile(L"C:\\fixture\\input.snd",out,readClose.ops()).error==file::FileError::Close);CHECK(same(out,marker));
    for(auto fault:{MockFiles::Fault::Create,MockFiles::Fault::Write,MockFiles::Fault::ShortWrite,MockFiles::Fault::Flush,MockFiles::Fault::Publish}) {
        MockFiles s;s.fault=fault;const auto r=file::WriteNewFile(L"C:\\fixture\\target.snd",init,s.ops());
        CHECK(!r && !r.published);CHECK(!s.existing && !s.temporary);
        CHECK(s.discards==(fault==MockFiles::Fault::Create?0u:1u));CHECK(!r.cleanupFailed);
    }
    MockFiles existing;existing.existing=true;existing.destination.fill(91);const auto old=existing.destination;
    auto r=file::WriteNewFile(L"C:\\fixture\\target.snd",init,existing.ops());
    CHECK(r.error==file::FileError::DestinationExists && !r.published);CHECK(existing.destination==old);CHECK(!existing.temporary);
    MockFiles exhausted;exhausted.collisions=16;r=file::WriteNewFile(L"C:\\fixture\\target.snd",init,exhausted.ops());
    CHECK(r.error==file::FileError::TemporaryExhausted);CHECK(exhausted.creates==16 && exhausted.discards==0 && exhausted.closes==0);
    MockFiles collision;collision.collisions=2;r=file::WriteNewFile(L"C:\\fixture\\target.snd",init,collision.ops());
    CHECK(r && r.published && collision.creates==3);CHECK(collision.destination==bytes);
    MockFiles cleanup;cleanup.fault=MockFiles::Fault::Write;cleanup.cleanupFailure=true;
    r=file::WriteNewFile(L"C:\\fixture\\target.snd",init,cleanup.ops());
    CHECK(r.error==file::FileError::Write && r.cleanupFailed && r.temporary[0]!=0);CHECK(!cleanup.existing && cleanup.temporary);
    MockFiles close;close.closeFailure=true;r=file::WriteNewFile(L"C:\\fixture\\target.snd",init,close.ops());
    CHECK(r.error==file::FileError::Close && r.published);CHECK(close.destination==bytes);CHECK(close.discards==0);
    auto canonical=MakeCanonicalInitializationKit();auto routes=MakeDisabledRoutingState().routes;
    routes[8].flags=1;routes[8].identityNamespace=2;routes[8].originalIndex=0x8008;
    MockFiles noIo;r=file::WriteNewFile(L"C:\\fixture\\target.snd",canonical,routes,noIo.ops());
    CHECK(r.error==file::FileError::Codec && r.codec.error==file::Error::NonrepresentableRoute);
    CHECK(r.codec.field==169 && r.codec.route==8 && noIo.operations==0);
    auto invalid=canonical;invalid.snare.mix1=128;MockFiles noInvalidIo;
    r=file::WriteNewFile(L"C:\\fixture\\invalid.snd",invalid,noInvalidIo.ops());
    CHECK(r.error==file::FileError::Codec && r.codec.error==file::Error::InvalidField && r.codec.field==28);
    CHECK(noInvalidIo.operations==0);
    for(const wchar_t* path:{L"",L"relative.snd",L"C:relative.snd",L"C:\\folder\\",L"C:\\NUL.snd",L"C:\\CON",L"C:\\x:ads",L"\\\\.\\pipe\\test",L"C:\\test.snd ",L"C:\\test.snd."}) {
        MockFiles s;r=file::WriteNewFile(path,init,s.ops());CHECK(r.error==file::FileError::InvalidPath);CHECK(s.operations==0);
    }
    MockFiles null;CHECK(file::WriteNewFile(nullptr,init,null.ops()).error==file::FileError::InvalidPath);CHECK(null.operations==0);
    std::array<wchar_t,file::kPathCapacity> longPath;longPath.fill(L'a');
    CHECK(file::WriteNewFile(longPath.data(),init,null.ops()).error==file::FileError::InvalidPath);
    CHECK(file::WriteNewFile(L"C:\\fixture\\target.snd",init,file::FileOps{}).error==file::FileError::InvalidOperations);
    // Independent contexts cannot share file payload, faults or ownership.
    MockFiles a,b;b.source.fill(17);auto ka=init,kb=init;
    CHECK(file::ReadFile(L"C:\\fixture\\a.snd",ka,a.ops()));CHECK(file::ReadFile(L"C:\\fixture\\b.snd",kb,b.ops()));
    CHECK(ka.values[1]==0 && kb.values[1]==17);
}

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
static std::wstring wide(const char* s) {
    const auto count=MultiByteToWideChar(CP_UTF8,0,s,-1,nullptr,0);CHECK(count>0);
    std::wstring result(static_cast<std::size_t>(count),0);MultiByteToWideChar(CP_UTF8,0,s,-1,result.data(),count);result.pop_back();return result;
}
static void nativeFiles(const char* directory) {
    const auto root=wide(directory);const auto path=[&](const wchar_t* leaf){return root+L"\\"+leaf;};
    auto raw=MakeFirmwareInitializationKit();raw.name={{'.','.', '/', 'x',0,255,1,2}};
    const auto filename=path(L"default.snd");auto r=file::WriteNewFile(filename.c_str(),raw);
    CHECK(r && r.published);RawKit restored {};CHECK(file::ReadFile(filename.c_str(),restored));CHECK(same(raw,restored));
    auto changed=raw;changed.values[1]=5;r=file::WriteNewFile(filename.c_str(),changed);
    CHECK(!r && !r.published);CHECK(file::ReadFile(filename.c_str(),restored));CHECK(same(raw,restored));
    const auto unicode=path(L"kit-\u5143.snd");CHECK(file::WriteNewFile(unicode.c_str(),changed));CHECK(file::ReadFile(unicode.c_str(),restored));CHECK(same(changed,restored));
    CanonicalKit can=MakeCanonicalInitializationKit();CHECK(file::ReadFile(filename.c_str(),can));
    CHECK(can.name==raw.name);
    auto corrupt=raw;corrupt.values[149]=205;const auto corruptPath=path(L"invalid-selector.snd");
    CHECK(file::WriteNewFile(corruptPath.c_str(),corrupt));const auto accepted=can;
    r=file::ReadFile(corruptPath.c_str(),can);
    CHECK(r.error==file::FileError::Codec && r.codec.error==file::Error::InvalidSelector && r.codec.field==149);
    CHECK(same(can,accepted));
    const auto absent=path(L"absent.snd");const auto before=restored;
    CHECK(!file::ReadFile(absent.c_str(),restored));CHECK(same(restored,before));
    const auto noParent=path(L"not-created\\kit.snd");CHECK(!file::WriteNewFile(noParent.c_str(),raw));
    CHECK(GetFileAttributesW(path(L"not-created").c_str())==INVALID_FILE_ATTRIBUTES);
    file::Bytes bytes {};CHECK(file::EncodeRaw(raw,bytes.data(),bytes.size()));
    for(unsigned n:{0u,1u,235u,237u}) {
        const auto bad=path((L"size-"+std::to_wstring(n)+L".snd").c_str());
        const auto h=CreateFileW(bad.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);CHECK(h!=INVALID_HANDLE_VALUE);
        std::array<std::uint8_t,237> data {};std::copy(bytes.begin(),bytes.end(),data.begin());DWORD wrote=0;
        CHECK(::WriteFile(h,data.data(),n,&wrote,nullptr)!=0 && wrote==n);CHECK(CloseHandle(h)!=0);
        restored=before;const auto result=file::ReadFile(bad.c_str(),restored);
        CHECK(!result && result.error==file::FileError::Codec);CHECK(same(restored,before));
    }
    const auto race=path(L"race.snd");file::FileResult first,second;
    std::thread one([&]{first=file::WriteNewFile(race.c_str(),raw);});
    std::thread two([&]{second=file::WriteNewFile(race.c_str(),changed);});one.join();two.join();
    CHECK(static_cast<bool>(first)!=static_cast<bool>(second));CHECK(file::ReadFile(race.c_str(),restored));
    CHECK(same(restored,raw)||same(restored,changed));
    WIN32_FIND_DATAW found {};const auto search=path(L"*.lxr-tmp-*");const auto scan=FindFirstFileW(search.c_str(),&found);
    CHECK(scan==INVALID_HANDLE_VALUE);if(scan!=INVALID_HANDLE_VALUE)FindClose(scan);
}
#else
static void nativeFiles(const char*) {CHECK(false);}
#endif
