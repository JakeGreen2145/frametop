// SPDX-License-Identifier: MIT
#pragma once
#include "../control/camera_gate.hpp"
#include "../arena/bridge.hpp"

namespace native_portal::binding {
// Source-equivalent binding for THREE established native boundaries only.
// Not installed by the live loader. Parent must integrate every selected entry,
// application precleanup and rebind before permitting any live camera admission.
class CameraNativeBinding final {
public:
    enum class Dispatch { Completed, ForwardedUnrelated, Refused };
    explicit CameraNativeBinding(CameraAccessGate&) noexcept;
    // Graphics initialization follows camera publication. Save only register
    // facts now, without an epoch/read scope, and bind later under independently
    // retained backend/association ownership. Any intervening cleanup revokes
    // these pending facts. No native pointer lookup is done by either method.
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
                     void* argument,std::chrono::milliseconds timeout);
private:
    CameraAccessGate& gate_;
    const int process_;
    std::mutex mutex_;
    uint64_t epoch_=0,camera_=0,application_=0,unrelated_inflight_=0;
};
} // namespace native_portal::binding
