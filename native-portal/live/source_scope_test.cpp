// SPDX-License-Identifier: MIT
#include "camera_owner.hpp"
#include <cassert>
#include <stdexcept>
using namespace native_portal;
int main() {
    // Process-lifetime owner: no fake native-idle certificate for its destructor.
    auto* owner=new live::CameraOwner;
    CameraBinding b{1,0x200000,3,4,5,6,7,8,9};arena::Saved s{};
    s.sp=0x80000;s.x[19]=b.application;s.x[20]=b.application+0x20000;s.x[26]=b.camera;
    assert(owner->observe_publication(0x8dc1c,s) && owner->bind(b,7));
    unsigned calls=0;uint64_t first=0;
    auto observe=[&](CameraAccessGate& gate,uint64_t ticket,const CameraBinding& held) {
        CameraBinding check;assert(held==b && ticket && gate.lookup(ticket,check) && check==b);++calls;
        if(!first)first=ticket;
    };
    assert(!owner->observe_scope(observe));
    owner->direct([&](const auto&,auto&) {
        assert(owner->observe_scope(observe));
        std::thread foreign([&]{assert(!owner->observe_scope(observe));});foreign.join();
        return coordination::NativeReceipt{};
    });
    assert(calls==1 && !owner->observe_scope(observe));
    owner->direct([&](const auto&,auto&)->coordination::NativeReceipt {
        assert(owner->observe_scope(observe));throw std::runtime_error("test scope unwind");
    });
    assert(calls==2 && !owner->observe_scope(observe));
    assert(owner->begin_close());
    uint64_t closing=0;
    for(unsigned i=0;i<2;++i)owner->drain([&](const auto&,auto&) {
        assert(owner->observe_scope([&](CameraAccessGate& gate,uint64_t ticket,const CameraBinding& held) {
            uint64_t rejected=0;assert(held==b && !gate.retain(owner->epoch(),rejected));
            assert(ticket!=first && (!closing || closing==ticket));closing=ticket;++calls;
        }));return coordination::NativeReceipt{};
    });
    assert(calls==4 && !owner->observe_scope(observe));
    std::puts("PASS original active-invoke ticket, foreign/outside-scope refusal, unwind and same closing barrier");
    std::fflush(stdout);_Exit(0);
}
