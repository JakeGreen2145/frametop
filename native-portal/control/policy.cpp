#include "policy.hpp"
#include "control.hpp"
#include <algorithm>
#include <cmath>
namespace native_portal {
PolicyOutput pure_policy(const PolicyInput& p,bool owned) {
    PolicyOutput r; r.construct=p.enabled&&((p.special?p.construct:p.request)||owned);
    r.style=p.style?p.style:(r.construct?2:1); r.demand=r.construct;
    if(p.p1) { r.alpha=p.p0?1.f:0.f; if(p.p0)r.demand=true; }
    else if(p.forced)r.alpha=1;
    else if(r.demand)r.alpha=r.style==3?.5f:1.f;
    else { r.alpha=p.base; if(r.style>=1&&r.style<=3)r.alpha=p.base*p.gain; if(r.style==3)r.alpha=std::min(r.alpha,.5f); }
    if(!r.demand&&!p.suppression)r.demand=p.forced;
    return r;
}
bool activity(bool available,bool f5d,bool construct,bool f5a,float alpha) { return available&&(f5d||construct||(f5a&&alpha>0)); }
bool background(bool active,bool native_portal,float alpha) { return !active||native_portal||double(alpha)<=.9; }
bool background_permission(bool original,bool owned,bool stock_active,float stock_alpha,float effective_alpha,bool native_portal,bool effective_active) {
    if(!std::isfinite(stock_alpha)||!std::isfinite(effective_alpha))return original;
    const bool expected=background(effective_active,native_portal,effective_alpha);
    return original||(original==expected&&owned&&background(stock_active,native_portal,stock_alpha)&&!expected);
}
FramePolicy frame_policy(const PolicyInput& p,bool lease,bool tracking,bool safety,bool stock_active) {
    FramePolicy f; f.stock=pure_policy(p,false); f.effective=f.stock;
    bool external=p.request||p.construct||p.forced||(p.special&&!p.normal_construct)||stock_active||f.stock.demand;
    f.portal=lease&&tracking&&safety&&p.enabled&&!external&&std::isfinite(p.base)&&std::isfinite(p.gain);
    if(f.portal)f.effective=pure_policy(p,true);
    return f;
}
DemandAdapter::DemandAdapter(Backend* bridge):bridge_(bridge),owner_(std::this_thread::get_id()) {
    if(bridge_)bridge_->new_lease_ready(false);
}
bool DemandAdapter::enter() {
    if(std::this_thread::get_id()!=owner_||busy_.test_and_set()) { unsafe_=true; revoked_=true; if(bridge_)bridge_->refuse(); return false; }
    return true;
}
void DemandAdapter::leave() { busy_.clear(); }
void DemandAdapter::establish(bool ordering_proven,bool native_idle) {
    if(!enter())return;
    // Explicit native idle readback is required after a handoff/failure. A stock
    // request=false observation alone is not evidence of idle resource handles.
    if(ordering_proven&&native_idle&&!effective_&&!unsafe_) {
        ready_=true; revoked_=false; failed_=false; handed_=false;
        debt_=false; debt_tag_={}; bound_={};
        if(bridge_) { bridge_->settled(); bridge_->new_lease_ready(true); }
    }
    else { ready_=false; if(bridge_)bridge_->new_lease_ready(false); }
    leave();
}
void DemandAdapter::establish_native_path(bool ordering_proven,bool connection_eligible) {
    if(!enter())return;
    const bool eligible=ordering_proven && connection_eligible && !unsafe_ && !failed_ && !revoked_ && !handed_;
    ready_=eligible;
    if(bridge_) {
        if(eligible && !effective_ && !debt_)bridge_->settled();
        bridge_->new_lease_ready(eligible && !effective_ && !debt_);
    }
    leave();
}
bool DemandAdapter::bind(DemandTag tag) {
    if(!enter())return false;
    bool ok=can_acquire()&&tag.epoch&&tag.origin&&tag.frame&&
        (!debt_||(tag.epoch==debt_tag_.epoch&&tag.origin==debt_tag_.origin))&&
        (tag.epoch!=bound_.epoch||tag.frame>=bound_.frame);
    if(ok)bound_=tag;
    leave(); return ok;
}
bool DemandAdapter::layer(bool stock,bool owned,Call acquire,Call release,void* ctx) {
    if(!enter())return false; // caller must disable hook path; no concurrent cleanup
    owned=owned&&can_acquire()&&bound_.epoch; bool next=stock||owned; bool ok=true;
    if(next!=effective_) {
        if(next) {
            ok=acquire(ctx); outcome_={bound_,stock?DemandSource::Stock:DemandSource::Owned,true,true,ok};
            if(ok&&!stock&&owned) { debt_=true; debt_tag_=bound_; if(bridge_)bridge_->new_lease_ready(false); }
        }
        else {
            // Only the extra P-only edge after handoff is ours to suppress.
            const bool forward=!(debt_&&handed_&&!stock_);
            if(forward)ok=release(ctx);
            outcome_={debt_tag_,stock_?DemandSource::Stock:DemandSource::Owned,false,forward,ok};
            if(ok&&(!handed_||!failed_)) { debt_=false; handed_=false; debt_tag_={}; }
        }
        if(ok)effective_=next; else { failed_=true; revoked_=true; if(bridge_)bridge_->invalidate(Reason::NativeFault); }
    }
    stock_=stock; leave(); return ok;
}
void DemandAdapter::external_observed() {
    if(bridge_) {bridge_->invalidate(Reason::External);bridge_->new_lease_ready(false);}
    revoked_=true;
    if(!enter())return;
    if(debt_)handed_=true;
    failed_=true; // unknown external result requires genuine idle readback later
    leave();
}
bool DemandAdapter::external(bool acquiring,Call original,void* ctx) {
    if(bridge_) { bridge_->invalidate(Reason::External); bridge_->new_lease_ready(false); }
    revoked_=true; // happens before forwarding even when callback ordering fails
    if(!enter())return original(ctx); // exact external call must never be swallowed
    if(debt_)handed_=true; // even a failed call makes cleanup ownership uncertain
    bool ok=original(ctx); outcome_={debt_tag_,DemandSource::External,acquiring,true,ok}; if(!ok)failed_=true;
    // External release does not establish settlement by itself. Require observed
    // K idle and layer debt settlement before allowing a new lease.
    (void)acquiring; leave(); return ok;
}
}
