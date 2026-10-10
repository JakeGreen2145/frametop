// SPDX-License-Identifier: MIT
#include "camera.hpp"
#include <unistd.h>
namespace native_portal::binding {
CameraNativeBinding::CameraNativeBinding(CameraAccessGate& gate) noexcept:gate_(gate),process_(getpid()) {}
CameraNativeBinding::Candidate CameraNativeBinding::candidate() const {
    if(process_!=getpid())return {};
    std::lock_guard<std::mutex> lock(mutex_);
    auto c=candidate_;c.camera=camera_;c.application=application_;
    c.inflight=unrelated_inflight_;c.epoch_published=epoch_!=0;return c;
}
void CameraNativeBinding::invalidate_locked(uint64_t pc,Invalidation kind) {
    // Saturation is terminal, never wrap an observation into fresh authority.
    if(candidate_.version!=UINT64_MAX)++candidate_.version;
    if(candidate_.invalidations!=UINT64_MAX)++candidate_.invalidations;
    candidate_.last_invalidate_pc=pc;candidate_.classification=kind;candidate_.invalidation_sequence=candidate_.version;
    candidate_.resource_ready=false;camera_=application_=0;
}
void CameraNativeBinding::invalidate_publication(uint64_t pc) {
    if(process_!=getpid())return;
    std::lock_guard<std::mutex> lock(mutex_);
    if(!epoch_){
        candidate_.seen_pc=pc;candidate_.accepted=false;
        if(candidate_.sequence!=UINT64_MAX)++candidate_.sequence;
        invalidate_locked(pc,Invalidation::Replacement);
    }
}
bool CameraNativeBinding::observe_publication(uint64_t pc,const arena::Saved& s) {
    if(process_!=getpid())return false;
    std::lock_guard<std::mutex> lock(mutex_);
    candidate_.seen_pc=pc;candidate_.accepted=false;
    auto reject=[&](Rejection r){candidate_.rejection=r;return false;};
    if(pc!=0x8dc1c)return reject(Rejection::Site);
    const bool exhausted=candidate_.sequence==UINT64_MAX;
    if(!exhausted)++candidate_.sequence;
    // Even an unsupported completed replacement must revoke the old pending
    // object before validating its new shape. Rejection never retains old P.
    const bool replacement=camera_ && !epoch_;
    if(replacement)invalidate_locked(pc,Invalidation::Replacement);
    if(!s.sp)return reject(Rejection::Stack);
    if(!s.x[19])return reject(Rejection::Application);
    if(!s.x[26])return reject(Rejection::Camera);
    if(s.x[19]>UINT64_MAX-0x20000 || s.x[20]!=s.x[19]+0x20000)return reject(Rejection::Relation);
    if(epoch_)return reject(Rejection::Epoch);
    if(replacement)return reject(Rejection::Existing);
    if(unrelated_inflight_)return reject(Rejection::Inflight);
    if(exhausted || candidate_.version==UINT64_MAX || candidate_.invalidations==UINT64_MAX)
        return reject(Rejection::Existing);
    ++candidate_.version;
    candidate_.accepted=true;candidate_.init_seen=false;candidate_.resource_ready=false;
    candidate_.rejection=Rejection::None;camera_=s.x[26];application_=s.x[19];return true;
}
bool CameraNativeBinding::bind(const CameraBinding& full,std::thread::id owner,uint64_t& out) {
    out=0;if(process_!=getpid())return false;
    std::lock_guard<std::mutex> lock(mutex_);
    auto reject=[&](Rejection r){candidate_.rejection=r;return false;};
    if(!camera_)return reject(Rejection::Missing);
    if(full.camera!=camera_)return reject(Rejection::CameraMismatch);
    if(full.application!=application_)return reject(Rejection::AppMismatch);
    if(epoch_)return reject(Rejection::Epoch);
    if(unrelated_inflight_)return reject(Rejection::Inflight);
    if(!gate_.publish(full,owner,out,&candidate_.gate_rejection))return reject(Rejection::Gate);
    candidate_.rejection=Rejection::None;epoch_=out;return true;
}
bool CameraNativeBinding::publish_completed(uint64_t pc,const arena::Saved& s,
    const CameraBinding& full,std::thread::id owner,uint64_t& out) {
    out=0;
    return full.application==s.x[19] && full.camera==s.x[26] &&
        observe_publication(pc,s) && bind(full,owner,out);
}
CameraNativeBinding::Dispatch CameraNativeBinding::cleanup(uint64_t pc,arena::Saved& s,
    CameraAccessGate::Cleanup original,void* argument,std::chrono::milliseconds timeout,bool initial_caller) {
    s.pad=arena::Unsupported;
    if(process_!=getpid() || (pc!=0x15e1c8 && pc!=0x15df70 && pc!=0x8b020 && pc!=0x162360))return Dispatch::Refused;
    uint64_t e=0,initial_version=0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool initial=pc==0x162360 && initial_caller && !epoch_ && camera_ &&
            camera_==s.x[0] && application_==s.x[1] && !candidate_.init_seen && !unrelated_inflight_;
        const bool related=camera_==s.x[0] || (pc==0x8b020 && application_==s.x[0]) || pc==0x162360;
        if(initial) {
            candidate_.init_seen=true;candidate_.resource_ready=false;
            candidate_.last_invalidate_pc=pc;candidate_.classification=Invalidation::InitialInit;
            if(candidate_.version==UINT64_MAX)return Dispatch::Refused;
            initial_version=++candidate_.version;candidate_.invalidation_sequence=initial_version;
        } else if(related) {
            // Keep bound identities for the closing lifetime, but revoke all
            // pending object and resource facts before destructive forwarding.
            const auto c=camera_,a=application_;
            invalidate_locked(pc,pc==0x162360?Invalidation::ResourceRebind:Invalidation::ObjectCleanup);
            if(epoch_){camera_=c;application_=a;}
        }
        if(epoch_ && related) {
            e=epoch_;
            if(!gate_.begin_close(e))return Dispatch::Refused;
        } else if(original) {
            if(unrelated_inflight_==UINT64_MAX)return Dispatch::Refused;
            ++unrelated_inflight_;
        }
    }
    if(!original)return Dispatch::Refused;
    if(!e) {
        original(argument); // no binding/gate lock across original, exactly once
        std::lock_guard<std::mutex> lock(mutex_);
        --unrelated_inflight_;
        // Nested/concurrent cleanup invalidates version before forwarding. Its
        // return can never resurrect the old object or resource association.
        if(initial_version && camera_ && candidate_.version==initial_version) {
            candidate_.resource_ready=uint32_t(s.x[0])!=0;
            if(candidate_.version!=UINT64_MAX)++candidate_.version;
        }
        s.pad=arena::SkipOwned;return Dispatch::ForwardedUnrelated;
    }
    const auto kind=pc==0x15df70?CameraAccessGate::CleanupKind::Destroy:CameraAccessGate::CleanupKind::Resources;
    if(gate_.close_and_cleanup(e,kind,original,argument,timeout)!=CameraAccessGate::CleanupResult::Completed)
        return Dispatch::Refused;
    if(kind==CameraAccessGate::CleanupKind::Destroy) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(epoch_==e) {epoch_=0;camera_=0;application_=0;}
    }
    s.pad=arena::SkipOwned;return Dispatch::Completed;
}
} // namespace native_portal::binding
