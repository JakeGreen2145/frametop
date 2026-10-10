// SPDX-License-Identifier: MIT
#include "camera_owner.hpp"
#include <utility>

namespace native_portal::live {
namespace co=coordination;
CameraOwner::CameraOwner() noexcept:process_(getpid()),owner_(std::this_thread::get_id()),native_(gate_) {}
CameraOwner::~CameraOwner() {
    // No destructor-side native operations or fabricated settlement. The barrier
    // outlives all queued and committed work, including faulted source evidence.
    if(process_==getpid() && barrier_)std::terminate();
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
bool CameraOwner::bind(const CameraBinding& binding,uint64_t generation,size_t capacity) {
    if(!owner() || bound_ || !generation || !capacity || capacity>co::OwnerCoordinator::MaxSlots)return false;
    if(!native_.bind(binding,owner_,epoch_))return false;
    // This ticket is acquired BEFORE close, never manufactured by drain. A
    // concurrent precleanup can instead win and make this generation unusable.
    bound_=true;
    if(!gate_.retain(epoch_,barrier_))return false;
    queue_.emplace(epoch_,generation,capacity,retention());
    demand_.emplace(epoch_,generation,retention());return true;
}
co::NativeReceipt CameraOwner::invoke(uint64_t ticket,const NativeMutation& fn) {
    if(!owner() || active_ || !fn || !demand_)return {};
    CameraBinding held;
    if(!gate_.lookup(ticket,held))return {};
    struct Guard {bool& active;~Guard(){active=false;}} guard{active_};active_=true;
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
    if(s.fault || s.committed_debts || !s.native_idle || s.producer_demand)return false;
    if(!gate_.release(barrier_))return false;
    barrier_=0;return true;
}
binding::CameraNativeBinding::Dispatch CameraOwner::cleanup(uint64_t pc,arena::Saved& saved,
    CameraAccessGate::Cleanup original,void* argument,std::chrono::milliseconds timeout) {
    return native_.cleanup(pc,saved,original,argument,timeout);
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
