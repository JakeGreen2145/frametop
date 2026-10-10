// SPDX-License-Identifier: MIT
// Production Runtime() and provider. Only native call/memory and driver storage
// are fixture boundaries. Queue events come from freshly executed pinned ELF.
#include "source_admission.hpp"
#include "selection.private.hpp"
#define FT_ATTACHMENT_TEST
#define FT_PRODUCTION_ADMISSION_TEST
#define ft_native_portal_call fixture_native_call
#include "runtime_chain_test.cpp"
#undef ft_native_portal_call
#include <sys/mman.h>
#include "source.private.hpp"
#include "admission.private.hpp"
static uint64_t client_base;
static std::byte* storage;
static bool modules_present=true;
static std::string mode;
static unsigned native_updates=0;
extern "C" uint64_t ft_native_portal_original(uint64_t pc) noexcept {return Chain::base+pc;}
extern "C" uint64_t ft_native_portal_camera_coverage() noexcept {return 0xfffff;}
extern "C" bool ft_native_portal_source_modules(uint64_t* comp,uint64_t* client) noexcept {
    if(!modules_present)return false;
    *comp=Chain::base;*client=client_base;return true;
}
static uint64_t relocate(uint64_t x) {
    if(x>=0x90000000 && x<0x90001000)return addr(active->camera)+x-0x90000000;
    if(x>=0x90010000 && x<0x90400000)return uint64_t(storage)+x-0x90000000;
    if(x>=0x10000000 && x<0x10700000)return client_base+x-0x10000000;
    if(x>=0x40000 && x<0x700000)return Chain::base+x;
    return x;
}
static void replay_events(const SourceEvent* events,unsigned first,unsigned last) {
    for(unsigned i=first;i<last;++i) {
        const auto& e=events[i];arena::Saved s{};s.sp=relocate(e.sp);
        for(unsigned j=0;j<31;++j)s.x[j]=relocate(e.x[j]);
        for(unsigned j=0;j<e.read_count;++j) {
            const auto& r=e.reads[j];uint64_t value=r.size==8?relocate(r.value):r.value;
            std::memcpy(reinterpret_cast<void*>(relocate(r.address)),&value,r.size);
        }
        if(mode=="missing-event" && e.pc==0x32a504)continue;
        if(mode=="provider-migration" && e.pc==0x15fa24)s.x[0]+=0x1000;
        active->router.event(e.pc|(e.module?(UINT64_C(1)<<63):0),s);
    }
}
static void replay(unsigned index) {
    const auto& c=source_calls[index];replay_events(source_events,c.first,c.last);
}
extern "C" bool ft_native_portal_call(uint64_t target,uint64_t* x,uint64_t x8) noexcept {
    if(target==Chain::base+0x15f9e0 || target==Chain::base+0x15fba8) {
        const bool on=target==Chain::base+0x15f9e0;
        if(!on) {
            arena::Saved decision{};decision.x[0]=decision.x[19]=addr(active->camera);decision.sp=addr(active->camera_stack);
            active->router.event(0x15fbb8,decision);
            if(decision.pad==arena::SkipOwned) {
                ++active->held_releases;arena::Saved returned{};returned.x[0]=1;active->router.event(0x15fbd8,returned);x[0]=1;return true;
            }
            ++active->native_clears;
        }
        if(on)++active->native_acquires;else ++active->native_releases;
        if(on && (mode=="partial-connect" || mode=="partial-alias")) {
            const auto& c=admission_calls[mode=="partial-alias"?1:0];
            replay_events(admission_events,c.first,c.last);active->camera_state(c.state);x[0]=c.result;return true;
        }
        if(on && uint8_t(active->camera[0x360])) {
            arena::Saved returned{};returned.x[0]=1;active->router.event(0x15faa4,returned);
        } else replay(on?(mode=="dedup"?3:0):2);
        put(active->camera,0x34a,uint8_t(on));put(active->camera,0x360,uint8_t(on));
        x[0]=1;return true;
    }
    if(target==Chain::base+0x15fcc0){++native_updates;replay(1);x[0]=1;return true;}
    return fixture_native_call(target,x,x8);
}
static int scenario(int argc,char** argv) {
    mode=argc>1?argv[1]:"cold";native_updates=0;
    if(mode=="bad-selection")assert(!setenv("FT_ADMISSION_BAD_SELECTION","1",1));
    if(mode=="sustained-on")assert(!setenv("FT_ADMISSION_WARM_FRAME","1",1));
    storage=static_cast<std::byte*>(mmap(nullptr,0x400000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));assert(storage!=MAP_FAILED);
    auto* client=mmap(nullptr,0x700000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(client!=MAP_FAILED);client_base=uint64_t(client);
    auto f=std::make_unique<Chain>();active=f.get();
    auto saved=f->s;f->s={};f->s.sp=addr(f->stack);f->s.x[19]=addr(f->app);
    f->s.x[20]=addr(f->app)+0x20000;f->s.x[26]=addr(f->camera);f->event(0x8dc1c);f->s=saved;
    f->bootstrap_router();f->publish_worker();f->run_enter();
    f->borrowed_fixture=mode=="warm" || mode=="dedup";
    if(mode=="warm") {
        put(f->camera,0x34a,uint8_t(1));put(f->camera,0x360,uint8_t(1));
        arena::Saved update{};update.x[0]=addr(f->camera);f->router.event(0x15fcc0,update);
    }
    f->frame_start();f->frame_end();
    assert(f->router.diagnostics().capable && "default Runtime must admit cold Connect without a publication");
    assert(!f->enqueues && !f->native_acquires && !f->router.diagnostics().submitted_frame);
    if(mode=="partial-connect" || mode=="partial-alias" || mode=="missing-event" || mode=="provider-migration" || mode=="provider-loss" || mode=="association-migration" || mode=="bad-selection") {
        Client client;assert(client.connect(f->directory+"/frametop-native-portal/control.sock"));Request request;Reply reply;
        request.sequence=1;request.sent_ns=boottime();assert(client.exchange(request,reply) && reply.epoch);
        request.op=Operation::Update;request.epoch=reply.epoch;request.origin=reply.origin;request.generation=1;
        ++request.sequence;request.sent_ns=boottime();assert(client.exchange(request,reply));
        f->frame_start();f->s.x[0]=addr(f->app);f->s.x[1]=0;f->s.x[2]=addr(f->outer_stack)+0xc70;
        f->s.x[3]=addr(f->outer_stack)+0x610;f->event(0x148244);f->s.x[0]=1;f->event(0x9ee5c);f->s.x[27]=1;f->event(0x9ee98);
        if(mode=="bad-selection")f->record(0);
        f->frame_end();
        assert(f->native_acquires==1 && !f->native_releases && !f->enqueues);
        if(mode=="provider-loss")modules_present=false;
        if(mode=="association-migration")put(f->camera,0,addr(f->app)+8);
        f->frame_start();f->frame_end();
        if(mode=="bad-selection")assert(f->router.diagnostics().pending && !f->enqueues && !f->router.diagnostics().submitted_frame);
        else assert(!f->router.diagnostics().capable);
        std::fprintf(stderr,"production-cleanup-guard: uncertain original connection stays retained\n");
        f->s={};f->s.x[0]=addr(f->app);f->event(0x8b020);assert(false);
    }
    assert(run_chain(*f,mode.rfind("--aggregate-",0)==0?mode:"--repeated-cycles",true)==0);
    assert(native_updates==f->router.diagnostics().queue_success+(mode=="warm"?1:0));
    f->router.stop();arena::Saved pump{};f->router.event(0,pump);
    // Original teardown is still native, after a paired exact worker join.
    put(f->worker,8,uint64_t(0x123456));f->s={};f->s.sp=0x990000;
    f->s.x[0]=addr(f->worker);f->s.x[20]=addr(f->context);f->event(0x142704);
    f->s.sp-=0x20;f->s.x[0]=0x123456;f->s.x[20]=addr(f->worker);f->event(0x183f78);
    f->s.x[0]=0;f->event(0x183f7c);put(f->worker,8,uint64_t(0));put(f->worker,0x10,uint32_t(0));f->s.sp+=0x20;
    f->s.x[0]=1;f->event(0x142708);f->event(0x1381b8);
    f->s={};f->s.x[0]=addr(f->app);f->event(0x8b020);assert(f->s.pad==arena::SkipOwned);
    f->s={};f->s.x[0]=addr(f->camera);f->event(0x15df70);assert(f->s.pad==arena::SkipOwned);
    std::printf("PASS production Runtime() %s: %llu exact source cycles, %u native Disconnects, normal joined cleanup\n",mode.c_str(),
        static_cast<unsigned long long>(f->router.diagnostics().queue_success),f->native_releases);
    f.reset();active=nullptr;assert(!munmap(client,0x700000));assert(!munmap(storage,0x400000));
    return 0;
}
int main(int argc,char** argv) {
    assert(!scenario(argc,argv));
    // Process-global nonreused ticket IDs now differ from this Adapter's count.
    assert(!scenario(argc,argv));
}
