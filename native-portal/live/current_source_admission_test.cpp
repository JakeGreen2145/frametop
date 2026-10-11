// SPDX-License-Identifier: MIT
// Explicit source-model boundary events, not installed hooks or hardware.
#define main stock_test_main
#include "stock_source_receipts_test.cpp"
#undef main
#include "current_source_selection.hpp"
#include <cstdio>
template<class Base=Fixture> struct CurrentFixture: Base {
    using Base::client;
    static bool identity(void* p,const CameraBinding& b,SourceIdentity& out) noexcept {
        auto& f=*static_cast<CurrentFixture*>(p);
        if(!f.auth_live || !(b==f.b))return false;
        out={1,1,client};return true;
    }
    static bool owner(void* p) noexcept {return static_cast<CurrentFixture*>(p)->fold_owner==std::this_thread::get_id();}
    static bool render_covered(void* p,ClientSourceBinding::Module,uint64_t pc) noexcept {
        return static_cast<CurrentFixture*>(p)->missing_pc!=pc;
    }
    NativeSourceAdmission admission{this,CurrentSourceContracts{identity},owner};
    CurrentRenderCollector render{this->gate,this,identity,render_covered};
    bool select(render::NativeBorrow n,uint64_t invocation) {
        n.provenance.origin=1;
        auto s=this->entry();
        assert(render.begin(this->ticket,this->b,invocation,0,s));
        s.sp-=0x460;s.x[27]=this->b.camera;s.x[0]=1;
        assert(render.capture(0x160ca8,s));
        assert(render.capture(0x161bc0,s));
        s.x[0]=n.resource_index;s.x[28]=99; // pre mov w28,w0 must use x0
        assert(render.capture(0x161be8,s));
        s.x[24]=99;assert(render.capture(0x161c74,s)); // pre-load stale x24
        s.x[24]=n.slot;assert(render.capture(0x161c78,s));
        s.x[0]=this->b.renderer;s.x[1]=77;s.x[2]=this->b.camera+0xc0;s.x[7]=0x2993f8;
        assert(render.capture(0x161d64,s));assert(render.upload_reserved(n.provenance.ticket));
        assert(render.capture(0x161d68,s));
        s.x[0]=n.selected_texture;assert(render.capture(0x161e40,s));
        s.x[0]=this->b.renderer;s.x[1]=s.sp+0x300;s.x[22]=s.x[1];s.x[2]=0x450650;s.x[4]=0x29b278;
        s.x[28]=n.resource_index;s.x[21]=s.x[24]=3*n.slot;
        assert(render.capture(0x161ed8,s));n.invocation_sp=s.sp;
        const bool result=render.with_selection(n,[](void* p,const CurrentRenderSelection& selection,render::NativeBorrow value) noexcept {
            auto& f=*static_cast<CurrentFixture*>(p);return f.admission.borrow_ready(f.b,selection,value);
        },this);
        render.finish(invocation,this->entry());return result;
    }
};
void cold_connect() {
    CurrentFixture<SourceFixture<CurrentConnectCollector>> f;
    assert(f.admission.bootstrap_eligible(f.b).needs_connect);
    assert(f.admission.begin_current_connect(f.b,f.transport.invocation_id()));
    f.connect();
    auto result=f.transport.take();assert(result);
    assert(f.admission.finish_current_connect(f.b,*result));
    assert(f.admission.bootstrap_eligible(f.b).available && !f.admission.bootstrap_eligible(f.b).needs_connect);
    coordination::ActionPlan plan;plan.action=A::Connect;
    const auto receipt=f.admission.transition(f.b,plan,1);
    assert(receipt.use==coordination::UseDisposition::AuthenticatedBorrowedUse);
    assert(receipt.queues[0].ownership==coordination::Ownership::Borrowed);
    assert(!f.admission.transition_eligible(f.b,A::Disconnect));
    f.permitted=false;f.memory.clear();const auto reads=f.reads;
    assert(f.select(f.borrow(),1));assert(f.select(f.borrow(1),2));
    render::Recorded record;record.prefix=f.borrow().replacement;
    assert(f.admission.backend(f.b,101,record,999,333));
    assert(!f.admission.settled(f.b,101));assert(f.admission.settled(f.b,2));
    assert(f.reads==reads && f.auth_reads==0);
}
void warm_without_new_update() {
    CurrentFixture<> f;f.warm();assert(f.begin(0x15fcc0));
    f.metadata(1);f.metadata(1,true);f.metadata(0);f.metadata(0,true);assert(f.end(0,0x15fdc8));
    assert(f.admission.fold_stock_history(f.transport));
    f.permitted=false;f.memory.clear();const auto reads=f.reads;
    assert(f.admission.bootstrap_eligible(f.b).available);
    assert(f.select(f.borrow(),1));assert(f.select(f.borrow(1),2));
    assert(f.reads==reads && f.auth_reads==0);
}
void current_update_optional(bool fail) {
    CurrentFixture<SourceFixture<CurrentUpdateCollector>> f;Fixture history;history.connect();
    assert(f.admission.fold_stock_history(history.transport));f.warm();
    assert(f.admission.begin_current_update(f.b,f.transport.invocation_id()));
    assert(f.begin(0x15fcc0));f.metadata(0,false,fail);if(!fail)f.metadata(0,true);
    assert(f.end(0,0x15fdc8));auto result=f.transport.take();assert(result);
    assert(f.admission.finish_current_update(f.b,*result)==!fail);
    assert(f.admission.bootstrap_eligible(f.b).available==!fail);
    f.permitted=false;f.memory.clear();assert(f.select(f.borrow(),1)==!fail);
}
int main() {
    warm_without_new_update();current_update_optional(false);current_update_optional(true);
    cold_connect();
    CurrentFixture<> f;
    f.permitted=false;
    const auto e=f.admission.bootstrap_eligible(f.b);
    assert(e.available && e.needs_connect);
    assert(f.reads==0 && f.auth_reads==0);
    assert(!f.admission.borrow_ready(f.b,f.borrow()));
    assert(!f.admission.transition_eligible(f.b,A::Disconnect));
    assert(f.admission.begin_current_connect(f.b,1));
    assert(!f.admission.bootstrap_eligible(f.b).available);
    assert(!f.admission.begin_current_connect(f.b,2));
    std::puts("PASS identity-only bootstrap without current camera reads");
}
