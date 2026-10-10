#include "owner_coordinator.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace native_portal::coordination {
namespace {
constexpr auto exhausted=std::numeric_limits<uint64_t>::max();
bool release_ticket(Retention r,uint64_t ticket) noexcept {
    if(!ticket)return true;
    if(!r.release)return false;
    try {return r.release(r.context,ticket);}catch(...) {return false;}
}
bool same(Caller a,Caller b) {return a.kind==b.kind&&a.id==b.id&&a.generation==b.generation;}
}
OwnerCoordinator::OwnerCoordinator(uint64_t epoch,uint64_t generation,size_t capacity,Retention retention)
    :epoch_(epoch),generation_(generation),capacity_(capacity),retention_(retention),owner_(std::this_thread::get_id()) {
    if(!epoch||!generation||!capacity||capacity>MaxSlots)throw std::invalid_argument("owner coordinator bounds/identity");
}
OwnerCoordinator::~OwnerCoordinator() {
    close();
    // Destruction while a callback is active is a caller lifetime violation.
    // A failed release must keep the interlock alive, never silently drop it.
    if(!drained())std::terminate();
}
size_t OwnerCoordinator::find(uint64_t id) const {
    if(id)for(size_t i=0;i<capacity_;++i)if(slots_[i].phase!=Phase::Free&&slots_[i].id==id)return i;
    return capacity_;
}
size_t OwnerCoordinator::first_pending() const {
    size_t first=capacity_;
    for(size_t i=0;i<capacity_;++i) {
        const auto& s=slots_[i];
        if((s.phase==Phase::Retaining||s.phase==Phase::Queued)&&
           (first==capacity_||s.id<slots_[first].id))first=i;
    }
    return first;
}
Submission OwnerCoordinator::submit(uint64_t epoch,uint64_t generation,Mutation mutation) {
    if(!mutation)return submit_scoped(epoch,generation,{});
    return submit_scoped(epoch,generation,[fn=std::move(mutation)](uint64_t){return fn();});
}
Submission OwnerCoordinator::submit_scoped(uint64_t epoch,uint64_t generation,ScopedMutation mutation) {
    size_t i=capacity_;uint64_t id=0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(epoch!=epoch_||generation!=generation_)return {0,Rejection::Stale};
        if(closing_)return {0,Rejection::Closing};
        if(!mutation)return {0,Rejection::Invalid};
        if(!retention_.retain||!retention_.release)return {0,Rejection::Retention};
        if(next_==exhausted)return {0,Rejection::Exhausted};
        for(size_t j=0;j<capacity_;++j)if(slots_[j].phase==Phase::Free){i=j;break;}
        if(i==capacity_)return {0,Rejection::Full};
        auto& s=slots_[i];s.phase=Phase::Retaining;s.id=id=++next_;s.mutation=std::move(mutation);
    }
    uint64_t ticket=0;bool retained=false;
    try {retained=retention_.retain(retention_.context,epoch_,ticket);}catch(...) {}
    bool finish_now=false,inline_now=false;
    ScopedMutation rejected;
    {
        std::lock_guard<std::mutex> lock(mutex_);auto& s=slots_[i];s.ticket=ticket;
        if(!retained||!ticket) {
            if(!ticket) {
                rejected=std::move(s.mutation);s=Slot{};changed_.notify_all();
                // The lock dies before rejected's potentially reentrant captures.
                return {0,Rejection::Retention};
            }
            s.result.reason=Rejection::Retention;s.phase=Phase::Releasing;finish_now=true;
        } else if(s.cancelled||closing_) {
            s.result.reason=closing_?Rejection::Closing:Rejection::Cancelled;
            s.phase=Phase::Releasing;finish_now=true;
        } else {
            s.phase=Phase::Queued;
            inline_now=std::this_thread::get_id()==owner_&&!active_&&first_pending()==i;
        }
    }
    if(finish_now)finish(i);
    else if(inline_now)pump(1);
    return {id,retained?Rejection::None:Rejection::Retention};
}
bool OwnerCoordinator::finish(size_t i) {
    uint64_t ticket;
    ScopedMutation retired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ticket=slots_[i].ticket;retired=std::move(slots_[i].mutation);
    }
    // Destroy captures before releasing the native lifetime, outside queue lock.
    retired=nullptr;
    const bool released=release_ticket(retention_,ticket);
    {
        std::lock_guard<std::mutex> lock(mutex_);auto& s=slots_[i];
        s.result.retention_released=released;
        if(released)s.ticket=0;
        s.phase=released?Phase::Done:Phase::Quarantined;
        changed_.notify_all();
    }
    return released;
}
size_t OwnerCoordinator::pump(size_t budget) {
    if(std::this_thread::get_id()!=owner_)return 0;
    size_t count=0;
    while(count<budget) {
        size_t i;uint64_t ticket=0;ScopedMutation mutation;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(active_)break;
            i=first_pending();
            if(i==capacity_||slots_[i].phase!=Phase::Queued)break;
            active_=true;slots_[i].phase=Phase::Running;mutation=std::move(slots_[i].mutation);
            ticket=slots_[i].ticket;
        }
        NativeReceipt receipt;
        try {receipt=mutation(ticket);}catch(...) {receipt.execution=Execution::Failed;}
        mutation=nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            slots_[i].result.native=receipt;slots_[i].phase=Phase::Releasing;
        }
        finish(i);
        {std::lock_guard<std::mutex> lock(mutex_);active_=false;}
        ++count;
    }
    return count;
}
bool OwnerCoordinator::cancel(uint64_t id) {
    size_t i;
    {
        std::lock_guard<std::mutex> lock(mutex_);i=find(id);if(i==capacity_)return false;
        auto& s=slots_[i];
        if(s.phase==Phase::Retaining) {
            if(s.cancelled)return false;
            s.cancelled=true;return true;
        }
        if(s.phase!=Phase::Queued)return false;
        s.phase=Phase::Releasing;s.result.reason=Rejection::Cancelled;
    }
    finish(i);return true;
}
void OwnerCoordinator::close() {
    std::array<size_t,MaxSlots> cancelled{};size_t count=0;
    {
        std::lock_guard<std::mutex> lock(mutex_);closing_=true;
        for(size_t i=0;i<capacity_;++i) {
            auto& s=slots_[i];
            if(s.phase==Phase::Retaining)s.cancelled=true;
            else if(s.phase==Phase::Queued) {
                s.phase=Phase::Releasing;s.result.reason=Rejection::Closing;cancelled[count++]=i;
            }
        }
    }
    for(size_t n=0;n<count;++n)finish(cancelled[n]);
}
Wait OwnerCoordinator::wait(uint64_t id,Result& result) {
    std::unique_lock<std::mutex> lock(mutex_);
    for(;;) {
        const auto i=find(id);if(i==capacity_)return Wait::Unknown;
        const auto phase=slots_[i].phase;
        if(phase==Phase::Done||phase==Phase::Quarantined){result=slots_[i].result;return Wait::Ready;}
        if(std::this_thread::get_id()==owner_)return Wait::OwnerWouldBlock;
        changed_.wait(lock);
    }
}
bool OwnerCoordinator::take(uint64_t id,Result& result) {
    std::lock_guard<std::mutex> lock(mutex_);const auto i=find(id);
    if(i==capacity_||slots_[i].phase!=Phase::Done)return false;
    result=slots_[i].result;slots_[i]=Slot{};changed_.notify_all();return true;
}
bool OwnerCoordinator::retry_release(uint64_t id) {
    size_t i;
    {
        std::lock_guard<std::mutex> lock(mutex_);i=find(id);
        if(i==capacity_||slots_[i].phase!=Phase::Quarantined)return false;
        slots_[i].phase=Phase::Releasing;
    }
    // take() can reclaim/reuse the slot as soon as finish publishes Done.
    // Return this release's result, never a later occupant's phase.
    return finish(i);
}
bool OwnerCoordinator::drained() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if(active_)return false;
    for(size_t i=0;i<capacity_;++i)if(slots_[i].phase!=Phase::Free&&slots_[i].phase!=Phase::Done)return false;
    return true;
}

