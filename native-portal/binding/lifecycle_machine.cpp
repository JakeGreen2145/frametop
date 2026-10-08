// SPDX-License-Identifier: MIT
// Test-only C ABI. Vulkan results and initial backend/submit evidence are synthetic.
#include "lifecycle.hpp"
namespace nb=native_portal::binding;
namespace nr=native_portal::render;
struct Machine {
    nb::Command command; nb::Generations ids; nb::Owner o{}; nb::Lifecycle life;
    nr::Provenance p[2]{}; uint64_t seq=0;
    nb::Scope q{},st{},wait{};
    nb::Signal signal{0x10005000,600,1};
    Machine(nb::Memory m):life(command,m,ids) {
        o={{1,0x10000000,1,2,1},1,ids.issue(),0x10004000,0xf00d,
           0x10006000,0xaabb0001,0x10040000,524288,64,0x1234,0xdeadbeef,0x10005000};
        if(!command.bind(o)||!life.bind(o,7)) __builtin_abort();
        for(unsigned i=0;i<2;++i) {
            auto &v=p[i];v.identity=o.identity;v.ticket=i+1;v.frame=601;v.lease=1;v.origin=1;
            v.geometry=1;v.deadline=100;v.revocation=1;v.recording_generation=1;v.eye=i;
            v.wrapper=0x777;v.marker=0x999; // shader pair supplied by the fixture caller
            v.descriptors={nr::Descriptor{o.buffer,i*640,592},nr::Descriptor{o.buffer,i*640,592}};
        }
    }
    nb::Scope scope(uint64_t object) {return {o,++seq,7,0x900000,0x800000,object};}
};
extern "C" {
void *life_new(nb::Memory *m,uint32_t vs,uint32_t ps) {
    auto *f=new Machine(*m);
    for(unsigned i=0;i<2;++i) {
        f->p[i].vs=vs;f->p[i].ps=ps;
        auto s=f->scope(0x10020000+i*0x2b0);
        if(!f->command.backend_enter(s,f->p[i]) || !f->command.backend_return(s,true)) __builtin_abort();
    }
    return f;
}
void life_delete(void *v) {delete static_cast<Machine *>(v);}
bool life_capture(void *v,unsigned site,const nb::Capture *r,uint64_t generation,uint64_t tid) {
    auto &f=*static_cast<Machine *>(v);
    return f.life.capture(static_cast<nb::Site>(site),*r,tid,generation,f.o.vk_command);
}
bool life_queue(void *v) {
    auto &f=*static_cast<Machine *>(v);f.q=f.scope(f.o.command);f.st=f.scope(f.o.command);
    nb::SubmitSnapshot s{true,true,1,1,1,1,f.o.queue,f.o.identity.device,f.o.vk_command,f.o.semaphore,153601,0};
    bool ok=f.command.queue_enter(f.q,s,f.signal)&&f.command.queue_result(f.q,0)&&f.command.stamp_enter(f.q,f.st,f.signal);
    for(auto object:{f.o.arena,f.o.command}) {auto r=f.scope(object);ok=ok&&f.command.resource_enter(f.st,r,f.signal)&&f.command.resource_post(r,object,f.signal);}
    return ok&&f.command.stamp_post(f.st,f.o.command,f.signal);
}
bool life_wait(void *v) {
    auto &f=*static_cast<Machine *>(v);f.wait=f.scope(f.signal.synchronization);
    return f.command.wait_enter(f.wait,f.signal,f.o.semaphore)&&f.command.wait_result(f.wait,f.signal,0)&&f.command.wait_return(f.wait,f.signal,1);
}
unsigned life_state(void *v) {
    auto &f=*static_cast<Machine *>(v);auto s=f.life.state();
    nr::Binding b{};b.identity=f.o.identity;nr::Completion c{};
    bool settled=f.command.submission(f.p[0],f.p[1],c)&&f.life.settled(b,f.p[0],f.p[1],c);
    return s.current | (s.retained<<1) | (s.invalidated<<2) | ((s.next_generation!=0)<<3) | (settled<<4) | (s.unsupported<<5);
}
}
