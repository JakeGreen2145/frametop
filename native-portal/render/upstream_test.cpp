// SPDX-License-Identifier: MIT
#define main prior_main
#include "test.cpp"
#undef main
#include "../arena/bridge.hpp"
#include <cstdlib>
#include <new>
static std::atomic<size_t> hot_allocations{0};
void* operator new(size_t n){++hot_allocations;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,size_t) noexcept{std::free(p);}
void operator delete[](void* p,size_t) noexcept{std::free(p);}
namespace ar = native_portal::arena;
struct UpstreamNative : Fake {
    std::array<unsigned char,0x2000> renderer{};
    std::array<unsigned char,0x1000> camera{};
    std::array<unsigned char,0xc0> arena_resource{}, owned_resource{}, stock_resource{};
    std::array<unsigned char,0x20> owned_wrapper{}, stock_wrapper{};
    ar::Bridge bridge;
    Address allocation_slot=0;
    bool settled_ok=false;
    unsigned native_uploads=0;
    Adapter* off_during_staging=nullptr;bool expire_during_staging=false;
    static bool read(void*,uint64_t a,void* b,size_t n) noexcept {std::memcpy(b,(void*)a,n);return true;}
    static bool write(void* u,uint64_t a,const void* b,size_t n) noexcept {
        auto& f=*static_cast<UpstreamNative*>(u);std::memcpy((void*)a,b,n);
        if(f.off_during_staging)f.off_during_staging->off();
        if(f.expire_during_staging)now=100;
        return true;
    }
    static bool coherent(void*,uint64_t) noexcept {return true;}
    template<class A> static Address addr(A& a) {return reinterpret_cast<Address>(a.data());}
    UpstreamNative():bridge({this,read,write,coherent}) {
        binding.identity.renderer=addr(renderer); binding.headroom_evidence=0;
        binding.upstream=&bridge; binding.legacy_test_upload=false;
        wrapper=addr(owned_wrapper);
        obs.wrapper_resource=addr(owned_resource);
        obs.arena.resource=addr(arena_resource); obs.arena.capacity=ar::Capacity;
        put(owned_wrapper,8,obs.wrapper_resource); put(owned_wrapper,0x18,uint32_t{UINT32_MAX});
        put(owned_resource,0xb8,uint32_t{592}); put(stock_wrapper,8,addr(stock_resource));
        put(stock_resource,0xb8,uint32_t{528});
        put(camera,8,binding.identity.renderer); put(camera,0x2d0,addr(stock_wrapper));
        put(renderer,0x1390,obs.arena.resource); put(renderer,0x1410,obs.arena.map);
        put(renderer,0x328,uint64_t{64}); put(renderer,0x1380,obs.arena.command_wrapper);
        put(renderer,0x15a0,obs.arena.pass_begin); put(renderer,0x15a8,obs.arena.pass_end);
        put(arena_resource,0xb0,obs.arena.buffer); put(arena_resource,0xb8,uint32_t{ar::Capacity});
        assert(bridge.arm({binding.identity.renderer,1,2,99,0},true));
        ar::Saved s{};s.x[19]=binding.identity.renderer;s.x[0]=99;s.x[1]=ar::StockBudget;
        s.x[8]=reinterpret_cast<Address>(&allocation_slot);bridge.dispatch(s,ar::Allocate,1,2);
        allocation_slot=obs.arena.resource;bridge.dispatch(s,ar::AllocationReturn,1,2);
        s={};s.x[24]=binding.identity.renderer;s.x[0]=1;bridge.dispatch(s,ar::Initialized,1,2);
        assert(bridge.ready());
    }
    static bool observe_new(void* u,const Binding& b,Address w,Address m,Observation& o) noexcept {
        auto& f=*static_cast<UpstreamNative*>(u);
        f.obs.arena.cursor=get<uint32_t>(f.renderer,0x1418);
        f.obs.wrapper_offset=get<uint32_t>(f.owned_wrapper,0x18);
        return Fake::observe(u,b,w,m,o);
    }
    static bool settled_new(void* u,const Binding&,const Provenance&,const Provenance&,const Completion&) noexcept {
        return static_cast<UpstreamNative*>(u)->settled_ok;
    }
    Callbacks callbacks() {auto c=Fake::callbacks();c.observe=observe_new;c.headroom=nullptr;c.settled=settled_new;return c;}
    void reset() {ar::Saved s{};s.x[22]=binding.identity.renderer;bridge.dispatch(s,ar::Reset,1,2);put(renderer,0x1418,uint32_t{0});++obs.arena.recording_generation;uploads=enqueues=0;}
    ar::Saved upload(Adapter& a,bool finish=true, unsigned interrupt=0) {
        ar::Saved s{};s.x[0]=binding.identity.renderer;s.x[1]=addr(stock_wrapper);
        s.x[2]=addr(camera)+0xc0;s.x[7]=0x2993f8;s.x[27]=addr(camera);
        a.upstream(s,ar::Upload,1,2);
        if(s.pad==ar::Replacement) {
            ++native_uploads;
            ar::Saved admission{};admission.x[19]=binding.identity.renderer;
            admission.x[0]=get<uint32_t>(renderer,0x1418);admission.x[20]=592;
            admission.x[21]=wrapper+0x18;admission.x[25]=s.x[2];admission.x[23]=0x808;admission.x[24]=8;
            a.upstream(admission,ar::Admission,1,2);assert(admission.pad==ar::Replacement);
            if(!finish)return s;
            Registers r{0x78,{binding.identity.renderer,wrapper,s.x[2]}};
            assert(Fake::call(this,r));
            put(renderer,0x1418,uint32_t(obs.arena.cursor));put(owned_wrapper,0x18,uint32_t(obs.wrapper_offset));
        }
        if(interrupt==1)a.off();
        if(interrupt==2)now=100;
        a.upstream(s,ar::UploadReturn,1,2);return s;
    }
};
struct UpstreamFixture {
    Fixture source;
    UpstreamNative native;
    Adapter adapter{source.control,source.gate,native.binding,native.callbacks(),2};
    Frame frame;
    UpstreamFixture(){assert(adapter.initialize());begin(1);}
    void begin(uint64_t id) {now=10;source.lease.revocation=source.control.revocation();assert(source.control.publish_lease(source.lease));PolicyInput p;p.enabled=true;frame=source.gate.begin(id,p,true,true,false,now);assert(adapter.latch(frame));}
    EyeInput eye(unsigned e){return source.eye(e);}
    uint64_t reserve(EyeInput in) {std::memcpy(native.camera.data()+0xc0,in.constants.data(),528);return adapter.reserve_upload(in,UpstreamNative::addr(native.camera),UpstreamNative::addr(native.stock_wrapper));}
    void record(unsigned e) {auto in=eye(e);auto t=reserve(in);assert(t);native.upload(adapter);assert(adapter.enqueue(t,in));}
    Registers lookup(){auto r=source.lookup();r.x[0]=native.binding.identity.renderer;return r;}
    Context context(){return {native.binding.identity,native.obs.arena.recording_generation};}
    void commit() {assert(adapter.outer_return());assert(prepare_commit(adapter,native,context(),lookup()));}
};
#ifndef FT_UPSTREAM_FIXTURE_ONLY
int main(int argc,char** argv) {
    (void)argc;(void)argv;
    for(unsigned order=0;order<2;++order) {
        UpstreamFixture f;f.record(order);f.record(1-order);
        assert(f.native.native_uploads==2 && f.native.uploads==2 && f.native.enqueues==2);
        assert(f.native.bridge.policy().actual()==1280);
        f.commit();f.adapter.off();
        for(unsigned i=0;i<2;++i)assert(f.adapter.backend(f.native.records[i],f.context(),f.lookup()).decision==Decision::Portal);
        assert(!f.adapter.submitted() && f.adapter.state().stereo_submitted);
        f.native.complete=true;assert(f.adapter.poll_completion());assert(!f.adapter.reclaim());
        f.native.settled_ok=true;assert(f.adapter.reclaim());
    }
    for(unsigned boundary=0;boundary<3;++boundary)for(bool expire:{false,true}) {
        UpstreamFixture f;auto in=f.eye(0);auto t=f.reserve(in);assert(t);
        if(boundary==0){if(expire)now=100;else f.adapter.off();}
        f.native.upload(f.adapter,true,boundary==1?(expire?2:1):0);
        if(boundary==2){if(expire)now=100;else f.adapter.off();}
        assert(!f.adapter.enqueue(t,in));assert(!f.native.enqueues);
        if(boundary)assert(f.adapter.state().pending);
        assert(!f.adapter.reclaim());
    }
    {UpstreamFixture f;auto in=f.eye(0);auto t=f.reserve(in);assert(t);f.native.upload(f.adapter,false);assert(f.adapter.state().pending);assert(f.native.bridge.policy().actual()==640);assert(!f.adapter.enqueue(t,in));assert(!f.adapter.retire());}
    {UpstreamFixture f;f.record(0);auto in=f.eye(1);auto t=f.reserve(in);assert(t);f.adapter.off();f.native.upload(f.adapter);assert(!f.adapter.enqueue(t,in));assert(f.adapter.state().pending && f.native.enqueues==1);}
    {UpstreamFixture f;auto in=f.eye(0);auto t=f.reserve(in);assert(t);f.native.upload(f.adapter);assert(f.adapter.enqueue(t,in));assert(!f.adapter.enqueue(t,in));assert(f.native.enqueues==1 && f.native.uploads==1);}
    for(unsigned mode=0;mode<4;++mode) {UpstreamFixture f;auto in=f.eye(0);auto t=f.reserve(in);assert(t);if(mode==0)++f.native.obs.arena.recording_generation;if(mode==1)++f.native.obs.arena.command;if(mode==2)f.native.camera[0xc0+64]^=1;f.native.upload(f.adapter);if(mode==3)in.eye=1;assert(!f.adapter.enqueue(t,in));assert(!f.native.enqueues);}
    for(bool expire:{false,true}) {
        UpstreamFixture f;auto in=f.eye(0);auto t=f.reserve(in);assert(t);
        if(expire)f.native.expire_during_staging=true;else f.native.off_during_staging=&f.adapter;
        f.native.upload(f.adapter);assert(!f.adapter.enqueue(t,in));
        assert(!f.native.native_uploads && !f.native.enqueues);
    }
    for(unsigned failing_eye=0;failing_eye<2;++failing_eye) {
        UpstreamFixture f;f.record(0);f.record(1);assert(f.adapter.outer_return());
        f.native.fail_pipeline_eye=int(failing_eye);
        assert(!prepare_commit(f.adapter,f.native,f.context(),f.lookup()));
        for(unsigned e=0;e<2;++e)assert(f.adapter.backend(f.native.records[e],f.context(),f.lookup()).decision==Decision::Drop);
        assert(f.adapter.state().pending && !f.adapter.state().cpu_done);
    }
    for(unsigned stage=0;stage<3;++stage) {
        UpstreamFixture f;auto in=f.eye(0);auto t=f.reserve(in);assert(t);
        if(stage==0)f.native.wrong_descriptor=true;
        f.native.upload(f.adapter);
        if(stage==1)put(f.native.owned_wrapper,0x18,uint32_t{64});
        if(stage==2)++t;
        assert(!f.adapter.enqueue(t,in));assert(!f.native.enqueues && f.native.uploads==1);
    }
    // Native constants finish after reservation, but M1/M2 cannot change. The
    // cloned native packet prefix is deliberately different at late enqueue.
    {UpstreamFixture f;auto in=f.eye(0);auto t=f.reserve(in);assert(t);
     f.native.camera[0xc0+8]^=0x55;in.prefix[200]^=std::byte{0x66};
     f.native.upload(f.adapter);assert(f.adapter.enqueue(t,in));
     assert(f.native.records[0].prefix[200]==in.prefix[200]);
     assert(!std::memcmp(f.native.payloads[0].data(),f.native.camera.data()+0xc0,528));}
    // Reclamation/queue disposal must not reset the fixed reservation.
    {UpstreamFixture f;f.record(0);f.record(1);f.commit();
     for(unsigned e=0;e<2;++e)assert(f.adapter.backend(f.native.records[e],f.context(),f.lookup()).decision==Decision::Portal);
     assert(f.adapter.submitted());f.native.complete=true;assert(f.adapter.poll_completion());
     f.native.settled_ok=true;assert(f.adapter.reclaim());++f.native.obs.arena.recording_generation;
     f.begin(2);auto in=f.eye(0);assert(!f.reserve(in));f.native.upload(f.adapter);assert(f.native.native_uploads==2);assert(!f.adapter.enqueue(0,in));}
    {UpstreamFixture f;
     uint64_t last=0;const auto allocs=hot_allocations.load();
     for(uint64_t frame=1;frame<=10000;++frame) {
        if(frame>1){f.native.reset();f.begin(frame);}
        for(unsigned e=0;e<2;++e){auto in=f.eye(e);auto t=f.reserve(in);assert(t>last);last=t;f.native.upload(f.adapter);assert(f.adapter.enqueue(t,in));}
        f.commit();for(unsigned e=0;e<2;++e)assert(f.adapter.backend(f.native.records[e],f.context(),f.lookup()).decision==Decision::Portal);
        assert(f.adapter.submitted());f.native.complete=true;assert(f.adapter.poll_completion());
        f.native.settled_ok=true;assert(f.adapter.reclaim());assert(!f.adapter.state().pending && !f.adapter.state().tickets_used);
     }
     assert(f.native.native_uploads==20000 && f.native.allocations==1 && hot_allocations==allocs);
     std::puts("10000 upstream stereo cycles, native uploads=20000, one persistent wrapper, resident<=2");}
    if(argc==2) {
        FILE* file=std::fopen(argv[1],"rb");assert(file);uint32_t count=0;
        assert(std::fread(&count,4,1,file)==1 && count==882);
        for(unsigned n=0;n<count;n+=2){UpstreamFixture f;
            for(unsigned e=0;e<2;++e){auto in=f.eye(e);std::array<float,12> origin{};
                assert(std::fread(in.prefix.data(),352,1,file)==1);
                assert(std::fread(in.constants.data(),528,1,file)==1);
                assert(std::fread(origin.data(),48,1,file)==1);
                for(unsigned i=0;i<12;++i)in.raw_to_standing.v[i]=origin[i];
                f.native.expected_stock=get<Address>(in.prefix,8);
                auto t=f.reserve(in);assert(t);f.native.upload(f.adapter);assert(f.adapter.enqueue(t,in));
                assert(!std::memcmp(f.native.payloads[e].data(),in.constants.data(),528));
            }
            f.commit();for(unsigned e=0;e<2;++e)assert(f.adapter.backend(f.native.records[e],f.context(),f.lookup()).decision==Decision::Portal);
            assert(f.native.native_uploads==2);
        }
        assert(std::fgetc(file)==EOF);std::fclose(file);
        std::puts("882 pinned trace records through actual upstream Adapter + Bridge, synthetic native calls");
    }
    std::puts("upstream Adapter + Bridge production tests passed");
}
#endif
