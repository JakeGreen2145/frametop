// SPDX-License-Identifier: MIT
// Compile the exact runtime stock callback and read_self definitions into a
// minimal owner shell. Other native operations use the existing CPU boundary.
#define FT_UPSTREAM_FIXTURE_ONLY
#include "upstream_test.cpp"
#include <sys/uio.h>
#include <sys/syscall.h>
namespace native_portal::runtime {
#include "read_self.private.inc"
}
namespace native_portal::live {
#include "identity.private.inc"
struct Impl : UpstreamNative {
    bool owned=true;
    uint64_t invocation=1;
#include "stock.private.inc"
    unsigned resolutions=0;
    static bool native_call(void* u,Registers& r) noexcept {
        auto& p=*static_cast<Impl*>(u);
        if(r.target==p.binding.targets.descriptor)++p.resolutions;
        return Fake::call(u,r);
    }
    Callbacks callbacks() {
        auto c=UpstreamNative::callbacks();c.stock_blend=stock;c.call=native_call;return c;
    }
};
}
struct NativeCase {
    uint32_t special=0;float alpha=0;
    std::array<uint32_t,8> words{};
    Prefix prefix{};ft::portal::NativeConstants constants{};
};
struct BlendFixture {
    Fixture source;
    native_portal::live::Impl native;
    Adapter adapter{source.control,source.gate,native.binding,native.callbacks(),2};
    std::array<uint32_t,10> stock{};
    explicit BlendFixture(float opacity) {
        source.lease.geometry.opacity=opacity;
        assert(source.control.publish_lease(source.lease));
        assert(adapter.initialize());PolicyInput p;p.enabled=true;
        auto frame=source.gate.begin(1,p,true,true,false,now);
        if(opacity==0) {assert(!frame.policy.portal);assert(!adapter.latch(frame));}
        else assert(adapter.latch(frame));
    }
    bool record(const NativeCase& c,unsigned e) {
        auto in=source.eye(e);in.prefix=c.prefix;in.constants=c.constants;
        std::copy(c.words.begin(),c.words.end(),stock.begin()+2);
        put(in.prefix,8,UpstreamNative::addr(stock));
        const auto original=in;
        std::memcpy(native.camera.data()+0xc0,in.constants.data(),528);
        auto ticket=adapter.reserve_upload(in,UpstreamNative::addr(native.camera),UpstreamNative::addr(native.stock_wrapper));
        assert(ticket);native.upload(adapter);
        const bool accepted=adapter.enqueue(ticket,in);
        assert(in.prefix==original.prefix && in.constants==original.constants);
        assert(std::equal(c.words.begin(),c.words.end(),stock.begin()+2));
        if(accepted) {
            const auto& rec=native.records[native.enqueues-1];
            for(size_t i=0;i<352;++i)
                if(!(i>=8 && i<16) && !(i>=0x38 && i<0x40) && !(i>=0x58 && i<0x60))
                    assert(rec.prefix[i]==in.prefix[i]);
            assert(get<Address>(rec.prefix,8)==native.marker && native.marker!=UpstreamNative::addr(stock));
            assert(get<Address>(rec.prefix,0x38)==native.wrapper && get<Address>(rec.prefix,0x58)==native.wrapper);
            for(unsigned stage:{0,4}) {
                const auto& d=rec.descriptors[stage];assert(d.range==592 && d.offset%64==0 && d.buffer==native.obs.arena.buffer);
                assert(d.offset==640*(native.enqueues-1));
            }
            assert(!std::memcmp(native.payloads[native.enqueues-1].data(),in.constants.data(),528));
            assert(get<float>(native.payloads[native.enqueues-1],576)==source.lease.geometry.opacity);
            assert(native.obs.attachment==(std::array<uint32_t,8>{1,1,7,0,1,7,0,15}));
        }
        return accepted;
    }
    void commit() {
        assert(adapter.outer_return());auto lookup=source.lookup();lookup.x[0]=native.binding.identity.renderer;
        const Context context{native.binding.identity,native.obs.arena.recording_generation};
        assert(prepare_commit(adapter,native,context,lookup));
        for(unsigned e=0;e<2;++e)assert(adapter.backend(native.records[e],context,lookup).decision==Decision::Portal);
        assert(native.resolutions==4); // no late enqueue/backend resolver calls
    }
};
int main(int argc,char** argv) {
    assert(argc==2);FILE* file=std::fopen(argv[1],"rb");assert(file);uint32_t count=0;
    assert(std::fread(&count,4,1,file)==1 && count==36);
    unsigned positive=0,negative=0;
    NativeCase normal{};
    for(unsigned i=0;i<count;++i) {
        NativeCase c;
        assert(std::fread(&c.special,4,1,file)==1 && std::fread(&c.alpha,4,1,file)==1);
        assert(std::fread(c.words.data(),32,1,file)==1 && std::fread(c.prefix.data(),352,1,file)==1 && std::fread(c.constants.data(),528,1,file)==1);
        if(i==0)normal=c;
        for(float opacity:{.25f,1.f})for(unsigned order:{0,1}) {
            BlendFixture f(opacity);const bool accepted=f.record(c,order);
            if(!c.special) {
                if(!accepted)std::fprintf(stderr,"ordinary native alpha=%g portal opacity=%g VS=%u PS=%u rejected before private clone\n",c.alpha,opacity,get<uint32_t>(c.prefix,0x20),get<uint32_t>(c.prefix,0x30));
                assert(accepted);assert(f.record(c,1-order));f.commit();++positive;
            } else {
                assert(!accepted && !f.native.enqueues && !f.adapter.state().committed);
                // No accepted packet exists. A delayed owned-marker packet must
                // still DROP, never enter the stock pipeline callback.
                Recorded delayed{};put(delayed.prefix,8,f.native.marker);
                auto lookup=f.source.lookup();lookup.x[0]=f.native.binding.identity.renderer;
                assert(f.adapter.backend(delayed,{f.native.binding.identity,f.native.obs.arena.recording_generation},lookup).decision==Decision::Drop);
                assert(!f.native.lookups);++negative;
            }
        }
    }
    assert(std::fgetc(file)==EOF);std::fclose(file);
    // Every attachment word remains exact, including disabled-state inert factors.
    for(bool opaque:{false,true})for(unsigned word=0;word<8;++word) {
        auto c=normal;c.words=opaque?std::array<uint32_t,8>{0,0,0,0,0,0,0,15}:std::array<uint32_t,8>{1,1,7,0,1,7,0,15};
        c.words[word]^=0x100;BlendFixture f(.25f);assert(!f.record(c,0) && !f.native.enqueues);++negative;
    }
    for(unsigned vs:{57,58})for(unsigned ps=125;ps<=130;++ps) {
        if(supported_pair(vs,ps))continue;
        auto c=normal;put(c.prefix,0x20,vs);put(c.prefix,0x30,ps);
        BlendFixture f(.25f);assert(!f.record(c,0) && !f.native.enqueues);++negative;
    }
    {BlendFixture f(0);assert(!f.native.uploads && !f.native.enqueues && !f.adapter.state().committed);}
    {BlendFixture f(.25f);auto& p=f.native;std::copy(normal.words.begin(),normal.words.end(),f.stock.begin()+2);
     const auto address=UpstreamNative::addr(f.stock);
     assert(p.stock(&p,p.binding,address));p.owned=false;assert(!p.stock(&p,p.binding,address));p.owned=true;
     p.invocation=0;assert(!p.stock(&p,p.binding,address));p.invocation=1;
     auto b=p.binding;++b.identity.renderer_generation;assert(!p.stock(&p,b,address));
     assert(!p.stock(&p,p.binding,0));assert(!p.stock(&p,p.binding,1));}
    std::printf("exact runtime callback + upstream Adapter/Bridge: %u positive stereo cases, %u negative cases; zero opacity no records; private blend/prefix/528 bytes/two aligned 592 descriptors PASS\n",positive,negative);
}
