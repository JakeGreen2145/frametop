// SPDX-License-Identifier: MIT
// Real self-process memory, modeled native worker and dispatcher event boundaries.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main prior_evidence_test_main
#include "../runtime/evidence_test.cpp"
#undef main
#pragma GCC diagnostic pop
static void classifier_scope() {
    Fixture f;arena::Bridge bridge(f.evidence.bridge_memory());assert(f.bootstrap(true,&bridge));
    std::array<std::byte,0x20> wrapper{};std::array<std::byte,0x128> resource{};
    put(wrapper,8,addr(resource));put(resource,8,addr(f.r)+0x50);put(resource,0xb8,uint32_t(528));
    arena::Saved s{};s.x[0]=addr(f.r);s.x[2]=addr(wrapper);
    const auto id=f.evidence.identity();uint64_t value=0;
    const auto memory=f.evidence.bridge_memory();const auto reader=f.evidence.pass_reader();
    assert(!memory.read(memory.user,addr(wrapper)+8,&value,8));
    assert(f.evidence.arena_event(bridge,arena::UboClassify,s) && s.pad==arena::Replacement);
    assert(!memory.read(memory.user,addr(wrapper)+8,&value,8));
    assert(!memory.read(memory.user,addr(resource)+0xb8,&value,4));
    assert(!reader.read(reader.user,addr(wrapper)+8,&value,8));
    assert(!reader.live(reader.user,{id,1}));
    assert(bridge.ready() && bridge.policy().actual()==0);
    // Native allocation identity still gates every classifier after a valid borrow.
    put(f.r,0x1390,addr(resource));
    assert(!f.evidence.arena_event(bridge,arena::UboClassify,s) && s.pad==arena::Unsupported);
    assert(!bridge.ready());
}
static void owner_predicates() {
    Fixture f;assert(f.bootstrap(true,nullptr,1));
    std::array<std::byte,0x200> context{},worker{},scheduler{};
    std::array<std::byte,0x25700> app{};
    put(worker,0,uint64_t(Fixture::base+0x5a1428));put(worker,0x48,addr(context));
    put(context,0x98,addr(worker));put(context,0x1c0,addr(scheduler));put(scheduler,8,addr(app));put(app,0x256a0,addr(f.r));
    arena::Saved s{};s.x[19]=addr(worker);s.x[21]=addr(context);
    auto refused=[&](uint64_t pc,uint16_t expected) {
        const auto before=s;uint16_t predicate=999;
        assert(!f.evidence.owner_event(pc,s,&predicate) && predicate==expected);
        assert(!std::memcmp(&s,&before,sizeof s));
        assert(!f.evidence.first_failure_pc() && !f.evidence.graph_capable());
    };
    s.x[19]=1;refused(0x14099c,6);s.x[19]=addr(worker);
    put(worker,0,uint64_t(0));refused(0x14099c,7);put(worker,0,uint64_t(Fixture::base+0x5a1428));
    s.x[21]+=8;refused(0x14099c,9);s.x[21]-=8;
    uint16_t predicate=999;assert(f.evidence.owner_event(0x14099c,s,&predicate) && !predicate);
    refused(0x14099c,3);
    s.x[0]=addr(worker)+8;refused(0x140dbc,3);s.x[0]=addr(worker);
    s.x[21]+=8;refused(0x140dbc,4);s.x[21]-=8;
    put(context,0x98,uint64_t(0));refused(0x140dbc,7);put(context,0x98,addr(worker));
    put(context,0x1c0,uint64_t(0));refused(0x140dbc,9);
    put(context,0x1c0,uint64_t(1));refused(0x140dbc,10);put(context,0x1c0,addr(scheduler));
    put(scheduler,8,uint64_t(0));refused(0x140dbc,11);
    put(scheduler,8,uint64_t(1));refused(0x140dbc,12);put(scheduler,8,addr(app));
    put(app,0x256a0,uint64_t(0));refused(0x140dbc,13);put(app,0x256a0,addr(f.r));
    // A pending bootstrap dispatch cannot be transferred to Run midway through
    // relinquishing the current wrapper, even though initialization is intact.
    arena::Saved turnover{};turnover.x[0]=addr(f.r);turnover.sp=0x900000;turnover.x[30]=Fixture::base+FT_BOOT_DISPATCH_LR;
    put(f.executable,0x50,uint64_t(0xa000));assert(f.evidence.event(0x2a1e18,turnover));refused(0x140dbc,17);
    turnover.sp-=0xd0;turnover.x[19]=addr(f.r);
    assert(f.evidence.event(0x2a2230,turnover));assert(f.evidence.event(0x2a2320,turnover));
    put(f.r,0x1380,uint64_t(0));turnover.x[0]=addr(f.command);turnover.x[30]=Fixture::base+FT_BOOT_RELEASE_LR;
    assert(f.evidence.event(0x305260,turnover));refused(0x140dbc,17);
    turnover.sp-=0x30;turnover.x[0]=0xa0d0;turnover.x[1]=addr(f.executable);turnover.x[19]=addr(f.command);
    turnover.x[30]=Fixture::base+FT_BOOT_POOL_LR;assert(f.evidence.event(0x311648,turnover));
    std::array<std::byte,0x10> next{};put(next,8,addr(f.other));turnover.sp+=0x30;
    turnover.x[0]=addr(next);turnover.x[19]=addr(f.r);assert(f.evidence.event(0x2a23f4,turnover));put(f.r,0x1380,addr(next));
    assert(f.evidence.owner_event(0x140dbc,s,&predicate) && !predicate);
    std::thread thread([&] {
        s.x[30]=Fixture::base+FT_NATIVE_RUN_RETURN;
        put(worker,0x10,uint32_t(syscall(SYS_gettid)));put(worker,0x14,uint32_t(0x80000000));
        s.x[30]+=4;refused(0x14e5a8,3);s.x[30]-=4;
        s.x[0]+=8;refused(0x14e5a8,2);s.x[0]-=8;
        put(worker,0x10,uint32_t(0));refused(0x14e5a8,9);put(worker,0x10,uint32_t(syscall(SYS_gettid)));
        put(worker,0x14,uint32_t(0));refused(0x14e5a8,11);put(worker,0x14,uint32_t(0x80000000));
        put(app,0x256a0,uint64_t(0));refused(0x14e5a8,17);put(app,0x256a0,addr(f.r));
        assert(f.evidence.owner_event(0x14e5a8,s,&predicate) && !predicate);
        assert(f.evidence.graph_capable());
    });thread.join();
}
int main() {
    owner_predicates();
    classifier_scope();
    Fixture f;arena::Bridge bridge(f.evidence.bridge_memory());assert(f.bootstrap(true,&bridge));
    std::array<std::byte,0x200> context{},worker{},scheduler{};
    std::array<std::byte,0x25700> app{};
    put(worker,0,uint64_t(Fixture::base+0x5a1428));put(worker,0x48,addr(context));
    put(context,0x98,addr(worker));put(context,0x1c0,addr(scheduler));put(scheduler,8,addr(app));put(app,0x256a0,addr(f.r));
    f.s.x[19]=addr(worker);f.s.x[21]=addr(context);assert(f.evidence.owner_event(0x14099c,f.s));
    f.s.x[0]=addr(worker);assert(f.evidence.owner_event(0x140dbc,f.s));
    std::thread render([&]{
        arena::Saved r{};r.x[0]=addr(worker);r.sp=0x900000;r.x[30]=Fixture::base+FT_NATIVE_RUN_RETURN;
        put(worker,0x10,uint32_t(syscall(SYS_gettid)));put(worker,0x14,uint32_t(0x80000000));
        assert(f.evidence.owner_event(0x14e5a8,r));assert(f.evidence.graph_capable());
        assert(f.objects());render::Observation o{};assert(f.evidence.snapshot(addr(f.wrapper),addr(f.marker),o));
        // Passing readable renderer bytes off as a pass graph must be refused.
        put(f.r,0x15a0,addr(f.r));put(f.r,0x15a8,addr(f.r)+0x98);
        r.x[0]=addr(f.r);r.x[1]=0;r.x[30]=Fixture::base+0x6caf0;
        assert(!f.evidence.graph_event(0x2a1e18,r));
        assert(!f.evidence.pass_reader().live(&f.evidence,{f.evidence.identity(),o.arena.recording_generation}));
    });render.join();
    std::puts("runtime owner: event-paired native Run handoff, foreign initializer refusal and role-alias rejection PASS");
}
