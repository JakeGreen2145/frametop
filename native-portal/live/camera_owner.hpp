// SPDX-License-Identifier: MIT
#pragma once
#include "../binding/camera.hpp"
#include "../control/owner_coordinator.hpp"
#include "../render/adapter.hpp"
#include <optional>
#include <unistd.h>

namespace native_portal::live {
struct CameraCoverage {
    bool supported=false;
    uint64_t installed_boundaries=0;
    const char* reason="unsupported: camera writers, precleanup/rebind and source settlement are not installed";
};

// One completed camera publication, one backend binding, one owner. Bind before
// publishing this instance to listener threads; join all users before destroying.
// Never reconstruct over debt. Runtime intentionally does not install this class.
class CameraOwner final {
public:
    using NativeMutation=std::function<coordination::NativeReceipt(
        const CameraBinding&,coordination::DemandArbiter&)>;
    CameraOwner() noexcept;
    ~CameraOwner();
    CameraOwner(const CameraOwner&)=delete;
    CameraOwner& operator=(const CameraOwner&)=delete;
    bool observe_publication(uint64_t pc,const arena::Saved&);
    bool bind(const CameraBinding&,uint64_t generation,size_t capacity=8);
    uint64_t epoch() const noexcept {return epoch_;} // immutable after bind
    static CameraCoverage coverage() noexcept {return {};}

    // Caller authentication belongs to the incoming native/connection binding.
    // Capture identities, not unretained native pointers. The callback receives
    // native identities ONLY after lookup with the queue's actual retained ticket.
    // References/identities are borrowed for the callback, not permission to store
    // a native pointer, retain the arbiter, or execute on another thread later.
    coordination::Submission submit(uint64_t epoch,uint64_t generation,NativeMutation);
    size_t pump(size_t budget=coordination::OwnerCoordinator::MaxSlots);
    coordination::Wait wait(uint64_t id,coordination::Result&);
    bool take(uint64_t id,coordination::Result&);
    bool retry_release(uint64_t id);

    // Nonblocking owner-only close. Cancels unexecuted mutations, not native debt.
    // A real gate ticket acquired at bind protects drain after admission closes.
    bool begin_close();
    coordination::NativeReceipt drain(const NativeMutation&);
    bool finish_close(); // only after debt release AND proven native disconnect
    binding::CameraNativeBinding::Dispatch cleanup(uint64_t pc,arena::Saved&,
        CameraAccessGate::Cleanup,void*,std::chrono::milliseconds timeout);
private:
    bool owner() const noexcept;
    coordination::Retention retention() noexcept;
    coordination::NativeReceipt invoke(uint64_t ticket,const NativeMutation&);
    const int process_;
    const std::thread::id owner_;
    CameraAccessGate gate_;
    binding::CameraNativeBinding native_;
    std::optional<coordination::OwnerCoordinator> queue_;
    std::optional<coordination::DemandArbiter> demand_;
    uint64_t epoch_=0,barrier_=0;
    bool bound_=false,active_=false,closing_=false;
};

struct SourceSettlement {
    void* context=nullptr;
    // Must authenticate original source consumers/reuse generation, not just a
    // queue handle or camera flags. There is NO default live source proof.
    bool (*settled)(void*,uint64_t original_ticket_watermark) noexcept=nullptr;
};
// Bridge to the EXISTING Adapter's exact Command/Lifecycle settlement, not a new
// CPU/GPU truth registry. Construct after both immutable tickets are recorded,
// before native commitment; keep stable until its one arbiter debt is released.
// Prevent the Adapter from recording a successor before releasing this debt.
class RenderDebt final {
public:
    RenderDebt(render::Adapter&,uint64_t original_ticket_watermark,SourceSettlement={}) noexcept;
    RenderDebt(const RenderDebt&)=delete;
    RenderDebt& operator=(const RenderDebt&)=delete;
    coordination::Settlement callback() noexcept;
private:
    static bool settled(void*,uint64_t debt);
    render::Adapter& adapter_;
    const uint64_t watermark_;
    const SourceSettlement source_;
    uint64_t debt_=0;
};
} // namespace native_portal::live
