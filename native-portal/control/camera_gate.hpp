// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>

namespace native_portal {
// Integration-owned identities, NOT a native memory layout. Publishing these
// scalar facts never dereferences native memory. Backend/association retention
// is supplied by routing their destructive boundaries through this same gate.
struct CameraBinding {
    uint64_t context=0, application=0, camera=0, renderer=0, device=0, association=0;
    uint64_t renderer_generation=0, device_generation=0, association_generation=0;
    bool operator==(const CameraBinding& b) const noexcept {
        return context==b.context && application==b.application && camera==b.camera &&
            renderer==b.renderer && device==b.device && association==b.association &&
            renderer_generation==b.renderer_generation && device_generation==b.device_generation &&
            association_generation==b.association_generation;
    }
};

// One associated generation. Fixed storage, no per-ticket allocation. This gate
// actually withholds cleanup callbacks; it is not an observer next to a free.
// The integration must cover EVERY selected entry and destructive boundary.
// Missing coverage is not made safe by this component, and must keep live
// admission disabled. Retain BEFORE context/app/camera/association pointer reads.
class CameraAccessGate final {
public:
    static constexpr size_t capacity=64;
    enum class CleanupKind { Resources, Destroy };
    enum class CleanupResult { Completed, Stale, Invalid, Timeout, WouldDeadlock };
    using Cleanup = void (*)(void*) noexcept;
    CameraAccessGate() noexcept;
    ~CameraAccessGate(); // outstanding retention or a running cleanup is fatal
    CameraAccessGate(const CameraAccessGate&)=delete;
    CameraAccessGate& operator=(const CameraAccessGate&)=delete;

    // Only after completed native publication and independently authenticated
    // backend/association binding. No replacement, even if closing is quiescent,
    // until Destroy callback has returned. owner must never wait on this barrier.
    bool publish(const CameraBinding&,std::thread::id owner,uint64_t& epoch_out);
    bool retain(uint64_t epoch,uint64_t& ticket_out);
    bool release(uint64_t ticket);
    // Only a live ticket yields identities; remains valid after begin_close.
    // Caller must not concurrently release its ticket while using this copy.
    bool lookup(uint64_t ticket,CameraBinding& out) const;
    bool begin_close(uint64_t epoch);
    // Exactly "no tickets" on this generation, NOT CPU/GPU/source settlement
    // evidence. A transaction keeps its ticket until ALL independent debts end.
    bool quiescent(uint64_t epoch) const;

    // Close admission immediately, then wait boundedly WITHOUT holding the
    // mutex. Never invoke cleanup on timeout/stale/deadlock/invalid input.
    // The caller MUST withhold the destructive native continuation on failure,
    // using its existing explicit fail-stop rather than forging native success.
    // Resources leaves the original generation closed; Destroy retires it only
    // after callback return. Callbacks must complete synchronously, must not
    // throw, and must not schedule destruction for after their return.
    CleanupResult close_and_cleanup(uint64_t epoch,CleanupKind,Cleanup,void*,
                                    std::chrono::milliseconds timeout);
private:
    struct Ticket {uint64_t id=0;std::thread::id issuer;};
    bool same_process() const noexcept;
    bool valid_epoch(uint64_t) const noexcept;
    bool self_wait() const noexcept;
    const int process_;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    CameraBinding binding_{};
    std::array<Ticket,capacity> tickets_{};
    std::thread::id owner_,cleanup_thread_;
    uint64_t epoch_=0;
    size_t active_=0;
    bool closing_=false,cleaning_=false;
};
} // namespace native_portal
