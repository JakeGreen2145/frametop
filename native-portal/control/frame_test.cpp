#include "frame.hpp"
#include <cassert>
#include <thread>
using namespace native_portal;
int main() {
    // Construct mode is native policy, not external ownership. Only an
    // authenticated normal publisher admits its owned-demand counterfactual.
    PolicyInput construct;construct.enabled=construct.special=true;
    assert(!frame_policy(construct,true,true,true,false).portal);
    construct.normal_construct=true;
    auto cf=frame_policy(construct,true,true,true,false);
    assert(cf.portal && !cf.stock.construct && !cf.stock.demand && cf.effective.construct && cf.effective.demand);
    assert(background_permission(false,true,false,cf.stock.alpha,cf.effective.alpha,false,true));
    for(unsigned fault=0;fault<7;++fault) {
        auto bad=construct;bad.construct=fault==0;bad.request=fault==1;bad.forced=fault==2;bad.enabled=fault!=3;
        assert(!frame_policy(bad,fault!=4,fault!=5,fault!=6,false).portal);
    }
    assert(!frame_policy(construct,true,true,true,true).portal);
    assert(!background_permission(false,false,false,0,1,false,true));
    assert(!background_permission(false,true,true,1,1,false,true));
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
