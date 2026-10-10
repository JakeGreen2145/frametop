// SPDX-License-Identifier: MIT
// Actual Runtime + retained CameraOwner. Native calls below replay fresh pinned
// instruction observations, with explicit host relocation of modeled storage.
#include "source_admission.hpp"
#define FT_ATTACHMENT_TEST
#define ft_native_portal_call fixture_native_call
#include "runtime_chain_test.cpp"
#undef ft_native_portal_call
#include <sys/mman.h>
#include "source.private.hpp"
static uint64_t client_base;
static bool modules_present=true;
extern "C" uint64_t ft_native_portal_original(uint64_t pc) noexcept {return Chain::base+pc;}
extern "C" bool ft_native_portal_source_modules(uint64_t* comp,uint64_t* client) noexcept {
    if(!modules_present)return false;
    *comp=Chain::base;*client=client_base;return true;
}
struct Observed final:SourceAdmission {
    unsigned begins=0,events=0,ends=0;ClientSourceBinding::Observation last{};
    bool ready(const CameraBinding&) noexcept override {return false;} // no producer proof in this fixture
    coordination::NativeReceipt transition(const CameraBinding&,const coordination::ActionPlan&,uint64_t) noexcept override {std::abort();}
    bool settled(const CameraBinding&,uint64_t) noexcept override {return false;}
    void before(const CameraBinding&,coordination::Action) noexcept override {++begins;}
    void observe(const CameraBinding&,ClientSourceBinding::Module,uint64_t,const arena::Saved&) noexcept override {++events;}
    void after(const CameraBinding&,const ClientSourceBinding::Observation& s) noexcept override {++ends;last=s;}
};
static std::byte* storage;
static unsigned invocation=0;static bool omit=false,rebind=false,missing_return=false;
static uint64_t relocate(uint64_t x) {
    if(x>=0x90000000 && x<0x90001000)return addr(active->camera)+x-0x90000000;
    if(x>=0x90010000 && x<0x90400000)return uint64_t(storage)+x-0x90000000;
    if(x>=0x10000000 && x<0x10700000)return client_base+x-0x10000000;
    if(x>=0x40000 && x<0x700000)return Chain::base+x;
    return x;
}
extern "C" bool ft_native_portal_call(uint64_t target,uint64_t* x,uint64_t x8) noexcept {
    if(target==Chain::base+0x6d6e0 || target==Chain::base+0x6d6f0) {
        const uint64_t pc=target==Chain::base+0x6d6e0?0x15f9e0:0x15fba8;
        x[0]=addr(active->camera);arena::Saved nested{};nested.x[0]=x[0];
        active->router.event(pc,nested);
        if(nested.pad==arena::SkipOwned){x[0]=nested.x[0];return true;}
        assert(nested.pad==arena::Original);
        return ft_native_portal_call(Chain::base+pc,x,x8);
    }
    if(target!=Chain::base+0x15f9e0 && target!=Chain::base+0x15fba8 && target!=Chain::base+0x15fcc0)return fixture_native_call(target,x,x8);
    assert(invocation<std::size(source_calls));
    const auto& call=source_calls[invocation++];assert(target==Chain::base+call.pc);
    for(unsigned i=call.first;i<call.last;++i) {
        const auto& e=source_events[i];arena::Saved s{};s.sp=relocate(e.sp);
        for(unsigned j=0;j<31;++j)s.x[j]=relocate(e.x[j]);
        for(unsigned j=0;j<e.read_count;++j) {
            const auto& r=e.reads[j];uint64_t value=r.value;
            // Only native pointers, not queue IDs, are relocated.
            if(r.size==8)value=relocate(value);
            std::memcpy(reinterpret_cast<void*>(relocate(r.address)),&value,r.size);
        }
        if(rebind && e.pc==0x15fa24)s.x[0]+=0x1000;
        if(omit && e.pc==0x32a504)continue;
        if(missing_return && e.pc==0x15faa4)continue;
        active->router.event(e.pc|(e.module?(UINT64_C(1)<<63):0),s);
    }
    x[0]=call.result;return true;
}
int main(int argc,char** argv) {
    std::string mode=argc>1?argv[1]:"positive";
    modules_present=mode!="missing-module";omit=mode=="missing-event";missing_return=mode=="missing-nested-return";
    storage=static_cast<std::byte*>(mmap(nullptr,0x400000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    assert(storage!=MAP_FAILED);
    auto* client=mmap(nullptr,0x700000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(client!=MAP_FAILED);client_base=uint64_t(client);
    Observed source;auto* f=new Chain(&source);active=f;
    auto saved=f->s;f->s={};f->s.sp=addr(f->stack);f->s.x[19]=addr(f->app);f->s.x[20]=addr(f->app)+0x20000;f->s.x[26]=addr(f->camera);f->event(0x8dc1c);f->s=saved;
    f->bootstrap_router();f->publish_worker();f->run_enter();
    for(unsigned i=0;i<std::size(source_calls);++i) {
        if(i==3 && mode=="provider-rebind")rebind=true;
        f->s={};f->s.x[0]=addr(f->camera);f->s.x[30]=0x123456;f->s.x[8]=0xabcdef;
        uint64_t pc=source_calls[i].pc;
        if((mode=="nested" || missing_return) && (pc==0x15f9e0 || pc==0x15fba8)) {
            pc=pc==0x15f9e0?0x6d6e0:0x6d6f0;f->s.x[0]=addr(f->app);
        }
        if(mode=="foreign") {
            std::atomic<bool> done{false};
            std::thread incoming([&]{f->event(pc);done=true;});
            while(!done.load()) {arena::Saved pump{};f->router.event(0,pump);std::this_thread::yield();}
            incoming.join();
        } else f->event(pc);
        assert(f->s.pad==arena::SkipOwned && f->s.x[0]==source_calls[i].result && f->s.x[30]==0x123456 && f->s.x[8]==0xabcdef);
        if(missing_return) {assert(source.begins==1 && source.ends==1 && !source.last.valid);break;}
        if(modules_present) {
            assert(source.begins==i+1 && source.ends==i+1);
            if(rebind)assert(!source.last.valid);
            else assert(source.last.valid && source.last.actual==source_calls[i].result);
            if(i==1)for(const auto& r:source.last.reads)assert(r.acquire_observed && r.returned && r.increment && r.decrement && r.metadata && r.data);
            if(i==0)for(const auto& q:source.last.queues) {
                assert(q.observed && !q.result && q.id && q.header);
                assert(q.ownership==(omit?ClientSourceBinding::Entry::Unknown:ClientSourceBinding::Entry::NewEntry));
            }
        } else assert(!source.begins && !source.events && !source.ends);
    }
    assert(!f->router.diagnostics().capable);
    std::puts("PASS real Runtime retained invocation, pinned call vectors, persistent provider history; producer admission remains absent");
    std::fflush(stdout);_Exit(0); // process-lifetime native owner, no fabricated idle/producer settlement
}
