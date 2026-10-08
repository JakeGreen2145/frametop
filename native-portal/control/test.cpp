#include "control.hpp"
#include "policy.hpp"
#include "../../screens/native_portal.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <thread>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace native_portal;
static std::atomic<uint64_t> now{1000000000};
static uint64_t clock_now() { return now.load(); }
template<class F> void until(F f) { for (int i=0;i<500;++i) { if(f())return; std::this_thread::sleep_for(std::chrono::milliseconds(2)); } assert(false && "timeout"); }
static int calls=0; static bool result=true;
static bool native_call(void*) { ++calls; return result; }
static void owner_recovery() {
    std::string base=std::string(std::getenv("TMPDIR"))+"/nr-XXXXXX"; assert(mkdtemp(base.data()));
    Backend bridge; Server server(base,bridge,clock_now); assert(server.start());
    Owner owner(server.path(),clock_now); passthrough::Volume v; Lease lease; char text[512];
    auto status=[&](const char* reason) {
        assert(owner.command("passthrough portal status",text,sizeof text));
        return strstr(text,reason)!=nullptr;
    };
    owner.tick(v,true,false,false); owner.command("passthrough portal on",text,sizeof text);
    until([&]{return status("unsupported-backend");});
    BackendStatus b; b.capable=b.serialized=b.external_clear=true; b.origin=23; bridge.publish(b);
    auto recover_refused=[&](const char* reason,bool paused) {
        owner.tick(v,paused,paused,false);
        until([&]{return status("tracking-invalid");}); // worker acknowledged loss/pause
        owner.tick(v,true,false,false);
        until([&]{
            assert(!bridge.lease(lease,now) && "recovery must not bypass explicit On");
            return status(reason); // worker acknowledged ready with its original refusal
        });
        assert(strstr(text,"\"epoch\":1,"));
    };
    recover_refused("unsupported-backend",false);
    recover_refused("unsupported-backend",true);
    // Only another explicit On clears the failed-acquire latch.
    owner.command("passthrough portal on",text,sizeof text);
    until([&]{return bridge.lease(lease,now);});
    const auto active_epoch=lease.epoch;
    // Previously valid connections still recover automatically, without new intent.
    for(bool paused:{false,true}) {
        const auto previous=lease.epoch;
        owner.tick(v,paused,paused,false);
        until([&]{return status("tracking-invalid")&&!bridge.lease(lease,now);});
        owner.tick(v,true,false,false);
        until([&]{return bridge.lease(lease,now)&&lease.epoch>previous;});
        assert(lease.epoch>active_epoch); status("warming"); assert(strstr(text,"\"epoch\":2,"));
    }
    // Observed revocation also remains latched across both recovery paths.
    b.origin++; bridge.publish(b); now+=50000000; owner.tick(v,true,false,false);
    until([&]{return status("origin-changed");});
    for(bool paused:{false,true}) {
        owner.tick(v,paused,paused,false); until([&]{return status("tracking-invalid");});
        owner.tick(v,true,false,false);
        until([&]{assert(!bridge.lease(lease,now)); return status("origin-changed");});
        assert(strstr(text,"\"epoch\":2,"));
    }
    owner.command("passthrough portal on",text,sizeof text);
    until([&]{return bridge.lease(lease,now)&&lease.origin==b.origin;});
    status("warming"); assert(strstr(text,"\"epoch\":3,"));
    owner.shutdown(); server.stop(); std::filesystem::remove_all(base);
}
int main() {
    owner_recovery();
    Request q; q.op=Operation::Acquire; q.sequence=1; q.sent_ns=now;
    auto wire=encode(q); Request decoded;
    assert(wire.size()==128 && decode(wire.data(),wire.size(),decoded));
    assert(!decode(wire.data(),127,decoded)); wire[52]=1; assert(!decode(wire.data(),128,decoded));
    wire=encode(q); wire[127]=1; assert(!decode(wire.data(),128,decoded));
    q.geometry.quaternion[3]=2; wire=encode(q); assert(!decode(wire.data(),128,decoded)); q.geometry.quaternion[3]=1;
    q.ttl_ms=251; wire=encode(q); assert(!decode(wire.data(),128,decoded)); q.ttl_ms=250;
    q.geometry.size[0]=.05f; wire=encode(q); assert(decode(wire.data(),128,decoded)); q.geometry.size[0]=1;
    assert(!deadline(UINT64_MAX-1,UINT64_MAX-1,250)); assert(!deadline(now+1,now,250));
    // Graph fixture inputs cover the 2560 native-replayed policy combinations.
    for(unsigned bits=0;bits<256;++bits) for(unsigned style=0;style<5;++style) for(bool owned:{false,true}) {
        PolicyInput p{bool(bits&1),bool(bits&2),bool(bits&4),bool(bits&8),bool(bits&16),bool(bits&32),bool(bits&64),bool(bits&128),style,.37f,.63f};
        auto stock=pure_policy(p,false), e=pure_policy(p,owned);
        bool c=p.enabled && ((p.special?p.construct:p.request)||owned);
        assert(e.construct==c); assert(e.style==(style?style:(c?2u:1u)));
        float a=p.p1?(p.p0?1.f:0.f):p.forced?1.f:c?(e.style==3?.5f:1.f):((e.style>=1&&e.style<=3)?p.base*p.gain:p.base);
        if(!p.p1&&!p.forced&&!c&&e.style==3)a=std::min(a,.5f);
        assert(e.alpha==a); assert(e.demand==(c||(p.p1&&p.p0)||(!p.suppression&&p.forced)));
        assert(pure_policy(p,false).alpha==stock.alpha);
    }
    assert(background_permission(false,true,false,1.f,1.f,false,false)==false);
    assert(background_permission(false,true,false,0.f,1.f,false,true)==true);
    DemandAdapter demand; assert(!demand.can_acquire());
    demand.establish(true,true); assert(demand.can_acquire());
    assert(demand.bind({1,7,11}));
    assert(demand.layer(false,true,native_call,native_call,nullptr)); assert(calls==1 && demand.owned_debt());
    assert(demand.debt_tag().epoch==1 && demand.debt_tag().origin==7 && demand.debt_tag().frame==11);
    assert(!demand.bind({2,7,12})); // old native transition must settle before a new lease
    assert(demand.last_outcome().forwarded && demand.last_outcome().success);
    assert(demand.external(true,native_call,nullptr)); assert(calls==2 && !demand.can_acquire());
    assert(demand.layer(false,false,native_call,native_call,nullptr)); assert(calls==2 && !demand.owned_debt());
    assert(demand.external(false,native_call,nullptr)); assert(calls==3); // real external release forwarded
    demand.establish(true,true); assert(demand.layer(true,false,native_call,native_call,nullptr));
    assert(demand.layer(false,false,native_call,native_call,nullptr)); assert(calls==5); // genuine stock release
    result=false; assert(!demand.external(true,native_call,nullptr)); assert(!demand.can_acquire()); result=true;
    // Real private transport; no VR, public socket, settings or installed state.
    const char* tmp=std::getenv("TMPDIR"); assert(tmp);
    std::string base=std::string(tmp)+"/np-XXXXXX"; assert(mkdtemp(base.data()));
    Backend bridge; Server server(base,bridge,clock_now); assert(server.start());
    struct stat st{}; assert(!stat(server.path().c_str(),&st) && (st.st_mode&0777)==0600);
    assert(!stat((base+"/frametop-native-portal").c_str(),&st) && (st.st_mode&0777)==0700);
    Client c; assert(c.connect(server.path())); Reply r;
    assert(c.exchange(q,r)); assert(r.state==State::Unavailable && !r.epoch);
    BackendStatus b; b.capable=true; b.serialized=true; b.external_clear=true; b.origin=7; bridge.publish(b);
    q.sequence++; assert(c.exchange(q,r) && r.epoch && r.origin==7); auto epoch=r.epoch;
    q.op=Operation::Update; q.epoch=epoch; q.origin=r.origin; q.sequence++; q.generation=1;
    assert(c.exchange(q,r) && r.state==State::Warming);
    Lease l; assert(bridge.lease(l,now) && l.epoch==epoch && l.generation==1);
    b.submitted_frame=9; b.submitted_epoch=epoch; bridge.publish(b); q.op=Operation::Status; q.sequence++;
    assert(c.exchange(q,r) && r.state==State::ActiveSubmitted && r.submitted_frame==9);
    Client other; assert(other.connect(server.path())); Request aq; aq.sent_ns=now; aq.sequence=1;
    assert(other.exchange(aq,r) && r.reason==Reason::Busy); other.close();
    q.op=Operation::Update; q.sequence++; q.flags=3; assert(c.exchange(q,r) && r.state==State::SuppressedMenu); assert(!bridge.lease(l,now));
    q.flags=1; q.sequence++; assert(c.exchange(q,r)); assert(bridge.lease(l,now));
    assert(r.state==State::Warming); // a pre-menu submission is not new activation
    assert(!bridge.lease(l,0)); // failed CLOCK_BOOTTIME may not authorize rendering
    // Replay revokes; a queued valid update cannot rescue that epoch.
    assert(c.exchange(q,r) && r.state==State::Fault); assert(!bridge.lease(l,now));
    q.sequence++; assert(c.exchange(q,r) && r.state==State::Fault);
    c.close(); assert(c.connect(server.path())); q=Request{}; q.sent_ns=now; q.sequence=1;
    assert(c.exchange(q,r) && r.epoch>epoch); epoch=r.epoch;
    q.op=Operation::Update; q.epoch=epoch; q.origin=7; q.sequence++; q.generation=1; assert(c.exchange(q,r));
    now+=250000000; assert(!bridge.lease(l,now)); q.sent_ns=now; q.sequence++; assert(c.exchange(q,r) && r.reason==Reason::Expired);
    c.close(); assert(c.connect(server.path())); q=Request{}; q.sent_ns=now; q.sequence=1;
    assert(c.exchange(q,r)); q.op=Operation::Update; q.epoch=r.epoch; q.origin=7; q.sequence++; q.generation=1; assert(c.exchange(q,r));
    b.origin=8; bridge.publish(b); until([&]{return !bridge.lease(l,now);}); q.sequence++; assert(c.exchange(q,r) && r.reason==Reason::Origin);
    c.close(); assert(c.connect(server.path())); q=Request{}; q.sent_ns=now; q.sequence=1; assert(c.exchange(q,r));
    q.op=Operation::Update; q.epoch=r.epoch; q.origin=8; q.sequence++; q.generation=1; assert(c.exchange(q,r)); c.close(); until([&]{return !bridge.lease(l,now);});
    // Screens owner uses receiver-issued origin and exposes bounded JSON, not camera freshness.
    Owner owner(server.path(),clock_now); char text[512];
    assert(owner.command("passthrough portal on",text,sizeof text)); assert(strstr(text,"\"wanted\":true"));
    passthrough::Volume v; v.enabled=false; owner.tick(v,true,false,false); until([&]{owner.tick(v,true,false,false); return bridge.lease(l,now);});
    assert(l.origin==8); owner.tick(v,false,false,false); until([&]{return !bridge.lease(l,now);});
    assert(owner.command("passthrough portal off",text,sizeof text)); assert(strstr(text,"\"wanted\":false"));
    assert(strstr(text,"\"camera_freshness\":\"unknown\""));
    // GUI acknowledgements and immediate status must never contradict wanted,
    // nor regress the native intent epoch when an async lease is torn down.
    until([&]{owner.command("passthrough portal status",text,sizeof text); return strstr(text,"\"state\":\"off\"")!=nullptr;});
    std::string off_status=text;
    assert(owner.command("passthrough portal on",text,sizeof text));
    assert(!strstr(text,"\"state\":\"off\""));
    owner.command("passthrough portal off",text,sizeof text);
    assert(!strstr(text,"\"state\":\"warming\""));
    owner.command("passthrough portal status",text,sizeof text);
    assert(!strstr(text,"\"state\":\"warming\""));
    assert(!strstr(off_status.c_str(),"\"epoch\":0"));
    owner.shutdown();
    server.stop(); std::filesystem::remove_all(base);
    std::puts("native portal protocol, policy, demand, socket and owner PASS");
}