DemandArbiter::DemandArbiter(uint64_t epoch,uint64_t generation,Retention retention)
    :epoch_(epoch),generation_(generation),retention_(retention),owner_(std::this_thread::get_id()) {
    if(!epoch||!generation)throw std::invalid_argument("demand generation identity");
}
DemandArbiter::~DemandArbiter() {
    for(const auto& debt:debts_)if(debt.id)std::terminate();
}
bool DemandArbiter::owner() const {return owner_==std::this_thread::get_id();}
bool DemandArbiter::mutable_now() const {return owner()&&!busy_;}
size_t DemandArbiter::caller_index(Caller key) const {
    for(size_t i=0;i<MaxCallers;++i)if(callers_[i].used&&same(callers_[i].key,key))return i;
    return MaxCallers;
}
size_t DemandArbiter::token_index(PortalToken token) const {
    if(token.id&&token.epoch==epoch_&&token.generation==generation_)
        for(size_t i=0;i<MaxTokens;++i)if(tokens_[i].token.id==token.id)return i;
    return MaxTokens;
}
bool DemandArbiter::register_caller(Caller key) {
    if(!mutable_now()||closing_||!key.id||!key.generation)return false;
    size_t free=MaxCallers;
    for(size_t i=0;i<MaxCallers;++i) {
        auto& c=callers_[i];if(!c.used){if(free==MaxCallers)free=i;continue;}
        if(c.key.kind!=key.kind||c.key.id!=key.id)continue;
        if(c.key.generation==key.generation)return !c.revoked;
        if(key.generation<c.key.generation||!c.revoked)return false;
        for(const auto& t:tokens_)if(t.token.id&&t.caller==i)return false;
        c={true,false,false,false,key};return true;
    }
    if(free==MaxCallers)return false;
    callers_[free]={true,false,false,false,key};return true;
}
bool DemandArbiter::set_level(Caller key,bool producer,bool full) {
    if(!mutable_now()||closing_||key.kind==CallerKind::Portal||(full&&!producer))return false;
    const auto i=caller_index(key);if(i==MaxCallers||callers_[i].revoked)return false;
    callers_[i].producer=producer;callers_[i].full=full;return true;
}
bool DemandArbiter::connection_lost(Caller key) {
    if(!mutable_now())return false;
    const auto i=caller_index(key);if(i==MaxCallers||callers_[i].revoked)return false;
    auto& c=callers_[i];c.revoked=true;c.producer=false;c.full=false;
    for(size_t j=0;j<MaxTokens;++j)if(tokens_[j].token.id&&tokens_[j].caller==i) {
        tokens_[j].active=false;retire_token(j);
    }
    return true;
}
PortalToken DemandArbiter::portal_on(Caller key) {
    if(!mutable_now()||closing_||fault_||key.kind!=CallerKind::Portal||!retention_.retain||!retention_.release)return {};
    const auto caller=caller_index(key);if(caller==MaxCallers||callers_[caller].revoked)return {};
    size_t free=MaxTokens;
    for(size_t i=0;i<MaxTokens;++i) {
        const auto& t=tokens_[i];
        if(!t.token.id){if(free==MaxTokens)free=i;continue;}
        if(t.caller==caller)return t.active?t.token:PortalToken{};
    }
    if(free==MaxTokens||issued_tokens_==exhausted)return {};
    auto& t=tokens_[free];t.token={epoch_,generation_,++issued_tokens_};t.caller=caller;t.active=true;
    return t.token;
}
void DemandArbiter::retire_token(size_t i) {
    if(!tokens_[i].active&&!tokens_[i].debts)tokens_[i]=TokenSlot{};
    if(use_==UseDisposition::AuthenticatedBorrowedUse) {
        for(const auto& token:tokens_)if(token.token.id)return;
        // End only this local permission. The next On needs its own authentic
        // native receipt, possibly a different disposition after stock detached.
        // Keep queue observations and never assert global native idle.
        use_=UseDisposition::None;connect_attempted_=false;
    }
}
bool DemandArbiter::portal_off(PortalToken token) {
    if(!mutable_now())return false;
    const auto i=token_index(token);if(i==MaxTokens)return false;
    tokens_[i].active=false;retire_token(i);return true;
}
bool DemandArbiter::frame_admissible(PortalToken token) const {
    if(!owner()||busy_)return false;
    const auto i=token_index(token);return i!=MaxTokens&&tokens_[i].active&&snapshot().portal_ready;
}
uint64_t DemandArbiter::commit(PortalToken token,Settlement settlement) {
    if(!mutable_now()||!settlement.settled||!frame_admissible(token)||issued_debts_==exhausted)return 0;
    size_t free=MaxDebts;
    for(size_t i=0;i<MaxDebts;++i)if(!debts_[i].id){free=i;break;}
    if(free==MaxDebts)return 0;
    const auto t=token_index(token);uint64_t ticket=0;bool retained=false;busy_=true;
    try {retained=retention_.retain(retention_.context,epoch_,ticket);}catch(...) {}
    busy_=false;
    if(!retained||!ticket) {
        // A callback that violates false=>no acquisition is quarantined as debt.
        if(ticket) {
            debts_[free]={++issued_debts_,ticket,t,{},false};++tokens_[t].debts;fault_=true;
        }
        return 0;
    }
    debts_[free]={++issued_debts_,ticket,t,settlement,false};++tokens_[t].debts;
    return debts_[free].id;
}
bool DemandArbiter::poll_settlement(uint64_t id) {
    if(!mutable_now()||!id)return false;
    for(auto& debt:debts_)if(debt.id==id) {
        busy_=true;
        if(!debt.proven&&debt.settlement.settled) {
            try {debt.proven=debt.settlement.settled(debt.settlement.context,id);}catch(...) {}
        }
        const bool released=debt.proven&&release_ticket(retention_,debt.ticket);busy_=false;
        if(!released)return false;
        const auto t=debt.token;--tokens_[t].debts;debt=DebtSlot{};retire_token(t);return true;
    }
    return false;
}
bool DemandArbiter::close() {
    if(!mutable_now())return false;
    closing_=true;
    for(size_t i=0;i<MaxTokens;++i){tokens_[i].active=false;retire_token(i);}
    return true;
}
DemandSnapshot DemandArbiter::snapshot() const {
    DemandSnapshot s;
    if(!owner()){s.fault=true;return s;}
    s.closing=closing_;s.fault=fault_;s.native_idle=true;
    s.integration_obligations=connect_attempted_;s.use=use_;
    if(!closing_)for(const auto& c:callers_)if(c.used&&!c.revoked) {
        s.producer_demand|=c.producer;
        if(c.key.kind==CallerKind::Stock)s.legacy_stock_full|=c.full;
        if(c.key.kind==CallerKind::Foreign)s.foreign_full|=c.full;
    }
    for(const auto& t:tokens_)s.producer_demand|=t.active||t.debts;
    for(const auto& d:debts_)s.committed_debts+=d.id!=0;
    s.stock_full_display=s.legacy_stock_full||s.foreign_full;
    s.portal_ready=!closing_&&!fault_&&retention_.retain&&retention_.release;
    for(const auto& q:queues_) {
        s.native_idle&=q.state==QueueState::Disconnected && q.ownership!=Ownership::NativeConnection;
        s.portal_ready&=use_==UseDisposition::AuthenticatedBorrowedUse ||
            (q.state==QueueState::Connected && (q.ownership==Ownership::NewlyOwned ||
             (use_==UseDisposition::NativeConnection && q.ownership==Ownership::NativeConnection)));
    }
    if(use_==UseDisposition::AuthenticatedBorrowedUse && !s.producer_demand && !s.committed_debts)
        s.integration_obligations=false; // relinquish only local use, never global idle
    return s;
}
std::array<QueueJournal,2> DemandArbiter::queues() const {
    if(!owner())return {};
    return queues_;
}
ActionPlan DemandArbiter::plan() {
    ActionPlan p;p.epoch=epoch_;p.generation=generation_;p.queues=queues_;
    const auto s=snapshot();
    if(fault_||issued_plans_==exhausted)return p;
    if(s.producer_demand&&!closing_&&!connect_attempted_) {
        p.action=Action::Connect;p.selected={true,true};p.reattach=reattach_;
    } else if(!s.producer_demand) {
        for(size_t i=0;i<2;++i)if(queues_[i].state==QueueState::Connected&&(queues_[i].ownership==Ownership::NewlyOwned || queues_[i].ownership==Ownership::NativeConnection)) {
            p.action=Action::Disconnect;p.selected[i]=true;
        }
    }
    if(p.action!=Action::None)p.serial=++issued_plans_;
    return p;
}
void DemandArbiter::apply(const ActionPlan& p,const NativeReceipt& r) {
    if(r.execution==Execution::NotExecuted) {
        // NotExecuted is a binding promise of zero native effects. Contradictory
        // observations cannot be used to authorize another Connect attempt.
        if(!r.value_valid&&!r.queues[0].observed&&!r.queues[1].observed)return;
        fault_=true;
        for(size_t i=0;i<2;++i)if(p.selected[i]) {
            queues_[i].state=QueueState::Unknown;
            if(p.action==Action::Connect)queues_[i].connect=r.queues[i];
            else queues_[i].disconnect=r.queues[i];
        }
        if(p.action==Action::Connect)connect_attempted_=true;
        return;
    }
    if(p.action==Action::Connect)connect_attempted_=true;
    if(p.action==Action::Connect && r.use==UseDisposition::AuthenticatedBorrowedUse) {
        // An empty native warm Acquire is not two observed queue calls. Preserve
        // exactly the observations; this permission carries no detach ownership.
        if(r.execution!=Execution::Executed || !r.value_valid || r.value!=1) {fault_=true;return;}
        for(size_t i=0;i<2;++i) {
            const auto& v=r.queues[i];
            if(v.observed && (!v.value_valid || v.value!=0 || !v.id || v.ownership!=Ownership::Borrowed))fault_=true;
            queues_[i].connect=v;queues_[i].ownership=Ownership::Borrowed;
            if(v.observed){queues_[i].id=v.id;queues_[i].state=v.state;}
        }
        if(!fault_)use_=r.use;
        return;
    }
    if(p.action==Action::Connect && r.use==UseDisposition::NativeConnection)use_=r.use;
    if(r.execution!=Execution::Executed||!r.value_valid)fault_=true;
    if(p.reattach && r.use!=UseDisposition::NativeConnection && !r.producer_revalidated)fault_=true;
    for(size_t i=0;i<2;++i) {
        const auto& v=r.queues[i];auto& q=queues_[i];
        if(!p.selected[i]) {if(v.observed)fault_=true;continue;}
        if(p.action==Action::Connect)q.connect=v;else q.disconnect=v;
        if(!v.observed) {q.state=QueueState::Unknown;fault_=true;continue;}
        if(p.action==Action::Connect) {
            q.id=v.id;q.header_generation=v.header_generation;q.ownership=v.ownership;q.state=v.state;
            if(!v.value_valid||v.state!=QueueState::Connected||!v.id||
               ((!v.header_generation || (v.ownership!=Ownership::NewlyOwned && v.ownership!=Ownership::Borrowed)) &&
                !(r.use==UseDisposition::NativeConnection && v.ownership==Ownership::NativeConnection)))fault_=true;
        } else {
            // Preserve old obligations on missing/mismatched release evidence.
            if(!v.value_valid||v.id!=q.id||v.header_generation!=q.header_generation||
               v.ownership!=q.ownership||v.state!=QueueState::Disconnected) {
                q.state=QueueState::Unknown;fault_=true;
            } else q.state=QueueState::Disconnected;
        }
    }
    // Consumer detach is not driver Deactivate or attempted-once producer Create.
    // Only a full observed detach with zero debts permits another consumer plan.
    // Partial, borrowed, failed and unobserved transitions never clear this latch.
    if(p.action==Action::Disconnect&&!fault_&&!snapshot().committed_debts) {
        bool detached=true;
        for(const auto& q:queues_)
            detached&=q.state==QueueState::Disconnected && (q.ownership==Ownership::NewlyOwned ||
                (use_==UseDisposition::NativeConnection && q.ownership==Ownership::NativeConnection));
        if(detached){connect_attempted_=false;reattach_=true;}
    }
}
NativeReceipt DemandArbiter::reconcile(const Executor& executor) {
    if(!mutable_now()||!executor||!retention_.retain||!retention_.release)return {};
    const auto p=plan();if(p.action==Action::None)return {};
    busy_=true;NativeReceipt receipt;
    try {receipt=executor(p);}catch(...) {receipt.execution=Execution::Failed;}
    apply(p,receipt);busy_=false;return receipt;
}
}
