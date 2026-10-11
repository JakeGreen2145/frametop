// SPDX-License-Identifier: MIT
// SOURCE MODEL: real Runtime and real owner/listener threads. No vendor code.
// Saved private constants are opaque inputs, not freshly proved native paths.
#include "selection.private.hpp"
#define FT_PRODUCTION_ADMISSION_TEST
#define FT_ATTACHMENT_TEST
#define ft_native_portal_call chain_modeled_call
#include "runtime_chain_test.cpp"
#undef ft_native_portal_call
#include <condition_variable>
#include <csignal>
#include <unistd.h>

static std::atomic<unsigned> original_calls{0};
static std::atomic<long> original_tid{0};
static constexpr uint64_t handler_pc=0x13a100, returned_value=0xdecafbad;
static_assert(std::atomic<unsigned>::is_always_lock_free);
extern "C" uint64_t ft_native_portal_original(uint64_t pc) noexcept {return Chain::base+pc;}
extern "C" uint64_t ft_native_portal_camera_coverage() noexcept {return 0xfffff;}
extern "C" bool ft_native_portal_source_modules(uint64_t* a,uint64_t* b) noexcept {
    *a=Chain::base;*b=0x10000000;return true;
}
extern "C" bool ft_native_portal_call(uint64_t target,uint64_t* x,uint64_t x8) noexcept {
    if(target!=Chain::base+handler_pc)return chain_modeled_call(target,x,x8);
    // Explicit outer-only model. No camera edge and no native lifetime claim.
    assert(active && x[0]==addr(active->context));
    original_tid.store(syscall(SYS_gettid));++original_calls;
    x[0]=returned_value;
    return true;
}
static void aborted(int) {
    // Observe, never cause, the production SIGABRT. async-signal-safe output.
    const unsigned n=original_calls.load();
    if(n==0) {
        constexpr char line[]="SOURCE-MODEL at-SIGABRT original_calls=0\n";
        (void)!write(STDERR_FILENO,line,sizeof(line)-1);
    } else {
        constexpr char line[]="SOURCE-MODEL at-SIGABRT original_calls=nonzero\n";
        (void)!write(STDERR_FILENO,line,sizeof(line)-1);
    }
}
static void joined_cleanup(Chain& f) {
    // Same modeled exact native join coordinates as camera_runtime_test.cpp.
    put(f.worker,8,uint64_t(0x123456));f.s={};f.s.sp=0x990000;
    f.s.x[0]=addr(f.worker);f.s.x[20]=addr(f.context);f.event(0x142704);
    f.s.sp-=0x20;f.s.x[0]=0x123456;f.s.x[20]=addr(f.worker);f.event(0x183f78);
    f.s.x[0]=0;f.event(0x183f7c);
    put(f.worker,8,uint64_t(0));put(f.worker,0x10,uint32_t(0));f.s.sp+=0x20;
    f.s.x[0]=1;f.event(0x142708);f.event(0x1381b8);
    f.s={};f.s.x[0]=addr(f.app);f.event(0x8b020);
    assert(f.s.pad==arena::SkipOwned && f.s.x[0]==0xabcdef);
    f.s={};f.s.x[0]=addr(f.camera);f.event(0x15df70);
    assert(f.s.pad==arena::SkipOwned && f.s.x[0]==0xabcdef);
}
int main(int argc,char** argv) {
    if(argc!=2)return 64;
    const std::string mode=argv[1];
    if(mode!="blocked" && mode!="responsive" && mode!="caller-baseline")return 64;
    std::signal(SIGABRT,aborted);
    auto f=std::make_unique<Chain>();active=f.get(); // default production provider
    const auto initial=f->s;f->s={};f->s.sp=addr(f->stack);f->s.x[19]=addr(f->app);
    f->s.x[20]=addr(f->app)+0x20000;f->s.x[26]=addr(f->camera);f->event(0x8dc1c);f->s=initial;
    f->initial_camera_resources();f->bootstrap_router();f->publish_worker();
    std::mutex mutex;std::condition_variable changed;
    bool ready=false,release_owner=false;
    std::atomic<bool> listener_done{false};
    long owner_tid=0,listener_tid=0;
    std::thread owner([&] {
        f->run_enter();owner_tid=syscall(SYS_gettid);
        const auto binding=f->router.camera_binding_diagnostics();
        const auto d=f->router.diagnostics();
        assert(binding.run_authenticated && binding.runtime_bound && binding.barrier_retained);
        assert(!d.frames && !d.pending && !d.uploads[0] && !d.uploads[1]);
        assert(!f->native_acquires && !f->native_releases);
        std::fprintf(stderr,"SOURCE-MODEL bound=1 barrier_retained=1 run_authenticated=1 frames=0 uploads=0 pending=0 portal_requests=0 original_calls=0\n");
        {
            std::unique_lock<std::mutex> lock(mutex);ready=true;changed.notify_all();
            // The owner cannot progress until the listener completes, except in
            // the responsive control. This is synchronization, not a timed sleep.
            changed.wait(lock,[&]{return release_owner;});
        }
        if(mode=="responsive")while(!listener_done.load()) {
            arena::Saved pump{};f->router.event(0,pump);std::this_thread::yield();
        }
        f->router.stop();arena::Saved pump{};f->router.event(0,pump);
    });
    {std::unique_lock<std::mutex> lock(mutex);changed.wait(lock,[&]{return ready;});}
    std::thread listener([&] {
        listener_tid=syscall(SYS_gettid);
        uint64_t connection=0; // lookup is valid, no nested camera mutation
        arena::Saved request{};request.x[0]=addr(f->context);request.x[1]=uint64_t(&connection);
        request.x[30]=0x123456;request.sp=0x654321;
        std::fprintf(stderr,"SOURCE-MODEL stimulus mode=%s pc=0x13a100 owner_paused=%u\n",mode.c_str(),mode!="responsive");
        if(mode=="responsive") {
            std::lock_guard<std::mutex> lock(mutex);release_owner=true;changed.notify_all();
        }
        if(mode=="caller-baseline") {
            // Deliberate original-on-caller counterfactual, NOT repaired Runtime.
            assert(ft_native_portal_call_saved(ft_native_portal_original(handler_pc),&request));
        } else f->router.event(handler_pc,request);
        // Returning is recorded, not changed to a fixture SIGABRT. Regression
        // classification lives in the Python runner and requires exact results.
        std::fprintf(stderr,"SOURCE-MODEL returned original_calls=%u value=0x%llx selector=%llu original_on_caller=%u original_on_owner=%u preserved_lr=%u\n",
            original_calls.load(),(unsigned long long)request.x[0],(unsigned long long)request.pad,
            original_tid.load()==listener_tid,original_tid.load()==owner_tid,request.x[30]==0x123456);
        listener_done=true;
        {std::lock_guard<std::mutex> lock(mutex);release_owner=true;changed.notify_all();}
    });
    listener.join();owner.join();joined_cleanup(*f);
    active=nullptr;f.reset();
    std::fprintf(stderr,"SOURCE-MODEL clean-teardown=1\n");
    return 0;
}
