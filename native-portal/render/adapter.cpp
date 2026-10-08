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
           a.recording_generation && (a.capacity == 0x80000 || a.capacity == arena::Capacity) && a.alignment &&
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
    bool returned = false, preparation_attempted = false;
    bool rejection_attempted = false, rejection_armed = false, rejection_bad = false;
    unsigned suppressed_mask = 0;
    std::array<EyePreparation, 2> rejected_inputs{};
    std::array<EyePreparation, 2> prepared{};
    std::array<Address, 2> pipelines{};
    std::array<size_t, 2> eye_slots{};
    ft::portal::Mat4 origin{};
    uint64_t recording = 0;
    Completion completion{};
    Arena frame_arena{};
    uint64_t highwater = 0;
    Address wrapper_resource = 0;
    struct Upload {
        bool active=false, offered=false, entered=false, ready=false;
        Address camera=0, stock=0;
        uint64_t ticket=0;
        EyeInput input{};
        Observation before{}, after{};
        arena::Owned offer{};
        std::array<Descriptor,2> descriptors{};
        ft::portal::PortalConstants staging{};
    } upload;
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
        // Admission failure cannot roll back a committed mixed native command.
        // Stop new work but retain the pair's one-shot encoding authorization.
        if (status.committed) stopped = true;
        else {
            status.frame_invalid = true;
            if (frame.id) gate.end(frame);
        }
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
    Ticket *authenticate(const Recorded &record, const Context &context) noexcept {
        uint64_t number = 0;
        if (!decode_ticket(record.label, record.label_size, number)) return nullptr;
        for (size_t i = 0; i < used; ++i) {
            auto &ticket = tickets[i];
            const auto &auth = ticket.provenance;
            if (auth.ticket != number) continue;
            if (auth.eye > 1 || !same(auth.identity, context.identity) ||
                context.recording_generation != auth.recording_generation ||
                auth.recording_generation != recording || auth.frame != frame.id ||
                auth.lease != frame.lease.epoch || auth.origin != frame.lease.origin ||
                auth.geometry != frame.lease.generation || auth.deadline != frame.lease.deadline_ns ||
                auth.revocation != frame.lease.revocation || auth.marker != marker.load() ||
                auth.wrapper != wrapper || record.prefix != ticket.prefix ||
                !supported_pair(auth.vs, auth.ps) ||
                !same(record.descriptors[0], auth.descriptors[0]) ||
                !same(record.descriptors[4], auth.descriptors[1])) return nullptr;
            return &ticket;
        }
        return nullptr;
    }
    bool native_current(bool rejected_lifetime = false) noexcept {
        Observation current;
        if (!status.initialized || status.retired || (status.frame_invalid && !rejected_lifetime) ||
            !validated(Phase::Current) ||
            (binding.upstream && !binding.upstream->bound(binding.identity.renderer,binding.identity.renderer_generation)) ||
            !observe(current) ||
            !recording_progress(frame_arena, current.arena) ||
            current.arena.cursor < highwater || current.wrapper_resource != wrapper_resource ||
            current.wrapper_size != 592 || current.attachment != attachment ||
            !validated(Phase::Current)) return false;
        frame_arena = current.arena;
        highwater = current.arena.cursor;
        return true;
    }
    void pending(bool value) noexcept {
        status.pending = value;
        control.render_pending(value);
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
        !t.enqueue || !t.destroy || !t.blend || !p.cb.clock || !p.cb.call ||
        !(b.upstream ? (!b.legacy_test_upload && b.upstream->bound(id.renderer,id.renderer_generation))
                     : (b.legacy_test_upload && b.headroom_evidence && p.cb.headroom)) ||
        !p.cb.observe || !p.cb.stock_blend || !p.cb.pipeline || !p.capability() ||
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
    p.status.rejected = p.status.queue_observed = false;
    p.rejection_attempted = p.rejection_armed = p.rejection_bad = false;
    p.suppressed_mask = 0;
    p.rejected_inputs = {};
    p.status.prepared = p.status.committed = false;
    p.status.pair_generation = 0;
    p.preparation_attempted = false;
    p.prepared = {};
    p.pipelines = {};
    if (!p.live())
        return p.reject();
    return true;
}
bool Adapter::enqueue(const EyeInput &in) noexcept {
    auto &p = *p_;
    if (!p.binding.legacy_test_upload || p.binding.upstream) return false;
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
    p.pending(true);
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
uint64_t Adapter::reserve_upload(const EyeInput &in, Address camera, Address stock) noexcept {
    auto &p=*p_;
    Impl::Guard guard(p);
    if(!guard.ok) return 0;
    auto fail=[&]() -> uint64_t {p.reject();return 0;};
    if(!p.binding.upstream || p.binding.legacy_test_upload) return fail();
    // Do not overwrite an unconsumed invocation or permit its late enqueue to
    // borrow a newer offer. A failed claim is still owned, never stock fallback.
    if(p.upload.active) return fail();
    auto &u=p.upload;
    u={};u.active=true;u.camera=camera;u.stock=stock;u.input=in;
    if(!camera || !stock || !p.live() || p.returned || in.eye>1 ||
       (p.eyes & (1u<<in.eye)) || in.observed_origin!=p.frame.lease.origin ||
       !p.binding.upstream->bound(p.binding.identity.renderer,p.binding.identity.renderer_generation)) return fail();
    if(!p.eyes) {
        if(p.used || p.capacity<2 || !p.next) return fail();
        uint64_t observed=next_process_ticket.load();
        const uint64_t first=observed>p.next?observed:p.next;
        if(first>UINT64_MAX-2 || !next_process_ticket.compare_exchange_strong(observed,first+2)) return fail();
        p.next=first+2;
        // Issue in actual upload order, while preserving eye-indexed provenance.
        p.eye_slots[in.eye]=0;p.eye_slots[1-in.eye]=1;
        p.tickets[0].provenance.ticket=first;p.tickets[1].provenance.ticket=first+1;
        p.used=p.status.tickets_used=2;p.status.tickets_issued+=2;
        p.origin=in.raw_to_standing;
    } else if(p.origin.v!=in.raw_to_standing.v) return fail();
    ft::portal::Draw draw;
    const auto &g=p.frame.lease.geometry;
    draw.box.center={g.center[0],g.center[1],g.center[2]};
    draw.box.size={g.size[0],g.size[1],g.size[2]};
    for(unsigned i=0;i<4;++i) draw.box.quaternion[i]=g.quaternion[i];
    draw.raw_to_standing=in.raw_to_standing;draw.opacity=g.opacity;
    draw.epoch=p.frame.lease.epoch;draw.tracking_valid=true;
    for(unsigned i=0;i<16;++i) {
        draw.raw_to_eye.v[i]=load<float>(in.constants,64+i*4);
        draw.projection.v[i]=load<float>(in.constants,128+i*4);
    }
    const auto geometry=ft::portal::compute(draw);
    if(!geometry || !p.observe(u.before)) return fail();
    const auto &a=u.before.arena;
    if(!valid_arena(a) || a.capacity!=arena::Capacity || a.alignment!=64 ||
       u.before.wrapper_resource!=p.wrapper_resource || u.before.wrapper_size!=592 ||
       u.before.attachment!=attachment ||
       p.binding.upstream->policy().check(a.cursor,592,true)!=arena::Result::Admit ||
       (p.eyes ? (!recording_progress(p.frame_arena,a) || a.cursor<p.highwater)
               : a.recording_generation<=p.recording)) return fail();
    if(!p.gate.eye(p.frame,in.eye,in.observed_origin) || !p.live()) return fail();
    auto &auth=p.tickets[p.eye_slots[in.eye]].provenance;
    const auto number=auth.ticket;
    auth={p.binding.identity,number,p.frame.id,p.frame.lease.epoch,p.frame.lease.origin,
          p.frame.lease.generation,p.frame.lease.deadline_ns,p.frame.lease.revocation,
          a.recording_generation,in.eye,p.wrapper,p.marker.load(),0,0,{}};
    p.recording=a.recording_generation;p.frame_arena=a;
    const auto payload=ft::portal::snapshot(in.constants,geometry);
    u.offer={camera,stock,p.wrapper,reinterpret_cast<Address>(u.staging.data()),number,{}};
    std::memcpy(u.offer.portal.data(),payload.data()+528,64);
    u.ticket=number;
    return number;
}
void Adapter::upstream(arena::Saved &s,unsigned site,uint64_t lifetime,uint64_t thread) noexcept {
    auto &p=*p_;
    auto *bridge=p.binding.upstream;
    if(!bridge) {s.pad=arena::Unsupported;return;}
    if(getpid()!=p.pid || std::this_thread::get_id()!=p.owner) {
        p.control.refuse();s.pad=arena::Unsupported;return;
    }
    // Classifier hooks can be reached by the two immediate descriptor calls.
    // These only touch the serialized Bridge, not Adapter admission state.
    if(site!=arena::Upload && site!=arena::UploadReturn) {
        bridge->dispatch(s,site,lifetime,thread);return;
    }
    Impl::Guard guard(p);
    if(!guard.ok) {s.pad=arena::Unsupported;return;}
    s.pad=arena::Original;
    auto &u=p.upload;
    if(!u.active || s.x[27]!=u.camera) {
        bridge->dispatch(s,site,lifetime,thread);return;
    }
    auto fail=[&]() {u.ready=false;p.reject();if(site==arena::Upload && s.pad!=arena::Unsupported)bridge->skip(s);};
    if(site==arena::Upload) {
        Observation current;
        if(u.entered || !u.ticket || !p.live() || !p.observe(current) ||
           !same_upload(u.before.arena,current.arena) || current.arena.cursor!=u.before.arena.cursor ||
           current.wrapper_resource!=p.wrapper_resource || current.wrapper_size!=592 ||
           current.attachment!=attachment ||
           !bridge->geometry_matches(u.camera,u.input.constants.data()+64) ||
           !bridge->offer(u.offer)) {fail();return;}
        u.offered=true;u.entered=true;
        // Native ownership debt starts BEFORE any external copy/reference work.
        p.native_debt=true;p.pending(true);
        bridge->dispatch(s,site,lifetime,thread);
        if(s.pad!=arena::Replacement || !p.live()) fail();
        return;
    }
    bridge->dispatch(s,site,lifetime,thread);
    arena::Descriptor descriptor;
    const auto &a=u.before.arena;
    const uint64_t aligned=(a.cursor+592+63)&~uint64_t{63};
    if(!u.entered || u.ready || !p.live() || !bridge->descriptor(descriptor) ||
       descriptor.ticket!=u.ticket || descriptor.buffer!=a.buffer || descriptor.offset!=a.cursor ||
       descriptor.range!=592 || !p.observe(u.after) || !same_upload(a,u.after.arena) ||
       u.after.arena.cursor!=aligned || u.after.wrapper_resource!=p.wrapper_resource ||
       u.after.wrapper_size!=592 || u.after.wrapper_offset!=a.cursor || u.after.attachment!=attachment) {fail();return;}
    // Snapshot both native stage resolutions NOW, before the shared wrapper moves.
    for(auto &d:u.descriptors) {
        Registers r{p.binding.targets.descriptor,{p.binding.identity.renderer,reinterpret_cast<Address>(&d),p.wrapper}};
        if(!p.call(r) || !same(d,Descriptor{descriptor.buffer,descriptor.offset,descriptor.range})) {fail();return;}
    }
    Observation final;
    if(!p.live() || !p.observe(final) || !same_upload(u.after.arena,final.arena) ||
       final.arena.cursor!=aligned || final.wrapper_resource!=p.wrapper_resource ||
       final.wrapper_size!=592 || final.wrapper_offset!=a.cursor || final.attachment!=attachment) {fail();return;}
    u.ready=true;p.frame_arena=u.after.arena;p.highwater=aligned;
}
bool Adapter::enqueue(uint64_t number,const EyeInput &in) noexcept {
    auto &p=*p_;
    Impl::Guard guard(p);
    if(!guard.ok) return false;
    auto &u=p.upload;
    auto finish=[&](bool ok) {
        if(p.binding.upstream && u.offered) p.binding.upstream->release(u.ticket);
        u.active=false;u.ready=false;
        return ok?true:p.reject();
    };
    if(!p.binding.upstream || !u.active || !u.ready || !number || number!=u.ticket ||
       in.eye!=u.input.eye || in.observed_origin!=u.input.observed_origin ||
       in.raw_to_standing.v!=u.input.raw_to_standing.v || !p.live() || p.returned ||
       (p.eyes & (1u<<in.eye))) return finish(false);
    // The original packet prefix is only final at 0x161ed8. Never clone the
    // earlier camera prefix; constants are already captured by the upstream hook.
    const auto vs=load<uint32_t>(in.prefix,0x20),ps=load<uint32_t>(in.prefix,0x30);
    if(!supported_pair(vs,ps) || !p.cb.stock_blend(p.cb.user,p.binding,load<Address>(in.prefix,8))) return finish(false);
    Observation current;
    if(!p.observe(current) || !recording_progress(u.after.arena,current.arena) ||
       current.arena.cursor<p.highwater || current.wrapper_resource!=p.wrapper_resource ||
       current.wrapper_size!=592 || current.wrapper_offset!=u.descriptors[0].offset ||
       current.attachment!=attachment || !p.live()) return finish(false);
    auto &ticket=p.tickets[p.eye_slots[in.eye]];
    auto &auth=ticket.provenance;
    if(auth.ticket!=number || auth.recording_generation!=current.arena.recording_generation) return finish(false);
    auth.vs=vs;auth.ps=ps;auth.descriptors=u.descriptors;
    ticket.prefix=in.prefix;store(ticket.prefix,8,auth.marker);
    store(ticket.prefix,0x38,p.wrapper);store(ticket.prefix,0x58,p.wrapper);
    const auto label=encode_ticket(number);
    Registers r{p.binding.targets.enqueue,{p.binding.identity.renderer,
        reinterpret_cast<Address>(ticket.prefix.data()),reinterpret_cast<Address>(label.data()),0,p.binding.targets.enqueue}};
    // No managed upload or late descriptor resolution here.
    u.ready=false;
    if(!p.call(r) || !p.live()) return finish(false);
    p.frame_arena=current.arena;p.highwater=current.arena.cursor;p.eyes|=1u<<in.eye;
    return finish(true);
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
uint64_t Adapter::prepare(const PairPreparation &input) noexcept {
    auto &p = *p_;
    Impl::Guard guard(p);
    if (!guard.ok) return 0;
    if (p.preparation_attempted || p.status.committed || !p.live() ||
        p.eyes != 3 || !p.returned || !p.gate.accept(p.frame, p.cb.clock())) {
        p.reject(); return 0;
    }
    p.preparation_attempted = true;
    // Copy all caller input before invoking callbacks. Never retain borrowed
    // packet addresses or allow a callback to replace the other eye's snapshot.
    const PairPreparation snapshot = input;
    if (!snapshot.complete_scan || !p.native_current() ||
        snapshot.command != p.frame_arena.command) { p.reject(); return 0; }
    unsigned seen = 0;
    for (const auto &eye : snapshot.eyes) {
        auto *ticket = p.authenticate(eye.record, eye.context);
        if (!ticket || !eye.ready || !eye.lookup.target ||
            eye.lookup.x[0] != p.binding.identity.renderer ||
            eye.pass < p.frame_arena.pass_begin || eye.pass >= p.frame_arena.pass_end ||
            (eye.pass - p.frame_arena.pass_begin) % 0x98 || ticket->consumed ||
            (seen & (1u << ticket->provenance.eye))) { p.reject(); return 0; }
        seen |= 1u << ticket->provenance.eye;
        p.prepared[ticket->provenance.eye] = eye;
    }
    if (seen != 3) { p.reject(); return 0; }
    for (unsigned eye = 0; eye < 2; ++eye) {
        Registers lookup = p.prepared[eye].lookup;
        if (!p.live() || !p.cb.pipeline(p.cb.user, p.tickets[p.eye_slots[eye]].provenance, lookup) ||
            !lookup.x[0] || !p.native_current() || !p.live() ||
            !p.gate.accept(p.frame, p.cb.clock())) { p.reject(); return 0; }
        p.pipelines[eye] = lookup.x[0];
    }
    p.status.prepared = true;
    // Tickets are process-wide, never reused, and reserved in eye order.
    p.status.pair_generation = p.tickets[p.eye_slots[0]].provenance.ticket;
    return p.status.pair_generation;
}
bool Adapter::commit(uint64_t generation) noexcept {
    auto &p = *p_;
    Impl::Guard guard(p);
    if (!guard.ok) return false;
    if (!generation || generation != p.status.pair_generation || !p.status.prepared ||
        p.status.committed || !p.native_current() || !p.live() ||
        !p.gate.commit(p.frame, p.cb.clock())) return p.reject();
    p.status.committed = true;
    return true;
}
bool Adapter::reject_recorded(const PairPreparation &input) noexcept {
    auto &p = *p_;
    Impl::Guard guard(p);
    if (!guard.ok) return false;
    auto fail = [&]() { p.rejection_bad = true; return false; };
    if (p.rejection_attempted || !p.preparation_attempted || p.status.committed ||
        p.status.cpu_done || p.eyes != 3 || !p.returned || p.used != 2 ||
        !p.native_debt || !input.suppression_scan || !p.native_current(true) ||
        input.command != p.frame_arena.command) return fail();
    p.rejection_attempted = true;
    unsigned seen = 0;
    for (const auto &eye : input.eyes) {
        const auto *ticket = p.authenticate(eye.record, eye.context);
        if (!ticket || ticket->consumed || ticket->admitted ||
            !eye.lookup.x[1] || eye.lookup.x[0] != p.binding.identity.renderer ||
            eye.pass < p.frame_arena.pass_begin || eye.pass >= p.frame_arena.pass_end ||
            (eye.pass-p.frame_arena.pass_begin)%0x98 ||
            (seen & (1u << ticket->provenance.eye))) return fail();
        seen |= 1u << ticket->provenance.eye;
        p.rejected_inputs[ticket->provenance.eye] = eye;
    }
    if (seen != 3 || p.rejection_bad) return fail();
    p.reject(); // no commit or new owned work after arming the actual skips
    p.rejection_armed = true;
    return true;
}
bool Adapter::suppressed(const EyePreparation &input, Address command) noexcept {
    auto &p = *p_;
    Impl::Guard guard(p);
    if (!guard.ok) return false;
    auto fail = [&]() { p.rejection_bad = true; return false; };
    if (!p.rejection_armed || p.rejection_bad || p.status.committed ||
        command != p.frame_arena.command || !p.native_current(true)) return fail();
    auto *ticket = p.authenticate(input.record,input.context);
    if (!ticket || ticket->admitted || ticket->consumed) return fail();
    const unsigned eye = ticket->provenance.eye, bit = 1u << eye;
    const auto &saved = p.rejected_inputs[eye];
    if ((p.suppressed_mask & bit) || input.pass != saved.pass ||
        input.lookup.x[1] != saved.lookup.x[1] || input.record.prefix != saved.record.prefix ||
        input.record.label != saved.record.label || input.record.label_size != saved.record.label_size)
        return fail();
    for (unsigned i = 0; i < input.record.descriptors.size(); ++i)
        if (!same(input.record.descriptors[i],saved.record.descriptors[i])) return fail();
    p.suppressed_mask |= bit;
    if (p.suppressed_mask != 3) return true;
    if (!p.cb.rejected || !p.cb.rejected(p.cb.user,command,
        p.tickets[p.eye_slots[0]].provenance,p.tickets[p.eye_slots[1]].provenance) ||
        !p.native_current(true)) return fail();
    p.status.rejected = true; // still requires real queue, GPU, disposal and reset
    return true;
}
BackendResult Adapter::backend(const Recorded &record, const Context &context,
                               Registers lookup) noexcept {
    auto &p = *p_;
    // Unmarked stock lookup is unchanged, including mixed-command stock work.
    const auto marker = load<Address>(record.prefix, 8);
    if (!marker || marker != p.marker.load()) {
        if (!p.call(lookup)) return {Decision::Stock, 0};
        return {Decision::Stock, lookup.x[0]};
    }
    Impl::Guard guard(p);
    if (!guard.ok) return {};
    auto drop = [&]() { p.reject(); return BackendResult{}; };
    auto *ticket = p.authenticate(record, context);
    if (!ticket || ticket->consumed || !p.status.committed ||
        !p.gate.committed(p.frame)) return drop();
    const unsigned eye = ticket->provenance.eye;
    const auto &cached = p.prepared[eye];
    if (lookup.target != cached.lookup.target || lookup.x != cached.lookup.x ||
        record.label != cached.record.label || record.label_size != cached.record.label_size)
        return drop();
    for (unsigned i = 0; i < record.descriptors.size(); ++i)
        if (!same(record.descriptors[i], cached.record.descriptors[i])) return drop();
    // Revocation/expiry/external demand is NOT native lifetime revocation. Only
    // that latter condition can forbid finishing an already committed pair.
    if (!p.native_current()) { p.status.frame_invalid = true; return drop(); }
    ticket->consumed = ticket->admitted = true;
    p.status.cpu_done = p.tickets[p.eye_slots[0]].admitted && p.tickets[p.eye_slots[1]].admitted;
    return {Decision::Portal, p.pipelines[eye]}; // no new per-eye pipeline lookup
}
bool Adapter::submitted() noexcept {
    auto &p = *p_;
    Impl::Guard guard(p);
    if (!guard.ok)
        return false;
    // Observe already-submitted work under native lifetime ownership, not lease
    // authorization. This read-only callback must never queue new work.
    if (!p.status.initialized || p.status.retired ||
        (!p.status.cpu_done && (!p.status.rejected || p.rejection_bad)) ||
        p.status.queue_observed || !p.cb.submission || !p.validated(Phase::Current))
        return p.reject();
    Completion c;
    if (!p.cb.submission(p.cb.user, p.tickets[p.eye_slots[0]].provenance,
                         p.tickets[p.eye_slots[1]].provenance, c) ||
        c.frame != p.frame.id || !c.command || c.command != p.frame_arena.command ||
        !c.synchronization || !c.serial)
        return p.reject();
    // Save the exact lifetime token once, even if authorization has already ended.
    p.completion = c;
    p.status.queue_observed = true;
    if (p.status.rejected) return false; // never publish an owned stereo frame
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
    if (!p.status.queue_observed || !p.cb.completed || !p.validated(Phase::Current) ||
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
        (!p.status.cpu_done && (!p.status.rejected || p.rejection_bad)) ||
        !p.status.queue_observed || !p.status.gpu_complete ||
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
    p.gate.settle(p.frame);
    p.pending(false);
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
    p.gate.settle(p.frame);
    p.pending(false);
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
