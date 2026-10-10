// SPDX-License-Identifier: MIT
// Native userdata/service calls and source-return boundary are CPU fakes.
// Adapter, Command, Lifecycle, camera gate/binding, queue and arbiter are REAL.
#include "camera_owner.hpp"
#include "combined_fixture.hpp"
#include <future>
#include <sys/wait.h>
using namespace native_portal;
namespace co = native_portal::coordination;
namespace live = native_portal::live;
using namespace std::chrono_literals;
using Dispatch = binding::CameraNativeBinding::Dispatch;

struct ServiceCall { unsigned kind; uint64_t id; int64_t result; };
struct Transition {
    uint64_t pc;
    std::array<uint64_t,5> before,after;
    int64_t result;
    size_t calls;
    std::array<ServiceCall,2> service;
};
#include "camera_idle_fixture.hpp"

// Test-only receipt adapter consumes the pinned witness. Native output does NOT
// prove service ownership or header generations: those are explicit fixture inputs.
struct ReceiptShim {
    std::array<co::Ownership,2> ownership;
    std::array<uint64_t,2> header_generation;
    unsigned native_calls=0;
    co::NativeReceipt consume(const co::ActionPlan& p,const Transition& t,bool producer_revalidated=false) {
        if(p.reattach && !producer_revalidated)return {}; // no native execution
        ++native_calls;
        co::NativeReceipt r;r.execution=co::Execution::Executed;r.value_valid=true;r.value=t.result;
        r.producer_revalidated=producer_revalidated;
        const bool connect=p.action==co::Action::Connect;
        assert(t.pc==(connect?0x15f9e0:0x15fba8));
        for(unsigned i=0;i<2;++i) {
            auto& q=r.queues[i];q.id=t.after[3+i];q.header_generation=header_generation[i];
            q.ownership=ownership[i];
            const bool called=t.calls==2 && t.service[i].kind==(connect?1u:2u) && t.service[i].id==q.id;
            // Keep the exact service result, NOT the aggregate method boolean.
            q.observed=called;q.value_valid=called;q.value=t.service[i].result;
            if(!called || q.value!=0)q.ownership=co::Ownership::Unknown;
            if(called && q.value==0 && t.result==1 && connect && t.after[1]==1 && t.after[2]==1)q.state=co::QueueState::Connected;
            if(called && q.value==0 && t.result==1 && !connect && t.before[2]==1 && t.after[1]==0 && t.after[2]==0)
                q.state=co::QueueState::Disconnected;
        }
        return r;
    }
};
static const Transition& transition(uint64_t pc,bool calls,int64_t result,size_t occurrence=0) {
    for(const auto& t:transitions)if(t.pc==pc && bool(t.calls)==calls && t.result==result) {
        if(!occurrence--)return t;
    }
    std::abort();
}
static CameraBinding camera_id(Combined& f) {
    const auto& i=f.n.binding.identity;
    return {0x1000,0x200000,f.n.addr(f.n.camera),i.renderer,i.device,0x6000,
            i.renderer_generation,i.device_generation,1};
}
static arena::Saved published(const CameraBinding& b) {
    arena::Saved s{};s.sp=0x80000;s.x[19]=b.application;s.x[20]=b.application+0x20000;s.x[26]=b.camera;return s;
}
static void publication(live::CameraOwner& owner,const CameraBinding& b) {
    assert(!owner.observe_publication(0x15ddc8,published(b)));
    assert(owner.observe_publication(0x8dc1c,published(b)));
    assert(owner.bind(b,7,8));
    assert(owner.epoch()!=0 && !owner.coverage().supported && owner.coverage().installed_boundaries==0);
}
static co::Submission listener(live::CameraOwner& owner,live::CameraOwner::NativeMutation fn) {
    return std::async(std::launch::async,[&]{return owner.submit(owner.epoch(),7,std::move(fn));}).get();
}
static co::NativeReceipt result(live::CameraOwner& owner,co::Submission s) {
    assert(s.id && s.rejection==co::Rejection::None);
    co::Result r;assert(owner.wait(s.id,r)==co::Wait::OwnerWouldBlock);
    assert(owner.pump()==1);assert(owner.take(s.id,r) && r.retention_released);return r.native;
}
static void counted_cleanup(void* p) noexcept {++*static_cast<unsigned*>(p);}
static Dispatch cleanup(live::CameraOwner& owner,const CameraBinding& b,unsigned& count) {
    auto s=published(b);s.x[0]=b.camera;
    auto d=owner.cleanup(0x15df70,s,counted_cleanup,&count,0ms);
    assert(s.pad==(d==Dispatch::Refused?arena::Unsupported:arena::SkipOwned));return d;
}
struct SourceReturn {
    uint64_t watermark=0;
    bool returned=false;
    static bool settled(void* p,uint64_t watermark) noexcept {
        const auto& self=*static_cast<SourceReturn*>(p);
        return self.returned && self.watermark==watermark;
    }
};

