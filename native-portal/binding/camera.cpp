// SPDX-License-Identifier: MIT
#include "camera.hpp"
#include <unistd.h>
namespace native_portal::binding {
CameraNativeBinding::CameraNativeBinding(CameraAccessGate& gate) noexcept:gate_(gate),process_(getpid()) {}
bool CameraNativeBinding::observe_publication(uint64_t pc,const arena::Saved& s) {
    if(process_!=getpid() || pc!=0x8dc1c || !s.sp || !s.x[19] || !s.x[26] ||
       s.x[19]>UINT64_MAX-0x20000 || s.x[20]!=s.x[19]+0x20000)return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if(camera_ || epoch_ || unrelated_inflight_)return false;
    camera_=s.x[26];application_=s.x[19];return true;
}
bool CameraNativeBinding::bind(const CameraBinding& full,std::thread::id owner,uint64_t& out) {
    out=0;if(process_!=getpid())return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if(!camera_ || full.camera!=camera_ || full.application!=application_ ||
       epoch_ || unrelated_inflight_ || !gate_.publish(full,owner,out))return false;
    epoch_=out;return true;
}
bool CameraNativeBinding::publish_completed(uint64_t pc,const arena::Saved& s,
    const CameraBinding& full,std::thread::id owner,uint64_t& out) {
    out=0;
    return full.application==s.x[19] && full.camera==s.x[26] &&
        observe_publication(pc,s) && bind(full,owner,out);
}
CameraNativeBinding::Dispatch CameraNativeBinding::cleanup(uint64_t pc,arena::Saved& s,
    CameraAccessGate::Cleanup original,void* argument,std::chrono::milliseconds timeout) {
    s.pad=arena::Unsupported;
    if(process_!=getpid() || (pc!=0x15e1c8 && pc!=0x15df70 && pc!=0x8b020 && pc!=0x162360))return Dispatch::Refused;
    uint64_t e=0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(epoch_ && (camera_==s.x[0] || (pc==0x8b020 && application_==s.x[0]) || pc==0x162360)) {
            e=epoch_;
            // Close under the binding lock so a publisher cannot race this
            // classification. Neither lock is held across the native call.
            if(!gate_.begin_close(e))return Dispatch::Refused;
        } else if(original) {
            // Pending publication is not retention. Revoke it BEFORE any
            // unbound native cleanup, so later backend readiness cannot bind it.
            if(camera_==s.x[0] || (pc==0x8b020 && application_==s.x[0]) || pc==0x162360) {camera_=0;application_=0;}
            if(unrelated_inflight_==UINT64_MAX)return Dispatch::Refused;
            ++unrelated_inflight_;
        }
    }
    if(!original)return Dispatch::Refused;
    if(!e) {
        original(argument);
        std::lock_guard<std::mutex> lock(mutex_);
        --unrelated_inflight_;
        s.pad=arena::SkipOwned;
        return Dispatch::ForwardedUnrelated;
    }
    const auto kind=pc==0x15df70?CameraAccessGate::CleanupKind::Destroy:CameraAccessGate::CleanupKind::Resources;
    if(gate_.close_and_cleanup(e,kind,original,argument,timeout)!=CameraAccessGate::CleanupResult::Completed)
        return Dispatch::Refused;
    if(kind==CameraAccessGate::CleanupKind::Destroy) {
        std::lock_guard<std::mutex> lock(mutex_);
        // No publication can replace the generation until this record retires.
        if(epoch_==e) {epoch_=0;camera_=0;application_=0;}
    }
    s.pad=arena::SkipOwned;
    return Dispatch::Completed;
}
} // namespace native_portal::binding
