// SPDX-License-Identifier: MIT
// Reuse the actual Adapter + Command integration fixture, not a second adapter.
#include "build/lifecycle_fixture.hpp"
#include "lifecycle.hpp"
#include <algorithm>
#include <cstring>
struct LifeNative : Native {
    nb::Generations ids;
    std::array<unsigned char, 0x4000> memory{};
    nb::Lifecycle life;
    static bool read(void *u, uint64_t address, void *out, size_t n) noexcept {
        auto &f = *static_cast<LifeNative *>(u);
        if(address==f.owner.command+0x738 && n==8) {std::memcpy(out,&f.owner.vk_command,8);return true;}
        if(address < 0x100000 || address-0x100000 > f.memory.size() ||
           n > f.memory.size()-(address-0x100000)) return false;
        std::memcpy(out, f.memory.data()+address-0x100000, n); return true;
    }
    explicit LifeNative() : Native(chains[0]), life(command, {this, read}, ids) {
        binding.identity.renderer=0x100000; owner.identity=binding.identity;
    }
    void put(uint64_t at, uint64_t v) { assert(at>=0x100000 && at+8<=0x104000); std::memcpy(memory.data()+at-0x100000,&v,8); }
    void packet(unsigned eye) {
        const uint64_t p=0x102000+eye*0x2b0;
        auto label=encode_ticket(provenances[eye].ticket);
        put(p+0x160,p+0x170); put(p+0x168,15);
        std::memcpy(memory.data()+p+0x170-0x100000,label.data(),16);
        put(p+8,provenances[eye].marker);
    }
    static bool settled_bound(void *u,const Binding &b,const Provenance &a,const Provenance &c,const Completion &t) noexcept {
        return static_cast<LifeNative *>(u)->life.settled(b,a,c,t);
    }
    Callbacks callbacks() { auto c=Native::callbacks();c.settled=settled_bound;return c; }
};
struct LifeFixture {
    Fixture source; LifeNative n; Adapter adapter; Frame frame;
    LifeFixture(): adapter(source.control,source.gate,n.binding,n.callbacks(),2) {
        n.control=&source.control;
        // This fixture's renderer is a synthetic address in the checked memory reader.
        n.binding.identity.renderer=0x100000; n.owner.identity=n.binding.identity;
        n.owner.generation=n.ids.issue();
        assert(n.command.bind(n.owner)); assert(n.life.bind(n.owner,897014));
        assert(adapter.initialize());
        PolicyInput p;p.enabled=true;frame=source.gate.begin(chains[0].frame,p,true,true,false,now);
        assert(adapter.latch(frame));
    }
    void cpu() {
        assert(adapter.enqueue(source.eye(0)));assert(adapter.enqueue(source.eye(1)));assert(adapter.outer_return());
        Context ctx{n.binding.identity,n.owner.recording};auto lookup=source.lookup();lookup.x[0]=n.binding.identity.renderer;
        auto input=pair_input(n,ctx,lookup);input.eyes[1].pass=input.eyes[0].pass;
        auto g=adapter.prepare(input);assert(g && adapter.commit(g));
        for(unsigned eye=0;eye<2;++eye) {
            assert(n.command.backend_enter(scoped(n.owner,n.chain.backend[eye]),n.provenances[eye]));
            assert(adapter.backend(n.records[eye],ctx,lookup).decision==Decision::Portal);
            assert(n.command.backend_return(scoped(n.owner,n.chain.backend[eye]),true));
            n.packet(eye);
        }
        n.put(0x101a00+0x50,0x102000);n.put(0x101a00+0x58,0x102560);
        n.put(0x1015a0,0x101a00);n.put(0x1015a8,0x101a98);n.put(0x1015b8,0x101c00);n.put(0x1015c0,0x101c00);
    }
    nb::Capture regs(nb::Site s) {
        nb::Capture r{};r.sp=0x900000;r.x[30]=0x800000;
        r.x[0]=n.owner.command;r.x[1]=n.owner.command;r.x[19]=n.owner.command;
        if(s==nb::Site::CurrentEnter || s==nb::Site::CurrentPost)r.x[19]=n.owner.identity.renderer;
        if(s==nb::Site::RetainedEnter || s==nb::Site::RetainedPost)r.x[22]=n.owner.identity.renderer;
        return r;
    }
    bool event(nb::Site s,int32_t raw=0) {
        auto r=regs(s);if(s==nb::Site::ResetResult || s==nb::Site::BeginResult) {r.x[0]=static_cast<uint32_t>(raw);r.sp-=s==nb::Site::ResetResult?0x20:0x30;}
        return n.life.capture(s,r,897014,n.owner.generation,n.owner.vk_command);
    }
    void queued() {assert(event(nb::Site::QueueEnter));n.queue();n.stamps();assert(adapter.submitted());}
    void current() {
        assert(event(nb::Site::CurrentEnter));
        n.put(0x1015a0,0x101c00);n.put(0x1015a8,0x101c00);
        n.put(0x1015b8,0x101a00);n.put(0x1015c0,0x101a98);
        assert(event(nb::Site::CurrentPost));
    }
    void retained() {assert(event(nb::Site::RetainedEnter));n.put(0x1015c0,0x101a00);assert(event(nb::Site::RetainedPost));}
    void reset(int32_t result=0,int32_t begin=0) {
        assert(event(nb::Site::ResetEnter));assert(event(nb::Site::ResetResult,result)==(result==0));
        if(!result) {assert(event(nb::Site::BeginEnter));assert(event(nb::Site::BeginResult,begin)==(begin==0));}
    }
};
static void lifecycle_orders() {
    // GPU wait must precede reset; both disposal facts may occur on either side.
    std::array<unsigned,4> order{0,1,2,3};unsigned accepted=0;
    do {
        auto pos=[&](unsigned n){return std::find(order.begin(),order.end(),n)-order.begin();};
        if(pos(0)>pos(1) || pos(2)>pos(3))continue;
        LifeFixture f; f.cpu();f.queued();f.adapter.off();f.n.command.off();
        const auto allocs=binding_allocations.load();
        for(auto op:order) {
            if(op==0)f.current();if(op==1)f.retained();if(op==2)f.n.wait();if(op==3)f.reset();
        }
        assert(f.adapter.poll_completion());assert(f.adapter.reclaim());
        assert(f.adapter.state().tickets_used==0 && !f.adapter.state().pending);
        assert(f.n.life.state().invalidated && f.n.life.state().next_generation>f.n.owner.generation);
        assert(binding_allocations.load()==allocs);++accepted;
    } while(std::next_permutation(order.begin(),order.end()));
    assert(accepted==6);
}
static void lifecycle_failures() {
    for(int mode=0;mode<7;++mode) {
        LifeFixture f;f.cpu();f.queued();f.n.wait();f.adapter.off();
        if(mode!=0)f.current();
        if(mode!=0 && mode!=1)f.retained();
        if(mode==2)f.reset(-4);else if(mode!=3)f.reset(0,mode==4?-4:0);
        if(mode==5) {auto r=f.regs(nb::Site::QueueEnter);assert(!f.n.life.capture(nb::Site::QueueEnter,r,897014,f.n.owner.generation,f.n.owner.vk_command));}
        if(mode==6) {auto r=f.regs(nb::Site::QueueEnter);assert(!f.n.life.capture(nb::Site::QueueEnter,r,897014,f.n.owner.generation+99,f.n.owner.vk_command));}
        assert(f.adapter.poll_completion());
        assert(f.adapter.reclaim()==(mode==4));
        if(mode==4)assert(f.n.life.state().invalidated && !f.n.life.state().next_generation);
        else assert(f.adapter.state().tickets_used==2 && f.adapter.state().pending);
    }
    LifeFixture f;f.cpu();f.queued();assert(!f.event(nb::Site::ResetEnter));f.n.wait();
    assert(!f.event(nb::Site::CurrentEnter));
    assert(!f.adapter.reclaim()); // out-of-order reset cannot retroactively borrow completion
}
static void lifecycle_adversarial() {
    // Default false through the actual callback, not a substitute fake complete.
    LifeFixture fresh;
    nb::Lifecycle duplicate(fresh.n.command,{&fresh.n,LifeNative::read},fresh.n.ids);
    assert(!duplicate.bind(fresh.n.owner,897014)); // same numeric identity may never be rebound
    assert(!nb::Lifecycle::settled_callback(nullptr,{}, {}, {}, {}));
    for(unsigned field=0;field<7;++field) {
        LifeFixture f;f.cpu();f.queued();f.n.wait();f.current();f.retained();f.reset();
        Completion token;assert(f.n.command.submission(f.n.provenances[0],f.n.provenances[1],token));
        auto a=f.n.provenances[0];auto b=f.n.binding;
        if(field==0)++a.ticket;if(field==1)++a.recording_generation;if(field==2)++a.descriptors[0].offset;
        if(field==3)++token.serial;if(field==4)++token.command;if(field==5)++b.identity.device_generation;
        if(field==6)f.n.command.invalidate_lifetime();
        assert(!f.n.life.settled(b,a,f.n.provenances[1],token));
    }
    for(unsigned field=0;field<6;++field) {
        LifeFixture f;f.cpu();f.queued();f.n.wait();
        assert(f.event(nb::Site::ResetEnter));auto r=f.regs(nb::Site::ResetResult);r.sp-=0x20;r.x[0]=0;
        uint64_t gen=f.n.owner.generation,vk=f.n.owner.vk_command,tid=897014;
        if(field==0)++r.sp;if(field==1)++r.x[19];if(field==2)++gen;if(field==3)++vk;if(field==4)++tid;
        if(field==5)r.x[0]=1;
        assert(!f.n.life.capture(nb::Site::ResetResult,r,tid,gen,vk));
        assert(!f.n.life.state().invalidated);assert(!f.adapter.reclaim());
    }
    for(unsigned field=0;field<7;++field) {
        LifeFixture f;f.cpu();f.queued();
        if(field==0)f.n.put(0x1015a8,0x101a99); // stride
        if(field==1)f.n.put(0x1015a8,0x101a00+65*0x98); // work cap
        if(field==2)f.n.put(0x101a00+0x58,0x102000+129*0x2b0); // packet cap
        if(field==3)f.n.put(0x102000+0x168,14); // truncated ticket
        if(field==4)f.n.put(0x102000+0x160,UINT64_MAX-2); // reader/address failure
        if(field==5)f.n.put(0x102000+8,0); // wrong marker
        if(field==6) {auto label=encode_ticket(f.n.provenances[0].ticket);std::memcpy(f.n.memory.data()+0x2000+0x2b0+0x170,label.data(),16);}
        assert(!f.event(nb::Site::CurrentEnter));assert(!f.adapter.reclaim());
    }
    LifeFixture f;f.cpu();f.queued();f.n.wait();f.current();
    assert(f.event(nb::Site::RetainedEnter));
    // Even if both ticket markers are corrupted, a nonempty retained vector is
    // not a completed native disposal. Absence of recognizable labels is not proof.
    f.n.put(0x102008,0);f.n.put(0x1022b8,0);
    assert(!f.event(nb::Site::RetainedPost));
    LifeFixture late;late.cpu();late.adapter.off();late.n.command.off();
    assert(late.event(nb::Site::QueueEnter));late.n.queue();late.n.stamps();
    assert(!late.adapter.submitted() && late.adapter.state().stereo_submitted);
    late.n.wait();late.current();late.retained();late.reset();
    assert(late.adapter.poll_completion() && late.adapter.reclaim());
    LifeFixture g;g.cpu();g.queued();g.n.wait();
    auto r=g.regs(nb::Site::ResetEnter);
    std::thread t([&]{assert(!g.n.life.capture(nb::Site::ResetEnter,r,897014,g.n.owner.generation,g.n.owner.vk_command));assert(!g.n.life.state().bound);assert(!g.n.ids.issue());});t.join();
    assert(!g.n.life.state().unsupported);
    g.current();g.retained();g.reset();
    assert(g.adapter.poll_completion() && g.adapter.reclaim());
    assert(!g.n.life.bind(g.n.owner,897014));
    nb::Command reused;auto next=g.n.owner;next.generation=g.n.life.state().next_generation;++next.recording;
    assert(reused.bind(next));nb::Lifecycle next_life(reused,{&g.n,LifeNative::read},g.n.ids);
    assert(next_life.bind(next,897014));assert(!next_life.state().invalidated);
    assert(!next_life.capture(nb::Site::ResetResult,r,897014,g.n.owner.generation,g.n.owner.vk_command));
}
int main() {
    lifecycle_adversarial();
    LifeFixture empty;assert(!empty.n.life.state().invalidated);
    lifecycle_orders();lifecycle_failures();
    assert(binding_original_main()==0);
    static_assert(sizeof(nb::Lifecycle)<512);
    std::printf("lifecycle fixed bytes=%zu, issuer bytes=%zu; zero hot-path allocations\n",sizeof(nb::Lifecycle),sizeof(nb::Generations));
    std::puts("lifecycle: real Adapter reclamation, all six valid fact orders, reset failure and failed begin passed");
}
