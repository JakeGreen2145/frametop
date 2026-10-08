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
// Pass endpoints transcribed from live03 camera_enqueue pairs, trace SHA256
// 29705cfc2019d5c22f9cbf335a5312871d55c23e7f3be6db6beae94d3614d123.
// Native operations, lease, generation and backend arena observations are synthetic.
static void live_pass_appends() {
    struct Pair {
        uint64_t frame, begin, left_end, right_end, left_cursor, right_cursor, command;
    };
    constexpr Pair pairs[] = {
        {601, 0xffff603c5470, 0xffff603c5638, 0xffff603c56d0, 1536, 3072, 0xaaaaef535810},
        {602, 0xffff603d55b0, 0xffff603d5778, 0xffff603d5810, 1536, 3072, 0xaaaaef368ae0},
        {603, 0xffff603c5470, 0xffff603c5638, 0xffff603c56d0, 1536, 3072, 0xaaaaef535810},
        {743, 0xffff603c5470, 0xffff603c56d0, 0xffff603c5768, 2816, 5632, 0xaaaaef368ae0},
        {744, 0xffff603d55b0, 0xffff603d5810, 0xffff603d58a8, 2816, 5632, 0xaaaaef5432f0},
        {745, 0xffff603c5470, 0xffff603c56d0, 0xffff603c5768, 2816, 5632, 0xaaaaef368ae0},
    };
    bool all = true;
    for (const auto &pair : pairs) {
        ContinuousFixture f;
        f.begin(pair.frame);
        auto &a = f.native.obs.arena;
        a.pass_begin = pair.begin;
        a.pass_end = pair.left_end;
        a.cursor = pair.left_cursor;
        a.command = pair.command;
        const auto generation = a.recording_generation;
        assert(f.adapter.enqueue(f.source.eye(0)));
        a.pass_end = pair.right_end; // expose the REAL append; never freeze the reader
        a.cursor = pair.right_cursor + 640; // synthetic native work plus left portal upload
        const bool right = f.adapter.enqueue(f.source.eye(1));
        std::printf("live03 frame %llu: left=1 right=%d uploads=%u enqueues=%u resident=%zu\n",
                    static_cast<unsigned long long>(pair.frame), right, f.native.uploads,
                    f.native.enqueues, f.adapter.state().tickets_used);
        std::fflush(stdout);
        all = all && right;
        if (!right) continue;
        assert(a.recording_generation == generation && a.pass_begin == pair.begin &&
               a.pass_end == pair.right_end);
        assert(f.native.uploads == 2 && f.native.enqueues == 2);
        for (unsigned eye = 0; eye < 2; ++eye)
            assert(decode_ticket(f.native.records[eye].label, 15, f.native.settled_tickets[eye]));
        assert(f.adapter.outer_return());
        assert(f.native.obs.wrapper_offset == f.native.records[1].descriptors[0].offset);
        assert(f.native.obs.wrapper_offset != f.native.records[0].descriptors[0].offset);
        // Additional same-begin appends before and between deferred backends are
        // synthetic coverage, not fields captured at those live03 boundaries.
        a.pass_end += 0x98;
        a.cursor += 64;
        assert(f.draw(0).decision == Decision::Portal); // shared wrapper is already right
        a.pass_end += 0x98;
        a.cursor += 64;
        assert(f.draw(1).decision == Decision::Portal);
        f.queue_success();
        assert(f.adapter.submitted());
        f.complete();
        f.settled();
    }
    assert(all && "live native pass append must admit both eyes");
}
static void pass_structure_refusals() {
    for (unsigned mode = 0; mode < 8; ++mode) {
        Fixture f;
        f.start();
        auto &a = f.fake.obs.arena;
        switch (mode) {
        case 0: a.pass_begin = 0; break;
        case 1: a.pass_end = a.pass_begin; break;
        case 2: a.pass_end = a.pass_begin - 8; break;
        case 3: a.pass_end = a.pass_begin + 8; break;
        case 4: a.pass_end += 8; break; // aligned pointers, nonintegral stride
        case 5: ++a.pass_begin; ++a.pass_end; break; // valid span, misaligned pointers
        case 6: a.pass_begin = UINT64_MAX - 7; a.pass_end = 0x90; break; // wrapped append
        case 7: a.pass_begin = 8; a.pass_end = 0x8000000000000068; break; // oversized span
        }
        assert(!f.adapter.enqueue(f.eye(0)));
        assert(!f.fake.uploads && !f.fake.enqueues);
    }
}
static void mutate_recording(Arena &a, unsigned mode) {
    switch (mode) {
    case 0: a.pass_end -= 0x98; break; // structurally valid shrink
    case 1: a.pass_end += 8; break; // wrong stride
    case 2: a.pass_begin += 0x1000; a.pass_end += 0x1000; break; // unaudited relocation
    case 3: a.cursor = 0; break; // native reset under the old generation
    case 4: ++a.recording_generation; break;
    case 5: ++a.command; break;
    case 6: ++a.command_wrapper; break;
    case 7: ++a.resource; break;
    case 8: ++a.buffer; break;
    case 9: a.map += 64; break;
    case 10: a.capacity *= 2; break;
    case 11: a.alignment *= 2; break;
    case 12: a.cursor = a.capacity + 64; break;
    case 13: ++a.cursor; break;
    case 14: a.recording_generation = 0; break;
    case 15: --a.recording_generation; break;
    }
}
static void pass_progression_refusals() {
    // Check between eyes, before first backend, and after an accepted backend
    // advanced the observed cursor/extent. Never permit rollback to an older bound.
    for (unsigned phase = 0; phase < 3; ++phase) {
        for (unsigned mode = 0; mode < 16; ++mode) {
            Fixture f;
            f.start();
            assert(f.adapter.enqueue(f.eye(0)));
            auto &a = f.fake.obs.arena;
            if (phase) {
                a.pass_end += 0x98;
                assert(f.adapter.enqueue(f.eye(1)));
                assert(f.adapter.outer_return());
                if (phase == 2) {
                    a.pass_end += 0x98;
                    a.cursor += 128;
                    assert(f.draw(0).decision == Decision::Portal);
                }
            }
            const auto uploads = f.fake.uploads, lookups = f.fake.lookups;
            mutate_recording(a, mode);
            if (!phase) assert(!f.adapter.enqueue(f.eye(1)));
            else assert(f.draw(phase == 2 ? 1 : 0).decision == Decision::Drop);
            assert(f.adapter.state().frame_invalid);
            assert(f.fake.uploads == uploads && f.fake.lookups == lookups);
        }
    }
    Fixture rollback;
    rollback.start();
    rollback.stereo();
    rollback.fake.obs.arena.cursor += 128;
    assert(rollback.draw(0).decision == Decision::Portal);
    rollback.fake.obs.arena.cursor -= 64; // still above uploads, below latest observation
    assert(rollback.draw(1).decision == Decision::Drop);
}
static void upload_pass_mutation_refusals() {
    for (auto mutation : {
             +[](Arena &a) { a.pass_end += 0x98; }, // valid append only OUTSIDE upload
             +[](Arena &a) { a.pass_end -= 0x98; },
             +[](Arena &a) { a.pass_begin += 0x1000; a.pass_end += 0x1000; },
             +[](Arena &a) { ++a.recording_generation; },
             +[](Arena &a) { ++a.command; }}) {
        Fixture f;
        f.start();
        f.fake.during_upload = mutation;
        assert(!f.adapter.enqueue(f.eye(0)));
        assert(f.fake.uploads == 1 && !f.fake.enqueues);
        assert(f.adapter.state().frame_invalid);
    }
}
static void recycled_generation_refusals() {
    for (unsigned mode = 0; mode < 2; ++mode) {
        ContinuousFixture f;
        f.begin(10);
        f.submit();
        f.complete();
        f.settled();
        f.begin(11);
        f.native.obs.arena.recording_generation = mode ? 9 : 10;
        assert(!f.adapter.enqueue(f.source.eye(0)));
        assert(!f.native.uploads && !f.native.enqueues);
    }
}
int main(int argc, char **argv) {
    live_pass_appends();
    pass_structure_refusals();
    pass_progression_refusals();
    upload_pass_mutation_refusals();
    recycled_generation_refusals();
    sustained_storage();
    settlement_required();
    settlement_owner_guards();
    stopped_bounds();
    late_records();
    r1_reclamation();
    return original_suite_main(argc, argv);
}