static void positive(unsigned missing) {
    Combined f;live::CameraOwner owner;const auto b=camera_id(f);publication(owner,b);
    const auto owner_thread=std::this_thread::get_id();
    assert(!owner.submit(owner.epoch()+1,7,[](const auto&,auto&){assert(false);return co::NativeReceipt{};}).id);
    assert(!owner.submit(owner.epoch(),8,[](const auto&,auto&){assert(false);return co::NativeReceipt{};}).id);
    ReceiptShim native{{co::Ownership::NewlyOwned,co::Ownership::NewlyOwned},{41,42}};
    co::Caller portal{co::CallerKind::Portal,10,1},stock{co::CallerKind::Stock,20,1},foreign{co::CallerKind::Foreign,30,1};
    co::PortalToken token;uint64_t debt=0;unsigned calls=0;
    SourceReturn source;
    std::optional<live::RenderDebt> settlement;
    auto s=listener(owner,[&](const CameraBinding& held,co::DemandArbiter& demand) {
        assert(held==b && std::this_thread::get_id()==owner_thread);
        assert(demand.register_caller(portal) && demand.register_caller(stock) && demand.register_caller(foreign));
        assert(!demand.set_level({co::CallerKind::Stock,20,2},true,true));
        token=demand.portal_on(portal);assert(token.id && !demand.snapshot().stock_full_display);
        assert(demand.set_level(stock,true,true) && demand.set_level(foreign,true,false));
        assert(demand.snapshot().stock_full_display);
        return demand.reconcile([&](const co::ActionPlan& p) {return native.consume(p,transition(0x15f9e0,true,1));});
    });
    assert(result(owner,s).value==1);
    result(owner,listener(owner,[&](const CameraBinding& held,co::DemandArbiter& demand) {
        assert(held==b && demand.frame_admissible(token));
        f.start();f.record();source.watermark=f.adapter.state().tickets_issued;
        settlement.emplace(f.adapter,source.watermark,live::SourceSettlement{&source,SourceReturn::settled});
        debt=demand.commit(token,settlement->callback());assert(debt);
        assert(f.prepare());f.draw(0);f.draw(1);f.n.queue();assert(f.adapter.submitted());
        assert(demand.portal_off(token));f.adapter.off();
        assert(demand.set_level(stock,false,false) && demand.set_level(foreign,false,false));
        assert(demand.snapshot().producer_demand && !demand.frame_admissible(token));
        assert(!demand.poll_settlement(debt));
        return co::NativeReceipt{};
    }));
    // The cleanup boundary itself closes real gate admission, before owner drain.
    assert(cleanup(owner,b,calls)==Dispatch::Refused && calls==0);
    assert(owner.begin_close());
    assert(!owner.submit(owner.epoch(),7,[](const auto&,auto&){return co::NativeReceipt{};}).id);
    bool settled=false;
    owner.drain([&](const CameraBinding& held,co::DemandArbiter& demand) {
        assert(held==b);
        if(missing!=0)f.n.wait();
        if(missing!=1)f.n.dispose();
        if(missing!=0 && missing!=1)f.n.reset_command();
        source.returned=missing!=2;
        assert(!demand.poll_settlement(debt)); // each independent missing boundary retains gate debt
        assert(demand.snapshot().committed_debts==1);
        return co::NativeReceipt{};
    });
    assert(!owner.finish_close() && cleanup(owner,b,calls)==Dispatch::Refused && calls==0);
    owner.drain([&](const CameraBinding& held,co::DemandArbiter& demand) {
        assert(held==b);
        if(missing==0) {f.n.wait();f.n.reset_command();}
        if(missing==1) {f.n.dispose();f.n.reset_command();}
        source.returned=true;
        settled=demand.poll_settlement(debt);assert(settled);
        assert(!f.adapter.state().pending && f.adapter.state().cpu_settled && f.adapter.state().gpu_complete);
        live::RenderDebt unbound(f.adapter,source.watermark);
        auto absent=unbound.callback();assert(!absent.settled(absent.context,99));
        live::RenderDebt stale(f.adapter,source.watermark+1,{&source,SourceReturn::settled});
        auto wrong=stale.callback();assert(!wrong.settled(wrong.context,99));
        // CPU/GPU/source settlement is not native disconnect. Only the complete
        // actual native branch with BOTH service calls establishes warm idle.
        const auto r=demand.reconcile([&](const co::ActionPlan& p) {
            return native.consume(p,transition(0x15fba8,true,1));
        });
        assert(r.value_valid && r.value==1 && demand.snapshot().native_idle);
        return r;
    });
    assert(settled && owner.finish_close());
    assert(cleanup(owner,b,calls)==Dispatch::Completed && calls==1);
    std::printf("camera-owner: missing %s retains real cleanup barrier; exact settlement releases once\n",
                missing==0?"GPU wait":missing==1?"CPU disposal/reset":"source return");
}

