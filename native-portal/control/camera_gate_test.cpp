// SPDX-License-Identifier: MIT
#include "camera_gate.hpp"
#include <atomic>
#include <cassert>
#include <future>
#include <iostream>
#include <thread>
using namespace native_portal;
using namespace std::chrono_literals;
using Result = CameraAccessGate::CleanupResult;
using Kind = CameraAccessGate::CleanupKind;

static CameraBinding binding() {
    return {0x1000,0x2000,0x3000,0x4000,0x5000,0x6000,1,1,1};
}
static void mark(void* p) noexcept {++*static_cast<std::atomic<unsigned>*>(p);}
static uint64_t publish(CameraAccessGate& g) {
    uint64_t epoch=0;
    assert(g.publish(binding(),std::this_thread::get_id(),epoch));
    assert(epoch);return epoch;
}
static void destroy(CameraAccessGate& g,uint64_t e) {
    std::atomic<unsigned> calls{0};
    assert(g.close_and_cleanup(e,Kind::Destroy,mark,&calls,100ms)==Result::Completed);
    assert(calls==1);
}
static void ticket_before_lookup_and_aba() {
    CameraAccessGate g;CameraBinding out{};uint64_t ticket=123;
    assert(!g.lookup(0,out));assert(!g.retain(0,ticket)&&ticket==0);
    auto e=publish(g);
    assert(!g.lookup(e,out)); // epochs do not confer a pointer borrow
    assert(g.retain(e,ticket));assert(ticket);
    assert(g.lookup(ticket,out)&&out==binding());
    assert(g.begin_close(e));
    uint64_t refused=123;
    assert(!g.retain(e,refused)&&!refused);
    assert(g.lookup(ticket,out)&&out==binding());
    uint64_t replacement=123;
    assert(!g.publish(binding(),std::this_thread::get_id(),replacement)&&!replacement);
    assert(!g.quiescent(e));assert(!g.release(ticket+1));
    assert(g.release(ticket));assert(!g.release(ticket));assert(!g.lookup(ticket,out));
    assert(g.quiescent(e));
    // No replacement even when empty until native destruction actually returns.
    assert(!g.publish(binding(),std::this_thread::get_id(),replacement));
    destroy(g,e);assert(!g.quiescent(e));
    auto next=publish(g);assert(next>e);uint64_t t=0;
    assert(g.retain(next,t)&&t>ticket);assert(!g.retain(e,refused));
    assert(!g.release(ticket));assert(!g.begin_close(e));
    assert(g.release(t));destroy(g,next);
}
static void cpu_and_transaction_block_cleanup() {
    CameraAccessGate g;auto e=publish(g);uint64_t cpu=0,transaction=0;
    assert(g.retain(e,cpu)&&g.retain(e,transaction));
    std::atomic<unsigned> destroyed{0};std::promise<void> entered;
    auto done=std::async(std::launch::async,[&]{
        assert(g.begin_close(e));entered.set_value();
        return g.close_and_cleanup(e,Kind::Destroy,mark,&destroyed,2s);
    });
    entered.get_future().wait();
    CameraBinding original{};
    assert(g.lookup(cpu,original)&&original==binding());
    assert(done.wait_for(20ms)==std::future_status::timeout&&destroyed==0);
    assert(g.release(cpu));
    assert(done.wait_for(20ms)==std::future_status::timeout&&destroyed==0);
    assert(g.release(transaction));assert(done.get()==Result::Completed&&destroyed==1);
}
static void failure_owner_and_cancel() {
    CameraAccessGate g;auto e=publish(g);uint64_t task=0;
    assert(g.retain(e,task));std::atomic<unsigned> calls{0};
    assert(g.close_and_cleanup(e,Kind::Destroy,mark,&calls,1s)==Result::WouldDeadlock);
    assert(calls==0);uint64_t t=9;assert(!g.retain(e,t)&&!t);
    auto timed=std::async(std::launch::async,[&]{
        return g.close_and_cleanup(e,Kind::Destroy,mark,&calls,10ms);
    });
    assert(timed.get()==Result::Timeout&&calls==0);
    assert(g.close_and_cleanup(e,Kind::Destroy,nullptr,nullptr,0ms)==Result::Invalid);
    assert(g.release(task)); // a cancelled queued task, never entered native code
    destroy(g,e);
}
static void foreign_native_call_and_unlocked_callback() {
    CameraAccessGate g;auto e=publish(g);std::promise<void> entered,leave;
    auto exit=leave.get_future();std::atomic<unsigned> native_calls{0},cleanups{0};
    auto foreign=std::async(std::launch::async,[&]{
        uint64_t t=0;CameraBinding b{};
        assert(g.retain(e,t));assert(g.lookup(t,b)&&b==binding());
        ++native_calls;entered.set_value();exit.wait(); // native/IPC outside gate lock
        assert(g.lookup(t,b));assert(g.release(t));return 71; // preserve actual result
    });
    entered.get_future().wait();
    struct Args {CameraAccessGate* gate;uint64_t epoch;std::atomic<unsigned>* calls;} args{&g,e,&cleanups};
    auto cleanup=std::async(std::launch::async,[&]{
        return g.close_and_cleanup(e,Kind::Resources,[](void* p) noexcept {
            auto& a=*static_cast<Args*>(p);
            assert(a.gate->quiescent(a.epoch)); // would deadlock if mutex spanned callback
            uint64_t t=0;assert(!a.gate->retain(a.epoch,t));++*a.calls;
        },&args,2s);
    });
    assert(cleanup.wait_for(20ms)==std::future_status::timeout&&cleanups==0);
    leave.set_value();assert(foreign.get()==71&&native_calls==1);
    assert(cleanup.get()==Result::Completed&&cleanups==1);
    destroy(g,e);
}
static void bounded_reuse() {
    CameraAccessGate g;auto e=publish(g);
    std::array<uint64_t,CameraAccessGate::capacity> held{};
    for(auto& t:held)assert(g.retain(e,t));
    uint64_t t=123;assert(!g.retain(e,t)&&!t);
    for(auto id:held)assert(g.release(id));
    uint64_t last=held.back();
    for(unsigned i=0;i<4096;++i) {
        assert(g.retain(e,t)&&t>last);assert(!g.release(last));
        assert(g.release(t));last=t;
    }
    destroy(g,e);
}
static void cleanup_inflight_excludes_republication() {
    CameraAccessGate g;auto e=publish(g);std::promise<void> entered,leave;
    auto exit=leave.get_future();
    struct Args {std::promise<void>* entered;std::future<void>* leave;} args{&entered,&exit};
    auto a=std::async(std::launch::async,[&]{
        return g.close_and_cleanup(e,Kind::Destroy,[](void* p) noexcept {
            auto& x=*static_cast<Args*>(p);x.entered->set_value();x.leave->wait();
        },&args,2s);
    });
    entered.get_future().wait();uint64_t replacement=0;
    assert(!g.publish(binding(),std::this_thread::get_id(),replacement));
    std::atomic<unsigned> calls{0};
    assert(g.close_and_cleanup(e,Kind::Destroy,mark,&calls,1ms)==Result::WouldDeadlock);
    auto b=std::async(std::launch::async,[&]{
        return g.close_and_cleanup(e,Kind::Destroy,mark,&calls,1ms);
    });
    assert(b.get()==Result::Timeout&&calls==0);leave.set_value();
    assert(a.get()==Result::Completed);assert(!g.quiescent(e));
    assert(g.close_and_cleanup(e,Kind::Destroy,mark,&calls,0ms)==Result::Stale);
}
int main() {
    ticket_before_lookup_and_aba();cpu_and_transaction_block_cleanup();
    failure_owner_and_cancel();foreign_native_call_and_unlocked_callback();
    bounded_reuse();cleanup_inflight_excludes_republication();
    std::cout<<"camera gate: 6 real-thread/bounded-lifetime cases passed\n";
}
