// SPDX-License-Identifier: MIT
// CPU-only C entry used by the pinned instruction fixture, not a runtime export.
#include "camera.hpp"
#include <cassert>
using namespace native_portal;
extern "C" int camera_binding_probe(uint64_t pc,arena::Saved* s,int route) {
    CameraAccessGate gate;binding::CameraNativeBinding native(gate);
    CameraBinding b{1,s->x[19],s->x[26],4,5,6,1,1,1};
    uint64_t e=0;
    arena::Saved pub=*s;
    assert(native.publish_completed(0x8dc1c,pub,b,std::this_thread::get_id(),e));
    if(pc==0x8dc1c) {
        uint64_t t=0;CameraBinding observed{};
        assert(gate.retain(e,t)&&gate.lookup(t,observed)&&observed==b);
        assert(observed.camera!=s->x[0]);assert(gate.release(t));
        s->pad=route;return 1;
    }
    if(route==0) {s->pad=0;return 1;} // original displacement equivalence only
    uint64_t ticket=0;
    if(route==3)assert(gate.retain(e,ticket));
    struct Args {arena::Saved* s;unsigned calls=0;} args{s};
    auto result=native.cleanup(pc,*s,[](void* p) noexcept {
        auto& a=*static_cast<Args*>(p);++a.calls;a.s->x[0]=0x707;
    },&args,std::chrono::milliseconds(0));
    if(route==3) {
        assert(result==binding::CameraNativeBinding::Dispatch::Refused&&args.calls==0&&s->pad==3);
        assert(gate.release(ticket));
    } else assert(result==binding::CameraNativeBinding::Dispatch::Completed&&args.calls==1&&s->pad==2);
    return 1;
}
