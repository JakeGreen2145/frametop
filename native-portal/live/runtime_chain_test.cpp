// SPDX-License-Identifier: MIT
// Production router end-to-end. Native calls, native graph writes and Vulkan are
// modeled boundaries. No device, loader, camera, installed native bytes or SSH.
#include "runtime.hpp"
#include "command_trace.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main previous_evidence_main
#include "../runtime/evidence_test.cpp"
#undef main
#pragma GCC diagnostic pop
#include <filesystem>
#include "pump.private.hpp"
using namespace native_portal::live;
struct Chain:Fixture {
    Runtime router;
    alignas(4096) std::array<std::byte,0xe40> context_padding{};
    std::array<std::byte,4096> context{};
    alignas(4096) std::array<std::byte,8192> worker{},scheduler{};
    std::array<std::byte,0x40c00> shared_policy{};
    bool construct_mode=false;unsigned construct_fault=0;
    alignas(4096) std::array<std::byte,0xa00> app_padding{};
    std::array<std::byte,0x25700> app{};
    alignas(4096) std::array<std::byte,0x2000> outer_stack{};
    std::array<std::byte,0x1000> camera{},camera_stack{};
    std::array<std::byte,0x20> stock_wrapper{};
    std::array<std::byte,0x128> stock_resource{};
    std::array<std::byte,0x130> passes{};
    std::array<std::byte,0x560> packets{};
    std::array<std::byte,0x40> cache{};
    std::array<std::byte,0x150> attachment{};
    std::array<std::byte,0x80> pipeline_object{},sync{};
    std::array<std::byte,0x80080> mapped{};
    std::array<uint64_t,15> system_table{};
    uint64_t system_interface=0,signal_pointer=0;
    struct {uint64_t serial=42;int8_t subserial=1;uint8_t padding[7]{};} signal_value;
    std::array<float,12> origin_matrix{1,0,0,0,0,1,0,1.6f,0,0,1,0};
    unsigned enqueues=0,lookups=0,native_acquires=0,native_releases=0,policy_fault=0;
    std::string directory;
    Chain() {
        put(worker,0,uint64_t(base+0x5a1428));put(worker,0x48,addr(context));
        put(context,0x98,addr(worker));put(context,0x1c0,addr(scheduler));put(context,0x1a8,addr(app));
        put(scheduler,8,addr(app));put(app,0x256a0,addr(r));put(app,0x25320,addr(camera));
        put(camera,8,addr(r));put(camera,0x2d0,addr(stock_wrapper));
        put(stock_wrapper,8,addr(stock_resource));put(stock_resource,8,addr(r)+0x50);put(stock_resource,0xb8,uint32_t(528));
        put(r,0x1410,addr(mapped));put(resource,0xa0,addr(mapped));
        put(r,0x15a0,addr(passes));put(r,0x15a8,addr(passes)+passes.size());
        put(r,0x1478,addr(cache));put(r,0x1480,addr(cache)+cache.size());
        for(unsigned i=0;i<2;++i) {
            put(passes,i*0x98,addr(attachment));put(passes,i*0x98+0x20,uint64_t(0x1234));
            put(passes,i*0x98+0x50,addr(packets)+i*0x2b0);put(passes,i*0x98+0x58,addr(packets)+(i+1)*0x2b0);
            put(passes,i*0x98+0x68,uint32_t(0x101));
        }
        put(cache,0,uint64_t(0xa001));put(cache,8,uint64_t(0xa002));put(cache,0x10,addr(attachment));
        put(cache,0x30,uint64_t(0x1234));put(cache,0x38,uint32_t(0x101));put(attachment,0x14c,uint32_t(37));
        put(pipeline_object,0x60,uint64_t(0xa003));put(sync,0x58,uint64_t(0xa004));
        signal_pointer=addr(sync);put(executable,0x428,uint64_t(&signal_pointer));put(executable,0x430,uint64_t(&signal_pointer)+8);
        put(executable,0x50,addr(bootstrap_pool));
        put(executable,0x4f0,uint64_t(&signal_value));
        system_table[14]=base+0x1010;system_interface=uint64_t(system_table.data());
        directory=std::string(std::getenv("TMPDIR"))+"/rt-XXXXXX";assert(mkdtemp(directory.data()));
        assert(router.pin(base,elf_sha256));assert(router.start_server(directory.c_str()));
    }
    ~Chain(){std::filesystem::remove_all(directory);}
    void event(uint64_t pc) {router.event(pc,s);}
    std::array<std::byte,0x7b0> bootstrap_command{};
    std::array<std::byte,0x10> bootstrap_wrapper{};
    std::array<std::byte,0x400> bootstrap_pool{};
    void begin_command(uint64_t C) {
        s.x[0]=s.x[20]=C;s.sp=addr(stack)+0x100;event(0x301808);
        s.sp-=0x40;s.x[19]=C;s.x[30]=base+0x300ae0;event(0x300a08);
        s.sp-=0x30;put(stack,0x98,uint64_t(base+0x300ae0));s.x[0]=0;event(0x300a38);
        s.sp=addr(stack)+0x100;s.x[20]=C;s.x[0]=1;event(0x30180c);
    }
    void bootstrap_router(bool turnover=false) {
        event(0x7fda0);s.x[19]=addr(r);s.x[0]=addr(r)+0xec0;s.x[1]=0x80000;s.x[8]=s.sp+0x140;
        event(0x29a14c);assert(s.x[1]==0x80080);put(stack,0x140,addr(resource));event(0x29a150);put(stack,0x140,uint64_t(0));
        begin_command(addr(executable));
        if(turnover) {
            put(bootstrap_command,8,addr(r)+0x50);put(bootstrap_command,0x738,uint64_t(0xb001));
            put(bootstrap_command,0x50,addr(bootstrap_pool));put(bootstrap_wrapper,8,addr(bootstrap_command));
            begin_command(addr(bootstrap_command));put(r,0x1380,addr(bootstrap_wrapper));
        }
        s.x[19]=addr(r);s.x[1]=turnover?addr(bootstrap_wrapper):addr(command);event(0x29a528);
        s.x[24]=addr(r);s.x[0]=1;event(0x7fdbc);
    }
    void bootstrap_turnover(const std::string &mode,bool post_run=false) {
        // Native call/LR/SP coordinates come from bootstrap_native.py. The
        // queue and pool memory transitions here are modeled, never GPU proof.
        put(r,0x15a8,addr(passes));
        if(!post_run){s={};s.x[19]=addr(worker);s.x[21]=addr(context);event(0x14099c);}
        s={};s.x[0]=addr(r);s.sp=addr(outer_stack)+0x1000;s.x[30]=base+FT_BOOT_DISPATCH_LR;
        if(mode=="--publication-alternate")s.x[30]=base+0x9e6e0;
        if(mode=="--bootstrap-bad-caller")s.x[30]+=4;
        if(mode=="--bootstrap-bad-renderer")s.x[0]+=8;
        event(0x2a1e18);
        const auto dispatch=s;
        s.sp-=0xd0;s.x[0]=0xbeef;s.x[1]=addr(bootstrap_command);s.x[30]=base+FT_BOOT_QUEUE_LR;
        const auto queue=s;event(0x3018b0);assert(!std::memcmp(&queue,&s,sizeof s));
        s.x[19]=addr(bootstrap_command);s.x[0]=0xa005;event(0x3021d4);
        s.x[0]=0;event(0x3021d8);assert(!router.diagnostics().queue_success);
        s=dispatch;s.sp-=0xd0;s.x[19]=addr(r);event(0x2a2230);event(0x2a2320);
        put(r,0x1380,uint64_t(0));s.x[0]=addr(bootstrap_wrapper);s.x[30]=base+FT_BOOT_RELEASE_LR;
        if(mode=="--bootstrap-bad-generation")put(bootstrap_command,0x738,uint64_t(0xb002));
        if(mode=="--bootstrap-bad-release")s.x[30]+=4;
        event(0x305260);
        if(mode=="--bootstrap-turnover")assert(router.diagnostics().initialized);
        s.sp-=0x30;s.x[0]=addr(bootstrap_pool)+0xd0;s.x[1]=addr(bootstrap_command);
        s.x[19]=addr(bootstrap_wrapper);s.x[30]=base+FT_BOOT_POOL_LR;
        if(mode=="--bootstrap-bad-pool")s.x[30]+=4;
        if(mode!="--publication-phase")event(0x311648);
        if(mode=="--publication-unbegun")put(command,8,addr(bootstrap_command));
        if(mode=="--publication-read")put(command,8,uint64_t(1));
        if(mode=="--publication-null-vk")put(executable,0x738,uint64_t(0));
        if(mode=="--publication-handle")put(executable,0x738,uint64_t(0xc099));
        if(mode=="--publication-owner")put(executable,8,uint64_t(1));
        if(mode=="--publication-member")put(command,8,addr(other));
        s.sp+=0x30;s.x[19]=addr(r);s.x[0]=addr(command);
        if(mode=="--publication-sp")++s.sp;
        event(0x2a23f4);put(r,0x1380,addr(command));
        if(post_run) {
            // Later ignored publications cannot overwrite the first refusal.
            const bool good=mode=="--publication-good" || mode=="--publication-alternate" || mode=="--publication-pool-live";
            if(!good){s.x[0]=1;event(0x2a23f4);}
            // Probe actual capability through a frame; diagnostics.initialized
            // in the exported ABI is the initial attach snapshot, not Evidence.
            frame_start();frame_end();
            assert(router.diagnostics().capable==good);
            std::this_thread::sleep_for(std::chrono::milliseconds(600));return;
        }
        if(mode=="--bootstrap-bad-reset" || mode=="--bootstrap-bad-begin") {
            s.x[0]=addr(bootstrap_command);event(mode=="--bootstrap-bad-reset"?0x300b20:0x300a08);
        }
        if(mode!="--bootstrap-turnover") {
            s={};s.x[0]=addr(worker);s.x[21]=addr(context);event(0x140dbc);
            std::thread attempt([&]{run_enter();frame_start();frame_end();});attempt.join();
            assert(!router.diagnostics().capable && !router.diagnostics().frames);return;
        }
        assert(router.diagnostics().initialized);
        put(r,0x15a8,addr(passes)+passes.size());
        s={};s.x[0]=addr(worker);s.x[21]=addr(context);event(0x140dbc);
    }
    void stock_pool_return(const std::string &mode) {
        // Trial 14 returns first through the initializer, then through the stock
        // tail on Run. Pinned instructions derive both LRs; identity is relocated.
        s={};s.sp=addr(stack)+0x100;s.x[0]=addr(bootstrap_pool)+0xd0;s.x[1]=addr(executable);
        s.x[30]=base+FT_STOCK_POOL_LR;
        const char *lr=std::getenv("FT_TEST_INITIAL_POOL_LR");
        if(lr || !mode.empty())
            s.x[30]=base+(lr?std::strtoull(lr,nullptr,0):FT_INITIAL_POOL_LR);
        if(mode=="--publication-pool-lr")s.x[30]+=4;
        if(mode=="--publication-pool-pool")s.x[0]+=8;
        if(mode=="--publication-pool-device")put(executable,8,addr(r)+0x58);
        if(mode=="--publication-pool-handle")put(executable,0x738,uint64_t(0xc099));
        event(0x311648);
        put(executable,8,addr(r)+0x50);put(executable,0x738,uint64_t(0xc001));
        if(mode=="--publication-pool-delete" || mode=="--publication-pool-clear") {
            s.x[0]=addr(executable);event(mode=="--publication-pool-delete"?0x4b3d0:0x3003d0);
        }
    }
    void stock_pool_recycle(const std::string &mode,uint64_t C=0) {
        s={};s.x[0]=s.x[19]=C?C:addr(executable);s.x[20]=addr(bootstrap_pool);
        s.sp=addr(stack)+0x100;s.x[30]=base+FT_RECYCLE_LR;
        if(mode=="--publication-pool-no-reset") {event(0x300a08);return;}
        if(mode=="--publication-pool-reset-lr")s.x[30]+=4;
        event(0x300b20);
        s.sp-=0x20;s.x[0]=mode=="--publication-pool-reset-failed"?uint32_t(-4):0;event(0x300c54);
        if(mode=="--publication-pool-no-begin")return;
        s.sp+=0x20;s.x[0]=s.x[19];event(0x300a08);
        s.sp-=0x30;s.x[0]=mode=="--publication-pool-begin-failed"?uint32_t(-4):0;event(0x300a38);
    }
    void stock_turnover(uint64_t C,uint64_t W,uint64_t next) {
        // Empty stock dispatch, current transfer, wrapper release/pool return,
        // and actual replacement publication coordinates. No owned settlement.
        put(r,0x15a8,addr(passes));
        s={};s.x[0]=addr(r);s.sp=0x900000;s.x[30]=base+FT_BOOT_DISPATCH_LR;event(0x2a1e18);
        s.sp-=0xd0;s.x[19]=addr(r);event(0x2a2230);event(0x2a2320);
        put(r,0x1380,uint64_t(0));s.x[0]=W;s.x[30]=base+FT_BOOT_RELEASE_LR;event(0x305260);
        s.sp-=0x30;s.x[0]=addr(bootstrap_pool)+0xd0;s.x[1]=C;s.x[19]=W;s.x[30]=base+FT_BOOT_POOL_LR;event(0x311648);
        s.sp+=0x30;s.x[19]=addr(r);s.x[0]=next;event(0x2a23f4);put(r,0x1380,next);
    }
    void repeated_stock(const std::string &fault="") {
        // Start with executable published and bootstrap_command returned. Repeat
        // the trial-12 published -> reset/Begin -> stock return -> re-publication
        // schedule, not merely the initial unpublished pool cycle.
        stock_pool_recycle("",addr(bootstrap_command));
        for(unsigned cycle=0;cycle<12;++cycle) {
            stock_turnover(addr(executable),addr(command),addr(bootstrap_wrapper));
            stock_pool_recycle(cycle==11?fault:"");
            stock_pool_return("");
            stock_pool_recycle("");
            stock_turnover(addr(bootstrap_command),addr(bootstrap_wrapper),addr(command));
            stock_pool_recycle("",addr(bootstrap_command));
            frame_start();frame_end();
            assert(router.diagnostics().capable==(fault.empty() || cycle!=11));
        }
        if(fault.empty())std::puts("12 complete published stock pool cycles PASS; native boundaries modeled");
    }
    void publish_worker() {
        s.x[19]=addr(worker);s.x[21]=addr(context);event(0x14099c);s.x[0]=addr(worker);event(0x140dbc);
    }
    void stock_append() {
        s={};s.x[19]=addr(r);s.x[20]=528;s.x[21]=addr(stock_wrapper)+0x18;
        event(0x299110);assert(s.pad==arena::Replacement);put(r,0x1418,uint32_t(576));
    }
    void geometry_classifier() {
        s={};s.x[21]=addr(r);s.x[24]=addr(packets);s.x[3]=0;
        put(packets,0x1bc,uint32_t(0));put(packets,0x1c0,uint32_t(128));
        event(0x29f358);assert(s.pad==arena::Replacement);
    }
    void stock_classifier() {
        s={};s.x[0]=addr(r);s.x[2]=addr(stock_wrapper);s.x[4]=UINT32_MAX;
        s.x[30]=base+0x29b3c0;event(0x2988b0);assert(s.pad==arena::Original);
        s.x[4]=0;event(0x2988b0);
        std::fprintf(stderr,"stock classifier route=%llu\n",(unsigned long long)s.pad);
        assert(s.pad==arena::Replacement);
    }
    void run_enter() {
        s.x[0]=addr(worker);
        put(worker,0x10,uint32_t(syscall(SYS_gettid)));put(worker,0x14,uint32_t(0x80000000));s.x[30]=base+FT_NATIVE_RUN_RETURN;event(0x14e5a8);
    }
    void pump_event(unsigned i) {
        const auto &p=pump_events[i];s={};s.sp=0x990000;
        s.x[0]=p[1]?addr(context):p[2]?addr(worker):0;
        s.x[1]=p[3];s.x[20]=p[4]?addr(context):0;s.x[30]=base+p[5];event(p[0]);
    }
    void construct_publish() {
        put(context,0x1b0,addr(app));
        put(shared_policy,0x4027c,float(1));put(shared_policy,0x40280,float(1));
        put(outer_stack,0x160,addr(shared_policy)); // policy native SP+60
        s={};s.sp=addr(camera_stack);s.x[0]=addr(shared_policy);s.x[23]=addr(app);
        s.x[3]=addr(worker)+0xf88;s.x[2]=construct_fault==2?1:construct_fault==3?255:0;
        const float one=1;std::memcpy(s.q[3],&one,4);std::memcpy(s.q[2],&one,4);
        if(construct_fault==4)++s.x[23];
        if(construct_fault==5)++s.x[3];
        if(construct_fault==6)put(shared_policy,0x4027c,std::numeric_limits<float>::quiet_NaN());
        if(construct_fault==7)put(shared_policy,0x4027c,float(0));
        if(construct_fault==8)put(shared_policy,0x4027c,float(2));
        if(construct_fault==9)put(shared_policy,0x40280,std::numeric_limits<float>::infinity());
        if(construct_fault!=1)event(0x11a3a4);
        if(construct_fault==10)put(shared_policy,0x4027c,float(.5));
        if(construct_fault>=11 && construct_fault<=16)put(shared_policy,0x402b8+8*(construct_fault-11),uint64_t(99));
        if(construct_fault==17)put(shared_policy,0x402b4,uint32_t(99));
        if(construct_fault==18)put(outer_stack,0x160,uint64_t(1));
    }
    void frame_start(uint64_t owner=0) {
        if(construct_mode)construct_publish();
        s={};s.sp=addr(outer_stack);s.x[0]=owner?owner:addr(worker);s.x[30]=base+FT_OUTER_LR;event(0x147a48);
        s.sp=addr(outer_stack)+0x100;s.x[19]=addr(outer_stack)+0x610;
        if(construct_mode)s.x[20]=addr(app);
        s.x[3]=0;if(policy_fault!=8)event(0x112ab4);
        const auto forced=uint32_t(policy_fault==11);
        s.x[0]=forced;if(policy_fault!=7)event(0x112ac4);
        s.x[1]=1;s.x[2]=construct_mode?0:1;event(0x112aec);
        if(construct_mode){const float one=1;std::memcpy(s.q[0],&one,4);event(0x112af4);std::memset(s.q[0],0,16);}
        if(policy_fault==6){s.x[1]=0;event(0x112aec);}
        event(construct_mode || policy_fault==9?0x112c70:0x112afc);
        if(construct_mode || policy_fault==9){s.x[0]=construct_fault==19;event(0x112c78);}
        if(!construct_mode){s.x[1]=policy_fault==10;event(0x112c10);}
        s.x[0]=forced;if(policy_fault!=7)event(0x112b10);
        s.x[1]=forced;if(policy_fault!=7)event(0x112c50);
        event(0x112d2c);event(0x112d90);
        s.x[0]=0;event(0x112cfc);s.x[23]=forced;if(policy_fault!=7)event(0x112d14);
        put(outer_stack,0x610+0x1e0,uint8_t(policy_fault==12?2:0));
        put(outer_stack,0x610+0x1e1,uint8_t(policy_fault==13?2:0));
        put(outer_stack,0x610+0x62c,uint8_t(policy_fault==14));
        put(outer_stack,0x610+0x630,uint32_t(policy_fault==16?2:1));
        put(outer_stack,0x610+0x648,float(policy_fault==17?1:0));s.x[23]=policy_fault==15;
        if(construct_mode && construct_fault==19) {
            put(outer_stack,0x610+0x62c,uint8_t(1));put(outer_stack,0x610+0x630,uint32_t(2));
            put(outer_stack,0x610+0x648,float(1));s.x[23]=1;
        }
        void *policy_memory=MAP_FAILED;
        if(policy_fault>=1 && policy_fault<=5) {
            constexpr unsigned offsets[]={0,0x1e0,0x1e1,0x62c,0x630,0x648};
            policy_memory=mmap(nullptr,8192,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
            assert(policy_memory!=MAP_FAILED);
            assert(!mprotect(static_cast<std::byte*>(policy_memory)+4096,4096,PROT_NONE));
            s.x[19]=uint64_t(policy_memory)+4096-offsets[policy_fault];
        }
        if(policy_fault!=18)event(0x112b58);
        if(policy_memory!=MAP_FAILED)assert(!munmap(policy_memory,8192));
        std::array<std::byte,80> pose{};put(pose,0x48,uint32_t(200));put(pose,0x4c,uint16_t(0x101));s.x[3]=addr(pose);event(0x63920);
        s.sp=addr(outer_stack);event(0x1480b8);
    }
    void frame_end(){s.sp=addr(outer_stack);event(0x148de0);}
    void record(unsigned eye) {
        s={};s.sp=addr(outer_stack);s.x[0]=addr(app);s.x[1]=eye;s.x[2]=addr(outer_stack)+0xc70;s.x[3]=addr(outer_stack)+0x610;
        event(eye?0x14834c:0x148244);
        if(s.x[3]==addr(outer_stack)+0x610)std::fprintf(stderr,"unclaimed eye %u refusal=%u pending=%u capable=%u\n",eye,unsigned(router.diagnostics().refusal),router.diagnostics().pending,router.diagnostics().capable);
        assert(s.x[3]!=addr(outer_stack)+0x610);
        s.x[0]=1;event(0x9ee5c);s.x[27]=1;event(0x9ee98);
        s.x[23]=0;event(0x9f404);assert(s.x[23]==1);
        // The counterfactual modifies only the owned copy, never native output,
        // local enableConstructRoomView, shared mailboxes, or fade publication.
        assert(outer_stack[0x610+0x62c]==std::byte{0} && app[0x918]==std::byte{0});
        if(construct_mode) {
            float scalar=0;std::memcpy(&scalar,shared_policy.data()+0x4027c,4);assert(scalar==1);
            assert(shared_policy[0x40b45]==std::byte{0} && shared_policy[0x40b49]==std::byte{0});
        }
        // Native layer cache post-store, modeled only after actual transition callback.
        put(app,0x2568c,uint8_t(1));
        for(unsigned i=0;i<16;++i) {put(camera,0x100+i*4,float(i%5==0));put(camera,0x140+i*4,float(0));}
        put(camera,0x140,float(1));put(camera,0x154,float(1));put(camera,0x168,float(-1));put(camera,0x16c,float(-.1));put(camera,0x178,float(-1));
        std::memcpy(camera_stack.data()+0x170,origin_matrix.data(),48);
        put(camera_stack,0x308,addr(marker));put(camera_stack,0x320,uint32_t(57));put(camera_stack,0x330,uint32_t(125));
        s={};s.sp=addr(camera_stack);s.x[0]=addr(r);s.x[1]=addr(stock_wrapper);s.x[2]=addr(camera)+0xc0;s.x[7]=base+0x2993f8;s.x[27]=addr(camera);
        event(0x161d64);if(s.pad!=arena::Replacement)std::fprintf(stderr,"upload refusal %u eye %u\n",unsigned(router.diagnostics().refusal),eye);
        assert(s.pad==arena::Replacement);const auto upload=s;
        uint32_t cursor=0;std::memcpy(&cursor,r.data()+0x1418,4);
        arena::Saved admission{};admission.x[19]=addr(r);admission.x[0]=cursor;admission.x[20]=592;
        admission.x[21]=addr(wrapper)+0x18;admission.x[23]=0x808;admission.x[24]=8;admission.x[25]=s.x[2];
        router.event(0x299110,admission);assert(admission.pad==arena::Replacement);
        std::memcpy(mapped.data()+cursor,reinterpret_cast<void*>(s.x[2]),592);put(wrapper,0x18,cursor);put(r,0x1418,uint32_t(cursor+640));
        s=upload;event(0x161d68);
        s.x[0]=addr(r);s.x[1]=s.sp+0x300;s.x[22]=s.x[1];s.x[2]=base+0x450650;s.x[4]=base+0x29b278;event(0x161ed8);
        assert(s.pad==arena::SkipOwned && enqueues==eye+1);
    }
};
static Chain *active=nullptr;
extern "C" uint64_t ft_pipeline_begin(void *,uint64_t generation,uint32_t vs,uint32_t ps) {assert(generation==73 && vs==57 && ps==125);return 99;}
extern "C" int ft_pipeline_end(uint64_t token){assert(token==99);return 1;}
struct TestRecord {uint32_t abi,vs,ps,reserved;uint64_t generation,pipeline,ov,of,pv,pf;};
extern "C" int ft_pipeline_approved(void *,uint64_t generation,void *pipeline,uint32_t vs,uint32_t ps,TestRecord *out) {
    assert(generation==73 && uint64_t(pipeline)==0xa003);*out={1,vs,ps,0,generation,0xa003,1,2,3,4};return 1;
}
extern "C" bool ft_native_portal_call(uint64_t target,uint64_t *x,uint64_t x8) noexcept {
    assert(active);auto &f=*active;
    switch(target-Chain::base) {
    case 0x4b1a0:x[0]=1;break;
    case 0x4c450:*reinterpret_cast<int32_t*>(x[1])=0;x[0]=uint64_t(&f.system_interface);break;
    case 0x1010:assert(x8);std::memcpy(reinterpret_cast<void*>(x8),f.origin_matrix.data(),48);break;
    case 0x296c00:x[0]=addr(f.wrapper);break;case 0x294358:x[0]=addr(f.marker);break;
    case 0x15f9e0:++f.native_acquires;put(f.camera,0x34a,uint8_t(1));put(f.camera,0x360,uint8_t(1));x[0]=1;break;
    case 0x15fba8:++f.native_releases;put(f.camera,0x34a,uint8_t(0));put(f.camera,0x360,uint8_t(0));break;
    case 0x2988a8:{uint32_t offset=0;std::memcpy(&offset,f.wrapper.data()+0x18,4);render::Descriptor d{0x999,offset,592};std::memcpy(reinterpret_cast<void*>(x[1]),&d,24);break;}
    case 0x29b278:{const auto off=f.enqueues++*0x2b0;assert(f.enqueues<=2);
        std::memcpy(f.packets.data()+off,reinterpret_cast<void*>(x[1]),352);
        put(f.packets,off+0x160,addr(f.packets)+off+0x170);put(f.packets,off+0x168,uint64_t(15));
        std::memcpy(f.packets.data()+off+0x170,reinterpret_cast<void*>(x[2]),16);
        uint32_t offset=0;std::memcpy(&offset,f.wrapper.data()+0x18,4);render::Descriptor d{0x999,offset,592};
        std::memcpy(f.packets.data()+off+0x1c8,&d,24);std::memcpy(f.packets.data()+off+0x1c8+4*24,&d,24);break;}
    case 0x29dac0:++f.lookups;x[0]=addr(f.pipeline_object);break;
    default:std::fprintf(stderr,"unmodeled native call %llx\n",(unsigned long long)(target-Chain::base));std::abort();
    }
    return true;
}
int run_chain(Chain &f,const std::string &mode,bool entered=false) {
    if(std::getenv("FT_TEST_CONSTRUCT"))f.construct_mode=true;
    if(mode=="--worker") {
        // Each invalid Run must leave the waiting bootstrap scope untouched.
        // Restore one modeled native field at a time, then admit the real worker.
        auto rejected=[&] {
            f.event(0x14e5a8);
            arena::Saved probe{};probe.x[0]=addr(f.r);probe.x[2]=addr(f.stock_wrapper);
            const auto before=probe;f.router.event(0x2988b0,probe);
            assert(!std::memcmp(&probe,&before,sizeof probe));
        };
        f.s={};f.s.x[0]=addr(f.worker);f.s.x[30]=Chain::base+FT_NATIVE_RUN_RETURN;
        put(f.worker,0x10,uint32_t(syscall(SYS_gettid)));put(f.worker,0x14,uint32_t(0x80000000));
        f.s.x[30]+=4;rejected();f.s.x[30]-=4;
        f.s.x[0]+=8;rejected();f.s.x[0]-=8;
        put(f.worker,0x10,uint32_t(0));rejected();put(f.worker,0x10,uint32_t(syscall(SYS_gettid)));
        put(f.worker,0x14,uint32_t(0));rejected();put(f.worker,0x14,uint32_t(0x80000000));
        put(f.worker,0x48,uint64_t(0));rejected();put(f.worker,0x48,addr(f.context));
        put(f.app,0x256a0,uint64_t(0));rejected();put(f.app,0x256a0,addr(f.r));
        put(f.r,0xa78,uint64_t(0));rejected();put(f.r,0xa78,uint64_t(0x1234));
    }
    if(!entered)f.run_enter();
    if(mode=="--worker") {
        std::thread foreign([&] {
            arena::Saved probe{};probe.x[0]=addr(f.r);probe.x[2]=addr(f.stock_wrapper);
            const auto before=probe;f.router.event(0x2988b0,probe);
            assert(!std::memcmp(&probe,&before,sizeof probe));
            probe.x[0]=addr(f.worker);probe.x[30]=Chain::base+FT_NATIVE_RUN_RETURN;
            f.router.event(0x14e5a8,probe); // consumed handoff cannot be claimed again
        });foreign.join();
        // Stock startup before Adapter creation or any owned lease. Sentinel
        // proves scope handoff separately from the ordinary descriptor borrow.
        f.stock_classifier();f.geometry_classifier();
    }
    if(mode.rfind("--association-",0)==0) {
        const auto fault=unsigned(std::stoul(mode.substr(14)));
        void *protected_page=nullptr;
        if(fault==1)protected_page=f.worker.data();
        if(fault==2)put(f.worker,0x48,addr(f.app));
        if(fault==3)protected_page=reinterpret_cast<void*>(addr(f.context)&~uint64_t(4095));
        if(fault==4 || fault==9 || fault==10 || fault==11 || fault==17)put(f.context,0x1a8,uint64_t(0));
        if(fault==4)put(f.scheduler,8,uint64_t(0));
        if(fault==5 || fault==7) {
            assert((addr(f.app)&4095)==0xa00);
            protected_page=reinterpret_cast<void*>((addr(f.app)&~uint64_t(4095))+(fault==5?0x25000:0x26000));
        }
        if(fault==6)put(f.app,0x25320,uint64_t(0));
        if(fault==9) {
            assert((addr(f.context)&4095)==0xe40);
            protected_page=reinterpret_cast<void*>((addr(f.context)+0x1c0)&~uint64_t(4095));
        }
        if(fault==10)put(f.context,0x1c0,addr(f.worker));
        if(fault==11)protected_page=f.scheduler.data();
        if(fault==12)put(f.context,0x1a8,uint64_t(1)); // nonnull must not try scheduler
        if(fault==14)put(f.app,0x256a0,uint64_t(1));
        if(fault==16)put(f.r,0xa78,uint64_t(1));
        if(fault==17)put(f.scheduler,8,uint64_t(1)); // fallback app mismatch
        if(protected_page)assert(!mprotect(protected_page,4096,PROT_NONE));
        f.frame_start(fault==8?1:0);f.frame_end();
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        assert(!f.router.diagnostics().capable && !f.router.diagnostics().stopped);
        if(protected_page)assert(!mprotect(protected_page,4096,PROT_READ|PROT_WRITE));
        return 0;
    }
    if(mode=="--policy-native-disabled" || mode=="--policy-native-special") {
        const bool special=mode=="--policy-native-special";
        // Both native branches bypass request, for different proved reasons.
        f.s={};f.s.sp=addr(f.outer_stack);f.s.x[0]=addr(f.worker);f.s.x[30]=Chain::base+FT_OUTER_LR;f.event(0x147a48);
        f.s.sp+=0x100;f.s.x[19]=addr(f.outer_stack)+0x610;f.s.x[20]=addr(f.app);
        f.s.x[3]=0;f.event(0x112ab4);f.s.x[0]=0;f.event(0x112ac4);
        f.s.x[1]=special;f.s.x[2]=special?0:1;f.event(0x112aec);
        if(special) {
            const float positive=1;std::memcpy(f.s.q[0],&positive,4);f.event(0x112af4);
            f.event(0x112c70);f.s.x[0]=0;f.event(0x112c78);
        } else f.event(0x112afc);
        f.event(0x112b10);f.s.x[1]=0;f.event(0x112c50);
        std::memset(f.s.q[0],0,16);f.event(0x112d2c);f.event(0x112d90);
        f.s.x[0]=special;f.event(0x112cfc);f.s.x[23]=0;if(!special)f.event(0x112d14);
        put(f.outer_stack,0x610+0x630,uint32_t(1));f.event(0x112b58);
        std::array<std::byte,80> pose{};put(pose,0x48,uint32_t(200));put(pose,0x4c,uint16_t(0x101));f.s.x[3]=addr(pose);f.event(0x63920);
        f.s.sp=addr(f.outer_stack);f.event(0x1480b8);f.frame_end();
        assert(f.router.diagnostics().capable==!special); // readiness, NOT enabled/owned output
        assert(!f.router.diagnostics().pending && !f.native_acquires && !f.native_releases);
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        return 0;
    }
    if(mode.rfind("--policy-",0)==0) {
        f.policy_fault=unsigned(std::stoul(mode.substr(9)));
        f.frame_start();f.frame_end();std::this_thread::sleep_for(std::chrono::milliseconds(600));
        assert(!f.router.diagnostics().pending && !f.native_acquires && !f.native_releases);
        return 0;
    }
    if(mode.rfind("--construct",0)==0) {
        f.construct_mode=true;
        if(mode=="--construct-recover") {
            f.construct_fault=2;f.frame_start();f.frame_end();assert(!f.router.diagnostics().capable);
            f.construct_fault=0;
        }
        if(mode.rfind("--construct-fault-",0)==0) {
            f.construct_fault=unsigned(std::stoul(mode.substr(18)));
            f.frame_start();f.frame_end();assert(!f.router.diagnostics().capable);
            assert(!f.router.diagnostics().pending && !f.native_acquires);return 0;
        }
    }
    if(mode=="--application-fallback")put(f.context,0x1a8,uint64_t(0));
    if(mode=="--application-direct")put(f.context,0x1c0,uint64_t(1)); // native direct branch never reads scheduler
    if(mode=="--diagnostics-bad")put(f.app,0x25320,uint64_t(0));
    f.frame_start();f.frame_end();
    if(mode=="--diagnostics-good" || mode=="--diagnostics-bad") {
        // Exercise the actual production collector, without logging from callbacks.
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        for(unsigned i=0;i<100;++i) {f.frame_start();f.frame_end();}
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        assert(f.native_acquires==0 && f.native_releases==0);
        return 0;
    }
    if(mode=="--unsafe-cleanup") {std::fprintf(stderr,"cleanup-entry: active owned borrow, no join\n");f.event(0x1381b8);assert(false);}
    if(mode=="--joined-cleanup" || mode=="--join-error-cleanup" || mode=="--timeout-cleanup") {
        f.pump_event(2);f.frame_start();f.frame_end();assert(f.router.diagnostics().stopped);
        put(f.worker,8,uint64_t(0x123456));f.s={};f.s.sp=0x990000;f.s.x[0]=addr(f.worker);f.s.x[20]=addr(f.context);f.event(0x142704);
        if(mode!="--timeout-cleanup") {
        f.s.sp-=0x20;f.s.x[0]=0x123456;f.s.x[20]=addr(f.worker);f.event(0x183f78);
        f.s.x[0]=mode=="--joined-cleanup"?0:22;f.event(0x183f7c);
        put(f.worker,8,uint64_t(0));put(f.worker,0x10,uint32_t(0));
        f.s.sp+=0x20;
        }
        f.s.x[0]=mode=="--timeout-cleanup"?0:1;f.event(0x142708);std::fprintf(stderr,"cleanup-entry: observed join result\n");f.event(0x1381b8);
        assert(mode=="--joined-cleanup");std::puts("runtime cleanup: exact observed native join allows cleanup PASS; pthread boundary modeled");return 0;
    }
    if(mode=="--external") {
        f.s={};f.s.x[0]=addr(f.camera);f.s.x[1]=2;f.s.x[30]=Chain::base+0x6d6ec;f.event(0x15f9e0);
        assert(f.native_acquires==1 && f.s.x[0]==1 && f.s.pad==arena::SkipOwned);
        f.s.pad=0;f.s.x[0]=addr(f.camera);f.event(0x15fba8);
        assert(f.native_releases==1 && f.s.pad==arena::SkipOwned);
        std::puts("runtime external: real native acquire/release forwarded exactly once, original result preserved PASS; native calls modeled");return 0;
    }
    auto d=f.router.diagnostics();assert(d.capable && !d.pending && d.origin==1);
    Client client;assert(client.connect(f.directory+"/frametop-native-portal/control.sock"));Request request;Reply reply;
    request.sequence=1;request.sent_ns=boottime();assert(client.exchange(request,reply));assert(reply.capability && reply.epoch);
    request.op=Operation::Update;request.epoch=reply.epoch;request.origin=reply.origin;request.generation=1;++request.sequence;
    request.sent_ns=boottime();assert(client.exchange(request,reply));
    if(mode=="--construct-setter") {
        f.frame_start();
        f.s.x[0]=addr(f.app);f.s.x[1]=1;const auto original=f.s;f.event(0x1130a8);
        assert(!std::memcmp(&original,&f.s,sizeof original));
        f.s.x[0]=addr(f.app);f.s.x[1]=0;f.s.x[2]=addr(f.outer_stack)+0xc70;
        f.s.x[3]=addr(f.outer_stack)+0x610;f.event(0x148244);
        assert(f.s.x[3]==addr(f.outer_stack)+0x610 && !f.native_acquires);
        f.frame_end();return 0;
    }
    const bool repeated=mode.rfind("--repeated-",0)==0;
    const unsigned cycles=repeated?8:1;
    std::array<std::byte,0x7b0> next_command{};std::array<std::byte,0x10> next_wrapper{};
    uint64_t cb=0xc001,sem=0xa004,value=0;
    TimelineSubmit timeline{};SubmitInfo submit{};
    for(unsigned cycle=0;cycle<cycles;++cycle) {
    const bool final=cycle+1==cycles;
    const bool reset_failed=final && mode=="--repeated-owned-reset-failed";
    const bool begin_failed=final && mode=="--repeated-owned-begin-failed";
    const bool no_wait=final && mode=="--repeated-owned-no-wait";
    const bool no_disposal=final && mode=="--repeated-owned-no-disposal";
    f.frame_start();f.record(0);f.record(1);f.frame_end();
    if(mode=="--foreign-external") {
        std::thread external([&]{
            arena::Saved r{};r.x[0]=addr(f.camera);r.x[1]=2;r.x[30]=Chain::base+0x6d6ec;
            const auto original=r;f.router.event(0x15f9e0,r);assert(!std::memcmp(&r,&original,sizeof r));
            ++f.native_acquires; // the untouched native continuation, modeled once
        });external.join();
        f.frame_start();f.s.x[0]=addr(f.app);f.s.x[1]=0;f.s.x[2]=addr(f.outer_stack)+0xc70;f.s.x[3]=addr(f.outer_stack)+0x610;f.event(0x148244);
        f.s.x[0]=1;f.event(0x9ee5c);f.s.x[27]=0;f.event(0x9ee98);
        assert(f.native_acquires==2 && f.native_releases==0 && f.router.diagnostics().pending);
        std::puts("runtime foreign external: original continuation unchanged, owner notified without replay, extra owned release suppressed, debt remains pending PASS; native boundaries modeled");return 0;
    }
    if(mode=="--reject-graph")put(f.r,0x1478,addr(f.r));
    f.s={};f.s.x[0]=addr(f.r);f.s.sp=0x900000;f.s.x[30]=Chain::base+0x6caf0;f.event(0x2a1e18);
    if(mode=="--reject-graph") {
        for(unsigned eye=0;eye<2;++eye) {
            f.s={};f.s.x[21]=addr(f.r);f.s.x[19]=addr(f.packets)+eye*0x2b0;f.s.x[22]=addr(f.passes)+eye*0x98;f.event(0x2a1c18);assert(f.s.pad==1);
        }
        std::array<std::byte,0x2b0> stock{};f.s.x[19]=addr(stock);f.s.pad=0;f.event(0x2a1c18);assert(!f.s.pad);
        assert(!f.lookups && f.router.diagnostics().pending && f.router.diagnostics().refusal==Refusal::GraphOwnership);
        std::puts("runtime refused graph: both immutable owned markers dropped, stock packet unchanged, no pipeline or settlement fabricated PASS; native boundaries modeled");return 0;
    }
    assert(f.lookups==2*(cycle+1));
    for(unsigned eye=0;eye<2;++eye) {
        f.s={};f.s.sp=0x8ff000;f.s.x[19]=addr(f.packets)+eye*0x2b0;f.s.x[21]=addr(f.r);f.s.x[22]=addr(f.passes)+eye*0x98;
        f.event(0x2a1c18);assert(f.s.pad==0);
        f.s.x[0]=addr(f.r);f.s.x[1]=addr(f.packets)+eye*0x2b0;f.s.x[2]=0xa001;f.s.x[3]=37;f.s.x[30]=Chain::base+0x2a1c58;
        f.event(0x29f1e8);f.event(0x29f200);assert(f.s.pad==arena::SkipOwned && f.s.x[0]==addr(f.pipeline_object));
        f.s.x[30]=Chain::base+0x2a1c58;f.event(0x29f518);
        if(eye==0 && mode=="--stop-after-left")f.router.stop();
    }
    d=f.router.diagnostics();assert(d.uploads[0]==cycle+1 && d.uploads[1]==cycle+1 && d.encoded[0]==cycle+1 && d.encoded[1]==cycle+1 && d.pending);
    assert(f.native_acquires==cycle+1 && f.lookups==2*(cycle+1));
    request.op=Operation::Release;++request.sequence;assert(client.exchange(request,reply));assert(reply.state==State::Draining);
    if(mode=="--pool-owned-current") {
        f.stock_pool_return("");
        assert(f.router.diagnostics().pending && !f.router.diagnostics().queue_success);
        ++request.sequence;assert(client.exchange(request,reply) && reply.state==State::Draining);
        std::puts("stock pool exception cannot settle current owned debt PASS");return 0;
    }
    // Real Command/Lifecycle classes, with explicitly modeled native queue,
    // post-stamp, wait, vector transfer/disposal and reset memory transitions.
    f.s={};f.s.sp=0x880000;f.s.x[0]=0xbeef;f.s.x[1]=addr(f.executable);f.s.x[30]=Chain::base+FT_BOOT_QUEUE_LR;f.event(0x3018b0);
    value=binding::Command::timeline_value({addr(f.sync),f.signal_value.serial,1});
    timeline.type=1000207003;timeline.signal_count=1;timeline.signals=uint64_t(&value);
    submit.type=4;submit.next=uint64_t(&timeline);submit.command_count=1;submit.commands=uint64_t(&cb);
    submit.signal_count=1;submit.signals=uint64_t(&sem);
    f.s.x[19]=addr(f.executable);f.s.x[0]=0xa005;f.s.x[1]=1;f.s.x[2]=uint64_t(&submit);f.s.x[3]=0;f.event(0x3021d4);
    f.s.x[0]=0;f.event(0x3021d8);
    f.s.x[0]=addr(f.executable);f.s.x[1]=addr(f.sync);f.s.x[2]=uint64_t(&f.signal_value);f.event(0x302208);
    for(auto *object:{&f.resource}) {
        f.s.sp=0x870000;f.s.x[0]=addr(*object);f.s.x[1]=addr(f.sync);f.s.x[2]=uint64_t(&f.signal_value);f.event(0x310ea0);
        put(*object,0x10,addr(f.sync));put(*object,0x18,f.signal_value.serial);put(*object,0x20,int8_t(1));
        f.s.x[19]=addr(*object);f.s.sp-=0x30;f.event(0x310f20);
    }
    f.s.sp=0x870000;f.s.x[0]=addr(f.executable);f.s.x[1]=addr(f.sync);f.s.x[2]=uint64_t(&f.signal_value);f.event(0x310ea0);
    put(f.executable,0x10,addr(f.sync));put(f.executable,0x18,f.signal_value.serial);put(f.executable,0x20,int8_t(1));
    f.s.x[19]=addr(f.executable);f.s.sp-=0x30;f.event(0x310f20);
    f.s.sp=0x880000;f.event(0x30220c);assert(f.router.diagnostics().queue_success==cycle+1);
    ++request.sequence;assert(client.exchange(request,reply) && reply.state==State::Draining);
    if(!no_wait) {
    f.s.sp=0x860000;f.s.x[0]=addr(f.sync);f.s.x[1]=uint64_t(&f.signal_value);f.s.x[30]=Chain::base+0x311104;f.event(0x3160b8);
    f.s.sp-=0x40;f.s.x[0]=0;f.event(0x316110);f.s.sp+=0x40;f.s.x[0]=1;f.event(0x316120);
    }
    ++request.sequence;assert(client.exchange(request,reply) && reply.state==State::Draining);
    f.s.sp=0x8fff30;f.s.x[19]=addr(f.r);f.event(0x2a2230);
    put(f.r,0x15a8,addr(f.passes));put(f.r,0x15b8,addr(f.passes));put(f.r,0x15c0,addr(f.passes)+f.passes.size());f.event(0x2a2320);
    // The real dispatcher relinquishes its current command wrapper after the
    // transfer. That does not destroy the pooled executable retained by lifecycle.
    put(f.r,0x1380,uint64_t(0));f.s.x[0]=addr(f.command);f.s.x[30]=Chain::base+FT_BOOT_RELEASE_LR;f.event(0x305260);
    put(f.executable,0x50,addr(f.bootstrap_pool));
    f.s.sp-=0x30;f.s.x[0]=addr(f.bootstrap_pool)+0xd0;f.s.x[1]=addr(f.executable);
    f.s.x[19]=addr(f.command);f.s.x[30]=Chain::base+FT_BOOT_POOL_LR;f.event(0x311648);
    if(!cycle) {
    put(next_command,8,addr(f.r)+0x50);put(next_command,0x738,uint64_t(0xc002));put(next_wrapper,8,addr(next_command));
    put(next_command,0x428,uint64_t(&f.signal_pointer));put(next_command,0x430,uint64_t(&f.signal_pointer)+8);
    put(next_command,0x50,addr(f.bootstrap_pool));
    put(next_command,0x4f0,uint64_t(&f.signal_value));
    f.s.x[0]=f.s.x[20]=addr(next_command);f.s.sp=addr(f.stack)+0x100;f.event(0x301808);
    f.s.sp-=0x40;f.s.x[19]=addr(next_command);f.s.x[30]=Chain::base+0x300ae0;f.event(0x300a08);
    f.s.sp-=0x30;put(f.stack,0x98,uint64_t(Chain::base+0x300ae0));f.s.x[0]=0;f.event(0x300a38);
    f.s.sp=addr(f.stack)+0x100;f.s.x[20]=addr(next_command);f.s.x[0]=1;f.event(0x30180c);
    }
    f.s.sp=0x8fff30;f.s.x[19]=addr(f.r);f.s.x[0]=addr(next_wrapper);f.event(0x2a23f4);put(f.r,0x1380,addr(next_wrapper));
    if(mode=="--pool-owned-retained") {
        f.stock_pool_return("");
        assert(f.router.diagnostics().pending && f.router.diagnostics().queue_success==1);
        ++request.sequence;assert(client.exchange(request,reply) && reply.state==State::Draining);
        std::puts("stock pool exception cannot settle retained owned debt PASS");return 0;
    }
    f.s.x[22]=addr(f.r);
    if(!no_disposal)f.event(0x2987d8);
    put(f.r,0x15c0,addr(f.passes));
    if(!no_disposal)f.event(0x298840);
    ++request.sequence;assert(client.exchange(request,reply) && reply.state==State::Draining);
    f.s.sp=0x840000;f.s.x[0]=addr(f.executable);f.s.x[30]=Chain::base+0x301000;f.event(0x300b20);
    f.s.sp-=0x20;f.s.x[19]=addr(f.executable);f.s.x[0]=reset_failed?uint32_t(-4):0;f.event(0x300c54);
    if(!reset_failed) {
        f.s.sp+=0x20;f.s.x[0]=addr(f.executable);f.event(0x300a08);
        f.s.sp-=0x30;f.s.x[0]=begin_failed?uint32_t(-4):0;f.event(0x300a38);
    }
    if(begin_failed) {
        // Successful reset already destroyed the old executable and permits
        // settlement. Failed Begin must still forbid successor publication.
        assert(!f.router.diagnostics().pending);
        f.stock_pool_return("");f.stock_pool_recycle("");
        f.stock_turnover(addr(next_command),addr(next_wrapper),addr(f.command));
        f.frame_start();f.frame_end();
        assert(!f.router.diagnostics().capable && f.router.diagnostics().uploads[0]==cycles);
        ++request.sequence;assert(client.exchange(request,reply) && reply.state!=State::Draining);
        std::puts("owned old generation settled by reset, failed Begin cannot renew or publish after 7 settled cycles PASS");return 0;
    }
    if(reset_failed || no_wait || no_disposal) {
        assert(f.router.diagnostics().pending);
        f.stock_pool_return("");f.stock_pool_recycle("");
        assert(f.router.diagnostics().pending && f.router.diagnostics().queue_success==cycles);
        ++request.sequence;assert(client.exchange(request,reply) && reply.state==State::Draining);
        std::puts("owned incomplete settlement stays pending through stock pool return/retry after 7 settled cycles PASS");return 0;
    }
    d=f.router.diagnostics();std::fprintf(stderr,"settlement refusal=%u pending=%u queue=%llu\n",unsigned(d.refusal),d.pending,(unsigned long long)d.queue_success);
    assert(!d.pending);++request.sequence;assert(client.exchange(request,reply));
    if(mode!="--stop-after-left")assert(reply.state==State::Off);
    if(mode=="--stop-after-left") {
        f.frame_start();f.frame_end();assert(f.router.diagnostics().stopped);
        std::puts("runtime stop: committed right eye still encoded, both GPU/CPU debts settled, future admission stopped PASS; native boundaries modeled");return 0;
    }
    // Actual pool turnover: successor C was published before old C's GPU+CPU
    // reset issued its next generation. It remains an authentic recording.
    f.frame_start();f.s.x[0]=addr(f.app);f.s.x[1]=0;f.s.x[2]=addr(f.outer_stack)+0xc70;f.s.x[3]=addr(f.outer_stack)+0x610;f.event(0x148244);
    f.s.x[0]=1;f.event(0x9ee5c);f.s.x[27]=0;f.event(0x9ee98);assert(f.native_releases==cycle+1);put(f.app,0x2568c,uint8_t(0));f.frame_end();
    if(!repeated)break;
    // Lifecycle has settled both eyes and released its retained command. The
    // successor generation must survive until a NEW actual stock reset/Begin.
    // Then interleave the exact stock pool return and re-publication before the
    // next full owned queue/wait/disposal cycle on the same native C/handle.
    f.stock_pool_recycle("");f.stock_pool_return("");f.stock_pool_recycle("");
    f.stock_turnover(addr(next_command),addr(next_wrapper),addr(f.command));
    f.stock_pool_recycle("",addr(next_command));
    f.frame_start();f.frame_end();assert(f.router.diagnostics().capable && !f.router.diagnostics().pending);
    if(cycle+1==cycles) {
        assert(f.router.diagnostics().queue_success==cycles && f.native_releases==cycles);
        std::puts("8 complete owned queue/wait/disposal/reset/Release cycles interleaved with stock recycling PASS; native boundaries modeled");return 0;
    }
    request.op=Operation::Acquire;request.epoch=0;request.generation=0;request.origin=0;++request.sequence;request.sent_ns=boottime();
    assert(client.exchange(request,reply) && reply.epoch);request.epoch=reply.epoch;request.origin=reply.origin;
    request.op=Operation::Update;request.generation=1;++request.sequence;request.sent_ns=boottime();assert(client.exchange(request,reply));
    f.s.x[22]=addr(f.r);f.event(0x2987d4);put(f.r,0x1418,uint32_t(0));
    put(f.r,0x15a8,addr(f.passes)+f.passes.size());f.enqueues=0;++f.signal_value.serial;
    }
    f.frame_start();f.frame_end();request.op=Operation::Acquire;request.epoch=0;request.generation=0;request.origin=0;++request.sequence;request.sent_ns=boottime();
    assert(client.exchange(request,reply) && reply.epoch);request.epoch=reply.epoch;request.origin=reply.origin;
    request.op=Operation::Update;request.generation=1;++request.sequence;request.sent_ns=boottime();assert(client.exchange(request,reply));
    f.s.x[22]=addr(f.r);f.event(0x2987d4);put(f.r,0x1418,uint32_t(0));
    put(f.r,0x15a0,addr(f.passes));put(f.r,0x15a8,addr(f.passes)+f.passes.size());f.enqueues=0;
    f.frame_start();f.record(0);f.record(1);f.frame_end();
    f.s={};f.s.x[0]=addr(f.r);f.s.sp=0x900000;f.s.x[30]=Chain::base+0x6caf0;f.event(0x2a1e18);assert(f.lookups==4);
    for(unsigned eye=0;eye<2;++eye) {
        f.s={};f.s.sp=0x8ff000;f.s.x[19]=addr(f.packets)+eye*0x2b0;f.s.x[21]=addr(f.r);f.s.x[22]=addr(f.passes)+eye*0x98;
        f.event(0x2a1c18);assert(f.s.pad==0);
        f.s.x[0]=addr(f.r);f.s.x[1]=addr(f.packets)+eye*0x2b0;f.s.x[2]=0xa001;f.s.x[3]=37;f.s.x[30]=Chain::base+0x2a1c58;
        f.event(0x29f1e8);f.event(0x29f200);assert(f.s.pad==arena::SkipOwned && f.s.x[0]==addr(f.pipeline_object));
        f.s.x[30]=Chain::base+0x2a1c58;f.event(0x29f518);
    }
    f.s={};f.s.sp=0x880000;f.s.x[0]=0xbeef;f.s.x[1]=addr(next_command);f.s.x[30]=Chain::base+FT_BOOT_QUEUE_LR;f.event(0x3018b0);
    cb=0xc002;f.s.x[19]=addr(next_command);f.s.x[0]=0xa005;f.s.x[1]=1;f.s.x[2]=uint64_t(&submit);f.s.x[3]=0;f.event(0x3021d4);
    f.s.x[0]=0;f.event(0x3021d8);d=f.router.diagnostics();
    assert(d.refusal==Refusal::None && d.uploads[0]==2 && d.uploads[1]==2 && d.encoded[0]==2 && d.encoded[1]==2 && d.queue_success==2 && d.pending);
    std::puts("runtime chain: private lease, origin x8, native demand, stereo staging/preparation/encoding, exact queue/wait, disposal/reset, pending Release, real pool-publication turnover and second stereo queue PASS; native boundaries modeled");
    return 0;
}
#ifndef FT_ATTACHMENT_TEST
int main(int argc,char **argv) {
    const std::string mode=argc>1?argv[1]:"";
    const bool bootstrap=mode=="--bootstrap-turnover" || mode.rfind("--bootstrap-bad-",0)==0;
    const bool publication=mode.rfind("--publication-",0)==0;
    const bool repeated=mode.rfind("--repeated-",0)==0;
    Chain f;active=&f;f.bootstrap_router(bootstrap || publication || repeated);
    if(mode=="--main-pump" || mode=="--main-pump-owner") {
        f.pump_event(0); // normal entry on bootstrap-owned route before publication
        assert(!f.router.diagnostics().stopped);
        f.publish_worker();f.pump_event(0);f.pump_event(1); // handoff-wait route
        if(mode=="--main-pump-owner") {
            f.run_enter();f.pump_event(0);f.pump_event(1);f.frame_start();f.frame_end();
            assert(!f.router.diagnostics().stopped && f.router.diagnostics().capable);
            f.pump_event(2);f.frame_start();f.frame_end();
            assert(f.router.diagnostics().stopped && !f.router.diagnostics().capable);return 0;
        }
        std::atomic<unsigned> stage{0};
        std::thread worker([&] {
            f.run_enter();f.frame_start();f.frame_end();
            assert(!f.router.diagnostics().stopped && f.router.diagnostics().capable);
            // A foreign worker cannot authenticate the bootstrap shutdown store.
            f.pump_event(2);f.frame_start();f.frame_end();assert(!f.router.diagnostics().stopped);
            stage=1;while(stage.load()!=2)std::this_thread::yield();
            f.frame_start();f.frame_end();assert(!f.router.diagnostics().stopped && f.router.diagnostics().capable);
            stage=3;while(stage.load()!=4)std::this_thread::yield();
            f.frame_start();f.frame_end();assert(f.router.diagnostics().stopped && !f.router.diagnostics().capable);
        });
        while(stage.load()!=1)std::this_thread::yield();
        f.pump_event(0);f.pump_event(1);
        f.s={};f.s.x[20]=addr(f.app);f.s.x[1]=1;f.event(pump_events[2][0]); // unrelated context
        f.s.x[20]=addr(f.context);f.s.x[1]=0;f.event(pump_events[2][0]); // not pinned store value
        stage=2;while(stage.load()!=3)std::this_thread::yield();
        f.pump_event(2); // actual shutdown store, still BEFORE any wait callback
        stage=4;worker.join();
        std::puts("pinned normal entry/backedge and foreign/context/value rejection; real shutdown revokes before wait PASS");return 0;
    }
    if(repeated) {
        f.stock_pool_return("");f.publish_worker();
        std::thread worker([&] {
            f.run_enter();f.stock_pool_recycle("");
            f.bootstrap_turnover("--publication-pool-live",true);
            const auto fault=mode=="--repeated-reset-failed"?"--publication-pool-reset-failed":
                mode=="--repeated-begin-failed"?"--publication-pool-begin-failed":"";
            f.repeated_stock(fault);
            if(!*fault) {
                put(f.r,0x15a8,addr(f.passes)+f.passes.size());
                assert(run_chain(f,mode,true)==0);
            }
        });worker.join();return 0;
    }
    if(publication) {
        const bool pool=mode.rfind("--publication-pool-",0)==0 && mode!="--publication-pool-drop";
        // First native stock return is BEFORE the actual bootstrap/worker handoff.
        if(pool)f.stock_pool_return(mode);
        f.publish_worker();
        std::thread worker([&]{
            f.run_enter();
            if(mode=="--publication-pool-drop") {
                // Modeled peer-wrapper return followed by actual reset/Begin
                // callback coordinates. The pinned CPU pool witness separately
                // executes return and reacquisition without deleting C.
                f.s={};f.s.x[0]=addr(f.bootstrap_pool)+0xd0;f.s.x[1]=addr(f.executable);
                f.s.x[30]=Chain::base+0x3052f0;f.event(0x311648);
                f.s={};f.s.x[0]=addr(f.executable);f.s.sp=addr(f.stack)+0x100;
                f.s.x[30]=Chain::base+FT_RECYCLE_LR;f.event(0x300b20);
                f.s.sp-=0x20;f.s.x[19]=addr(f.executable);f.s.x[0]=0;f.event(0x300c54);
                f.s.sp+=0x20;f.s.x[0]=addr(f.executable);f.event(0x300a08);
                f.s.sp-=0x30;f.s.x[0]=0;f.event(0x300a38);
            }
            if(pool) {
                f.stock_pool_recycle(mode);
                f.stock_pool_return("");
                f.stock_pool_recycle(mode);
            }
            f.bootstrap_turnover(mode,true);
            if(mode=="--publication-pool-live") {
                put(f.r,0x15a8,addr(f.passes)+f.passes.size());
                assert(run_chain(f,"",true)==0);
            }
        });worker.join();return 0;
    }
    if(mode=="--command-trace-capacity") {
        f.publish_worker();f.s={};f.s.x[19]=addr(f.executable);
        for(size_t i=0;i<CommandTrace::capacity+5;++i){f.s.x[0]=i;f.event(0x300a38);}
        return 0; // actual collector teardown drains the bounded immutable slots
    }
    if(mode=="--command-trace-gap") {
        f.publish_worker();
        f.begin_command(addr(f.other)); // bootstrap events while handoff waits
        std::thread foreign([&]{f.begin_command(addr(f.other));});foreign.join();
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        assert(!f.router.diagnostics().frames);return 0;
    }
    if(bootstrap) {
        f.bootstrap_turnover(mode);
        if(mode!="--bootstrap-turnover")return 0;
        std::thread worker([&]{assert(run_chain(f,mode)==0);});worker.join();return 0;
    }
    if(mode.rfind("--diagnostics-start-",0)==0 || mode.rfind("--diagnostics-stop-",0)==0 ||
       mode=="--diagnostics-run-lr") {
        // Synthetic faults at the real router predicates, not a claimed replay
        // of trial 06. Keep Run on a real second thread and never enter a frame.
        if(mode=="--diagnostics-start-failed") {
            f.s={};f.s.x[0]=addr(f.command);f.event(0x305260);
        }
        if(mode=="--diagnostics-start-renderer")put(f.app,0x256a0,uint64_t(0));
        if(mode=="--diagnostics-start-unreadable")put(f.scheduler,8,uint64_t(1));
        if(mode!="--diagnostics-stop-unrelated" && mode!="--diagnostics-stop-unknown")f.publish_worker();
        if(mode=="--diagnostics-run-lr") {
            std::thread worker([&] {
                f.s={};f.s.x[0]=addr(f.worker);f.s.x[30]=Chain::base+FT_NATIVE_RUN_RETURN+4;
                f.event(0x14e5a8);
            });worker.join();
        } else if(mode=="--diagnostics-stop-waiting" || mode=="--diagnostics-stop-same" || mode=="--diagnostics-stop-unknown") {
            f.s={};f.s.x[20]=mode=="--diagnostics-stop-waiting"?addr(f.app):addr(f.context);
            f.s.x[1]=1;f.s.x[30]=Chain::base+0x123456;f.event(0x1426d4);
            f.s.x[30]=0;f.event(0x1426d4); // first record stays paired
        } else if(mode=="--diagnostics-stop-unrelated") {
            // Existing router stop is unconditional, Evidence revocation is not.
            f.s={};f.s.x[0]=addr(f.worker);f.event(0x29bd18);
        } else {
            std::thread worker([&]{f.run_enter();});worker.join();
            f.s={};f.s.x[0]=addr(f.r);f.event(0x29bd18);
            f.event(0x29c238); // neither sticky diagnostic may be overwritten
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        auto d=f.router.diagnostics();assert(!d.frames && !d.uploads[0] && !d.uploads[1] && !d.capable);
        return 0;
    }
    if(mode=="--bootstrap-stock") {f.stock_append();f.stock_classifier();f.geometry_classifier();return 0;}
    if(mode=="--bootstrap-geometry") {f.stock_append();f.geometry_classifier();return 0;}
    if(mode.rfind("--bad-stock-",0)==0 || mode.rfind("--bad-geometry-",0)==0) {
        f.stock_append();f.s={};f.s.x[0]=addr(f.r);f.s.x[2]=addr(f.stock_wrapper);
        f.s.x[21]=addr(f.r);f.s.x[24]=addr(f.packets);
        put(f.packets,0x1c0,uint32_t(128));
        if(mode=="--bad-stock-owner")put(f.stock_resource,8,uint64_t(0));
        if(mode=="--bad-stock-offset")f.s.x[4]=64;
        if(mode=="--bad-stock-unreadable")f.s.x[2]=1;
        if(mode=="--bad-stock-resource")put(f.stock_wrapper,8,uint64_t(1));
        if(mode=="--bad-stock-capacity")put(f.stock_resource,0xb8,uint32_t(arena::Capacity+1));
        if(mode=="--bad-geometry-offset")f.s.x[3]=64;
        if(mode=="--bad-geometry-unreadable")f.s.x[24]=1;
        if(mode=="--bad-geometry-cursor")put(f.packets,0x1c0,uint32_t(577));
        f.event(mode.rfind("--bad-stock-",0)==0?0x2988b0:0x29f358);
        assert(f.s.pad==arena::Unsupported);
        f.s={};f.s.x[22]=addr(f.r);f.event(0x2987d4);assert(f.s.pad==arena::Unsupported);
        std::puts("runtime stock: invalid scalar/owner/bounds refused, reset cannot heal failure PASS");return 0;
    }
    if(mode=="--worker")f.stock_append(); // existing charged stock debt crosses handoff intact
    f.publish_worker();
    if(mode=="--worker" || mode=="--worker-no-stock" || mode.rfind("--application-",0)==0) {
        std::thread worker([&]{assert(run_chain(f,mode)==0);});worker.join();
        std::puts("runtime chain: actual bootstrap-to-worker thread boundary PASS");return 0;
    }
    return run_chain(f,mode);
}
#endif
