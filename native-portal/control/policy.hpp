#pragma once
#include <atomic>
#include <cstdint>
#include <thread>
namespace native_portal {
class Backend;
struct PolicyInput {
    bool enabled=false, request=false, construct=false, special=false, forced=false, p0=false, p1=false, suppression=false;
    uint32_t style=0; float gain=0,base=0;
    // A same-frame, zero-override native ramp publication with no alternate
    // background handles. Mode selection alone never grants this authority.
    bool normal_construct=false;
};
struct PolicyOutput { bool demand=false,construct=false; uint32_t style=0; float alpha=0; };
PolicyOutput pure_policy(const PolicyInput&,bool owned);
bool activity(bool available,bool f5d,bool construct,bool f5a,float alpha);
bool background(bool active,bool native_portal,float alpha);
// On disagreement the caller must revoke the owned frame, not guess a reason.
bool background_permission(bool original,bool owned,bool stock_active,float stock_alpha,float effective_alpha,bool native_portal,bool effective_active);
struct FramePolicy { PolicyOutput stock,effective; bool portal=false,disagreement=false; };
FramePolicy frame_policy(const PolicyInput&,bool lease,bool tracking,bool safety,bool stock_active);
// There is no native owner-token refcount. This ledger records only our layer
// transition debt and real callback outcomes. The adapter is not a lock around
// Valve: an off-thread/recursive callback revokes and permanently disables it.
struct DemandTag { uint64_t epoch=0,origin=0,frame=0; };
enum class DemandSource { Stock, Owned, External };
struct NativeOutcome { DemandTag tag; DemandSource source=DemandSource::Stock; bool acquire=false,forwarded=false,success=false; };
class DemandAdapter {
public:
    using Call=bool(*)(void*);
    explicit DemandAdapter(Backend* bridge=nullptr);
    void establish(bool ordering_proven,bool native_idle);
    // Narrow original-native-path capability, NOT a native-idle assertion.
    // Never recovers an unknown handoff/failure or clears existing demand debt.
    void establish_native_path(bool ordering_proven,bool connection_eligible);
    bool can_acquire() const { return ready_.load()&&!revoked_.load()&&!failed_.load()&&!unsafe_.load(); }
    bool bind(DemandTag);
    DemandTag debt_tag() const { return debt_tag_; }
    NativeOutcome last_outcome() const { return outcome_; }
    bool owned_debt() const { return debt_; } // owning callback thread only
    bool layer(bool stock,bool owned,Call acquire,Call release,void* context);
    bool external(bool acquiring,Call original,void* context);
    // Owner-thread notice: another native thread already forwarded an external
    // call. No success is inferred, no native call is replayed or suppressed here.
    void external_observed();
private:
    bool enter(); void leave();
    Backend* bridge_; const std::thread::id owner_; std::atomic_flag busy_=ATOMIC_FLAG_INIT;
    std::atomic<bool> ready_{false},revoked_{false},failed_{false},unsafe_{false};
    bool debt_=false, handed_=false, stock_=false,effective_=false;
    DemandTag bound_{},debt_tag_{}; NativeOutcome outcome_{};
};
}