static void inflight_and_reentrant() {
    Combined f;live::CameraOwner owner;auto b=camera_id(f);publication(owner,b);
    ReceiptShim native{{co::Ownership::NewlyOwned,co::Ownership::NewlyOwned},{41,42}};
    unsigned calls=0;std::promise<void> entered,leave;auto released=leave.get_future();
    co::Submission nested;
    auto s=listener(owner,[&](const CameraBinding& held,co::DemandArbiter& demand) {
        assert(held==b);
        co::Caller c{co::CallerKind::Stock,1,1};assert(demand.register_caller(c) && demand.set_level(c,true,false));
        demand.reconcile([&](const auto& p){return native.consume(p,transition(0x15f9e0,true,1));});
        nested=owner.submit(owner.epoch(),7,[&](const auto&,auto&){assert(false);return co::NativeReceipt{};});
        co::Result ignored;assert(owner.wait(nested.id,ignored)==co::Wait::OwnerWouldBlock);
        assert(!owner.begin_close()); // no reentrant owner mutation
        entered.set_value();released.wait();
        assert(held==b);
        assert(demand.set_level(c,false,false));
        return co::NativeReceipt{co::Execution::Executed,true,-37,{}};
    });
    auto closer=std::async(std::launch::async,[&]{
        entered.get_future().wait();
        assert(cleanup(owner,b,calls)==Dispatch::Refused && calls==0);
        leave.set_value();
    });
    assert(owner.pump(1)==1);closer.get();co::Result r;
    assert(owner.take(s.id,r) && r.native.value==-37 && r.retention_released);
    assert(owner.begin_close());assert(owner.take(nested.id,r) && r.reason==co::Rejection::Closing);
    owner.drain([&](const auto&,auto& demand) {return demand.reconcile([&](const auto& p){return native.consume(p,transition(0x15fba8,true,1));});});
    assert(owner.finish_close() && cleanup(owner,b,calls)==Dispatch::Completed && calls==1);
}

