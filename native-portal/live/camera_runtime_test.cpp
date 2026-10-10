// SPDX-License-Identifier: MIT
// The provider, native service, source-consumer returns and native memory writes
// are EXPLICIT FIXTURES. Runtime/Adapter/Command/Lifecycle/gate/queue are production.
#include "source_admission.hpp"
#define FT_ATTACHMENT_TEST
#include "runtime_chain_test.cpp"
#include <future>
struct ModeledProvider final:SourceAdmission {
    bool returned=true,partial=false;
    bool ready(const CameraBinding& b) noexcept override {return b.camera && b.application;}
    coordination::NativeReceipt transition(const CameraBinding&,const coordination::ActionPlan& p,uint64_t actual) noexcept override {
        coordination::NativeReceipt r;assert(actual==1);
        r.producer_revalidated=p.reattach; // explicit modeled provider/header revalidation
        for(unsigned i=0;i<2;++i)r.queues[i]={true,true,0,
            p.action==coordination::Action::Connect?coordination::QueueState::Connected:coordination::QueueState::Disconnected,
            coordination::Ownership::NewlyOwned,100+i,200+i};
        if(partial)r.queues[1]={};
        return r;
    }
    bool settled(const CameraBinding&,uint64_t watermark) noexcept override {return (returned || watermark<16) && watermark;}
};
extern "C" uint64_t ft_native_portal_original(uint64_t pc) noexcept {return Chain::base+pc;}
int main(int argc,char** argv) {
    const std::string mode=argc>1?argv[1]:"--absent";
    if(mode=="--absent") {
        auto f=std::make_unique<Chain>();active=f.get();
        f->bootstrap_router();f->publish_worker();f->run_enter();
        f->frame_start();f->frame_end();assert(!f->router.diagnostics().capable);
        std::puts("real Runtime: absent native publication/source authority refuses PASS");return 0;
    }
    ModeledProvider provider;
    auto f=std::make_unique<Chain>(mode=="--no-source"?nullptr:&provider);active=f.get();
    auto saved=f->s;f->s={};f->s.sp=addr(f->stack);f->s.x[19]=addr(f->app);
    f->s.x[20]=addr(f->app)+0x20000;f->s.x[26]=addr(f->camera);
    f->event(0x8dc1c);f->s=saved; // actual bootstrap publication BEFORE graphics/Run
    f->bootstrap_router();f->publish_worker();
    std::atomic<unsigned> stage{0};
    std::thread worker([&] {
        f->run_enter();stage=1;
        while(stage.load()<2) {arena::Saved pump{};f->router.event(0,pump);std::this_thread::yield();}
        if(mode=="--no-source") {
            f->frame_start();f->frame_end();assert(!f->router.diagnostics().capable);
            // No provider proves native detach. Cleanup is intentionally fail-stop.
            std::puts("real Runtime: published/retained owner without provider stays unadmitted PASS");
            std::fflush(stdout);_Exit(0);
        }
        f->native_acquires=f->native_releases=0; // independent modeled prelude counted below
        if(mode=="--partial-receipt") {
            provider.partial=true;f->frame_start();f->frame_end();
            Client client;assert(client.connect(f->directory+"/frametop-native-portal/control.sock"));
            Request request;Reply reply;request.sequence=1;request.sent_ns=boottime();
            assert(client.exchange(request,reply) && reply.epoch);
            request.op=Operation::Update;request.epoch=reply.epoch;request.origin=reply.origin;
            request.generation=1;++request.sequence;request.sent_ns=boottime();assert(client.exchange(request,reply));
            f->frame_start();f->s.x[0]=addr(f->app);f->s.x[1]=0;
            f->s.x[2]=addr(f->outer_stack)+0xc70;f->s.x[3]=addr(f->outer_stack)+0x610;f->event(0x148244);
            f->s.x[0]=1;f->event(0x9ee5c);f->s.x[27]=1;f->event(0x9ee98);f->frame_end();
            assert(f->native_acquires==1 && !f->native_releases);
            const auto d=f->router.diagnostics();assert(!d.uploads[0] && !d.uploads[1] && !d.capable);
            std::fprintf(stderr,"camera-cleanup-guard: partial service result is not authority\n");
            f->s={};f->s.x[0]=addr(f->app);f->event(0x8b020);assert(false);
        }
        if(mode=="--no-source-return")provider.returned=false;
        const auto chain_mode=mode=="--foreign-debt"?"--foreign-debt":mode=="--no-gpu"?"--repeated-owned-no-wait":
            mode=="--no-cpu"?"--repeated-owned-no-disposal":
            mode=="--no-source-return"?"--repeated-owned-no-source":"--repeated-cycles";
        assert(run_chain(*f,chain_mode,true)==0);
        if(mode=="--no-gpu" || mode=="--no-cpu" || mode=="--no-source-return") {
            assert(f->router.diagnostics().pending);
            std::fprintf(stderr,"camera-cleanup-guard: debt must fail-stop\n");
            f->s={};f->s.x[0]=addr(f->app);f->event(0x8b020);assert(false);
        }
        f->router.stop();arena::Saved pump{};f->router.event(0,pump);
    });
    while(stage.load()!=1)std::this_thread::yield();
    if(mode!="--no-source")for(const uint64_t pc:{0x6d6e0,0x6d6f0}) {
        arena::Saved request{};request.x[0]=addr(f->app);request.x[1]=2;request.x[30]=0x123456;
        f->router.event(pc,request); // real foreign thread, owner pumps retained FIFO
        assert(request.pad==arena::SkipOwned && request.x[0]==1 && request.x[30]==0x123456);
    }
    assert(mode=="--no-source" || (f->native_acquires==1 && f->native_releases==1));
    stage=2;worker.join();
    // The real C++ worker is joined above. Native wait/join callbacks below are
    // modeled separately and retain the exact production guard coordinates.
    put(f->worker,8,uint64_t(0x123456));f->s={};f->s.sp=0x990000;
    f->s.x[0]=addr(f->worker);f->s.x[20]=addr(f->context);f->event(0x142704);
    if(mode!="--timeout") {
        f->s.sp-=0x20;f->s.x[0]=0x123456;f->s.x[20]=addr(f->worker);f->event(0x183f78);
        f->s.x[0]=mode=="--join-error"?22:0;f->event(0x183f7c);
        put(f->worker,8,uint64_t(0));put(f->worker,0x10,uint32_t(0));f->s.sp+=0x20;
    }
    f->s.x[0]=mode=="--timeout"?0:1;f->event(0x142708);
    std::fprintf(stderr,"camera-cleanup-guard: exact native join required\n");f->event(0x1381b8);
    f->s={};f->s.x[0]=addr(f->app);f->event(0x8b020);
    assert(f->s.pad==arena::SkipOwned && f->s.x[0]==0xabcdef);
    f->s={};f->s.x[0]=addr(f->camera);f->event(0x15df70);
    assert(f->s.pad==arena::SkipOwned && f->s.x[0]==0xabcdef);
    std::puts("real Runtime: bootstrap publication, authenticated Run, foreign prelookup/nested original, 8 stereo CPU/GPU/source settlements, cleanup PASS");
}
