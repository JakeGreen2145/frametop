// SPDX-License-Identifier: MIT
#pragma once
#include "../control/frame.hpp"
#include "../include/ft_aperture.hpp"
#include "../arena/bridge.hpp"
#include <memory>

namespace native_portal::runtime { class Evidence; }
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
    uint64_t headroom_evidence = 0; // legacy CPU fixture only
    arena::Bridge *upstream = nullptr; // same owner/lifetime; must outlive Adapter
    bool legacy_test_upload = false; // explicitly unbound fixture, never live integration
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
// Binding contract, NOT a native extractor: complete_scan certifies that the
// entire dispatch contains exactly these two owned packets (all stock untouched).
// ready certifies actual per-pass state and exact lookup arguments are ready and
// remain usable through encoding in this command generation. False is unsupported.
struct EyePreparation {
    Recorded record;
    Context context;
    Registers lookup;
    Address pass = 0;
    bool ready = false;
};
struct PairPreparation {
    std::array<EyePreparation, 2> eyes{};
    Address command = 0;
    bool complete_scan = false;
    // Complete bounded packet scan, even when cold passes prohibit preparation.
    // No pre-dispatch mutation; only Ready/ColdPass may supply this proof.
    bool suppression_scan = false;
};
struct Completion {
    uint64_t frame = 0, command = 0, synchronization = 0, serial = 0;
    int8_t subserial = 0;
};
// By-value original caller selection. This is a synchronous native borrow, not
// an import lease, source generation or authority to add another reader.
struct NativeBorrow {
    Provenance provenance{};
    uint64_t watermark=0,command=0,invocation_sp=0,camera=0;
    uint32_t resource_index=UINT32_MAX,slot=UINT32_MAX;
    Address selected_texture=0;
    Prefix original{},replacement{};
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
    // During prepare only: execute exact per-pass x0..x3 lookup without drawing,
    // approve its actual pipeline and retain that pipeline's native lifetime
    // through encoding. A cache hit still requires approval. False/null refuses
    // BOTH eyes before commit; backend never calls this callback.
    bool (*pipeline)(void *, const Provenance &, Registers &) noexcept = nullptr;
    // Observe actual successful native queue submission and exact immutable token.
    // Includes a proved rejected pair's uploads/stock command, never invented work.
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
    // Called only after both exact packets took the pre-encoding DROP path.
    // Register the immutable pair on its actual upload command, NOT GPU completion.
    bool (*rejected)(void *, Address command, const Provenance &,
                     const Provenance &) noexcept = nullptr;
    bool (*borrow)(void *, NativeBorrow) noexcept = nullptr;
};
struct EyeInput {
    unsigned eye = 0;
    uint64_t observed_origin = 0;
    Prefix prefix{};
    ft::portal::NativeConstants constants{};
    ft::portal::Mat4 raw_to_standing{};
    uint64_t invocation_sp=0,camera=0;
    uint32_t resource_index=UINT32_MAX,slot=UINT32_MAX;
    Address selected_texture=0;
};
enum class Decision { Stock, Drop, Portal };
struct BackendResult {
    Decision decision = Decision::Drop;
    Address pipeline = 0;
};
struct State {
    bool initialized = false, frame_invalid = true, cpu_done = false, stereo_submitted = false,
         gpu_complete = false, retired = false, cpu_settled = false;
    bool prepared = false, committed = false, pending = false;
    bool rejected = false, queue_observed = false;
    uint64_t pair_generation = 0; // never reused, derived from the reserved ticket identity
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
    friend class native_portal::runtime::Evidence;
    bool staging_owner(const arena::Bridge *, const Identity &,uint64_t base) const noexcept;
    bool staging_write(uint64_t ticket,Address destination,const void *,size_t) noexcept;

  public:
    Adapter(Backend &, FrameGate &, Binding, Callbacks, size_t capacity = 1024,
            uint64_t first_ticket = 1);
    ~Adapter();
    Adapter(const Adapter &) = delete;
    Adapter &operator=(const Adapter &) = delete;
    bool initialize() noexcept;
    bool latch(const Frame &) noexcept;
    bool enqueue(const EyeInput &) noexcept; // only explicit legacy_test_upload
    // Claim an owned invocation BEFORE native upload. Zero still means owned DROP.
    // Constants provide M1/M2 for immutable suffix; final prefix comes at enqueue.
    uint64_t reserve_upload(const EyeInput &, Address camera, Address stock_wrapper) noexcept;
    // Route all nine arena islands here, on the established native owner thread.
    void upstream(arena::Saved &, unsigned site, uint64_t lifetime, uint64_t thread) noexcept;
    bool enqueue(uint64_t ticket, const EyeInput &) noexcept;
    bool outer_return() noexcept;
    // One prepare attempt per retained frame; resolves BOTH pipelines without
    // drawing. Commit rechecks current authorization once, immediately before
    // encoding. Off/expiry after commit stops new work, not the committed pair.
    uint64_t prepare(const PairPreparation &) noexcept;
    bool commit(uint64_t pair_generation) noexcept;
    // Serialized dispatch-only rejection transaction. Missing/changed/duplicate
    // skips retain debt. Never applicable to committed or partially encoded work.
    bool reject_recorded(const PairPreparation &) noexcept;
    bool suppressed(const EyePreparation &, Address command) noexcept;
    BackendResult backend(const Recorded &, const Context &, Registers lookup) noexcept;
    // Always false for rejected pairs, even after saving their native queue token.
    // queue_observed is lifetime evidence; stereo_submitted is owned draw evidence.
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