static void real_ticket_release_quarantine() {
    CameraAccessGate gate;binding::CameraNativeBinding native(gate);
    CameraBinding b{1,0x200000,3,4,5,6,1,1,1};uint64_t epoch=0;
    assert(native.publish_completed(0x8dc1c,published(b),b,std::this_thread::get_id(),epoch));
    struct Fault {CameraAccessGate& gate;bool fail=true;} fault{gate};
    co::Retention retention{&fault,[](void* p,uint64_t e,uint64_t& t){return static_cast<Fault*>(p)->gate.retain(e,t);},
        [](void* p,uint64_t t){auto& f=*static_cast<Fault*>(p);return !f.fail && f.gate.release(t);}};
    co::OwnerCoordinator queue(epoch,1,2,retention);uint64_t seen=0;unsigned called=0,destroyed=0;
    assert(!queue.submit_scoped(epoch,2,[](uint64_t){assert(false);return co::NativeReceipt{};}).id);
    auto s=std::async(std::launch::async,[&]{return queue.submit_scoped(epoch,1,[&](uint64_t ticket) {
        CameraBinding held;assert(gate.lookup(ticket,held) && held==b);seen=ticket;++called;
        auto regs=published(b);regs.x[0]=b.camera;
        // No CameraOwner barrier in this supplemental seam probe: the running
        // native mutation's exact queue ticket alone prevents original cleanup.
        assert(native.cleanup(0x15df70,regs,counted_cleanup,&destroyed,0ms)==Dispatch::Refused && destroyed==0);
        return co::NativeReceipt{co::Execution::Executed,true,0,{}};
    });}).get();
    assert(gate.begin_close(epoch));uint64_t refused=0;assert(!gate.retain(epoch,refused));
    assert(queue.pump()==1);co::Result r;assert(queue.wait(s.id,r)==co::Wait::Ready && !r.retention_released);
    assert(r.native.execution==co::Execution::Executed && r.native.value_valid && r.native.value==0);
    assert(!queue.take(s.id,r) && !queue.drained());
    auto regs=published(b);regs.x[0]=b.camera;
    assert(native.cleanup(0x15df70,regs,counted_cleanup,&destroyed,0ms)==Dispatch::Refused);
    CameraBinding held;assert(gate.lookup(seen,held));fault.fail=false;
    assert(queue.retry_release(s.id) && queue.take(s.id,r) && called==1);
    assert(!gate.lookup(seen,held));
    assert(native.cleanup(0x15df70,regs,counted_cleanup,&destroyed,0ms)==Dispatch::Completed && destroyed==1);
}

