// SPDX-License-Identifier: MIT
// Real native-register collector -> Evidence -> unchanged Adapter/Command chain.
// CPU coverage declaration and event schedule are modeled. No installed ABI claim.
#define FT_STOCK_TEST_LIBRARY
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "stock_facts_test.cpp"
#pragma GCC diagnostic pop
#include "native_stock_facts.hpp"
#ifndef FT_NATIVE_STOCK_SOURCE_MODEL
#include "native-stock-witness.hpp"
#endif
#include <vector>

struct NativeFixture:StockFixture {
    NativeStockFactCollector collector;
    uint64_t skip=0,corrupt=0;unsigned lane=0;bool corrupted=false;
    uint64_t pool_result=1;bool failed_pool=false;
    NativeFixture():StockFixture(false) {
        stock_mode=true;size_t count=0;const auto *sites=NativeStockFactCollector::required_sites(count);
        assert(collector.install_coverage(sites,count)); // explicitly modeled loader coverage
        assert(evidence.begin_dormant_stock(collector.reader()));
    }
    bool emit(uint64_t pc,bool consume=true,arena::Bridge *bridge=nullptr) {
        if(pc==skip)return true;
        auto actual=s;
        if(pc==corrupt) {if(lane==31)++actual.sp;else ++actual.x[lane];corrupted=true;}
        const auto original=actual;const auto before=reads;
        const bool captured=collector.capture(pc,actual);
        assert(!std::memcmp(&actual,&original,sizeof actual) && reads==before);
        const bool accepted=bridge?evidence.route(*bridge,pc,actual):evidence.event(pc,actual);
        collector.finish();
        return captured && (!consume || accepted);
    }
    bool create(uint64_t C,uint64_t vk) {
        // SP and LR follow the saved 3017b0/303ce0/300a08 instruction texts.
        s={};s.x[0]=s.x[20]=C;s.x[1]=addr(r)+0x50;s.x[2]=s.x[19]=addr(pool);s.sp=addr(stack)+0x100;
        s.x[3]=0x50000000;s.x[30]=base+0x3017f0;if(!emit(0x301800,false))return false;
        s.sp-=0xa0;s.x[19]=C;s.x[30]=base+0x301804;if(!emit(0x303d10,false))return false;
        s.x[22]=addr(pool);s.x[30]=base+0x303d38;if(!emit(0x303d70,false))return false;
        s.sp+=0xa0;s.x[20]=C;s.x[30]=base+0x301804;if(!emit(0x301804,false))return false;
        s.x[0]=C;if(!emit(0x301808))return false;
        s.sp-=0x40;s.x[0]=s.x[19]=C;s.x[30]=base+0x300ae0;if(!emit(0x300a08))return false;
        s.sp-=0x30;s.x[0]=vk;if(!emit(0x300a28))return false;
        s.x[0]=0;s.x[30]=base+0x300a38;if(!emit(0x300a38))return false;
        s.sp=addr(stack)+0x100;s.x[20]=C;s.x[0]=1;s.x[30]=base+0x30180c;
        return emit(0x30180c);
    }
    bool bootstrap(bool recording=false,arena::Bridge *bridge=nullptr,unsigned pooled=0) {
        assert(recording);assert(evidence.pin(base,elf_sha256));
        assert(emit(0x7fda0,true,bridge));
        s.x[19]=addr(r);s.x[0]=addr(r)+0xec0;s.x[1]=bridge?0x80000:0x80080;s.x[8]=s.sp+0x140;
        assert(emit(0x29a14c,true,bridge));put(stack,0x140,addr(resource));assert(emit(0x29a150,true,bridge));put(stack,0x140,uint64_t(0));
        assert(create(addr(executable),0xc001));if(pooled)assert(create(addr(other),0xc002));
        s={};s.sp=addr(stack);s.x[19]=addr(r);s.x[1]=addr(command);assert(emit(0x29a528));
        s.x[24]=addr(r);s.x[0]=1;return emit(0x7fdbc,true,bridge);
    }
    void run() {StockFixture::run();assert(collector.handoff(evidence));}
    bool factory(uint64_t W,uint64_t C) {
        const auto sp=addr(stack)+0x100;
#ifdef FT_NATIVE_STOCK_SOURCE_MODEL
        // Explicit complete contract model for source-only test environments.
        // Never exported or described as instruction-generated Saved vectors.
        bool ok=true;
        wrapper_factory_model(base,addr(r),W,C,addr(pool),sp,[&](uint64_t pc,arena::Saved call) {
            if(pc==0x2a23f4 || pc==0x2a23f8 || (failed_pool && pc==0x304ec8))return;
            s=call;if(ok)ok=emit(pc,false);
        });
        return ok;
#else
        // Relocate synthetic object identities, not the observed ABI lanes.
        // PC/SP/LR and all register values originate in the pinned-text witness.
        auto relocate=[&](uint64_t v) {
            if(v>=0x100000 && v<0x700000)return base+(v-0x100000);
            if(v>=0x1100000 && v<0x1101000)return W+(v-0x1100000);
            if(v>=0x1200000 && v<0x1201000)return C+(v-0x1200000);
            if(v>=0x1300000 && v<0x1301000)return addr(pool)+(v-0x1300000);
            if(v>=0x13ff000 && v<0x1401000)return sp+(v-0x1400000);
            return v;
        };
        const auto *vectors=failed_pool?failure_factory_vectors:factory_vectors;
        const size_t count=failed_pool?std::size(failure_factory_vectors):std::size(factory_vectors);
        uint64_t factory_owner=0;
        for(size_t index=0;index<count;++index) {
            const auto &v=vectors[index];
            s={};s.sp=relocate(v.sp);for(unsigned i=0;i<31;++i)s.x[i]=relocate(v.x[i]);
            // The saved factory-only witness did not execute the outer caller.
            // These caller-context lanes are explicit contract models, not
            // additional instruction-generated records or a complete replay.
            if(v.pc==0x3053f8){factory_owner=s.x[0];s.x[19]=addr(r);s.x[20]=factory_owner;}
            if(v.pc==0x302790){s.x[19]=W;s.x[3]=factory_owner;}
            if(!emit(v.pc,false))return false;
        }
        return true;
#endif
    }
    bool turn(uint64_t W,uint64_t C,uint64_t nextW,unsigned fault=0) {
        assert(!fault);
        const uint64_t nextC=nextW==addr(next)?addr(other):addr(executable);
        s={};s.sp=addr(stack)+0x1d0;s.x[0]=addr(r);s.x[30]=base+0x6caf0;
        if(!emit(0x2a1e18))return false;
        s.sp-=0xd0;s.x[19]=addr(r);s.x[1]=W;if(!emit(0x2a1e3c))return false;
        // No 2a1f64 here: native can bypass that branch. No invented C load.
        if(corrupt==0x2a1f64) {s.x[20]=C;s.x[2]=W;if(!emit(0x2a1f64))return false;}
        if(!emit(0x2a2230) || !emit(0x2a2320))return false;
        put(r,0x1380,uint64_t(0));s.x[0]=s.x[20]=W;s.x[30]=base+0x2a23b4;if(!emit(0x305260))return false;
        s.sp-=0x30;s.x[19]=W;s.x[1]=C;if(!emit(0x3052e0))return false;
        s.x[0]=addr(pool)+0xd0;if(!emit(0x3052ec))return false;
        s.x[30]=base+0x3052f0;if(!emit(0x311648))return false;
        s.x[0]=pool_result;if(!emit(0x3052f0))return false;
        s.sp+=0x30;s.x[0]=s.x[20]=W;s.x[19]=addr(r);s.x[1]=0x10;s.x[30]=base+0x2a23c0;
        if(!emit(0x4b3d0))return false;
        if(!factory(nextW,nextC))return false;
        s.x[19]=addr(r);s.x[0]=nextW;s.x[20]=0;
        assert(!evidence.event(0x2a23f4,s)); // cannot publish before store
        put(r,0x1380,nextW);return emit(0x2a23f8);
    }
    bool recycle_native(uint64_t C,unsigned fault=0) {
        s={};s.sp=addr(stack)+0x100;s.x[0]=s.x[19]=C;s.x[20]=addr(pool);s.x[30]=base+0x2a9cdc;
        if(fault==1)++s.x[30];
        if(!emit(0x300b20))return false;
        s.sp-=0x20;s.x[0]=fault==2?uint32_t(-4):0;s.x[30]=base+0x300c54;if(!emit(0x300c54))return false;
        s.sp+=0x20;s.x[0]=C;s.x[30]=base+0x2a9cdc;if(!emit(0x300a08))return false;
        s.sp-=0x30;s.x[0]=C==addr(executable)?0xc001:0xc002;if(fault==4)++s.x[0];if(!emit(0x300a28))return false;
        s.x[0]=fault==3?uint32_t(-4):0;s.x[30]=base+0x300a38;return emit(0x300a38);
    }
    void recycle(uint64_t C,unsigned fault=0) {
        if(!stock_mode) {StockFixture::recycle(C,fault);return;}
        assert(recycle_native(C,fault));
    }
};
static void positive_and_reads() {
    auto f=std::make_unique<NativeFixture>();assert(f->bootstrap(true,nullptr,1));
    f->s={};f->s.x[0]=addr(f->pool)+0xd0;f->s.x[1]=addr(f->other);f->s.x[30]=Fixture::base+0x29a8b8;
    assert(f->emit(0x311648));assert(f->recycle_native(addr(f->other)));f->run();
    auto *guard=mmap(nullptr,4096,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(guard!=MAP_FAILED);
    put(f->r,0x15a0,uint64_t(guard));put(f->r,0x15a8,uint64_t(guard)+0x98);
    put(f->r,0x15b8,uint64_t(guard));put(f->r,0x15c0,uint64_t(guard)+0x98);
    forbidden_begin=addr(f->resource);forbidden_end=forbidden_begin+f->resource.size();
    graph_begin=uint64_t(guard);graph_end=graph_begin+4096;forbidden_reads=0;const auto before=reads;
    for(unsigned i=0;i<8;++i) {
        const bool odd=i&1;
        assert(f->turn(odd?addr(f->next):addr(f->command),odd?addr(f->other):addr(f->executable),odd?addr(f->command):addr(f->next)));
        assert(f->recycle_native(odd?addr(f->other):addr(f->executable)));
    }
    const auto facts=f->evidence.stock_command_facts();assert(facts.command==addr(f->executable) && facts.generation>1);
    assert(f->collector.complete());assert(f->evidence.engage_stock(f->evidence.identity(),facts.generation));
    assert(reads==before && forbidden_reads==0);
    forbidden_begin=forbidden_end=graph_begin=graph_end=0;assert(!munmap(guard,4096));
}
static void collector_negatives() {
    // Coverage is not inferred from seeing one good sequence.
    size_t n=0;const auto *required=NativeStockFactCollector::required_sites(n);
    for(size_t omitted=0;omitted<n;++omitted) {
        NativeStockFactCollector c;std::vector<uint64_t> sites(required,required+n);sites.erase(sites.begin()+omitted);
        assert(!c.install_coverage(sites.data(),sites.size()));assert(!c.complete());
    }
    for(uint64_t omitted:{0x301800u,0x303d10u,0x303d70u,0x301804u,0x300a28u,0x300a38u}) {
        NativeFixture f;assert(f.evidence.pin(Fixture::base,elf_sha256));assert(f.emit(0x7fda0));f.skip=omitted;
        assert(!f.create(addr(f.executable),0xc001));assert(!f.collector.complete());
    }
    for(auto fault:std::array<std::pair<uint64_t,unsigned>,8>{{{0x301800,1},{0x301800,19},{0x303d10,1},{0x303d70,22},{0x301804,30},{0x300a28,30},{0x300a28,31},{0x30180c,30}}}) {
        NativeFixture f;assert(f.evidence.pin(Fixture::base,elf_sha256));assert(f.emit(0x7fda0));f.corrupt=fault.first;f.lane=fault.second;
        assert(!f.create(addr(f.executable),0xc001));assert(f.corrupted && !f.collector.complete());
    }
    for(uint64_t omitted:{0x4b3d0u,0x304ec8u,0x2a1e3cu,0x3052e0u,0x3052ecu,0x3052f0u,0x3053f8u,0x305308u,0x302790u,0x304e60u,0x304eccu,0x3053a8u,0x305490u,0x305470u}) {
        auto f=std::make_unique<NativeFixture>();assert(f->bootstrap(true,nullptr,1));f->run();f->skip=omitted;
        assert(!f->turn(addr(f->command),addr(f->executable),addr(f->next)));assert(!f->collector.complete());
        assert(!f->evidence.engage_stock(f->evidence.identity(),f->evidence.stock_command_facts().generation));
    }
    for(auto fault:std::array<std::pair<uint64_t,unsigned>,17>{{{0x305308,2},{0x302790,1},{0x2a1e18,30},{0x2a1e3c,1},{0x2a1e3c,31},{0x2a1f64,20},{0x3052e0,1},{0x3052ec,0},{0x305308,30},{0x302790,8},{0x304ecc,22},{0x304ecc,31},{0x305490,22},{0x305470,0},{0x305470,31},{0x2a23f8,0},{0x2a23f8,31}}}) {
        auto f=std::make_unique<NativeFixture>();assert(f->bootstrap(true,nullptr,1));f->run();f->corrupt=fault.first;f->lane=fault.second;
        assert(!f->turn(addr(f->command),addr(f->executable),addr(f->next)));assert(f->corrupted && !f->collector.complete());
    }
    for(unsigned fault=1;fault<=4;++fault) {
        auto f=std::make_unique<NativeFixture>();assert(f->bootstrap(true,nullptr,1));f->run();
        assert(f->turn(addr(f->command),addr(f->executable),addr(f->next)));
        assert(!f->recycle_native(addr(f->executable),fault));assert(!f->collector.complete());
    }
    for(uint64_t raw:{0u,2u,256u}) {
        auto f=std::make_unique<NativeFixture>();assert(f->bootstrap(true,nullptr,1));f->run();f->pool_result=raw;
        assert(!f->turn(addr(f->command),addr(f->executable),addr(f->next)));assert(!f->collector.complete());
    }
    {
        auto f=std::make_unique<NativeFixture>();assert(f->bootstrap(true,nullptr,1));f->run();f->failed_pool=true;
        assert(!f->turn(addr(f->command),addr(f->executable),addr(f->next)));
        assert(f->collector.failure_pc()==0x304ecc); // executed failure304f9c/304fa0 bypass
    }
    {
        auto f=std::make_unique<NativeFixture>();assert(f->bootstrap(true,nullptr,1));f->run();
        const auto before=reads;uint64_t value=0xfeed;auto reader=f->collector.reader();
        assert(!reader.scalar(reader.user,0x2a23f8,addr(f->executable)+8,value));assert(value==0xfeed && reads==before);
        assert(!f->evidence.engage_stock(f->evidence.identity(),f->evidence.stock_command_facts().generation));
    }
    {
        auto f=std::make_unique<NativeFixture>();assert(f->bootstrap(true,nullptr,1));f->run();
        f->s={};f->s.x[0]=addr(f->executable);assert(!f->emit(0x3003d0));
        assert(!f->create(addr(f->executable),0xc001)); // reused live address cannot resurrect
    }
    {
        auto f=std::make_unique<NativeFixture>();assert(f->bootstrap(true,nullptr,1));f->run();
        std::thread foreign([&] {arena::Saved s{};assert(!f->collector.capture(0x2a1e18,s));});foreign.join();
        assert(!f->collector.complete());
    }
    {
        auto f=std::make_unique<NativeFixture>();assert(f->bootstrap(true,nullptr,1));f->run();
        const auto child=fork();assert(child>=0);
        if(!child) {arena::Saved s{};_exit(f->collector.capture(0x2a1e18,s)?1:0);}
        int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
        assert(f->collector.complete());
    }
    {
        NativeFixture f;assert(f.evidence.pin(Fixture::base,elf_sha256));assert(f.emit(0x7fda0));
        for(unsigned i=0;i<64;++i)assert(f.create(0x40000000+uint64_t(i)*0x1000,0xc100+i));
        assert(!f.create(0x40040000,0xc140)); // bounded capacity, not address-based overwrite
    }
    {
        NativeFixture f;assert(f.evidence.pin(Fixture::base,elf_sha256));assert(f.emit(0x7fda0));
        for(unsigned i=0;i<128;++i) {
            assert(f.create(0x40000000,0xc100));
            f.s={};f.s.x[0]=0x40000000;assert(f.emit(0x3003d0));
        }
    }
    {
        auto f=std::make_unique<NativeFixture>();assert(f->bootstrap(true,nullptr,1));f->run();f->skip=0x2a23f8;
        assert(f->turn(addr(f->command),addr(f->executable),addr(f->next)));
        assert(!f->collector.complete());assert(!f->evidence.engage_stock(f->evidence.identity(),1));
    }
    {
        auto f=std::make_unique<NativeFixture>();assert(f->bootstrap(true,nullptr,1));f->run();
        f->s={};assert(!f->emit(0x305470,false)); // orphan actual RET is not factory evidence
    }
}
static void bootstrap_call_scope() {
    NativeStockFactCollector c;size_t n=0;const auto *sites=NativeStockFactCollector::required_sites(n);assert(c.install_coverage(sites,n));
    auto reader=c.reader();render::Identity identity{};identity.process_instance=getpid();identity.renderer=0x1000000;identity.renderer_generation=1;
    assert(!reader.bootstrap(reader.user,Fixture::base,identity,0,0,0) && "qualified bootstrap still needs its actual callback scope");
}
static void real_thread_handoff() {
    auto f=std::make_unique<NativeFixture>();assert(f->bootstrap(true,nullptr,1));
    put(f->worker,0,uint64_t(Fixture::base+0x5a1428));put(f->worker,0x48,addr(f->context_full));
    put(f->context_full,0x98,addr(f->worker));put(f->context_full,0x1c0,addr(f->scheduler));
    put(f->scheduler,8,addr(f->app));put(f->app,0x256a0,addr(f->r));
    f->s={};f->s.x[19]=addr(f->worker);f->s.x[21]=addr(f->context_full);assert(f->evidence.owner_event(0x14099c,f->s));
    f->s.x[0]=addr(f->worker);assert(f->evidence.owner_event(0x140dbc,f->s));
    std::thread worker([&] {
        put(f->worker,0x10,uint32_t(syscall(SYS_gettid)));put(f->worker,0x14,uint32_t(0x80000000));
        f->s.x[30]=Fixture::base+0x182aa8;assert(f->evidence.owner_event(0x14e5a8,f->s));
        assert(f->collector.handoff(f->evidence));
        assert(f->turn(addr(f->command),addr(f->executable),addr(f->next)));
        assert(f->recycle_native(addr(f->executable)));
        const auto facts=f->evidence.stock_command_facts();assert(f->evidence.engage_stock(f->evidence.identity(),facts.generation));
    });worker.join();
}
int main() {
    bootstrap_call_scope();positive_and_reads();collector_negatives();real_thread_handoff();recovery<NativeFixture>();
    std::puts("native stock facts: register-derived creation/pool reuse/turnover -> engagement -> real Adapter/Command/Lifecycle PASS; modeled coverage and callback schedule");
}
