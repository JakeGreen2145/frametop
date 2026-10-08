#include "frame.hpp"
#include <cassert>
#include <thread>
using namespace native_portal;
int main() {
    Backend b; BackendStatus s; s.capable=s.serialized=s.external_clear=true; s.origin=4; assert(b.publish(s));
    Lease l; l.epoch=8; l.origin=4; l.generation=2; l.deadline_ns=100; l.flags=1; b.publish_lease(l);
    FrameGate gate(b); PolicyInput p; p.enabled=true;
    auto f=gate.begin(1,p,true,true,false,10); assert(f.policy.portal && f.lease.epoch==8);
    assert(gate.eye(f,0,4)); assert(!gate.accept(f,11)); // early backend invalidates whole stereo frame
    assert(!gate.eye(f,1,4));
    f=gate.begin(2,p,true,true,false,12); assert(gate.eye(f,0,4)); assert(gate.eye(f,1,4)); assert(gate.accept(f,13));
    assert(gate.submitted(f,13)); BackendStatus observed; assert(b.status(observed)&&observed.submitted_frame==2&&observed.submitted_epoch==8);
    b.publish_lease(Lease{}); assert(!gate.accept(f,14));
    l.epoch=9; b.publish_lease(l); assert(!gate.accept(f,14));
    f=gate.begin(3,p,true,true,false,15); assert(gate.eye(f,0,4)); assert(!gate.eye(f,1,5)); assert(!gate.accept(f,16));
    f=gate.begin(4,p,true,true,false,17); assert(gate.eye(f,0,4)); assert(gate.eye(f,1,4)); assert(!gate.accept(f,100));
    p.request=true; f=gate.begin(5,p,true,true,true,18); assert(!f.policy.portal); assert(f.policy.effective.construct==f.policy.stock.construct);
    p.request=false; f=gate.begin(6,p,false,true,false,19); assert(!f.policy.portal);
    // Off-thread callbacks are explicitly refused and latch the bridge unusable.
    f=gate.begin(7,p,true,true,false,20); std::thread t([&]{assert(!gate.eye(f,0,4));}); t.join(); assert(!gate.eye(f,1,4));
    assert(!b.lease(l,21));
}
