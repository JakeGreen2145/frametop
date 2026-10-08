// SPDX-License-Identifier: MIT
#define main original_render_suite_main
#include "../render/test.cpp"
#undef main
#if __has_include("pass.hpp")
#include "pass.hpp"
#if __has_include("pass_gate.hpp")
#include "pass_gate.hpp"
#endif
#include <vector>
using namespace native_portal::binding;
struct Memory {
    // Synthetic readable native object lifetime; no native calls in this reader.
    std::vector<std::byte> data = std::vector<std::byte>(0x30000);
    bool alive = true;
    unsigned reads = 0;
    template<class T> void set(Address a, T v) { std::memcpy(data.data()+a, &v, sizeof v); }
    void copy(Address a, const void *p, size_t n) { std::memcpy(data.data()+a,p,n); }
    static bool read(void *u, Address a, void *p, size_t n) noexcept {
        auto &m=*static_cast<Memory*>(u); ++m.reads;
        if (!m.alive || a>m.data.size() || n>m.data.size()-a) return false;
        std::memcpy(p,m.data.data()+a,n); return true;
    }
    static bool live(void *u, const Context &) noexcept { return static_cast<Memory*>(u)->alive; }
    PassReader reader() { return {this,read,live}; }
    void record(Address a, const Recorded &r) {
        copy(a,r.prefix.data(),r.prefix.size());
        set(a+0x160,a+0x170); set(a+0x168,r.label_size);
        copy(a+0x170,r.label.data(),16);
        copy(a+0x1c8,r.descriptors.data(),216);
        set<uint64_t>(a+0x180,0); set<uint8_t>(a+0x2a8,0);
    }
    void setup(Fixture &f) {
        const auto R=f.fake.binding.identity.renderer;
        const auto b=f.fake.obs.arena.pass_begin;
        set(R+0x15a0,b); set(R+0x15a8,f.fake.obs.arena.pass_end);
        set<Address>(R+0x1380,0x5000); set<Address>(0x5008,4);
        set<Address>(R+0x1478,0x6000); set<Address>(R+0x1480,0x6080);
        for (unsigned i=0;i<2;++i) {
            const Address pass=b+i*0x98, packet=0x8000+i*0x1000, resource=0x4000+i*0x200, cache=0x6000+i*0x40;
            set(pass,resource); set<uint64_t>(pass+0x20,0x1234+i);
            set(pass+0x50,packet); set(pass+0x58,packet+0x2b0);
            set<uint32_t>(pass+0x68,0x0101); // graphics and alternate attachment allowed
            set<uint32_t>(resource+0x14c,37+i);
            set<Address>(cache,0xdead0000+i); set<Address>(cache+8,0xbeef0000+i);
            set(cache+0x10,resource); set<uint64_t>(cache+0x30,0x1234+i);
            set<uint32_t>(cache+0x38,0x0101);
            record(packet,f.fake.records[i]);
        }
        // Third pass is empty, intentionally lacks a target/cache record.
    }
};
static PassRequest request(Fixture &f) { return {f.context(),f.fake.marker,0,0}; }
static void warm_pair() {
    for (uint32_t ps=125;ps<=130;++ps) {
        const uint32_t vs=ps<=127?57:58;
        Fixture f; f.start(); assert(f.adapter.enqueue(f.eye(0,vs,ps)));
        assert(f.adapter.enqueue(f.eye(1,vs,ps))); assert(f.adapter.outer_return());
        Memory m; m.setup(f);
        auto s=extract_passes(m.reader(),request(f));
        assert(s.reason==PassReason::Ready && s.pair.complete_scan);
        assert(s.pair.command==4 && s.owned==2);
        for (unsigned i=0;i<2;++i) {
            const auto &e=s.pair.eyes[i];
            assert(e.ready && e.pass==0x1000+i*0x98);
            assert(e.record.prefix==f.fake.records[i].prefix);
            assert(e.lookup.target==0x29dac0);
            assert(e.lookup.x[0]==0x100 && e.lookup.x[1]==0x8000+i*0x1000);
            assert(e.lookup.x[2]==0xdead0000+i && e.lookup.x[3]==37+i);
        }
        f.fake.check_lookup=true; f.fake.expected_lookup={s.pair.eyes[0].lookup,s.pair.eyes[1].lookup};
        auto generation=prepare_passes(f.adapter,m.reader(),request(f));
        assert(generation && f.fake.lookups==2 && !f.adapter.state().cpu_done);
        assert(f.adapter.commit(generation)); f.adapter.off();
        for (unsigned i : {1u,0u})
            assert(f.adapter.backend(s.pair.eyes[i].record,f.context(),s.pair.eyes[i].lookup).decision==Decision::Portal);
        assert(f.fake.lookups==2 && f.adapter.state().cpu_done);
    }
}
static void refusals() {
    for(unsigned mode=0;mode<29;++mode) {
        Fixture f; f.start(); f.stereo(); Memory m; m.setup(f);
        auto q=request(f);
        switch(mode) {
        case 0: m.set<Address>(0x9008,0x123); break; // missing eye, not an owned shader test
        case 1: m.record(0x9000,f.fake.records[0]); break;
        case 2: m.set<uint64_t>(0x9168,14); break;
        case 3: m.set<char>(0x9170,'X'); break;
        case 4: m.set<Address>(0x9160,0x17000); break;
        case 5: m.set<Address>(0x1580,0x6000); break; // empty native cache
        case 6: m.set<Address>(0x6040,0); break;
        case 7: m.set<uint32_t>(0x6078,0x0201); break;
        case 8: m.set<Address>(0x16a8,0x11c9); break;
        case 9: m.set<Address>(0x10f0,0x9001); break;
        case 10: m.set<Address>(0x10e8,UINT64_MAX-7); break;
        case 11: m.set<Address>(0x16a8,0x1000+(pass_limit+1)*0x98); break;
        case 12: m.set<Address>(0x10f0,0x9000+(packet_limit+1)*0x2b0); break;
        case 13: m.set<Address>(0x1580,0x6000+(cache_limit+1)*0x40); break;
        case 14: m.set<uint8_t>(0x16d0,1); break;
        case 15: m.set<Address>(0x1670,1); break;
        case 16: m.set<Address>(0x1690,1); break;
        case 17: m.set<uint8_t>(0x92a8,1); break;
        case 18: m.set<Address>(0x9180,1); break;
        case 19: m.set<uint8_t>(0x1100,0); break;
        case 20: m.set<Address>(0x1098,0x2ffff); m.set<Address>(0x6050,0x2ffff); break;
        case 21: m.set<uint32_t>(0x9030,129); break; // mismatched shader family
        case 22: m.alive=false; break;
        case 23: ++q.context.recording_generation; break;
        case 24: ++q.context.identity.device_generation; break;
        case 25: m.set<Address>(0x91c8,42); break; // authentication, not scanner shape
        case 26: m.set<uint8_t>(0x9042,0x77); break;
        case 27: m.set<Address>(0x5008,99); break;
        case 28: f.adapter.off(); break;
        }
        const auto result=extract_passes(m.reader(),q);
        if(mode<23 && mode!=21) assert(!result.pair.complete_scan);
        assert(!prepare_passes(f.adapter,m.reader(),q));
        assert(!f.fake.lookups && !f.adapter.state().committed);
        for(unsigned eye=0;eye<2;++eye)
            assert(f.adapter.backend(f.fake.records[eye],f.context(),f.lookup()).decision==Decision::Drop);
        // Even after whole-owned refusal the stock Adapter route forwards once.
        Recorded stock=f.fake.records[0]; put(stock.prefix,8,Address{0xabc});
        assert(f.adapter.backend(stock,{},f.lookup()).decision==Decision::Stock);
    }
}
static void stock_interleaving() {
    Fixture f; f.start(); f.stereo(); Memory m; m.setup(f);
    Recorded stock=f.fake.records[0]; put(stock.prefix,8,Address{0xabc});
    // Both eyes in different passes with a stock ticket-lookalike on each side.
    for(unsigned i=0;i<2;++i) {
        const Address packet=0x8000+i*0x1000;
        m.record(packet,stock); m.record(packet+0x2b0,f.fake.records[i]);
        m.record(packet+2*0x2b0,stock);
        m.set<Address>(0x1058+i*0x98,packet+3*0x2b0);
    }
    const auto scan=extract_passes(m.reader(),request(f));
    assert(scan.reason==PassReason::Ready && scan.owned==2);
    auto g=prepare_passes(f.adapter,m.reader(),request(f));assert(g && f.adapter.commit(g));
    assert(f.adapter.backend(stock,{},f.lookup()).decision==Decision::Stock);
    assert(f.adapter.backend(scan.pair.eyes[0].record,f.context(),scan.pair.eyes[0].lookup).decision==Decision::Portal);
    assert(f.adapter.backend(stock,{},f.lookup()).decision==Decision::Stock);
    assert(f.adapter.backend(scan.pair.eyes[1].record,f.context(),scan.pair.eyes[1].lookup).decision==Decision::Portal);
    assert(f.adapter.backend(stock,{},f.lookup()).decision==Decision::Stock);
    assert(f.fake.lookups==5);
}
static void empty_mode_and_partial() {
    for(unsigned kind=0;kind<5;++kind) {
        Fixture f;f.start();f.stereo();Memory m;m.setup(f);auto q=request(f);
        if(kind==0) q.dispatch_mode=1;
        if(kind==1) m.set<Address>(0x16a8,0x1000);
        if(kind==2) m.set<Address>(0x1058,0x8000);
        if(kind==3) { m.record(0xa000,f.fake.records[0]); m.set<Address>(0x1180,0xa000); m.set<Address>(0x1188,0xa2b0); }
        if(kind==4) { // nonempty cold STOCK pass must also refuse early materialization
            Recorded stock=f.fake.records[0];put(stock.prefix,8,Address{0xabc});m.record(0xa000,stock);
            m.set<Address>(0x1180,0xa000);m.set<Address>(0x1188,0xa2b0);
        }
        assert(!extract_passes(m.reader(),q).pair.complete_scan);
        assert(!prepare_passes(f.adapter,m.reader(),q) && !f.fake.lookups);
    }
    for(int failed:{0,1}) {
        Fixture f;f.start();f.stereo();Memory m;m.setup(f);f.fake.fail_pipeline_eye=failed;
        assert(!prepare_passes(f.adapter,m.reader(),request(f)));
        assert(!f.adapter.state().committed);
        for(unsigned eye=0;eye<2;++eye)
            assert(f.adapter.backend(f.fake.records[eye],f.context(),f.lookup()).decision==Decision::Drop);
    }
}
static void dispatch_gate() {
#if __has_include("pass_gate.hpp")
    for(unsigned mode=0;mode<7;++mode) {
        Fixture f;f.start();f.stereo();Memory m;m.setup(f);
        if(mode==1)m.set<Address>(0x1580,0x6000);
        if(mode==2)m.set<Address>(0x16a8,0x1000+(pass_limit+1)*0x98);
        if(mode==3)m.record(0x9000,f.fake.records[0]);
        if(mode==4)f.adapter.off();
        if(mode==5)f.fake.fail_pipeline_eye=1;
        const bool fail=mode!=0 && mode!=6;
        PassGate gate(f.adapter,m.reader(),request(f));
        PassRegisters r{}; r.x[0]=0x100;r.x[1]=0;
        gate.callback(r,0);
        assert(f.adapter.state().committed==!fail);
        r={};r.x[19]=0x8000;r.x[21]=0x100;r.x[22]=0x1000;
        gate.callback(r,1);assert(bool(r.selector)==fail);
        f.adapter.off();r.selector=0;gate.callback(r,1);assert(bool(r.selector)==fail);
        // Unmarked label-lookalikes never skip, even on a refused owned dispatch.
        m.set<Address>(0x8008,0xabc);r.selector=9;gate.callback(r,1);assert(!r.selector);
    }
#else
    assert(false && "missing production dispatch/packet gate");
#endif
}
static Memory *changing_memory=nullptr;
static void changed_during_lookup() {
    Fixture f; f.start(); f.stereo(); Memory m; m.setup(f);
    changing_memory=&m;
    f.fake.during_pipeline=+[](Fake &,unsigned eye) {
        if(eye==1) changing_memory->set<Address>(0x6040,0x999999);
    };
    assert(!prepare_passes(f.adapter,m.reader(),request(f)));
    assert(!f.adapter.commit(f.adapter.state().pair_generation));
    changing_memory=nullptr;
}
#endif
int main() {
#if __has_include("pass.hpp")
    dispatch_gate(); warm_pair(); changed_during_lookup(); refusals(); stock_interleaving(); empty_mode_and_partial();
    std::puts("pass readiness production tests passed");
#else
    assert(false && "missing native pass readiness extractor");
#endif
}
