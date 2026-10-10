// SPDX-License-Identifier: MIT
// Modeled source-only startup schedules. No external binary inputs or device.
#include "selection.private.hpp"
#define FT_PRODUCTION_ADMISSION_TEST
#define FT_ATTACHMENT_TEST
#include "runtime_chain_test.cpp"
#include <sys/resource.h>
#include <signal.h>
extern "C" uint64_t ft_native_portal_original(uint64_t pc) noexcept {return Chain::base+pc;}
extern "C" uint64_t ft_native_portal_camera_coverage() noexcept {return 0xfffff;}
extern "C" bool ft_native_portal_source_modules(uint64_t* a,uint64_t* b) noexcept {*a=Chain::base;*b=0x10000000;return true;}
int main(int argc,char** argv) {
    rlimit zero{0,0};setrlimit(RLIMIT_CORE,&zero);
    if(argc!=3)return 64;
    signal(SIGPIPE,SIG_DFL);
    const std::string mode=argv[1];const int output=std::atoi(argv[2]);
    auto f=std::make_unique<Chain>();active=f.get();
    auto saved=f->s;f->s={};f->s.sp=addr(f->stack);f->s.x[19]=addr(f->app);
    f->s.x[20]=addr(f->app)+0x20000;f->s.x[26]=addr(f->camera);
    f->event(0x8dc1c);f->s=saved;
    f->bootstrap_router();
    f->s={};f->s.x[19]=addr(f->worker);f->s.x[21]=addr(f->context);f->event(0x14099c);
    const auto bootstrap=uint64_t(syscall(SYS_gettid));
    auto attempt=[&] {
        const auto d=f->router.diagnostics();
        assert(!d.frames && !d.uploads[0] && !d.uploads[1] && !d.pending);
        arena::Saved r{};r.sp=addr(f->camera_stack);r.x[30]=Chain::base+0x123456;r.x[1]=2;
        const uint64_t event=mode=="foreign-noncamera"?0:mode=="foreign-cleanup"?0x8b020:
            mode=="foreign-publication"?0x8dc1c:0x15fba8;
        r.x[0]=event==0x8b020?addr(f->app):addr(f->camera);
        std::printf("coordinates event=%llu current_tid=%llu bootstrap_tid=%llu\n",
            (unsigned long long)event,(unsigned long long)syscall(SYS_gettid),(unsigned long long)bootstrap);
        std::fflush(stdout);
        if(output<0)close(STDERR_FILENO);else assert(dup2(output,STDERR_FILENO)==STDERR_FILENO);
        f->router.event(event,r);
        assert(r.pad==(event==0 || event==0x8dc1c?arena::Original:arena::SkipOwned));
        if(event==0x15fba8)assert(r.x[0]==1);
        if(event==0x8b020)assert(r.x[0]==0xabcdef);
        std::puts("returned");
    };
    if(mode=="bootstrap-release")attempt();
    else {std::thread worker(attempt);worker.join();}
    active=nullptr;
    return 0;
}
