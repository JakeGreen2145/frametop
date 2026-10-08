#include "native_portal.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
namespace native_portal {
struct Owner::Impl {
    struct Sample { Geometry geometry; bool tracking=false,paused=false,menu=false; uint64_t generation=0,time=0; };
    struct Observed { Reply reply; uint64_t intent=0; };
    std::string path; Clock clock; Mailbox<Sample> sample; Mailbox<Observed> status;
    std::atomic<bool> wanted{false},quit{false}; std::atomic<uint64_t> intent{0};
    std::thread worker; uint64_t generation=0;
    Impl(std::string p,Clock c):path(std::move(p)),clock(c) {
        if(path.empty()) { const char* rt=std::getenv("XDG_RUNTIME_DIR"); if(rt&&rt[0]=='/')path=std::string(rt)+"/frametop-native-portal/control.sock"; }
        Reply r; r.reason=Reason::MissingBridge; status.publish({r,intent.load()}); sample.publish(Sample{});
    }
    void run() {
        Client client; Reply r; Request q; uint64_t seen=0,last=0; bool connected=false; uint32_t last_flags=0;
        while(!quit) {
            auto ticket=intent.load(); bool want=wanted.load(); Sample s; bool have=sample.read(s); auto now=clock();
            bool ready=have&&s.tracking&&!s.paused&&s.time&&now>=s.time&&now-s.time<250000000;
            if(!want||!ready) {
                if(connected) { client.close(); connected=false; }
                r=Reply{}; r.state=want?State::Warming:State::Off; r.reason=want?Reason::Tracking:Reason::Off; status.publish({r,ticket});
                seen=0; last=0;
            } else {
                if(ticket!=seen) { client.close(); connected=false; q=Request{}; seen=ticket; last=0; }
                if(!connected) {
                    // No retries for a failed/revoked epoch until explicit On, or
                    // a tracking/suppression recovery starts a new acquire.
                    if(!last) {
                        r=Reply{}; r.reason=Reason::MissingBridge;
                        if(client.connect(path)) {
                            q=Request{}; q.sent_ns=now; q.sequence=1;
                            if(client.exchange(q,r)&&r.epoch&&r.capability) {
                                connected=true; q.epoch=r.epoch; q.origin=r.origin; q.op=Operation::Update;
                            } else client.close();
                        }
                        status.publish({r,ticket}); last=now;
                    }
                }
                uint32_t flags=1|(s.menu?2:0);
                if(connected&&((q.op==Operation::Update&&q.sequence==1) || flags!=last_flags || now-last>=50000000)) {
                    q.sequence++; q.op=Operation::Update; q.sent_ns=s.time; q.geometry=s.geometry; q.generation=s.generation; q.flags=flags;
                    if(!client.exchange(q,r)) { r=Reply{}; r.state=State::Fault; r.reason=Reason::OwnerGone; connected=false; }
                    else if(r.state==State::Fault||r.state==State::Unavailable||r.state==State::SuspendedExternal) { client.close(); connected=false; }
                    status.publish({r,ticket}); last=now; last_flags=flags;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        client.close(); r=Reply{}; r.state=State::Off; r.reason=Reason::Off; status.publish({r,intent.load()});
    }
};
Owner::Owner(std::string path,Clock clock):impl_(new Impl(std::move(path),clock)) {}
Owner::~Owner(){shutdown();delete impl_;}
void Owner::shutdown() { impl_->wanted=false; impl_->quit=true; if(impl_->worker.joinable())impl_->worker.join(); }
bool Owner::command(const char* cmd,char* out,int size) {
    std::istringstream input(cmd); std::string subsystem,action,value,extra; input>>subsystem>>action;
    if(subsystem!="passthrough"||action!="portal")return false;
    if(!(input>>value)||input>>extra||(value!="on"&&value!="off"&&value!="status")) {
        std::snprintf(out,size,"error passthrough portal <on|off|status>"); return true;
    }
    auto& s=*impl_;
    if(value=="on") { s.wanted=true; ++s.intent; if(!s.worker.joinable()) { s.quit=false; s.worker=std::thread([&s]{s.run();}); } }
    if(value=="off") {
        s.wanted=false; ++s.intent;
        if(!s.worker.joinable()) { Reply off; off.state=State::Off; off.reason=Reason::Off; s.status.publish({off,s.intent.load()}); }
    }
    Impl::Observed observed; Reply r;
    if(s.status.read(observed))r=observed.reply;
    else { r.state=State::Fault; r.reason=Reason::Concurrency; }
    if(observed.intent!=s.intent.load()) {
        r=Reply{}; r.state=s.wanted?State::Warming:State::Draining; r.reason=Reason::None;
    }
    const bool wanted=s.wanted.load();
    if(wanted&&(r.state==State::Off||r.state==State::Draining)) { r.state=State::Warming; r.reason=Reason::None; }
    if(!wanted&&(r.state==State::Warming||r.state==State::ActiveSubmitted||r.state==State::SuspendedExternal||r.state==State::SuppressedMenu)) {
        r.state=State::Draining; r.reason=Reason::Off;
    }
    // Public epoch identifies this process's intent lifetime, not the receiver's
    // independently issued socket lease. It cannot drop to zero on async EOF.
    r.epoch=s.intent.load();
    std::snprintf(out,size,"ok portal {\"abi\":1,\"wanted\":%s,\"state\":\"%s\",\"reason\":\"%s\",\"epoch\":%llu,\"submitted_frame\":%llu,\"camera_freshness\":\"unknown\"}",
        s.wanted?"true":"false",name(r.state),name(r.reason),(unsigned long long)r.epoch,(unsigned long long)r.submitted_frame);
    return true;
}
void Owner::tick(const passthrough::Volume& v,bool tracking,bool paused,bool menu) {
    Impl::Sample s; s.time=impl_->clock(); s.generation=++impl_->generation;
    s.tracking=tracking&&passthrough::Valid(v); s.paused=paused; s.menu=menu;
    for(int i=0;i<3;++i) { s.geometry.center[i]=float(v.center[i]); s.geometry.size[i]=float(v.size[i]); }
    // R = Ry(yaw) Rx(pitch) Rz(roll), exactly the retained native volume.
    constexpr double rad=3.14159265358979323846/360;
    const double cy=std::cos(v.yaw*rad),sy=std::sin(v.yaw*rad),cx=std::cos(v.pitch*rad),sx=std::sin(v.pitch*rad),cz=std::cos(v.roll*rad),sz=std::sin(v.roll*rad);
    s.geometry.quaternion[0]=float(cy*sx*cz+sy*cx*sz);
    s.geometry.quaternion[1]=float(sy*cx*cz-cy*sx*sz);
    s.geometry.quaternion[2]=float(cy*cx*sz-sy*sx*cz);
    s.geometry.quaternion[3]=float(cy*cx*cz+sy*sx*sz);
    if(!impl_->sample.publish(s))impl_->wanted=false;
}
}
