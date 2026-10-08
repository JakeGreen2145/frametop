// SPDX-License-Identifier: MIT
#include "evidence.hpp"
#include "../binding/pass_gate.hpp"
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
    std::array<std::byte,0x7b0> executable{},other{};
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
        put(command,8,addr(executable));put(executable,8,R+0x50);
        put(executable,0x738,uint64_t(0xc001));put(other,8,R+0x50);put(other,0x738,uint64_t(0xc002));
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
    bool bootstrap(bool recording=false,arena::Bridge *bridge=nullptr,unsigned pooled=0) {
        assert(evidence.pin(base,elf_sha256));
        auto event=[&](uint64_t pc){return bridge?evidence.route(*bridge,pc,s):evidence.event(pc,s);};
        assert(event(0x7fda0));
        s.x[19]=addr(r);s.x[0]=addr(r)+0xec0;s.x[1]=bridge?0x80000:0x80080;s.x[8]=s.sp+0x140;
        assert(event(0x29a14c));assert(s.x[1]==0x80080);
        put(stack,0x140,addr(resource));assert(event(0x29a150));
        // Native initializer consumes and clears the output slot. Never reread it.
        put(stack,0x140,uint64_t(0));
        if(recording) {
            // Real in-process objects, synthetic delivery of pinned native sites.
            const auto old=s;
            for(auto *object : {&executable,&other}) {
            if(object==&other && !pooled) break;
            s.x[0]=s.x[20]=addr(*object);s.sp=addr(stack)+0x100;
            assert(evidence.event(0x301808,s));
            s.sp-=0x40;s.x[19]=addr(*object);s.x[30]=base+0x300ae0;
            assert(evidence.event(0x300a08,s));
            s.sp-=0x30;put(stack,0x98,uint64_t(base+0x300ae0));s.x[0]=0;
            assert(evidence.event(0x300a38,s));
            s.sp=addr(stack)+0x100;s.x[20]=addr(*object);s.x[0]=1;
            assert(evidence.event(0x30180c,s));s=old;
            }
        }
        if(pooled==2) {
            const auto old=s;s.x[1]=addr(other);assert(evidence.event(0x311648,s));s=old;
        }
        s.x[1]=addr(command);
        assert(evidence.event(0x29a528,s));
        s.x[0]=1;return event(0x7fdbc);
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
struct AdapterFixture : Fixture {
    unsigned calls=0,observations=0,pipelines=0;
    static uint64_t clock() {return 10;}
    static bool validate(void *u,const render::Binding &b,render::Phase phase) noexcept {
        auto &f=*static_cast<AdapterFixture*>(u);
        return phase!=render::Phase::Retire && f.evidence.initialized() && b.identity.renderer==addr(f.r);
    }
    static bool call(void *u,render::Registers &r) noexcept {
        auto &f=*static_cast<AdapterFixture*>(u);
        if(!f.evidence.call_enter(r)) return false;
        // Synthetic allocation boundary only; actual Adapter and Evidence calls.
        ++f.calls;
        if(r.target==base+0x296c00) r.x[0]=addr(f.wrapper);
        else if(r.target==base+0x294358) r.x[0]=addr(f.marker);
        else return false;
        return f.evidence.call_return(r);
    }
    static bool observe(void *u,const render::Binding &b,uint64_t w,uint64_t m,render::Observation &o) noexcept {
        auto &f=*static_cast<AdapterFixture*>(u);++f.observations;
        return Evidence::observe(&f.evidence,b,w,m,o);
    }
    static bool stock(void *,const render::Binding &,uint64_t) noexcept {return false;}
    static bool pipeline(void *u,const render::Provenance &,render::Registers &) noexcept {
        ++static_cast<AdapterFixture*>(u)->pipelines;return false;
    }
};
static void adapter_and_pass_refusal() {
    AdapterFixture f;arena::Bridge bridge(f.evidence.bridge_memory());assert(f.bootstrap(true,&bridge));
    // Real process storage does not by itself grant pass/cache/attachment lifetime.
    std::array<std::byte,0x130> passes{};std::array<std::byte,0x560> packets{};
    std::array<std::byte,0x40> cache{};std::array<std::byte,0x150> target{};
    put(f.r,0x15a0,addr(passes));put(f.r,0x15a8,addr(passes)+passes.size());
    put(f.r,0x1478,addr(cache));put(f.r,0x1480,addr(cache)+cache.size());
    for(unsigned i=0;i<2;++i) {
        put(passes,i*0x98,addr(target));put(passes,i*0x98+0x50,addr(packets)+i*0x2b0);
        put(passes,i*0x98+0x58,addr(packets)+(i+1)*0x2b0);put(passes,i*0x98+0x68,uint32_t(1));
        put(packets,i*0x2b0+8,addr(f.marker));
    }
    put(cache,0,uint64_t(0xabcd));put(cache,8,uint64_t(0xdcba));put(cache,0x10,addr(target));
    put(cache,0x38,uint32_t(1));put(target,0x14c,uint32_t(37));
    Backend backend;FrameGate gate(backend);BackendStatus status;
    // CPU control boundary, not runtime capability publication.
    status.capable=status.serialized=status.external_clear=true;status.origin=4;assert(backend.publish(status));
    Lease lease;lease.epoch=8;lease.origin=4;lease.generation=2;lease.deadline_ns=100;lease.flags=1;
    lease.geometry.center[1]=-2.5f;assert(backend.publish_lease(lease));
    render::Binding b{};b.identity=f.evidence.identity();b.upstream=&bridge;
    // Unexercised upload/descriptor/enqueue slots are explicit refusal sentinels.
    b.targets={Fixture::base+0x296c00,1,1,1,Fixture::base+0x2966a0,Fixture::base+0x294358};
    render::Callbacks cb{};cb.user=&f;cb.validate=AdapterFixture::validate;cb.call=AdapterFixture::call;
    cb.observe=AdapterFixture::observe;cb.stock_blend=AdapterFixture::stock;cb.pipeline=AdapterFixture::pipeline;cb.clock=AdapterFixture::clock;
    render::Adapter adapter(backend,gate,b,cb,2);
    if(!expected_device_generation) {assert(!adapter.initialize() && !f.calls);return;}
    assert(adapter.initialize() && f.calls==2 && f.observations);
    render::Observation o{};assert(f.evidence.snapshot(addr(f.wrapper),addr(f.marker),o));
    PolicyInput input;input.enabled=true;auto frame=gate.begin(1,input,true,true,false,10);assert(adapter.latch(frame));
    binding::PassRequest request{{b.identity,o.arena.recording_generation},addr(f.marker),Fixture::base,0};
    auto reader=f.evidence.pass_reader();
    assert(binding::extract_passes(reader,request).reason==binding::PassReason::Lifetime);
    binding::PassGate pass(adapter,reader,request);binding::PassRegisters regs{};
    regs.x[0]=addr(f.r);pass.callback(regs,0);
    assert(!adapter.state().committed && !f.pipelines);
    uint64_t out=0;
    for(auto address : {addr(passes),addr(packets),addr(cache),addr(target)})
        assert(!reader.read(reader.user,address,&out,8));
    // Cold/optimizer/cache replacement cannot turn an unretained graph live.
    put(f.r,0x15d0,uint8_t(1));put(f.r,0x1480,addr(cache));
    assert(!reader.live(reader.user,request.context));
    f.s.x[0]=addr(f.executable);assert(f.evidence.event(0x4b3d0,f.s));
    assert(!Evidence::observe(&f.evidence,b,addr(f.wrapper),addr(f.marker),o));
    assert(!reader.read(reader.user,addr(f.executable)+0x738,&out,8));
}
static void initial_failures() {
    for(unsigned fault=0;fault<9;++fault) {
        Fixture f;assert(f.evidence.pin(Fixture::base,elf_sha256));assert(f.evidence.event(0x7fda0,f.s));
        f.s.x[0]=f.s.x[20]=addr(f.executable);f.s.sp=addr(f.stack)+0x100;
        assert(f.evidence.event(0x301808,f.s));
        f.s.sp-=0x40;f.s.x[19]=addr(f.executable);f.s.x[30]=Fixture::base+0x300ae0;
        if(fault==1) ++f.s.sp;
        if(fault==2) ++f.s.x[30];
        if(fault==3) ++f.s.x[0];
        if(fault>=1 && fault<=3) {assert(!f.evidence.event(0x300a08,f.s));continue;}
        assert(f.evidence.event(0x300a08,f.s));
        f.s.sp-=0x30;put(f.stack,0x98,uint64_t(Fixture::base+0x300ae0));f.s.x[0]=0;
        if(fault==0) f.s.x[0]=uint32_t(-4);
        if(fault==4) ++f.s.sp;
        if(fault==5) put(f.stack,0x98,uint64_t(Fixture::base+0x300ae4));
        if(fault==6) put(f.executable,0x738,uint64_t(0xc002));
        if(fault<=6) assert(!f.evidence.event(0x300a38,f.s));
        else assert(f.evidence.event(0x300a38,f.s));
        if(fault>=4 && fault<=6) continue;
        f.s.sp=addr(f.stack)+0x100;f.s.x[20]=addr(f.executable);f.s.x[0]=fault==8?0:1;
        if(fault==7) ++f.s.sp;
        assert(!f.evidence.event(0x30180c,f.s));
        assert(!f.evidence.initialized());
    }
    // Both successful begin and a matching published native C are required.
    Fixture released_pool;assert(released_pool.bootstrap(true,nullptr,2));
    Fixture mismatch;put(mismatch.command,8,addr(mismatch.other));
    assert(mismatch.bootstrap(true));assert(mismatch.objects());render::Observation o{};
    assert(mismatch.evidence.snapshot(addr(mismatch.wrapper),addr(mismatch.marker),o));assert(!o.arena.recording_generation);
    for(uint64_t pc : {0x311648u,0x4b3d0u,0x3003d0u,0x305260u,0x2953a0u}) {
        Fixture f;assert(f.bootstrap(true));assert(f.objects());
        f.s.x[0]=(pc==0x305260 || pc==0x2953a0)?addr(f.command):addr(f.executable);
        f.s.x[1]=addr(f.executable);assert(f.evidence.event(pc,f.s));
        uint64_t value=0;auto m=f.evidence.bridge_memory();
        assert(!m.read(m.user,addr(f.executable)+0x738,&value,8));
        assert(!f.evidence.snapshot(addr(f.wrapper),addr(f.marker),o));
    }
    Fixture reset;assert(reset.bootstrap(true));assert(reset.objects());
    reset.s.x[0]=addr(reset.executable);assert(!reset.evidence.event(0x300b20,reset.s));
    assert(reset.evidence.snapshot(addr(reset.wrapper),addr(reset.marker),o));assert(!o.arena.recording_generation);
    reset.s.x[0]=0;assert(!reset.evidence.event(0x300c54,reset.s));
    reset.s.x[0]=addr(reset.executable);assert(!reset.evidence.event(0x300a08,reset.s));
    reset.s.x[0]=0;assert(!reset.evidence.event(0x300a38,reset.s));
    assert(reset.evidence.snapshot(addr(reset.wrapper),addr(reset.marker),o));assert(!o.arena.recording_generation);
    Fixture reused;assert(reused.bootstrap(true));reused.s.x[0]=reused.s.x[20]=addr(reused.executable);
    assert(!reused.evidence.event(0x301808,reused.s));
}
static void unpaired_begin_revokes() {
    Fixture f;assert(f.bootstrap(true));assert(f.objects());
    render::Observation o{};render::Binding b{};b.identity=f.evidence.identity();
    f.s.x[0]=addr(f.executable);f.s.x[30]=Fixture::base+0x300ae0;
    assert(!f.evidence.event(0x300a08,f.s));
    assert(!Evidence::observe(&f.evidence,b,addr(f.wrapper),addr(f.marker),o));
}
#include <sys/syscall.h>
#include "staging_test.inc"
int main() {
    for(unsigned fault=0;fault<=34;++fault)staging_self_process(fault);
    unpaired_begin_revokes();initial_failures();adapter_and_pass_refusal();
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
    // Native unmap's signed-positive predicate at 0x2fe0c4/0x2fe0c8.
    for(uint32_t refs : {0u,1u,0x7fffffffu,0x80000000u,0xffffffffu}) {
        const bool positive=refs>0 && refs<=INT32_MAX;
        Fixture initial;render::Observation observation{};
        put(initial.resource,0x98,refs);
        assert(initial.bootstrap()==positive);
        assert(initial.evidence.initialized()==positive);
        assert(initial.evidence.coherent(addr(initial.resource))==positive);
        if(positive)assert(initial.objects());
        assert(initial.evidence.snapshot(addr(initial.wrapper),addr(initial.marker),observation)==positive);
    }
    for(uint32_t refs : {0u,1u,0x7fffffffu,0x80000000u,0xffffffffu}) {
        const bool positive=refs>0 && refs<=INT32_MAX;
        Fixture later;render::Observation observation{};
        assert(later.bootstrap());assert(later.objects());
        assert(later.evidence.snapshot(addr(later.wrapper),addr(later.marker),observation));
        put(later.resource,0x98,refs);
        assert(later.evidence.coherent(addr(later.resource))==positive);
        assert(later.evidence.snapshot(addr(later.wrapper),addr(later.marker),observation)==positive);
    }
    Fixture f;render::Observation o{};
    assert(!f.evidence.snapshot(addr(f.wrapper),addr(f.marker),o));
    assert(f.bootstrap());assert(f.evidence.initialized());assert(f.evidence.coherent(addr(f.resource)));
    assert(f.evidence.identity().device_generation==expected_device_generation);assert(f.evidence.identity().renderer_generation);
    assert(f.objects());assert(f.evidence.snapshot(addr(f.wrapper),addr(f.marker),o));
    assert(o.arena.capacity==0x80080 && o.wrapper_size==592 && o.wrapper_offset==UINT32_MAX);
    assert(o.arena.command==addr(f.executable) && o.arena.recording_generation==0);
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
    Fixture bridged;arena::Bridge bridge(bridged.evidence.bridge_memory());
    assert(bridged.bootstrap(true,&bridge));assert(bridge.ready());
    auto bm=bridged.evidence.bridge_memory();
    assert(!bm.read(bm.user,addr(bridged.stack)+0x140,&value,8));
    Fixture recorded;assert(recorded.bootstrap(true));assert(recorded.objects());
    assert(recorded.evidence.snapshot(addr(recorded.wrapper),addr(recorded.marker),o));
    assert(o.arena.recording_generation!=0);
    render::Binding supported{};supported.identity=recorded.evidence.identity();
    assert(Evidence::observe(&recorded.evidence,supported,addr(recorded.wrapper),addr(recorded.marker),o)==bool(expected_device_generation));
    std::thread foreign([&]{
        assert(!Evidence::observe(&recorded.evidence,supported,addr(recorded.wrapper),addr(recorded.marker),o));
        assert(!recorded.evidence.event(0x300b20,recorded.s));
    });foreign.join();
    for(unsigned field=0;field<5;++field) {
        auto bad=supported;
        uint64_t *fields[]={&bad.identity.process_instance,&bad.identity.renderer,&bad.identity.renderer_generation,
            &bad.identity.device,&bad.identity.device_generation};
        ++*fields[field];assert(!Evidence::observe(&recorded.evidence,bad,addr(recorded.wrapper),addr(recorded.marker),o));
    }
    if(expected_device_generation) {
        binding::Command command;
        Fixture pooled;assert(pooled.bootstrap(true,nullptr,true));assert(pooled.objects());
        assert(pooled.evidence.snapshot(addr(pooled.wrapper),addr(pooled.marker),o));
        const auto &a=o.arena;
        binding::Owner owner{pooled.evidence.identity(),a.recording_generation,a.recording_generation,a.command,0xc001,
            a.resource,a.buffer,a.map,a.capacity,a.alignment,0x701,0x702,0x703};
        assert(command.bind(owner));
        auto wrong=owner;++wrong.generation;assert(!pooled.evidence.bind_lifecycle(command,wrong));
        assert(pooled.evidence.bind_lifecycle(command,owner));
        assert(!pooled.evidence.bind_lifecycle(command,owner));
        // Registered renderer bytes must NOT become pass-graph lifetime merely
        // because malicious/corrupt vector endpoints alias that same allocation.
        for(unsigned eye=0;eye<2;++eye) {
            render::Provenance ticket{};ticket.identity=owner.identity;ticket.ticket=eye+1;
            ticket.frame=1;ticket.recording_generation=owner.recording;ticket.eye=eye;
            ticket.wrapper=addr(pooled.wrapper);ticket.marker=addr(pooled.marker);ticket.vs=58;ticket.ps=130;
            for(auto &d:ticket.descriptors) d={owner.buffer,eye*640,592};
            binding::Scope scope{owner,eye+1,2,0x900000,0x800000,owner.identity.renderer};
            assert(command.backend_enter(scope,ticket));assert(command.backend_return(scope,true));
            size_t packet=0x500+eye*0x2b0;put(pooled.r,packet+8,ticket.marker);
            put(pooled.r,packet+0x160,addr(pooled.r)+packet+0x170);put(pooled.r,packet+0x168,uint64_t(15));
            auto label=render::encode_ticket(ticket.ticket);std::memcpy(pooled.r.data()+packet+0x170,label.data(),16);
        }
        put(pooled.r,0x15a0,addr(pooled.r)+0x400);put(pooled.r,0x15a8,addr(pooled.r)+0x498);
        put(pooled.r,0x450,addr(pooled.r)+0x500);put(pooled.r,0x458,addr(pooled.r)+0xa60);
        pooled.s.x[19]=addr(pooled.r);assert(!pooled.evidence.event(0x2a2230,pooled.s));
        // A real bound Lifecycle refuses premature reset without exact queue/GPU
        // completion; Evidence cannot restore its old generation after refusal.
        pooled.s.x[0]=owner.command;pooled.s.x[30]=Fixture::base+0x1234;
        assert(!pooled.evidence.event(0x300b20,pooled.s));
        assert(pooled.evidence.snapshot(addr(pooled.wrapper),addr(pooled.marker),o));assert(!o.arena.recording_generation);
    }
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
    std::puts(expected_device_generation
        ? "evidence: real self-read, scoped begin/publication, Bridge bootstrap, bounded staging with 35 synthetic cases, Adapter descriptor capture, Lifecycle bind/refusal and fail-closed PassGate passed"
        : "evidence: no-layer initialization refusal and existing self-read/lifetime negatives passed; positive staging skipped without the synthetic layer export");
}
