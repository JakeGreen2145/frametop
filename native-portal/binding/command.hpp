// SPDX-License-Identifier: MIT
#pragma once
#include "../render/adapter.hpp"
#include <thread>
#include <unistd.h>

namespace native_portal::binding {
// Values are copied by an audited native reader. No address is dereferenced here.
struct Owner {
    render::Identity identity{};
    uint64_t recording = 0, generation = 0, command = 0, vk_command = 0;
    uint64_t arena = 0, buffer = 0, map = 0, capacity = 0, alignment = 0;
    uint64_t queue = 0, semaphore = 0, synchronization = 0;
};
struct Scope {
    Owner owner{};
    uint64_t invocation = 0, tid = 0, sp = 0, lr = 0, object = 0;
};
struct Signal {
    uint64_t synchronization = 0, serial = 0;
    int8_t subserial = 0;
};
// Normalized VkSubmitInfo + VkTimelineSemaphoreSubmitInfo, copied at queue entry.
// The reader must reject unreadable arrays, unsupported pNext and count mismatch.
// This is NOT a native Vulkan ABI declaration or proof that live03 read these bytes.
struct SubmitSnapshot {
    bool readable = false, supported_chain = false;
    uint32_t submit_count = 0, command_count = 0, signal_count = 0, timeline_signal_count = 0;
    uint64_t queue = 0, device = 0, command = 0, semaphore = 0, value = 0, fence = 0;
};
struct State {
    bool bound = false, recording_debt = false, success_debt = false;
    bool certificate = false, gpu_complete = false, stopped = false, unsupported = false;
    unsigned admitted = 0, returned = 0;
    bool rejected = false;
};
// One owner, one recording, one command generation, two immutable provenances.
// No reuse/reset API. A new owner requires external lifetime proof for the old one.
class Command {
  public:
    Command() noexcept;
    Command(const Command &) = delete;
    Command &operator=(const Command &) = delete;
    bool bind(const Owner &) noexcept;
    bool backend_enter(const Scope &, const render::Provenance &) noexcept;
    bool backend_return(const Scope &, bool admitted) noexcept;
    // Adapter-issued full-pair actual skip proof, scoped to the registered upload
    // command. No backend admission may have occurred. Observation survives Off.
    bool rejected(const Scope &, uint64_t upload_command, const render::Provenance &,
                  const render::Provenance &) noexcept;
    bool queue_enter(const Scope &, const SubmitSnapshot &, const Signal &) noexcept;
    bool queue_result(const Scope &, int32_t raw_vk_result) noexcept;
    bool stamp_enter(const Scope &queue, const Scope &stamp, const Signal &) noexcept;
    bool resource_enter(const Scope &stamp, const Scope &resource, const Signal &) noexcept;
    bool resource_post(const Scope &, uint64_t resource, const Signal &) noexcept;
    bool stamp_post(const Scope &, uint64_t command, const Signal &) noexcept;
    bool wait_enter(const Scope &, const Signal &, uint64_t semaphore) noexcept;
    bool wait_result(const Scope &, const Signal &, int32_t raw_vk_result) noexcept;
    bool wait_return(const Scope &, const Signal &, uint32_t native_boolean) noexcept;
    void off() noexcept;
    void invalidate_lifetime() noexcept;
    State state() const noexcept;
    bool submission(const render::Provenance &, const render::Provenance &,
                    render::Completion &) const noexcept;
    bool completed(const render::Binding &, const render::Completion &) const noexcept;
    static bool submission_callback(void *, const render::Provenance &,
                                    const render::Provenance &, render::Completion &) noexcept;
    static bool completed_callback(void *, const render::Binding &,
                                   const render::Completion &) noexcept;
    static uint64_t timeline_value(const Signal &) noexcept;
  private:
    friend class Lifecycle; // exact immutable certificate and owner-thread checks only
    bool thread() const noexcept;
    bool scope(const Scope &) const noexcept;
    bool fail() noexcept;
    bool pair_recorded() const noexcept;
    const pid_t pid_;
    const std::thread::id thread_;
    Owner owner_{};
    State state_{};
    bool lifetime_ = false, bad_ = false, queue_seen_ = false, queue_valid_ = false;
    bool result_seen_ = false, stamp_open_ = false, resource_open_ = false;
    bool backend_open_ = false, wait_open_ = false, wait_result_seen_ = false, wait_ok_ = false;
    unsigned backend_eye_ = 0, stamps_ = 0;
    std::array<render::Provenance, 2> tickets_{};
    Scope backend_{}, queue_{}, stamp_{}, resource_{}, wait_{};
    Signal signal_{};
    render::Completion token_{};
};
} // namespace native_portal::binding
