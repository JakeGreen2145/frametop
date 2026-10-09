// SPDX-License-Identifier: MIT
// Actual production router/Adapter/Evidence. Native API boundaries are modeled.
#define FT_ATTACHMENT_TEST
#define ft_native_portal_call modeled_native_portal_call
#include "runtime_chain_test.cpp"
#undef ft_native_portal_call
static unsigned constructors[2]{};
static bool fail_blend=false;
using Constructor=bool(*)(uint64_t,uint64_t*,uint64_t,uint64_t,uint64_t);
static Constructor constructor=nullptr;
extern "C" void attachment_constructor(Constructor callback) {constructor=callback;}
extern "C" bool ft_native_portal_call(uint64_t target,uint64_t *x,uint64_t x8) noexcept {
    const bool create=target==Chain::base+0x296c00 || target==Chain::base+0x294358;
    if(target==Chain::base+0x296c00)++constructors[0];
    if(target==Chain::base+0x294358) {++constructors[1];if(fail_blend)return false;}
    if(create && constructor)
        return constructor(target-Chain::base,x,addr(active->wrapper),addr(active->own),addr(active->marker));
    return modeled_native_portal_call(target,x,x8);
}
int main(int argc,char **argv) {
    const std::string mode=argc>1?argv[1]:"recover";
    auto owner=std::make_unique<Chain>();auto &f=*owner;active=&f;
    f.bootstrap_router();assert(!constructors[0] && !constructors[1]);f.publish_worker();
    std::thread worker([&] {
        f.run_enter();
        if(mode=="partial") {
            fail_blend=true;f.frame_start();f.frame_end();
            assert(constructors[0]==1 && constructors[1]==1 && !f.router.diagnostics().capable);
            fail_blend=false;
            for(unsigned i=0;i<5;++i){f.frame_start();f.frame_end();}
            assert(!f.router.diagnostics().capable && constructors[0]==1 && constructors[1]==1);
            std::puts("partial constructor failure retained without retry PASS");return;
        }
        // First seals have all native eligibility but unsupported policy. No
        // initialization may be consumed before Adapter::capability is true.
        f.policy_fault=8;
        for(unsigned i=0;i<3;++i){f.frame_start();f.frame_end();}
        assert(!constructors[0] && !constructors[1] && !f.router.diagnostics().capable);
        f.policy_fault=0;f.frame_start();f.frame_end();
        std::fprintf(stderr,"attachment recovery capable=%u constructors=%u,%u\n",
                     f.router.diagnostics().capable,constructors[0],constructors[1]);
        assert(f.router.diagnostics().capable && constructors[0]==1 && constructors[1]==1);
        // Complete the existing real private-lease/queue/settlement chain after
        // recovery. No partial state reconstruction and no constructor replay.
        assert(run_chain(f,"",true)==0);
        assert(constructors[0]==1 && constructors[1]==1);
        std::puts("unsupported policy startup recovers with exactly one initialization PASS");
    });worker.join();
}
