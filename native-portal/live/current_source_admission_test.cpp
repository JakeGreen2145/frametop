// SPDX-License-Identifier: MIT
// Explicit source-model boundary events, not installed hooks or hardware.
#define main stock_test_main
#include "stock_source_receipts_test.cpp"
#undef main
#include "current_source_selection.hpp"
#include <cstdio>
#include <fstream>
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
    uint64_t fault_pc=0,fault_value=0;unsigned fault_reg=0;bool fault_sp=false;
    unsigned borrow_fault=0;std::optional<CurrentRenderSelection> escaped;
    render::NativeBorrow escaped_borrow{};
    std::map<uint64_t,arena::Saved> vectors;
    bool capture(uint64_t pc,arena::Saved s) {
        if(vectors.count(pc))s=vectors.at(pc);
        if(pc==fault_pc){if(fault_sp)s.sp=fault_value;else s.x[fault_reg]=fault_value;}
        return render.capture(pc,s);
    }
    render::NativeBorrow borrow(unsigned eye=0) const {
        auto n=Base::borrow(eye);n.provenance.origin=1;return n;
    }
    bool select(render::NativeBorrow n,uint64_t invocation) {
        auto s=this->entry();
        if(!render.begin(this->ticket,this->b,invocation,0,s))return false;
        s.sp-=0x460;s.x[27]=this->b.camera;s.x[0]=1;
        if(!capture(0x160ca8,s))return false;
        if(!capture(0x161bc0,s))return false;
        s.x[0]=n.resource_index;s.x[28]=99; // pre mov w28,w0 must use x0
        if(!capture(0x161be8,s))return false;
        s.x[0]=this->b.camera+4*n.resource_index;s.x[24]=99;
        if(!capture(0x161c74,s))return false; // pre-load stale x24
        s.x[24]=n.slot;if(!capture(0x161c78,s))return false;
        s.x[0]=this->b.renderer;s.x[1]=77;s.x[2]=this->b.camera+0xc0;s.x[7]=0x2993f8;
        if(!capture(0x161d64,s))return false;
        if(!render.upload_reserved(n.provenance.ticket))return false;
        if(!capture(0x161d68,s))return false;
        s.x[0]=n.selected_texture;if(!capture(0x161e40,s))return false;
        s.x[0]=this->b.renderer;s.x[1]=s.sp+0x300;s.x[22]=s.x[1];s.x[2]=0x450650;s.x[4]=0x29b278;
        s.x[28]=n.resource_index;s.x[21]=s.x[24]=3*n.slot;
        if(!capture(0x161ed8,s))return false;
        n.invocation_sp=s.sp;
        auto sample=render.sample();assert(sample && sample->resource_index==n.resource_index &&
            sample->slot==n.slot && sample->selected_texture==n.selected_texture && sample->upload_ticket==n.provenance.ticket);
        const bool result=render.with_selection(n,[](void* p,const CurrentRenderSelection& selection,render::NativeBorrow value) noexcept {
            auto& f=*static_cast<CurrentFixture*>(p);f.escaped=selection;f.escaped_borrow=value;
            switch(f.borrow_fault) {
            case 1:++value.command;break;case 2:++value.provenance.recording_generation;break;
            case 3:++value.provenance.frame;break;case 4:++value.provenance.origin;break;
            case 5:++value.provenance.eye;break;case 6:++value.provenance.ticket;break;
            case 7:++value.watermark;break;case 8:++value.invocation_sp;break;
            case 9:++value.selected_texture;break;case 10:++value.resource_index;break;
            case 11:++value.slot;break;case 12:value.original[0]=std::byte{1};break;
            case 13:value.replacement[8]=std::byte{1};break;
            case 14:++value.provenance.identity.renderer_generation;break;
            case 15:++value.provenance.identity.device_generation;break;
            case 16:++value.provenance.descriptors[0].offset;break;
            case 17:f.auth_live=false;break;
            default:break;
            }
            const bool accepted=f.admission.borrow_ready(f.b,selection,value);
            assert(!f.admission.borrow_ready(f.b,selection,value)); // one-shot, even on failure
            return accepted;
        },this);
        assert(!render.sample());
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
void no_op_update_before_bootstrap() {
    CurrentFixture<SourceFixture<CurrentUpdateCollector>> f;
    assert(f.admission.begin_current_update(f.b,f.transport.invocation_id()));
    assert(f.begin(0x15fcc0));assert(f.end(0,0x15fdc8));auto r=f.transport.take();assert(r);
    assert(f.admission.finish_current_update(f.b,*r));
    const auto e=f.admission.bootstrap_eligible(f.b);assert(e.available && e.needs_connect);
    assert(!f.select(f.borrow(),1));
}
void current_connect_negatives() {
    using F=CurrentFixture<SourceFixture<CurrentConnectCollector>>;
    {F f;assert(!f.begin(0x15fcc0));assert(f.reads==0);}
    for(unsigned kind=0;kind<5;++kind) {
        F f;assert(f.admission.begin_current_connect(f.b,f.transport.invocation_id()));assert(f.begin());
        if(kind!=2)f.fact(0x15f9fc,1);
        f.fact(0x15fa04,0);
        if(kind!=3){f.connect_queue(0,false,kind==0?7:0);f.connect_queue(1);}
        if(kind!=1)f.fact(0x15fabc);
        if(kind==4){auto s=f.entry();s.x[0]=1;assert(f.collector.camera_return(0x15faa4,s));assert(!f.transport.take());}
        else {
            assert(f.end(kind==0?0:1,0x15faa4));auto r=f.transport.take();assert(r);
            assert(!f.admission.finish_current_connect(f.b,*r));
            if(kind==0){coordination::ActionPlan p;p.action=A::Connect;const auto raw=f.admission.transition(f.b,p,0);
                assert(raw.use==coordination::UseDisposition::None && raw.queues[0].value==7 && raw.queues[1].id==12);}
        }
        f.state={F::client,{0,0},0,0,0};assert(!f.admission.bootstrap_eligible(f.b).available);
        assert(!f.admission.begin_current_connect(f.b,f.transport.invocation_id()+100));
    }
    for(unsigned kind=0;kind<4;++kind) {
        F f;assert(f.admission.begin_current_connect(f.b,f.transport.invocation_id()));assert(f.begin());
        auto call=f.collector.invocation();auto s=f.entry();s.x[0]=1;
        if(kind==0){++s.sp;assert(!f.collector.camera_return(0x15faa4,s));}
        else if(kind==1){++s.x[30];assert(!f.collector.camera_return(0x15faa4,s));}
        else {assert(f.collector.camera_return(0x15faa4,s));if(kind==2)++call.sequence;else ++call.ticket;
            assert(!f.collector.complete_own_bypass(call,1));}
        assert(!f.transport.take());assert(!f.admission.bootstrap_eligible(f.b).available);
    }
    for(const auto& site:NativeStockReadBoundary::required_sites) {
        F f;f.missing_pc=site.pc;assert(f.admission.begin_current_connect(f.b,f.transport.invocation_id()));
        assert(!f.begin());assert(f.reads==0);assert(!f.admission.bootstrap_eligible(f.b).available);
    }
}
void selection_negatives() {
    struct Fault {uint64_t pc;unsigned reg;uint64_t value;};
    const Fault faults[]={{0x161c74,0,0},{0x161c78,0,0},{0x160ca8,0,0},{0x160ca8,27,1},{0x161be8,0,2},{0x161c78,24,16},
        {0x161d64,0,99},{0x161d64,2,0},{0x161d64,7,0},{0x161e40,0,0},{0x161ed8,0,99},
        {0x161ed8,1,0},{0x161ed8,22,0},{0x161ed8,2,0},{0x161ed8,4,0},{0x161ed8,28,0},
        {0x161ed8,21,6},{0x161ed8,24,6}};
    for(const auto& fault:faults) {
        CurrentFixture<> f;f.connect();assert(f.admission.fold_stock_history(f.transport));
        f.fault_pc=fault.pc;f.fault_reg=fault.reg;f.fault_value=fault.value;
        assert(!f.select(f.borrow(),1));
    }
    for(const auto pc:CurrentRenderCollector::required_sites) {
        CurrentFixture<> f;f.connect();assert(f.admission.fold_stock_history(f.transport));
        f.missing_pc=pc;assert(!f.select(f.borrow(),1));
    }
    for(const auto pc:CurrentRenderCollector::required_sites)if(pc!=0x160be8 && pc!=0x160dd4 && pc!=0x161f30) {
        CurrentFixture<> f;f.connect();assert(f.admission.fold_stock_history(f.transport));
        f.fault_pc=pc;f.fault_sp=true;f.fault_value=Fixture::sp-0x360;assert(!f.select(f.borrow(),1));
    }
    for(unsigned fault=1;fault<=17;++fault) {
        CurrentFixture<> f;f.connect();assert(f.admission.fold_stock_history(f.transport));
        f.borrow_fault=fault;assert(!f.select(f.borrow(),1));f.auth_live=true;
        assert(f.escaped && !f.admission.borrow_ready(f.b,*f.escaped,f.escaped_borrow));
    }
    for(unsigned fault=0;fault<7;++fault) {
        CurrentFixture<> f;f.connect();assert(f.admission.fold_stock_history(f.transport));assert(f.select(f.borrow(),1));
        auto n=f.borrow(1);
        switch(fault) {case 0:++n.provenance.frame;break;case 1:++n.provenance.origin;break;
        case 2:++n.command;break;case 3:++n.provenance.recording_generation;break;
        case 4:++n.watermark;break;case 5:n.provenance.eye=0;break;case 6:n.provenance.ticket=101;break;}
        assert(!f.select(n,2));assert(!f.admission.settled(f.b,2));
    }
    for(unsigned offset=0;offset<352;++offset) {
        CurrentFixture<> f;f.connect();assert(f.admission.fold_stock_history(f.transport));auto n=f.borrow();
        n.replacement[offset]^=std::byte{1};const bool owned=(offset>=8 && offset<16) || (offset>=0x38 && offset<0x40) || (offset>=0x58 && offset<0x60);
        assert(f.select(n,1)==owned);
    }
    {CurrentFixture<> f;assert(!f.select(f.borrow(),1));} // selection without source history
    {CurrentFixture<> f;f.connect();assert(f.admission.fold_stock_history(f.transport));
        assert(f.select(f.borrow(),1));assert(!f.admission.borrow_ready(f.b,*f.escaped,f.escaped_borrow));
        assert(f.select(f.borrow(1),2));f.state={Fixture::client,{777,888},0,0,255};f.permitted=false;
        render::Recorded r;r.prefix=f.borrow().replacement;
        assert(!f.admission.backend(f.b,101,r,998,333));assert(!f.admission.backend(f.b,101,r,999,334));
        f.auth_live=false;assert(!f.admission.backend(f.b,101,r,999,333));assert(!f.admission.settled(f.b,2));
        f.auth_live=true;assert(f.admission.backend(f.b,101,r,999,333));assert(f.admission.settled(f.b,2));
        assert(!f.admission.backend(f.b,101,r,999,333));assert(f.admission.settled(f.b,2));
        auto n=f.borrow();n.watermark=4;assert(!f.select(n,3));}
}
void render_return_scope() {
    CurrentFixture<> f;auto s=f.entry();assert(f.render.begin(f.ticket,f.b,1,0,s));
    assert(f.render.capture(0x160dd4,s));assert(f.render.begin(f.ticket,f.b,2,0,s));
    auto body=s;body.sp-=0x460;body.x[27]=f.b.camera;body.x[0]=0;
    assert(f.render.capture(0x160ca8,body));
    assert(f.render.capture(0x161f30,s));
    assert(f.render.begin(f.ticket,f.b,3,0,s));++s.x[30];assert(!f.render.capture(0x161f30,s));
}
void replay_current_vectors(const char* path) {
    CurrentFixture<SourceFixture<CurrentConnectCollector>> f;
    assert(f.admission.begin_current_connect(f.b,f.transport.invocation_id()));f.connect();
    auto result=f.transport.take();assert(result && f.admission.finish_current_connect(f.b,*result));
    std::ifstream in(path,std::ios::binary);assert(in);uint64_t pc=0;arena::Saved s{};
    while(in.read(reinterpret_cast<char*>(&pc),8)) {
        assert(in.read(reinterpret_cast<char*>(&s),sizeof(s)));assert(!f.vectors.count(pc));f.vectors[pc]=s;
    }
    assert(in.eof() && !in.gcount() && f.vectors.size()==5);
    f.permitted=false;f.memory.clear();const auto reads=f.reads;
    auto n=f.borrow();n.resource_index=0;assert(f.select(n,1));
    n=f.borrow(1);n.resource_index=0;assert(f.select(n,2));assert(f.admission.settled(f.b,2));
    assert(f.reads==reads);
    std::puts("PASS pinned selector/helper, connected/slot/texture Saved through current collector and cold borrowed journal");
}
int main(int argc,char** argv) {
    if(argc==2 && !std::strcmp(argv[1],"--inventory")) {
        for(const auto pc:CurrentRenderCollector::required_sites)std::printf("compositor 0x%llx\n",static_cast<unsigned long long>(pc));
        return 0;
    }
    if(argc==2){replay_current_vectors(argv[1]);return 0;}
    no_op_update_before_bootstrap();render_return_scope();current_connect_negatives();selection_negatives();
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
