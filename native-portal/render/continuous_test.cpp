// SPDX-License-Identifier: MIT
// Reuse real production-path fixtures, then run the unchanged R1 suite and trace replay.
#define main original_suite_main
#include "test.cpp"
#undef main
#include <cstdlib>
#include <new>

static std::atomic<size_t> allocations{0};
void *operator new(size_t n) {
    ++allocations;
    if (void *p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }

static bool exact(const Completion &a, const Completion &b) {
    return a.frame == b.frame && a.command == b.command &&
           a.synchronization == b.synchronization && a.serial == b.serial &&
           a.subserial == b.subserial;
}
struct ContinuousNative : QueueObserver {
    bool cpu_settled = false, invalidate_settlement = false;
    Adapter *reclaim_inside = nullptr;
    unsigned settlements = 0;
    Completion settled_token{};
    std::array<uint64_t, 2> settled_tickets{};
    Backend *off_inside = nullptr;
    uint64_t expire_at = 0;
    static bool submission_read(void *u, const Provenance &a, const Provenance &b,
                                Completion &out) noexcept {
        auto &f = *static_cast<ContinuousNative *>(u);
        bool ok = submission_queue(&f, a, b, out);
        if (f.off_inside) f.off_inside->invalidate(Reason::Off);
        if (f.expire_at) now = f.expire_at;
        return ok;
    }
    static bool settled(void *u, const Binding &binding, const Provenance &a,
                        const Provenance &b, const Completion &token) noexcept {
        auto &f = *static_cast<ContinuousNative *>(u);
        ++f.settlements;
        assert(binding.identity.device == f.binding.identity.device);
        assert(a.frame == token.frame && b.frame == token.frame && a.eye == 0 && b.eye == 1);
        assert(a.ticket == f.settled_tickets[0] && b.ticket == f.settled_tickets[1]);
        assert(a.recording_generation == b.recording_generation);
        assert(exact(token, f.settled_token));
        if (f.invalidate_settlement) f.valid = false;
        if (f.reclaim_inside) assert(!f.reclaim_inside->reclaim());
        return f.cpu_settled;
    }
    Callbacks callbacks(bool settlement = true) {
        auto c = QueueObserver::callbacks(true);
        c.user = this;
        c.submission = submission_read;
        c.settled = settlement ? settled : nullptr;
        return c;
    }
};
struct ContinuousFixture {
    Fixture source;
    ContinuousNative native;
    Adapter adapter;
    Frame frame;
    explicit ContinuousFixture(size_t capacity = 2, bool settlement = true, uint64_t first = 1)
        : adapter(source.control, source.gate, native.binding, native.callbacks(settlement), capacity, first) {
        assert(adapter.initialize());
    }
    void begin(uint64_t id, bool fresh_lease = true) {
        now += 10;
        if (fresh_lease) {
            source.lease.deadline_ns = now + 100;
            ++source.lease.epoch;
            source.lease.revocation = source.control.revocation();
            assert(source.control.publish_lease(source.lease));
        }
        PolicyInput policy;
        policy.enabled = true;
        frame = source.gate.begin(id, policy, true, true, false, now);
        assert(adapter.latch(frame));
        native.uploads = native.enqueues = 0;
        native.obs.arena.cursor = 0;
        native.obs.arena.recording_generation = id;
        native.queued = native.complete = native.cpu_settled = false;
    }
    Context context() { return {native.binding.identity, native.obs.arena.recording_generation}; }
    BackendResult draw(unsigned eye) {
        return adapter.backend(native.records[eye], context(), source.lookup());
    }
    void stereo() {
        for (unsigned eye = 0; eye < 2; ++eye) {
            if (!adapter.enqueue(source.eye(eye))) {
                std::fprintf(stderr, "sustained frame %llu eye %u refused: resident=%zu\n",
                             static_cast<unsigned long long>(frame.id), eye,
                             adapter.state().tickets_used);
                std::abort();
            }
            assert(decode_ticket(native.records[eye].label, 15, native.settled_tickets[eye]));
        }
        assert(adapter.outer_return());
    }
    void cpu() {
        assert(draw(0).decision == Decision::Portal);
        assert(draw(1).decision == Decision::Portal);
    }
    void queue_success() {
        native.queued = true;
        native.token = {frame.id, native.obs.arena.command, 0x901, frame.id, -2};
        native.settled_token = native.token;
    }
    void submit() {
        stereo();
        cpu();
        queue_success();
        assert(adapter.submitted());
    }
    void complete() {
        native.complete = true;
        assert(adapter.poll_completion());
        assert(exact(native.observed_completion, native.settled_token));
    }
    void settled() {
        native.cpu_settled = true;
        assert(adapter.reclaim());
        assert(adapter.state().cpu_settled && !adapter.state().tickets_used);
    }
};
static void sustained_storage() {
    for (size_t capacity : {2u, 4u}) {
        ContinuousFixture f(capacity, true, uint64_t{1} << 63);
        const auto allocation_count = allocations.load();
        uint64_t last_ticket = 0;
        for (uint64_t frame_id = 1; frame_id <= 10000; ++frame_id) {
            f.begin(frame_id);
            f.submit();
            for (auto ticket : f.native.settled_tickets) {
                assert(ticket > last_ticket && ticket >= (uint64_t{1} << 63));
                last_ticket = ticket;
            }
            assert(f.adapter.state().tickets_used == 2);
            assert(f.adapter.state().tickets_issued == frame_id * 2);
            assert(f.adapter.state().tickets_capacity == capacity);
            assert(!f.adapter.poll_completion());
            assert(!f.adapter.reclaim());
            f.complete();
            assert(!f.adapter.reclaim()); // both CPU admissions are NOT settlement
            assert(f.adapter.state().tickets_used == 2);
            f.settled();
            const auto calls = f.native.settlements;
            assert(f.adapter.reclaim()); // idempotent, does not reobserve released storage
            assert(f.native.settlements == calls);
            assert(allocations == allocation_count); // no allocations or growing logs per frame
        }
        assert(f.native.allocations == 1 && !f.native.destroys);
        std::printf("10000 stereo frames: capacity=%zu, resident<=2, issued=20000, hot allocations=0\n",
                    capacity);
    }
}
static void settlement_required() {
    for (unsigned mode = 0; mode < 5; ++mode) {
        ContinuousFixture f(4, mode != 0);
        f.begin(1);
        f.submit();
        f.complete();
        f.native.cpu_settled = mode != 1;
        if (mode == 2) ++f.native.binding.identity.renderer_generation;
        if (mode == 3) ++f.native.binding.identity.device_generation;
        if (mode == 4) ++f.native.binding.identity.process_instance;
        assert(!f.adapter.reclaim());
        assert(f.adapter.state().tickets_used == 2 && !f.adapter.state().cpu_settled);
        const auto records = f.native.records;
        PolicyInput p;
        p.enabled = true;
        auto next = f.source.gate.begin(2, p, true, true, false, now);
        assert(!f.adapter.latch(next)); // free spare slots cannot hide unsettled ownership
        assert(!f.adapter.enqueue(f.source.eye(0)));
        assert(f.native.enqueues == 2 && f.native.uploads == 2);
        assert(!std::memcmp(records.data(), f.native.records.data(), sizeof records));
        assert(f.adapter.state().tickets_used == 2 && f.adapter.state().tickets_issued == 2);
    }
}
static void settlement_owner_guards() {
    // Existing owner-thread/recursion/process contract also covers the new entrypoint.
    for (unsigned mode = 0; mode < 3; ++mode) {
        ContinuousFixture f;
        f.begin(1);
        f.submit();
        f.complete();
        f.native.cpu_settled = true;
        if (mode == 0) {
            f.native.invalidate_settlement = true;
            assert(!f.adapter.reclaim());
        } else if (mode == 1) {
            std::thread other([&] { assert(!f.adapter.reclaim()); });
            other.join();
            assert(!f.native.settlements);
        } else {
            f.native.reclaim_inside = &f.adapter;
            // Outer settlement may complete under native lifetime ownership;
            // recursion refuses control authorization, not valid lifetime evidence.
            assert(f.adapter.reclaim());
            BackendStatus s;
            assert(!f.source.control.status(s));
        }
        if (mode != 2) assert(f.adapter.state().tickets_used == 2);
    }
}
static void stopped_bounds() {
    // Off, missing/malformed queue evidence, partial admission and failed native
    // calls never free or partially overwrite the pre-reserved stereo slots.
    for (unsigned mode = 0; mode < 9; ++mode) {
        ContinuousFixture f(4);
        f.begin(1);
        if (mode < 3) {
            f.native.fail = mode == 0 ? 0x78 : mode == 1 ? 0x2988a8 : 0x228;
            assert(!f.adapter.enqueue(f.source.eye(0)));
        } else {
            f.stereo();
            if (mode != 3) f.cpu();
            else assert(f.draw(0).decision == Decision::Portal);
            if (mode == 5 || mode == 6 || mode == 7) f.queue_success();
            if (mode == 6) f.native.token.synchronization = 0;
            if (mode == 7) f.native.token.serial = 0;
            if (mode != 8) assert(f.adapter.submitted() == (mode == 5));
        }
        f.native.cpu_settled = true;
        assert(!f.adapter.reclaim());
        f.adapter.off();
        f.native.retire_ok = true;
        assert(!f.adapter.poll_completion());
        assert(!f.adapter.retire());
        const auto enqueues = f.native.enqueues, uploads = f.native.uploads;
        for (uint64_t attempt = 2; attempt < 1002; ++attempt) {
            assert(!f.adapter.reclaim());
            PolicyInput p;
            p.enabled = true;
            auto next = f.source.gate.begin(attempt, p, true, true, false, now);
            assert(!f.adapter.latch(next));
            assert(!f.adapter.enqueue(f.source.eye(0)));
            assert(f.adapter.state().tickets_used == 2);
            assert(f.adapter.state().tickets_issued == 2);
        }
        assert(!f.native.settlements && !f.native.destroys);
        assert(f.native.enqueues == enqueues && f.native.uploads == uploads);
    }
}
static void late_records() {
    for (unsigned when = 0; when < 3; ++when) {
        ContinuousFixture f;
        f.begin(1);
        f.submit();
        auto old = f.native.records[0];
        auto old_context = f.context();
        f.complete();
        f.settled();
        if (when == 1) { f.begin(2); f.stereo(); }
        if (when == 2) {
            f.adapter.off();
            f.native.retire_ok = true;
            assert(f.adapter.retire());
        }
        const auto lookups = f.native.lookups;
        assert(f.adapter.backend(old, old_context, f.source.lookup()).decision == Decision::Drop);
        assert(f.native.lookups == lookups); // never stock-forward, even after marker retirement
        if (when == 1) {
            // Even putting the old identity on a fully current packet cannot borrow its slot.
            auto forgery = f.native.records[0];
            forgery.label = old.label;
            assert(f.adapter.backend(forgery, f.context(), f.source.lookup()).decision == Decision::Drop);
            assert(f.draw(0).decision == Decision::Drop && f.draw(1).decision == Decision::Drop);
            assert(f.native.lookups == lookups);
            assert(f.adapter.state().tickets_used == 2 && f.adapter.state().tickets_issued == 4);
        }
    }
}
static void r1_reclamation() {
    for (unsigned mode = 0; mode < 4; ++mode) {
        ContinuousFixture f;
        f.begin(1);
        f.stereo();
        f.cpu();
        f.queue_success();
        const Completion original = f.native.token;
        if (mode == 0) now = f.frame.lease.deadline_ns;
        if (mode == 1) f.adapter.off();
        if (mode == 2) f.native.expire_at = f.frame.lease.deadline_ns;
        if (mode == 3) f.native.off_inside = &f.source.control;
        assert(!f.adapter.submitted());
        assert(f.adapter.state().stereo_submitted);
        BackendStatus state;
        assert(f.source.control.status(state) && !state.submitted_frame);
        ++f.native.token.serial;
        ++f.native.token.subserial;
        assert(!f.adapter.submitted() && f.native.submissions == 1);
        assert(!f.adapter.reclaim());
        f.complete();
        assert(exact(f.native.observed_completion, original));
        assert(!f.adapter.reclaim());
        f.settled();
        // Expired or revoked admission must not revive just because storage is free.
        PolicyInput p;
        p.enabled = true;
        auto invalid = f.source.gate.begin(2, p, true, true, false, now);
        assert(!f.adapter.latch(invalid));
        assert(!f.adapter.enqueue(f.source.eye(0)));
        assert(!f.adapter.state().tickets_used);
        f.native.expire_at = 0;
        f.native.off_inside = nullptr;
        f.source.control.settled(); // synthetic native owner explicitly reopens lease admission
        f.begin(3);
        f.submit();
        f.complete();
        f.settled();
        f.adapter.off();
        f.native.retire_ok = true;
        assert(f.adapter.retire() && f.native.destroys == 1);
    }
}
int main(int argc, char **argv) {
    sustained_storage();
    settlement_required();
    settlement_owner_guards();
    stopped_bounds();
    late_records();
    r1_reclamation();
    return original_suite_main(argc, argv);
}