static void negative_alias_and_fork() {
    // Deliberately unresolved native debt is quarantined, never force-released for
    // test teardown. Isolated child exits without invoking component destructors.
    for(bool partial:{false,true}) {
        auto child=fork();assert(child>=0);
        if(child==0) {
            Combined f;live::CameraOwner owner;const auto b=camera_id(f);publication(owner,b);
            ReceiptShim native{{co::Ownership::NewlyOwned,co::Ownership::NewlyOwned},{41,42}};
            co::PortalToken token;
            owner.submit(owner.epoch(),7,[&](const auto&,auto& demand) {
                co::Caller c{co::CallerKind::Portal,1,1};assert(demand.register_caller(c));token=demand.portal_on(c);
                demand.reconcile([&](const auto& p){return native.consume(p,transition(0x15f9e0,true,partial?0:1));});
                if(partial) {assert(!demand.frame_admissible(token) && demand.snapshot().fault);}
                else {assert(demand.frame_admissible(token));assert(demand.portal_off(token));
                    demand.reconcile([&](const auto& p){return native.consume(p,transition(0x15fba8,false,1));});
                    assert(!demand.snapshot().native_idle && demand.snapshot().fault);
                }
                return co::NativeReceipt{};
            });
            assert(owner.begin_close() && !owner.finish_close());unsigned calls=0;
            assert(cleanup(owner,b,calls)==Dispatch::Refused && calls==0);_exit(0);
        }
        int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
    Combined f;live::CameraOwner owner;auto b=camera_id(f);publication(owner,b);
    auto child=fork();assert(child>=0);
    if(child==0) {
        assert(!owner.submit(owner.epoch(),7,[](const auto&,auto&){assert(false);return co::NativeReceipt{};}).id);
        assert(!owner.begin_close() && owner.pump()==0);unsigned calls=0;
        assert(cleanup(owner,b,calls)==Dispatch::Refused && calls==0);_exit(0);
    }
    int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    // An attempted integration transition must actually detach before teardown.
    ReceiptShim native{{co::Ownership::NewlyOwned,co::Ownership::NewlyOwned},{41,42}};
    owner.submit(owner.epoch(),7,[&](const auto&,auto& demand) {
        co::Caller c{co::CallerKind::Stock,1,1};assert(demand.register_caller(c)&&demand.set_level(c,true,false));
        demand.reconcile([&](const auto& p){return native.consume(p,transition(0x15f9e0,true,1));});
        assert(demand.set_level(c,false,false));return co::NativeReceipt{};
    });
    assert(owner.begin_close());owner.drain([&](const auto&,auto& demand){return demand.reconcile([&](const auto& p){return native.consume(p,transition(0x15fba8,true,1));});});
    unsigned calls=0;assert(owner.finish_close() && cleanup(owner,b,calls)==Dispatch::Completed);
}
static void repeated_consumer_cycles() {
    Combined f;live::CameraOwner owner;const auto b=camera_id(f);publication(owner,b);
    const auto epoch=owner.epoch();uint64_t prior_token=0;
    ReceiptShim native{{co::Ownership::NewlyOwned,co::Ownership::NewlyOwned},{41,42}};
    co::Caller caller{co::CallerKind::Portal,1,1};
    for(unsigned cycle=1;cycle<=3;++cycle) {
        result(owner,listener(owner,[&](const auto& held,auto& demand) {
            assert(held==b && owner.epoch()==epoch && demand.register_caller(caller));
            auto token=demand.portal_on(caller);assert(token.id>prior_token);prior_token=token.id;
            if(cycle>1) {
                const auto calls=native.native_calls;
                auto unsupported=demand.reconcile([&](const auto& p) {
                    assert(p.reattach);return native.consume(p,transition(0x15f9e0,true,1,1));
                });
                assert(unsupported.execution==co::Execution::NotExecuted && native.native_calls==calls);
                assert(!demand.frame_admissible(token) && !demand.snapshot().fault);
            }
            const auto connected=demand.reconcile([&](const auto& p) {
                assert(p.reattach==(cycle>1));
                // Explicit fixture input: intact same producer/provider/alias and
                // exact per-queue header generations revalidated before execution.
                return native.consume(p,transition(0x15f9e0,true,1,cycle==1?0:1),true);
            });
            assert(connected.value_valid && connected.value==1 && demand.frame_admissible(token));
            if(cycle>1) {
                f.n.reset();f.n.attach();f.source.control.settled();
                ++f.source.lease.epoch;f.source.lease.revocation=f.source.control.revocation();
                assert(f.source.control.publish_lease(f.source.lease));
            }
            f.start(cycle);f.record();SourceReturn source{f.adapter.state().tickets_issued,false};
            live::RenderDebt debt(f.adapter,source.watermark,{&source,SourceReturn::settled});
            const auto id=demand.commit(token,debt.callback());assert(id);
            assert(f.prepare());f.draw(0);f.draw(1);f.n.queue();assert(f.adapter.submitted());
            assert(demand.portal_off(token));f.adapter.off();
            assert(!demand.portal_on(caller).id && !demand.poll_settlement(id));
            assert(demand.reconcile([&](const auto&){assert(false);return co::NativeReceipt{};}).execution==co::Execution::NotExecuted);
            f.n.wait();f.n.dispose();f.n.reset_command();source.returned=true;
            assert(demand.poll_settlement(id));
            auto disconnected=demand.reconcile([&](const auto& p){return native.consume(p,transition(0x15fba8,true,1));});
            assert(disconnected.value_valid && disconnected.value==1 && demand.snapshot().native_idle);
            return disconnected;
        }));
    }
    unsigned calls=0;assert(owner.begin_close() && owner.finish_close());
    assert(cleanup(owner,b,calls)==Dispatch::Completed && calls==1);
    std::puts("camera-owner: three On/commit/Off/settle/detach cycles share one camera and producer generation");
}
static void unused_cleanup() {
    Combined f;live::CameraOwner owner;const auto b=camera_id(f);publication(owner,b);
    const auto initial=owner.snapshot();
    assert(!initial.native_idle && !initial.portal_ready && !initial.integration_obligations);
    // Merely accepting intent or a retained call does not acquire a native queue.
    result(owner,listener(owner,[&](const auto&,auto& demand) {
        co::Caller c{co::CallerKind::Portal,1,1};assert(demand.register_caller(c));
        const auto token=demand.portal_on(c);assert(token.id && !demand.frame_admissible(token));
        return co::NativeReceipt{};
    }));
    assert(owner.begin_close() && owner.finish_close());
    const auto closed=owner.snapshot();
    assert(!closed.native_idle && !closed.portal_ready && !closed.integration_obligations);
    unsigned calls=0;assert(cleanup(owner,b,calls)==Dispatch::Completed && calls==1);
}
int main() {
    unused_cleanup();
    repeated_consumer_cycles();
    for(unsigned absent=0;absent<3;++absent)positive(absent);
    inflight_and_reentrant();real_ticket_release_quarantine();negative_alias_and_fork();
    std::puts("camera-owner: threaded lifetime/owner/demand composition, pinned warm receipts, reentrance, quarantine and fork passed");
}
