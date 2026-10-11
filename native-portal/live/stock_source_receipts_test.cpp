// SPDX-License-Identifier: MIT
// Source models, not native instruction execution or Runtime routing coverage.
#include "stock_source_receipts.hpp"
#include "native_source_admission.hpp"
#include <cassert>
#include <cstring>
#include <map>
#include <thread>
#include <sys/wait.h>
using namespace native_portal;
using namespace native_portal::live;
using M=ClientSourceBinding::Module;
using A=coordination::Action;
template<class Collector=StockSourceCollector> struct SourceFixture {
    CameraBinding b{1,2,0x90000000,4,5,6,7,8,9};
    CameraAccessGate gate;
    uint64_t epoch=0,ticket=0;
    static constexpr uint64_t client=0x10000000,manager=0x90020000,sp=0x90040000;
    StockSourceState state{client,{0,0},0,0,0};
    unsigned reads=0,auth_reads=0,state_reads=0;
    bool permitted=true,auth_live=true;
    std::thread::id fold_owner=std::this_thread::get_id();
    uint64_t missing_pc=0;bool provider_lifetime=true;
    const arena::Saved* native_output_vector=nullptr;
    struct ScopeRejected {};
    std::map<uint64_t,unsigned char> memory;
    typename Collector::Transport transport{b,1,std::this_thread::get_id(),1};
    Collector collector{gate,transport,{read,this,covered,provider_live}};
    Collector* current=&collector;
    NativeSourceAdmission provider{this,auth};
    SourceFixture() {
        assert(gate.publish(b,std::this_thread::get_id(),epoch));assert(gate.retain(epoch,ticket));
        put(manager,client+0x60f108);put(client+0x60f110,client+0x32a270);
        put(client+0x60f118,client+0x328380);
        put(client+0x60f138,client+0x3280a8);put(client+0x60f140,client+0x327640);
    }
    ~SourceFixture(){assert(gate.release(ticket));}
    void put(uint64_t a,uint64_t v,size_t size=8){for(size_t i=0;i<size;++i)memory[a+i]=static_cast<unsigned char>(v>>(i*8));}
    static bool read(void* p,const StockSourceReadScope& scope,uint64_t a,void* out,size_t n) noexcept {
        auto& f=*static_cast<SourceFixture*>(p);++f.reads;
        assert(scope.binding==f.b && scope.ticket==f.ticket && scope.version==1);
        if(!f.permitted || !scope.pc)return false;
        // Camera fields are never exposed by the backend, even if populated.
        assert(!(a>=f.b.camera+0x349 && a<f.b.camera+0x361));
        assert(scope.pc!=0x15f9e0 && scope.pc!=0x15fba8 && scope.pc!=0x15fcc0 &&
               scope.pc!=0x15fa78 && scope.pc!=0x15faa4 && scope.pc!=0x15fbd8 && scope.pc!=0x15fc20 && scope.pc!=0x15fdc8);
        for(size_t i=0;i<n;++i){auto it=f.memory.find(a+i);if(it==f.memory.end())return false;static_cast<unsigned char*>(out)[i]=it->second;}
        return true;
    }
    static bool covered(void* p,M m,uint64_t pc) noexcept {
        auto& f=*static_cast<SourceFixture*>(p);if(pc==f.missing_pc)return false;
        for(const auto& site:NativeStockReadBoundary::required_sites)if(site.module==m && site.pc==pc)return true;
        return false;
    }
    static bool provider_live(void* p,const StockSourceReadScope& scope,uint64_t provider) noexcept {
        auto& f=*static_cast<SourceFixture*>(p);
        return f.provider_lifetime && scope.binding==f.b && scope.ticket==f.ticket && provider==manager;
    }
    static bool auth(void* p,const CameraBinding& b,NativeSourceAdmission::State& out) noexcept {
        auto& f=*static_cast<SourceFixture*>(p);++f.auth_reads;if(!f.auth_live || !(b==f.b))return false;out=f.state;return true;
    }
    arena::Saved entry() const {arena::Saved s{};s.x[0]=b.camera;s.sp=sp;s.x[30]=0x123456;return s;}
    bool begin(uint64_t method=0x15f9e0){
        const bool ok=current->begin(ticket,b,1,method,entry(),0,client,ClientSourceBinding::compositor_sha,ClientSourceBinding::client_sha);
        if(ok && method==0x15fcc0){fact(0x15fd30,state.active);fact(0x15fd90,state.connected);}return ok;
    }
    bool end(uint64_t actual=1,uint64_t pc=0x15fa78){
        auto s=entry();s.x[0]=actual;const bool candidate=current->camera_return(pc,s);
        const bool complete=current->complete_own_bypass(current->invocation(),actual);return candidate && complete;
    }
    void fact(uint64_t pc,uint64_t value=0){
        auto s=entry();s.sp-=pc>=0x15fd00?0x360:0x20;s.x[19]=b.camera;s.x[0]=s.x[1]=s.x[20]=value;
        if(!current->camera_fact(M::Compositor,pc,s))throw ScopeRejected{};
    }
    void release_flags(){fact(0x15fbc4);fact(0x15fc4c);fact(0x15fc08);}
    void guard(uint64_t pc,const arena::Saved& s){if(!current->boundary_event(M::Client,pc,s))throw ScopeRejected{};}
    void event(M m,uint64_t pc,const arena::Saved& s){if(!current->event(m,pc,s))throw ScopeRejected{};}
    void connect_queue(unsigned q,bool existing=false,int result=0) {
        arena::Saved s{};s.sp=sp-0x20;s.x[19]=b.camera;s.x[0]=manager;
        s.x[1]=b.camera+0x350+8*q;s.x[2]=q?0x450358:0x4502e8;s.x[3]=client+0x32a270;
        const auto start=q?0x15fa44:0x15fa24;event(M::Compositor,start,s);
        s.x[30]=start+4;event(M::Client,0x32a270,s);
        s.sp-=0xc0;s.x[20]=manager;s.x[23]=b.camera+0x350+8*q;s.x[19]=11+q;
        if(!result){
            if(existing){s.x[0]=11+q;event(M::Client,0x32a398,s);}
            else {event(M::Client,0x32a39c,s);s.x[0]=0x90100000+q*0x1000;put(s.x[0],11+q);event(M::Client,0x32a49c,s);event(M::Client,0x32a504,s);}
            state.ids[q]=11+q;put(b.camera+0x350+8*q,11+q);
            assert(current->camera_fact(M::Client,0x32a628,(q==0 && native_output_vector)?*native_output_vector:s));
        }
        s.sp=sp-0x20;s.x[19]=b.camera;s.x[0]=static_cast<uint32_t>(result);event(M::Compositor,start+4,s);
    }
    void connect(bool existing=false){assert(begin());fact(0x15f9fc,1);fact(0x15fa04,0);
        connect_queue(0,existing);connect_queue(1,existing);fact(0x15fabc);fact(0x15fa94,1);
        state.active=state.connected=1;assert(end(1,0x15faa4));}
    void disconnect_queue(unsigned q,int result=0){
        arena::Saved s{};s.sp=sp-0x20;s.x[19]=b.camera;s.x[0]=manager;s.x[1]=state.ids[q];s.x[2]=client+0x328380;
        const auto pc=q?0x15fc34:0x15fbec;event(M::Compositor,pc,s);s.x[0]=static_cast<uint32_t>(result);event(M::Compositor,pc+4,s);
    }
    void warm(){state.ids={11,12};state.active=state.connected=1;for(unsigned q=0;q<2;++q)put(b.camera+0x350+8*q,11+q);}
    void metadata(unsigned q,bool release=false,bool fail=false,int lock_result=1) {
        const uint64_t local_sp=sp-0x360,object=0x90200000+q*0x1000,header=0x90300000+q*0x1000;
        const uint64_t meta=0x90400000+q*0x1000,data=meta+0x100;
        const uint64_t start=release?(q?0x160164:0x160380):(q?0x15fee4:0x160184);
        const uint64_t manager_entry=release?0x327640:0x3280a8,call=release?0x3276c4:0x32813c;
        const uint64_t inner_entry=release?0x3274d8:0x327e18,mframe=release?0x40:0x50,iframe=release?0x60:0x70;
        arena::Saved s{};s.sp=local_sp;s.x[19]=b.camera;s.x[0]=manager;s.x[1]=state.ids[q];
        s.x[2]=release?meta:local_sp+0x90;s.x[3]=release?client+manager_entry:local_sp+0x98;s.x[5]=client+manager_entry;
        event(M::Compositor,start,s);s.x[30]=start+4;event(M::Client,manager_entry,s);
        if(!fail){
            s.sp=local_sp-mframe;s.x[0]=0;s.x[30]=client+(release?0x327678:0x3280ec);
            guard(release?0x327678:0x3280ec,s);
            s.x[19]=state.ids[q];s.x[20]=manager+0x70;s.x[0]=object;
            s.x[1]=release?meta:local_sp+0x90;s.x[2]=release?0:local_sp+0x98;s.x[3]=0;
            event(M::Client,call,s);s.x[30]=client+call+4;event(M::Client,inner_entry,s);
            s.sp-=iframe;s.x[release?21:20]=object;
            const auto inner=s.sp,address=inner+(release?0x48:0x58);
            auto g=s;g.x[0]=address;g.x[30]=client+(release?0x327504:0x327e4c);guard(0x258438,g);
            if(lock_result>=0){g.sp=inner-0x30;g.x[19]=address;g.x[0]=lock_result;g.x[30]=client+0x2584c4;guard(0x2584c4,g);}
            // Normal accessor return leaves the successful guard alive.
            g.sp=inner;g.x[30]=client+(release?0x327504:0x327e4c);guard(0x2584f8,g);
            s.x[0]=header;put(header,state.ids[q]);
            event(M::Client,release?0x32750c:0x327e54,s);
            s.x[1]=release?meta:local_sp+0x90;event(M::Client,release?0x3275a0:0x327f2c,s);
            put(local_sp+0x90,meta);put(local_sp+0x98,data);s.x[0]=0;
            event(M::Client,release?0x3275a4:0x327f30,s);
            put(header+0x68,release?2:1,4);s.x[19]=header;s.x[1]=header;s.x[24]=0x68;s.x[0]=release?1:2;
            event(M::Client,release?0x32758c:0x327f70,s);put(header+0x68,s.x[0],4);
            event(M::Client,release?0x327590:0x327f74,s);
            g.sp=inner;g.x[0]=address;g.x[30]=client+(release?0x3275cc:0x327f80);guard(0x258508,g);
            s.sp=local_sp-mframe;s.x[0]=0;event(M::Client,call+4,s);
            s.x[0]=manager+8;guard(release?0x3276d8:0x328150,s);
        }
        s.sp=local_sp;s.x[19]=b.camera;s.x[0]=fail?4:0;event(M::Compositor,start+4,s);
    }
    render::NativeBorrow borrow(unsigned eye=0) const {
        render::NativeBorrow n;n.watermark=2;n.command=999;n.invocation_sp=444;n.camera=b.camera;n.resource_index=1;n.slot=3;n.selected_texture=555;
        std::memcpy(n.original.data()+0x80,&n.selected_texture,8);n.replacement=n.original;
        n.provenance.ticket=101+eye;n.provenance.eye=eye;n.provenance.frame=222;n.provenance.recording_generation=333;
        n.provenance.identity={1,b.renderer,b.renderer_generation,b.device,b.device_generation};return n;
    }
};
using Fixture=SourceFixture<>;
void positive_connect(bool dedup) {
    Fixture f;f.connect(dedup);
    assert(!f.transport.take()); // immutable facts aren't visible before exclusion
    const auto reads=f.reads,states=f.state_reads,auth=f.auth_reads;
    f.permitted=false;f.memory.clear(); // all historical addresses now unreadable
    assert(f.transport.seal());auto receipt=f.transport.take();assert(receipt);
    assert(receipt->observation().actual==1 && receipt->sequence()==1 && receipt->runtime()==1);
    assert(f.provider.fold_stock_receipt(f.b,1,*receipt));
    assert(f.reads==reads && f.state_reads==states && f.auth_reads==auth);
    assert(f.provider.connect_eligible(f.b));
    assert(!f.provider.transition_eligible(f.b,A::Disconnect)); // old stock Connect is never ours
    assert(f.provider.borrow_ready(f.b,f.borrow()));
    assert(!f.provider.borrow_ready(f.b,f.borrow()));
    f.auth_live=false;assert(!f.provider.borrow_ready(f.b,f.borrow(1)));
    f.auth_live=true;assert(f.provider.borrow_ready(f.b,f.borrow(1)));
    assert(f.provider.settled(f.b,2));
}
void warm_history() {
    Fixture f;f.warm();assert(f.begin(0x15fcc0));
    f.metadata(1);f.metadata(1,true);f.metadata(0);f.metadata(0,true);
    assert(f.end(0,0x15fdc8)); // Update's actual x0 is preserved, not invented true
    const auto reads=f.reads,states=f.state_reads;
    f.permitted=false;f.memory.clear();
    assert(f.provider.fold_stock_history(f.transport));
    assert(f.reads==reads && f.state_reads==states && f.auth_reads==0);
    assert(f.provider.connect_eligible(f.b));assert(f.provider.borrow_ready(f.b,f.borrow()));
    assert(!f.provider.transition_eligible(f.b,A::Disconnect));
}
void failed_warm_history() {
    Fixture f;f.warm();assert(f.begin(0x15fcc0));f.metadata(1,false,true);assert(f.end(0,0x15fdc8));
    assert(!f.provider.fold_stock_history(f.transport));
    f.state={Fixture::client,{0,0},0,0,0}; // later On/cold scalars cannot erase failure
    assert(!f.provider.connect_eligible(f.b));
}
void negative_and_ordering() {
    {Fixture f;assert(f.begin());f.connect_queue(0,false,7);f.connect_queue(1);assert(f.end(0));
     assert(f.transport.seal());auto r=f.transport.take();assert(r && r->observation().actual==0);
     assert(r->observation().queues[0].result==7 && r->observation().queues[1].id==12);
     assert(!f.provider.fold_stock_receipt(f.b,1,*r));coordination::ActionPlan plan;plan.action=A::Connect;
     const auto raw=f.provider.transition(f.b,plan,0);assert(raw.queues[0].value==7 && raw.queues[1].observed && raw.queues[1].id==12);
     assert(raw.use==coordination::UseDisposition::None);f.state={Fixture::client,{0,0},0,0,0};assert(!f.provider.connect_eligible(f.b));}
    {Fixture f;assert(f.begin());f.connect_queue(0);f.connect_queue(1);assert(f.end());
     assert(!f.provider.fold_stock_history(f.transport));f.state.active=f.state.connected=1;assert(!f.provider.connect_eligible(f.b));}
    {Fixture f;f.connect();assert(f.begin(0x15fba8));f.disconnect_queue(0);f.disconnect_queue(1);
     assert(f.end(1,0x15fc20));assert(!f.provider.fold_stock_history(f.transport));}
    {Fixture f;f.connect();assert(f.provider.fold_stock_history(f.transport));f.state.client+=0x1000;assert(!f.provider.connect_eligible(f.b));}
    {Fixture f;assert(f.begin());assert(f.end(1));assert(!f.provider.fold_stock_history(f.transport));} // early Acquire isn't history
    {Fixture f;f.connect();assert(f.begin(0x15fba8));assert(f.end(1,0x15fbd8));assert(!f.provider.fold_stock_history(f.transport));}
    {Fixture f;f.connect();assert(f.begin(0x15fba8));auto s=f.entry();s.sp-=0x20;s.x[19]=f.b.camera;
     assert(f.collector.held_disconnect(0x15fbb8,s));assert(f.end(1,0x15fbd8));
     assert(f.provider.fold_stock_history(f.transport));assert(f.provider.connect_eligible(f.b));}
    {Fixture f;f.connect();assert(f.begin(0x15fba8));f.disconnect_queue(0);f.disconnect_queue(1);
     f.state.active=f.state.connected=0;f.release_flags();assert(f.end(1,0x15fc20));assert(f.provider.fold_stock_history(f.transport));
     assert(f.provider.connect_eligible(f.b));assert(!f.provider.borrow_ready(f.b,f.borrow()));
     // Actual current-owner cold Connect may now obtain its own connection.
     f.provider.before(f.b,A::Connect);f.state.active=f.state.connected=1;
     ClientSourceBinding::Observation o;o.valid=true;o.actual=1;o.action=A::Connect;o.provider=Fixture::manager;
     for(unsigned q=0;q<2;++q)o.queues[q]={true,0,11+q,11+q,99+q,ClientSourceBinding::Entry::NewEntry};
     f.provider.after(f.b,o);assert(f.provider.transition_eligible(f.b,A::Disconnect));}
    {Fixture f;f.connect();assert(f.begin(0x15fba8));f.disconnect_queue(0,2);assert(f.end(0,0x15fc20));assert(!f.provider.fold_stock_history(f.transport));}
    {Fixture f;assert(f.begin());f.collector.cancel();auto pending=f.transport.status();
     assert(pending.pending==1 && pending.completed==0 && pending.abandoned==0 && pending.failed);
     assert(!f.end());auto ended=f.transport.status();assert(ended.pending==0 && ended.completed==1 && ended.abandoned==0 && ended.failed);
     assert(!f.transport.seal());}
    {Fixture f;{StockSourceCollector abandoned(f.gate,f.transport,{Fixture::read,&f,Fixture::covered,Fixture::provider_live});
       assert(abandoned.begin(f.ticket,f.b,1,0x15f9e0,f.entry(),0,Fixture::client,ClientSourceBinding::compositor_sha,ClientSourceBinding::client_sha));}
     auto gap=f.transport.status();assert(gap.pending==0 && gap.completed==0 && gap.abandoned==1 && gap.failed);assert(!f.transport.seal());}
    {Fixture f;assert(f.begin());assert(!f.transport.take());assert(!f.provider.fold_stock_history(f.transport));assert(!f.end());}
    for(unsigned kind=0;kind<3;++kind){Fixture f;assert(f.begin());auto s=f.entry();s.x[0]=1;
      if(kind==0)s.sp+=16;else if(kind==1)++s.x[30];
      assert(!f.collector.camera_return(kind==2?0x15fdc8:0x15fa78,s));assert(!f.transport.seal());}
    {Fixture f;assert(f.begin());assert(!f.begin());assert(!f.end());assert(!f.transport.seal());}
    {Fixture f;f.warm();f.state.ids[1]=f.state.ids[0];f.put(f.b.camera+0x358,f.state.ids[1]);assert(f.begin(0x15fcc0));
     f.metadata(1);f.metadata(1,true);f.metadata(0);f.metadata(0,true);assert(f.end(0,0x15fdc8));
     assert(!f.provider.fold_stock_history(f.transport));}
    {Fixture f;f.warm();assert(f.begin(0x15fcc0));f.metadata(1);assert(!f.end(0,0x15fdc8));assert(!f.transport.seal());}
    for(int lock_result:{-1,0}){Fixture f;f.warm();assert(f.begin(0x15fcc0));
      try{f.metadata(1,false,false,lock_result);assert(false);}catch(const Fixture::ScopeRejected&){}
      assert(!f.end(0,0x15fdc8));assert(!f.provider.fold_stock_history(f.transport));}
    {Fixture f;f.permitted=false;assert(f.begin());assert(f.reads==0);
     try{f.connect_queue(0);assert(false);}catch(const Fixture::ScopeRejected&){}
     assert(!f.provider.fold_stock_history(f.transport));}
    {Fixture f;auto b=f.b;++b.association_generation;assert(!f.collector.begin(f.ticket,b,1,0x15f9e0,f.entry(),0,Fixture::client,ClientSourceBinding::compositor_sha,ClientSourceBinding::client_sha));assert(f.reads==0 && f.state_reads==0);}
    {Fixture f;assert(!f.collector.begin(f.ticket,f.b,2,0x15f9e0,f.entry(),0,Fixture::client,ClientSourceBinding::compositor_sha,ClientSourceBinding::client_sha));assert(f.reads==0 && f.state_reads==0);}
    {Fixture f;assert(f.begin());auto s=f.entry();const auto reads=f.reads;
     std::thread t([&]{assert(!f.collector.event(M::Compositor,0x15fa24,s));});t.join();assert(f.reads==reads);assert(!f.end());}
    {Fixture f;f.connect();assert(f.transport.seal());auto r=f.transport.take();assert(r);
     std::thread t([&]{assert(!f.provider.fold_stock_receipt(f.b,1,*r));});t.join();assert(!f.provider.connect_eligible(f.b));}
    {Fixture f;f.connect();assert(f.transport.seal());auto r=f.transport.take();assert(r);
     assert(f.provider.fold_stock_receipt(f.b,1,*r));assert(!f.provider.fold_stock_receipt(f.b,1,*r));assert(!f.provider.connect_eligible(f.b));}
    {Fixture f;f.connect();assert(f.begin());assert(f.end());assert(f.transport.seal());auto first=f.transport.take(),second=f.transport.take();assert(first && second);
     assert(!f.provider.fold_stock_receipt(f.b,1,*second));assert(!f.provider.fold_stock_receipt(f.b,1,*first));}
    {Fixture f;f.connect();assert(f.transport.seal());auto r=f.transport.take();assert(r);assert(!f.provider.fold_stock_receipt(f.b,2,*r));}
    {Fixture f;f.connect();++f.state.external;assert(f.begin(0x15fcc0));assert(f.end(0,0x15fdc8));
     assert(f.provider.fold_stock_history(f.transport));assert(!f.provider.connect_eligible(f.b));} // async external change belongs to fresh auth
    {Fixture f;f.connect();for(size_t i=1;i<StockSourceTransport::capacity;++i){assert(f.begin());assert(f.end());}
     assert(!f.begin());assert(!f.provider.fold_stock_history(f.transport));assert(!f.provider.connect_eligible(f.b));}
    {Fixture f;assert(f.begin());std::thread t([&]{StockSourceCollector other(f.gate,f.transport,{Fixture::read,&f,Fixture::covered,Fixture::provider_live});
      assert(!other.begin(f.ticket,f.b,1,0x15f9e0,f.entry(),0,Fixture::client,ClientSourceBinding::compositor_sha,ClientSourceBinding::client_sha));});
     t.join();assert(f.transport.failed());assert(!f.transport.take());assert(!f.end());assert(!f.provider.fold_stock_history(f.transport));}
    {Fixture f;assert(f.begin());const auto pid=fork();assert(pid>=0);if(!pid){const auto reads=f.reads;
      const bool rejected=!f.end() && !f.transport.seal() && f.reads==reads;_exit(rejected?0:1);}
     int status=0;assert(waitpid(pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==0);f.collector.cancel();}
}
void independent_callers() {
    // A foreign caller cannot pick itself as a second transport's owner and
    // thereby mutate an existing render-owner provider.
    {Fixture f;std::thread foreign([&]{StockSourceTransport other(f.b,1,std::this_thread::get_id(),1);
      StockSourceCollector own(f.gate,other,{Fixture::read,&f,Fixture::covered,Fixture::provider_live});f.current=&own;f.connect();
      assert(!f.provider.fold_stock_history(other));});foreign.join();f.current=&f.collector;
      assert(!f.provider.connect_eligible(f.b));}
    {Fixture f;NativeSourceAdmission handoff(&f,Fixture::auth,[](void* p) noexcept {
        return std::this_thread::get_id()==static_cast<Fixture*>(p)->fold_owner;
     });std::thread render([&]{f.fold_owner=std::this_thread::get_id();StockSourceTransport stream(f.b,1,f.fold_owner,2);
       StockSourceCollector own(f.gate,stream,{Fixture::read,&f,Fixture::covered,Fixture::provider_live});f.current=&own;f.connect();
       assert(handoff.fold_stock_history(stream));assert(handoff.borrow_ready(f.b,f.borrow()));
     });render.join();f.current=&f.collector;}
    Fixture f;f.connect();const auto original_ticket=f.ticket;
    std::thread t([&]{assert(f.gate.retain(f.epoch,f.ticket));StockSourceCollector own(f.gate,f.transport,{Fixture::read,&f,Fixture::covered,Fixture::provider_live});f.current=&own;
      assert(f.begin(0x15fba8));f.disconnect_queue(0);f.disconnect_queue(1);f.state.active=f.state.connected=0;f.release_flags();assert(f.end(1,0x15fc20));
      f.connect(true);assert(f.gate.release(f.ticket));});t.join();f.ticket=original_ticket;f.current=&f.collector;
    assert(f.provider.fold_stock_history(f.transport));assert(f.provider.borrow_ready(f.b,f.borrow()));assert(!f.provider.transition_eligible(f.b,A::Disconnect));
}
#ifndef STOCK_RECEIPTS_FIXTURE_ONLY
int main(){positive_connect(false);positive_connect(true);warm_history();failed_warm_history();negative_and_ordering();independent_callers();std::puts("PASS stock source receipts: native-input binding/provider, warm borrow, ordered callers, sticky refusals");return 0;}
#endif
