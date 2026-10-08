#pragma once
#include "control.hpp"
#include "policy.hpp"
namespace native_portal {
struct Frame {
    uint64_t id=0; Lease lease; FramePolicy policy;
};
// Conservative first adapter: ALL native callbacks, including backend acceptance
// and native resource transitions, must run on the established owner thread.
// A runtime using deferred workers cannot activate this adapter until a proven
// handoff implementation replaces this restriction. No blocking lock is hidden.
class FrameGate {
    Backend& backend_; const std::thread::id owner_=std::this_thread::get_id();
    std::atomic_flag busy_=ATOMIC_FLAG_INIT; uint64_t last_=0,active_=0;
    uint32_t eyes_=0; bool invalid_=true;
    bool enter() {
        if(std::this_thread::get_id()!=owner_||busy_.test_and_set()) { backend_.refuse(); return false; }
        return true;
    }
    void leave() { busy_.clear(); }
    bool accepted(const Frame& f,uint64_t now) {
        Lease current;
        if(invalid_||f.id!=active_||!f.policy.portal)return false;
        if(eyes_!=3) { invalid_=true; return false; }
        return now&&now<f.lease.deadline_ns&&backend_.lease(current,now)&&current.epoch==f.lease.epoch&&current.origin==f.lease.origin;
    }
public:
    explicit FrameGate(Backend& b):backend_(b) {}
    Frame begin(uint64_t id,const PolicyInput& inputs,bool tracking,bool safety,bool stock_active,uint64_t now) {
        Frame f; if(!enter())return f;
        if(!id||id<=last_) { backend_.refuse(); leave(); return f; }
        last_=active_=id; eyes_=0; invalid_=false; f.id=id;
        bool lease=backend_.lease(f.lease,now);
        f.policy=frame_policy(inputs,lease,tracking,safety,stock_active);
        leave(); return f;
    }
    bool eye(const Frame& f,unsigned eye,uint64_t observed_origin) {
        if(!enter())return false;
        BackendStatus s;
        bool ok=!invalid_&&f.policy.portal&&f.id==active_&&eye<2&&backend_.status(s)&&
            observed_origin==f.lease.origin&&s.origin==observed_origin&&!(eyes_&(1u<<eye));
        if(ok)eyes_|=1u<<eye; else invalid_=true;
        leave(); return ok;
    }
    bool accept(const Frame& f,uint64_t now) {
        if(!enter())return false;
        bool ok=accepted(f,now); leave(); return ok;
    }
    // Call only after the actual native stereo submission has succeeded. This
    // reports submission, never exposure freshness or visible headset pixels.
    bool submitted(const Frame& f,uint64_t now) {
        if(!enter())return false;
        BackendStatus s; bool ok=accepted(f,now)&&backend_.status(s);
        if(ok) { s.submitted_epoch=f.lease.epoch; s.submitted_frame=f.id; ok=backend_.publish(s); }
        leave(); return ok;
    }
    void end(const Frame& f) { if(!enter())return; if(f.id==active_)invalid_=true; leave(); }
};
}
