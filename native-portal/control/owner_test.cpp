// CPU-only owner regressions. Private production SOCK_SEQPACKET server/backend.
#include "control.hpp"
#include "../../screens/native_portal.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
using namespace native_portal;
using namespace std::chrono;
template<class F> static void until(F f) {
    for(int i=0;i<500;++i) { if(f())return; std::this_thread::sleep_for(milliseconds(2)); }
    assert(false && "owner acknowledgement timeout");
}
static std::atomic<uint64_t> now{1000000000};
static uint64_t clock_now() { return now; }
struct Fixture {
    std::string dir;
    Backend backend;
    BackendStatus b;
    Server server;
    Owner owner;
    passthrough::Volume volume;
    static std::string directory() {
        std::string p=std::string(std::getenv("TMPDIR"))+"/no-XXXXXX";
        assert(mkdtemp(p.data())); return p;
    }
    Fixture():dir(directory()),server(dir,backend,clock_now),owner(server.path(),clock_now) {
        b.capable=b.serialized=b.external_clear=true; b.origin=7; backend.publish(b);
        assert(server.start());
    }
    ~Fixture() { owner.shutdown(); server.stop(); std::filesystem::remove_all(dir); }
    std::string command(const char* value) {
        char out[512]; auto start=steady_clock::now();
        assert(owner.command(value,out,sizeof out));
        assert(steady_clock::now()-start<milliseconds(50));
        return out;
    }
    std::string status() { return command("passthrough portal status"); }
    bool state(const char* value) { return status().find(std::string("\"state\":\"")+value+"\"")!=std::string::npos; }
    bool leased() { Lease l; return backend.lease(l,now); }
    void on() { owner.tick(volume,true,false,false); command("passthrough portal on"); until([&]{return leased();}); }
    uint64_t epoch() { Lease l; assert(backend.lease(l,now)); return l.epoch; }
    void submitted() {
        b.submitted_epoch=epoch(); b.submitted_frame++; backend.publish(b);
        now+=50000000; owner.tick(volume,true,false,false);
        until([&]{return state("active-submitted");});
        backend.render_pending(true);
    }
    void pending(bool wanted=false) {
        auto text=status();
        assert(text.find(wanted?"\"wanted\":true":"\"wanted\":false")!=std::string::npos);
        assert(!state("off") && "Off must wait for native settlement, not socket close");
        assert(!leased() && "polling settlement must not renew or acquire");
    }
};
static void delayed_settlement() {
    Fixture f; f.on(); f.submitted();
    f.command("passthrough portal off");
    until([&]{return !f.leased();});
    // Hold debt beyond both the lease TTL and many worker polls. Never time it out.
    for(int i=0;i<12;++i) {
        now+=50000000; std::this_thread::sleep_for(milliseconds(10)); f.pending();
    }
    assert(f.state("draining")); std::puts(f.status().c_str());
    f.backend.render_pending(false); // synthetic exact GPU AND CPU settlement
    until([&]{return f.state("off");}); std::puts(f.status().c_str());
}
static void readiness_loss(bool paused) {
    Fixture f; f.on(); f.submitted(); auto old=f.epoch();
    f.owner.tick(f.volume,paused,paused,false);
    until([&]{return !f.leased();});
    f.owner.tick(f.volume,true,false,false);
    for(int i=0;i<12;++i) { std::this_thread::sleep_for(milliseconds(10)); f.pending(true); }
    f.backend.render_pending(false);
    until([&]{return f.leased();}); assert(f.epoch()>old);
    assert(f.status().find("\"epoch\":1,")!=std::string::npos);
}
static void rapid_toggle() {
    Fixture f; f.on(); f.submitted(); auto old=f.epoch();
    f.command("passthrough portal off");
    f.command("passthrough portal on");
    until([&]{return !f.leased();});
    for(int i=0;i<12;++i) { std::this_thread::sleep_for(milliseconds(10)); f.pending(true); }
    assert(f.status().find("\"epoch\":3,")!=std::string::npos);
    std::puts(f.status().c_str());
    f.backend.render_pending(false);
    until([&]{return f.leased();}); assert(f.epoch()>old);
    f.submitted();
    f.command("passthrough portal off"); f.command("passthrough portal on"); f.command("passthrough portal off");
    until([&]{return !f.leased();});
    for(int i=0;i<12;++i) { std::this_thread::sleep_for(milliseconds(10)); f.pending(); }
    f.backend.render_pending(false); until([&]{return f.state("off");});
    assert(f.status().find("\"epoch\":6,")!=std::string::npos);
}
static void revoked_debt(bool retry) {
    Fixture f; f.on(); f.submitted(); auto old=f.epoch();
    ++f.b.origin; f.backend.publish(f.b); now+=50000000; f.owner.tick(f.volume,true,false,false);
    until([&]{return f.status().find("origin-changed")!=std::string::npos;});
    for(bool paused:{false,true}) {
        f.owner.tick(f.volume,paused,paused,false);
        std::this_thread::sleep_for(milliseconds(60)); f.pending(true);
        f.owner.tick(f.volume,true,false,false);
        std::this_thread::sleep_for(milliseconds(60)); f.pending(true);
    }
    if(retry)f.command("passthrough portal on");
    // Let the worker see the explicit retry while settlement is still blocked.
    for(int i=0;i<6;++i) { std::this_thread::sleep_for(milliseconds(10)); f.pending(true); }
    f.backend.render_pending(false);
    if(!retry) {
        until([&]{return f.state("fault");});
        for(bool paused:{false,true}) {
            f.owner.tick(f.volume,paused,paused,false);
            until([&]{return f.status().find("tracking-invalid")!=std::string::npos;});
            f.owner.tick(f.volume,true,false,false);
            until([&]{assert(!f.leased()); return f.state("fault");});
        }
        f.command("passthrough portal on");
    }
    until([&]{return f.leased();}); assert(f.epoch()>old);
}
static void disconnected() {
    Fixture f; f.on(); f.submitted(); f.server.stop();
    f.command("passthrough portal off");
    for(int i=0;i<12;++i) { std::this_thread::sleep_for(milliseconds(10)); f.pending(); }
    assert(f.state("fault")); std::puts(f.status().c_str());
    // A replacement server is not evidence that the prior instance settled.
    assert(f.server.start()); f.command("passthrough portal on");
    for(int i=0;i<12;++i) { std::this_thread::sleep_for(milliseconds(10)); f.pending(true); }
    f.owner.shutdown(); assert(!f.state("off"));
}
static void shutdown_pending() {
    Fixture f; f.on(); f.submitted();
    auto start=steady_clock::now(); f.owner.shutdown();
    assert(steady_clock::now()-start<milliseconds(400));
    assert(!f.leased()); assert(f.state("draining")||f.state("fault"));
    std::puts(f.status().c_str());
    // A later command on the same object must not erase shutdown's unresolved debt.
    f.command("passthrough portal off"); assert(!f.state("off"));
    f.command("passthrough portal on");
    for(int i=0;i<12;++i) { std::this_thread::sleep_for(milliseconds(10)); f.pending(true); }
    assert(f.state("fault"));
}
static void missing_bridge() {
    Fixture f; f.server.stop();
    f.command("passthrough portal off"); assert(f.state("off"));
    f.owner.tick(f.volume,true,false,false); f.command("passthrough portal on");
    until([&]{return f.state("unavailable");});
    f.command("passthrough portal off"); until([&]{return f.state("off");});
}
// Controlled wire peer for request ordering and late/unresponsive replies.
// The settlement tests above use the production Server, not this script.
struct DelayedPeer {
    std::string dir=Fixture::directory(),path=dir+"/control.sock";
    int listener=-1;
    std::atomic<bool> stop{false},hold{false},entered{false},pending{true};
    std::atomic<unsigned> acquires{0},updates{0},releases{0};
    Operation delayed=Operation::Release;
    std::thread worker;
    explicit DelayedPeer(Operation op=Operation::Release):delayed(op) {
        listener=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0); assert(listener>=0);
        sockaddr_un a{}; a.sun_family=AF_UNIX; assert(path.size()<sizeof a.sun_path);
        std::memcpy(a.sun_path,path.c_str(),path.size()+1);
        assert(!bind(listener,reinterpret_cast<sockaddr*>(&a),sizeof a));
        assert(!chmod(path.c_str(),0600)); assert(!listen(listener,4));
        worker=std::thread([this]{run();});
    }
    ~DelayedPeer() { stop=true; hold=false; worker.join(); close(listener); std::filesystem::remove_all(dir); }
    void run() {
        int fd=-1; uint64_t seq=0,epoch=0; bool released=false;
        while(!stop) {
            pollfd p{fd<0?listener:fd,POLLIN,0};
            if(poll(&p,1,5)<=0)continue;
            if(fd<0) { fd=accept4(listener,nullptr,nullptr,SOCK_CLOEXEC|SOCK_NONBLOCK); seq=0; released=false; continue; }
            Wire w; auto n=recv(fd,w.data(),w.size(),MSG_TRUNC);
            if(n<=0) { close(fd); fd=-1; continue; }
            Request q; assert(decode(w.data(),size_t(n),q)); assert(q.sequence>seq); seq=q.sequence;
            Reply r; r.capability=1; r.instance=77; r.origin=7; r.sequence=seq;
            if(q.op==Operation::Acquire) {
                assert(!acquires||!pending); assert(!q.epoch); epoch=++acquires;
                r.state=State::Warming; r.reason=Reason::None;
            } else {
                assert(q.epoch==epoch);
                if(q.op==Operation::Update) {
                    assert(!released); ++updates; r.state=State::ActiveSubmitted; r.submitted_frame=9;
                } else {
                    assert(q.op==Operation::Release); assert(!q.flags); released=true; ++releases;
                    r.state=pending?State::Draining:State::Off; r.reason=Reason::Off;
                }
            }
            r.epoch=epoch;
            if(q.op==delayed&&hold) {
                entered=true;
                while(hold&&!stop)std::this_thread::sleep_for(milliseconds(1));
            }
            if(stop)break;
            w=encode(r); (void)send(fd,w.data(),w.size(),MSG_NOSIGNAL);
        }
        if(fd>=0)close(fd);
    }
};
static std::string command(Owner& owner,const char* cmd="passthrough portal status") {
    char text[512]; auto start=steady_clock::now(); assert(owner.command(cmd,text,sizeof text));
    assert(steady_clock::now()-start<milliseconds(50)); return text;
}
static void delayed_reply(Operation op) {
    DelayedPeer peer(op); Owner owner(peer.path,clock_now); passthrough::Volume v;
    peer.hold=op==Operation::Update;
    owner.tick(v,true,false,false); command(owner,"passthrough portal on");
    until([&]{return peer.updates.load()!=0;});
    peer.hold=true;
    command(owner,"passthrough portal off");
    until([&]{return peer.entered.load();});
    command(owner,"passthrough portal on");
    auto text=command(owner); assert(text.find("\"wanted\":true")!=std::string::npos);
    assert(text.find("\"epoch\":3,")!=std::string::npos); std::puts(text.c_str());
    command(owner,"passthrough portal off"); // newer intent than the blocked reply
    peer.hold=false;
    until([&]{return peer.releases>=3;});
    text=command(owner);
    assert(text.find("\"state\":\"draining\"")!=std::string::npos);
    assert(text.find("\"epoch\":4,")!=std::string::npos); std::puts(text.c_str());
    assert(peer.acquires==1); assert(peer.updates==1);
    peer.pending=false;
    until([&]{return command(owner).find("\"state\":\"off\"")!=std::string::npos;});
    assert(peer.acquires==1); assert(peer.updates==1);
}
static void unresponsive(Operation op) {
    DelayedPeer peer(op); Owner owner(peer.path,clock_now); passthrough::Volume v;
    peer.hold=op==Operation::Acquire;
    owner.tick(v,true,false,false); command(owner,"passthrough portal on");
    if(op==Operation::Release) {
        until([&]{return peer.updates!=0;}); peer.hold=true;
    } else until([&]{return peer.entered.load();});
    command(owner,"passthrough portal off");
    until([&]{return peer.entered.load();});
    until([&]{return command(owner).find("\"state\":\"fault\"")!=std::string::npos;});
    std::puts(command(owner).c_str());
    auto start=steady_clock::now(); owner.shutdown();
    assert(steady_clock::now()-start<milliseconds(400));
    assert(command(owner).find("\"state\":\"off\"")==std::string::npos);
    assert(peer.acquires==1);
}
int main(int argc,char** argv) {
    const std::string only=argc>1?argv[1]:"";
    if(only.empty()||only=="settlement")delayed_settlement();
    if(only.empty()||only=="tracking")readiness_loss(false);
    if(only.empty()||only=="pause")readiness_loss(true);
    if(only.empty()||only=="toggle")rapid_toggle();
    if(only.empty()||only=="revoked")revoked_debt(false);
    if(only.empty()||only=="revoked-retry")revoked_debt(true);
    if(only.empty()||only=="disconnect")disconnected();
    if(only.empty()||only=="shutdown")shutdown_pending();
    if(only.empty()||only=="missing")missing_bridge();
    if(only.empty()||only=="late-release")delayed_reply(Operation::Release);
    if(only.empty()||only=="late-update")delayed_reply(Operation::Update);
    if(only.empty()||only=="timeout")unresponsive(Operation::Release);
    if(only.empty()||only=="lost-acquire")unresponsive(Operation::Acquire);
    std::puts("native owner pending Off PASS");
}
