// SPDX-License-Identifier: MIT
#include "evidence.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <sys/mman.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <cstdio>
using namespace native_portal;
using namespace native_portal::runtime;
#ifndef EVIDENCE_NO_LAYER
extern "C" uint64_t ft_pipeline_generation(void *p) { return p==reinterpret_cast<void*>(0x1234)?73:0; }
constexpr uint64_t expected_device_generation=73;
#else
constexpr uint64_t expected_device_generation=0;
#endif
template<class T,size_t N> void put(std::array<std::byte,N>& a,size_t off,T v) { std::memcpy(a.data()+off,&v,sizeof v); }
template<size_t N> uint64_t addr(std::array<std::byte,N>& a) {return reinterpret_cast<uint64_t>(a.data());}
struct Fixture {
    std::array<std::byte,0x16c0> r{};
    std::array<std::byte,0x128> resource{},own{};
    std::array<std::byte,0x20> wrapper{};
    std::array<std::byte,0x28> marker{};
    std::array<std::byte,0x10> command{};
    std::array<std::byte,0x200> stack{};
    Evidence evidence;
    arena::Saved s{};
    static constexpr uint64_t base=0x70000000;
    Fixture() {
        const auto R=addr(r),A=addr(resource);
        put(r,0,uint64_t(base+0x599c38));put(r,0xa78,uint64_t(0x1234));
        put(r,0xb0,uint64_t(0x4321));put(r,0x78,uint64_t(0x8888));
        put(r,0xec0,R+0x50);put(r,0xf00,uint32_t(2));
        put(r,0xf04,uint32_t(15));put(r,0xf0c,uint32_t(14));
        put(r,0x1390,A);put(r,0x1408,A);put(r,0x1410,uint64_t(0x100000));
        put(r,0x328,uint64_t(64));put(r,0x1380,addr(command));
        put(r,0x15a0,uint64_t(0x200000));put(r,0x15a8,uint64_t(0x200098));
        put(command,8,uint64_t(0x400000));
        put(resource,8,R+0x50);put(resource,0x50,uint64_t(0x777));
        put(resource,0x58,uint32_t(1));put(resource,0x60,uint64_t(0x81000));
        put(resource,0x98,uint32_t(1));put(resource,0xa0,uint64_t(0x100000));
        put(resource,0xb0,uint64_t(0x999));put(resource,0xb8,uint32_t(0x80080));
        put(own,8,R+0x50);put(own,0xb8,uint32_t(592));
        put(wrapper,8,addr(own));put(wrapper,0x18,UINT32_MAX);
        const std::array<uint32_t,8> words{1,1,7,0,1,7,0,15};
        std::memcpy(marker.data()+8,words.data(),32);
        s.sp=addr(stack);s.x[24]=R;
    }
    bool bootstrap() {
        assert(evidence.pin(base,elf_sha256));
        assert(evidence.event(0x7fda0,s));
        s.x[19]=addr(r);s.x[0]=addr(r)+0xec0;s.x[1]=0x80080;s.x[8]=s.sp+0x140;
        assert(evidence.event(0x29a14c,s));
        put(stack,0x140,addr(resource));assert(evidence.event(0x29a150,s));
        // Native initializer consumes and clears the output slot. Never reread it.
        put(stack,0x140,uint64_t(0));s.x[1]=addr(command);
        assert(evidence.event(0x29a528,s));
        s.x[0]=1;return evidence.event(0x7fdbc,s);
    }
    bool objects() {
        render::Registers x{};x.target=base+0x296c00;x.x={addr(r),592,0,0,0,0,0,0};
        if(!evidence.call_enter(x))return false;
        x.x[0]=addr(wrapper);if(!evidence.call_return(x))return false;
        std::array<uint32_t,6> words{1,1,3,1,3,0};
        x.target=base+0x294358;x.x={addr(r),reinterpret_cast<uint64_t>(words.data()),0,0,0,0,0,0};
        if(!evidence.call_enter(x))return false;
        x.x[0]=addr(marker);return evidence.call_return(x);
    }
};
int main() {
    // Real kernel self-read, including page boundary, EFAULT and integer overflow.
    long page=sysconf(_SC_PAGESIZE);assert(page>0);
    auto *p=static_cast<unsigned char*>(mmap(nullptr,2*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    assert(p!=MAP_FAILED);std::memset(p,42,page);assert(mprotect(p+page,page,PROT_NONE)==0);
    std::array<unsigned char,16> out{};
    assert(read_self(reinterpret_cast<uint64_t>(p)+page-16,out.data(),16));assert(out[15]==42);
    assert(!read_self(reinterpret_cast<uint64_t>(p)+page-8,out.data(),16));
    assert(!read_self(UINT64_MAX-3,out.data(),8));assert(!read_self(0,out.data(),8));
    assert(!read_self(reinterpret_cast<uint64_t>(p),out.data(),0));
    assert(munmap(p,2*page)==0);assert(!read_self(reinterpret_cast<uint64_t>(p),out.data(),8));
    Fixture f;render::Observation o{};
    assert(!f.evidence.snapshot(addr(f.wrapper),addr(f.marker),o));
    assert(f.bootstrap());assert(f.evidence.initialized());assert(f.evidence.coherent(addr(f.resource)));
    assert(f.evidence.identity().device_generation==expected_device_generation);assert(f.evidence.identity().renderer_generation);
    assert(f.objects());assert(f.evidence.snapshot(addr(f.wrapper),addr(f.marker),o));
    assert(o.arena.capacity==0x80080 && o.wrapper_size==592 && o.wrapper_offset==UINT32_MAX);
    assert(o.arena.command==0x400000 && o.arena.recording_generation==0);
    // No fabricated recording/pass lifetime from successful bootstrap or maps.
    render::Binding b{};b.identity=f.evidence.identity();
    assert(!Evidence::observe(&f.evidence,b,addr(f.wrapper),addr(f.marker),o));
    auto pass=f.evidence.pass_reader();assert(!pass.live(pass.user,{b.identity,1}));
    uint64_t value=0;auto memory=f.evidence.bridge_memory();
    assert(memory.read(memory.user,addr(f.r)+0xa78,&value,8) && value==0x1234);
    assert(!memory.read(memory.user,addr(f.r)+f.r.size()-4,&value,8));
    assert(!memory.read(memory.user,addr(f.stack)+0x140,&value,8));
    assert(!memory.read(memory.user,0x200000,&value,8));
    assert(!memory.write(memory.user,addr(f.r),&value,8));
    // Selected index and its actual table flags, never the first type or nonnull map.
    for(uint32_t flags : {0u,2u,4u,10u}) {
        put(f.r,0xf0c,flags);assert(!f.evidence.coherent(addr(f.resource)));
    }
    put(f.r,0xf0c,uint32_t(14));assert(f.evidence.coherent(addr(f.resource)));
    put(f.resource,0x50,uint64_t(0x778));assert(!f.evidence.coherent(addr(f.resource)));
    put(f.resource,0x50,uint64_t(0x777));
    put(f.resource,0x58,uint32_t(0));assert(!f.evidence.coherent(addr(f.resource)));
    put(f.resource,0x58,uint32_t(1));
    put(f.wrapper,0x18,uint32_t(0));assert(!f.evidence.snapshot(addr(f.wrapper),addr(f.marker),o));
    put(f.wrapper,0x18,UINT32_MAX);
    put(f.resource,0x58,uint32_t(2));assert(!f.evidence.coherent(addr(f.resource)));
    put(f.resource,0x58,uint32_t(1));put(f.r,0xf00,uint32_t(33));assert(!f.evidence.coherent(addr(f.resource)));
    put(f.r,0xf00,uint32_t(2));put(f.resource,0xa0,uint64_t(0x200000));assert(!f.evidence.coherent(addr(f.resource)));
    put(f.resource,0xa0,uint64_t(0x100000));put(f.resource,0x98,uint32_t(0));assert(!f.evidence.coherent(addr(f.resource)));
    put(f.resource,0x98,uint32_t(1));put(f.r,0x1408,addr(f.own));assert(!f.evidence.coherent(addr(f.resource)));
    put(f.r,0x1408,addr(f.resource));put(f.resource,8,uint64_t(4));assert(!f.evidence.coherent(addr(f.resource)));
    put(f.resource,8,addr(f.r)+0x50);put(f.wrapper,8,addr(f.resource));assert(!f.evidence.snapshot(addr(f.wrapper),addr(f.marker),o));
    put(f.wrapper,8,addr(f.own));put(f.marker,8,uint32_t(0));assert(!f.evidence.snapshot(addr(f.wrapper),addr(f.marker),o));
    put(f.marker,8,uint32_t(1));put(f.r,0x15a8,uint64_t(0x200099));assert(!f.evidence.snapshot(addr(f.wrapper),addr(f.marker),o));
    put(f.r,0x15a8,uint64_t(0x200098));
    bool refused=false;std::thread t([&]{refused=!f.evidence.coherent(addr(f.resource));assert(!f.evidence.event(0x29bd18,f.s));});t.join();assert(refused);
    pid_t child=fork();assert(child>=0);if(!child)_exit(f.evidence.initialized()?1:0);
    int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    f.s.x[22]=addr(f.r);assert(f.evidence.event(0x2987d4,f.s));assert(!pass.live(pass.user,{b.identity,1}));
    f.s.x[0]=addr(f.r);assert(f.evidence.event(0x29bd18,f.s));assert(!f.evidence.initialized());assert(!memory.read(memory.user,addr(f.r),&value,8));
    Fixture wrong;assert(wrong.evidence.pin(Fixture::base,elf_sha256));assert(wrong.evidence.event(0x7fda0,wrong.s));wrong.s.x[0]=1;
    assert(!wrong.evidence.event(0x7fdbc,wrong.s)); // no allocation proof
    Fixture fail;put(fail.resource,0x58,uint32_t(999));assert(!fail.bootstrap());
    Fixture mismatch;put(mismatch.r,0x1390,addr(mismatch.own));assert(!mismatch.bootstrap());
    Fixture high;high.s.x[24]=UINT64_MAX-16;assert(high.evidence.pin(Fixture::base,elf_sha256));assert(!high.evidence.event(0x7fda0,high.s));
    Fixture epoch;assert(epoch.bootstrap());assert(epoch.evidence.identity().renderer_generation>b.identity.renderer_generation);
    assert(epoch.objects());uint64_t dying=addr(epoch.wrapper);
    epoch.s.x[0]=addr(epoch.r);epoch.s.x[1]=reinterpret_cast<uint64_t>(&dying);
    assert(epoch.evidence.event(0x2966a0,epoch.s));
    assert(!epoch.evidence.snapshot(addr(epoch.wrapper),addr(epoch.marker),o));
    Fixture unmap;assert(unmap.bootstrap());unmap.s.x[0]=addr(unmap.resource);
    assert(unmap.evidence.event(0x2fe090,unmap.s));assert(!unmap.evidence.coherent(addr(unmap.resource)));
    Fixture bad_pin;assert(!bad_pin.evidence.pin(Fixture::base,"not-the-pin"));
    Fixture no_map;put(no_map.r,0x1410,uint64_t(0));assert(!no_map.bootstrap());
    Fixture u32;put(u32.resource,0xbc,uint32_t(0xdeadbeef));assert(u32.bootstrap());
    Fixture marker_dead;assert(marker_dead.bootstrap());assert(marker_dead.objects());
    dying=addr(marker_dead.marker);marker_dead.s.x[0]=addr(marker_dead.r);marker_dead.s.x[1]=reinterpret_cast<uint64_t>(&dying);
    assert(marker_dead.evidence.event(0x294678,marker_dead.s));assert(!marker_dead.evidence.initialized());
    Fixture nested;assert(nested.bootstrap());render::Registers call{};
    call.target=Fixture::base+0x296c00;call.x={addr(nested.r),592,0,0,0,0,0,0};
    assert(nested.evidence.call_enter(call));assert(!nested.evidence.call_enter(call));
    call.x[0]=addr(nested.wrapper);assert(!nested.evidence.call_return(call));
    std::puts("evidence: real self-read boundaries, typed allocation/coherence/objects, epoch, fork/thread and unsupported recording passed");
}
