// SPDX-License-Identifier: MIT
#pragma once
#include "../render/adapter.hpp"

namespace native_portal::binding {
using namespace render;
// These are bounded read/ownership operations, not permission to materialize a
// pass. The binding must hold the serialized native recording, cache, attachments
// and device alive until BOTH committed eyes finish. An unbound reader refuses.
// read must validate readable spans, never blindly memcpy an untrusted pointer.
struct PassReader {
    void *user = nullptr;
    bool (*read)(void *, Address, void *, size_t) noexcept = nullptr;
    bool (*live)(void *, const Context &) noexcept = nullptr;
};
struct PassRequest {
    Context context;
    Address marker = 0; // exact adapter-owned immutable marker, not a shader match
    Address image_base = 0; // load bias of the separately hash-verified pinned ELF
    uint32_t dispatch_mode = 0;
};
enum class PassReason {
    Ready, InactiveMode, Lifetime, InvalidSpan, Limit, ReadFailure,
    NoOwned, PartialOrDuplicate, BadLabel, UnsupportedPacket,
    PreDispatchMutation, ColdPass
};
struct PassScan {
    PairPreparation pair{};
    PassReason reason = PassReason::Lifetime;
    size_t owned = 0;
};
inline constexpr size_t pass_limit = 64, packet_limit = 1024, cache_limit = 128;
// Read-only exact warm-cache extraction. A cache miss NEVER invokes 0x2a0638 or
// 0x2a1928. No draw, query, barrier, native allocation or vector mutation occurs.
PassScan extract_passes(PassReader, const PassRequest &) noexcept;
// Feeds the reviewed Adapter. It does not commit, install hooks or publish
// capability. Call only at the serialized dispatch entry 0x2a1e18, w1=0.
uint64_t prepare_passes(Adapter &, PassReader, const PassRequest &) noexcept;
} // namespace native_portal::binding
