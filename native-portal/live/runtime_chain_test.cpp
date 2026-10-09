// SPDX-License-Identifier: MIT
// Production router end-to-end. Native calls, native graph writes and Vulkan are
// modeled boundaries. No device, loader, camera, installed native bytes or SSH.
#include "runtime.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main previous_evidence_main
#include "../runtime/evidence_test.cpp"
#undef main
#pragma GCC diagnostic pop
#include <filesystem>
using namespace native_portal::live;
struct Chain:Fixture {
    Runtime router;
    std::array<std::byte,0x200> context{},worker{},scheduler{};
    std::array<std::byte,0x25700> app{};
    std::array<std::byte,0x2000> outer_stack{};
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
    unsigned enqueues=0,lookups=0,native_acquires=0,native_releases=0;
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
        put(executable,0x4f0,uint64_t(&signal_value));
        system_table[14]=base+0x1010;system_interface=uint64_t(system_table.data());
        directory=std::string(std::getenv("TMPDIR"))+"/rt-XXXXXX";assert(mkdtemp(directory.data()));
        assert(router.pin(base,elf_sha256));assert(router.start_server(directory.c_str()));
    }
    ~Chain(){std::filesystem::remove_all(directory);}
    void event(uint64_t pc) {router.event(pc,s);}
    void init() {
        event(0x7fda0);s.x[19]=addr(r);s.x[0]=addr(r)+0xec0;s.x[1]=0x80000;s.x[8]=s.sp+0x140;
        event(0x29a14c);assert(s.x[1]==0x80080);put(stack,0x140,addr(resource));event(0x29a150);put(stack,0x140,uint64_t(0));
        s.x[0]=s.x[20]=addr(executable);s.sp=addr(stack)+0x100;event(0x301808);
        s.sp-=0x40;s.x[19]=addr(executable);s.x[30]=base+0x300ae0;event(0x300a08);
        s.sp-=0x30;put(stack,0x98,uint64_t(base+0x300ae0));s.x[0]=0;event(0x300a38);
        s.sp=addr(stack)+0x100;s.x[20]=addr(executable);s.x[0]=1;event(0x30180c);
        s.x[19]=addr(r);s.x[1]=addr(command);event(0x29a528);s.x[24]=addr(r);s.x[0]=1;event(0x7fdbc);
        s.x[19]=addr(worker);s.x[21]=addr(context);event(0x14099c);s.x[0]=addr(worker);event(0x140dbc);
        put(worker,0x10,uint32_t(syscall(SYS_gettid)));put(worker,0x14,uint32_t(0x80000000));s.x[30]=base+FT_NATIVE_RUN_RETURN;event(0x14e5a8);
    }
    void frame_start() {
        s={};s.sp=addr(outer_stack);s.x[0]=addr(worker);event(0x147a48);
        s.sp=addr(outer_stack)+0x100;s.x[19]=addr(outer_stack)+0x610;
        s.x[3]=0;event(0x112ab4);s.x[1]=1;s.x[2]=0;event(0x112aec);
        event(0x112afc);s.x[1]=0;event(0x112c10);s.x[0]=0;event(0x112ac4);
        put(outer_stack,0x610+0x630,uint32_t(1));put(outer_stack,0x610+0x648,float(0));s.x[23]=0;event(0x112b58);
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
int main(int argc,char **argv) {
    const std::string mode=argc>1?argv[1]:"";
    Chain f;active=&f;f.init();f.frame_start();f.frame_end();
    if(mode=="--unsafe-cleanup") {std::fprintf(stderr,"cleanup-entry: active owned borrow, no join\n");f.event(0x1381b8);assert(false);}
    if(mode=="--joined-cleanup" || mode=="--join-error-cleanup") {
        put(f.worker,8,uint64_t(0x123456));f.s={};f.s.sp=0x990000;f.s.x[0]=addr(f.worker);f.s.x[20]=addr(f.context);f.event(0x142704);
        f.s.sp-=0x20;f.s.x[0]=0x123456;f.s.x[20]=addr(f.worker);f.event(0x183f78);
        f.s.x[0]=mode=="--joined-cleanup"?0:22;f.event(0x183f7c);
        put(f.worker,8,uint64_t(0));put(f.worker,0x10,uint32_t(0));
        f.s.sp+=0x20;f.s.x[0]=1;f.event(0x142708);std::fprintf(stderr,"cleanup-entry: observed join result\n");f.event(0x1381b8);
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
    assert(f.lookups==2);
    for(unsigned eye=0;eye<2;++eye) {
        f.s={};f.s.sp=0x8ff000;f.s.x[19]=addr(f.packets)+eye*0x2b0;f.s.x[21]=addr(f.r);f.s.x[22]=addr(f.passes)+eye*0x98;
        f.event(0x2a1c18);assert(f.s.pad==0);
        f.s.x[0]=addr(f.r);f.s.x[1]=addr(f.packets)+eye*0x2b0;f.s.x[2]=0xa001;f.s.x[3]=37;f.s.x[30]=Chain::base+0x2a1c58;
        f.event(0x29f1e8);f.event(0x29f200);assert(f.s.pad==arena::SkipOwned && f.s.x[0]==addr(f.pipeline_object));
        f.s.x[30]=Chain::base+0x2a1c58;f.event(0x29f518);
        if(eye==0 && mode=="--stop-after-left")f.router.stop();
    }
    d=f.router.diagnostics();assert(d.uploads[0]==1 && d.uploads[1]==1 && d.encoded[0]==1 && d.encoded[1]==1 && d.pending);
    assert(f.native_acquires==1 && f.lookups==2);
    request.op=Operation::Release;++request.sequence;assert(client.exchange(request,reply));assert(reply.state==State::Draining);
    // Real Command/Lifecycle classes, with explicitly modeled native queue,
    // post-stamp, wait, vector transfer/disposal and reset memory transitions.
    f.s={};f.s.sp=0x880000;f.s.x[0]=0xbeef;f.s.x[1]=addr(f.executable);f.s.x[30]=Chain::base+0x305500;f.event(0x3018b0);
    uint64_t cb=0xc001,sem=0xa004,value=binding::Command::timeline_value({addr(f.sync),42,1});
    TimelineSubmit timeline{};timeline.type=1000207003;timeline.signal_count=1;timeline.signals=uint64_t(&value);
    SubmitInfo submit{};submit.type=4;submit.next=uint64_t(&timeline);submit.command_count=1;submit.commands=uint64_t(&cb);
    submit.signal_count=1;submit.signals=uint64_t(&sem);
    f.s.x[19]=addr(f.executable);f.s.x[0]=0xa005;f.s.x[1]=1;f.s.x[2]=uint64_t(&submit);f.s.x[3]=0;f.event(0x3021d4);
    f.s.x[0]=0;f.event(0x3021d8);
    f.s.x[0]=addr(f.executable);f.s.x[1]=addr(f.sync);f.s.x[2]=uint64_t(&f.signal_value);f.event(0x302208);
    for(auto *object:{&f.resource}) {
        f.s.sp=0x870000;f.s.x[0]=addr(*object);f.s.x[1]=addr(f.sync);f.s.x[2]=uint64_t(&f.signal_value);f.event(0x310ea0);
        put(*object,0x10,addr(f.sync));put(*object,0x18,uint64_t(42));put(*object,0x20,int8_t(1));
        f.s.x[19]=addr(*object);f.s.sp-=0x30;f.event(0x310f20);
    }
    f.s.sp=0x870000;f.s.x[0]=addr(f.executable);f.s.x[1]=addr(f.sync);f.s.x[2]=uint64_t(&f.signal_value);f.event(0x310ea0);
    put(f.executable,0x10,addr(f.sync));put(f.executable,0x18,uint64_t(42));put(f.executable,0x20,int8_t(1));
    f.s.x[19]=addr(f.executable);f.s.sp-=0x30;f.event(0x310f20);
    f.s.sp=0x880000;f.event(0x30220c);assert(f.router.diagnostics().queue_success==1);
    ++request.sequence;assert(client.exchange(request,reply) && reply.state==State::Draining);
    f.s.sp=0x860000;f.s.x[0]=addr(f.sync);f.s.x[1]=uint64_t(&f.signal_value);f.s.x[30]=Chain::base+0x311104;f.event(0x3160b8);
    f.s.sp-=0x40;f.s.x[0]=0;f.event(0x316110);f.s.sp+=0x40;f.s.x[0]=1;f.event(0x316120);
    ++request.sequence;assert(client.exchange(request,reply) && reply.state==State::Draining);
    f.s.sp=0x85ff30;f.s.x[19]=addr(f.r);f.event(0x2a2230);
    put(f.r,0x15a8,addr(f.passes));put(f.r,0x15b8,addr(f.passes));put(f.r,0x15c0,addr(f.passes)+f.passes.size());f.event(0x2a2320);
    // The real dispatcher relinquishes its current command wrapper after the
    // transfer. That does not destroy the pooled executable retained by lifecycle.
    f.s.x[0]=addr(f.command);f.s.x[30]=Chain::base+0x2a2338;f.event(0x305260);put(f.r,0x1380,uint64_t(0));
    std::array<std::byte,0x7b0> next_command{};std::array<std::byte,0x10> next_wrapper{};
    put(next_command,8,addr(f.r)+0x50);put(next_command,0x738,uint64_t(0xc002));put(next_wrapper,8,addr(next_command));
    put(next_command,0x428,uint64_t(&f.signal_pointer));put(next_command,0x430,uint64_t(&f.signal_pointer)+8);
    put(next_command,0x4f0,uint64_t(&f.signal_value));
    f.s.x[0]=f.s.x[20]=addr(next_command);f.s.sp=addr(f.stack)+0x100;f.event(0x301808);
    f.s.sp-=0x40;f.s.x[19]=addr(next_command);f.s.x[30]=Chain::base+0x300ae0;f.event(0x300a08);
    f.s.sp-=0x30;put(f.stack,0x98,uint64_t(Chain::base+0x300ae0));f.s.x[0]=0;f.event(0x300a38);
    f.s.sp=addr(f.stack)+0x100;f.s.x[20]=addr(next_command);f.s.x[0]=1;f.event(0x30180c);
    f.s.x[19]=addr(f.r);f.s.x[0]=addr(next_wrapper);f.event(0x2a23f4);put(f.r,0x1380,addr(next_wrapper));
    f.s.x[22]=addr(f.r);f.event(0x2987d8);put(f.r,0x15c0,addr(f.passes));f.event(0x298840);
    ++request.sequence;assert(client.exchange(request,reply) && reply.state==State::Draining);
    f.s.sp=0x840000;f.s.x[0]=addr(f.executable);f.s.x[30]=Chain::base+0x301000;f.event(0x300b20);
    f.s.sp-=0x20;f.s.x[19]=addr(f.executable);f.s.x[0]=0;f.event(0x300c54);
    f.s.sp+=0x20;f.s.x[0]=addr(f.executable);f.event(0x300a08);
    f.s.sp-=0x30;f.s.x[0]=0;f.event(0x300a38);
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
    f.s.x[0]=1;f.event(0x9ee5c);f.s.x[27]=0;f.event(0x9ee98);assert(f.native_releases==1);put(f.app,0x2568c,uint8_t(0));f.frame_end();
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
    f.s={};f.s.sp=0x880000;f.s.x[0]=0xbeef;f.s.x[1]=addr(next_command);f.s.x[30]=Chain::base+0x305500;f.event(0x3018b0);
    cb=0xc002;f.s.x[19]=addr(next_command);f.s.x[0]=0xa005;f.s.x[1]=1;f.s.x[2]=uint64_t(&submit);f.s.x[3]=0;f.event(0x3021d4);
    f.s.x[0]=0;f.event(0x3021d8);d=f.router.diagnostics();
    assert(d.refusal==Refusal::None && d.uploads[0]==2 && d.uploads[1]==2 && d.encoded[0]==2 && d.encoded[1]==2 && d.queue_success==2 && d.pending);
    std::puts("runtime chain: private lease, origin x8, native demand, stereo staging/preparation/encoding, exact queue/wait, disposal/reset, pending Release, real pool-publication turnover and second stereo queue PASS; native boundaries modeled");
}
