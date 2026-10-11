// SPDX-License-Identifier: MIT
#include "camera_owner.hpp"
#include <utility>

namespace native_portal::live {
namespace co=coordination;
CameraOwner::CameraOwner() noexcept:process_(getpid()),owner_(std::this_thread::get_id()),native_(gate_) {}
CameraOwner::~CameraOwner() {
    // No destructor-side native operations or fabricated settlement. The barrier
    // outlives all queued and committed work, including faulted source evidence.
    if(process_==getpid()) {
        std::lock_guard<std::mutex> lock(callers_mutex_);
        if(barrier_ || callers_)std::terminate();
    }
}
bool CameraOwner::owner() const noexcept {
    return process_==getpid() && owner_==std::this_thread::get_id();
}
co::Retention CameraOwner::retention() noexcept {
    return {&gate_,[](void* p,uint64_t epoch,uint64_t& ticket) {
        return static_cast<CameraAccessGate*>(p)->retain(epoch,ticket);
    },[](void* p,uint64_t ticket) {return static_cast<CameraAccessGate*>(p)->release(ticket);}};
}
bool CameraOwner::observe_publication(uint64_t pc,const arena::Saved& saved) {
    return owner() && !bound_ && native_.observe_publication(pc,saved);
}
bool CameraOwner::bind_handoff(const CameraBinding& binding,uint64_t generation,size_t capacity) {
    if(process_!=getpid() || bound_ || active_)return false;
    const auto prior=owner_;owner_=std::this_thread::get_id();
    if(bind(binding,generation,capacity))return true;
    if(!bound_)owner_=prior; // a published epoch is dirty even if retain failed
    return false;
}
bool CameraOwner::retained_binding(CameraBinding& b) const {
    return owner() && barrier_ && gate_.lookup(barrier_,b);
}
CameraOwner::RetainedCaller::~RetainedCaller() {
    if(ticket_ && !release())std::terminate();
}
CameraOwner::RetainedCaller::RetainedCaller(RetainedCaller&& other) noexcept
    :owner_(std::exchange(other.owner_,nullptr)),ticket_(std::exchange(other.ticket_,0)) {}
CameraOwner::RetainedCaller& CameraOwner::RetainedCaller::operator=(RetainedCaller&& other) noexcept {
    if(this==&other)return *this;
    if(ticket_ && !release())std::terminate();
    owner_=std::exchange(other.owner_,nullptr);ticket_=std::exchange(other.ticket_,0);return *this;
}
bool CameraOwner::RetainedCaller::binding(CameraBinding& out) const {
    out={};
    return ticket_ && owner_->process_==getpid() && owner_->gate_.lookup(ticket_,out);
}
bool CameraOwner::RetainedCaller::release() noexcept {
    // Gate release is deliberately not issuer-thread-only. On failure preserve
    // BOTH the exact ticket and owner accounting so explicit retry remains real.
    if(!ticket_ || owner_->process_!=getpid())return false;
    try {
        std::lock_guard<std::mutex> lock(owner_->callers_mutex_);
        if(!owner_->gate_.release(ticket_))return false;
        --owner_->callers_;ticket_=0;owner_=nullptr;return true;
    } catch(...) {return false;}
}
CameraOwner::RetainedCaller CameraOwner::retain_caller(uint64_t epoch,uint64_t generation) {
    // Never touch an inherited mutex, owner thread ID, queue or arbiter here.
    if(process_!=getpid())return {};
    std::lock_guard<std::mutex> lock(callers_mutex_);
    // Bind publishes immutable epoch/generation and the scalar gate binding
    // before this release/acquire edge. Partial epochs grant no caller access.
    if(bind_stage_.load(std::memory_order_acquire)!=BindStage::Retained ||
       epoch!=epoch_ || generation!=generation_)return {};
    uint64_t ticket=0;
    // The existing gate serializes this acquisition with EVERY cleanup/close.
    if(!gate_.retain(epoch,ticket))return {};
    ++callers_;return RetainedCaller(*this,ticket);
}
bool CameraOwner::observe_scope(const ScopeObserver& fn) {
    if(!owner() || !active_ || !current_ticket_ || !fn)return false;
    CameraBinding held;if(!gate_.lookup(current_ticket_,held))return false;
    fn(gate_,current_ticket_,held);return true;
}
co::DemandSnapshot CameraOwner::snapshot() const {
    if(owner() && demand_)return demand_->snapshot();
    co::DemandSnapshot s;s.fault=true;return s;
}
co::NativeReceipt CameraOwner::direct(const NativeMutation& fn) {
    if(!owner() || active_ || closing_ || !barrier_)return {};
    // Reserve in the SAME FIFO as listeners. submit executes inline only if it
    // really is first. Never overtake an earlier retain or self-wait behind it.
    const auto submission=submit(epoch_,generation_,fn);if(!submission.id)return {};
    queue_->pump();co::Result result;
    if(!queue_->take(submission.id,result)) {
        if(!queue_->cancel(submission.id) || !queue_->take(submission.id,result))std::terminate();
    }
    return result.native;
}
bool CameraOwner::bind(const CameraBinding& binding,uint64_t generation,size_t capacity) {
    if(!owner() || bound_ || !generation || !capacity || capacity>co::OwnerCoordinator::MaxSlots)return false;
    uint64_t issued=0;
    if(!native_.bind(binding,owner_,issued)){bind_stage_=BindStage::CleanRefused;return false;}
    epoch_=issued;generation_=generation;bind_stage_=BindStage::Published;
    // This ticket is acquired BEFORE close, never manufactured by drain. A
    // concurrent precleanup can instead win and make this generation unusable.
    bound_=true;
    if(!gate_.retain(epoch_,barrier_))return false;
    queue_.emplace(epoch_,generation,capacity,retention());
    demand_.emplace(epoch_,generation,retention());
    bind_stage_.store(BindStage::Retained,std::memory_order_release);return true;
}
co::NativeReceipt CameraOwner::invoke(uint64_t ticket,const NativeMutation& fn) {
    if(!owner() || active_ || !fn || !demand_)return {};
    CameraBinding held;
    if(!gate_.lookup(ticket,held))return {};
    struct Guard {bool& active;uint64_t& ticket;~Guard(){ticket=0;active=false;}} guard{active_,current_ticket_};
    active_=true;current_ticket_=ticket;
    return fn(held,*demand_);
}
co::Submission CameraOwner::submit(uint64_t epoch,uint64_t generation,NativeMutation fn) {
    // Check PID before touching inherited queue/gate locks. No native lookup here.
    if(process_!=getpid() || !queue_ || !fn)return {0,co::Rejection::Invalid};
    return queue_->submit_scoped(epoch,generation,[this,fn=std::move(fn)](uint64_t ticket) {
        return invoke(ticket,fn);
    });
}
size_t CameraOwner::pump(size_t budget) {return owner() && queue_?queue_->pump(budget):0;}
co::Wait CameraOwner::wait(uint64_t id,co::Result& out) {
    return process_==getpid() && queue_?queue_->wait(id,out):co::Wait::Unknown;
}
bool CameraOwner::take(uint64_t id,co::Result& out) {
    return process_==getpid() && queue_ && queue_->take(id,out);
}
bool CameraOwner::retry_release(uint64_t id) {
    return process_==getpid() && queue_ && queue_->retry_release(id);
}
bool CameraOwner::begin_close() {
    if(!owner() || active_ || !demand_ || !barrier_)return false;
    if(!demand_->close() || !gate_.begin_close(epoch_))return false;
    closing_=true;queue_->close();return true;
}
co::NativeReceipt CameraOwner::drain(const NativeMutation& fn) {
    if(!owner() || !closing_ || !barrier_)return {};
    // The existing closing barrier is enough. New retain would (correctly) fail.
    try {return invoke(barrier_,fn);}catch(...) {co::NativeReceipt r;r.execution=co::Execution::Failed;return r;}
}
bool CameraOwner::finish_close() {
    if(!owner() || active_ || !closing_ || !barrier_ || !queue_->drained())return false;
    const auto s=demand_->snapshot();
    if(s.fault || s.committed_debts || s.producer_demand ||
       (s.integration_obligations && !s.native_idle))return false;
    std::lock_guard<std::mutex> lock(callers_mutex_);
    if(callers_)return false;
    // Relinquishing an unused closing barrier gives stock its cleanup back.
    // It does not change Unknown queues or authorize portal readiness/detach.
    if(!gate_.release(barrier_))return false;
    barrier_=0;bind_stage_=BindStage::Closed;return true;
}
binding::CameraNativeBinding::Dispatch CameraOwner::cleanup(uint64_t pc,arena::Saved& saved,
    CameraAccessGate::Cleanup original,void* argument,std::chrono::milliseconds timeout,bool initial_caller) {
    return native_.cleanup(pc,saved,original,argument,timeout,initial_caller);
}

RenderDebt::RenderDebt(render::Adapter& adapter,uint64_t watermark,SourceSettlement source) noexcept
    :adapter_(adapter),watermark_(watermark),source_(source) {}
co::Settlement RenderDebt::callback() noexcept {return {this,settled};}
bool RenderDebt::settled(void* p,uint64_t debt) {
    auto& self=*static_cast<RenderDebt*>(p);
    if(!debt || !self.watermark_ || (self.debt_ && self.debt_!=debt))return false;
    self.debt_=debt;
    auto s=self.adapter_.state();
    if(!s.initialized || s.tickets_issued!=self.watermark_)return false;
    // These call the real Command's exact completion and Lifecycle's disposal /
    // executable reset certificates. Off does not invalidate their lifetime scope.
    if(!self.adapter_.poll_completion() || !self.adapter_.reclaim())return false;
    if(!self.source_.settled || !self.source_.settled(self.source_.context,self.watermark_))return false;
    s=self.adapter_.state();
    return s.tickets_issued==self.watermark_ && s.queue_observed && s.gpu_complete &&
        s.cpu_settled && !s.pending && !s.tickets_used;
}
} // namespace native_portal::live
