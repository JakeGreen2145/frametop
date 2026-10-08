// SPDX-License-Identifier: MIT
// CPU-only boundaries reuse the upstream native-operation fixture. No live hooks.
#include "build/upstream_fixture.hpp"
#include "../binding/pass_gate.hpp"
#include "../binding/lifecycle.hpp"
#include <optional>
#include <filesystem>
namespace nb = native_portal::binding;
struct CombinedNative : UpstreamNative {
    alignas(8) std::array<unsigned char,0x4000> memory{};
    nb::Generations ids;
    std::optional<nb::Command> command;
    std::optional<nb::Lifecycle> life;
    nb::Owner owner{};
    std::array<Provenance,2> provenance{};
    uint64_t next=0;
    bool mutate_after_pipeline=false;
    Address at(size_t offset) {return addr(memory)+offset;}
    static bool read_checked(void* u,Address a,void* out,size_t n) noexcept {
        auto& f=*static_cast<CombinedNative*>(u);
        for(auto span : {std::pair<Address,size_t>{addr(f.renderer),f.renderer.size()},
                        {addr(f.memory),f.memory.size()}}) {
            if(a>=span.first && a-span.first<=span.second && n<=span.second-(a-span.first)) {
                std::memcpy(out,reinterpret_cast<void*>(a),n);return true;
            }
        }
        return false;
    }
    static bool live(void* u,const Context& c) noexcept {
        auto& f=*static_cast<CombinedNative*>(u);
        return f.valid && c.identity.renderer==f.binding.identity.renderer && c.recording_generation==f.owner.recording;
    }
    nb::PassReader reader() {return {this,read_checked,live};}
    CombinedNative() {
        obs.arena.pass_begin=at(0);obs.arena.pass_end=at(3*0x98);
        obs.arena.command_wrapper=at(0x300);obs.arena.command=at(0x3000);
        put(renderer,0x1380,obs.arena.command_wrapper);
        put(memory,0x308,obs.arena.command);put(memory,0x3738,uint64_t{0xc001});
        attach();
    }
    void attach() {
        // Synthetic owned command/recording identities, not reconstructed live03 IDs.
        // Never reconstruct an observer over unresolved CPU/executable/GPU debt.
        if(life) {
            const auto old=life->state();
            assert(old.current && old.retained && old.invalidated && !old.unsupported);
            assert(command->state().gpu_complete && next==old.next_generation);
            life.reset();
        }
        command.emplace();
        owner={binding.identity,obs.arena.recording_generation,next?next:ids.issue(),obs.arena.command,0xc001,
               obs.arena.resource,obs.arena.buffer,obs.arena.map,obs.arena.capacity,64,0x701,0x702,0x703};
        assert(command->bind(owner));life.emplace(*command,nb::Memory{this,read_checked},ids);
        assert(life->bind(owner,2));
        put(renderer,0x15a0,obs.arena.pass_begin);put(renderer,0x15a8,obs.arena.pass_end);
        put(renderer,0x15b8,at(0x400));put(renderer,0x15c0,at(0x400));
        put(renderer,0x1478,at(0x500));put(renderer,0x1480,at(0x580));
        for(unsigned i=0;i<2;++i) {
            const size_t pass=i*0x98,packet=0x1000+i*0x800,resource=0x800+i*0x200,cache=0x500+i*0x40;
            put(memory,pass,at(resource));put(memory,pass+0x20,uint64_t{0x1234+i});
            put(memory,pass+0x50,at(packet));put(memory,pass+0x58,at(packet+0x2b0));
            put(memory,pass+0x68,uint32_t{0x0101});put(memory,resource+0x14c,uint32_t{37+i});
            put(memory,cache,uint64_t{0xdead0000+i});put(memory,cache+8,uint64_t{0xbeef0000+i});
            put(memory,cache+0x10,at(resource));put(memory,cache+0x30,uint64_t{0x1234+i});
            put(memory,cache+0x38,uint32_t{0x0101});
        }
    }
    void packet(unsigned slot,const Recorded& r) {
        const size_t p=0x1000+slot*0x800;
        std::memcpy(memory.data()+p,r.prefix.data(),352);
        put(memory,p+0x160,at(p+0x170));put(memory,p+0x168,uint64_t{15});
        std::memcpy(memory.data()+p+0x170,r.label.data(),16);
        std::memcpy(memory.data()+p+0x1c8,r.descriptors.data(),216);
        put(memory,p+0x180,uint64_t{0});put(memory,p+0x2a8,uint8_t{0});
    }
    static bool pipeline_bound(void* u,const Provenance& p,Registers& r) noexcept {
        auto& n=*static_cast<CombinedNative*>(u);n.provenance[p.eye]=p;
        const bool ok=Fake::pipeline(u,p,r);
        if(ok && p.eye==1 && n.mutate_after_pipeline)put(n.renderer,0x15d0,uint8_t{1});
        return ok;
    }
    static bool submission_bound(void* u,const Provenance& a,const Provenance& b,Completion& c) noexcept {
        return static_cast<CombinedNative*>(u)->command->submission(a,b,c);
    }
    static bool completed_bound(void* u,const Binding& b,const Completion& c) noexcept {
        return static_cast<CombinedNative*>(u)->command->completed(b,c);
    }
    static bool settled_bound(void* u,const Binding& b,const Provenance& a,const Provenance& c,const Completion& t) noexcept {
        return static_cast<CombinedNative*>(u)->life->settled(b,a,c,t);
    }
    static bool rejected_bound(void* u,Address command,const Provenance& a,const Provenance& b) noexcept {
        auto& n=*static_cast<CombinedNative*>(u);
        n.provenance={a,b};
        return n.command->rejected(n.scope(8,n.owner.command),command,a,b);
    }
    Callbacks callbacks(bool bind_rejection=true) {
        auto c=UpstreamNative::callbacks();c.pipeline=pipeline_bound;c.submission=submission_bound;
        c.completed=completed_bound;c.settled=settled_bound;c.rejected=bind_rejection?rejected_bound:nullptr;return c;
    }
    // Deliberately synthetic synchronous scope coordinates, not historical event replay.
    nb::Scope scope(unsigned id,Address object) {return {owner,id,2,0x900000-id*0x100,0x800000+id*4,object};}
    nb::Signal signal() {return {owner.synchronization,22,1};}
    bool event(nb::Site site,int32_t raw=0,uint64_t generation=0) {
        nb::Capture r{};r.sp=0x900000;r.x[30]=0x800000;
        r.x[0]=r.x[1]=r.x[19]=owner.command;
        if(site==nb::Site::CurrentEnter || site==nb::Site::CurrentPost)r.x[19]=owner.identity.renderer;
        if(site==nb::Site::RetainedEnter || site==nb::Site::RetainedPost)r.x[22]=owner.identity.renderer;
        if(site==nb::Site::ResetResult || site==nb::Site::BeginResult) {
            r.x[0]=static_cast<uint32_t>(raw);r.sp-=site==nb::Site::ResetResult?0x20:0x30;
        }
        return life->capture(site,r,2,generation?generation:owner.generation,owner.vk_command);
    }
    void queue(int mode=0) {
        assert(event(nb::Site::QueueEnter));auto q=scope(3,owner.command);
        nb::SubmitSnapshot v{true,true,1,1,1,1,owner.queue,owner.identity.device,owner.vk_command,owner.semaphore,nb::Command::timeline_value(signal()),0};
        assert(command->queue_enter(q,v,signal()));
        if(mode==2)return; // actual result not observed, never invent success
        assert(command->queue_result(q,mode==1?-4:0)==(mode!=1));if(mode==1)return;
        auto st=scope(4,owner.command);assert(command->stamp_enter(q,st,signal()));
        for(unsigned i=0;i<2;++i) {
            auto resource=scope(5+i,i?owner.command:owner.arena);
            assert(command->resource_enter(st,resource,signal()));
            assert(command->resource_post(resource,resource.object,signal()));
        }
        assert(command->stamp_post(st,owner.command,signal()));
    }
    void wait() {
        auto w=scope(7,owner.synchronization);assert(command->wait_enter(w,signal(),owner.semaphore));
        assert(command->wait_result(w,signal(),0));assert(command->wait_return(w,signal(),1));
    }
    void dispose() {
        assert(event(nb::Site::CurrentEnter));
        // CPU fixture models the pinned native swap. Instruction suites execute it.
        put(renderer,0x15a0,at(0x400));put(renderer,0x15a8,at(0x400));
        put(renderer,0x15b8,obs.arena.pass_begin);put(renderer,0x15c0,obs.arena.pass_end);
        assert(event(nb::Site::CurrentPost));assert(event(nb::Site::RetainedEnter));
        put(renderer,0x15c0,obs.arena.pass_begin);assert(event(nb::Site::RetainedPost));
    }
    void reset_command(int32_t raw=0) {
        assert(event(nb::Site::ResetEnter));assert(event(nb::Site::ResetResult,raw)==!raw);
        if(!raw) {assert(event(nb::Site::BeginEnter));assert(event(nb::Site::BeginResult));next=life->state().next_generation;}
    }
};
struct Combined {
    Fixture source;
    CombinedNative n;
    Adapter adapter;
    nb::PassScan scan{};
    std::optional<nb::PassGate> pass;
    explicit Combined(bool bind_rejection=true)
        : adapter(source.control,source.gate,n.binding,n.callbacks(bind_rejection),2) {
        assert(!n.binding.legacy_test_upload && adapter.initialize());
    }
    Context context(){return {n.binding.identity,n.owner.recording};}
    void start(uint64_t id=1) {
        PolicyInput p;p.enabled=true;auto frame=source.gate.begin(id,p,true,true,false,now);assert(adapter.latch(frame));
    }
    void record(unsigned first=0,unsigned count=2) {
        for(unsigned slot=0;slot<count;++slot) {
            auto in=source.eye(slot?1-first:first);std::memcpy(n.camera.data()+0xc0,in.constants.data(),528);
            const auto ticket=adapter.reserve_upload(in,n.addr(n.camera),n.addr(n.stock_wrapper));assert(ticket);
            n.upload(adapter);assert(adapter.enqueue(ticket,in));n.packet(slot,n.records[slot]);
            assert(n.records[slot].descriptors[0].offset==slot*640 && n.records[slot].descriptors[4].range==592);
        }
        if(count==2)assert(adapter.outer_return());
    }
    Recorded cold_stock() {
        auto stock=n.records[0];put(stock.prefix,8,Address{0xabc});n.packet(2,stock);
        put(n.memory,2*0x98,n.at(0xc00));put(n.memory,0xd4c,uint32_t{39});
        put(n.memory,2*0x98+0x20,uint64_t{0x1236});put(n.memory,2*0x98+0x68,uint32_t{0x0101});
        put(n.memory,2*0x98+0x50,n.at(0x2000));put(n.memory,2*0x98+0x58,n.at(0x22b0));
        // A cold pass is real stock work, not a dummy owned camera packet.
        put(n.renderer,0x1480,n.at(0x580));
        return stock;
    }
    void materialize_stock(const Recorded& stock,unsigned pass_slot=2) {
        // Called at the native stock pass position, never during preparation.
        // Synthetic helper/cache memory effect. Pinned helper execution is tested
        // independently, not claimed by this production-chain CPU fixture.
        put(n.memory,0x580,uint64_t{0xdead0002});put(n.memory,0x588,uint64_t{0xbeef0002});
        put(n.memory,0x590,n.at(0xc00));put(n.memory,0x5b0,uint64_t{0x1236});
        put(n.memory,0x5b8,uint32_t{0x0101});put(n.renderer,0x1480,n.at(0x5c0));
        assert(!skip(2,pass_slot));
        auto lookup=source.lookup();lookup.x[0]=n.binding.identity.renderer;
        assert(adapter.backend(stock,{},lookup).decision==Decision::Stock);
    }
    bool prepare() {
        nb::PassRequest q{context(),n.marker,0,0};scan=nb::extract_passes(n.reader(),q);
        pass.emplace(adapter,n.reader(),q);nb::PassRegisters regs{};regs.x[0]=n.binding.identity.renderer;
        pass->callback(regs,0);return adapter.state().committed;
    }
    bool skip(unsigned slot,unsigned pass_slot=3) {
        nb::PassRegisters regs{};regs.x[19]=n.at(0x1000+slot*0x800);regs.x[21]=n.binding.identity.renderer;
        regs.x[22]=n.at((pass_slot==3?slot:pass_slot)*0x98);pass->callback(regs,1);return regs.selector;
    }
    void draw(unsigned slot) {
        assert(!skip(slot));const auto& e=scan.pair.eyes[slot];uint64_t ticket=0;
        assert(decode_ticket(e.record.label,e.record.label_size,ticket));
        const unsigned eye=n.provenance[0].ticket==ticket?0:1;
        auto s=n.scope(1+eye,n.owner.identity.renderer);
        assert(n.command->backend_enter(s,n.provenance[eye]));
        assert(adapter.backend(e.record,context(),e.lookup).decision==Decision::Portal);
        assert(n.command->backend_return(s,true));
    }
};
struct ControlWire {
    Combined& f;std::string path;Server server;Client client;Request q;Reply reply;
    static std::string directory() {
        std::string p=std::string(std::getenv("TMPDIR"))+"/cb-XXXXXX";assert(mkdtemp(p.data()));return p;
    }
    explicit ControlWire(Combined& c):f(c),path(directory()),server(path,c.source.control,clock_now) {
        assert(server.start() && client.connect(server.path()));q.sequence=1;q.sent_ns=now;
        assert(client.exchange(q,reply) && reply.epoch);q.epoch=reply.epoch;q.origin=reply.origin;q.generation=1;
        q.op=Operation::Update;++q.sequence;assert(client.exchange(q,reply));
    }
    void release(bool pending=true) {
        q.op=Operation::Release;++q.sequence;assert(client.exchange(q,reply));
        assert(reply.state==(pending?native_portal::State::Draining:native_portal::State::Off));
    }
    ~ControlWire(){client.close();server.stop();std::filesystem::remove_all(path);}
};
static void rejected_before_dispatch(bool cold,bool expiry) {
    Combined f;ControlWire wire(f);
    // Seed a real published frame so a rejected replacement cannot clear or
    // advance submission status while settling its distinct native command.
    f.start();f.record();assert(f.prepare());f.draw(0);f.draw(1);
    f.n.queue();assert(f.adapter.submitted());f.n.wait();assert(f.adapter.poll_completion());
    f.n.dispose();f.n.reset_command();assert(f.adapter.reclaim());
    f.n.reset();f.n.attach();f.start(2);f.record();
    std::optional<Recorded> stock;if(cold)stock=f.cold_stock();
    const auto epoch=wire.q.epoch;
    const auto lookups=f.n.lookups;bool stock_materialized=false;
    BackendStatus published;assert(f.source.control.status(published) && published.submitted_frame==1);
    auto unchanged=[&]() {
        BackendStatus s;assert(f.source.control.status(s));
        assert(s.submitted_frame==published.submitted_frame && s.submitted_epoch==published.submitted_epoch);
        assert(!f.adapter.state().stereo_submitted && !f.adapter.state().committed);
        assert(!f.n.command->state().admitted && f.n.lookups==lookups+unsigned(stock_materialized));
    };
    if(expiry)now=1000000000000ULL;
    else wire.release(); // actual same-connection Off, before PassGate entry
    assert(!f.prepare() && f.scan.pair.suppression_scan);
    assert(f.scan.pair.complete_scan==!cold && !f.adapter.state().prepared);
    assert(f.skip(0) && !f.adapter.state().rejected);
    assert(!f.adapter.reclaim() && f.adapter.state().pending);unchanged();
    assert(f.skip(1));
    assert(f.adapter.state().rejected && f.n.command->state().rejected);
    assert(!f.adapter.state().cpu_done && !f.adapter.state().gpu_complete && !f.adapter.state().cpu_settled);
    if(stock) {f.materialize_stock(*stock);stock_materialized=true;}
    assert(!f.adapter.reclaim() && f.adapter.state().tickets_used==2);wire.release();unchanged();
    f.n.queue();assert(!f.adapter.submitted() && f.adapter.state().queue_observed);
    assert(!f.adapter.reclaim());wire.release();unchanged();
    f.n.wait();assert(f.adapter.poll_completion());
    assert(!f.adapter.reclaim());wire.release();unchanged();
    f.n.dispose();assert(!f.adapter.reclaim());wire.release();unchanged();
    f.n.reset_command();assert(f.adapter.reclaim());
    if(expiry) {
        // Expiry remains a latched control fault after render debt settles;
        // same-epoch Release observes it, never renews or clears that fault.
        wire.q.op=Operation::Release;++wire.q.sequence;
        assert(wire.client.exchange(wire.q,wire.reply));
        assert(wire.reply.state==native_portal::State::Fault && wire.reply.reason==Reason::Expired);
    } else wire.release(false);
    unchanged();
    assert(wire.q.epoch==epoch && wire.reply.epoch==epoch && !f.adapter.state().tickets_used && !f.adapter.state().pending);
    std::printf("combined: pre-dispatch %s, %s pair, exact settlement and same-epoch Release, submitted_frame unchanged\n",
                expiry?"expiry":"Release",cold?"cold":"warm");
}
int main() {
    for(unsigned cold=0;cold<2;++cold) for(unsigned expiry=0;expiry<2;++expiry)
        rejected_before_dispatch(cold,expiry);
    for(unsigned order=0;order<2;++order) for(unsigned encoding=0;encoding<2;++encoding) {
        Combined f;ControlWire wire(f);f.start();f.record(order);assert(f.prepare());
        f.draw(encoding);wire.release();f.draw(1-encoding); // actual socket Off between encodings
        f.n.command->off();f.n.queue();assert(!f.adapter.submitted() && f.adapter.state().stereo_submitted);
        assert(!f.adapter.reclaim());f.n.wait();assert(f.adapter.poll_completion());wire.release();
        f.n.dispose();wire.release();assert(!f.adapter.reclaim());f.n.reset_command();
        assert(f.adapter.reclaim());wire.release(false);
        assert(!f.adapter.state().tickets_used && !f.adapter.state().pending);
    }
    std::puts("combined: both record/encode orders, socket Off between eyes, exact queue/wait and lifecycle settlement");
    for(unsigned mode=0;mode<7;++mode) {
        Combined f;ControlWire wire(f);f.start();f.record();assert(f.prepare());f.draw(0);f.draw(1);wire.release();
        f.n.queue(mode<2?mode+1:0);
        if(mode<2)assert(!f.adapter.submitted() && !f.adapter.state().stereo_submitted);
        else {
            assert(!f.adapter.submitted() && f.adapter.state().stereo_submitted);
            if(mode!=2) {f.n.wait();assert(f.adapter.poll_completion());}
            if(mode!=3)f.n.dispose();
            if(mode!=2)f.n.reset_command(mode==4?-4:0);
            if(mode==5)assert(!f.n.event(nb::Site::QueueEnter));
            if(mode==6)assert(!f.n.event(nb::Site::QueueEnter,0,f.n.owner.generation+99));
        }
        assert(!f.adapter.reclaim() && f.adapter.state().tickets_used==2);wire.release();
    }
    std::puts("combined: failed/lost queue, missing GPU/CPU, failed reset, repeat/unknown queue retain pending debt");
    {Combined f;ControlWire wire(f);f.start();f.record();
     // Known cold stock-only third pass refuses OWNED packets, never drops stock.
     auto stock=f.cold_stock();
     assert(!f.prepare() && f.scan.reason==nb::PassReason::ColdPass);
     assert(f.skip(0) && f.skip(1) && !f.skip(2) && !f.n.lookups);
     assert(!f.adapter.state().gpu_complete && !f.adapter.state().cpu_settled);
     f.materialize_stock(stock);
     assert(!f.adapter.reclaim());wire.release();
     // Refusal is not settlement. The stock command must really queue and finish.
     f.n.queue();assert(!f.adapter.submitted());f.n.wait();assert(f.adapter.poll_completion());
     f.n.dispose();assert(!f.adapter.reclaim());f.n.reset_command();
     assert(f.adapter.reclaim());wire.release(false);}
    std::puts("combined: cold stock pass settles only after exact queue/wait/disposal/reset");
    for(unsigned order=0;order<2;++order) for(unsigned position=0;position<3;++position) {
        Combined f;ControlWire wire(f);f.start();f.record(order);auto stock=f.cold_stock();
        // Put cold stock before, between or after the owned passes, with no
        // packet relocation. Native materialization may warm the scan mid-dispatch.
        std::array<unsigned char,3*0x98> original{};
        std::memcpy(original.data(),f.n.memory.data(),original.size());
        unsigned owned_pass[2]{},owned=0;
        for(unsigned i=0;i<3;++i) {
            const unsigned source=i==position?2:owned++;
            std::memcpy(f.n.memory.data()+i*0x98,original.data()+source*0x98,0x98);
            if(source!=2)owned_pass[source]=i;
        }
        assert(!f.prepare() && !f.n.lookups);
        unsigned skipped=0;
        for(unsigned i=0;i<3;++i) {
            if(i==position)f.materialize_stock(stock,position);
            else {assert(f.skip(skipped,owned_pass[skipped]));if(!skipped)wire.release();++skipped;}
        }
        assert(f.adapter.state().rejected && !f.n.command->state().admitted);
        f.n.queue();assert(!f.adapter.submitted());f.n.wait();assert(f.adapter.poll_completion());
        f.n.dispose();f.n.reset_command();assert(f.adapter.reclaim());wire.release(false);
    }
    std::puts("combined: cold stock before/between/after owned passes, both recording orders, Off between actual skips");
    for(unsigned failed=0;failed<2;++failed) {
        Combined f;ControlWire wire(f);f.start();f.record();f.n.fail_pipeline_eye=failed;
        assert(!f.prepare() && f.skip(0) && f.skip(1));
        assert(!f.n.command->state().admitted && !f.adapter.state().cpu_done);
        assert(!f.adapter.reclaim());wire.release();
        f.n.queue();assert(!f.adapter.submitted());f.n.wait();assert(f.adapter.poll_completion());
        f.n.dispose();f.n.reset_command();assert(f.adapter.reclaim());wire.release(false);
    }
    std::puts("combined: either pipeline failure settles through real command/lifecycle proof without drawing");
    // Repeat every missing-proof case on the new no-owned-encoding path.
    for(unsigned mode=0;mode<8;++mode) {
        Combined f;ControlWire wire(f);f.start();f.record();f.cold_stock();
        assert(!f.prepare() && f.skip(1) && f.skip(0));wire.release();
        assert(f.adapter.state().rejected && !f.adapter.state().cpu_done);
        f.n.queue(mode<2?mode+1:0);
        assert(!f.adapter.submitted());
        if(mode>=2) {
            assert(f.adapter.state().queue_observed && !f.adapter.state().stereo_submitted);
            if(mode!=2) {f.n.wait();assert(f.adapter.poll_completion());}
            if(mode!=3)f.n.dispose();
            if(mode!=2)f.n.reset_command(mode==4?-4:0);
            if(mode==5)assert(!f.n.event(nb::Site::QueueEnter));
            if(mode==6)assert(!f.n.event(nb::Site::QueueEnter,0,f.n.owner.generation+1));
            if(mode==7) {
                auto q=f.n.scope(9,f.n.owner.command);auto signal=f.n.signal();++signal.serial;
                assert(!f.n.command->queue_enter(q,{},signal));
                Completion token{};assert(f.n.command->submission(f.n.provenance[0],f.n.provenance[1],token));
                assert(token.serial==f.n.signal().serial); // saved evidence cannot be replaced
            }
        }
        assert(!f.adapter.reclaim() && f.adapter.state().pending);wire.release();
        if(mode==4) { // failed old reset may be retried after Off, not guessed from empty vectors
            f.n.reset_command();assert(f.adapter.reclaim());wire.release(false);
        }
    }
    std::puts("combined: rejected pairs retain exact queue/GPU/CPU/reset and resubmission debt; reset retry settles");
    for(unsigned mode=0;mode<11;++mode) {
        Combined f;f.start();f.record();f.cold_stock();
        if(mode==0)put(f.n.renderer,0x15d0,uint8_t{1}); // unclassified pre-dispatch mutation
        if(mode==1)put(f.n.memory,0x98+0x58,f.n.at(0x1800)); // missing second packet
        if(mode==2)put(f.n.memory,0x1800+0x168,uint64_t{14}); // malformed ticket in original scan
        assert(!f.prepare());
        if(mode==0)put(f.n.renderer,0x15d0,uint8_t{0}); // a later good scan cannot replace missing entry proof
        if(mode==1)put(f.n.memory,0x98+0x58,f.n.at(0x1ab0));
        if(mode==2)put(f.n.memory,0x1800+0x168,uint64_t{15});
        assert(f.skip(0));
        if(mode==3)put(f.n.memory,0x1800+0x168,uint64_t{14}); // malformed actual second skip
        if(mode==4)put(f.n.memory,0x1800+0x1c8+sizeof(Descriptor),Descriptor{99,88,77}); // nonowned descriptor changed
        if(mode==5)put(f.n.memory,0x1800+0x1c8,Descriptor{f.n.owner.buffer,64,592}); // copied owned descriptor changed
        if(mode==6)assert(f.skip(0)); // repeated first skip
        if(mode==7)++f.n.obs.arena.recording_generation; // native lifetime generation changed
        if(mode==8)f.n.valid=false;
        if(mode!=9)assert(f.skip(1)); // missing actual skip is not implied by planned DROP
        if(mode==10)assert(f.skip(1)); // duplicate after proof must still block reclamation
        assert(!f.adapter.reclaim() && f.adapter.state().pending);
        if(mode!=10) {
            assert(!f.adapter.state().rejected && !f.n.command->state().rejected);
            assert(!f.n.event(nb::Site::QueueEnter));
        } else {
            f.n.queue();assert(!f.adapter.submitted());f.n.wait();
            assert(!f.adapter.poll_completion()); // poisoned skip proof never becomes adapter queue evidence
        }
    }
    std::puts("combined: malformed/missing/duplicate skips, descriptors, lifetime and unclassified scans retain debt");
    {Combined f;f.start();f.record();assert(f.prepare());f.draw(0);
     assert(!f.adapter.reject_recorded(f.scan.pair));
     assert(!f.n.command->rejected(f.n.scope(8,f.n.owner.command),f.n.owner.command,
                                 f.n.provenance[0],f.n.provenance[1]));
     assert(!f.adapter.reclaim() && !f.adapter.state().rejected);}
    {Combined f;f.start();f.record();f.n.mutate_after_pipeline=true;
     assert(!f.prepare());put(f.n.renderer,0x15d0,uint8_t{0});
     assert(f.skip(0) && f.skip(1));
     assert(!f.adapter.state().rejected && !f.n.command->state().rejected && !f.adapter.reclaim());}
    {Combined f(false);f.start();f.record();f.cold_stock();
     assert(!f.prepare() && f.skip(0) && f.skip(1));
     assert(!f.adapter.state().rejected && !f.adapter.reclaim());}
    {Combined f;f.start();f.record(0,1);f.cold_stock();
     assert(!f.prepare() && f.skip(0));
     assert(!f.adapter.state().rejected && !f.adapter.reclaim() && f.adapter.state().pending);}
    // Exact command and full provenance, never a same-looking foreign registration.
    for(unsigned mode=0;mode<7;++mode) {
        Combined f;f.start();f.record();assert(f.prepare());
        auto a=f.n.provenance[0],b=f.n.provenance[1];auto command=f.n.owner.command;
        if(mode==0)++command;
        if(mode==1)++b.recording_generation;
        if(mode==2)++b.identity.device_generation;
        if(mode==3)++b.descriptors[1].buffer;
        if(mode==4)b.ticket=a.ticket;
        if(mode==5)++b.deadline;
        if(mode==6)b.eye=0;
        assert(!f.n.command->rejected(f.n.scope(8,f.n.owner.command),command,a,b));
        assert(!f.n.command->state().rejected && !f.adapter.reclaim());
    }
    std::puts("combined: one encoded eye and foreign/malformed command provenance cannot use rejection settlement");
    {Combined f;const auto allocations=hot_allocations.load();uint64_t published=0;
     for(unsigned frame=1;frame<=2000;++frame) {
        if(frame>1) {f.n.reset();f.n.attach();}
        f.start(frame);f.record(frame%2);
        const bool rejected=frame%2; // every refusal is followed by a warm retry without another On
        auto stock=f.cold_stock();
        if(!rejected) {
            // The prior native helper populated this exact cache entry. attach()
            // models a new command while retaining native render-pass cache memory.
            put(f.n.renderer,0x1480,f.n.at(0x5c0));
        }
        f.n.fail_pipeline_eye=(rejected && frame%4==3)?1:-1;
        if(f.n.fail_pipeline_eye==1)put(f.n.renderer,0x1480,f.n.at(0x5c0));
        const auto lookups=f.n.lookups;
        assert(f.prepare()==!rejected);
        if(rejected) {
            assert(f.skip(frame%2) && f.skip(1-frame%2));
            assert(f.adapter.state().rejected && !f.adapter.state().cpu_done && !f.n.command->state().admitted);
            if(f.n.fail_pipeline_eye==-1)assert(f.n.lookups==lookups);
        } else {f.draw(frame%2);f.draw(1-frame%2);}
        f.materialize_stock(stock);
        assert(!f.adapter.reclaim() && f.adapter.state().pending);
        f.n.queue();assert(f.adapter.submitted()==!rejected);
        BackendStatus status;assert(f.source.control.status(status));
        if(!rejected)published=frame;
        assert(status.submitted_frame==published && !f.n.settled_ok);
        f.n.wait();assert(f.adapter.poll_completion());assert(!f.adapter.reclaim());
        f.n.dispose();assert(!f.adapter.reclaim());f.n.reset_command();assert(f.adapter.reclaim());
        assert(!f.adapter.state().tickets_used && !f.adapter.state().pending);
        assert(f.n.bridge.policy().actual()==1280);
     }
     assert(f.n.native_uploads==4000 && f.n.allocations==1 && hot_allocations==allocations);
     std::puts("combined: 2000 alternating cold/preparation-refused and warm retries, exact settlement, no new On, bounded storage/zero hot allocation");}
    {Combined f;const auto allocations=hot_allocations.load();uint64_t last=0;
     for(unsigned frame=1;frame<=2000;++frame) {
        if(frame>1) {f.n.reset();f.n.attach();}
        f.start(frame);f.record(frame%2);assert(f.prepare());f.draw(frame%2);f.draw(1-frame%2);
        assert(f.n.provenance[0].ticket>last);last=f.n.provenance[0].ticket;
        f.n.queue();assert(f.adapter.submitted());f.n.wait();assert(f.adapter.poll_completion());
        f.n.dispose();f.n.reset_command();assert(f.adapter.reclaim());
        assert(!f.adapter.state().tickets_used && !f.adapter.state().pending);
        assert(f.n.bridge.policy().actual()==1280); // only next native reset clears it
     }
     assert(f.n.native_uploads==4000 && f.n.allocations==1 && hot_allocations==allocations);
     std::puts("combined: 2000 complete frames, 4000 upstream uploads, resident<=2, one wrapper, zero hot allocations");}
}
