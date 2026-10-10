#include "owner_coordinator.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>
using namespace native_portal::coordination;
static_assert(!std::is_copy_constructible<DemandArbiter>::value,"retained debt cannot be copied");
static_assert(!std::is_move_constructible<DemandArbiter>::value,"native owner cannot move");

// In-memory destruction interlock, not a native runtime certificate.
struct Gate {
    std::mutex mutex;
    uint64_t next=0;
    std::map<uint64_t,uint64_t> tickets;
    bool closing=false, fail_release=false;
    static bool retain(void* p,uint64_t epoch,uint64_t& ticket) {
        auto& g=*static_cast<Gate*>(p); std::lock_guard<std::mutex> l(g.mutex);
        if(g.closing||epoch!=7)return false;
        ticket=++g.next; g.tickets.emplace(ticket,epoch); return true;
    }
    static bool release(void* p,uint64_t ticket) {
        auto& g=*static_cast<Gate*>(p); std::lock_guard<std::mutex> l(g.mutex);
        if(g.fail_release)return false;
        return g.tickets.erase(ticket)==1;
    }
    Retention callbacks() { return {this,retain,release}; }
    size_t size() { std::lock_guard<std::mutex> l(mutex);return tickets.size(); }
};
struct Signal {
    std::mutex mutex; std::condition_variable cv; bool value=false;
    void set() {std::lock_guard<std::mutex> l(mutex);value=true;cv.notify_all();}
    void wait() {std::unique_lock<std::mutex> l(mutex);cv.wait(l,[&]{return value;});}
    bool bounded_wait() {std::unique_lock<std::mutex> l(mutex);return cv.wait_for(l,std::chrono::seconds(2),[&]{return value;});}
};
NativeReceipt native(int64_t value) {
    NativeReceipt r; r.execution=Execution::Executed; r.value_valid=true;r.value=value;return r;
}
void queue_fifo_and_retention() {
    Gate gate; OwnerCoordinator q(7,9,3,gate.callbacks());
    std::vector<int> calls; Submission a,b,c;
    std::thread foreign([&]{
        a=q.submit(7,9,[&]{calls.push_back(1);return native(0);});
        b=q.submit(7,9,[&]{calls.push_back(2);return native(-27);});
        c=q.submit(7,9,[&]{calls.push_back(3);return native(1);});
    }); foreign.join();
    assert(gate.size()==3);
    assert(q.submit(7,9,[]{return native(5);}).rejection==Rejection::Full);
    assert(q.submit(7,8,[]{return native(5);}).rejection==Rejection::Stale);
    Result result;
    assert(q.wait(a.id,result)==Wait::OwnerWouldBlock);
    std::thread waiter([&]{assert(q.wait(b.id,result)==Wait::Ready);});
    assert(q.pump()==3); waiter.join();
    assert((calls==std::vector<int>{1,2,3}));
    assert(result.native.execution==Execution::Executed&&result.native.value==-27);
    assert(gate.size()==0);
    assert(q.take(a.id,result)&&q.take(b.id,result)&&q.take(c.id,result));
    assert(q.drained());
    auto inline_call=q.submit(7,9,[]{return native(0);});
    assert(q.take(inline_call.id,result)&&result.native.value_valid&&result.native.value==0);
}
void queue_cancel_close_reentrancy() {
    Gate gate; OwnerCoordinator q(7,9,4,gate.callbacks());
    Submission a,b,nested; int calls=0;
    std::thread t([&]{
        a=q.submit(7,9,[&]{++calls;return native(1);});
        b=q.submit(7,9,[&]{++calls;return native(2);});
    });t.join();
    assert(q.cancel(a.id));assert(!q.cancel(a.id));
    Result r;assert(q.take(a.id,r)&&r.native.execution==Execution::NotExecuted);
    assert(gate.size()==1);
    assert(q.pump()==1&&calls==1);assert(q.take(b.id,r));
    auto outer=q.submit(7,9,[&]{
        nested=q.submit(7,9,[&]{++calls;return native(8);});
        Result pending;assert(q.wait(nested.id,pending)==Wait::OwnerWouldBlock);
        assert(q.pump()==0); // recursion cannot execute ahead of the active mutation
        return native(6);
    });
    assert(q.take(outer.id,r)&&r.native.value==6);
    assert(q.pump()==1);assert(q.take(nested.id,r)&&r.native.value==8);
    std::thread t2([&]{a=q.submit(7,9,[&]{++calls;return native(9);});});t2.join();
    q.close();assert(q.take(a.id,r)&&r.native.execution==Execution::NotExecuted);
    assert(q.submit(7,9,[]{return native(4);}).rejection==Rejection::Closing);
    assert(q.drained()&&gate.size()==0);
}
void queue_cancel_execution_race() {
    Gate gate; Signal entered,finish,ready; OwnerCoordinator* owner=nullptr;
    std::thread worker([&]{OwnerCoordinator q(7,9,2,gate.callbacks());owner=&q;ready.set();
        entered.wait();assert(q.pump()==1);finish.wait();});
    ready.wait(); Signal executing,resume; auto s=owner->submit(7,9,[&]{executing.set();resume.wait();return native(-71);});
    entered.set();executing.wait();assert(!owner->cancel(s.id));owner->close();
    assert(gate.size()==1);resume.set();Result r;assert(owner->wait(s.id,r)==Wait::Ready);
    assert(r.native.execution==Execution::Executed&&r.native.value==-71);
    assert(owner->take(s.id,r));assert(gate.size()==0);finish.set();worker.join();
}
void queue_retaining_fifo_and_close() {
    struct BlockingGate {
        Gate gate;Signal entered,resume;
        static bool retain(void* p,uint64_t epoch,uint64_t& ticket) {
            auto& b=*static_cast<BlockingGate*>(p);
            const bool ok=Gate::retain(&b.gate,epoch,ticket);
            if(ticket==1){b.entered.set();b.resume.wait();}
            return ok;
        }
        static bool release(void* p,uint64_t ticket) {
            return Gate::release(&static_cast<BlockingGate*>(p)->gate,ticket);
        }
    } gate;
    OwnerCoordinator q(7,9,2,{&gate,BlockingGate::retain,BlockingGate::release});Submission first,second;
    int calls=0;
    std::thread a([&]{first=q.submit(7,9,[&]{++calls;return native(1);});});
    gate.entered.wait();
    std::thread b([&]{second=q.submit(7,9,[&]{++calls;return native(2);});});b.join();
    assert(q.pump()==0&&gate.gate.size()==2); // pending retain keeps FIFO position
    q.close();assert(!q.drained()&&gate.gate.size()==1);
    gate.resume.set();a.join();Result result;
    assert(q.take(first.id,result)&&result.native.execution==Execution::NotExecuted);
    assert(q.take(second.id,result)&&result.native.execution==Execution::NotExecuted);
    assert(q.drained()&&gate.gate.size()==0&&calls==0);
}
void queue_gate_failure() {
    Gate gate;OwnerCoordinator absent(7,9,2,{});
    assert(absent.submit(7,9,[]{return native(1);}).rejection==Rejection::Retention);
    OwnerCoordinator q(7,9,2,gate.callbacks());gate.fail_release=true;
    auto s=q.submit(7,9,[]{return native(0);});Result r;
    assert(q.wait(s.id,r)==Wait::Ready&&r.native.execution==Execution::Executed);
    assert(!r.retention_released&&!q.drained()&&!q.take(s.id,r));
    gate.fail_release=false;assert(q.retry_release(s.id));assert(q.take(s.id,r));
    auto throws=q.submit(7,9,[]()->NativeReceipt{throw 1;});
    assert(q.take(throws.id,r)&&r.native.execution==Execution::Failed&&!r.native.value_valid);
    assert(gate.size()==0);
}
void rejected_capture_reentrancy() {
    Gate gate;gate.closing=true;OwnerCoordinator q(7,9,1,gate.callbacks());Signal destroyed;
    struct Capture {
        OwnerCoordinator* q;Signal* destroyed;
        Capture(OwnerCoordinator* queue,Signal* signal):q(queue),destroyed(signal){}
        ~Capture(){q->drained();destroyed->set();}
    };
    std::thread reject([&]{
        auto capture=std::make_shared<Capture>(&q,&destroyed);
        OwnerCoordinator::Mutation mutation=[capture]{return native(5);};capture.reset();
        assert(q.submit(7,9,std::move(mutation)).rejection==Rejection::Retention);
    });
    assert(destroyed.bounded_wait());reject.join();assert(q.drained());
}
struct NativeExecutor {
    std::vector<ActionPlan> plans;
    bool partial=false, lie_release=false, borrow=false;
    NativeReceipt operator()(const ActionPlan& plan) {
        plans.push_back(plan);auto r=native(plan.action==Action::Connect?11:22);
        for(size_t i=0;i<2;++i)if(plan.selected[i]) {
            auto& v=r.queues[i];v.observed=true;v.value_valid=true;v.value=0;
            v.id=plan.action==Action::Connect?101+i:plan.queues[i].id;
            v.header_generation=plan.action==Action::Connect?81:plan.queues[i].header_generation;
            v.ownership=plan.action==Action::Connect?(borrow?Ownership::Borrowed:Ownership::NewlyOwned):plan.queues[i].ownership;
            v.state=plan.action==Action::Connect?QueueState::Connected:QueueState::Disconnected;
            if(partial&&i==1){v.state=QueueState::Unknown;v.value=-1;}
            if(lie_release&&plan.action==Action::Disconnect)v.state=QueueState::Unknown;
        }
        return r;
    }
};
struct SettlementProof {
    bool cpu=false,gpu=false,source=false;
    static bool settled(void* p,uint64_t id) {assert(id);auto& s=*static_cast<SettlementProof*>(p);return s.cpu&&s.gpu&&s.source;}
};
Caller stock{CallerKind::Stock,1,1},foreign{CallerKind::Foreign,2,1},portal{CallerKind::Portal,3,1};
void demand_interleaving_and_debt() {
    Gate gate;DemandArbiter d(7,9,gate.callbacks());NativeExecutor ex;
    assert(d.register_caller(stock)&&d.register_caller(foreign)&&d.register_caller(portal));
    auto token=d.portal_on(portal);assert(token.id&&d.portal_on(portal).id==token.id);
    auto state=d.snapshot();assert(state.producer_demand&&!state.stock_full_display&&!state.portal_ready);
    assert(!state.native_idle&&d.queues()[0].state==QueueState::Unknown); // no observed native state yet
    auto connect=d.reconcile([&](const ActionPlan& p){return ex(p);});
    assert(connect.execution==Execution::Executed&&connect.value==11);
    assert(d.snapshot().portal_ready&&ex.plans.size()==1);
    assert(d.set_level(stock,true,true));assert(d.set_level(foreign,true,true));
    assert(d.set_level(foreign,true,true)); // Acquire remains a level, not a native count
    assert(d.reconcile([&](const ActionPlan& p){return ex(p);}).execution==Execution::NotExecuted);
    assert(d.snapshot().stock_full_display&&ex.plans.size()==1);
    SettlementProof settled;auto debt=d.commit(token,{&settled,SettlementProof::settled});assert(debt&&gate.size()==1);
    assert(d.portal_off(token)&&!d.frame_admissible(token));
    assert(!d.portal_on(portal).id); // old committed obligation cannot be replaced
    assert(d.set_level(stock,false,false)&&d.connection_lost(foreign));
    assert(!d.set_level(foreign,true,true));
    assert(!d.snapshot().stock_full_display&&d.snapshot().producer_demand);
    assert(d.reconcile([&](const ActionPlan& p){return ex(p);}).execution==Execution::NotExecuted);
    settled.cpu=true;settled.gpu=true;assert(!d.poll_settlement(debt));
    assert(d.snapshot().producer_demand&&gate.size()==1);
    settled.source=true;assert(d.poll_settlement(debt));assert(gate.size()==0);
    assert(!d.snapshot().producer_demand);
    assert(d.reconcile([&](const ActionPlan& p){return ex(p);}).value==22);
    assert(ex.plans.size()==2&&d.snapshot().native_idle);
    auto next=d.portal_on(portal);assert(next.id>token.id&&!d.portal_off(token));
    assert(d.portal_off(next));
    assert(!d.register_caller(foreign));foreign.generation=2;assert(d.register_caller(foreign));
    assert(d.set_level(foreign,true,true));assert(!d.connection_lost({CallerKind::Foreign,2,1}));
    assert(d.snapshot().stock_full_display);
}
void demand_failures_borrow_and_close() {
    Gate gate;DemandArbiter d(7,9,gate.callbacks());NativeExecutor ex;ex.partial=true;
    assert(d.register_caller(portal));auto t=d.portal_on(portal);assert(t.id);
    auto r=d.reconcile([&](const ActionPlan& p){return ex(p);});assert(r.value==11);
    assert(d.snapshot().fault&&d.snapshot().producer_demand&&!d.snapshot().portal_ready);
    assert(d.queues()[0].state==QueueState::Connected&&d.queues()[0].id==101);
    assert(d.queues()[1].state==QueueState::Unknown);
    assert(d.reconcile([&](const ActionPlan& p){return ex(p);}).execution==Execution::NotExecuted);
    assert(ex.plans.size()==1);d.close();assert(!d.portal_on(portal).id);
    assert(d.snapshot().fault&&!d.snapshot().native_idle);

    DemandArbiter borrowed(7,10,gate.callbacks());NativeExecutor bx;bx.borrow=true;
    assert(borrowed.register_caller(stock)&&borrowed.set_level(stock,true,true));
    borrowed.reconcile([&](const ActionPlan& p){return bx(p);});
    assert(borrowed.set_level(stock,false,false));
    assert(borrowed.reconcile([&](const ActionPlan& p){return bx(p);}).execution==Execution::NotExecuted);
    assert(bx.plans.size()==1&&!borrowed.snapshot().portal_ready&&!borrowed.snapshot().native_idle);

    DemandArbiter use(7,12,gate.callbacks());assert(use.register_caller(portal));
    auto use_token=use.portal_on(portal);unsigned use_plans=0;
    use.reconcile([&](const ActionPlan& p){assert(p.action==Action::Connect);++use_plans;
        auto receipt=native(1);receipt.use=UseDisposition::AuthenticatedBorrowedUse;return receipt;});
    assert(use.frame_admissible(use_token) && !use.queues()[0].connect.observed);
    assert(use.portal_off(use_token));
    assert(!use.snapshot().native_idle && !use.snapshot().integration_obligations);
    use_token=use.portal_on(portal);
    assert(!use.frame_admissible(use_token)); // old borrowed USE is not a new On receipt
    use.reconcile([&](const ActionPlan& p){assert(p.action==Action::Connect);++use_plans;
        auto receipt=native(1);receipt.use=UseDisposition::NativeConnection;
        for(unsigned i=0;i<2;++i)receipt.queues[i]={true,true,0,QueueState::Connected,Ownership::NativeConnection,101+i,0};
        return receipt;});
    assert(use_plans==2 && use.frame_admissible(use_token));
    assert(use.portal_off(use_token));
    use.reconcile([&](const ActionPlan& p){assert(p.action==Action::Disconnect);++use_plans;
        auto receipt=native(1);receipt.use=UseDisposition::NativeConnection;
        for(unsigned i=0;i<2;++i)receipt.queues[i]={true,true,0,QueueState::Disconnected,Ownership::NativeConnection,p.queues[i].id,0};
        return receipt;});
    assert(use_plans==3 && !use.snapshot().fault && !use.snapshot().native_idle && !use.snapshot().integration_obligations);

    DemandArbiter closing(7,11,gate.callbacks());NativeExecutor cx;
    assert(closing.register_caller(portal));auto ct=closing.portal_on(portal);
    closing.reconcile([&](const ActionPlan& p){return cx(p);});
    SettlementProof settle;auto debt=closing.commit(ct,{&settle,SettlementProof::settled});assert(debt);
    closing.close();assert(!closing.frame_admissible(ct)&&closing.snapshot().producer_demand);
    assert(!closing.commit(ct,{&settle,SettlementProof::settled}));
    assert(closing.reconcile([&](const ActionPlan& p){return cx(p);}).execution==Execution::NotExecuted);
    settle.cpu=settle.gpu=settle.source=true;assert(closing.poll_settlement(debt));
    cx.lie_release=true;auto release=closing.reconcile([&](const ActionPlan& p){return cx(p);});
    assert(release.execution==Execution::Executed&&release.value==22);
    assert(closing.snapshot().fault&&!closing.snapshot().native_idle);
    assert(closing.queues()[0].id==101); // successful public bool cannot erase old IDs
    closing.reconcile([&](const ActionPlan& p){return cx(p);});assert(cx.plans.size()==2);
}
void demand_restart_and_contradictory_receipt() {
    Gate gate;DemandArbiter d(7,9,gate.callbacks());NativeExecutor ex;
    assert(d.register_caller(stock)&&d.set_level(stock,true,true));
    d.reconcile([&](const ActionPlan& p){return ex(p);});
    assert(d.set_level(stock,false,false));d.reconcile([&](const ActionPlan& p){return ex(p);});
    assert(d.set_level(stock,true,true));
    // Consumer reattach is supported only after complete detach, and the executor
    // must refuse BEFORE executing if producer/provider/alias/header proof is absent.
    auto retry=d.reconcile([&](const ActionPlan& p){assert(p.reattach);return NativeReceipt{};});
    assert(retry.execution==Execution::NotExecuted&&ex.plans.size()==2);
    assert(d.snapshot().producer_demand&&d.queues()[0].id==101);
    retry=d.reconcile([&](const ActionPlan& p){auto r=ex(p);r.producer_revalidated=true;return r;});
    assert(retry.execution==Execution::Executed&&d.snapshot().portal_ready&&ex.plans.size()==3);
    // An executor that executes without the required proof cannot admit frames.
    assert(d.set_level(stock,false,false));d.reconcile([&](const ActionPlan& p){return ex(p);});
    assert(d.set_level(stock,true,true));d.reconcile([&](const ActionPlan& p){return ex(p);});
    assert(d.snapshot().fault&&!d.snapshot().portal_ready);
    DemandArbiter bad(7,10,gate.callbacks());
    assert(bad.register_caller(stock)&&bad.set_level(stock,true,true));
    bad.reconcile([](const ActionPlan&){auto r=native(1);r.execution=Execution::NotExecuted;return r;});
    assert(bad.snapshot().fault&&!bad.snapshot().native_idle);
    assert(bad.queues()[0].state==QueueState::Unknown);
}
void demand_null_and_bounds() {
    DemandArbiter absent(7,9,{});NativeExecutor ex;
    assert(absent.register_caller(stock)&&absent.set_level(stock,true,true));
    assert(absent.reconcile([&](const ActionPlan& p){return ex(p);}).execution==Execution::NotExecuted);
    assert(ex.plans.empty());
    Gate gate;DemandArbiter d(7,10,gate.callbacks());
    for(uint64_t i=1;i<=DemandArbiter::MaxCallers;++i)
        assert(d.register_caller({CallerKind::Portal,i,1}));
    assert(!d.register_caller({CallerKind::Portal,DemandArbiter::MaxCallers+1,1}));
    auto token=d.portal_on({CallerKind::Portal,1,1});
    d.reconcile([&](const ActionPlan& p){return ex(p);});
    SettlementProof settled;std::vector<uint64_t> debts;
    for(size_t i=0;i<DemandArbiter::MaxDebts;++i) {
        auto debt=d.commit(token,{&settled,SettlementProof::settled});assert(debt);debts.push_back(debt);
    }
    assert(!d.commit(token,{&settled,SettlementProof::settled}));
    assert(gate.size()==DemandArbiter::MaxDebts&&d.snapshot().committed_debts==DemandArbiter::MaxDebts);
    assert(d.portal_off(token));settled.cpu=settled.gpu=settled.source=true;
    for(auto debt:debts)assert(d.poll_settlement(debt));
    uint64_t prior=token.id;
    for(size_t i=0;i<1024;++i) {
        auto current=d.portal_on({CallerKind::Portal,1,1});assert(current.id>prior);prior=current.id;
        auto debt=d.commit(current,{&settled,SettlementProof::settled});assert(debt);
        assert(d.portal_off(current)&&d.poll_settlement(debt));
    }
    assert(gate.size()==0&&!d.snapshot().committed_debts);
    assert(!d.frame_admissible(token)&&!d.poll_settlement(debts.front()));
    // Mixed borrowed/new results never schedule release of borrowed membership.
    DemandArbiter mixed(7,11,gate.callbacks());
    assert(mixed.register_caller(stock)&&mixed.set_level(stock,true,true));
    mixed.reconcile([&](const ActionPlan& p){auto r=ex(p);r.queues[0].ownership=Ownership::Borrowed;return r;});
    assert(mixed.set_level(stock,false,false));
    auto receipt=mixed.reconcile([&](const ActionPlan& p){assert(!p.selected[0]&&p.selected[1]);return ex(p);});
    assert(receipt.execution==Execution::Executed);
    assert(mixed.queues()[0].state==QueueState::Connected&&mixed.queues()[1].state==QueueState::Disconnected);
}
void combined_threaded_positive() {
    Gate gate;Signal ready,run,done;OwnerCoordinator* q=nullptr;DemandArbiter* d=nullptr;
    NativeExecutor ex;SettlementProof settlement;PortalToken token;uint64_t debt=0;
    std::thread render([&]{OwnerCoordinator queue(7,9,8,gate.callbacks());DemandArbiter demand(7,9,gate.callbacks());
        q=&queue;d=&demand;ready.set();run.wait();assert(queue.pump()==5);done.wait();});
    ready.wait();
    auto a=q->submit(7,9,[&]{assert(d->register_caller(portal));token=d->portal_on(portal);return d->reconcile([&](const ActionPlan& p){return ex(p);});});
    auto b=q->submit(7,9,[&]{assert(d->register_caller(stock));assert(d->set_level(stock,true,true));return d->reconcile([&](const ActionPlan& p){return ex(p);});});
    auto c=q->submit(7,9,[&]{assert(d->register_caller(foreign));assert(d->set_level(foreign,true,true));assert(d->portal_on(portal).id==token.id);debt=d->commit(token,{&settlement,SettlementProof::settled});assert(debt);return d->reconcile([&](const ActionPlan& p){return ex(p);});});
    auto off=q->submit(7,9,[&]{assert(d->portal_off(token));assert(d->set_level(stock,false,false));assert(d->connection_lost(foreign));assert(d->snapshot().producer_demand&&!d->snapshot().stock_full_display);return d->reconcile([&](const ActionPlan& p){return ex(p);});});
    auto settle=q->submit(7,9,[&]{settlement.cpu=settlement.gpu=settlement.source=true;assert(d->poll_settlement(debt));return d->reconcile([&](const ActionPlan& p){return ex(p);});});
    run.set();Result result;
    assert(q->wait(settle.id,result)==Wait::Ready&&result.native.value==22);
    for(auto id:{a.id,b.id,c.id,off.id,settle.id})assert(q->take(id,result));
    assert(ex.plans.size()==2&&gate.size()==0);done.set();render.join();
    std::cout<<"PASS modeled threaded positive: native_plans="<<ex.plans.size()
             <<" issued_gate_tickets="<<gate.next<<" live_gate_tickets="<<gate.size()<<'\n';
}
// Reproduce result consumption racing the return of a successful release retry.
// The slot may be reclaimed immediately; the retry must return its own outcome.
void queue_retry_take_race() {
    Gate gate;OwnerCoordinator q(7,9,1,gate.callbacks());
    std::atomic<uint64_t> published{0},taken{0};
    std::atomic<bool> done{false};
    std::thread consumer([&]{
        uint64_t last=0;
        while(!done.load()) {
            const auto id=published.load();
            if(id==last){std::this_thread::yield();continue;}
            Result r;
            while(!q.take(id,r))std::this_thread::yield();
            assert(r.retention_released && r.native.value_valid && r.native.value==123);
            last=id;taken.store(id);
        }
    });
    unsigned false_after_success=0;
    for(unsigned i=0;i<20000;++i) {
        gate.fail_release=true;
        const auto s=q.submit(7,9,[]{return native(123);});assert(s.id);
        Result r;assert(q.wait(s.id,r)==Wait::Ready && !r.retention_released);
        published=s.id;gate.fail_release=false;
        if(!q.retry_release(s.id))++false_after_success;
        while(taken.load()!=s.id)std::this_thread::yield();
    }
    done=true;consumer.join();
    assert(q.drained() && gate.size()==0 && gate.next==20000);
    std::cout<<"retry_release false after successful release: "<<false_after_success<<std::endl;
    assert(false_after_success==0);
}
int main() {
    queue_retry_take_race();
    rejected_capture_reentrancy();
    queue_fifo_and_retention();queue_cancel_close_reentrancy();queue_cancel_execution_race();queue_retaining_fifo_and_close();queue_gate_failure();
    demand_interleaving_and_debt();demand_failures_borrow_and_close();demand_restart_and_contradictory_receipt();demand_null_and_bounds();combined_threaded_positive();
    std::cout<<"PASS owner coordinator: FIFO, actual results, gate retention, cancellation race, reentrancy, full/stale/close; demand interleaving, borrowed/partial outcomes, CPU/GPU/source debt; real-thread positive chain\n";
}
