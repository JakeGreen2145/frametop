// SPDX-License-Identifier: MIT
#pragma once
#include "../control/camera_gate.hpp"
#include "../arena/bridge.hpp"

namespace native_portal::binding {
// Completed publication and synchronous destructive-boundary interlock.
// Runtime supplies authenticated owner/bypass execution. Rebind conservatively
// closes the bound generation until an exact association contract is available.
class CameraNativeBinding final {
public:
    enum class Dispatch { Completed, ForwardedUnrelated, Refused };
    enum class Invalidation : uint8_t { None, InitialInit, ResourceRebind, ObjectCleanup, Replacement };
    enum class Rejection : uint8_t { None, Process, Site, Stack, Application, Camera, Relation,
        Existing, Inflight, Missing, CameraMismatch, AppMismatch, Epoch, Gate };
    // Immutable copy under mutex, never an unretained native read permission.
    struct Candidate {
        uint64_t camera=0,application=0,sequence=0,version=0,invalidations=0;
        uint64_t seen_pc=0,last_invalidate_pc=0,invalidation_sequence=0,inflight=0;
        bool accepted=false,init_seen=false,resource_ready=false,epoch_published=false;
        Invalidation classification=Invalidation::None;
        Rejection rejection=Rejection::None;
        CameraAccessGate::PublishFailure gate_rejection=CameraAccessGate::PublishFailure::None;
    };
    explicit CameraNativeBinding(CameraAccessGate&) noexcept;
    Candidate candidate() const;
    void invalidate_publication(uint64_t pc);
    // Graphics initialization follows camera publication. Save only register
    // facts now, without an epoch/read scope, and bind later under independently
    // retained backend/association ownership. Destructive/unclassified cleanup
    // revokes pending facts; the authenticated first same-object resource Init
    // preserves P and separately records its actual returned resource readiness.
    // No native pointer lookup is done by either method.
    bool observe_publication(uint64_t pc,const arena::Saved&);
    bool bind(const CameraBinding& full,std::thread::id owner,uint64_t& epoch_out);
    // Call only from the validated 8dc1c site, after the 8dc18 publication store.
    // full carries independently authenticated context/backend/association facts.
    // x26=camera, x19=app, x20=app+0x20000; x0 is NOT the camera.
    // Constructor observations deliberately grant neither epochs nor read scopes.
    bool publish_completed(uint64_t pc,const arena::Saved&,const CameraBinding& full,
                           std::thread::id owner,uint64_t& epoch_out);
    // Exact x0 at 15e1c8/15df70 BEFORE displaced destructive instructions.
    // original must synchronously invoke the complete original native method
    // through a bypass trampoline and preserve its real outputs. It must NOT
    // merely mark an observer and then let destruction resume after this call.
    // Refused never invokes original. Sets the emitted selector to Unsupported
    // before checking anything; only synchronous native return selects SkipOwned
    // (the method-return route). Refusal therefore traps, never falls through.
    // Unrelated cameras are forwarded unchanged through the same original call.
    Dispatch cleanup(uint64_t pc,arena::Saved&,CameraAccessGate::Cleanup original,
                     void* argument,std::chrono::milliseconds timeout,bool initial_caller=false);
private:
    void invalidate_locked(uint64_t pc,Invalidation);
    CameraAccessGate& gate_;
    const int process_;
    mutable std::mutex mutex_;
    Candidate candidate_{};
    uint64_t epoch_=0,camera_=0,application_=0,unrelated_inflight_=0;
};
} // namespace native_portal::binding
