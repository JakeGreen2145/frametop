// SPDX-License-Identifier: MIT
#include "camera.hpp"
#include <atomic>
#include <cassert>
#include <future>
#include <iostream>
using namespace native_portal;
using namespace std::chrono_literals;
using Dispatch=binding::CameraNativeBinding::Dispatch;

static CameraBinding identities() {return {0x1000,0x200000,0x3000,0x4000,0x5000,0x6000,1,1,1};}
static arena::Saved publication() {
    arena::Saved s{};s.x[19]=identities().application;s.x[20]=s.x[19]+0x20000;
    s.x[26]=identities().camera;s.x[0]=0xbad;s.sp=0x80000;return s;
}
static void count(void* p) noexcept {++*static_cast<std::atomic<unsigned>*>(p);}
static void publication_and_unrelated_cleanup() {
    CameraAccessGate gate;binding::CameraNativeBinding native(gate);uint64_t e=42;
    auto s=publication();
    assert(!native.publish_completed(0x15ddc8,s,identities(),std::this_thread::get_id(),e)&&e==0);
    assert(!native.publish_completed(0x8dc18,s,identities(),std::this_thread::get_id(),e));
    auto bad=s;bad.x[26]=s.x[0];
    assert(!native.publish_completed(0x8dc1c,bad,identities(),std::this_thread::get_id(),e));
    bad=s;bad.x[20]++;
    assert(!native.publish_completed(0x8dc1c,bad,identities(),std::this_thread::get_id(),e));
    assert(native.publish_completed(0x8dc1c,s,identities(),std::this_thread::get_id(),e)&&e);
    uint64_t ticket=0;assert(gate.retain(e,ticket));CameraBinding b{};
    assert(gate.lookup(ticket,b)&&b.camera==s.x[26]&&b.camera!=s.x[0]);
    std::atomic<unsigned> calls{0};s.x[0]=b.camera+8;
    assert(native.cleanup(0x15df70,s,count,&calls,0ms)==Dispatch::ForwardedUnrelated&&calls==1);
    assert(gate.lookup(ticket,b));assert(gate.release(ticket));s.x[0]=b.camera;
    assert(native.cleanup(0x15df70,s,count,&calls,0ms)==Dispatch::Completed&&calls==2);
    uint64_t next=0;
    assert(native.publish_completed(0x8dc1c,publication(),identities(),std::this_thread::get_id(),next)&&next>e);
    assert(!gate.release(ticket));assert(!gate.retain(e,ticket));
    assert(native.cleanup(0x15df70,s,count,&calls,0ms)==Dispatch::Completed);
}
static void exact_x0_precleanup_waits_and_refuses() {
    CameraAccessGate gate;binding::CameraNativeBinding native(gate);uint64_t e=0,ticket=0;
    auto s=publication();assert(native.publish_completed(0x8dc1c,s,identities(),std::this_thread::get_id(),e));
    assert(gate.retain(e,ticket));s.x[0]=identities().camera;
    std::atomic<unsigned> calls{0};
    assert(native.cleanup(0x15e1c8,s,count,&calls,1s)==Dispatch::Refused&&calls==0);
    assert(s.pad==arena::Unsupported); // refusal cannot reach destructive continuation
    uint64_t rejected=0;assert(!gate.retain(e,rejected));
    auto worker=std::async(std::launch::async,[&]{return native.cleanup(0x15e1c8,s,count,&calls,2s);});
    assert(worker.wait_for(20ms)==std::future_status::timeout&&calls==0);
    uint64_t next=0;
    assert(!native.publish_completed(0x8dc1c,publication(),identities(),std::this_thread::get_id(),next));
    assert(gate.release(ticket));assert(worker.get()==Dispatch::Completed&&calls==1);
    assert(!gate.retain(e,rejected));
    assert(native.cleanup(0x15df70,s,nullptr,nullptr,0ms)==Dispatch::Refused&&calls==1);
    assert(native.cleanup(0x15df74,s,count,&calls,0ms)==Dispatch::Refused&&calls==1);
    assert(native.cleanup(0x15df70,s,count,&calls,0ms)==Dispatch::Completed&&calls==2);
}
static void publication_cannot_overtake_unbound_cleanup() {
    CameraAccessGate gate;binding::CameraNativeBinding native(gate);
    std::promise<void> entered,leave;auto exit=leave.get_future();
    struct Args {std::promise<void>* entered;std::future<void>* leave;} args{&entered,&exit};
    auto s=publication();s.x[0]=identities().camera;
    auto cleanup=std::async(std::launch::async,[&]{
        return native.cleanup(0x15df70,s,[](void* p) noexcept {
            auto& a=*static_cast<Args*>(p);a.entered->set_value();a.leave->wait();
        },&args,0ms);
    });
    entered.get_future().wait();uint64_t e=0;
    const bool admitted=native.publish_completed(0x8dc1c,publication(),identities(),std::this_thread::get_id(),e);
    leave.set_value();assert(cleanup.get()==Dispatch::ForwardedUnrelated);
    assert(!admitted); // cannot publish a camera while original destruction runs
}
static void publication_precedes_backend_binding() {
    CameraAccessGate gate;binding::CameraNativeBinding native(gate);auto s=publication();uint64_t e=42;
    assert(!native.observe_publication(0x15ddc8,s));
    assert(native.observe_publication(0x8dc1c,s));
    assert(!gate.retain(1,e)&&e==0); // pending publication grants no usable epoch
    std::atomic<unsigned> calls{0};s.x[0]=identities().camera;
    assert(native.cleanup(0x15e1c8,s,count,&calls,0ms)==Dispatch::ForwardedUnrelated&&calls==1);
    assert(!native.bind(identities(),std::this_thread::get_id(),e)&&e==0);
    assert(native.observe_publication(0x8dc1c,publication()));
    auto bad=identities();bad.camera++;
    assert(!native.bind(bad,std::this_thread::get_id(),e));
    assert(native.bind(identities(),std::this_thread::get_id(),e)&&e);
    assert(native.cleanup(0x15df70,s,count,&calls,0ms)==Dispatch::Completed);
}
int main() {
    publication_precedes_backend_binding();
    publication_and_unrelated_cleanup();exact_x0_precleanup_waits_and_refuses();
    publication_cannot_overtake_unbound_cleanup();
    std::cout<<"native camera binding: completed publication, exact x0, interlocked cleanup, ABA passed\n";
}
