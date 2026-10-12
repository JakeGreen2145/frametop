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
    struct Intent { uint64_t ticket=0; bool wanted=false; };
    std::string path; Clock clock; Mailbox<Sample> sample; Mailbox<Observed> status;
    Mailbox<Intent> desired; // one coherent command snapshot for the worker
    std::atomic<bool> wanted{false},quit{false},unsettled{false}; std::atomic<uint64_t> intent{0};
    std::atomic<uint64_t> poc_until{0};
    std::thread worker; uint64_t generation=0;
    Impl(std::string p,Clock c):path(std::move(p)),clock(c) {
        if(path.empty()) { const char* rt=std::getenv("XDG_RUNTIME_DIR"); if(rt&&rt[0]=='/')path=std::string(rt)+"/frametop-native-portal/control.sock"; }
        Reply r; r.reason=Reason::MissingBridge; status.publish({r,intent.load()}); sample.publish(Sample{}); desired.publish(Intent{});
    }
    void run() {
        enum class Link { Idle, Active, Releasing, Lost };
        Client client; Reply r; Request q;
        Link link=unsettled?Link::Lost:Link::Idle;
        if(unsettled) { r.state=State::Fault; r.reason=Reason::OwnerGone; }
        uint64_t seen=0,last=0,lease_intent=0; uint32_t last_flags=0;
        bool attempted=false,off_known=!unsettled;
        auto next_release=std::chrono::steady_clock::now();
        auto lost=[&] {
            client.close(); link=Link::Lost;
            r=Reply{}; r.state=State::Fault; r.reason=Reason::OwnerGone;
        };
        auto release=[&] {
            // Status requires a live writer in ABI-1. Repeat the idempotent
            // Release on this exact connection/epoch instead; it never renews.
            q.op=Operation::Release; ++q.sequence; q.flags=0;
            if(!client.exchange(q,r)) { lost(); return; }
            if(r.state==State::Draining) { link=Link::Releasing; return; }
            off_known=r.state==State::Off;
            unsettled=!off_known;
            // A revocation latches its original intent, not a newer explicit
            // On queued while that lease was still draining.
            if(!off_known&&seen==lease_intent)attempted=true;
            client.close(); link=Link::Idle;
        };
        while(!quit) {
            Intent request;
            if(!desired.read(request)) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }
            const auto ticket=request.ticket; const bool want=request.wanted;
            Sample s; bool have=sample.read(s); auto now=clock();
            bool ready=now>=poc_until.load()&&have&&s.tracking&&!s.paused&&s.time&&now>=s.time&&now-s.time<250000000;
            auto current=[&] {
                Intent latest;
                return !quit&&desired.read(latest)&&latest.ticket==ticket&&latest.wanted==want;
            };
            if(ticket!=seen) {
                seen=ticket; attempted=false;
                if(link==Link::Active) { link=Link::Releasing; next_release=std::chrono::steady_clock::now(); }
            }
            if(link==Link::Active&&(!want||!ready)) {
                link=Link::Releasing; attempted=false; // a previously valid intent may recover
                next_release=std::chrono::steady_clock::now();
            }
            if(link==Link::Releasing) {
                if(std::chrono::steady_clock::now()>=next_release) {
                    release(); next_release=std::chrono::steady_clock::now()+std::chrono::milliseconds(50);
                }
            } else if(link==Link::Idle&&want&&ready&&!attempted&&current()) {
                attempted=true; r=Reply{}; r.reason=Reason::MissingBridge;
                if(client.connect(path)) {
                    if(current()) {
                        q=Request{}; q.sent_ns=now; q.sequence=1;
                        // A lost Acquire reply is ambiguous too: the server may
                        // have accepted it. EOF or a replacement server cannot
                        // prove that the old native work settled.
                        off_known=false; unsettled=true;
                        if(!client.exchange(q,r))lost();
                        else if(r.epoch&&r.capability&&r.state==State::Warming) {
                            link=Link::Active; q.epoch=r.epoch; q.origin=r.origin; last=0; lease_intent=ticket;
                        } else { off_known=!r.epoch; unsettled=!off_known; client.close(); }
                    } else client.close();
                }
            }
            uint32_t flags=1|(s.menu?2:0);
            if(link==Link::Active&&want&&ready&&current()&&
               (q.sequence==1||flags!=last_flags||now-last>=50000000)) {
                ++q.sequence; q.op=Operation::Update; q.sent_ns=s.time;
                q.geometry=s.geometry; q.generation=s.generation; q.flags=flags;
                if(!client.exchange(q,r))lost();
                else if(r.state==State::Draining||r.state==State::Fault||
                        r.state==State::Unavailable||r.state==State::SuspendedExternal) {
                    // Keep the revoked connection while it can still report debt.
                    link=Link::Releasing; next_release=std::chrono::steady_clock::now();
                }
                last=now; last_flags=flags;
            }
            Reply visible=r;
            if(link==Link::Idle) {
                if(!want&&off_known) { visible=Reply{}; visible.state=State::Off; visible.reason=Reason::Off; }
                else if(want&&!ready) { visible=Reply{}; visible.state=State::Warming; visible.reason=Reason::Tracking; }
            }
            status.publish({visible,ticket});
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        // Shutdown is bounded by the in-flight exchange and one Release. It
        // relinquishes demand, not debt, and never waits for native settlement.
        if(link==Link::Active||link==Link::Releasing)release();
        client.close();
        if(link==Link::Idle&&off_known) { r=Reply{}; r.state=State::Off; r.reason=Reason::Off; }
        status.publish({r,intent.load()});
    }
};
Owner::Owner(std::string path,Clock clock):impl_(new Impl(std::move(path),clock)) {}
Owner::~Owner(){shutdown();delete impl_;}
bool Owner::wanted() const { return impl_->wanted.load(); }
void Owner::poc_heartbeat() { impl_->poc_until=impl_->clock()+500000000; }
void Owner::shutdown() { impl_->wanted=false; impl_->desired.publish({impl_->intent.load(),false}); impl_->quit=true; if(impl_->worker.joinable())impl_->worker.join(); }
bool Owner::command(const char* cmd,char* out,int size) {
    std::istringstream input(cmd); std::string subsystem,action,value,extra; input>>subsystem>>action;
    if(subsystem!="passthrough"||action!="portal")return false;
    if(!(input>>value)||input>>extra||(value!="on"&&value!="off"&&value!="status")) {
        std::snprintf(out,size,"error passthrough portal <on|off|status>"); return true;
    }
    auto& s=*impl_;
    const bool poc=s.clock()<s.poc_until.load();
    if(value=="on") { s.wanted=true; ++s.intent; s.desired.publish({s.intent.load(),true}); if(!poc&&!s.worker.joinable()) { s.quit=false; s.worker=std::thread([&s]{s.run();}); } }
    if(value=="off") {
        s.wanted=false; ++s.intent; s.desired.publish({s.intent.load(),false});
        if(!s.worker.joinable()&&!s.unsettled) { Reply off; off.state=State::Off; off.reason=Reason::Off; s.status.publish({off,s.intent.load()}); }
    }
    if(poc) {
        std::snprintf(out,size,"ok portal {\"abi\":1,\"wanted\":%s,\"state\":\"%s\",\"reason\":\"POC desktop mask publisher connected; rendering unverified\",\"epoch\":%llu,\"submitted_frame\":0,\"camera_freshness\":\"unknown\"}",
            s.wanted?"true":"false",s.wanted?"poc-requested":"off",(unsigned long long)s.intent.load());
        return true;
    }
    Impl::Observed observed; Reply r;
    if(s.status.read(observed))r=observed.reply;
    else { r.state=State::Fault; r.reason=Reason::Concurrency; }
    if(observed.intent!=s.intent.load()) {
        r=Reply{}; r.state=s.wanted?State::Warming:State::Draining; r.reason=Reason::None;
    }
    const bool wanted=s.wanted.load();
    if(wanted&&r.state==State::Off) { r.state=State::Warming; r.reason=Reason::None; }
    if(wanted&&r.state==State::Draining)r.state=State::Warming; // keep the pending/revocation reason
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
    if(!impl_->sample.publish(s)) { impl_->wanted=false; impl_->desired.publish({++impl_->intent,false}); }
}
}
