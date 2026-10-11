// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stddef.h>

// Separate from the legacy 20-bit camera mask. This is installed site coverage,
// NOT a command-generation, lock-success, producer, graph or readiness proof.
#define FT_STOCK_OBSERVER_COVERAGE_VERSION 1u
#define FT_INSTALLED_SITES_VERSION 1u
#define FT_STOCK_OBSERVER_SITE_COUNT 44u
#define FT_STOCK_OBSERVER_CLIENT_TAG (UINT64_C(1) << 63)
#define FT_STOCK_COMMAND_OUTPUT          (UINT64_C(1) << 0) // compositor 304ecc
#define FT_STOCK_PUBLICATION_COMMITTED   (UINT64_C(1) << 1) // compositor 2a23f8
#define FT_STOCK_WRAPPER_COMMAND         (UINT64_C(1) << 2) // compositor 3052e0
#define FT_STOCK_POOL_RETURN_CALL        (UINT64_C(1) << 3) // compositor 3052ec
#define FT_STOCK_METADATA_LOCK_RESULT    (UINT64_C(1) << 4) // client 2584c4
#define FT_STOCK_METADATA_GUARD_BEGIN    (UINT64_C(1) << 5) // client 258438
#define FT_STOCK_METADATA_GUARD_RETURN   (UINT64_C(1) << 6) // client 2584f8
#define FT_STOCK_METADATA_GUARD_REVOKE   (UINT64_C(1) << 7) // client 258508
#define FT_STOCK_TURNOVER_WRAPPER            (UINT64_C(1) << 8) // compositor 2a1e3c
#define FT_STOCK_TURNOVER_COMMAND            (UINT64_C(1) << 9) // compositor 2a1f64
#define FT_STOCK_BEGIN_VK                    (UINT64_C(1) << 10) // compositor 300a28
#define FT_STOCK_COMMAND_CTOR_CALL           (UINT64_C(1) << 11) // compositor 301800
#define FT_STOCK_COMMAND_CTOR_RETURN         (UINT64_C(1) << 12) // compositor 301804
#define FT_STOCK_COMMAND_DEVICE              (UINT64_C(1) << 13) // compositor 303d10
#define FT_STOCK_COMMAND_POOL                (UINT64_C(1) << 14) // compositor 303d70
#define FT_STOCK_POOL_RETURN_RESULT          (UINT64_C(1) << 15) // compositor 3052f0
#define FT_STOCK_WRAPPER_CTOR_ENTRY          (UINT64_C(1) << 16) // compositor 305308
#define FT_STOCK_WRAPPER_CTOR_PRERET         (UINT64_C(1) << 17) // compositor 3053a8
#define FT_STOCK_FACTORY_PRERET              (UINT64_C(1) << 18) // compositor 305470
#define FT_STOCK_COMMAND_OUTPUT_STORE        (UINT64_C(1) << 19) // compositor 304ec8
// 304ecc also has a failed-creation predecessor. COMMAND_OUTPUT is only a site
// bit. A positive C requires the same invocation's COMMAND_OUTPUT_STORE event
// before the join. PRERET bits describe intent, not actual caller completion.
// POOL_RETURN_RESULT includes the null no-call branch; only paired nonnull call
// plus literal low-byte result 1 can certify container return, never GPU work.
#define FT_STOCK_FACTORY_ENTRY               (UINT64_C(1) << 20) // compositor 3053f8
#define FT_STOCK_HIDDEN_OUTPUT_ENTRY         (UINT64_C(1) << 21) // compositor 302790
#define FT_STOCK_POOL_POP_ENTRY              (UINT64_C(1) << 22) // compositor 304e60
#define FT_STOCK_SOURCE_15F9FC               (UINT64_C(1) << 23) // compositor camera_fact
#define FT_STOCK_SOURCE_15FA60               (UINT64_C(1) << 24) // compositor camera_fact
#define FT_STOCK_SOURCE_15FA94               (UINT64_C(1) << 25) // compositor camera_fact
#define FT_STOCK_SOURCE_15FBC4               (UINT64_C(1) << 26) // compositor camera_fact
#define FT_STOCK_SOURCE_15FD30               (UINT64_C(1) << 27) // compositor camera_fact
#define FT_STOCK_SOURCE_15FA04               (UINT64_C(1) << 28) // compositor camera_fact
#define FT_STOCK_SOURCE_15FA8C               (UINT64_C(1) << 29) // compositor camera_fact
#define FT_STOCK_SOURCE_15FABC               (UINT64_C(1) << 30) // compositor camera_fact
#define FT_STOCK_SOURCE_15FBC0               (UINT64_C(1) << 31) // compositor camera_fact
#define FT_STOCK_SOURCE_15FC4C               (UINT64_C(1) << 32) // compositor camera_fact
#define FT_STOCK_SOURCE_15FD48               (UINT64_C(1) << 33) // compositor camera_fact
#define FT_STOCK_SOURCE_15FD90               (UINT64_C(1) << 34) // compositor camera_fact
#define FT_STOCK_SOURCE_15FBD0               (UINT64_C(1) << 35) // compositor camera_fact
#define FT_STOCK_SOURCE_15FC08               (UINT64_C(1) << 36) // compositor camera_fact
#define FT_STOCK_SOURCE_16013C               (UINT64_C(1) << 37) // compositor camera_fact
#define FT_STOCK_SOURCE_32A628               (UINT64_C(1) << 38) // client camera_fact
#define FT_STOCK_SOURCE_3280EC               (UINT64_C(1) << 39) // client boundary_event
#define FT_STOCK_SOURCE_327678               (UINT64_C(1) << 40) // client boundary_event
#define FT_STOCK_SOURCE_328150               (UINT64_C(1) << 41) // client boundary_event
#define FT_STOCK_SOURCE_3276D8               (UINT64_C(1) << 42) // client boundary_event
#define FT_STOCK_FACTORY_OUTPUT_COMMITTED (UINT64_C(1) << 43) // compositor 305490
#define FT_STOCK_OBSERVER_REQUIRED UINT64_C(0xfffffffffff)

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
// Actual selected, committed callback inventory for collector install_coverage.
// module_tag is 0 for compositor or FT_STOCK_OBSERVER_CLIENT_TAG for client.
// Entries are the same module-tagged relative PCs passed to the event callback.
// Returns required count. Null output or insufficient capacity writes nothing.
// Returns zero if version/tag invalid, forked, disabled or either module's
// installed lifetime unavailable. Not a required-sites list or a readiness claim.
size_t ft_native_portal_installed_sites(uint32_t version,uint64_t module_tag,
    uint64_t *pcs,size_t capacity);

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
