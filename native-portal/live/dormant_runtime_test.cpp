// SPDX-License-Identifier: MIT
// Actual Runtime, source-modeled native boundaries. No ELF or hardware execution.
// This focused fixture does not depend on ephemeral opaque-coordinate files.
#include "runtime.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main evidence_fixture_previous_main
#include "../runtime/evidence_test.cpp"
#undef main
#pragma GCC diagnostic pop
#include <condition_variable>
#include <csignal>
#include <sys/syscall.h>
using namespace native_portal::live;
struct DormantFixture:Fixture {
    Runtime router;
    alignas(4096) std::array<std::byte,4096> context{};
    alignas(4096) std::array<std::byte,8192> worker{},scheduler{};
    alignas(4096) std::array<std::byte,0x25700> app{};
    std::array<std::byte,4096> camera{},camera_stack{};
    std::array<std::byte,0x400> pool{};
    std::array<std::byte,0x80080> mapped{};
    void event(uint64_t pc){router.event(pc,s);}
    DormantFixture() {
        put(worker,0,uint64_t(base+0x5a1428));put(worker,0x48,addr(context));
        put(context,0x98,addr(worker));put(context,0x1c0,addr(scheduler));put(context,0x1a8,addr(app));
        put(scheduler,8,addr(app));put(app,0x256a0,addr(r));put(app,0x25320,addr(camera));
        put(camera,0,addr(app));put(camera,8,addr(r));
        put(r,0x1410,addr(mapped));put(resource,0xa0,addr(mapped));put(executable,0x50,addr(pool));
        assert(router.pin(base,elf_sha256));
    }
    void startup() {
        const auto initial=s;s={};s.sp=addr(stack);s.x[19]=addr(app);s.x[20]=addr(app)+0x20000;s.x[26]=addr(camera);event(0x8dc1c);
        s={};s.sp=addr(camera_stack);s.x[0]=addr(camera);s.x[1]=addr(app);s.x[30]=base+0x92fdc;
        event(0x162360);assert(s.pad==arena::SkipOwned && s.x[0]==1);s=initial;
        // Same modeled allocation, Begin and publication as Fixture and Chain.
        event(0x7fda0);s.x[19]=addr(r);s.x[0]=addr(r)+0xec0;s.x[1]=0x80000;s.x[8]=s.sp+0x140;
        event(0x29a14c);assert(s.x[1]==0x80080);put(stack,0x140,addr(resource));event(0x29a150);put(stack,0x140,uint64_t(0));
        s.x[0]=s.x[20]=addr(executable);s.sp=addr(stack)+0x100;event(0x301808);
        s.sp-=0x40;s.x[19]=addr(executable);s.x[30]=base+0x300ae0;event(0x300a08);
        s.sp-=0x30;put(stack,0x98,uint64_t(base+0x300ae0));s.x[0]=0;event(0x300a38);
        s.sp=addr(stack)+0x100;s.x[20]=addr(executable);s.x[0]=1;event(0x30180c);
        s.x[19]=addr(r);s.x[1]=addr(command);event(0x29a528);s.x[24]=addr(r);s.x[0]=1;event(0x7fdbc);
        s.x[19]=addr(worker);s.x[21]=addr(context);event(0x14099c);s.x[0]=addr(worker);event(0x140dbc);
    }
    void run() {
        s.x[0]=addr(worker);put(worker,0x10,uint32_t(syscall(SYS_gettid)));put(worker,0x14,uint32_t(0x80000000));
        // Existing source fixture stock_facts_test.cpp uses this Run coordinate.
        // This does NOT independently prove native thunk reachability.
        s.x[30]=base+0x182aa8;event(0x14e5a8);
        const auto b=router.camera_binding_diagnostics();
        assert(b.run_authenticated && b.runtime_bound && b.barrier_retained);
        const auto d=router.diagnostics();assert(!d.frames && !d.pending && !d.uploads[0] && !d.uploads[1]);
    }
    void cleanup() {
        put(worker,8,uint64_t(0x123456));s={};s.sp=0x990000;s.x[0]=addr(worker);s.x[20]=addr(context);event(0x142704);
        s.sp-=0x20;s.x[0]=0x123456;s.x[20]=addr(worker);event(0x183f78);s.x[0]=0;event(0x183f7c);
        put(worker,8,uint64_t(0));put(worker,0x10,uint32_t(0));s.sp+=0x20;s.x[0]=1;event(0x142708);event(0x1381b8);
        s={};s.x[0]=addr(app);event(0x8b020);assert(s.pad==arena::SkipOwned && s.x[0]==0xabcdef);
        s={};s.x[0]=addr(camera);event(0x15df70);assert(s.pad==arena::SkipOwned && s.x[0]==0xabcdef);
    }
};
static DormantFixture* active=nullptr;
static std::atomic<unsigned> outer_calls{0},app_calls{0},method_calls{0},bank_clears{0};
static std::atomic<long> original_tid{0};
static bool nested=false;
static constexpr uint64_t outer_pc=0x13a100,outer_value=0xdecafbad;
extern "C" uint64_t ft_native_portal_original(uint64_t pc) noexcept{return Fixture::base+pc;}
extern "C" uint64_t ft_native_portal_camera_coverage() noexcept{return 0xfffff;}
extern "C" bool ft_native_portal_source_modules(uint64_t* a,uint64_t* b) noexcept{*a=Fixture::base;*b=0x10000000;return true;}
extern "C" bool ft_native_portal_call(uint64_t target,uint64_t* x,uint64_t) noexcept {
    assert(active);
    switch(target-Fixture::base) {
    case 0x162360:x[0]=1;return true;
    case 0x8b020:case 0x15df70:x[0]=0xabcdef;return true;
    case outer_pc: {
        assert(x[0]==addr(active->context));++outer_calls;original_tid=syscall(SYS_gettid);
        if(nested){arena::Saved app{};app.x[0]=addr(active->app);app.sp=0x810000;app.x[30]=Fixture::base+0x13a3a8;
            active->router.event(0x6d6f0,app);assert(app.pad==arena::SkipOwned && app.x[0]==0);}
        x[0]=outer_value;return true;
    }
    case 0x6d6f0: {
        assert(x[0]==addr(active->app));++app_calls;
        arena::Saved method{};method.x[0]=addr(active->camera);method.sp=0x80ff00;method.x[30]=0xfeed0100; // synthetic LR; no native caller reachability claim
        active->router.event(0x15fba8,method);assert(method.pad==arena::SkipOwned && method.x[0]==0);
        x[0]=method.x[0];return true;
    }
    case 0x15fba8: {
        assert(x[0]==addr(active->camera));++method_calls;
        arena::Saved decision{};decision.x[19]=addr(active->camera);decision.sp=0x80fee0;decision.pad=arena::Original;
        active->router.event(0x15fbb8,decision);assert(decision.pad==arena::Original);
        // Modeled normal failed Release still executes its native bank-clear.
        ++bank_clears;x[0]=0;return true;
    }
    default:std::fprintf(stderr,"unexpected modeled native call 0x%llx\n",(unsigned long long)(target-Fixture::base));std::abort();
    }
}
static void aborted(int) {
    if(!outer_calls.load()){constexpr char m[]="DORMANT at-SIGABRT originals=0\n";(void)!write(2,m,sizeof(m)-1);}
}
int main(int argc,char** argv) {
    if(argc!=2)return 64;
    const std::string mode=argv[1];nested=mode=="nested";
    if(mode!="baseline" && mode!="responsive" && mode!="blocked" && !nested)return 64;
    std::signal(SIGABRT,aborted);
    auto f=std::make_unique<DormantFixture>();active=f.get();f->startup();
    std::mutex mutex;std::condition_variable cv;bool ready=false,resume=false;
    std::atomic<bool> finished{false};long owner_tid=0,caller_tid=0;
    std::thread owner([&]{
        f->run();owner_tid=syscall(SYS_gettid);
        std::fprintf(stderr,"DORMANT bound=1 zero_work=1\n");
        {std::unique_lock<std::mutex> lock(mutex);ready=true;cv.notify_all();cv.wait(lock,[&]{return resume;});}
        if(mode=="responsive")while(!finished.load()){arena::Saved pump{};f->router.event(0,pump);std::this_thread::yield();}
        f->router.stop();arena::Saved pump{};f->router.event(0,pump);
    });
    {std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return ready;});}
    std::thread caller([&]{
        caller_tid=syscall(SYS_gettid);uint64_t connection=0;
        arena::Saved s{};s.x[0]=addr(f->context);s.x[1]=uint64_t(&connection);s.sp=0x820000;s.x[30]=0x123456;
        if(mode=="responsive"){std::lock_guard<std::mutex> lock(mutex);resume=true;cv.notify_all();}
        if(mode=="baseline")assert(ft_native_portal_call_saved(ft_native_portal_original(outer_pc),&s));
        else f->router.event(outer_pc,s);
        assert(outer_calls==1 && s.x[0]==outer_value && s.x[30]==0x123456);
        assert(s.pad==(mode=="baseline"?arena::Original:arena::SkipOwned));
        if(nested)assert(app_calls==1 && method_calls==1 && bank_clears==1);
        std::fprintf(stderr,"DORMANT returned caller=%u owner=%u nested=%u\n",original_tid==caller_tid,original_tid==owner_tid,nested);
        finished=true;{std::lock_guard<std::mutex> lock(mutex);resume=true;cv.notify_all();}
    });
    caller.join();owner.join();f->cleanup();active=nullptr;f.reset();
    std::fprintf(stderr,"DORMANT clean-teardown=1\n");return 0;
}
