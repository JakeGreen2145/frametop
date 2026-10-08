// SPDX-License-Identifier: MIT
// Production adapter, control and geometry. Only native register operations are fake.
#define main render_original_main
#include "../render/test.cpp"
#undef main
#include "command.hpp"
#include <sys/wait.h>
#include <cstdlib>
#include <new>
static std::atomic<size_t> binding_allocations{0};
void *operator new(size_t n) {
    ++binding_allocations;
    if (void *p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
namespace nb = native_portal::binding;
struct Bits { uint64_t invocation, tid, sp, lr, object; };
struct Chain {
    uint64_t frame, renderer, command, arena, buffer, map, capacity, alignment, queue,
             semaphore, synchronization, serial;
    int subserial, queue_result, wait_result, wait_boolean;
    Bits backend[2], queue_scope, stamp_scope, resources[2], wait_scope;
};
#include "build/private_chains.hpp"
static nb::Scope scoped(const nb::Owner &o, Bits b) {
    return {o, b.invocation, b.tid, b.sp, b.lr, b.object};
}
// CPU-only modeled public submit fields. No Vulkan loader or native pointers.
struct ModeledVkSubmitInfo {
    uint32_t commandBufferCount = 1, signalSemaphoreCount = 1;
    std::array<uint64_t,2> pCommandBuffers{}, pSignalSemaphores{};
    struct Timeline {
        uint32_t signalSemaphoreValueCount = 1;
        std::array<uint64_t,2> pSignalSemaphoreValues{};
        bool terminal_chain = true;
    } timeline;
};
static nb::SubmitSnapshot normalized(const ModeledVkSubmitInfo &v, const nb::Owner &o,
                                     uint32_t submitCount = 1) {
    return {true, v.timeline.terminal_chain, submitCount, v.commandBufferCount,
            v.signalSemaphoreCount, v.timeline.signalSemaphoreValueCount,
            o.queue, o.identity.device, v.pCommandBuffers[0],v.pSignalSemaphores[0],
            v.timeline.pSignalSemaphoreValues[0],0};
}
struct Native : Fake {
    nb::Command command;
    nb::Owner owner;
    const Chain &chain;
    std::array<Provenance, 2> provenances{};
    bool off_inside = false, expire_inside = false;
    Backend *control = nullptr;
    explicit Native(const Chain &c) : chain(c) {
        binding.identity.renderer = c.renderer;
        // Device/process/command generations and VkCommandBuffer are SYNTHETIC:
        // live03 did not capture them. Never infer generation from recycled C.
        owner = {binding.identity, c.frame + 10, c.frame + 20, c.command, 0xc001,
                 c.arena, c.buffer, c.map, c.capacity, c.alignment, c.queue, c.semaphore, c.synchronization};
        obs.arena = {0x1000, 0x1098, 3, c.command, c.arena, c.buffer, c.map,
                     c.capacity, 0, c.alignment, owner.recording};
    }
    static bool pipeline_bound(void *u, const Provenance &p, Registers &r) noexcept {
        auto &n = *static_cast<Native *>(u);
        n.provenances[p.eye] = p;
        return n.command.backend_enter(scoped(n.owner, n.chain.backend[p.eye]), p) &&
               Fake::pipeline(u, p, r);
    }
    static bool submission_bound(void *u, const Provenance &a, const Provenance &b,
                                 Completion &out) noexcept {
        auto &n = *static_cast<Native *>(u);
        bool ok = nb::Command::submission_callback(&n.command, a, b, out);
        if (n.off_inside) n.control->invalidate(Reason::Off);
        if (n.expire_inside) now = 100;
        return ok;
    }
    static bool completed_bound(void *u, const Binding &b, const Completion &c) noexcept {
        return nb::Command::completed_callback(&static_cast<Native *>(u)->command, b, c);
    }
    Callbacks callbacks() {
        auto c = Fake::callbacks();
        c.user = this;
        c.pipeline = pipeline_bound;
        c.submission = submission_bound;
        c.completed = completed_bound;
        assert(!c.settled); // there is deliberately NO settlement callback in the binding
        return c;
    }
    nb::Signal signal() const {
        return {chain.synchronization, chain.serial, static_cast<int8_t>(chain.subserial)};
    }
    ModeledVkSubmitInfo modeled_submit() const {
        ModeledVkSubmitInfo v;
        v.pCommandBuffers[0] = owner.vk_command;
        v.pSignalSemaphores[0] = owner.semaphore;
        // Independent model of the native packed timeline value.
        v.timeline.pSignalSemaphoreValues[0] = (chain.serial << 8) |
            static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(chain.subserial)));
        return v;
    }
    nb::SubmitSnapshot snapshot() const { return normalized(modeled_submit(),owner); }
    nb::Scope q() const { return scoped(owner, chain.queue_scope); }
    nb::Scope st() const { return scoped(owner, chain.stamp_scope); }
    nb::Scope res(unsigned i) const { return scoped(owner, chain.resources[i]); }
    nb::Scope w() const { return scoped(owner, chain.wait_scope); }
    void queue() {
        assert(command.queue_enter(q(), snapshot(), signal()));
        assert(command.queue_result(q(), chain.queue_result));
    }
    void stamps() {
        assert(command.stamp_enter(q(), st(), signal()));
        for (unsigned i = 0; i < 2; ++i) {
            assert(command.resource_enter(st(), res(i), signal()));
            assert(command.resource_post(res(i), res(i).object, signal()));
        }
        assert(command.stamp_post(st(), owner.command, signal()));
    }
    void wait() {
        assert(command.wait_enter(w(), signal(), owner.semaphore));
        assert(command.wait_result(w(), signal(), chain.wait_result));
        assert(command.wait_return(w(), signal(), chain.wait_boolean));
    }
};
struct Integrated {
    Fixture source;
    Native native;
    Adapter adapter;
    Frame frame;
    explicit Integrated(const Chain &c = chains[0])
        : native(c), adapter(source.control, source.gate, native.binding, native.callbacks(), 2) {
        native.control = &source.control;
        assert(native.command.bind(native.owner));
        assert(adapter.initialize());
        PolicyInput p; p.enabled = true;
        frame = source.gate.begin(c.frame, p, true, true, false, now);
        assert(adapter.latch(frame));
    }
    void cpu() {
        assert(adapter.enqueue(source.eye(0)));
        assert(adapter.enqueue(source.eye(1)));
        assert(adapter.outer_return());
        for (unsigned i = 0; i < 2; ++i) {
            Context ctx{native.binding.identity, native.owner.recording};
            assert(adapter.backend(native.records[i], ctx, source.lookup()).decision == Decision::Portal);
            assert(native.command.backend_return(scoped(native.owner, native.chain.backend[i]), true));
        }
    }
    void queued() { cpu(); native.queue(); native.stamps(); }
};
static void six_chains() {
    for (const auto &c : chains) {
        Integrated f(c);
        assert(!f.adapter.submitted()); // no success from initialized or prepared-only state
        // The failed observation latches adapter invalid; use a fresh adapter for happy path.
        Integrated g(c); const auto allocations_before = binding_allocations.load(); g.queued();
        assert(g.adapter.submitted());
        assert(!g.adapter.poll_completion());
        g.native.wait();
        assert(g.adapter.poll_completion());
        assert(!g.adapter.reclaim());
        assert(!g.adapter.state().cpu_settled && g.adapter.state().tickets_used == 2);
        assert(g.native.command.state().success_debt); // CPU/executable debt never erased
        assert(!g.adapter.latch(g.frame));
        assert(binding_allocations.load() == allocations_before);
        std::printf("exact live03 frame %llu: queue/stamps/wait -> production adapter; no CPU settlement\n",
                    static_cast<unsigned long long>(c.frame));
    }
}
static void inactive() {
    nb::Command c;
    Completion out{9,9,9,9,9};
    assert(!c.submission({}, {}, out));
    assert(out.frame == 9);
    assert(!c.completed({}, {}));
    assert(!c.state().bound);
    Native n(chains[0]);
    assert(!c.queue_enter(n.q(), n.snapshot(), n.signal()));
    assert(!c.bind({}));
}
static void r1_and_duplicates() {
    for (unsigned mode = 0; mode < 4; ++mode) {
        Integrated f; f.cpu(); f.native.queue();
        if (mode == 0) { now = 100; f.native.command.off(); }
        if (mode == 1) { f.adapter.off(); f.adapter.off(); f.native.command.off(); }
        f.native.stamps();
        f.native.expire_inside = mode == 2;
        f.native.off_inside = mode == 3;
        assert(!f.adapter.submitted());
        assert(f.adapter.state().stereo_submitted);
        Completion saved;
        auto &n = f.native;
        assert(n.command.submission(n.provenances[0], n.provenances[1], saved));
        auto changed = n.provenances[1]; ++changed.ticket;
        Completion output{99,99,99,99,99};
        assert(!n.command.submission(n.provenances[0], changed, output));
        assert(output.frame == 99);
        assert(!f.adapter.submitted());
        n.wait();
        assert(f.adapter.poll_completion());
        assert(!f.adapter.reclaim());
        BackendStatus s; assert(f.source.control.status(s) && !s.submitted_frame);
        assert(n.command.state().success_debt && n.command.state().recording_debt);
    }
}
static void payload_and_results() {
    for (unsigned mode=0;mode<7;++mode) {
        Integrated f; f.cpu(); auto &n=f.native;
        auto v=n.modeled_submit();
        if(mode==0) v.commandBufferCount=2;
        if(mode==1) ++v.pCommandBuffers[0];
        if(mode==2) v.signalSemaphoreCount=2;
        if(mode==3) ++v.pSignalSemaphores[0];
        if(mode==4) ++v.timeline.pSignalSemaphoreValues[0];
        if(mode==5) v.timeline.signalSemaphoreValueCount=0;
        if(mode==6) v.timeline.terminal_chain=false;
        assert(!n.command.queue_enter(n.q(),normalized(v,n.owner),n.signal()));
        assert(!n.command.queue_result(n.q(),0));
        assert(n.command.state().success_debt);
    }
    for (unsigned mode = 0; mode < 14; ++mode) {
        Integrated f; f.cpu(); auto &n = f.native;
        auto p = n.snapshot();
        switch (mode) {
        case 0: p.readable = false; break;
        case 1: p.supported_chain = false; break;
        case 2: ++p.submit_count; break;
        case 3: ++p.command_count; break;
        case 4: ++p.signal_count; break;
        case 5: ++p.timeline_signal_count; break;
        case 6: ++p.queue; break;
        case 7: ++p.device; break;
        case 8: ++p.command; break;
        case 9: ++p.semaphore; break;
        case 10: ++p.value; break;
        case 11: ++p.fence; break;
        case 12: p.command_count = 0; break;
        case 13: p.signal_count = 0; break;
        }
        assert(!n.command.queue_enter(n.q(), p, n.signal()));
        assert(!n.command.queue_result(n.q(), 0)); // real work can succeed despite invalid metadata
        assert(n.command.state().success_debt);
        assert(!f.adapter.submitted());
        assert(!n.command.bind(n.owner));
    }
    for (int raw : {-4, 1, 2}) {
        Integrated f; f.cpu(); auto &n = f.native;
        assert(n.command.queue_enter(n.q(), n.snapshot(), n.signal()));
        assert(!n.command.queue_result(n.q(), raw));
        assert(!n.command.state().success_debt);
        assert(n.command.state().recording_debt);
        assert(!n.command.stamp_enter(n.q(), n.st(), n.signal()));
        assert(!f.adapter.submitted());
    }
    Integrated f; f.cpu(); auto &n = f.native;
    assert(n.command.queue_enter(n.q(), n.snapshot(), n.signal()));
    assert(!n.command.stamp_enter(n.q(), n.st(), n.signal())); // preparation is NOT result
    assert(!f.adapter.submitted());
}
static void mutate_scope(nb::Scope &s, unsigned i) {
    switch (i) {
    case 0: ++s.invocation; break; case 1: ++s.tid; break;
    case 2: ++s.sp; break; case 3: ++s.lr; break; case 4: ++s.object; break;
    case 5: ++s.owner.identity.process_instance; break;
    case 6: ++s.owner.identity.renderer; break;
    case 7: ++s.owner.identity.renderer_generation; break;
    case 8: ++s.owner.identity.device; break;
    case 9: ++s.owner.identity.device_generation; break;
    case 10: ++s.owner.recording; break; case 11: ++s.owner.generation; break;
    case 12: ++s.owner.command; break; case 13: ++s.owner.vk_command; break;
    case 14: ++s.owner.arena; break; case 15: ++s.owner.buffer; break;
    case 16: ++s.owner.map; break; case 17: ++s.owner.capacity; break;
    case 18: ++s.owner.alignment; break; case 19: ++s.owner.queue; break;
    case 20: ++s.owner.semaphore; break;
    case 21: ++s.owner.synchronization; break;
    }
}
static void exact_scopes_and_stamps() {
    for (unsigned mode = 0; mode < 22; ++mode) {
        Integrated f; f.cpu(); auto &n = f.native;
        assert(n.command.queue_enter(n.q(), n.snapshot(), n.signal()));
        auto bad = n.q(); mutate_scope(bad, mode);
        assert(!n.command.queue_result(bad, 0));
        assert(n.command.state().success_debt);
        assert(!f.adapter.submitted());
        Integrated g; g.queued(); auto &m = g.native;
        assert(m.command.wait_enter(m.w(), m.signal(), m.owner.semaphore));
        bad = m.w(); mutate_scope(bad, mode);
        assert(!m.command.wait_result(bad, m.signal(), 0));
        assert(!m.command.wait_return(m.w(), m.signal(), 1));
        assert(!g.adapter.poll_completion());
    }
    for (unsigned which = 0; which < 2; ++which) {
        for (unsigned mode = 0; mode < 6; ++mode) {
            Integrated f; f.cpu(); auto &n = f.native; n.queue();
            assert(n.command.stamp_enter(n.q(), n.st(), n.signal()));
            for (unsigned i = 0; i <= which; ++i) {
                assert(n.command.resource_enter(n.st(), n.res(i), n.signal()));
                auto signal = n.signal(); auto scope = n.res(i); auto object = scope.object;
                if (i == which) {
                    if (mode == 0) ++signal.synchronization;
                    if (mode == 1) ++signal.serial;
                    if (mode == 2) signal.subserial = -1;
                    if (mode == 3) ++object;
                    if (mode == 4) ++scope.invocation;
                    if (mode == 5) ++scope.owner.generation;
                    assert(!n.command.resource_post(scope, object, signal));
                } else assert(n.command.resource_post(scope, object, signal));
            }
            assert(!n.command.stamp_post(n.st(), n.owner.command, n.signal()));
            assert(n.command.state().success_debt && !n.command.state().certificate);
            assert(!f.adapter.submitted());
        }
    }
    Integrated f; f.cpu(); auto &n = f.native; n.queue();
    assert(n.command.stamp_enter(n.q(), n.st(), n.signal()));
    assert(!n.command.stamp_post(n.st(), n.owner.command, n.signal())); // arena missing
    assert(n.command.state().success_debt);
}
static void waits_and_immutable_tokens() {
    for (unsigned mode = 0; mode < 9; ++mode) {
        Integrated f; f.queued(); assert(f.adapter.submitted()); auto &n = f.native;
        auto s = n.signal(); auto w = n.w();
        if (mode == 0) { assert(!n.command.wait_return(w, s, 1)); }
        else if (mode == 1) { ++s.serial; assert(!n.command.wait_enter(w, s, n.owner.semaphore)); }
        else if (mode == 2) { ++w.object; assert(!n.command.wait_enter(w, s, n.owner.semaphore)); }
        else if (mode == 3) { assert(!n.command.wait_enter(w, s, n.owner.semaphore+1)); }
        else {
            assert(n.command.wait_enter(w, s, n.owner.semaphore));
            if (mode == 4) assert(!n.command.wait_return(w, s, 1)); // missing Vulkan call/result
            if (mode == 5 || mode == 6) {
                assert(!n.command.wait_result(w, s, mode == 5 ? 2 : -4));
                assert(!n.command.wait_return(w, s, 1));
            }
            if (mode == 7) {
                assert(n.command.wait_result(w,s,0));
                assert(!n.command.wait_return(w,s,2)); // Boolean must be exactly 1
            }
            if (mode == 8) {
                assert(n.command.wait_result(w,s,0)); ++s.serial;
                assert(!n.command.wait_return(w,s,1));
            }
        }
        assert(!f.adapter.poll_completion());
        assert(n.command.state().success_debt);
    }
    Integrated f; f.queued(); auto &n = f.native;
    Completion original;
    assert(n.command.submission(n.provenances[0],n.provenances[1],original));
    auto next = n.signal(); ++next.serial;
    assert(!n.command.stamp_enter(n.q(),n.st(),next));
    Completion kept;
    assert(n.command.submission(n.provenances[0],n.provenances[1],kept));
    assert(kept.serial == original.serial);
    n.wait();
    assert(n.command.completed(n.binding, original));
    ++kept.serial; assert(!n.command.completed(n.binding, kept));
}
static void resubmission_and_bound_storage() {
    static_assert(sizeof(nb::Command) < 4096, "fixed record grew unexpectedly");
    for (unsigned mode = 0; mode < 3; ++mode) {
        Integrated f; f.queued(); auto &n = f.native;
        if (mode == 1) { f.adapter.off(); n.command.off(); }
        if (mode == 2) n.obs.arena.cursor = 0; // reset-entry is no settlement proof
        auto q = n.q(); ++q.invocation;
        assert(!n.command.queue_enter(q,n.snapshot(),n.signal()));
        assert(n.command.state().unsupported && n.command.state().success_debt);
        n.wait();
        Completion c;
        assert(n.command.submission(n.provenances[0],n.provenances[1],c));
        assert(!n.command.completed(n.binding,c)); // old wait cannot cover repeated execution
        for (unsigned i=0;i<2000;++i) {
            assert(!n.command.bind(n.owner));
            assert(!f.adapter.reclaim());
            assert(!f.adapter.latch(f.frame));
        }
        assert(f.adapter.state().tickets_used == 2);
    }
}
static void owner_thread_fork_and_nested() {
    Integrated f; f.queued(); auto &n = f.native;
    std::thread t([&] {
        assert(!n.command.state().bound);
        assert(!n.command.queue_result(n.q(),0));
        n.command.off(); n.command.invalidate_lifetime();
        Completion c; assert(!n.command.submission(n.provenances[0],n.provenances[1],c));
    }); t.join();
    pid_t pid = fork(); assert(pid >= 0);
    if (!pid) {
        assert(!n.command.state().bound);
        assert(!n.command.wait_enter(n.w(),n.signal(),n.owner.semaphore));
        _exit(0);
    }
    int status; assert(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    assert(!n.command.state().stopped);
    n.wait(); assert(f.adapter.submitted()); assert(f.adapter.poll_completion());
    n.command.invalidate_lifetime();
    Completion c; assert(!n.command.submission(n.provenances[0],n.provenances[1],c));
    assert(!n.command.completed(n.binding,c));
    Integrated g; g.cpu(); auto &m = g.native;
    assert(m.command.queue_enter(m.q(),m.snapshot(),m.signal()));
    assert(!m.command.queue_enter(m.q(),m.snapshot(),m.signal()));
    assert(!m.command.queue_result(m.q(),0));
    assert(m.command.state().success_debt && m.command.state().unsupported);
}
static void immutable_provenance_and_signed_values() {
    Integrated f; f.queued(); auto &n = f.native;
    for (unsigned eye = 0; eye < 2; ++eye) {
        for (unsigned field = 0; field < 26; ++field) {
            auto p = n.provenances;
            auto &v = p[eye];
            uint64_t *words[] = {&v.identity.process_instance,&v.identity.renderer,
                &v.identity.renderer_generation,&v.identity.device,&v.identity.device_generation,
                &v.ticket,&v.frame,&v.lease,&v.origin,&v.geometry,&v.deadline,&v.revocation,
                &v.recording_generation,&v.wrapper,&v.marker,&v.descriptors[0].buffer,
                &v.descriptors[0].offset,&v.descriptors[0].range,&v.descriptors[1].buffer,
                &v.descriptors[1].offset,&v.descriptors[1].range};
            if (field < 21) ++*words[field];
            else if (field == 21) v.eye ^= 1;
            else if (field == 22) ++v.vs;
            else if (field == 23) ++v.ps;
            else if (field == 24) p[eye] = p[eye ^ 1];
            else std::swap(p[0],p[1]);
            Completion out{99,99,99,99,99};
            assert(!n.command.submission(p[0],p[1],out));
            assert(out.frame == 99);
        }
    }
    assert(nb::Command::timeline_value({1,600,1}) == 153601);
    assert(nb::Command::timeline_value({1,600,-2}) == UINT64_MAX-1);
    assert(nb::Command::timeline_value({1,600,-128}) == UINT64_MAX-127);
    // These are synthetic sign-boundary cases, not additional live chains.
    for (int8_t sub : {int8_t{-2}, int8_t{-128}, int8_t{0}, int8_t{127}}) {
        Chain c = chains[0]; c.subserial = sub;
        Integrated g(c); g.queued(); assert(g.adapter.submitted());
        g.native.wait(); assert(g.adapter.poll_completion());
    }
    for (unsigned mode = 0; mode < 22; ++mode) {
        nb::Command c; assert(c.bind(n.owner));
        auto s = scoped(n.owner,n.chain.backend[0]);
        assert(c.backend_enter(s,n.provenances[0]));
        mutate_scope(s,mode);
        assert(!c.backend_return(s,true));
        assert(!c.queue_enter(n.q(),n.snapshot(),n.signal()));
        assert(c.state().recording_debt);
    }
    nb::Command c; assert(c.bind(n.owner));
    auto s = scoped(n.owner,n.chain.backend[0]);
    assert(c.backend_enter(s,n.provenances[0]));
    assert(!c.backend_enter(s,n.provenances[0])); // nested backend scope
    assert(!c.backend_return(s,true));
    assert(c.state().recording_debt);
}
static void new_regressions() {
    // A semaphore value alone cannot authenticate a different native sync resource.
    Integrated f; f.cpu(); auto &n = f.native;
    auto token = n.signal(); ++token.synchronization;
    assert(!n.command.queue_enter(n.q(),n.snapshot(),token));
    assert(!n.command.queue_result(n.q(),0));
    assert(n.command.state().success_debt);
}
static void late_lifetime_observation() {
    // Off cannot conceal a real queue call for the already-recorded pair.
    Integrated f; f.cpu(); auto &n=f.native;
    f.adapter.off(); n.command.off();
    assert(n.command.queue_enter(n.q(),n.snapshot(),n.signal()));
    assert(n.command.queue_result(n.q(),0));
    n.stamps(); assert(!f.adapter.submitted());
    assert(f.adapter.state().stereo_submitted);
    n.wait(); assert(f.adapter.poll_completion()); assert(!f.adapter.reclaim());
    // Lifetime revocation forbids publication, not conservative debt bookkeeping.
    Integrated g; g.cpu(); auto &m=g.native;
    assert(m.command.queue_enter(m.q(),m.snapshot(),m.signal()));
    m.command.invalidate_lifetime();
    assert(!m.command.queue_result(m.q(),0));
    assert(m.command.state().success_debt);
    assert(!g.adapter.submitted());
}
int main() {
    late_lifetime_observation();
    new_regressions();
    immutable_provenance_and_signed_values();
    inactive();
    six_chains();
    r1_and_duplicates();
    payload_and_results();
    exact_scopes_and_stamps();
    waits_and_immutable_tokens();
    resubmission_and_bound_storage();
    owner_thread_fork_and_nested();
    assert(render_original_main(1,nullptr) == 0);
    std::printf("binding fixed storage bytes=%zu; zero allocation lifecycle checks passed\n", sizeof(nb::Command));
    std::puts("binding tests passed");
}
