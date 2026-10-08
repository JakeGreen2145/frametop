// SPDX-License-Identifier: MIT
// Test-only bridge between the compiled production Adapter and pinned Unicorn.
#define main upstream_fixture_main
#include "../render/test.cpp"
#undef main
namespace ar=native_portal::arena;
using Resolve=bool (*)(void*,Descriptor*) noexcept;
struct Machine : Fake {
    Fixture source;
    ar::Memory memory;
    Resolve resolve;
    uint64_t camera,stock;
    Adapter adapter;
    EyeInput input{};
    uint64_t ticket=0;
    static Binding bind(ar::Bridge* bridge,uint64_t r) {
        Binding b{{123,r,1,0x200,1},{0x60,0x78,0x2988a8,0x228,0xb0,0x1a8},0,bridge,false};return b;
    }
    template<class T> T read(uint64_t a) {T t{};assert(memory.read(memory.user,a,&t,sizeof t));return t;}
    static bool observe_machine(void* u,const Binding& b,Address w,Address,Observation& out) noexcept {
        auto& f=*static_cast<Machine*>(u);auto r=b.identity.renderer;
        f.obs.arena.pass_begin=f.read<uint64_t>(r+0x15a0);f.obs.arena.pass_end=f.read<uint64_t>(r+0x15a8);
        f.obs.arena.command_wrapper=f.read<uint64_t>(r+0x1380);
        f.obs.arena.command=f.read<uint64_t>(f.obs.arena.command_wrapper+8);
        f.obs.arena.resource=f.read<uint64_t>(r+0x1390);
        f.obs.arena.buffer=f.read<uint64_t>(f.obs.arena.resource+0xb0);
        f.obs.arena.capacity=f.read<uint32_t>(f.obs.arena.resource+0xb8);
        f.obs.arena.map=f.read<uint64_t>(r+0x1410);f.obs.arena.cursor=f.read<uint32_t>(r+0x1418);
        f.obs.arena.alignment=f.read<uint64_t>(r+0x328);
        f.obs.wrapper_resource=f.read<uint64_t>(w+8);f.obs.wrapper_size=f.read<uint32_t>(f.obs.wrapper_resource+0xb8);
        f.obs.wrapper_offset=f.read<uint32_t>(w+0x18);out=f.obs;return true;
    }
    static bool call_machine(void* u,Registers& r) noexcept {
        auto& f=*static_cast<Machine*>(u);
        assert(r.target!=0x78 && "late native upload is forbidden");
        if(r.target==0x2988a8)return f.resolve(f.memory.user,reinterpret_cast<Descriptor*>(r.x[1]));
        return Fake::call(u,r);
    }
    static bool submit_machine(void* u,const Provenance& a,const Provenance&,Completion& c) noexcept {
        auto& f=*static_cast<Machine*>(u);c={a.frame,f.obs.arena.command,0x900,22,1};return true;
    }
    static bool settled_machine(void*,const Binding&,const Provenance&,const Provenance&,const Completion&) noexcept{return true;}
    Callbacks callbacks_machine() {auto c=Fake::callbacks();c.user=this;c.observe=observe_machine;c.call=call_machine;c.headroom=nullptr;c.submission=submit_machine;c.settled=settled_machine;return c;}
    Machine(ar::Bridge* bridge,ar::Memory m,Resolve res,uint64_t r,uint64_t w,uint64_t k,uint64_t sw)
        :memory(m),resolve(res),camera(k),stock(sw),adapter(source.control,source.gate,bind(bridge,r),callbacks_machine(),2) {
        binding=bind(bridge,r);wrapper=w;
        assert(adapter.initialize());PolicyInput p;p.enabled=true;
        auto frame=source.gate.begin(1,p,true,true,false,now);assert(adapter.latch(frame));
    }
    uint64_t reserve(unsigned eye) {
        input=source.eye(eye);
        assert(memory.read(memory.user,camera+0xc0,input.constants.data(),528));
        ticket=adapter.reserve_upload(input,camera,stock);return ticket;
    }
    bool enqueue(uint64_t t) {return adapter.enqueue(t,input);}
    bool finish() {
        if(!adapter.outer_return())return false;
        auto lookup=source.lookup();lookup.x[0]=binding.identity.renderer;
        Context context{binding.identity,obs.arena.recording_generation};
        if(!prepare_commit(adapter,*this,context,lookup))return false;
        for(auto& record:std::array<Recorded,2>{records[0],records[1]})
            if(adapter.backend(record,context,lookup).decision!=Decision::Portal)return false;
        if(!adapter.submitted())return false;
        complete=true;return adapter.poll_completion() && adapter.reclaim();
    }
};
extern "C" {
Machine* upstream_new(ar::Bridge* b,ar::Memory* m,Resolve res,uint64_t r,uint64_t w,uint64_t k,uint64_t sw){return new Machine(b,*m,res,r,w,k,sw);}
void upstream_delete(Machine* m){delete m;}
uint64_t upstream_reserve(Machine* m,unsigned eye){return m->reserve(eye);}
void upstream_dispatch(Machine* m,ar::Saved* s,unsigned site,uint64_t life,uint64_t thread){m->adapter.upstream(*s,site,life,thread);}
bool upstream_enqueue(Machine* m,uint64_t ticket){return m->enqueue(ticket);}
bool upstream_finish(Machine* m){return m->finish();}
void upstream_off(Machine* m){m->adapter.off();}
void upstream_expire(Machine*){now=100;}
void upstream_generation(Machine* m){++m->obs.arena.recording_generation;}
bool upstream_pending(Machine* m){return m->adapter.state().pending;}
unsigned upstream_enqueues(Machine* m){return m->enqueues;}
void upstream_record(Machine* m,unsigned i,Descriptor* out){assert(i<m->enqueues);*out=m->records[i].descriptors[0];}
}
