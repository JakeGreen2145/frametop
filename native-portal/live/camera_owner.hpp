// SPDX-License-Identifier: MIT
#pragma once
#include "../binding/camera.hpp"
#include "../control/owner_coordinator.hpp"
#include "../render/adapter.hpp"
#include <optional>
#include <atomic>
#include <unistd.h>

namespace native_portal::live {
struct CameraCoverage {
    bool supported=false;
    uint64_t installed_boundaries=0;
    const char* reason="unsupported: camera writers, precleanup/rebind and source settlement are not installed";
};

// One completed camera publication, one backend binding, one owner. Bind before
// publishing this instance to listener threads; join all users before destroying.
// Never reconstruct over debt. Runtime owns this instance across bootstrap/Run.
class CameraOwner final {
public:
    using NativeMutation=std::function<coordination::NativeReceipt(
        const CameraBinding&,coordination::DemandArbiter&)>;
    CameraOwner() noexcept;
    ~CameraOwner();
    CameraOwner(const CameraOwner&)=delete;
    CameraOwner& operator=(const CameraOwner&)=delete;
    enum class BindStage : uint8_t { Unattempted, CleanRefused, Published, Retained, Closed };
    BindStage bind_stage() const noexcept {return bind_stage_.load();}
    binding::CameraNativeBinding::Candidate candidate() const {return native_.candidate();}
    void invalidate_publication(uint64_t pc) {native_.invalidate_publication(pc);}
    bool observe_publication(uint64_t pc,const arena::Saved&);
    bool bind(const CameraBinding&,uint64_t generation,size_t capacity=8);
    // Runtime calls ONLY after Evidence authenticates the actual Run handoff.
    // The pending publication stays in native_, not reconstructed from Run reads.
    bool bind_handoff(const CameraBinding&,uint64_t generation,size_t capacity=8);
    coordination::NativeReceipt direct(const NativeMutation&);
    bool retained_binding(CameraBinding&) const;
    // Lifetime ONLY. Authenticate the incoming caller and expected epoch/generation
    // before calling, and retain BEFORE any native pointer lookup. No owner pump,
    // native operation, graph/mutation permission or arbiter access is provided.
    // Keep through the COMPLETE original invocation. Join all users and destroy
    // every handle before destroying CameraOwner. A handle is not thread-safe;
    // externally synchronized moves/releases on other threads are permitted.
    class RetainedCaller final {
    public:
        RetainedCaller() noexcept=default;
        ~RetainedCaller(); // failed fallback release is fail-stop, never forgotten
        RetainedCaller(const RetainedCaller&)=delete;
        RetainedCaller& operator=(const RetainedCaller&)=delete;
        RetainedCaller(RetainedCaller&&) noexcept;
        RetainedCaller& operator=(RetainedCaller&&) noexcept;
        explicit operator bool() const noexcept {return ticket_!=0;}
        bool binding(CameraBinding&) const; // immutable scalars via this real ticket
        using Observer=std::function<void(CameraAccessGate&,uint64_t,const CameraBinding&)>;
        // Synchronous existing-ticket borrow for an invocation-local collector.
        // Keep this handle alive/unmoved/unreleased throughout fn. Do not release
        // its ticket through gate or retain the binding reference. No new ticket,
        // native field read permission, mutation authority or owner work is issued.
        bool observe_scope(const Observer& fn) const;
        bool release() noexcept; // false preserves a live handle/accounting for retry
    private:
        friend class CameraOwner;
        RetainedCaller(CameraOwner& owner,uint64_t ticket) noexcept:owner_(&owner),ticket_(ticket) {}
        CameraOwner* owner_=nullptr;
        uint64_t ticket_=0;
    };
    RetainedCaller retain_caller(uint64_t expected_epoch,uint64_t expected_generation);
    // Only within an active owner mutation. This is the SAME FIFO ticket or
    // pre-close barrier, never an additional retain. Gate/identities are borrowed
    // for this callback. The Runtime observer must not outlive this CameraOwner.
    using ScopeObserver=std::function<void(CameraAccessGate&,uint64_t,const CameraBinding&)>;
    bool observe_scope(const ScopeObserver&);
    coordination::DemandSnapshot snapshot() const;
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
    // Integration barrier only: all callers/queue/debts released and attempted
    // native work detached. This does NOT execute cleanup or assert global idle.
    bool finish_close();
    binding::CameraNativeBinding::Dispatch cleanup(uint64_t pc,arena::Saved&,
        CameraAccessGate::Cleanup,void*,std::chrono::milliseconds timeout,bool initial_caller=false);
private:
    bool owner() const noexcept;
    coordination::Retention retention() noexcept;
    coordination::NativeReceipt invoke(uint64_t ticket,const NativeMutation&);
    const int process_;
    std::thread::id owner_;
    CameraAccessGate gate_;
    binding::CameraNativeBinding native_;
    std::optional<coordination::OwnerCoordinator> queue_;
    std::optional<coordination::DemandArbiter> demand_;
    uint64_t epoch_=0,barrier_=0,generation_=0,current_ticket_=0;
    bool bound_=false,active_=false,closing_=false;
    // Lock order is callers_mutex_ -> gate_. No callback or native operation
    // under this mutex. It joins successful retain/accounting with finish_close.
    std::mutex callers_mutex_;
    size_t callers_=0;
    std::atomic<BindStage> bind_stage_{BindStage::Unattempted};
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
