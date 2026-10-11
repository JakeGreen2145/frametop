// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stddef.h>

// Separate from the legacy 20-bit camera mask. This is installed site coverage,
// NOT a command-generation, lock-success, producer, graph or readiness proof.
#define FT_STOCK_OBSERVER_COVERAGE_VERSION 1u
#define FT_STOCK_OBSERVER_SITE_COUNT 8u
#define FT_STOCK_OBSERVER_CLIENT_TAG (UINT64_C(1) << 63)
#define FT_STOCK_COMMAND_OUTPUT          (UINT64_C(1) << 0) // compositor 304ecc
#define FT_STOCK_PUBLICATION_COMMITTED   (UINT64_C(1) << 1) // compositor 2a23f8
#define FT_STOCK_WRAPPER_COMMAND         (UINT64_C(1) << 2) // compositor 3052e0
#define FT_STOCK_POOL_RETURN_CALL        (UINT64_C(1) << 3) // compositor 3052ec
#define FT_STOCK_METADATA_LOCK_RESULT    (UINT64_C(1) << 4) // client 2584c4
#define FT_STOCK_METADATA_GUARD_BEGIN    (UINT64_C(1) << 5) // client 258438
#define FT_STOCK_METADATA_GUARD_RETURN   (UINT64_C(1) << 6) // client 2584f8
#define FT_STOCK_METADATA_GUARD_REVOKE   (UINT64_C(1) << 7) // client 258508
#define FT_STOCK_OBSERVER_REQUIRED UINT64_C(0xff)

struct ft_stock_observer_coverage_v1 {
    uint32_t version;
    uint32_t size;
    uint64_t installed_mask;
    uint64_t required_mask;
    uint64_t compositor_base;
    uint64_t client_base;
};

#ifdef __cplusplus
extern "C" {
#endif
// Returns 1 for an authenticated installed snapshot, even if its mask is partial.
// Returns 0 with a zero snapshot when modules are unavailable, forked or disabled.
// Unknown version/size or null output returns 0 without touching the output.
// A consumer must require ALL required bits, then separately authenticate every
// invocation and native result. Missing export on an old owner means unsupported.
int ft_native_portal_stock_observer_coverage(uint32_t version,
    struct ft_stock_observer_coverage_v1 *out, size_t size);
#ifdef __cplusplus
}
#endif

static inline int ft_stock_observers_complete(const struct ft_stock_observer_coverage_v1 *s) {
    return s && s->version==FT_STOCK_OBSERVER_COVERAGE_VERSION &&
        s->size==sizeof(*s) && s->required_mask==FT_STOCK_OBSERVER_REQUIRED &&
        s->compositor_base && s->client_base &&
        (s->installed_mask & FT_STOCK_OBSERVER_REQUIRED)==FT_STOCK_OBSERVER_REQUIRED;
}
