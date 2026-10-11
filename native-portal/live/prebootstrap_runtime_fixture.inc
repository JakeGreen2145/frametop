// SPDX-License-Identifier: MIT
// Included by the actual default Runtime fixture. Source-modeled native events.
#include "../runtime/native_stock_facts.hpp"
#include <cstdarg>
#include <sys/uio.h>
static std::atomic<unsigned> native_reads{0};
extern "C" long __real_syscall(long,...);
extern "C" long __wrap_syscall(long n,...) {
    if(n==SYS_gettid)return __real_syscall(n);
    assert(n==SYS_process_vm_readv);
    va_list args;va_start(args,n);
    const auto pid=va_arg(args,int);auto* local=va_arg(args,iovec*);const auto lc=va_arg(args,int);
    auto* remote=va_arg(args,iovec*);const auto rc=va_arg(args,int);const auto flags=va_arg(args,int);va_end(args);
    ++native_reads;return __real_syscall(n,pid,local,lc,remote,rc,flags);
}
extern "C" ssize_t __real_process_vm_readv(pid_t,const iovec*,unsigned long,const iovec*,unsigned long,unsigned long);
extern "C" ssize_t __wrap_process_vm_readv(pid_t pid,const iovec* local,unsigned long lc,const iovec* remote,unsigned long rc,unsigned long flags) {
    ++native_reads;return __real_process_vm_readv(pid,local,lc,remote,rc,flags);
}
static void prebootstrap_observe(Chain& f) {
    if(current_mode.rfind("prebootstrap-",0)!=0)return;
    auto* guard=mmap(nullptr,4096,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(guard!=MAP_FAILED);
    const auto before=native_reads.load();const auto saved=f.s;
    auto emit=[&](uint64_t pc,uint64_t pointer) {
        arena::Saved s{};s.x[0]=pointer;s.x[1]=pointer;s.sp=addr(f.stack);s.x[30]=Chain::base+FT_INITIAL_POOL_LR;
        const auto original=s;f.router.event(pc,s);assert(!std::memcmp(&original,&s,sizeof s));
    };
    const uint64_t pointer=current_mode=="prebootstrap-zero"?0:
        current_mode=="prebootstrap-reuse"?addr(f.executable):uint64_t(guard);
    if(current_mode=="prebootstrap-constructor") {
        // Complete earlier unrelated lifetime, then the normal owned lifetime.
        // No member/constructor scope exists before the renderer is qualified.
        f.begin_command(addr(f.other));emit(0x3003d0,addr(f.other));emit(0x4b3d0,addr(f.other));
    } else {
        const uint64_t pc=current_mode=="prebootstrap-destroy"?0x3003d0:
            current_mode=="prebootstrap-return"?0x311648:0x4b3d0;
        emit(pc,pointer);
    }
    f.s=saved;
    assert(native_reads.load()==before);
    assert(!f.router.diagnostics().initialized && !f.router.diagnostics().frames && !f.router.diagnostics().capable);
    assert(!munmap(guard,4096));
    std::fprintf(stderr,"PREBOOTSTRAP %s reads=0 readiness=0 saved-unchanged=1\n",current_mode.c_str());
}
static int prebootstrap_cause() {
    // Diagnose the failure through real collector/Evidence, not a copied policy.
    // The positive full Runtime regression below uses the same earlier callback.
    auto f=std::make_unique<Fixture>();NativeStockFactCollector c;arena::Bridge bridge(f->evidence.bridge_memory());
    assert(c.install_coverage(modeled_comp_sites,std::size(modeled_comp_sites)));
    assert(f->evidence.begin_dormant_stock(c.reader()));assert(f->evidence.pin(Chain::base,elf_sha256));
    arena::Saved unrelated{};unrelated.x[0]=1;
    const auto before=native_reads.load();const bool observed=c.capture(0x4b3d0,unrelated);
    assert(!f->evidence.event(0x4b3d0,unrelated));c.finish();assert(native_reads.load()==before);
    const auto earlier=c.failure_pc();c.capture(0x7fda0,f->s);
    const bool boot=f->evidence.route(bridge,0x7fda0,f->s);c.finish();
    std::fprintf(stderr,"PREBOOTSTRAP cause observed=%u earlier_stock=0x%llx first_evidence=0x%llx bootstrap=%u reads=%u\n",
        observed,(unsigned long long)earlier,(unsigned long long)f->evidence.first_failure_pc(),boot,native_reads.load()-before);
    return boot && !earlier && !f->evidence.first_failure_pc()?0:1;
}
static void postbootstrap_observe(Chain& f) {
    if(current_mode.rfind("postbootstrap-",0)!=0)return;
    const auto saved=f.s;const auto before=native_reads.load();
    if(current_mode=="postbootstrap-constructor") {
        put(f.other,8,uint64_t(0x60000000));f.begin_command(addr(f.other),0x60000000);
    } else {
        f.s={};f.s.x[0]=1;f.s.x[1]=current_mode=="postbootstrap-null-return"?0:1;
        const auto original=f.s;f.event(0x311648);assert(!std::memcmp(&f.s,&original,sizeof original));
    }
    assert(native_reads.load()==before);f.s=saved;
    std::fprintf(stderr,"POSTBOOTSTRAP %s reads=0\n",current_mode.c_str());
}
static int prebootstrap_runtime_cause() {
    auto f=std::make_unique<Chain>();active=f.get();
    const auto original=f->s;f->s={};f->s.x[0]=1;
    const auto before=native_reads.load();f->event(0x4b3d0);assert(native_reads.load()==before);
    f->s=original;f->event(0x7fda0);
    f->s.x[19]=addr(f->r);f->s.x[0]=addr(f->r)+0xec0;f->s.x[1]=0x80000;f->s.x[8]=f->s.sp+0x140;
    f->event(0x29a14c);const bool armed=f->s.x[1]==0x80080;
    f.reset();active=nullptr;
    std::fprintf(stderr,"PREBOOTSTRAP runtime bootstrap-armed=%u\n",armed);return armed?0:1;
}
static int prebootstrap_scope_guards() {
    arena::Saved s{};s.x[0]=1;
    const auto before=native_reads.load();
    auto cover=[](NativeStockFactCollector& c){assert(c.install_coverage(modeled_comp_sites,std::size(modeled_comp_sites)));};
    {
        NativeStockFactCollector c;assert(!c.capture(0x4b3d0,s)); // no installed coverage
        cover(c);c.finish();assert(!c.capture(0x7fda0,s) && c.failure_pc()==0x4b3d0);
    }
    {
        NativeStockFactCollector c;cover(c);assert(c.capture(0x4b3d0,s));
        assert(!c.capture(0x7fda0,s));c.finish();assert(!c.complete()); // nested active scope
    }
    {
        NativeStockFactCollector c;cover(c);assert(c.capture(0x4b3d0,s));
        auto reader=c.reader();uint64_t value=0xfeed;
        assert(!reader.scalar(reader.user,0x4b3d0,1,value) && value==0xfeed);
        c.finish();assert(!c.capture(0x7fda0,s)); // ignored observer grants no field and no reset
    }
    {
        NativeStockFactCollector c;cover(c);assert(c.capture(0x4b3d0,s));
        auto reader=c.reader();render::Identity id{};id.process_instance=getpid();id.renderer=0x1000000;id.renderer_generation=1;
        assert(!reader.bootstrap(reader.user,Chain::base,id,0,0,0)); // wrong actual callback
        c.finish();assert(!c.capture(0x7fda0,s));
    }
    {
        NativeStockFactCollector c;cover(c);
        std::thread foreign([&]{assert(!c.capture(0x4b3d0,s));});foreign.join();
        assert(!c.capture(0x7fda0,s));
    }
    {
        NativeStockFactCollector c;cover(c);const auto pid=fork();assert(pid>=0);
        if(!pid)_exit(c.capture(0x4b3d0,s)?1:0);
        int status=0;assert(waitpid(pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==0);
        assert(c.capture(0x4b3d0,s));c.finish();assert(!c.failure_pc());
    }
    assert(native_reads.load()==before);
    std::puts("PREBOOTSTRAP scope guards=1 reads=0");return 0;
}
static int bootstrap_guard(const std::string& mode) {
    auto f=std::make_unique<Chain>();active=f.get();
    if(mode=="bootstrap-vtable") {
        put(f->r,0,uint64_t(Chain::base+0x599c40));f->event(0x7fda0);
        put(f->r,0,uint64_t(Chain::base+0x599c38));f->event(0x7fda0);
    } else {
        if(mode=="bootstrap-constructor-fault")f->event(0x7fda0);
        else f->bootstrap_router();
        f->s={};f->s.x[0]=addr(f->executable);
        if(mode=="bootstrap-delete-renderer" || mode=="bootstrap-delete-wrapper") {
            f->s.x[0]=mode=="bootstrap-delete-renderer"?addr(f->r):addr(f->command);f->event(0x4b3d0);
        }
        else if(mode=="bootstrap-return-pool" || mode=="bootstrap-return-lr" || mode=="bootstrap-return-duplicate" || mode=="bootstrap-reset-subject") {
            put(f->other,0x50,addr(f->bootstrap_pool));put(f->other,0x738,uint64_t(0xc001));f->begin_command(addr(f->other));
            f->s={};f->s.x[0]=addr(f->bootstrap_pool)+0xd0;f->s.x[1]=addr(f->other);f->s.x[30]=Chain::base+FT_INITIAL_POOL_LR;
            if(mode=="bootstrap-return-pool")++f->s.x[0];
            if(mode=="bootstrap-return-lr")++f->s.x[30];
            f->event(0x311648);
            if(mode=="bootstrap-return-duplicate")f->event(0x311648);
            if(mode=="bootstrap-reset-subject") {
                f->s={};f->s.x[0]=f->s.x[19]=addr(f->other);f->s.x[20]=addr(f->bootstrap_pool);
                f->s.sp=addr(f->stack)+0x100;f->s.x[30]=Chain::base+FT_RECYCLE_LR;f->event(0x300b20);
                f->s.sp-=0x20;f->s.x[0]=0;f->s.x[19]=1;f->s.x[30]=Chain::base+0x300c54;f->event(0x300c54);
            }
        }
        else if(mode=="bootstrap-disposal-subject") {
            f->s={};f->s.x[0]=addr(f->r);f->s.sp=addr(f->outer_stack)+0x1000;f->s.x[30]=Chain::base+FT_BOOT_DISPATCH_LR;f->event(0x2a1e18);
            f->s.sp-=0xd0;f->s.x[19]=addr(f->r);f->s.x[1]=addr(f->command);f->event(0x2a1e3c);
            f->event(0x2a2230);f->event(0x2a2320);
            f->s.x[0]=addr(f->command);f->s.x[30]=Chain::base+FT_BOOT_RELEASE_LR;f->event(0x305260);
            f->s.sp-=0x30;f->s.x[19]=addr(f->command);f->s.x[1]=addr(f->executable);f->event(0x3052e0);
            f->s.x[0]=addr(f->bootstrap_pool)+0xd0;f->event(0x3052ec);
            f->s.x[1]=1;f->s.x[30]=Chain::base+FT_BOOT_POOL_LR;f->event(0x311648);
        }
        else if(mode=="bootstrap-known-delete")f->event(0x4b3d0);
        else if(mode=="bootstrap-known-destroy")f->event(0x3003d0);
        else if(mode=="bootstrap-constructor-fault") {
            f->s.x[0]=f->s.x[20]=addr(f->other);f->s.x[1]=addr(f->r)+0x50;
            f->s.x[2]=f->s.x[19]=addr(f->bootstrap_pool);f->s.sp=addr(f->stack)+0x100;f->event(0x301800);
            f->s.sp-=0xa0;f->s.x[19]=addr(f->other);f->s.x[30]=Chain::base+0x301804;
            f->s.x[1]=addr(f->r)+0x58;f->event(0x303d10);
            // Evidence requests the scoped constructor fields at this observer.
            f->s.sp+=0xa0;f->s.x[1]=addr(f->r)+0x50;f->event(0x301808);
        } else return 64;
        // A later valid-looking constructor must never erase a known failure.
        f->s={};f->s.sp=addr(f->stack);f->s.x[24]=addr(f->r);f->event(0x7fda0);
    }
    assert(!f->router.diagnostics().capable && !f->router.diagnostics().frames);
    f.reset();active=nullptr; // joins and flushes the actual scalar diagnostic collector
    std::fprintf(stderr,"PREBOOTSTRAP guard=%s refused=1\n",mode.c_str());return 0;
}
