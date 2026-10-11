// SPDX-License-Identifier: MIT
// Actual Evidence/Adapter, modeled native events. No installed ABI or GPU claim.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main original_evidence_main
#include "evidence_test.cpp"
#undef main
#pragma GCC diagnostic pop
#include <cstdarg>
#include <sys/uio.h>
#include <memory>

static uint64_t forbidden_begin=0,forbidden_end=0,graph_begin=0,graph_end=0;
static unsigned forbidden_reads=0,reads=0;
extern "C" long __real_syscall(long,...);
extern "C" long __wrap_syscall(long n,...) {
    if(n==SYS_gettid)return __real_syscall(n);
    assert(n==SYS_process_vm_readv);
    va_list args;va_start(args,n);
    const auto pid=va_arg(args,int);auto *local=va_arg(args,iovec*);const auto lc=va_arg(args,int);
    auto *remote=va_arg(args,iovec*);const auto rc=va_arg(args,int);const auto flags=va_arg(args,int);va_end(args);
    ++reads;const auto a=uint64_t(remote->iov_base),z=a+remote->iov_len;
    if((a<forbidden_end && forbidden_begin<z) || (a<graph_end && graph_begin<z))++forbidden_reads;
    return __real_syscall(n,pid,local,lc,remote,rc,flags);
}
static bool collector_enabled=true;
#ifdef FT_STOCK_COLLECTOR_API
static bool modeled_scalar(void *,uint64_t,uint64_t address,uint64_t &value) noexcept {
    // TEST MODEL of native-local scalar authority, not a qualified collector.
    return collector_enabled && read_self(address,&value,8);
}
#endif
static void dormant(Evidence &e) {
#ifdef FT_STOCK_COLLECTOR_API
    assert(e.begin_dormant_stock({nullptr,modeled_scalar}));
#elif defined(FT_STOCK_FACTS_API)
    assert(e.begin_dormant_stock());
#else
    (void)e; // Baseline reproducer runs the real legacy implementation.
#endif
}
struct StockFixture:StagingFixture {
    bool stock_mode=true;
    std::array<std::byte,0x100> worker{},context{},scheduler{};
    std::array<std::byte,0x200> context_full{};
    std::array<std::byte,0x25700> app{};
    std::array<std::byte,0x10> next{};
    std::array<std::byte,0x400> pool{};
    binding::Command *observed_command=nullptr;
    std::array<render::Provenance,2> provenance{};
    static bool submitted(void *u,const render::Provenance &a,const render::Provenance &b,render::Completion &c) noexcept {
        return static_cast<StockFixture*>(u)->observed_command->submission(a,b,c);
    }
    static bool completed(void *u,const render::Binding &b,const render::Completion &c) noexcept {
        return static_cast<StockFixture*>(u)->observed_command->completed(b,c);
    }
    static bool settled(void *u,const render::Binding &b,const render::Provenance &a,const render::Provenance &c,const render::Completion &t) noexcept {
        return static_cast<StockFixture*>(u)->evidence.settled(b,a,c,t);
    }
    explicit StockFixture(bool stock=true) {
        stock_mode=stock;
        if(stock)dormant(evidence);
        put(executable,0x50,addr(pool));put(other,0x50,addr(pool));put(next,8,addr(other));
    }
    bool bootstrap(bool recording=false,arena::Bridge *bridge=nullptr,unsigned pooled=0) {
        return Fixture::bootstrap(recording,bridge,pooled,stock_mode);
    }
    void run() {
        put(worker,0,uint64_t(base+0x5a1428));put(worker,0x48,addr(context_full));
        put(context_full,0x98,addr(worker));put(context_full,0x1c0,addr(scheduler));
        put(scheduler,8,addr(app));put(app,0x256a0,addr(r));
        s={};s.x[19]=addr(worker);s.x[21]=addr(context_full);assert(evidence.owner_event(0x14099c,s));
        s.x[0]=addr(worker);assert(evidence.owner_event(0x140dbc,s));
        put(worker,0x10,uint32_t(syscall(SYS_gettid)));put(worker,0x14,uint32_t(0x80000000));
        s.x[30]=base+0x182aa8;assert(evidence.owner_event(0x14e5a8,s));
    }
    bool turn(uint64_t old_wrapper,uint64_t old_command,uint64_t new_wrapper,unsigned fault=0) {
        s={};s.x[0]=addr(r);s.sp=addr(stack)+0x1d0;s.x[30]=base+0x6caf0;
        if(fault==1)++s.x[30];
        if(fault==2)put(old_command==addr(executable)?executable:other,8,addr(r)+0x58);
        if(fault==9)++s.x[0]; // wrong actual native renderer, not an unseen memory mutation
        if(fault==10)put(old_command==addr(executable)?executable:other,0x50,addr(pool)+8);
        evidence.graph_event(0x2a1e18,s);evidence.event(0x2a1e18,s);
        s.sp-=0xd0;s.x[19]=addr(r);
        if(stock_mode) {s.x[1]=old_wrapper;evidence.event(0x2a1e3c,s);}
        if(fault==3)++s.sp;
        evidence.graph_event(0x2a2230,s);evidence.event(0x2a2230,s);evidence.graph_event(0,s);
        if(fault!=4){evidence.graph_event(0x2a2320,s);evidence.event(0x2a2320,s);evidence.graph_event(0,s);}
        put(r,0x1380,uint64_t(0));s.x[0]=s.x[20]=old_wrapper;s.x[30]=base+0x2a23b4;
        evidence.event(0x305260,s);
        s.sp-=0x30;s.x[1]=old_command;s.x[19]=old_wrapper;
        evidence.event(0x3052e0,s);
        s.x[0]=addr(pool)+0xd0;
        if(fault==5)++s.x[0];
        evidence.event(0x3052ec,s);
        s.x[30]=base+0x3052f0;
        evidence.event(0x311648,s);
        s.x[0]=1;evidence.event(0x3052f0,s);
        s.sp+=0x30;s.x[19]=addr(r);s.x[0]=s.x[20]=old_wrapper;s.x[1]=0x10;s.x[30]=base+0x2a23c0;
        evidence.event(0x4b3d0,s);
        if(fault==6)put(next,8,addr(executable)); // returned but not re-begun
        if(fault==7)put(other,0x738,uint64_t(0xc099));
        if(fault==8)put(next,8,addr(resource)); // readable raw pointer, no creation/Begin
        uint64_t nextC=0;std::memcpy(&nextC,reinterpret_cast<void*>(new_wrapper+8),8);
        wrapper_factory_model(base,addr(r),new_wrapper,nextC,addr(pool),s.sp,[&](uint64_t pc,arena::Saved call) {
            if(pc==0x2a23f8)put(r,0x1380,new_wrapper);
            evidence.event(pc,call);
        });
        return evidence.initialized();
    }
    void recycle(uint64_t c,unsigned fault=0) {
        s={};s.sp=addr(stack)+0x100;s.x[0]=s.x[19]=c;s.x[20]=addr(pool);s.x[30]=base+0x2a9cdc;
        if(fault==1)++s.x[30];
        evidence.event(0x300b20,s);s.sp-=0x20;s.x[0]=fault==2?uint32_t(-4):0;evidence.event(0x300c54,s);
        s.sp+=0x20;s.x[0]=c;evidence.event(0x300a08,s);
        s.sp-=0x30;
        if(stock_mode) {s.x[0]=c==addr(executable)?0xc001:0xc002;evidence.event(0x300a28,s);}
        s.x[0]=fault==3?uint32_t(-4):0;put(stack,0xd8,uint64_t(base+0x2a9cdc));evidence.event(0x300a38,s);
    }
};
static void no_graph_turnover() {
    auto f=std::make_unique<StockFixture>();assert(f->bootstrap(true,nullptr,1));f->run();
    const long page=sysconf(_SC_PAGESIZE);auto *guard=mmap(nullptr,page,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(guard!=MAP_FAILED);
    graph_begin=uint64_t(guard);graph_end=graph_begin+page;
    put(f->r,0x15a0,graph_begin);put(f->r,0x15a8,graph_begin+0x98);
    put(f->r,0x15b8,graph_begin);put(f->r,0x15c0,graph_begin+0x98);
    forbidden_begin=addr(f->resource);forbidden_end=forbidden_begin+f->resource.size();forbidden_reads=0;
    const bool ok=f->turn(addr(f->command),addr(f->executable),addr(f->next));
    assert(forbidden_reads==0 && "stock history dereferenced optional graph/backing");assert(ok);
#ifdef FT_STOCK_FACTS_API
    const auto facts=f->evidence.stock_command_facts();assert(facts.command==addr(f->other) && facts.vk==0xc002 && facts.generation && facts.begun && facts.pool==addr(f->pool));
    binding::Owner owner{};const auto before=reads;assert(!f->evidence.command_owner(owner));
    auto reader=f->evidence.pass_reader();uint64_t value=0;assert(!reader.read(reader.user,addr(f->r),&value,8));
    assert(!reader.live(reader.user,{f->evidence.identity(),facts.generation}));
    auto memory=f->evidence.bridge_memory();assert(!memory.read(memory.user,addr(f->resource)+0x50,&value,8));
    assert(!f->evidence.coherent(addr(f->resource)));assert(!f->objects());assert(reads==before);
    auto wrong=f->evidence.identity();++wrong.renderer_generation;assert(!f->evidence.engage_stock(wrong,facts.generation));
    assert(!f->evidence.engage_stock(f->evidence.identity(),facts.generation+1));
    assert(!f->evidence.graph_capable());
#endif
    forbidden_begin=forbidden_end=graph_begin=graph_end=0;assert(!munmap(guard,page));
}
static void negatives() {
#ifdef FT_STOCK_FACTS_API
    {
        auto f=std::make_unique<StockFixture>();assert(f->bootstrap(true,nullptr,1));f->run();
        // An observed native phase without its entry is a sticky history gap.
        f->s={};f->s.x[19]=addr(f->r);f->s.sp=addr(f->stack)+0x100;
        assert(!f->evidence.event(0x2a2320,f->s));
        const auto facts=f->evidence.stock_command_facts();
        assert(!f->evidence.engage_stock(f->evidence.identity(),facts.generation));
    }
    {
        auto f=std::make_unique<StockFixture>();assert(f->bootstrap(true,nullptr,1));f->run();
        // Current command has not been returned to its pool. No borrowed reset.
        f->recycle(addr(f->executable));assert(!f->evidence.initialized());
    }
    {
        auto f=std::make_unique<StockFixture>();assert(f->bootstrap(true,nullptr,1));f->run();
        f->s={};f->s.x[0]=addr(f->pool)+0xd0;f->s.x[1]=addr(f->other);f->s.x[30]=Fixture::base+0x29a8bc;
        assert(!f->evidence.event(0x311648,f->s));assert(!f->evidence.initialized());
    }
#endif
    for(unsigned fault=1;fault<=10;++fault) {
        auto f=std::make_unique<StockFixture>();assert(f->bootstrap(true,nullptr,1));f->run();
        assert(!f->turn(addr(f->command),addr(f->executable),addr(f->next),fault));
#ifdef FT_STOCK_FACTS_API
        assert(!f->evidence.engage_stock(f->evidence.identity(),1));
#endif
    }
    for(unsigned fault=1;fault<=3;++fault) {
        auto f=std::make_unique<StockFixture>();assert(f->bootstrap(true,nullptr,1));f->run();
        assert(f->turn(addr(f->command),addr(f->executable),addr(f->next)));f->recycle(addr(f->executable),fault);
        assert(!f->evidence.initialized());
    }
    // Default production retains graph/backing guards and refuses this graph.
    auto f=std::make_unique<StockFixture>(false);assert(f->bootstrap(true,nullptr,1));f->run();
    f->s={};f->s.x[0]=addr(f->r);f->s.x[30]=Fixture::base+0x6caf0;f->s.sp=addr(f->stack)+0x1d0;
    assert(!f->evidence.graph_event(0x2a1e18,f->s));
    put(f->resource,0x50,uint64_t(0));assert(!f->evidence.event(0x2a1e18,f->s));
}
static bool pipeline_positive(void *u,const render::Provenance &p,render::Registers &r) noexcept {
    auto &f=*static_cast<StockFixture*>(u);++f.pipelines;f.provenance[p.eye]=p;r.x[0]=0xa003;return true;
}
template<class FixtureType=StockFixture> static void recovery() {
    auto storage=std::make_unique<FixtureType>();auto &f=*storage;
    arena::Bridge bridge(f.evidence.bridge_memory());assert(f.bootstrap(true,&bridge,1));f.run();
    // No native graph storage is readable at these historical addresses.
    for(unsigned i=0;i<4;++i) {
        const bool odd=i&1;
        assert(f.turn(odd?addr(f.next):addr(f.command),odd?addr(f.other):addr(f.executable),odd?addr(f.command):addr(f.next)));
        f.recycle(odd?addr(f.other):addr(f.executable));assert(f.evidence.initialized());
    }
#ifdef FT_STOCK_FACTS_API
    const auto facts=f.evidence.stock_command_facts();assert(facts.command==addr(f.executable) && facts.generation>1);
    const auto before=reads;assert(f.evidence.engage_stock(f.evidence.identity(),facts.generation));assert(reads==before);
    assert(!f.evidence.begin_dormant_stock({}));assert(!f.evidence.engage_stock(f.evidence.identity(),facts.generation));
    f.stock_mode=false; // subsequent lifecycle keeps the unchanged ENGAGED path
#endif
    std::array<std::byte,0x130> passes{};std::array<std::byte,0x560> packets{};
    std::array<std::byte,0x40> cache{};std::array<std::byte,0x150> target{};
    put(f.r,0x15a0,addr(passes));put(f.r,0x15a8,addr(passes)+passes.size());
    put(f.r,0x1478,addr(cache));put(f.r,0x1480,addr(cache)+cache.size());
    for(unsigned i=0;i<2;++i) {
        put(passes,i*0x98,addr(target));put(passes,i*0x98+0x50,addr(packets)+i*0x2b0);
        put(passes,i*0x98+0x58,addr(packets)+(i+1)*0x2b0);put(passes,i*0x98+0x68,uint32_t(1));
    }
    put(cache,0,uint64_t(0xa001));put(cache,8,uint64_t(0xa002));put(cache,0x10,addr(target));
    put(cache,0x38,uint32_t(1));put(target,0x14c,uint32_t(37));
    binding::Command command;f.observed_command=&command;binding::Owner owner{};assert(f.evidence.command_owner(owner));
    owner.queue=0x701;owner.synchronization=0x702;owner.semaphore=0x703;assert(command.bind(owner));
    assert(f.evidence.bind_lifecycle(command,owner));assert(!f.evidence.release_lifecycle());
    Backend backend;FrameGate gate(backend);BackendStatus status;
    status.capable=status.serialized=status.external_clear=true;status.origin=4;assert(backend.publish(status));
    Lease lease;lease.epoch=8;lease.origin=4;lease.generation=2;lease.deadline_ns=100;lease.flags=1;
    lease.geometry.center[1]=-2.5f;assert(backend.publish_lease(lease));
    render::Binding b{};b.identity=f.evidence.identity();b.upstream=&bridge;
    b.targets={Fixture::base+0x296c00,Fixture::base+0x2993f8,Fixture::base+0x2988a8,Fixture::base+0x29b278,Fixture::base+0x2966a0,Fixture::base+0x294358};
    render::Callbacks cb{};cb.user=&f;cb.validate=AdapterFixture::validate;cb.call=StagingFixture::call;
    cb.observe=AdapterFixture::observe;cb.stock_blend=StagingFixture::stock;cb.pipeline=pipeline_positive;cb.clock=StagingFixture::clock;
    cb.submission=StockFixture::submitted;cb.completed=StockFixture::completed;cb.settled=StockFixture::settled;
    render::Adapter adapter(backend,gate,b,cb,2);f.adapter=&adapter;assert(adapter.initialize());
    PolicyInput policy;policy.enabled=true;assert(adapter.latch(gate.begin(1,policy,true,true,false,10)));
    for(unsigned eye=0;eye<2;++eye) {
        auto input=f.input(eye);arena::Saved s{};s.sp=addr(f.caller);s.x[27]=addr(f.camera);
        s.x[0]=addr(f.r);s.x[1]=addr(f.stock_wrapper);s.x[2]=addr(f.camera)+0xc0;s.x[7]=Fixture::base+0x2993f8;
        const auto ticket=f.evidence.stage(adapter,0x161d64,s,input);assert(ticket && s.pad==arena::Replacement);
        arena::Saved admission{};admission.x[19]=addr(f.r);admission.x[0]=eye*640;
        admission.x[20]=592;admission.x[21]=addr(f.wrapper)+0x18;admission.x[25]=s.x[2];admission.x[23]=0x808;admission.x[24]=8;
        adapter.upstream(admission,arena::Admission,b.identity.renderer_generation,uint64_t(syscall(SYS_gettid)));assert(admission.pad==arena::Replacement);
        assert(read_self(s.x[2],f.uploaded.data()+eye*640,592));put(f.wrapper,0x18,uint32_t(eye*640));put(f.r,0x1418,uint32_t((eye+1)*640));
        assert(f.evidence.stage(adapter,0x161d68,s,input,ticket));
        s.x[0]=addr(f.r);s.x[1]=s.x[22]=s.sp+0x300;s.x[4]=Fixture::base+0x29b278;s.x[2]=Fixture::base+0x450650;
        assert(f.evidence.stage(adapter,0x161ed8,s,input,ticket));
        const size_t p=eye*0x2b0;auto &rec=f.records[eye];
        rec.descriptors[0]=rec.descriptors[4]={0x999,eye*640,592};
        std::memcpy(packets.data()+p,rec.prefix.data(),352);put(packets,p+0x160,addr(packets)+p+0x170);put(packets,p+0x168,uint64_t(15));
        std::memcpy(packets.data()+p+0x170,rec.label.data(),16);std::memcpy(packets.data()+p+0x1c8,rec.descriptors.data(),216);
    }
    assert(adapter.outer_return());assert(adapter.state().pending);
    f.s={};f.s.x[0]=addr(f.r);f.s.sp=addr(f.stack)+0x1d0;f.s.x[30]=Fixture::base+0x6caf0;
    assert(f.evidence.graph_event(0x2a1e18,f.s));assert(f.evidence.event(0x2a1e18,f.s));
    auto reader=f.evidence.pass_reader();binding::PassRequest request{{b.identity,owner.generation},addr(f.marker),Fixture::base,0};
    const auto scan=binding::extract_passes(reader,request);assert(scan.reason==binding::PassReason::Ready);
    binding::PassGate pass(adapter,reader,request);binding::PassRegisters regs{};regs.x[0]=addr(f.r);pass.callback(regs,0);
    assert(adapter.state().committed && f.pipelines==2);
    adapter.off();assert(adapter.state().pending && backend.render_pending());
#ifdef FT_STOCK_FACTS_API
    assert(!f.evidence.begin_dormant_stock({}));
#endif
    auto scope=[&](uint64_t invocation,uint64_t object) {
        return binding::Scope{owner,invocation,uint64_t(syscall(SYS_gettid)),addr(f.stack),Fixture::base+0x2a21f0,object};
    };
    for(unsigned i=0;i<2;++i) {
        const auto &eye=scan.pair.eyes[i];auto entry=scope(i+1,owner.identity.renderer);
        assert(command.backend_enter(entry,f.provenance[i]));
        assert(adapter.backend(eye.record,eye.context,eye.lookup).decision==render::Decision::Portal);
        assert(command.backend_return(entry,true));
    }
    // No stock phase or Off can manufacture queue/GPU/CPU completion.
    assert(!f.evidence.release_lifecycle());assert(adapter.state().pending);
    arena::Saved queue{};queue.sp=addr(f.stack)+0x100;queue.x[1]=owner.command;
    assert(f.evidence.event(0x3018b0,queue));
    const binding::Signal signal{owner.synchronization,42,1};auto q=scope(3,owner.command);
    binding::SubmitSnapshot submit{true,true,1,1,1,1,owner.queue,owner.identity.device,owner.vk_command,owner.semaphore,binding::Command::timeline_value(signal),0};
    assert(command.queue_enter(q,submit,signal));assert(command.queue_result(q,0));
    auto stamp=scope(4,owner.command);assert(command.stamp_enter(q,stamp,signal));
    for(unsigned i=0;i<2;++i) {auto resource=scope(5+i,i?owner.command:owner.arena);assert(command.resource_enter(stamp,resource,signal));assert(command.resource_post(resource,resource.object,signal));}
    assert(command.stamp_post(stamp,owner.command,signal));
    adapter.submitted();assert(adapter.state().pending && !f.evidence.release_lifecycle());
    auto wait=scope(7,owner.synchronization);assert(command.wait_enter(wait,signal,owner.semaphore));
    assert(command.wait_result(wait,signal,0));assert(command.wait_return(wait,signal,1));
    assert(adapter.state().pending && !f.evidence.release_lifecycle());
    f.s.sp-=0xd0;f.s.x[19]=addr(f.r);
    assert(f.evidence.graph_event(0x2a2230,f.s));assert(f.evidence.event(0x2a2230,f.s));assert(f.evidence.graph_event(0,f.s));
    // Modeled native move, not a synthesized disposal certificate.
    put(f.r,0x15a8,addr(passes));put(f.r,0x15b8,addr(passes));put(f.r,0x15c0,addr(passes)+passes.size());
    assert(f.evidence.graph_event(0x2a2320,f.s));assert(f.evidence.event(0x2a2320,f.s));assert(f.evidence.graph_event(0,f.s));
    put(f.r,0x1380,uint64_t(0));f.s.x[0]=f.s.x[20]=addr(f.command);f.s.x[30]=Fixture::base+0x2a23b4;assert(f.evidence.event(0x305260,f.s));
    f.s.sp-=0x30;f.s.x[1]=addr(f.executable);f.s.x[19]=addr(f.command);assert(f.evidence.event(0x3052e0,f.s));
    f.s.x[0]=addr(f.pool)+0xd0;assert(f.evidence.event(0x3052ec,f.s));f.s.x[30]=Fixture::base+0x3052f0;
    assert(f.evidence.event(0x311648,f.s));f.s.x[0]=1;assert(f.evidence.event(0x3052f0,f.s));
    f.s.sp+=0x30;f.s.x[0]=f.s.x[20]=addr(f.command);f.s.x[19]=addr(f.r);f.s.x[1]=0x10;f.s.x[30]=Fixture::base+0x2a23c0;
    assert(f.evidence.event(0x4b3d0,f.s));
    wrapper_factory_model(Fixture::base,addr(f.r),addr(f.next),addr(f.other),addr(f.pool),f.s.sp,[&](uint64_t pc,arena::Saved call) {
        if(pc==0x2a23f8)put(f.r,0x1380,addr(f.next));
        f.evidence.event(pc,call);f.s=call;
    });
    f.s.x[22]=addr(f.r);assert(f.evidence.graph_event(0x2987d4,f.s));assert(f.evidence.graph_event(0x2987d8,f.s));assert(f.evidence.event(0x2987d8,f.s));assert(f.evidence.graph_event(0,f.s));
    put(f.r,0x15c0,addr(passes));assert(f.evidence.graph_event(0x298840,f.s));assert(f.evidence.event(0x298840,f.s));assert(f.evidence.graph_event(0,f.s));
    assert(!f.evidence.release_lifecycle()); // disposed + GPU is not executable reset
    f.recycle(addr(f.executable));assert(f.evidence.initialized());
    render::Completion completion{};assert(command.submission(f.provenance[0],f.provenance[1],completion));
    assert(f.evidence.settled(b,f.provenance[0],f.provenance[1],completion));
    assert(adapter.poll_completion() && adapter.reclaim());assert(!adapter.state().pending && !backend.render_pending());
    assert(f.evidence.release_lifecycle());
    uint64_t value=0;assert(!reader.read(reader.user,addr(packets),&value,8));
    std::puts("stock facts: pool reuse -> engagement -> stereo commit/backend -> exact queue/wait/disposal/reset and lifecycle release PASS");
}
static void pending_pool_before_run() {
    auto f=std::make_unique<StockFixture>();assert(f->bootstrap(true,nullptr,1));
    f->s={};f->s.x[0]=addr(f->pool)+0xd0;f->s.x[1]=addr(f->other);f->s.x[30]=Fixture::base+0x29a8b8;
    assert(f->evidence.event(0x311648,f->s));f->recycle(addr(f->other));assert(f->evidence.initialized());
    f->run();assert(f->turn(addr(f->command),addr(f->executable),addr(f->next)));
#ifdef FT_STOCK_FACTS_API
    const auto facts=f->evidence.stock_command_facts();assert(facts.command==addr(f->other) && facts.begun);
    assert(f->evidence.engage_stock(f->evidence.identity(),facts.generation));
#endif
}
static void stock_arena_and_retained() {
    auto f=std::make_unique<StockFixture>();arena::Bridge bridge(f->evidence.bridge_memory());assert(f->bootstrap(true,&bridge,1));f->run();
    put(f->r,0x15a0,uint64_t(1));put(f->r,0x15a8,uint64_t(2)); // deliberately invalid graph bounds
    f->input(0);put(f->stock_wrapper,0x18,uint32_t(0));
    arena::Saved classify{};classify.x[0]=addr(f->r);classify.x[2]=addr(f->stock_wrapper);classify.x[4]=0;
    assert(f->evidence.arena_event(bridge,arena::UboClassify,classify));assert(classify.pad==arena::Replacement);
    auto memory=f->evidence.bridge_memory();uint64_t out=0;const auto reads_before=reads;
    assert(!memory.read(memory.user,addr(f->stock_wrapper)+8,&out,8));assert(!memory.read(memory.user,addr(f->r)+0x1390,&out,8));assert(reads==reads_before);
    arena::Saved retained{};retained.x[22]=addr(f->r);retained.sp=addr(f->stack);
    assert(f->evidence.event(0x2987d8,retained));
#ifdef FT_STOCK_FACTS_API
    const auto facts=f->evidence.stock_command_facts();assert(!f->evidence.engage_stock(f->evidence.identity(),facts.generation));
#endif
    assert(f->evidence.event(0x298840,retained));assert(reads==reads_before);
    arena::Saved reset{};reset.x[22]=addr(f->r);assert(f->evidence.arena_event(bridge,arena::Reset,reset));
    assert(bridge.policy().actual()==0);
}
static void unsupported_collector() {
    {
        StockFixture early;assert(early.evidence.pin(Fixture::base,elf_sha256));assert(early.evidence.event(0x7fda0,early.s));
        auto memory=early.evidence.bridge_memory();uint64_t value=0;const auto before=reads;
        assert(!memory.read(memory.user,addr(early.r),&value,8));assert(reads==before);
    }
    auto f=std::make_unique<StockFixture>();assert(f->bootstrap(true,nullptr,1));f->run();
    collector_enabled=false;const auto before=reads;
    assert(!f->turn(addr(f->command),addr(f->executable),addr(f->next)));
    assert(reads==before);collector_enabled=true;
#ifdef FT_STOCK_FACTS_API
    assert(!f->evidence.engage_stock(f->evidence.identity(),1));
#endif
}
#ifndef FT_STOCK_TEST_LIBRARY
int main() {
    unsupported_collector();no_graph_turnover();negatives();pending_pool_before_run();stock_arena_and_retained();recovery();
    std::puts("stock facts: protected graph and read counters, strict history negatives PASS");
}
#endif
