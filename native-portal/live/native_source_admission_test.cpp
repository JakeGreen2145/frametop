// SPDX-License-Identifier: MIT
// Pure provider contract tests. Scope/observations here are explicitly synthetic;
// production_admission_test exercises Runtime() with pinned native event vectors.
#include "native_source_admission.hpp"
#include <cassert>
using namespace native_portal;
using namespace native_portal::live;
struct Scope {
    CameraBinding b{1,2,3,4,5,6,7,8,9};
    NativeSourceAdmission::State state{0x10000000,{0,0},0,0,0};
    bool present=true;
    static bool auth(void* p,const CameraBinding& b,NativeSourceAdmission::State& s) noexcept {
        auto& f=*static_cast<Scope*>(p);if(!f.present || !(b==f.b))return false;s=f.state;return true;
    }
};
int main() {
    Scope f;NativeSourceAdmission provider(&f,Scope::auth);
    assert(provider.connect_eligible(f.b));
    render::NativeBorrow n;assert(!provider.borrow_ready(f.b,n));
    provider.before(f.b,coordination::Action::Connect);
    f.state.ids={11,12};f.state.active=f.state.connected=1;
    ClientSourceBinding::Observation observed;observed.valid=true;observed.actual=1;observed.provider=77;observed.action=coordination::Action::Connect;
    for(unsigned i=0;i<2;++i)observed.queues[i]={true,0,11+i,11+i,88+i,ClientSourceBinding::Entry::NewEntry};
    provider.after(f.b,observed);
    assert(provider.connect_eligible(f.b));
    coordination::ActionPlan plan;plan.action=coordination::Action::Connect;
    auto receipt=provider.transition(f.b,plan,1);
    assert(receipt.use==coordination::UseDisposition::NativeConnection);
    assert(receipt.queues[0].ownership==coordination::Ownership::NativeConnection && !receipt.queues[0].header_generation);
    n.watermark=2;n.command=999;n.invocation_sp=444;n.camera=f.b.camera;n.resource_index=1;n.slot=3;n.selected_texture=555;
    std::memcpy(n.original.data()+0x80,&n.selected_texture,8);n.replacement=n.original;
    n.provenance.ticket=101;n.provenance.frame=222;n.provenance.recording_generation=333;
    n.provenance.identity={1,f.b.renderer,f.b.renderer_generation,f.b.device,f.b.device_generation};
    assert(provider.borrow_ready(f.b,n));
    assert(!provider.borrow_ready(f.b,n));
    n.provenance.eye=1;n.provenance.ticket=102;assert(provider.borrow_ready(f.b,n));
    assert(!provider.settled(f.b,102)); // a ticket ID is not the pair watermark
    assert(provider.settled(f.b,2));
    // A retained-debt release can fail after proof. Rechecking that same proof
    // must be idempotent without a second borrow or another native read.
    assert(provider.settled(f.b,2));
    assert(!provider.settled(f.b,4));
    Scope partial;NativeSourceAdmission failed(&partial,Scope::auth);
    failed.before(partial.b,coordination::Action::Connect);
    auto failure=observed;failure.actual=0;failure.queues[0]={true,7,0,0,0,ClientSourceBinding::Entry::Unknown};
    partial.state.ids={0,12};failed.after(partial.b,failure);
    assert(!failed.connect_eligible(partial.b));
    const auto raw=failed.transition(partial.b,plan,0);
    assert(raw.queues[0].observed && raw.queues[0].value==7);
    assert(raw.queues[1].observed && raw.queues[1].value==0 && raw.queues[1].id==12);
    assert(raw.use==coordination::UseDisposition::None && raw.queues[1].ownership==coordination::Ownership::Unknown);
    provider.before(f.b,coordination::Action::Disconnect);
    ClientSourceBinding::Observation empty;empty.valid=true;empty.actual=1;empty.action=coordination::Action::Disconnect;
    provider.after(f.b,empty);assert(!provider.connect_eligible(f.b));
    plan.action=coordination::Action::Disconnect;const auto release=provider.transition(f.b,plan,1);
    assert(release.use==coordination::UseDisposition::None && !release.queues[0].observed && !release.queues[1].observed);
}
