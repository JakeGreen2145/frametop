// SPDX-License-Identifier: MIT
// Actual Runtime, default source provider and gate. MockNative models only the
// service boundary. No native executable, emulator, loader or device is used.
#include "selection.private.hpp"
#define FT_PRODUCTION_ADMISSION_TEST
#define FT_ATTACHMENT_TEST
#define ft_native_portal_call fixture_native_call
#include "runtime_chain_test.cpp"
#undef ft_native_portal_call
#include <sys/resource.h>
static std::string mode;
static unsigned init_calls=0,updates=0,properties=0,lostpipes=0;
static std::atomic<uint64_t> expected_tid{0};
static std::atomic<unsigned> blocked{0};
namespace native_portal {void exhaust_camera_gate_for_test(bool epochs) noexcept;}
extern "C" uint64_t ft_native_portal_original(uint64_t pc) noexcept {return Chain::base+pc;}
extern "C" uint64_t ft_native_portal_camera_coverage() noexcept {return 0xfffff;}
extern "C" bool ft_native_portal_source_modules(uint64_t* a,uint64_t* b) noexcept {*a=Chain::base;*b=0x10000000;return true;}
extern "C" bool ft_native_portal_call(uint64_t target,uint64_t* x,uint64_t indirect) noexcept {
    const auto pc=target-Chain::base;
    if(pc==0x162360) {
        ++init_calls;
        auto cleanup=[] {arena::Saved d{};d.x[0]=addr(active->camera);active->router.event(0x15df70,d);};
        if(mode=="nested-cleanup")cleanup();
        if(mode=="concurrent-cleanup"){std::thread t(cleanup);t.join();}
        x[0]=mode=="init-failure"?0:1;return true;
    }
    if(pc==0x13a100 || pc==0x15fcc0) {
        if(mode=="partial-bind")_Exit(70); // forbidden native execution, not a test-supplied abort
        assert(uint64_t(syscall(SYS_gettid))==expected_tid.load());
        if(mode=="blocked-stock" && pc==0x13a100) {blocked=1;while(blocked==1)std::this_thread::yield();
            assert(!active->router.camera_binding_diagnostics().runtime_bound);}
        // The stock Update property calls precede its zero-demand return. The
        // inactive lostpipe executes no Release. These are explicit source models.
        if(pc==0x15fcc0){++updates;properties+=2;x[0]=0;}
        else {++lostpipes;x[0]=0xfeed;}
        return true;
    }
    return fixture_native_call(target,x,indirect);
}
static void publication(Chain& f,bool mismatch=false) {
    arena::Saved p{};p.sp=addr(f.stack);p.x[19]=addr(f.app)+(mismatch?8:0);
    p.x[20]=p.x[19]+0x20000+(mode=="bad-publication"?8:0);p.x[26]=addr(f.camera);f.router.event(0x8dc1c,p);
}
static void initialize(Chain& f) {
    arena::Saved s{};s.sp=addr(f.camera_stack);s.x[0]=addr(f.camera);s.x[1]=addr(f.app)+(mode=="mismatch"?8:0);
    s.x[30]=Chain::base+0x92fdc+(mode=="unclassified-init"?4:0);f.router.event(0x162360,s);
    assert(s.pad==arena::SkipOwned && s.x[0]==(mode=="init-failure"?0u:1u));
}
static void stock(Chain& f,uint64_t pc) {
    arena::Saved s{};for(unsigned i=0;i<31;++i)s.x[i]=0xface0000+i;
    s.sp=addr(f.camera_stack);s.x[0]=pc==0x13a100?addr(f.context):addr(f.camera);
    const auto before=s;expected_tid=uint64_t(syscall(SYS_gettid));f.router.event(pc,s);
    assert(s.pad==arena::SkipOwned && s.x[0]==(pc==0x13a100?0xfeedu:0u));
    s.pad=before.pad;s.x[0]=before.x[0];assert(!std::memcmp(&s,&before,sizeof s));
}
int main(int argc,char** argv) {
    rlimit zero{0,0};setrlimit(RLIMIT_CORE,&zero);mode=argc>1?argv[1]:"initial-success";
    auto f=std::make_unique<Chain>();active=f.get();
    const bool absent=mode=="foreign-stock" || mode=="worker-stock";
    const bool late=mode=="late";
    if(!absent && !late){publication(*f,mode=="mismatch");if(mode!="aba")initialize(*f);}
    if(mode=="repeated-init")initialize(*f);
    if(mode=="replacement-malformed") {
        arena::Saved p{};p.sp=addr(f->stack);p.x[19]=addr(f->app);p.x[20]=1;p.x[26]=addr(f->camera);
        f->router.event(0x8dc1c,p);
    }
    std::thread listener;
    if(mode=="blocked-stock") {listener=std::thread([&]{stock(*f,0x13a100);});while(blocked!=1)std::this_thread::yield();}
    if(mode=="cleanup-between") {arena::Saved s{};s.x[0]=addr(f->app);f->router.event(0x8b020,s);}
    if(mode=="foreign-stock") {std::thread t([&]{stock(*f,0x13a100);stock(*f,0x15fcc0);});t.join();assert(lostpipes==1 && updates==1 && properties==2);}
    f->bootstrap_router();f->publish_worker();
    if(mode=="start-aba") {
        arena::Saved s{};s.x[0]=addr(f->camera);f->router.event(0x15df70,s);
        publication(*f);initialize(*f);
    }
    if(mode=="exhausted" || mode=="partial-bind")exhaust_camera_gate_for_test(mode=="exhausted");
    std::atomic<unsigned> stage{0};
    std::thread owner([&] {
        if(mode=="rejected-run") {
            f->s.x[0]=addr(f->worker);put(f->worker,0x10,uint32_t(syscall(SYS_gettid)));
            put(f->worker,0x14,uint32_t(0x80000000));f->s.x[30]=Chain::base+FT_NATIVE_RUN_RETURN+4;f->event(0x14e5a8);
        } else f->run_enter();
        if(mode=="blocked-stock") {
            const auto d=f->router.camera_binding_diagnostics();
            assert(d.inflight==1 && d.run_authenticated && !d.runtime_bound && !d.join_attempts);
        }
        stage=1;while(stage==1)std::this_thread::yield();
        if(absent || mode=="rejected-run")stock(*f,0x15fcc0);
        arena::Saved pump{};f->router.event(0,pump);
        f->frame_start();f->frame_end();
        const bool capable=mode=="initial-success" || mode=="blocked-stock" || mode=="active-rebind" || mode=="bound-publication" || late;
        assert(f->router.diagnostics().capable==capable);
        auto d=f->router.camera_binding_diagnostics();
        assert(d.run_seen && d.start_scope);
        if(mode=="rejected-run")assert(!d.run_authenticated && d.run_rejection==3 && !d.join_attempts);
        else assert(d.run_authenticated);
        assert(d.runtime_bound==capable);
        if(capable)assert(d.publication_accepted && d.pending && d.native_pending && d.resource_ready &&
            d.join_attempts==1 && d.app_match==2 && d.worker_match==2 && d.context_match==2 &&
            d.epoch_published && d.barrier_retained && d.internal_bound && !d.partial && !d.join_rejection);
        if(mode=="mismatch")assert(d.app_match==1 && !d.worker_match && !d.context_match && !d.join_attempts);
        if(mode=="init-failure") {assert(d.pending && !d.resource_ready && !d.join_attempts);stock(*f,0x15fcc0);}
        if(mode=="aba" || mode=="start-aba")assert(d.join_rejection==4 && d.invalidations && !d.join_attempts);
        if(mode=="nested-cleanup" || mode=="concurrent-cleanup")assert(!d.pending && !d.resource_ready && d.invalidate_class==3);
        if(mode=="exhausted") {
            assert(d.join_attempts==1 && !d.epoch_published && !d.internal_bound && !d.partial && d.gate_rejection==5);
            stock(*f,0x15fcc0);assert(f->router.camera_binding_diagnostics().join_attempts==1);
        }
        if(mode=="partial-bind") {
            assert(d.join_attempts==1 && d.epoch_published && d.internal_bound && !d.barrier_retained && d.partial && d.first_failure==12);
            stock(*f,0x15fcc0);_Exit(71); // only the real production guard can satisfy SIGABRT
        }
        if(mode=="bound-publication"){publication(*f);_Exit(71);}
        if(mode=="active-rebind") {
            initialize(*f);f->router.event(0,pump);d=f->router.camera_binding_diagnostics();
            assert(d.runtime_bound && d.internal_bound && !d.barrier_retained && !d.resource_ready && d.invalidate_class==2);
            assert(d.join_attempts==1);f->frame_start();f->frame_end();assert(!f->router.diagnostics().capable);
        }
        assert(!f->router.diagnostics().pending && !f->native_acquires && !f->native_releases);
        f->router.stop();f->router.event(0,pump);
    });
    while(stage!=1)std::this_thread::yield();
    if(late){publication(*f);initialize(*f);} // modeled order only, not a native late path claim
    if(mode=="aba") {arena::Saved s{};s.x[0]=addr(f->camera);f->router.event(0x15df70,s);publication(*f);initialize(*f);}
    if(mode=="blocked-stock"){blocked=2;listener.join();}
    stage=2;owner.join();
    put(f->worker,8,uint64_t(0x123456));f->s={};f->s.sp=0x990000;
    f->s.x[0]=addr(f->worker);f->s.x[20]=addr(f->context);f->event(0x142704);
    f->s.sp-=0x20;f->s.x[0]=0x123456;f->s.x[20]=addr(f->worker);f->event(0x183f78);
    f->s.x[0]=0;f->event(0x183f7c);f->s.sp+=0x20;f->s.x[0]=1;f->event(0x142708);
    f->event(0x1381b8);
    f->s={};f->s.x[0]=addr(f->app);f->event(0x8b020);
    f->s={};f->s.x[0]=addr(f->camera);f->event(0x15df70);
    assert(init_calls==(absent?0u:(mode=="active-rebind" || mode=="start-aba" || mode=="repeated-init")?2u:1u));
    std::printf("PASS source Runtime %s init=%u updates=%u properties=%u lostpipes=%u\n",mode.c_str(),init_calls,updates,properties,lostpipes);
}
