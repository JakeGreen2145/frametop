// SPDX-License-Identifier: MIT
// Synthetic native-operation boundary only. FrameGate, geometry, packet cloning,
// managed guards, label parsing and admission all execute production code.
#include "adapter.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <thread>
#include <unistd.h>
using namespace native_portal;
using namespace native_portal::render;
static uint64_t now = 10;
static uint64_t clock_now() { return now; }
template <class T, class A> static void put(A &a, size_t o, T v) {
    std::memcpy(a.data() + o, &v, sizeof v);
}
template <class T, class A> static T get(const A &a, size_t o) {
    T v;
    std::memcpy(&v, a.data() + o, sizeof v);
    return v;
}
struct Fake {
    Binding binding{{123, 0x100, 1, 0x200, 1}, {0x60, 0x78, 0x2988a8, 0x228, 0xb0, 0x1a8}, 991};
    Observation obs;
    Address wrapper = 0x400, marker = 0x500;
    bool valid = true, budget = true, blend_ok = true, pipeline_ok = true, queue_ok = true,
         complete = false, retire_ok = false;
    Address expected_stock = 0xabc;
    bool bad_queue_command = false, retire_marker_ok = true;
    Address fail = 0;
    bool wrong_descriptor = false, wrong_upload = false;
    unsigned allocations = 0, uploads = 0, enqueues = 0, destroys = 0, lookups = 0;
    std::array<Recorded, 32> records{};
    std::array<ft::portal::PortalConstants, 32> payloads{};
    Adapter *reenter = nullptr;
    Fake() {
        obs.arena = {1, 2, 3, 4, 5, 6, 0x100000, 0x80000, 0, 64, 7};
        obs.wrapper_resource = 0x600;
        obs.wrapper_size = 592;
        obs.wrapper_offset = UINT32_MAX;
        obs.attachment = {1, 1, 7, 0, 1, 7, 0, 15};
    }
    static bool validate(void *u, const Binding &, Phase p) noexcept {
        auto &f = *(Fake *)u;
        return f.valid && (p != Phase::Retire || f.retire_ok);
    }
    static bool observe(void *u, const Binding &, Address, Address, Observation &out) noexcept {
        out = ((Fake *)u)->obs;
        return true;
    }
    static bool headroom(void *u, const Binding &b, const Arena &, uint64_t) noexcept {
        return b.headroom_evidence && ((Fake *)u)->budget;
    }
    static bool stock(void *u, const Binding &, Address a) noexcept {
        return a == ((Fake *)u)->expected_stock && ((Fake *)u)->blend_ok;
    }
    static bool call(void *u, Registers &r) noexcept {
        auto &f = *(Fake *)u;
        if (r.target == f.fail)
            return false;
        assert(r.x[0] == f.binding.identity.renderer);
        if (r.target == 0x60) {
            assert(r.x[1] == 592 && !r.x[2] && !r.x[3] && !r.x[4] && !r.x[5]);
            ++f.allocations;
            r.x[0] = f.wrapper;
        } else if (r.target == 0x1a8) {
            const std::array<uint32_t, 6> expected{1, 1, 3, 1, 3, 0};
            assert(!std::memcmp((void *)r.x[1], expected.data(), 24));
            r.x[0] = f.marker;
        } else if (r.target == 0x78) {
            assert(r.x[1] == f.wrapper);
            assert(f.uploads < f.payloads.size());
            std::memcpy(f.payloads[f.uploads++].data(), (void *)r.x[2], 592);
            if (f.reenter) {
                assert(!f.reenter->outer_return());
                f.reenter = nullptr;
            }
            f.obs.wrapper_offset = f.obs.arena.cursor + (f.wrong_upload ? 1 : 0);
            f.obs.arena.cursor = (f.obs.arena.cursor + 592 + f.obs.arena.alignment - 1) &
                                 ~(f.obs.arena.alignment - 1);
            r.x[0] = 0; // upload has NO native boolean result
        } else if (r.target == 0x2988a8) {
            assert(r.x[2] == f.wrapper);
            Descriptor d{f.obs.arena.buffer, f.obs.wrapper_offset,
                         f.wrong_descriptor ? 528u : 592u};
            std::memcpy((void *)r.x[1], &d, sizeof d);
        } else if (r.target == 0x228) {
            assert(r.x[4] == r.target);
            assert(f.enqueues < f.records.size());
            auto &rec = f.records[f.enqueues++];
            std::memcpy(rec.prefix.data(), (void *)r.x[1], 352);
            std::memcpy(rec.label.data(), (void *)r.x[2], 16);
            rec.label_size = 15;
            rec.descriptors[0] =
                rec.descriptors[4] = {f.obs.arena.buffer, f.obs.wrapper_offset, 592};
        } else if (r.target == 0xb0) {
            assert(*(Address *)r.x[1] == f.wrapper);
            *(Address *)r.x[1] = 0;
            ++f.destroys;
        } else if (r.target == 0x999) {
            ++f.lookups;
            r.x[0] = 0x888;
        } else
            assert(false);
        return true;
    }
    static bool pipeline(void *u, const Provenance &p, Registers &r) noexcept {
        auto &f = *(Fake *)u;
        assert(p.marker == f.marker && p.wrapper == f.wrapper);
        assert(p.identity.device == f.binding.identity.device && supported_pair(p.vs, p.ps));
        assert(p.descriptors[0].range == 592 && p.descriptors[1].range == 592);
        ++f.lookups;
        r.x[0] = 0x777;
        return f.pipeline_ok;
    }
    static bool submission(void *u, const Provenance &a, const Provenance &b,
                           Completion &c) noexcept {
        auto &f = *(Fake *)u;
        assert(a.eye == 0 && b.eye == 1 && a.frame == b.frame);
        c = {a.frame, f.bad_queue_command ? 44u : 4u, 0x900, 22, 1};
        return f.queue_ok;
    }
    static bool completed(void *u, const Binding &, const Completion &c) noexcept {
        assert(c.synchronization == 0x900 && c.serial == 22);
        return ((Fake *)u)->complete;
    }
    static bool retire_marker(void *u, const Binding &, Address m) noexcept {
        assert(m == 0x500);
        return ((Fake *)u)->retire_marker_ok;
    }
    Callbacks callbacks() {
        return {this,     validate,   call,      observe,       headroom, stock,
                pipeline, submission, completed, retire_marker, clock_now};
    }
};
struct Fixture {
    Backend control;
    FrameGate gate{control};
    Fake fake;
    Adapter adapter;
    Lease lease;
    Frame frame;
    Fixture(size_t cap = 16, uint64_t first = 1, bool capable = true)
        : adapter(control, gate, fake.binding, fake.callbacks(), cap, first) {
        now = 10;
        BackendStatus s;
        s.capable = s.serialized = s.external_clear = capable;
        s.origin = 4;
        assert(control.publish(s));
        lease.epoch = 8;
        lease.origin = 4;
        lease.generation = 2;
        lease.deadline_ns = 100;
        lease.flags = 1;
        lease.geometry.center[1] = -2.5f;
        assert(control.publish_lease(lease));
    }
    void start(uint64_t id = 1) {
        assert(adapter.initialize());
        PolicyInput p;
        p.enabled = true;
        frame = gate.begin(id, p, true, true, false, now);
        assert(adapter.latch(frame));
    }
    EyeInput eye(unsigned eye, uint32_t vs = 58, uint32_t ps = 130) {
        EyeInput in;
        in.eye = eye;
        in.observed_origin = 4;
        in.raw_to_standing = ft::portal::Mat4::identity();
        for (size_t i = 0; i < in.prefix.size(); ++i)
            in.prefix[i] = std::byte((i * 17 + eye) % 256);
        put(in.prefix, 8, Address{0xabc});
        put(in.prefix, 0x20, vs);
        put(in.prefix, 0x30, ps);
        for (size_t i = 0; i < in.constants.size(); ++i)
            in.constants[i] = std::byte((i * 13 + eye) % 256);
        auto m = ft::portal::Mat4::identity();
        auto p = m;
        p.v = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, -.1, 0, 0, -1, 0};
        for (size_t i = 0; i < 16; ++i) {
            put(in.constants, 64 + i * 4, float(m.v[i]));
            put(in.constants, 128 + i * 4, float(p.v[i]));
        }
        return in;
    }
    Context context() { return {fake.binding.identity, 7}; }
    Registers lookup() {
        Registers r;
        r.target = 0x999;
        r.x = {0x100, 0x1234, 0x5678, 0x9abc, 0, 0, 0, 0};
        return r;
    }
    BackendResult draw(unsigned eye) {
        return adapter.backend(fake.records[eye], context(), lookup());
    }
    void stereo() {
        assert(adapter.enqueue(eye(0)));
        assert(adapter.enqueue(eye(1)));
        assert(adapter.outer_return());
    }
};
static void initialization() {
    Fixture f;
    assert(f.adapter.initialize());
    assert(f.fake.allocations == 1);
    assert(f.adapter.initialize());
    assert(f.fake.allocations == 1);
    Fixture unavailable(16, 1, false);
    assert(!unavailable.adapter.initialize());
    assert(!unavailable.fake.allocations);
    Fixture invalid;
    invalid.fake.valid = false;
    assert(!invalid.adapter.initialize());
    assert(!invalid.fake.allocations);
    Fixture bad;
    bad.fake.obs.attachment[2] = 1;
    assert(!bad.adapter.initialize());
    assert(!bad.fake.destroys);
    Fixture null;
    null.fake.wrapper = 0;
    assert(!null.adapter.initialize());
}
static void stereo_bytes_and_deferred() {
    Fixture f;
    f.start();
    auto left = f.eye(0);
    auto right = f.eye(1, 57, 127);
    assert(f.adapter.enqueue(left));
    assert(f.adapter.enqueue(right));
    assert(f.adapter.outer_return()); // keep FrameGate alive through actual deferred schedule
    for (unsigned eye = 0; eye < 2; ++eye) {
        auto original = eye ? right : left;
        const auto &rec = f.fake.records[eye];
        auto expected = original.prefix;
        put(expected, 8, f.fake.marker);
        put(expected, 0x38, f.fake.wrapper);
        put(expected, 0x58, f.fake.wrapper);
        assert(rec.prefix == expected);
        assert(!std::memcmp(f.fake.payloads[eye].data(), original.constants.data(), 528));
        assert(get<uint32_t>(f.fake.payloads[eye], 588) & 1);
        assert(rec.descriptors[0].range == 592 && rec.descriptors[4].range == 592);
        assert(rec.descriptors[0].offset == eye * 640);
        auto relocated = rec; // no stored CPU packet address participates in identity
        assert(f.adapter.backend(relocated, f.context(), f.lookup()).decision == Decision::Portal);
    }
    assert(f.adapter.state().cpu_done && !f.adapter.state().stereo_submitted);
    BackendStatus status;
    assert(f.control.status(status) && status.submitted_frame == 0);
    assert(f.draw(0).decision == Decision::Drop);
    assert(f.draw(1).decision == Decision::Drop);
}
static void labels_and_pairs() {
    for (uint64_t id : {uint64_t{1}, uint64_t{255}, uint64_t{1} << 63, UINT64_MAX}) {
        auto label = encode_ticket(id);
        uint64_t got = 0;
        assert(decode_ticket(label, 15, got) && got == id);
        assert(!decode_ticket(label, 14, got));
        assert(!decode_ticket(label, 16, got));
        auto bad = label;
        bad[14] = 'B';
        assert(!decode_ticket(bad, 15, got)); // nonzero pad bits
        bad = label;
        bad[4] = '+';
        assert(!decode_ticket(bad, 15, got));
        bad = label;
        bad[15] = 'x';
        assert(!decode_ticket(bad, 15, got));
        bad = label;
        bad[0] = 'X';
        assert(!decode_ticket(bad, 15, got));
    }
    assert(encode_ticket(1) == (std::array<char, 16>{'F', 'T', 'P', ':', 'A', 'A', 'A', 'A', 'A',
                                                     'A', 'A', 'A', 'A', 'A', 'E', 0}));
    uint64_t zero = 1;
    assert(!decode_ticket(encode_ticket(0), 15, zero));
    for (uint32_t ps = 125; ps <= 130; ++ps) {
        Fixture f;
        f.start();
        uint32_t vs = ps < 128 ? 57 : 58;
        assert(supported_pair(vs, ps));
        assert(!supported_pair(vs == 57 ? 58 : 57, ps));
        assert(f.adapter.enqueue(f.eye(0, vs, ps)));
        assert(f.adapter.enqueue(f.eye(1, vs, ps)));
        assert(f.adapter.outer_return());
        assert(f.draw(0).decision == Decision::Portal);
        assert(f.draw(1).decision == Decision::Portal);
    }
    Fixture mixed;
    mixed.start();
    assert(!mixed.adapter.enqueue(mixed.eye(0, 57, 130)));
    assert(!mixed.fake.uploads);
}
static void refused_packets() {
    for (unsigned mutation = 0; mutation < 15; ++mutation) {
        Fixture f;
        f.start();
        f.stereo();
        auto rec = f.fake.records[0];
        auto ctx = f.context();
        switch (mutation) {
        case 0:
            rec.label[0] = 'X';
            break;
        case 1:
            rec.label_size = 0;
            break;
        case 2:
            rec.label = encode_ticket(999);
            break;
        case 3:
            rec.descriptors[0].range = 528;
            break;
        case 4:
            rec.descriptors[4].offset++;
            break;
        case 5:
            put(rec.prefix, 0x38, Address{1});
            break;
        case 6:
            put(rec.prefix, 0x58, Address{1});
            break;
        case 7:
            put(rec.prefix, 0x30, uint32_t{127});
            break;
        case 8:
            ctx.identity.renderer++;
            break;
        case 9:
            ctx.identity.device_generation++;
            break;
        case 10:
            ctx.identity.process_instance++;
            break;
        case 11:
            ctx.recording_generation++;
            break;
        case 12:
            rec.label = f.fake.records[1].label;
            break;
        case 13:
            rec.prefix[200] = std::byte{0};
            break;
        case 14:
            ctx.identity.renderer_generation++;
            break;
        }
        assert(f.adapter.backend(rec, ctx, f.lookup()).decision == Decision::Drop);
        assert(!f.fake.lookups);
        assert(f.draw(1).decision == Decision::Drop); // whole frame invalidated
    }
    Fixture stock;
    auto rec = stock.fake.records[0];
    rec.label = encode_ticket(1);
    rec.label_size = 15;
    put(rec.prefix, 8, Address{0xabc});
    auto before = rec;
    auto r = stock.adapter.backend(rec, stock.context(), stock.lookup());
    assert(r.decision == Decision::Stock && r.pipeline == 0x888);
    assert(!std::memcmp(&before, &rec, sizeof rec));
}
static void whole_frame_guards() {
    for (unsigned mutation = 0; mutation < 8; ++mutation) {
        Fixture f;
        f.start();
        assert(f.adapter.enqueue(f.eye(0)));
        auto right = f.eye(1);
        switch (mutation) {
        case 0:
            now = 100;
            break;
        case 1:
            f.control.invalidate(Reason::Off);
            break;
        case 2:
            right.observed_origin++;
            break;
        case 3:
            right.raw_to_standing.v[7] = 1;
            break;
        case 4:
            f.fake.valid = false;
            break;
        case 5:
            f.lease.generation++;
            f.control.publish_lease(f.lease);
            break;
        case 6:
            f.lease.epoch++;
            f.control.publish_lease(f.lease);
            break;
        case 7:
            f.fake.obs.arena.recording_generation++;
            break;
        }
        assert(!f.adapter.enqueue(right));
        assert(!f.adapter.outer_return());
        assert(f.draw(0).decision == Decision::Drop);
    }
    Fixture early;
    early.start();
    assert(early.adapter.enqueue(early.eye(0)));
    assert(early.draw(0).decision == Decision::Drop);
    assert(!early.adapter.enqueue(early.eye(1)));
    Fixture expired;
    expired.start();
    expired.stereo();
    now = 100;
    assert(expired.draw(0).decision == Decision::Drop);
    Fixture revoked;
    revoked.start();
    revoked.stereo();
    revoked.control.invalidate(Reason::Off);
    assert(revoked.draw(0).decision == Decision::Drop);
    Fixture lease;
    lease.start();
    lease.stereo();
    lease.lease.deadline_ns = 200;
    lease.control.publish_lease(lease.lease);
    now = 100;
    assert(lease.draw(0).decision == Decision::Drop);
}
static void managed_contract() {
    for (unsigned mutation = 0; mutation < 18; ++mutation) {
        Fixture f;
        f.start();
        auto &a = f.fake.obs.arena;
        switch (mutation) {
        case 0:
            a.pass_end = a.pass_begin;
            break;
        case 1:
            a.command = 0;
            break;
        case 2:
            a.command_wrapper = 0;
            break;
        case 3:
            a.resource = 0;
            break;
        case 4:
            a.map = 0;
            break;
        case 5:
            a.buffer = 0;
            break;
        case 6:
            a.capacity = 591;
            break;
        case 7:
            a.cursor = 0x80000 - 591;
            break;
        case 8:
            a.cursor = UINT64_MAX - 1;
            break;
        case 9:
            a.alignment = 0;
            break;
        case 10:
            a.alignment = 3;
            break;
        case 11:
            a.alignment = UINT64_MAX;
            break;
        case 12:
            f.fake.budget = false;
            break;
        case 13:
            f.fake.obs.wrapper_size = 528;
            break;
        case 14:
            f.fake.obs.wrapper_resource = 0;
            break;
        case 15:
            a.recording_generation = 0;
            break;
        case 16:
            a.map = UINT64_MAX - 10;
            break;
        case 17:
            a.cursor = 1;
            break;
        }
        assert(!f.adapter.enqueue(f.eye(0)));
        assert(!f.fake.uploads && !f.fake.enqueues);
    }
    for (unsigned mutation = 0; mutation < 5; ++mutation) {
        Fixture f;
        f.start();
        if (mutation == 0)
            f.fake.fail = 0x78;
        if (mutation == 1)
            f.fake.fail = 0x2988a8;
        if (mutation == 2)
            f.fake.fail = 0x228;
        if (mutation == 3)
            f.fake.wrong_descriptor = true;
        if (mutation == 4)
            f.fake.wrong_upload = true;
        assert(!f.adapter.enqueue(f.eye(0)));
        assert(f.draw(0).decision != Decision::Portal);
    }
    Fixture badblend;
    badblend.start();
    badblend.fake.blend_ok = false;
    assert(!badblend.adapter.enqueue(badblend.eye(0)));
    assert(!badblend.fake.uploads);
}
static void capacity_and_retirement() {
    Fixture full(1);
    full.start();
    assert(!full.adapter.enqueue(full.eye(0)));
    assert(!full.fake.uploads); // reserve both tickets up front
    Fixture f(2);
    f.start();
    f.stereo();
    assert(f.draw(0).decision == Decision::Portal);
    assert(f.draw(1).decision == Decision::Portal);
    assert(!f.adapter.poll_completion());
    assert(!f.adapter.state().stereo_submitted);
    f.fake.queue_ok = false;
    assert(!f.adapter.submitted());
    assert(!f.adapter.state().stereo_submitted);
    // A failed stereo submission invalidates authorization, not native lifetime.
    f.adapter.off();
    f.fake.retire_ok = true;
    assert(!f.adapter.retire());
    assert(!f.fake.destroys);
    Fixture submitted(2);
    submitted.start();
    submitted.stereo();
    assert(submitted.draw(0).decision == Decision::Portal);
    assert(submitted.draw(1).decision == Decision::Portal);
    assert(submitted.adapter.submitted());
    assert(submitted.adapter.state().stereo_submitted);
    BackendStatus s;
    assert(submitted.control.status(s) && s.submitted_frame == 1);
    assert(!submitted.adapter.poll_completion());
    submitted.adapter.off();
    submitted.fake.retire_ok = true;
    assert(!submitted.adapter.retire());
    assert(!submitted.fake.destroys);
    submitted.fake.complete = true;
    assert(submitted.adapter.poll_completion());
    assert(submitted.adapter.retire());
    assert(submitted.fake.destroys == 1);
    assert(submitted.adapter.retire());
    assert(submitted.fake.destroys == 1);
    Fixture exhaustion(2);
    exhaustion.start();
    exhaustion.stereo();
    assert(exhaustion.draw(0).decision == Decision::Portal);
    assert(exhaustion.draw(1).decision == Decision::Portal);
    assert(exhaustion.adapter.submitted());
    exhaustion.fake.complete = true;
    assert(exhaustion.adapter.poll_completion());
    PolicyInput p;
    p.enabled = true;
    auto next = exhaustion.gate.begin(2, p, true, true, false, now);
    assert(!exhaustion.adapter.reclaim()); // no CPU/recording settlement binding
    assert(!exhaustion.adapter.latch(next));
    assert(!exhaustion.adapter.enqueue(exhaustion.eye(0)));
    assert(exhaustion.adapter.state().tickets_used == 2);
    assert(exhaustion.draw(0).decision == Decision::Drop);
}
static void threads_pipeline_and_reentry() {
    Fixture f;
    f.start();
    f.stereo();
    std::thread t([&] { assert(f.draw(0).decision == Decision::Drop); });
    t.join();
    assert(f.draw(1).decision == Decision::Drop);
    Fixture re;
    re.start();
    re.fake.reenter = &re.adapter;
    assert(!re.adapter.enqueue(re.eye(0)));
    assert(!re.fake.enqueues);
    Fixture rejected;
    rejected.start();
    rejected.stereo();
    rejected.fake.pipeline_ok = false;
    assert(rejected.draw(0).decision == Decision::Drop);
    assert(rejected.draw(1).decision == Decision::Drop);
}
static void binding_and_arena_regressions() {
    Fixture a;
    a.start();
    a.stereo();
    uint64_t first = 0;
    assert(decode_ticket(a.fake.records[0].label, 15, first));
    Fixture b;
    b.start();
    b.stereo();
    uint64_t second = 0;
    assert(decode_ticket(b.fake.records[0].label, 15, second));
    assert(second > first + 1);
    for (unsigned mutation = 0; mutation < 5; ++mutation) {
        Fixture f;
        f.start();
        assert(f.adapter.enqueue(f.eye(0)));
        if (mutation == 0)
            f.fake.obs.arena.cursor = 0;
        if (mutation == 1)
            f.fake.obs.arena.command++;
        if (mutation == 2)
            f.fake.obs.arena.buffer++;
        if (mutation == 3)
            f.fake.obs.arena.map++;
        if (mutation == 4)
            f.fake.obs.wrapper_resource++;
        assert(!f.adapter.enqueue(f.eye(1)));
        assert(f.fake.uploads == 1);
    }
    for (unsigned mutation = 0; mutation < 3; ++mutation) {
        Fixture f;
        f.start();
        f.stereo();
        if (mutation == 0)
            f.fake.obs.arena.cursor = 0;
        if (mutation == 1)
            f.fake.obs.arena.recording_generation++;
        if (mutation == 2)
            f.fake.obs.attachment[7] = 1;
        assert(f.draw(0).decision == Decision::Drop);
        assert(!f.fake.lookups);
    }
}
static void lifetime_regressions() {
    Fixture wrong;
    wrong.start();
    wrong.stereo();
    assert(wrong.draw(0).decision == Decision::Portal);
    assert(wrong.draw(1).decision == Decision::Portal);
    wrong.fake.bad_queue_command = true;
    assert(!wrong.adapter.submitted());
    assert(!wrong.adapter.state().stereo_submitted);
    Fixture partial;
    partial.start();
    partial.adapter.off();
    partial.fake.retire_ok = true;
    partial.fake.retire_marker_ok = false;
    assert(!partial.adapter.retire());
    assert(partial.fake.destroys == 1);
    partial.control.settled();
    partial.lease.revocation = partial.control.revocation();
    assert(partial.control.publish_lease(partial.lease));
    PolicyInput p;
    p.enabled = true;
    auto next = partial.gate.begin(2, p, true, true, false, now);
    assert(!partial.adapter.latch(next));
    assert(!partial.adapter.initialize());
    partial.fake.retire_marker_ok = true;
    assert(partial.adapter.retire());
    assert(partial.fake.destroys == 1);
}
// Read-only queue evidence is distinct from authorization and CPU admission.
struct QueueObserver : Fake {
    bool queued = false, expire_inside = false;
    unsigned submissions = 0, completions = 0;
    Completion token{}, observed_completion{};
    static bool validate_queue(void *u, const Binding &b, Phase phase) noexcept {
        auto &f = *static_cast<QueueObserver *>(u);
        const auto &a = b.identity;
        const auto &current = f.binding.identity;
        return Fake::validate(&f, b, phase) && a.process_instance == current.process_instance &&
               a.renderer == current.renderer && a.renderer_generation == current.renderer_generation &&
               a.device == current.device && a.device_generation == current.device_generation;
    }
    static bool submission_queue(void *u, const Provenance &a, const Provenance &b,
                                 Completion &out) noexcept {
        auto &f = *static_cast<QueueObserver *>(u);
        ++f.submissions;
        assert(a.eye == 0 && b.eye == 1 && a.frame == b.frame);
        out = f.token; // even populated output is not evidence when returning false
        if (f.expire_inside)
            now = 100;
        return f.queued;
    }
    static bool completed_queue(void *u, const Binding &, const Completion &c) noexcept {
        auto &f = *static_cast<QueueObserver *>(u);
        ++f.completions;
        f.observed_completion = c;
        return f.complete;
    }
    Callbacks callbacks(bool observer) {
        auto c = Fake::callbacks();
        c.user = this;
        c.validate = validate_queue;
        c.submission = observer ? submission_queue : nullptr;
        c.completed = completed_queue;
        return c;
    }
};
struct QueueFixture {
    Fixture source;
    QueueObserver fake;
    Adapter adapter;
    Frame frame;
    explicit QueueFixture(bool observer = true)
        : adapter(source.control, source.gate, fake.binding, fake.callbacks(observer)) {
        assert(adapter.initialize());
        PolicyInput policy;
        policy.enabled = true;
        frame = source.gate.begin(1, policy, true, true, false, now);
        assert(adapter.latch(frame));
        assert(adapter.enqueue(source.eye(0)));
        assert(adapter.enqueue(source.eye(1)));
        assert(adapter.outer_return());
    }
    BackendResult draw(unsigned eye) {
        return adapter.backend(fake.records[eye], source.context(), source.lookup());
    }
    void cpu() {
        assert(draw(0).decision == Decision::Portal);
        assert(draw(1).decision == Decision::Portal);
        assert(adapter.state().cpu_done);
    }
    void queue_success() {
        // Synthetic external queue success happens while the frame is authorized.
        // The submission callback only reads this immutable result; it queues nothing.
        assert(now < frame.lease.deadline_ns);
        assert(adapter.state().cpu_done);
        fake.queued = true;
        fake.token = {frame.id, fake.obs.arena.command, 0x901, 23, -2};
    }
    void unpublished() {
        BackendStatus s;
        assert(source.control.status(s) && !s.submitted_frame && !s.submitted_epoch);
    }
    void retained() {
        assert(!adapter.state().stereo_submitted);
        assert(!adapter.poll_completion());
        assert(!fake.completions);
        adapter.off();
        fake.retire_ok = true;
        assert(!adapter.retire() && !fake.destroys);
        unpublished();
    }
};
static void submission_observation_negatives() {
    // Invalid/absent queue evidence stays fail-closed, even after authorization ends.
    for (unsigned mode = 0; mode < 7; ++mode) {
        QueueFixture f(mode != 6);
        f.cpu();
        f.queue_success();
        switch (mode) {
        case 0: f.fake.queued = false; break;
        case 1: ++f.fake.token.frame; break;
        case 2: ++f.fake.token.command; break;
        case 3: f.fake.token.command = 0; break;
        case 4: f.fake.token.synchronization = 0; break;
        case 5: f.fake.token.serial = 0; break;
        }
        now = 100;
        assert(!f.adapter.submitted());
        f.retained();
    }
    // Native binding validation must still run before read-only observation.
    for (unsigned mode = 0; mode < 3; ++mode) {
        QueueFixture f;
        f.cpu();
        f.queue_success();
        if (mode == 0) ++f.fake.binding.identity.renderer_generation;
        if (mode == 1) ++f.fake.binding.identity.device_generation;
        if (mode == 2) ++f.fake.binding.identity.process_instance;
        now = 100;
        assert(!f.adapter.submitted());
        assert(!f.fake.submissions);
        f.retained();
    }
    for (unsigned mode = 0; mode < 2; ++mode) {
        QueueFixture f;
        assert(f.draw(0).decision == Decision::Portal);
        if (mode == 0) now = 100;
        else f.adapter.off();
        assert(f.draw(1).decision == Decision::Drop);
        assert(!f.adapter.state().cpu_done);
        // Even a claimed token cannot replace missing second-eye CPU admission.
        f.fake.queued = true;
        f.fake.token = {f.frame.id, f.fake.obs.arena.command, 0x901, 23, -2};
        assert(!f.adapter.submitted());
        assert(!f.fake.submissions);
        f.retained();
    }
    QueueFixture other_thread;
    other_thread.cpu();
    other_thread.queue_success();
    now = 100;
    std::thread t([&] { assert(!other_thread.adapter.submitted()); });
    t.join();
    assert(!other_thread.fake.submissions);
    assert(!other_thread.adapter.state().stereo_submitted);
    assert(!other_thread.adapter.poll_completion() && !other_thread.fake.completions);
    other_thread.adapter.off();
    other_thread.fake.retire_ok = true;
    assert(!other_thread.adapter.retire() && !other_thread.fake.destroys);
    BackendStatus refused;
    assert(!other_thread.source.control.status(refused));
}
static void submission_observation_after_authorization() {
    bool all_preserved = true;
    for (unsigned mode = 0; mode < 3; ++mode) {
        QueueFixture f;
        f.cpu();
        f.queue_success();
        const Completion exact = f.fake.token;
        if (mode == 0) f.fake.expire_inside = true;
        if (mode == 1) now = 100;
        if (mode == 2) f.adapter.off();
        assert(!f.adapter.submitted()); // never publish expired/revoked authorization
        f.unpublished();
        const bool saved = f.adapter.state().stereo_submitted;
        all_preserved = all_preserved && saved;
        std::printf("submission evidence %s: observations=%u saved=%d\n",
                    mode == 0 ? "expiry-inside" : mode == 1 ? "expiry-before" : "off-before",
                    f.fake.submissions, saved);
        std::fflush(stdout);
        // A duplicate must not observe again or replace the exact saved token.
        ++f.fake.token.serial;
        ++f.fake.token.subserial;
        assert(!f.adapter.submitted());
        assert(f.fake.submissions == (saved ? 1u : 0u));
        assert(!f.adapter.poll_completion());
        f.adapter.off();
        f.fake.retire_ok = true;
        assert(!f.adapter.retire() && !f.fake.destroys);
        if (saved) {
            f.fake.complete = true;
            assert(f.adapter.poll_completion());
            assert(f.fake.completions == 2);
            const auto &seen = f.fake.observed_completion;
            assert(seen.frame == exact.frame && seen.command == exact.command &&
                   seen.synchronization == exact.synchronization && seen.serial == exact.serial &&
                   seen.subserial == exact.subserial);
            assert(f.adapter.retire() && f.fake.destroys == 1);
            assert(f.adapter.poll_completion() && f.fake.completions == 2);
            assert(f.adapter.retire() && f.fake.destroys == 1);
        }
        assert(f.fake.uploads == 2 && f.fake.enqueues == 2 && f.fake.lookups == 2);
        f.unpublished();
    }
    assert(all_preserved && "expiry/Off must preserve already-successful exact queue evidence");
}
static void ticket_wrap() {
    Fixture wrap(16, UINT64_MAX);
    wrap.start();
    assert(!wrap.adapter.enqueue(wrap.eye(0)));
    assert(!wrap.fake.uploads);
}
static void captured_fixtures(const char *path) {
    FILE *file = std::fopen(path, "rb");
    assert(file);
    uint32_t count = 0;
    assert(std::fread(&count, 4, 1, file) == 1 && count && count % 2 == 0);
    for (uint32_t index = 0; index < count; index += 2) {
        Fixture f;
        f.start();
        for (unsigned eye = 0; eye < 2; ++eye) {
            auto in = f.eye(eye);
            std::array<float, 12> origin{};
            assert(std::fread(in.prefix.data(), 352, 1, file) == 1);
            assert(std::fread(in.constants.data(), 528, 1, file) == 1);
            assert(std::fread(origin.data(), 48, 1, file) == 1);
            for (unsigned i = 0; i < 12; ++i)
                in.raw_to_standing.v[i] = origin[i];
            f.fake.expected_stock = get<Address>(in.prefix, 8);
            assert(f.adapter.enqueue(in));
            auto expected = in.prefix;
            put(expected, 8, f.fake.marker);
            put(expected, 0x38, f.fake.wrapper);
            put(expected, 0x58, f.fake.wrapper);
            assert(f.fake.records[eye].prefix == expected);
            assert(!std::memcmp(f.fake.payloads[eye].data(), in.constants.data(), 528));
        }
        assert(f.adapter.outer_return());
        assert(f.draw(0).decision == Decision::Portal);
        assert(f.draw(1).decision == Decision::Portal);
    }
    assert(std::fgetc(file) == EOF);
    std::fclose(file);
    std::printf("captured native prefixes/constants/origins: %u adapter replays passed, synthetic "
                "leases and native operations\n",
                count);
}
int main(int argc, char **argv) {
    initialization();
    stereo_bytes_and_deferred();
    labels_and_pairs();
    refused_packets();
    whole_frame_guards();
    managed_contract();
    capacity_and_retirement();
    threads_pipeline_and_reentry();
    binding_and_arena_regressions();
    lifetime_regressions();
    submission_observation_negatives();
    submission_observation_after_authorization();
    ticket_wrap();
    if (argc == 2)
        captured_fixtures(argv[1]);
    else
        assert(argc == 1);
    std::puts("render adapter tests passed");
    return 0;
}
