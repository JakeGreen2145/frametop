// SPDX-License-Identifier: MIT
#include "adapter.hpp"
#include <limits>
#include <unistd.h>

namespace native_portal::render {
namespace {
std::atomic<uint64_t> next_process_ticket{1}; // never reset while native references can exist
constexpr std::array<uint32_t, 8> attachment{1, 1, 7, 0, 1, 7, 0, 15};
constexpr std::array<uint32_t, 6> blend_descriptor{1, 1, 3, 1, 3, 0};
template <class T, class A> T load(const A &a, size_t offset) noexcept {
    T v;
    std::memcpy(&v, a.data() + offset, sizeof v);
    return v;
}
template <class T, class A> void store(A &a, size_t offset, T v) noexcept {
    std::memcpy(a.data() + offset, &v, sizeof v);
}
bool same(const Identity &a, const Identity &b) noexcept {
    return a.process_instance == b.process_instance && a.renderer == b.renderer &&
           a.renderer_generation == b.renderer_generation && a.device == b.device &&
           a.device_generation == b.device_generation;
}
bool same(const Descriptor &a, const Descriptor &b) noexcept {
    return a.buffer == b.buffer && a.offset == b.offset && a.range == b.range;
}
bool same_geometry(const Geometry &a, const Geometry &b) noexcept {
    for (unsigned i = 0; i < 3; ++i)
        if (a.center[i] != b.center[i] || a.size[i] != b.size[i])
            return false;
    for (unsigned i = 0; i < 4; ++i)
        if (a.quaternion[i] != b.quaternion[i])
            return false;
    return a.opacity == b.opacity;
}
bool valid_arena(const Arena &a) noexcept {
    // Pinned native pass stride, with 8-byte fields. Check ordered bounds before
    // subtracting, and require a representable, integral, nonempty native span.
    constexpr uint64_t stride = 0x98;
    return a.pass_begin && !(a.pass_begin % 8) && a.pass_begin <= UINT64_MAX - stride &&
           a.pass_end >= a.pass_begin + stride &&
           a.pass_end - a.pass_begin <= INT64_MAX &&
           (a.pass_end - a.pass_begin) % stride == 0 &&
           a.command_wrapper && a.command && a.resource && a.buffer && a.map &&
           a.recording_generation && a.capacity == 0x80000 && a.alignment &&
           a.alignment <= a.capacity && !(a.alignment & (a.alignment - 1)) &&
           a.cursor <= a.capacity && !(a.cursor % a.alignment) &&
           a.map <= UINT64_MAX - a.capacity;
}
bool same_generation(const Arena &a, const Arena &b) noexcept {
    return a.command_wrapper == b.command_wrapper && a.command == b.command &&
           a.resource == b.resource && a.buffer == b.buffer && a.map == b.map &&
           a.capacity == b.capacity && a.alignment == b.alignment &&
           a.recording_generation == b.recording_generation;
}
bool same_upload(const Arena &a, const Arena &b) noexcept {
    // An append is supported between calls, never during one managed upload.
    return valid_arena(b) && same_generation(a, b) &&
           a.pass_begin == b.pass_begin && a.pass_end == b.pass_end;
}
bool recording_progress(const Arena &previous, const Arena &current) noexcept {
    // Only same-begin append has been audited. Relocation needs a future native
    // ownership binding, not relaxed pointer checks or normalized observations.
    return valid_arena(current) && same_generation(previous, current) &&
           current.pass_begin == previous.pass_begin && current.pass_end >= previous.pass_end &&
           current.cursor >= previous.cursor;
}
struct Ticket {
    Provenance provenance;
    Prefix prefix{};
    bool consumed = false, admitted = false;
};
} // namespace
struct Adapter::Impl {
    Backend &control;
    FrameGate &gate;
    const Binding binding;
    const Callbacks cb;
    const std::thread::id owner = std::this_thread::get_id();
    const pid_t pid = getpid();
    std::atomic_flag busy = ATOMIC_FLAG_INIT;
    std::atomic<Address> marker{0};
    Address wrapper = 0;
    size_t capacity = 0, used = 0;
    uint64_t next = 0;
    std::unique_ptr<Ticket[]> tickets;
    State status;
    bool attempted = false;
    Frame frame;
    unsigned eyes = 0;
    bool returned = false;
    std::array<size_t, 2> eye_slots{};
    ft::portal::Mat4 origin{};
    uint64_t recording = 0;
    Completion completion{};
    Arena frame_arena{};
    uint64_t highwater = 0;
    Address wrapper_resource = 0;
    Impl(Backend &b, FrameGate &g, Binding bind, Callbacks c, size_t cap, uint64_t first)
        : control(b), gate(g), binding(bind), cb(c), capacity(cap <= 4096 ? cap : 0), next(first),
          tickets(capacity ? new Ticket[capacity] : nullptr) {
        status.tickets_capacity = capacity;
    }
    bool enter() noexcept {
        if (getpid() != pid || std::this_thread::get_id() != owner) {
            control.refuse();
            return false;
        }
        if (busy.test_and_set()) {
            control.refuse();
            return false;
        }
        return true;
    }
    struct Guard {
        Impl &p;
        bool ok;
        explicit Guard(Impl &v) : p(v), ok(p.enter()) {}
        ~Guard() {
            if (ok)
                p.busy.clear();
        }
    };
    bool capability() const noexcept {
        BackendStatus s;
        return control.status(s) && s.capable && s.serialized && s.external_clear && !s.fault;
    }
    bool validated(Phase phase) const noexcept {
        return cb.validate && cb.validate(cb.user, binding, phase);
    }
    bool call(Registers &r) const noexcept { return cb.call && cb.call(cb.user, r); }
    bool native_debt = false, stopped = false;
    bool reject() noexcept {
        status.frame_invalid = true;
        if (frame.id)
            gate.end(frame);
        return false;
    }
    bool live() const noexcept {
        Lease current;
        const uint64_t now = cb.clock ? cb.clock() : 0;
        return status.initialized && !status.retired && !status.frame_invalid && !stopped &&
               capability() && validated(Phase::Current) && now && now < frame.lease.deadline_ns &&
               control.lease(current, now) && current.epoch == frame.lease.epoch &&
               current.origin == frame.lease.origin &&
               current.generation == frame.lease.generation &&
               current.revocation == frame.lease.revocation && current.flags == frame.lease.flags &&
               same_geometry(current.geometry, frame.lease.geometry);
    }
    bool observe(Observation &o) const noexcept {
        return cb.observe && cb.observe(cb.user, binding, wrapper, marker.load(), o);
    }
};
Adapter::Adapter(Backend &b, FrameGate &g, Binding bind, Callbacks c, size_t cap, uint64_t first)
    : p_(new Impl(b, g, bind, c, cap, first)) {}
// Deliberately no native destruction here: absence of proven completion leaks
// native handles safely. Integration must retain this owner until retire succeeds.
Adapter::~Adapter() = default;
bool Adapter::initialize() noexcept {
    auto &p = *p_;
    Impl::Guard guard(p);
    if (!guard.ok)
        return false;
    if (p.status.initialized)
        return p.validated(Phase::Current) && p.capability();
    if (p.attempted)
        return false;
    p.attempted = true;
    const auto &b = p.binding;
    const auto &id = b.identity;
    const auto &t = b.targets;
    if (!p.capacity || !p.next || !id.process_instance || !id.renderer || !id.renderer_generation ||
        !id.device || !id.device_generation || !t.allocate || !t.upload || !t.descriptor ||
        !t.enqueue || !t.destroy || !t.blend || !b.headroom_evidence || !p.cb.clock || !p.cb.call ||
        !p.cb.observe || !p.cb.headroom || !p.cb.stock_blend || !p.cb.pipeline || !p.capability() ||
        !p.validated(Phase::Initialize))
        return false;
    Registers r;
    r.target = t.allocate;
    r.x = {id.renderer, 592, 0, 0, 0, 0, 0, 0};
    if (!p.call(r))
        return false;
    p.wrapper = r.x[0];
    if (!p.wrapper)
        return false;
    r = {};
    r.target = t.blend;
    r.x[0] = id.renderer;
    r.x[1] = reinterpret_cast<Address>(blend_descriptor.data());
    if (!p.call(r))
        return false;
    p.marker = r.x[0];
    if (!p.marker.load())
        return false;
    Observation o;
    if (!p.observe(o) || !o.wrapper_resource || o.wrapper_size != 592 ||
        o.wrapper_offset != UINT32_MAX || o.attachment != attachment)
        return false;
    p.wrapper_resource = o.wrapper_resource;
    p.status.initialized = true;
    return true;
}
bool Adapter::latch(const Frame &frame) noexcept {
    auto &p = *p_;
    Impl::Guard guard(p);
    if (!guard.ok)
        return false;
    // One retained frame: completion alone does not settle recorded CPU references.
    if (!p.status.initialized || p.status.retired || p.native_debt || p.used || !frame.id ||
        frame.id <= p.frame.id || !frame.policy.portal || !valid(frame.lease.geometry))
        return p.reject();
    p.frame = frame;
    p.eyes = 0;
    p.returned = false;
    // Keep the recording watermark across reclamation and new frame latches.
    p.stopped = false;
    p.completion = {};
    p.highwater = 0;
    p.frame_arena = {};
    p.status.frame_invalid = false;
    p.status.cpu_done = false;
    p.status.stereo_submitted = false;
    p.status.gpu_complete = false;
    p.status.cpu_settled = false;
    if (!p.live())
        return p.reject();
    return true;
}
bool Adapter::enqueue(const EyeInput &in) noexcept {
    auto &p = *p_;
    Impl::Guard guard(p);
    if (!guard.ok)
        return false;
    if (!p.live() || p.returned || in.eye > 1 || (p.eyes & (1u << in.eye)) ||
        in.observed_origin != p.frame.lease.origin)
        return p.reject();
    if (p.eyes == 0) {
        // Reserve both entries before native work. Only reclaim() releases storage;
        // the process-wide numbers never recycle, including after failed calls.
        if (p.capacity - p.used < 2 || !p.next)
            return p.reject();
        uint64_t observed = next_process_ticket.load();
        const uint64_t first = observed > p.next ? observed : p.next;
        if (first > UINT64_MAX - 2 ||
            !next_process_ticket.compare_exchange_strong(observed, first + 2))
            return p.reject();
        p.next = first;
        for (unsigned eye = 0; eye < 2; ++eye) {
            p.eye_slots[eye] = p.used++;
            p.tickets[p.eye_slots[eye]].provenance.ticket = p.next++;
        }
        p.status.tickets_used = p.used;
        p.status.tickets_issued += 2;
        p.origin = in.raw_to_standing;
    } else if (p.origin.v != in.raw_to_standing.v)
        return p.reject();
    const uint32_t vs = load<uint32_t>(in.prefix, 0x20), ps = load<uint32_t>(in.prefix, 0x30);
    if (!supported_pair(vs, ps) ||
        !p.cb.stock_blend(p.cb.user, p.binding, load<Address>(in.prefix, 8)))
        return p.reject();
    ft::portal::Draw draw;
    const auto &g = p.frame.lease.geometry;
    draw.box.center = {g.center[0], g.center[1], g.center[2]};
    draw.box.size = {g.size[0], g.size[1], g.size[2]};
    for (unsigned i = 0; i < 4; ++i)
        draw.box.quaternion[i] = g.quaternion[i];
    draw.raw_to_standing = in.raw_to_standing;
    draw.opacity = g.opacity;
    draw.epoch = p.frame.lease.epoch;
    draw.tracking_valid = true;
    for (unsigned i = 0; i < 16; ++i) {
        draw.raw_to_eye.v[i] = load<float>(in.constants, 64 + i * 4);
        draw.projection.v[i] = load<float>(in.constants, 128 + i * 4);
    }
    const auto geometry = ft::portal::compute(draw);
    if (!geometry)
        return p.reject();
    const auto payload = ft::portal::snapshot(in.constants, geometry);
    Observation before;
    if (!p.observe(before) || before.wrapper_resource != p.wrapper_resource ||
        before.wrapper_size != 592 || before.attachment != attachment)
        return p.reject();
    const auto &a = before.arena;
    if (!valid_arena(a) || a.capacity - a.cursor < 592)
        return p.reject();
    const uint64_t end = a.cursor + 592, aligned = (end + a.alignment - 1) & ~(a.alignment - 1);
    if (aligned > a.capacity || !p.cb.headroom(p.cb.user, p.binding, a, aligned))
        return p.reject();
    if (p.eyes ? (!recording_progress(p.frame_arena, a) || a.cursor < p.highwater)
               : a.recording_generation <= p.recording)
        return p.reject();
    p.recording = a.recording_generation;
    p.frame_arena = a;
    if (!p.gate.eye(p.frame, in.eye, in.observed_origin) || !p.live())
        return p.reject();
    Registers r;
    r.target = p.binding.targets.upload;
    r.x = {p.binding.identity.renderer,
           p.wrapper,
           reinterpret_cast<Address>(payload.data()),
           0,
           0,
           0,
           0,
           0};
    p.native_debt = true; // even an interrupted upload can register native references
    if (!p.call(r))
        return p.reject(); // never interpret upload x0 as Boolean
    Observation after;
    if (!p.observe(after) || !same_upload(a, after.arena) || after.arena.cursor != aligned ||
        after.wrapper_resource != before.wrapper_resource || after.wrapper_size != 592 ||
        after.wrapper_offset != a.cursor || after.attachment != attachment || !p.live())
        return p.reject();
    std::array<Descriptor, 2> descriptors{};
    for (auto &descriptor : descriptors) {
        r = {};
        r.target = p.binding.targets.descriptor;
        r.x[0] = p.binding.identity.renderer;
        r.x[1] = reinterpret_cast<Address>(&descriptor);
        r.x[2] = p.wrapper;
        if (!p.call(r) || !same(descriptor, Descriptor{a.buffer, a.cursor, 592}))
            return p.reject();
    }
    auto &ticket = p.tickets[p.eye_slots[in.eye]];
    auto &auth = ticket.provenance;
    const auto number = auth.ticket;
    auth = {p.binding.identity,
            number,
            p.frame.id,
            p.frame.lease.epoch,
            p.frame.lease.origin,
            p.frame.lease.generation,
            p.frame.lease.deadline_ns,
            p.frame.lease.revocation,
            a.recording_generation,
            in.eye,
            p.wrapper,
            p.marker.load(),
            vs,
            ps,
            descriptors};
    ticket.prefix = in.prefix;
    store(ticket.prefix, 8, auth.marker);
    store(ticket.prefix, 0x38, p.wrapper);
    store(ticket.prefix, 0x58, p.wrapper);
    const auto label = encode_ticket(number);
    if (!p.live())
        return p.reject();
    r = {};
    r.target = p.binding.targets.enqueue;
    r.x[0] = p.binding.identity.renderer;
    r.x[1] = reinterpret_cast<Address>(ticket.prefix.data());
    r.x[2] = reinterpret_cast<Address>(label.data());
    r.x[4] = r.target;
    if (!p.call(r) || !p.live())
        return p.reject();
    p.highwater = aligned;
    p.eyes |= 1u << in.eye;
    return true;
}
bool Adapter::outer_return() noexcept {
    auto &p = *p_;
    Impl::Guard guard(p);
    if (!guard.ok)
        return false;
    if (!p.live() || p.eyes != 3 || p.returned || !p.gate.accept(p.frame, p.cb.clock()))
        return p.reject();
    p.returned = true;
    return true; // deliberately NOT FrameGate::end
}
BackendResult Adapter::backend(const Recorded &record, const Context &context,
                               Registers lookup) noexcept {
    auto &p = *p_;
    // Marker first. Spoofed labels on stock packets are irrelevant. This path
    // forwards original registers without acquiring our owner-thread admission.
    const auto marker = load<Address>(record.prefix, 8);
    if (!marker || marker != p.marker.load()) {
        if (!p.call(lookup))
            return {Decision::Stock, 0};
        return {Decision::Stock, lookup.x[0]};
    }
    Impl::Guard guard(p);
    if (!guard.ok)
        return {};
    auto drop = [&]() {
        p.reject();
        return BackendResult{};
    };
    uint64_t number = 0;
    if (!decode_ticket(record.label, record.label_size, number))
        return drop();
    Ticket *found = nullptr;
    for (size_t i = 0; i < p.used; ++i)
        if (p.tickets[i].provenance.ticket == number) {
            found = &p.tickets[i];
            break;
        }
    if (!found)
        return drop();
    auto &ticket = *found;
    const auto &auth = ticket.provenance;
    if (ticket.consumed)
        return drop();
    ticket.consumed = true; // consume once, also when later checks fail
    if (!p.live() || p.eyes != 3 || !p.returned || auth.eye > 1 ||
        !same(auth.identity, context.identity) ||
        context.recording_generation != auth.recording_generation ||
        auth.recording_generation != p.recording || auth.frame != p.frame.id ||
        auth.lease != p.frame.lease.epoch || auth.origin != p.frame.lease.origin ||
        auth.geometry != p.frame.lease.generation || auth.deadline != p.frame.lease.deadline_ns ||
        auth.revocation != p.frame.lease.revocation || auth.marker != marker ||
        auth.wrapper != p.wrapper || record.prefix != ticket.prefix ||
        !supported_pair(auth.vs, auth.ps) || !same(record.descriptors[0], auth.descriptors[0]) ||
        !same(record.descriptors[4], auth.descriptors[1]) || !p.gate.accept(p.frame, p.cb.clock()))
        return drop();
    Observation current;
    if (!p.observe(current) || !recording_progress(p.frame_arena, current.arena) ||
        current.arena.cursor < p.highwater || current.wrapper_resource != p.wrapper_resource ||
        current.wrapper_size != 592 || current.attachment != attachment)
        return drop();
    if (!p.cb.pipeline(p.cb.user, auth, lookup) || !lookup.x[0] || !p.live() ||
        !p.gate.accept(p.frame, p.cb.clock()))
        return drop();
    // Remember actual accepted progress so the second deferred backend cannot
    // roll back to a prior extent/cursor that merely exceeds the upload boundary.
    p.frame_arena = current.arena;
    p.highwater = current.arena.cursor;
    ticket.admitted = true;
    p.status.cpu_done = p.tickets[p.eye_slots[0]].admitted && p.tickets[p.eye_slots[1]].admitted;
    return {Decision::Portal, lookup.x[0]};
}
bool Adapter::submitted() noexcept {
    auto &p = *p_;
    Impl::Guard guard(p);
    if (!guard.ok)
        return false;
    // Observe already-submitted work under native lifetime ownership, not lease
    // authorization. This read-only callback must never queue new work.
    if (!p.status.initialized || p.status.retired || !p.status.cpu_done ||
        p.status.stereo_submitted || !p.cb.submission || !p.validated(Phase::Current))
        return p.reject();
    Completion c;
    if (!p.cb.submission(p.cb.user, p.tickets[p.eye_slots[0]].provenance,
                         p.tickets[p.eye_slots[1]].provenance, c) ||
        c.frame != p.frame.id || !c.command || c.command != p.frame_arena.command ||
        !c.synchronization || !c.serial)
        return p.reject();
    // Save the exact lifetime token once, even if authorization has already ended.
    p.completion = c;
    p.status.stereo_submitted = true;
    if (!p.live() || !p.gate.submitted(p.frame, p.cb.clock()))
        return p.reject();
    return true;
}
bool Adapter::poll_completion() noexcept {
    auto &p = *p_;
    Impl::Guard guard(p);
    if (!guard.ok)
        return false;
    if (p.status.gpu_complete)
        return true;
    if (!p.status.stereo_submitted || !p.cb.completed || !p.validated(Phase::Current) ||
        !p.cb.completed(p.cb.user, p.binding, p.completion))
        return false;
    p.status.gpu_complete = true;
    p.native_debt = false;
    return true;
}
bool Adapter::reclaim() noexcept {
    auto &p = *p_;
    Impl::Guard guard(p);
    if (!guard.ok)
        return false;
    if (p.status.cpu_settled)
        return true;
    // Read-only lifetime evidence, not render authorization. Expiry/Off must not
    // discard the R1 exact submission token or prevent settlement under its owner.
    if (!p.status.initialized || p.status.retired || p.native_debt || p.used != 2 ||
        !p.status.cpu_done || !p.status.stereo_submitted || !p.status.gpu_complete ||
        !p.cb.settled || !p.validated(Phase::Current) ||
        !p.cb.settled(p.cb.user, p.binding, p.tickets[p.eye_slots[0]].provenance,
                      p.tickets[p.eye_slots[1]].provenance, p.completion) ||
        !p.validated(Phase::Current))
        return false;
    // No tombstone collection: immutable marker recognition plus nonreused IDs
    // makes every late old packet an owned DROP, including after slot reuse.
    for (size_t i = 0; i < p.used; ++i)
        p.tickets[i] = {};
    p.used = p.status.tickets_used = 0;
    p.status.cpu_settled = true;
    return true;
}
void Adapter::off() noexcept {
    auto &p = *p_;
    p.control.invalidate(Reason::Off);
    Impl::Guard guard(p);
    if (!guard.ok)
        return;
    p.stopped = true;
    p.reject(); // no resource deletion on Off
}
bool Adapter::retire() noexcept {
    auto &p = *p_;
    Impl::Guard guard(p);
    if (!guard.ok)
        return false;
    if (p.status.retired)
        return true;
    if (!p.stopped || p.native_debt || !p.validated(Phase::Retire) || !p.cb.retire_marker)
        return false;
    p.status.initialized = false; // partial teardown cannot become a live binding again
    if (p.wrapper) {
        Registers r;
        r.target = p.binding.targets.destroy;
        r.x[0] = p.binding.identity.renderer;
        r.x[1] = reinterpret_cast<Address>(&p.wrapper);
        if (!p.call(r) || p.wrapper)
            return false;
    }
    if (p.marker.load() && !p.cb.retire_marker(p.cb.user, p.binding, p.marker.load()))
        return false;
    // Retain marker identity as a tombstone: late owned records must never stock-forward.
    p.status.retired = true;
    p.status.initialized = false;
    return true;
}
State Adapter::state() const noexcept {
    if (getpid() != p_->pid || std::this_thread::get_id() != p_->owner)
        return {};
    return p_->status;
}
std::array<char, 16> encode_ticket(uint64_t ticket) noexcept {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::array<char, 16> out{'F', 'T', 'P', ':'};
    // Eight big-endian bytes: ten complete sextets, then four bits and zero pad.
    for (unsigned i = 0; i < 10; ++i)
        out[4 + i] = alphabet[(ticket >> (58 - 6 * i)) & 63];
    out[14] = alphabet[(ticket & 15) << 2];
    return out;
}
bool decode_ticket(const std::array<char, 16> &label, uint64_t length, uint64_t &out) noexcept {
    out = 0;
    if (length != 15 || label[15] || label[0] != 'F' || label[1] != 'T' || label[2] != 'P' ||
        label[3] != ':')
        return false;
    uint64_t value = 0;
    for (unsigned i = 4; i < 15; ++i) {
        const unsigned char c = label[i];
        unsigned v;
        if (c >= 'A' && c <= 'Z')
            v = c - 'A';
        else if (c >= 'a' && c <= 'z')
            v = c - 'a' + 26;
        else if (c >= '0' && c <= '9')
            v = c - '0' + 52;
        else if (c == '-')
            v = 62;
        else if (c == '_')
            v = 63;
        else
            return false;
        if (i == 14) {
            if (v & 3)
                return false;
            value = (value << 4) | (v >> 2);
        } else
            value = (value << 6) | v;
    }
    if (!value || encode_ticket(value) != label)
        return false;
    out = value;
    return true;
}
bool supported_pair(uint32_t vs, uint32_t ps) noexcept {
    return (vs == 57 && (ps == 125 || ps == 126 || ps == 127)) ||
           (vs == 58 && (ps == 128 || ps == 129 || ps == 130));
}
} // namespace native_portal::render
