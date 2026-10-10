// SPDX-License-Identifier: MIT
#pragma once
#include "camera_owner.hpp"
#include "source_provider.hpp"
namespace native_portal::live {
// Production Runtime installs NativeSourceAdmission. Explicit injected instances
// remain a CPU fixture boundary. Nonnull IDs, Release=1 and camera flags alone
// grant neither frame borrowing nor detach. No runtime truth setter is exported.
struct SourceAdmission {
    virtual ~SourceAdmission()=default;
    // Observations of the original retained invocation. These notifications do
    // not confer producer readiness, header generations or release authority.
    virtual void before(const CameraBinding&,coordination::Action) noexcept {}
    virtual void observe(const CameraBinding&,ClientSourceBinding::Module,uint64_t,const arena::Saved&) noexcept {}
    virtual void after(const CameraBinding&,const ClientSourceBinding::Observation&) noexcept {}
    // Actual aggregate Release decision selected the native no-op continuation.
    // It is neither Disconnect nor a queue receipt.
    virtual void held_disconnect(const CameraBinding&) noexcept {}
    virtual bool connect_eligible(const CameraBinding&) noexcept=0;
    virtual bool transition_eligible(const CameraBinding&,coordination::Action) noexcept=0;
    virtual bool borrow_ready(const CameraBinding&,render::NativeBorrow) noexcept=0;
    virtual bool backend(const CameraBinding&,uint64_t ticket,const render::Recorded&,uint64_t command,uint64_t recording) noexcept=0;
    virtual coordination::NativeReceipt transition(const CameraBinding&,
        const coordination::ActionPlan&,uint64_t actual_native_result) noexcept=0;
    // Independent provider/consumer lifetime fact. Runtime additionally requires
    // its native sampled-record/reverse-release attribution and existing exact
    // Command/Lifecycle certificates. Metadata return is not GPU completion.
    // This is NOT a request for an extra DMA fence or a new source reader.
    virtual bool settled(const CameraBinding&,uint64_t original_ticket_watermark) noexcept=0;
};
}
