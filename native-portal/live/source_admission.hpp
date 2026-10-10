// SPDX-License-Identifier: MIT
#pragma once
#include "camera_owner.hpp"
#include "source_provider.hpp"
namespace native_portal::live {
// Explicit provider boundary. The default Runtime has no provider and cannot
// acquire portal demand. Nonnull native IDs, Release=1 and camera flags are NOT
// implementations of this interface. No exported runtime truth setter exists.
// Implementations must pin/authenticate provider, consumer/header identities and
// distinguish actual per-queue ownership from partial/unobserved native results.
struct SourceAdmission {
    virtual ~SourceAdmission()=default;
    // Observations of the original retained invocation. These notifications do
    // not confer producer readiness, header generations or release authority.
    virtual void before(const CameraBinding&,coordination::Action) noexcept {}
    virtual void observe(const CameraBinding&,ClientSourceBinding::Module,uint64_t,const arena::Saved&) noexcept {}
    virtual void after(const CameraBinding&,const ClientSourceBinding::Observation&) noexcept {}
    virtual bool ready(const CameraBinding&) noexcept=0;
    virtual coordination::NativeReceipt transition(const CameraBinding&,
        const coordination::ActionPlan&,uint64_t actual_native_result) noexcept=0;
    // Current path preserves native sampled resources and command. This is NOT
    // a request for an extra DMA fence. Prove that original source-consumer
    // lifetime, or stay false; Command/Lifecycle independently prove CPU/GPU.
    virtual bool settled(const CameraBinding&,uint64_t original_ticket_watermark) noexcept=0;
};
}
