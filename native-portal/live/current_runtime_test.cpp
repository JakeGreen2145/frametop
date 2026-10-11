// SPDX-License-Identifier: MIT
// Actual default Runtime through stereo/settlement. Native bodies and Vulkan are
// source models; no ELF, loader execution, hardware, or service activation.
#include "stock_observer_coverage.h"
#include "dormant-inventory.private.hpp"
#include "selection.private.hpp"
#define FT_ATTACHMENT_TEST
#define FT_PRODUCTION_ADMISSION_TEST
#define FT_DORMANT_CURRENT_TEST
#define ft_native_portal_call fixture_native_call
static unsigned source_test_reads();
static uint64_t admission_scalar(const char*,uint64_t,unsigned);
#include "admission-native.private.hpp"
#include "runtime_chain_test.cpp"
#undef ft_native_portal_call
static std::string current_mode;
static unsigned updates=0;
static unsigned connect_calls=0;
static uint64_t observed_client_module=Chain::client_base;
static unsigned cleanup_calls=0;
static bool negative=false,source_fault_active=false;
static uint64_t missing_alternative=0;
static bool no_gpu=false,no_cpu=false,no_source=false;
static constexpr uint64_t bypass_tag=0x20000000;
extern "C" uint64_t ft_native_portal_original(uint64_t pc) noexcept {
    return Chain::base+pc+((pc==0x15f9e0 || pc==0x15fcc0)?bypass_tag:0);
}
extern "C" uint64_t ft_native_portal_camera_coverage() noexcept{return 0xfffff;}
extern "C" size_t ft_native_portal_installed_sites(uint32_t version,uint64_t tag,uint64_t* out,size_t capacity) {
    if(version!=FT_INSTALLED_SITES_VERSION || (tag && tag!=FT_STOCK_OBSERVER_CLIENT_TAG))return 0;
    const auto* sites=tag?modeled_client_sites:modeled_comp_sites;
    size_t count=tag?std::size(modeled_client_sites):std::size(modeled_comp_sites);
    const uint64_t omitted=missing_alternative?missing_alternative:0x161e40;
    const bool missing=(missing_alternative && bool(tag)==bool(missing_alternative&FT_STOCK_OBSERVER_CLIENT_TAG)) || (!tag && current_mode=="missing-hook");
    if(missing)--count;
    if(out && capacity>=count) {
        size_t n=0;
        for(size_t i=0;i<(tag?std::size(modeled_client_sites):std::size(modeled_comp_sites));++i)
            if(!missing || sites[i]!=omitted)out[n++]=sites[i];
        assert(n==count);
    }
    return count;
}
extern "C" bool ft_native_portal_source_modules(uint64_t* a,uint64_t* b) noexcept {
    *a=Chain::base;*b=observed_client_module;return true;
}
extern "C" bool ft_native_portal_call(uint64_t target,uint64_t* x,uint64_t x8) noexcept {
    if(target==Chain::base+0x15f9e0) {
        // Actual hooked entry is separate from its loader-issued bypass.
        arena::Saved entry{};std::memcpy(entry.x,x,31*8);entry.sp=addr(active->camera_stack)+0x360;
        entry.x[30]=Chain::base+0x9eeb4;active->router.event(0x15f9e0,entry);
        assert(entry.pad==arena::SkipOwned);std::memcpy(x,entry.x,31*8);return true;
    }
    if(target==Chain::base+0x15f9e0+bypass_tag) {
        ++active->native_acquires;
        const auto sp=addr(active->camera_stack)+0x360;
        if(uint8_t(active->camera[0x360])) {
            active->source_fact(0x15f9fc,sp-0x20,1);active->source_fact(0x15fa04,sp-0x20,1);
            active->source_fact(0x15fa60,sp-0x20,1);
            arena::Saved ret{};ret.sp=sp;ret.x[30]=x[30];ret.x[0]=1;active->router.event(0x15fa78,ret);
            x[0]=1;return true;
        }
        ++connect_calls;active->source_connect(sp,x[30],false,current_mode=="source-uncertainty"?4:0);
        x[0]=current_mode=="source-uncertainty"?0:1;return true;
    }
    if(target==Chain::base+0x15fcc0+bypass_tag) {
        ++updates;arena::Saved ret{};ret.sp=addr(active->camera_stack)+0x360;ret.x[30]=x[30];ret.x[0]=current_mode=="update-three"?0x103:current_mode.rfind("empty-",0)==0?2:0;
        const bool connected=uint8_t(active->camera[0x360]);
        active->source_fact(0x15fd30,ret.sp-0x360,connected);
        active->source_fact(0x15fd90,ret.sp-0x360,connected);
        if(connected)for(unsigned q:{1u,0u}) {
            const bool empty=current_mode.rfind("empty-",0)==0 &&
                (current_mode.rfind("empty-warm",0)!=0 || updates>1) && (current_mode!="empty-q1" || q==1);
            const int raw=source_fault_active && current_mode=="source-negative-unpaired" && q==0?0:source_fault_active?(current_mode=="source-negative-4"?4:current_mode=="source-negative-5"?5:
                current_mode=="source-negative-7"?7:3):(empty?3:0);
            active->source_metadata(ret.sp,q,false,raw,current_mode=="empty-bypass");
            if(!raw && !(source_fault_active && current_mode=="source-negative-unpaired"))active->source_metadata(ret.sp,q,true);
        }
        if(!(source_fault_active && current_mode=="source-negative-no-ret"))active->router.event(0x15fdc8,ret);
        x[0]=ret.x[0];return true; // preserve actual raw x0
    }
    if(target==Chain::base+0x8b020 || target==Chain::base+0x15df70) {
        ++cleanup_calls;
        if(negative) {
            std::fputs("CURRENT forbidden native cleanup\n",stderr);_Exit(70);
        }
    }
    return fixture_native_call(target,x,x8);
}
#include "prebootstrap_runtime_fixture.inc"
static unsigned source_test_reads(){return native_reads.load();}
extern "C" unsigned graph_test_reads(){return native_reads.load();}
static uint64_t admission_scalar(const char* name,uint64_t pc,unsigned word) {
    assert(word<100);
    for(const auto& vector:admission_vectors)if(vector.pc==pc && !std::strcmp(vector.name,name))return vector.words[word];
    std::fprintf(stderr,"missing frozen admission vector %s pc=%llx\n",name,static_cast<unsigned long long>(pc));std::abort();
}
#include "factory_arguments_fixture.inc"
#include "factory_scope_fixture.inc"
#include "shared_factory_fixture.inc"
#include "retired_wrapper_fixture.inc"
static void initializer_factory_observe(Chain &f) {
    // After renderer qualification, before 29a528 and before the initial
    // wrapper factory. The earlier direct acquisition has a clobbered x24.
    factory_scope_probe(current_mode.find("null")!=std::string::npos?
        "factory-root-2-null":"factory-root-2-positive",&f,true);
    bootstrap_factory_observe(f);
    std::fputs("INITIALIZER direct-before-wrapper=1 before-29a528=1\n",stderr);
}
#ifdef FT_FIRST_CONSUMER_TEST
#include "present_runtime_fixture.inc"
#endif
static void scenario() {
    updates=0;source_fault_active=false;
    const bool retired_bad=current_mode.rfind("retired-bad-",0)==0 || current_mode.rfind("retired-engaged-bad-",0)==0;
    const unsigned connects_before=connect_calls;
    auto f=std::make_unique<Chain>();active=f.get();f->resource_map();f->source_map();f->borrowed_fixture=true;
    f->alternative_render=current_mode;
    auto saved=f->s;f->s={};f->s.sp=addr(f->stack);f->s.x[19]=addr(f->app);
    f->s.x[20]=addr(f->app)+0x20000;f->s.x[26]=addr(f->camera);f->event(0x8dc1c);f->s=saved;
    prebootstrap_observe(*f);
    f->initial_camera_resources();
    const bool shared_chain=current_mode=="shared-chain-cold" || current_mode=="shared-chain-warm";
    bool first_consumer=false;
#ifdef FT_FIRST_CONSUMER_TEST
    first_consumer=current_mode.rfind("consumer-",0)==0;
    if(first_consumer)f->first_consumer=consumer_chain;
#endif
    const bool initializer_chain=current_mode.rfind("initializer-chain-",0)==0;
    f->bootstrap_router(false,initializer_chain?initializer_factory_observe:(shared_chain || first_consumer)?bootstrap_factory_observe:nullptr);
    if(initializer_chain) {
        // The initializer's known-member return follows publication, not the
        // earlier direct acquisition. It must still run the ordinary ledger.
        const auto before=native_reads.load();
        arena::Saved s{};s.sp=addr(f->stack)+0x100;s.x[0]=addr(f->r)+0x11e0+0xd0;
        s.x[1]=addr(f->other);s.x[30]=Chain::base+FT_INITIAL_POOL_LR;
        const auto copy=s;f->router.event(0x311648,s);assert(!std::memcmp(&copy,&s,sizeof s));
        s={};s.x[0]=addr(f->other);f->router.event(0x4b3d0,s);
        assert(native_reads.load()==before);
    }
    postbootstrap_observe(*f);
    retired_turnover(*f,true); // mandatory pre-Run stock cleanup includes sized delete
    if(current_mode=="retired-different") {retired_turnover(*f,false);retired_turnover(*f,false);}
    if(current_mode=="retired-lock-paths")for(uint64_t lr:{0x2a23b4u,0x30528cu,0x3052c4u,0x3052dcu})retired_turnover(*f,true,"",lr,0x101);
    f->publish_worker();
    std::thread owner([&]{
        f->run_enter();
        if(current_mode=="retired-repeated")for(unsigned i=0;i<8;++i)retired_turnover(*f,(i&1)==0);
        if(shared_chain || first_consumer) {
            for(unsigned c=0;c<2;++c)for(unsigned i=0;i<6;++i)for(const auto result:{"positive","null"}) {
                argument_vector=&factory_saved_vectors[c][i];
                if(i<5)factory_scope_probe("factory-root-"+std::to_string(i)+"-"+result,f.get());
                else shared_factory_sequence(*f,std::string(result)=="null"?"shared-acquire-null":"shared-initial");
            }
            argument_vector=nullptr;

            for(const auto mode:{"shared-initial","shared-unselected","shared-factory-null","shared-acquire-null",
                                 "shared-cleanup-unknown","shared-cleanup-null","shared-cleanup-known"})
                shared_factory_sequence(*f,mode);
        }
        if(retired_bad) {
            const bool engaged=current_mode.rfind("retired-engaged-",0)==0;
            if(engaged){f->frame_start();f->frame_end();assert(f->router.diagnostics().capable);}
            retired_turnover(*f,true,current_mode.substr(engaged?20:12));
            f->frame_start();f->frame_end();
            assert(!f->router.diagnostics().capable && !f->router.diagnostics().pending && !f->enqueues);
        } else if(current_mode=="bound-known-delete" || current_mode=="bound-known-destroy") {
            const auto binding=f->router.camera_binding_diagnostics();
            assert(binding.run_authenticated && binding.runtime_bound && binding.barrier_retained);
            f->s={};f->s.x[0]=addr(f->executable);
            const auto before=native_reads.load();f->event(current_mode=="bound-known-delete"?0x4b3d0:0x3003d0);
            assert(native_reads.load()==before);
            f->frame_start();f->frame_end();
            assert(!f->router.diagnostics().capable && !f->router.diagnostics().pending && !f->enqueues);
        } else {
        if(current_mode=="retired-engaged" || current_mode=="retired-settled") {
            f->frame_start();f->frame_end();assert(f->router.diagnostics().capable);
            if(current_mode=="retired-engaged")for(unsigned i=0;i<8;++i)retired_turnover(*f,(i&1)==0);
            else f->settled_observe=[](Chain& chain) {
                for(unsigned i=0;i<8;++i)retired_turnover(chain,(i&1)==0);
                chain.frame_start();chain.frame_end();assert(chain.router.diagnostics().capable && !chain.router.diagnostics().pending);
            };
        }
        const bool warm=current_mode.rfind("empty-warm",0)==0 || current_mode=="warm" || current_mode=="retired-warm" || current_mode=="shared-chain-warm" ||
            (initializer_chain && current_mode.find("-warm-")!=std::string::npos);
        if(warm) {
            put(f->camera,0x34a,uint8_t(1));put(f->camera,0x360,uint8_t(1));
            put(f->camera,0x350,uint64_t(11));put(f->camera,0x358,uint64_t(12));
        }
        arena::Saved scheduled{};scheduled.sp=addr(f->camera_stack)+0x360;scheduled.x[30]=Chain::base+0xfeed;scheduled.x[0]=addr(f->camera);
        f->router.event(0x15fcc0,scheduled);assert(scheduled.pad==arena::SkipOwned && scheduled.x[0]==(current_mode=="update-three"?0x103u:current_mode.rfind("empty-",0)==0?2u:0u));
        if(negative && !first_consumer && !no_gpu && !no_cpu && !no_source && current_mode!="retired-owned-missing-return") {
            f->frame_start();f->frame_end();assert(f->router.diagnostics().capable);
            Client client;Request request;Reply reply;
            assert(client.connect(f->directory+"/frametop-native-portal/control.sock"));
            request.sequence=1;request.sent_ns=boottime();assert(client.exchange(request,reply) && reply.epoch);
            request.op=Operation::Update;request.epoch=reply.epoch;request.origin=reply.origin;request.generation=1;
            ++request.sequence;request.sent_ns=boottime();assert(client.exchange(request,reply));
            f->frame_start();
            if(current_mode=="source-uncertainty") {
                f->s={};f->s.x[0]=addr(f->app);f->s.x[1]=0;f->s.x[2]=addr(f->outer_stack)+0xc70;f->s.x[3]=addr(f->outer_stack)+0x610;
                f->event(0x148244);f->s.x[0]=1;f->event(0x9ee5c);f->s.x[27]=1;f->event(0x9ee98);
                assert(f->native_acquires==1 && !f->enqueues);
            } else {
                source_fault_active=current_mode.rfind("source-negative-",0)==0;
                f->record(0);if(!source_fault_active)assert(f->router.diagnostics().pending);
                if(source_fault_active)assert(!f->enqueues);
                if(current_mode=="identity-migration") {
                    observed_client_module+=0x1000;assert(!setenv("FT_CURRENT_SECOND_REJECT","1",1));
                    f->record(1);assert(f->enqueues==1 && !f->router.diagnostics().queue_success);
                }
            }
            f->frame_end();assert(!f->native_releases && !f->router.diagnostics().submitted_frame);
            request.op=Operation::Release;++request.sequence;assert(client.exchange(request,reply));
            assert(reply.state==State::Draining);
        } else if((current_mode=="missing-hook" || missing_alternative)) {
            f->frame_start();f->frame_end();assert(!f->router.diagnostics().capable && !f->enqueues && !f->native_acquires);
        } else {
        if(current_mode=="stock-render") {
            f->frame_start();f->frame_end();
            // Ordinary unclaimed native rendering after engagement must not
            // create optional source debt or poison a later owned first frame.
            arena::Saved r{};r.sp=addr(f->camera_stack)+0x460;r.x[0]=addr(f->camera);r.x[30]=Chain::base+0x9f400;
            f->router.event(0x160be8,r);r.sp-=0x460;r.x[27]=addr(f->camera);r.x[0]=1;
            r.x[7]=1;f->router.event(0x160ca0,r);f->router.event(0x160ca8,r);
            double time=10;std::memcpy(&r.q[0],&time,8);r.nzcv=0x20000000;f->router.event(0x160db0,r);
            float ramp=.5f;std::memcpy(&r.q[2],&ramp,4);f->router.event(0x160e34,r);
            f->router.event(0x160e60,r);f->router.event(0x161bc0,r);
            r.x[0]=0;f->router.event(0x161be8,r);r.x[0]=addr(f->camera);f->router.event(0x161c74,r);
            r.x[24]=0;f->router.event(0x161c78,r);r.x[0]=addr(f->r);r.x[1]=addr(f->stock_wrapper);
            r.x[2]=addr(f->camera)+0xc0;r.x[7]=Chain::base+0x2993f8;f->router.event(0x161d64,r);
            f->router.event(0x161d68,r);
            r.sp+=0x460;r.x[30]=Chain::base+0x9f400;f->router.event(0x161f30,r);
        }
        const auto chain_mode=no_gpu?"--repeated-owned-no-wait":no_cpu?"--repeated-owned-no-disposal":
            no_source?"--repeated-owned-no-source":"--current-first";
        if(current_mode=="retired-owned-missing-return")assert(!setenv("FT_RETIRED_MISSING_RETURN","1",1));
        assert(run_chain(*f,chain_mode,true)==0);
        const auto d=f->router.diagnostics();
        if(first_consumer)negative=d.pending; // actual settlement decides cleanup expectation
        if(!first_consumer)assert(d.uploads[0]==1 && d.uploads[1]==1 && d.encoded[0]==1 && d.encoded[1]==1 && d.queue_success==1 && d.pending==negative);
        assert(!f->native_releases && updates);
        assert(f->modeled_acquires==f->modeled_returns+f->modeled_empty);
        if(current_mode.rfind("empty-",0)==0) {
            assert(f->modeled_empty==unsigned(current_mode=="empty-q1"?1:2));
            assert(f->modeled_returns==unsigned(warm?2:current_mode=="empty-q1"?1:0));
            std::printf("EMPTY literal=2 acquires=%u returns=%u recognized=%u stale-read-count=0\n",
                f->modeled_acquires,f->modeled_returns,f->modeled_empty);
        }
        assert(connect_calls-connects_before==unsigned(!warm));
        }
        }
        f->router.stop();arena::Saved pump{};f->router.event(0,pump);
    });owner.join();
    put(f->worker,8,uint64_t(0x123456));f->s={};f->s.sp=0x990000;
    f->s.x[0]=addr(f->worker);f->s.x[20]=addr(f->context);f->event(0x142704);
    f->s.sp-=0x20;f->s.x[0]=0x123456;f->s.x[20]=addr(f->worker);f->event(0x183f78);
    f->s.x[0]=0;f->event(0x183f7c);put(f->worker,8,uint64_t(0));put(f->worker,0x10,uint32_t(0));f->s.sp+=0x20;
    f->s.x[0]=1;f->event(0x142708);f->event(0x1381b8);
#ifdef FT_FIRST_CONSUMER_TEST
    if(first_consumer)consumer_snapshot(*f,"joined");
#endif
    if(negative)std::fprintf(stderr,"CURRENT pending cleanup negative=%s native_cleanup=%u\n",current_mode.c_str(),cleanup_calls);
    f->s={};f->s.x[0]=addr(f->app);f->event(0x8b020);assert(f->s.pad==arena::SkipOwned && f->s.x[0]==0xabcdef);
    if(negative){std::fputs("CURRENT unexpected cleanup return\n",stderr);_Exit(71);}
    f->s={};f->s.x[0]=addr(f->camera);f->event(0x15df70);assert(f->s.pad==arena::SkipOwned && f->s.x[0]==0xabcdef);
    f->source_unmap();f.reset();active=nullptr;
    if(retired_bad || current_mode.rfind("bound-known-",0)==0)std::printf("CURRENT %s refused=1 clean-teardown=1\n",current_mode.c_str());
    else if((current_mode=="missing-hook" || missing_alternative))std::puts("CURRENT missing-hook refused=1 clean-teardown=1");
    else if(first_consumer)std::printf("CURRENT %s clean-teardown=1 native_cleanup=%u\n",current_mode.c_str(),cleanup_calls);
    else std::printf("CURRENT %s stereo=1 pending-Off=1 exact-settlement=1 disconnects=0 clean-teardown=1\n",current_mode.c_str());
}
int main(int argc,char** argv){
    current_mode=argc>1?argv[1]:"cold";
    if(current_mode.rfind("missing-alt-",0)==0) {
        const bool client=current_mode.rfind("missing-alt-client-",0)==0;
        missing_alternative=std::stoull(current_mode.substr(client?19:23),nullptr,16)|(client?FT_STOCK_OBSERVER_CLIENT_TAG:0);
    }
    if(current_mode.rfind("vectors-",0)==0) {
        unsigned c=99,r=99;assert(std::sscanf(current_mode.c_str(),"vectors-%u-root-%u-",&c,&r)==2 && c<2 && r<6);
        argument_vector=&factory_saved_vectors[c][r];
        const bool null=current_mode.find("null")!=std::string::npos;
        const int result=r<5?factory_scope_probe("factory-root-"+std::to_string(r)+(null?"-null":"-positive")):
            shared_factory_probe(null?"shared-acquire-null":"shared-initial");
        std::fprintf(stderr,"VECTORS mode=%s replayed=1\n",current_mode.c_str());return result;
    }
    if(current_mode.rfind("initializer-chain-vectors-",0)==0) {
        assert(std::sscanf(current_mode.c_str(),"initializer-chain-vectors-%d-",&ancestry_case)==1 && ancestry_case>=0 && ancestry_case<2);
        argument_vector=&factory_saved_vectors[ancestry_case][2];
    }
    if(current_mode.rfind("factory-",0)==0)return factory_scope_probe(current_mode);
    if(current_mode.rfind("shared-",0)==0 && current_mode.rfind("shared-chain-",0)!=0)return shared_factory_probe(current_mode);
    if(current_mode=="prebootstrap-cause")return prebootstrap_cause();
    if(current_mode=="prebootstrap-runtime-cause")return prebootstrap_runtime_cause();
    if(current_mode=="prebootstrap-scope-guards")return prebootstrap_scope_guards();
    if(current_mode.rfind("bootstrap-",0)==0)return bootstrap_guard(current_mode);
    no_gpu=current_mode=="no-gpu" || current_mode=="initializer-chain-no-gpu";
    no_cpu=current_mode=="no-cpu" || current_mode=="initializer-chain-no-cpu";
    no_source=current_mode=="no-source" || current_mode=="initializer-chain-no-source";
    negative=current_mode.rfind("source-negative-",0)==0 || current_mode=="partial-upload" || current_mode=="rejected-enqueue" || current_mode=="actual-return" ||
        current_mode=="source-uncertainty" || current_mode=="identity-migration" || current_mode=="retired-owned-missing-return" || no_gpu || no_cpu || no_source;
#ifdef FT_FIRST_CONSUMER_TEST
    negative=negative || current_mode=="consumer-present" || current_mode=="consumer-unknown";
#endif

    if(current_mode=="partial-upload")assert(!setenv("FT_CURRENT_PARTIAL_UPLOAD","1",1));
    if(current_mode=="rejected-enqueue")assert(!setenv("FT_ADMISSION_BAD_SELECTION","1",1));
    if(current_mode=="actual-return")assert(!setenv("FT_CURRENT_BAD_RETURN","1",1));
    if(no_source)assert(!setenv("FT_SOURCE_FAULT","missing",1));
    Chain::reserve_source_vtable(); // before Runtime allocations, retained across both instances
    scenario();scenario();
}
