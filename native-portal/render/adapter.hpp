// SPDX-License-Identifier: MIT
#pragma once
#include "../control/frame.hpp"
#include "../include/ft_aperture.hpp"
#include <memory>

namespace native_portal::render {
using Address = uint64_t;
using Prefix = std::array<std::byte, 352>;
struct Identity {
    uint64_t process_instance = 0, renderer = 0, renderer_generation = 0, device = 0,
             device_generation = 0;
};
struct Targets {
    Address allocate = 0, upload = 0, descriptor = 0, enqueue = 0, destroy = 0, blend = 0;
};
struct Binding {
    Identity identity;
    Targets targets;
    uint64_t headroom_evidence = 0;
};
// Register-call transport, NOT invented native C++ declarations. A true return
// means the trampoline completed. x[0] is meaningful only for native value returns.
struct Registers {
    Address target = 0;
    std::array<uint64_t, 8> x{};
};
struct Descriptor {
    uint64_t buffer = 0, offset = 0, range = 0;
};
// observe() must return actual coherent bounds, never normalized/frozen pointers.
// Same-begin 0x98-stride append is supported between uploads/backends only.
// recording_generation is binding-issued, stable within a frame and strictly
// increasing across frames even when command/arena pointers are reused. The
// binding must advance it at reset; pointers/extents cannot authenticate a reset.
struct Arena {
    uint64_t pass_begin = 0, pass_end = 0, command_wrapper = 0, command = 0, resource = 0,
             buffer = 0, map = 0;
    uint64_t capacity = 0, cursor = 0, alignment = 0, recording_generation = 0;
};
struct Observation {
    Arena arena;
    Address wrapper_resource = 0;
    uint64_t wrapper_size = 0, wrapper_offset = 0;
    std::array<uint32_t, 8> attachment{};
};
enum class Phase { Initialize, Current, Retire };
struct Provenance {
    Identity identity;
    uint64_t ticket = 0, frame = 0, lease = 0, origin = 0, geometry = 0, deadline = 0,
             revocation = 0, recording_generation = 0;
    unsigned eye = 0;
    Address wrapper = 0, marker = 0;
    uint32_t vs = 0, ps = 0;
    std::array<Descriptor, 2> descriptors{};
};
struct Recorded {
    Prefix prefix{};
    std::array<char, 16> label{};
    uint64_t label_size = 0;
    std::array<Descriptor, 9> descriptors{};
};
struct Context {
    Identity identity;
    uint64_t recording_generation = 0;
};
struct Completion {
    uint64_t frame = 0, command = 0, synchronization = 0, serial = 0;
    int8_t subserial = 0;
};
struct Callbacks {
    void *user = nullptr;
    bool (*validate)(void *, const Binding &, Phase) noexcept = nullptr;
    bool (*call)(void *, Registers &) noexcept = nullptr;
    bool (*observe)(void *, const Binding &, Address wrapper, Address marker,
                    Observation &) noexcept = nullptr;
    // Evidence-backed workload budget. Must reserve space for subsequent native
    // work, not merely confirm that this upload fits.
    bool (*headroom)(void *, const Binding &, const Arena &,
                     uint64_t aligned_end) noexcept = nullptr;
    bool (*stock_blend)(void *, const Binding &, Address) noexcept = nullptr;
    // Execute exact x0..x3 lookup and approve its actual returned pipeline for
    // this authenticated pair, marker and device. False/null means DROP.
    bool (*pipeline)(void *, const Provenance &, Registers &) noexcept = nullptr;
    // Observe actual successful stereo queue submission and exact immutable token.
    bool (*submission)(void *, const Provenance &, const Provenance &,
                       Completion &) noexcept = nullptr;
    bool (*completed)(void *, const Binding &, const Completion &) noexcept = nullptr;
    // Lifecycle validator must establish CPU settlement and native cache teardown.
    // No decoded blend destroy ABI is assumed: lifecycle owner retires marker.
    bool (*retire_marker)(void *, const Binding &, Address) noexcept = nullptr;
    Clock clock = boottime;
    // Read-only, nonblocking native lifetime evidence. True promises all CPU and
    // recording references to BOTH exact tickets are settled, including commands
    // that could be queued again. GPU completion alone does not establish this.
    // No binding means no reclamation. Must not queue work or reauthorize a lease.
    bool (*settled)(void *, const Binding &, const Provenance &, const Provenance &,
                    const Completion &) noexcept = nullptr;
};
struct EyeInput {
    unsigned eye = 0;
    uint64_t observed_origin = 0;
    Prefix prefix{};
    ft::portal::NativeConstants constants{};
    ft::portal::Mat4 raw_to_standing{};
};
enum class Decision { Stock, Drop, Portal };
struct BackendResult {
    Decision decision = Decision::Drop;
    Address pipeline = 0;
};
struct State {
    bool initialized = false, frame_invalid = true, cpu_done = false, stereo_submitted = false,
         gpu_complete = false, retired = false, cpu_settled = false;
    size_t tickets_used = 0; // resident entries, not lifetime identities
    size_t tickets_capacity = 0;
    uint64_t tickets_issued = 0; // cumulative identities reserved by this adapter
};
// Construct/initialize on the established native owner thread, outside callbacks.
// Storage is bounded and recyclable only after exact GPU and CPU/recording
// settlement. Ticket identities never recycle. Keep this owner alive through
// native cache/device teardown, including Off and missing settlement.
class Adapter {
    struct Impl;
    std::unique_ptr<Impl> p_;

  public:
    Adapter(Backend &, FrameGate &, Binding, Callbacks, size_t capacity = 1024,
            uint64_t first_ticket = 1);
    ~Adapter();
    Adapter(const Adapter &) = delete;
    Adapter &operator=(const Adapter &) = delete;
    bool initialize() noexcept;
    bool latch(const Frame &) noexcept;
    bool enqueue(const EyeInput &) noexcept;
    bool outer_return() noexcept;
    BackendResult backend(const Recorded &, const Context &, Registers lookup) noexcept;
    bool submitted() noexcept;
    bool poll_completion() noexcept;
    bool reclaim() noexcept; // no new work; explicit CPU/recording settlement
    void off() noexcept;
    bool retire() noexcept;
    State state() const noexcept; // owner thread only, no native calls
};
std::array<char, 16> encode_ticket(uint64_t) noexcept;
bool decode_ticket(const std::array<char, 16> &, uint64_t length, uint64_t &) noexcept;
bool supported_pair(uint32_t vs, uint32_t ps) noexcept;
} // namespace native_portal::render
