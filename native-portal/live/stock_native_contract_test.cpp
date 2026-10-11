// SPDX-License-Identifier: MIT
// Saved coordinates derived from hash-pinned FIELD-READ-PERMISSIONS and retained
// native prologues. Native operations/memory stores are explicitly modeled here;
// this CPU fixture does not claim emitted-island/native-instruction execution.
#define STOCK_RECEIPTS_FIXTURE_ONLY
#include "stock_source_receipts_test.cpp"
#undef STOCK_RECEIPTS_FIXTURE_ONLY
#include <sys/mman.h>
#include <fstream>
using C=ClientSourceBinding::NativeReadContext;
struct BoundaryFixture {
    Fixture f;
    NativeStockReadBoundary boundary;
    arena::Saved entry=f.entry(),s{};
    C c{};
    uint64_t method=0x15fcc0;
    BoundaryFixture(bool self=false):boundary(f.gate,{self?nullptr:Fixture::read,&f,Fixture::covered,Fixture::provider_live}) {
        const auto scope=at(M::Compositor,method,entry);assert(boundary.begin(scope,Fixture::client));
        c.pending=c.entered=c.inner_entered=true;c.sp=Fixture::sp-0x360;c.id=11;c.provider=Fixture::manager;
        c.object=0x90200000;c.header=0x90300000;c.return_pc=0x160188;
        s.sp=c.sp-0xc0;s.x[20]=c.object;
    }
    StockSourceReadScope at(M m,uint64_t pc,const arena::Saved& saved) {
        return {f.b,f.ticket,1,1,method,pc,1,m,saved};
    }
    void lock() {
        auto g=s;g.sp=c.sp-0x50;g.x[0]=0;g.x[30]=Fixture::client+0x3280ec;
        assert(boundary.observe(at(M::Client,0x3280ec,g),c));
        g.sp=s.sp;g.x[0]=s.sp+0x58;g.x[30]=Fixture::client+0x327e4c;
        assert(boundary.observe(at(M::Client,0x258438,g),c));
        g.sp=s.sp-0x30;g.x[19]=s.sp+0x58;g.x[0]=1;g.x[30]=Fixture::client+0x2584c4;
        assert(boundary.observe(at(M::Client,0x2584c4,g),c));
    }
    bool read(M m,uint64_t pc,uint64_t address,size_t size) {
        uint64_t out=0;const auto scope=at(m,pc,s);
        assert(boundary.callback(scope,c));const bool ok=boundary.read(address,&out,size);boundary.leave();return ok;
    }
};
void pre_ret_is_not_completion() {
    for(uint64_t actual:{uint64_t(0),uint64_t(-7),uint64_t(1)}) {
        Fixture f;assert(f.begin(0x15fcc0));assert(f.state_reads==0 && f.reads==0);
        auto s=f.entry();s.x[0]=actual;assert(f.collector.camera_return(0x15fdc8,s));
        auto status=f.transport.status();assert(status.pending==1 && status.completed==0);
        assert(f.collector.complete_own_bypass(f.collector.invocation(),actual));
        assert(f.transport.seal());auto receipt=f.transport.take();assert(receipt && receipt->observation().actual==actual);
        assert(!receipt->native_fields().has(NativeFieldFacts::EXTERNAL));
        assert(f.provider.fold_stock_receipt(f.b,1,*receipt));assert(!f.provider.borrow_ready(f.b,f.borrow()));
        assert(f.state_reads==0 && f.reads==0);
    }
    for(unsigned failure=0;failure<5;++failure) {
        Fixture f;assert(f.begin(0x15fcc0));auto s=f.entry();s.x[0]=0;
        if(failure)assert(f.collector.camera_return(0x15fdc8,s));
        if(failure==4){assert(!f.transport.seal());continue;}
        auto call=f.collector.invocation();if(failure==1)call.method=0x15f9e0;if(failure==2)call.sp+=16;
        assert(!f.collector.complete_own_bypass(call,failure==3?1:0));
        assert(!f.transport.seal());assert(f.transport.status().completed==0);
    }
    Fixture f;
    {StockSourceCollector local(f.gate,f.transport,{Fixture::read,&f,Fixture::covered,Fixture::provider_live});
     auto s=f.entry();assert(local.begin(f.ticket,f.b,1,0x15fcc0,s,0,Fixture::client,ClientSourceBinding::compositor_sha,ClientSourceBinding::client_sha));
     s.x[0]=0;assert(local.camera_return(0x15fdc8,s));}
    auto status=f.transport.status();assert(status.abandoned==1 && status.completed==0 && status.failed);
}
void native_paths_and_current_auth() {
    for(bool dedup:{false,true}) {
        Fixture f;f.connect(dedup);assert(f.state_reads==0);
        assert(f.transport.seal());auto receipt=f.transport.take();assert(receipt);
        const auto& facts=receipt->native_fields();assert(facts.known==31 && !facts.has(NativeFieldFacts::EXTERNAL));
        assert(facts.values.client==Fixture::client && facts.values.client!=Fixture::manager);
        for(unsigned q=0;q<2;++q) {
            const auto& fact=facts.last[q+1];assert(fact.width==8 && fact.value==11+q && fact.pc==0x32a628 &&
                fact.module==M::Client && fact.path==NativeFactPath::CommittedStore && fact.binding==f.b && fact.ticket==f.ticket &&
                fact.method==0x15f9e0 && fact.runtime==1 && fact.sequence==1 && fact.version==1 && fact.sp==Fixture::sp-0xe0 &&
                fact.process==getpid() && fact.caller==std::this_thread::get_id());
        }
        f.permitted=false;const auto reads=f.reads;
        assert(f.provider.fold_stock_receipt(f.b,1,*receipt));assert(f.reads==reads && f.auth_reads==0);
        assert(f.provider.borrow_ready(f.b,f.borrow()));assert(!f.provider.transition_eligible(f.b,A::Disconnect));
    }
    for(bool starts_external:{false,true}) {
        Fixture f;f.warm();f.state.external=starts_external;
        for(unsigned invocation=0;invocation<2;++invocation) {
            assert(f.begin(0x15fcc0));f.metadata(1);f.fact(0x16013c,!f.state.external);f.state.external=!f.state.external;
            f.metadata(1,true);f.metadata(0);f.metadata(0,true);assert(f.end(0,0x15fdc8));
        }
        f.permitted=false;const auto reads=f.reads;assert(f.provider.fold_stock_history(f.transport));
        assert(f.reads==reads && f.auth_reads==0);
        f.state.external=1;assert(!f.provider.connect_eligible(f.b)); // Runtime's current external admission remains separate
        f.state.external=0;assert(f.provider.connect_eligible(f.b));assert(f.provider.borrow_ready(f.b,f.borrow()));
        assert(!f.provider.transition_eligible(f.b,A::Disconnect));
    }
    for(uint64_t pc:{0x15fbd0u,0x15fc08u}) {
        Fixture f;assert(f.begin(0x15fba8));f.fact(pc);assert(f.end(1,pc==0x15fbd0?0x15fbd8:0x15fc20));
        assert(f.transport.seal());auto receipt=f.transport.take();assert(receipt);
        assert(receipt->native_fields().has(NativeFieldFacts::EXTERNAL));
        assert(receipt->native_fields().last[5].pc==pc && receipt->native_fields().last[5].value==0 &&
            receipt->native_fields().last[5].path==NativeFactPath::CommittedStore);
        assert(!f.provider.fold_stock_receipt(f.b,1,*receipt)); // no pair is not detach
    }
}
void native_negative_facts_and_coverage() {
    for(const auto& site:NativeStockReadBoundary::required_sites) {
        Fixture f;f.missing_pc=site.pc;assert(!f.begin());assert(f.reads==0);assert(!f.transport.seal());
    }
    for(unsigned bad=0;bad<6;++bad) {
        Fixture f;assert(f.begin());auto s=f.entry();s.sp-=0x20;s.x[19]=f.b.camera;s.x[0]=1;
        if(bad==0)++s.x[19];
        if(bad==1)s.sp+=16;
        assert(!f.collector.camera_fact(bad==2?M::Client:M::Compositor,bad==3?0x15fd30:bad==4?0x15f9f8:bad==5?0x15f9fd:0x15f9fc,s));
        assert(!f.transport.seal());assert(f.reads==0);
    }
    {Fixture f;assert(f.begin());f.provider_lifetime=false;
     try{f.connect_queue(0);assert(false);}catch(const Fixture::ScopeRejected&){}
     assert(f.reads==0);assert(!f.transport.seal());}
    {Fixture f;f.connect();assert(f.begin(0x15fcc0));f.state.ids[0]=99;
     f.metadata(0);f.metadata(0,true);assert(f.end(0,0x15fdc8));assert(!f.provider.fold_stock_history(f.transport));}
}
void exact_read_allowlist() {
    BoundaryFixture b;
    // This callback PC belongs to Connect, not Update. A native-safe boundary
    // must reject before the injected backend even if register bits look valid.
    b.s.sp=Fixture::sp-0x360;b.s.x[19]=b.f.b.camera;b.s.x[0]=Fixture::manager;b.s.x[3]=Fixture::client+0x32a270;
    assert(!b.read(M::Compositor,0x15fa24,Fixture::manager,8));assert(b.f.reads==0);
    {
        Fixture f;
        const auto revoke=[](void* p,const StockSourceReadScope&,uint64_t) noexcept {
            auto& fixture=*static_cast<Fixture*>(p);return fixture.gate.release(fixture.ticket);
        };
        NativeStockReadBoundary boundary(f.gate,{Fixture::read,&f,Fixture::covered,revoke});
        auto entry=f.entry();StockSourceReadScope begin{f.b,f.ticket,1,1,0x15f9e0,0x15f9e0,1,M::Compositor,entry};
        assert(boundary.begin(begin,Fixture::client));
        auto s=entry;s.sp-=0x20;s.x[19]=f.b.camera;s.x[0]=Fixture::manager;s.x[3]=Fixture::client+0x32a270;
        StockSourceReadScope scope{f.b,f.ticket,1,1,0x15f9e0,0x15fa24,1,M::Compositor,s};
        assert(boundary.callback(scope,{}));uint64_t out=0;
        assert(!boundary.read(Fixture::manager,&out,8));boundary.leave();
        assert(f.gate.retain(f.epoch,f.ticket)); // restore only fixture teardown retention
        assert(f.reads==0); // lifetime loss must be caught BEFORE byte backend
    }
}
void scoped_reads_and_guard_lifetimes() {
    for(int mode=0;mode<3;++mode) {
        BoundaryFixture b;
        if(mode==2)b.lock();
        b.s.x[0]=b.c.header;b.f.put(b.c.header,11);
        assert(b.read(M::Client,0x327e54,b.c.header,8)==(mode==2));
        assert(b.f.reads==(mode==2?1u:0u));
    }
    {
        BoundaryFixture b;b.lock();b.s.x[0]=b.c.header;b.f.put(b.c.header,11);
        const auto before=b.f.reads;
        for(size_t size:{1u,4u,7u,9u,0x470u})assert(!b.read(M::Client,0x327e54,b.c.header,size));
        assert(!b.read(M::Client,0x327e54,b.c.header+8,8));
        assert(!b.read(M::Client,0x327e54,UINT64_MAX-3,8));
        assert(b.f.reads==before);
        // Every native ref slot, including 127. Width4, stride8, no whole header.
        for(unsigned slot=0;slot<128;++slot) {
            const auto address=b.c.header+0x68+8*slot;b.f.put(address,2,4);
            b.s.x[19]=address-0x68;assert(b.read(M::Client,0x327f70,address,4));
            b.c.ref=address;assert(b.read(M::Client,0x327f74,address,4));
        }
        const auto reads=b.f.reads;b.s.x[19]=b.c.header+0x400;
        assert(!b.read(M::Client,0x327f70,b.c.header+0x468,4));
        b.s.x[19]=b.c.header+1;assert(!b.read(M::Client,0x327f70,b.c.header+0x69,4));
        b.s.x[19]=UINT64_MAX;assert(!b.read(M::Client,0x327f70,b.c.header+0x68,4));
        assert(!b.read(M::Client,0x327f70,b.c.header+0x68,8));
        assert(b.f.reads==reads);
        auto g=b.s;g.x[0]=g.sp+0x58;g.x[30]=Fixture::client+0x327f80;
        assert(b.boundary.observe(b.at(M::Client,0x258508,g),b.c));
        b.s.x[0]=b.c.header;assert(!b.read(M::Client,0x327e54,b.c.header,8));
        uint64_t out=0;assert(!b.boundary.read(b.c.header,&out,8));assert(b.f.reads==reads);
    }
    for(unsigned bad=0;bad<9;++bad) {
        BoundaryFixture b;b.lock();auto c=b.c;auto g=b.s;
        if(bad==0)++b.c.queue;
        if(bad==1)++b.c.id;
        if(bad==2)++b.c.provider;
        if(bad==3){++b.c.object;++b.s.x[20];}
        if(bad<4) {b.s.x[0]=b.c.header;b.f.put(b.c.header,11);assert(!b.read(M::Client,0x327e54,b.c.header,8));}
        else if(bad==4) {g.x[0]=g.sp+0x58;g.x[30]=Fixture::client+0x327e4c;assert(!b.boundary.observe(b.at(M::Client,0x258438,g),c));}
        else if(bad==5) {g.sp-=0x30;g.x[19]=b.s.sp+0x58;g.x[0]=1;g.x[30]=Fixture::client+0x2584c4;assert(!b.boundary.observe(b.at(M::Client,0x2584c4,g),c));}
        else {g.x[0]=g.sp+0x58;g.x[30]=Fixture::client+0x327f80;
            if(bad==6)g.sp+=16;
            if(bad==7)++g.x[0];
            if(bad==8)++g.x[30];
            assert(!b.boundary.observe(b.at(M::Client,0x258508,g),c));}
        assert(b.f.reads==0);
    }
    // Exact raw manager result and exact shared bool are independent.
    for(unsigned bad=0;bad<7;++bad) {
        BoundaryFixture b;auto g=b.s;g.sp=b.c.sp-0x50;g.x[0]=0;g.x[30]=Fixture::client+0x3280ec;
        if(bad==0){g.x[0]=1;assert(!b.boundary.observe(b.at(M::Client,0x3280ec,g),b.c));continue;}
        assert(b.boundary.observe(b.at(M::Client,0x3280ec,g),b.c));
        g.sp=b.s.sp;g.x[0]=g.sp+0x58;g.x[30]=Fixture::client+0x327e4c;
        if(bad==1){++g.x[0];assert(!b.boundary.observe(b.at(M::Client,0x258438,g),b.c));continue;}
        assert(b.boundary.observe(b.at(M::Client,0x258438,g),b.c));
        g.sp=b.s.sp-0x30;g.x[19]=b.s.sp+0x58;g.x[0]=bad==2?0:1;g.x[30]=Fixture::client+0x2584c4;
        if(bad==3)++g.x[19];
        if(bad==4)g.sp+=16;
        if(bad==5)++g.x[30];
        if(bad!=6)assert(b.boundary.observe(b.at(M::Client,0x2584c4,g),b.c)==(bad==2));
        b.s.x[0]=b.c.header;b.f.put(b.c.header,11);assert(!b.read(M::Client,0x327e54,b.c.header,8));assert(b.f.reads==0);
    }
    // Default read_self is fault-contained, including a real protected boundary.
    {
        const size_t page=size_t(sysconf(_SC_PAGESIZE));
        auto* pages=static_cast<unsigned char*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
        assert(pages!=MAP_FAILED);assert(mprotect(pages+page,page,PROT_NONE)==0);
        uint64_t id=11;std::memcpy(pages+page-8,&id,8);
        BoundaryFixture b(true);b.lock();b.c.header=reinterpret_cast<uint64_t>(pages+page-8);b.s.x[0]=b.c.header;
        assert(b.read(M::Client,0x327e54,b.c.header,8));
        b.c.header+=4;b.s.x[0]=b.c.header;assert(!b.read(M::Client,0x327e54,b.c.header,8));
        b.c.header+=4;b.s.x[0]=b.c.header;assert(!b.read(M::Client,0x327e54,b.c.header,8));
        assert(munmap(pages,page*2)==0);
    }
}
void replay_native_vectors(const char* path) {
    struct Vector {uint64_t method,module,pc,field,value,width;arena::Saved saved;};
    static_assert(sizeof(Vector)==848);
    std::ifstream input(path,std::ios::binary);assert(input);Vector v{};size_t count=0;
    while(input.read(reinterpret_cast<char*>(&v),sizeof(v))) {
        Fixture f;assert(f.begin(v.method));
        if(v.module) {
            assert(v.pc==0x32a628);f.native_output_vector=&v.saved;f.connect_queue(0);f.connect_queue(1);
        } else assert(f.collector.camera_fact(M::Compositor,v.pc,v.saved));
        const uint64_t ret=v.method==0x15f9e0?0x15faa4:v.method==0x15fba8?0x15fc20:0x15fdc8;
        assert(f.end(0,ret));assert(f.transport.seal());auto receipt=f.transport.take();assert(receipt);
        assert(receipt->native_fields().has(uint8_t(v.field)));unsigned index=0;
        while((1u<<index)!=v.field){assert(index<5);++index;}
        const auto& fact=receipt->native_fields().last[index];
        assert(fact.pc==v.pc && fact.width==v.width && fact.value==v.value && fact.sp==v.saved.sp &&
               fact.lr==v.saved.x[30] && fact.method==v.method && fact.module==(v.module?M::Client:M::Compositor));
        ++count;
    }
    assert(input.eof() && input.gcount()==0 && count>0);
    std::printf("PASS replayed %zu native-executed full Saved vectors through real collector\n",count);
}
void stale_completion_does_not_match_reused_sp() {
    Fixture f;assert(f.begin(0x15fcc0));const auto first_call=f.collector.invocation();
    auto first=f.entry();first.x[0]=0;assert(f.end(0,0x15fdc8));
    assert(f.begin(0x15fcc0));assert(f.collector.camera_return(0x15fdc8,first));
    // A completion from the earlier bypass is not the second invocation.
    assert(!f.collector.complete_own_bypass(first_call,0));
}
int main(int argc,char** argv) {
    if(argc==2 && !std::strcmp(argv[1],"--inventory")) {
        for(const auto& site:NativeStockReadBoundary::required_sites)
            std::printf("%s 0x%llx\n",site.module==M::Client?"client":"compositor",static_cast<unsigned long long>(site.pc));
        return 0;
    }
    if(argc==2){replay_native_vectors(argv[1]);return 0;}
    stale_completion_does_not_match_reused_sp();
    pre_ret_is_not_completion();native_paths_and_current_auth();native_negative_facts_and_coverage();exact_read_allowlist();
    scoped_reads_and_guard_lifetimes();
    std::puts("PASS native contract: field provenance, own completion, masked fold and scoped read allowlist");
}
