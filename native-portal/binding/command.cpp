// SPDX-License-Identifier: MIT
#include "command.hpp"
namespace native_portal::binding {
namespace {
bool same(const render::Identity &a, const render::Identity &b) noexcept {
    return a.process_instance == b.process_instance && a.renderer == b.renderer &&
           a.renderer_generation == b.renderer_generation && a.device == b.device &&
           a.device_generation == b.device_generation;
}
bool same(const Owner &a, const Owner &b) noexcept {
    return same(a.identity,b.identity) && a.recording == b.recording &&
           a.generation == b.generation && a.command == b.command &&
           a.vk_command == b.vk_command && a.arena == b.arena && a.buffer == b.buffer &&
           a.map == b.map && a.capacity == b.capacity && a.alignment == b.alignment &&
           a.queue == b.queue && a.semaphore == b.semaphore && a.synchronization == b.synchronization;
}
bool same(const Scope &a, const Scope &b) noexcept {
    return same(a.owner,b.owner) && a.invocation == b.invocation && a.tid == b.tid &&
           a.sp == b.sp && a.lr == b.lr && a.object == b.object;
}
bool same(const Signal &a, const Signal &b) noexcept {
    return a.synchronization == b.synchronization && a.serial == b.serial &&
           a.subserial == b.subserial;
}
bool same(const render::Completion &a, const render::Completion &b) noexcept {
    return a.frame == b.frame && a.command == b.command &&
           a.synchronization == b.synchronization && a.serial == b.serial &&
           a.subserial == b.subserial;
}
bool same(const render::Descriptor &a, const render::Descriptor &b) noexcept {
    return a.buffer == b.buffer && a.offset == b.offset && a.range == b.range;
}
bool pair(const render::Provenance &a, const render::Provenance &b) noexcept {
    return same(a.identity,b.identity) && a.frame == b.frame && a.lease == b.lease &&
           a.origin == b.origin && a.geometry == b.geometry && a.deadline == b.deadline &&
           a.revocation == b.revocation && a.recording_generation == b.recording_generation &&
           a.wrapper == b.wrapper && a.marker == b.marker;
}
bool same(const render::Provenance &a, const render::Provenance &b) noexcept {
    return pair(a,b) && a.ticket == b.ticket && a.eye == b.eye && a.vs == b.vs &&
           a.ps == b.ps && same(a.descriptors[0],b.descriptors[0]) &&
           same(a.descriptors[1],b.descriptors[1]);
}
}
Command::Command() noexcept : pid_(getpid()), thread_(std::this_thread::get_id()) {}
bool Command::thread() const noexcept {
    // Read only immutable fields until the owner thread and process are established.
    return getpid() == pid_ && std::this_thread::get_id() == thread_;
}
bool Command::scope(const Scope &s) const noexcept {
    return lifetime_ && state_.bound && same(s.owner,owner_) && s.invocation && s.tid &&
           s.sp && s.lr && s.object && (!backend_.tid || s.tid == backend_.tid);
}
bool Command::fail() noexcept { bad_ = true; return false; }
bool Command::bind(const Owner &o) noexcept {
    if (!thread() || state_.bound) return false;
    const auto &i = o.identity;
    if (!i.process_instance || !i.renderer || !i.renderer_generation || !i.device ||
        !i.device_generation || !o.recording || !o.generation || !o.command || !o.vk_command ||
        !o.arena || !o.buffer || !o.map || !o.capacity || !o.alignment ||
        (o.alignment & (o.alignment-1)) || !o.queue || !o.semaphore || !o.synchronization) return false;
    owner_ = o;
    state_.bound = lifetime_ = true;
    return true;
}
bool Command::backend_enter(const Scope &s, const render::Provenance &p) noexcept {
    if (!thread()) return false;
    if (!scope(s) || state_.stopped || state_.rejected || bad_ || queue_seen_ || backend_open_ || p.eye > 1 ||
        (state_.admitted & (1u << p.eye)) || !same(p.identity,owner_.identity) ||
        p.recording_generation != owner_.recording || !p.ticket || !p.frame || !p.wrapper ||
        !p.marker || !render::supported_pair(p.vs,p.ps)) return fail();
    const unsigned other = p.eye ^ 1;
    if ((state_.admitted & (1u << other)) &&
        (!pair(p,tickets_[other]) || p.ticket == tickets_[other].ticket)) return fail();
    for (const auto &d : p.descriptors)
        if (d.buffer != owner_.buffer || d.range != 592 || d.offset > owner_.capacity ||
            d.range > owner_.capacity - d.offset) return fail();
    tickets_[p.eye] = p;
    backend_ = s;
    backend_eye_ = p.eye;
    backend_open_ = state_.recording_debt = true;
    state_.admitted |= 1u << p.eye;
    return true;
}
bool Command::backend_return(const Scope &s, bool admitted) noexcept {
    if (!thread()) return false;
    if (!scope(s) || !backend_open_ || !same(s,backend_) || !admitted || bad_) return fail();
    backend_open_ = false;
    state_.returned |= 1u << backend_eye_;
    return true;
}
bool Command::pair_recorded() const noexcept {
    return !bad_ && (state_.returned == 3 ||
           (state_.rejected && !state_.admitted && !state_.returned && !backend_open_));
}
bool Command::rejected(const Scope &s, uint64_t command, const render::Provenance &a,
                       const render::Provenance &b) noexcept {
    if (!thread()) return false;
    if (!scope(s) || s.object != owner_.command || command != owner_.command ||
        bad_ || queue_seen_ || backend_open_ || state_.admitted || state_.returned ||
        state_.rejected || a.eye != 0 || b.eye != 1 || !pair(a,b) ||
        !a.ticket || !b.ticket || a.ticket == b.ticket) return fail();
    for (const auto *p : {&a,&b}) {
        if (!same(p->identity,owner_.identity) || p->recording_generation != owner_.recording ||
            !p->frame || !p->lease || !p->origin || !p->geometry || !p->deadline ||
            !p->wrapper || !p->marker || !render::supported_pair(p->vs,p->ps)) return fail();
        for (const auto &d : p->descriptors)
            if (d.buffer != owner_.buffer || d.range != 592 || d.offset > owner_.capacity ||
                d.range > owner_.capacity-d.offset) return fail();
    }
    tickets_ = {a,b}; backend_ = s;
    state_.recording_debt = state_.rejected = true;
    return true;
}
uint64_t Command::timeline_value(const Signal &s) noexcept {
    // Match native OR with SIGN extension, not uint8_t masking. Unsigned shift is defined.
    return (s.serial << 8) | static_cast<uint64_t>(static_cast<int64_t>(s.subserial));
}
bool Command::queue_enter(const Scope &s, const SubmitSnapshot &v, const Signal &token) noexcept {
    if (!thread()) return false;
    if (queue_seen_) {
        state_.unsupported = true;
        return fail(); // NOT authorization to suppress/cancel a native Vulkan call
    }
    // This observes an actual native call, not permission to issue one. Off stops
    // backend admission but cannot erase a call for an already-recorded pair.
    if (!scope(s) || bad_ || backend_open_ || !pair_recorded() ||
        s.object != owner_.command) return fail();
    // Retain the attempt even if payload validation fails. A later raw success is debt.
    queue_seen_ = true;
    queue_ = s;
    signal_ = token;
    queue_valid_ = v.readable && v.supported_chain && v.submit_count == 1 &&
        v.command_count == 1 && v.signal_count == 1 && v.timeline_signal_count == 1 &&
        v.queue == owner_.queue && v.device == owner_.identity.device &&
        v.command == owner_.vk_command && v.semaphore == owner_.semaphore && !v.fence &&
        token.synchronization == owner_.synchronization && token.serial && v.value == timeline_value(token);
    return queue_valid_ ? true : fail();
}
bool Command::queue_result(const Scope &s, int32_t raw) noexcept {
    if (!thread() || !queue_seen_) return false;
    // Conservative even for revoked lifetime or malformed result scope. Never
    // forget possible successful work; scope() still forbids its certification.
    if (!raw) state_.success_debt = true;
    if (!scope(s) || !same(s,queue_) || result_seen_ || bad_) return fail();
    result_seen_ = true;
    if (raw || !queue_valid_) return fail();
    token_ = {tickets_[0].frame, owner_.command, signal_.synchronization,
              signal_.serial, signal_.subserial};
    return true;
}
bool Command::stamp_enter(const Scope &q, const Scope &s, const Signal &token) noexcept {
    if (!thread()) return false;
    if (state_.certificate) return false; // immutable certificate cannot be replaced
    if (!scope(q) || !same(q,queue_) || !scope(s) || s.object != owner_.command ||
        !result_seen_ || !state_.success_debt || bad_ || stamp_open_ ||
        !same(token,signal_)) return fail();
    stamp_ = s;
    stamp_open_ = true;
    return true;
}
bool Command::resource_enter(const Scope &s, const Scope &r, const Signal &token) noexcept {
    if (!thread()) return false;
    if (!scope(s) || !same(s,stamp_) || !scope(r) || !stamp_open_ || resource_open_ || bad_ ||
        !same(token,signal_) || (r.object != owner_.arena && r.object != owner_.command)) return fail();
    const unsigned bit = r.object == owner_.arena ? 1 : 2;
    if (stamps_ & bit) return fail();
    resource_ = r;
    resource_open_ = true;
    return true;
}
bool Command::resource_post(const Scope &s, uint64_t object, const Signal &token) noexcept {
    if (!thread()) return false;
    if (!scope(s) || !resource_open_ || !same(s,resource_) || object != s.object ||
        !same(token,signal_) || bad_) return fail();
    stamps_ |= object == owner_.arena ? 1 : 2;
    resource_open_ = false;
    return true;
}
bool Command::stamp_post(const Scope &s, uint64_t command, const Signal &token) noexcept {
    if (!thread()) return false;
    if (state_.certificate) return false;
    if (!scope(s) || !same(s,stamp_) || !stamp_open_ || resource_open_ || stamps_ != 3 ||
        command != owner_.command || !same(token,signal_) || bad_) return fail();
    stamp_open_ = false;
    state_.certificate = true;
    return true;
}
bool Command::wait_enter(const Scope &s, const Signal &token, uint64_t semaphore) noexcept {
    if (!thread()) return false;
    if (!scope(s) || !state_.certificate || wait_open_ || wait_result_seen_ ||
        s.object != signal_.synchronization || semaphore != owner_.semaphore ||
        !same(token,signal_)) return false;
    wait_ = s;
    wait_open_ = true;
    return true;
}
bool Command::wait_result(const Scope &s, const Signal &token, int32_t raw) noexcept {
    if (!thread()) return false;
    if (!scope(s) || !wait_open_ || wait_result_seen_ || !same(s,wait_) ||
        !same(token,signal_)) {
        wait_ok_ = false;
        wait_open_ = false;
        return false;
    }
    wait_result_seen_ = true;
    wait_ok_ = raw == 0;
    return wait_ok_;
}
bool Command::wait_return(const Scope &s, const Signal &token, uint32_t boolean) noexcept {
    if (!thread()) return false;
    const bool ok = scope(s) && wait_open_ && wait_result_seen_ && wait_ok_ &&
                    same(s,wait_) && same(token,signal_) && boolean == 1;
    wait_open_ = false;
    if (ok) state_.gpu_complete = true;
    return ok;
}
void Command::off() noexcept { if (thread()) state_.stopped = true; }
void Command::invalidate_lifetime() noexcept {
    if (thread()) { lifetime_ = false; state_.stopped = true; }
}
State Command::state() const noexcept { return thread() ? state_ : State{}; }
bool Command::submission(const render::Provenance &a, const render::Provenance &b,
                         render::Completion &out) const noexcept {
    if (!thread() || !lifetime_ || !state_.certificate || !same(a,tickets_[0]) ||
        !same(b,tickets_[1])) return false;
    out = token_;
    return true;
}
bool Command::completed(const render::Binding &b, const render::Completion &c) const noexcept {
    return thread() && lifetime_ && state_.certificate && state_.gpu_complete &&
           !state_.unsupported && same(b.identity,owner_.identity) && same(c,token_);
}
bool Command::submission_callback(void *u, const render::Provenance &a,
                                  const render::Provenance &b, render::Completion &c) noexcept {
    return u && static_cast<Command *>(u)->submission(a,b,c);
}
bool Command::completed_callback(void *u, const render::Binding &b,
                                 const render::Completion &c) noexcept {
    return u && static_cast<Command *>(u)->completed(b,c);
}
} // namespace native_portal::binding
