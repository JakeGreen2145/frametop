// SPDX-License-Identifier: MIT
// Real self-process memory, modeled native worker and dispatcher event boundaries.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main prior_evidence_test_main
#include "../runtime/evidence_test.cpp"
#undef main
#pragma GCC diagnostic pop
int main() {
    Fixture f;arena::Bridge bridge(f.evidence.bridge_memory());assert(f.bootstrap(true,&bridge));
    std::array<std::byte,0x200> context{},worker{},scheduler{};
    std::array<std::byte,0x25700> app{};
    put(worker,0,uint64_t(Fixture::base+0x5a1428));put(worker,0x48,addr(context));
    put(context,0x98,addr(worker));put(context,0x1c0,addr(scheduler));put(scheduler,8,addr(app));put(app,0x256a0,addr(f.r));
    f.s.x[19]=addr(worker);f.s.x[21]=addr(context);assert(f.evidence.owner_event(0x14099c,f.s));
    f.s.x[0]=addr(worker);assert(f.evidence.owner_event(0x140dbc,f.s));
    std::thread render([&]{
        arena::Saved r{};r.x[0]=addr(worker);r.sp=0x900000;r.x[30]=Fixture::base+0x1841ec;
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
