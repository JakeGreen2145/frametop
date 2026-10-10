// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstddef>
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>
namespace native_portal::live {
// Scalar, by-value publication. Match values: 0 unevaluated, 1 false, 2 true.
// No field here is binding authority. Sequence/version are local observations.
struct CameraBindingDiagnostic {
    uint64_t publication_pc=0,publication_accepted=0,publication_sequence=0,publication_rejection=0;
    uint64_t pending=0,native_pending=0,version=0,invalidate_pc=0,invalidate_class=0,invalidations=0,invalidation_sequence=0;
    uint64_t resource_ready=0,run_seen=0,run_authenticated=0,run_rejection=0,start_scope=0;
    uint64_t join_attempts=0,app_match=0,worker_match=0,context_match=0,join_rejection=0,native_rejection=0;
    uint64_t gate_rejection=0;
    uint64_t epoch_published=0,barrier_retained=0,internal_bound=0,runtime_bound=0,partial=0,inflight=0,first_failure=0;
};
// Bounded stack formatting is also safe on fatal ingress. One socket send, no
// locks, native reads, allocations, retries, or fallback writes at that boundary.
inline size_t format_camera_binding(const CameraBindingDiagnostic& d,char* out,size_t size) noexcept {
    if(size<1536)return 0;
    char* p=out;
    auto literal=[&](const char* text){for(size_t i=0;i<64 && text[i];++i)*p++=text[i];};
    auto field=[&](const char* name,uint64_t value){literal(name);*p++='=';*p++='0';*p++='x';
        for(unsigned i=0;i<16;++i)*p++="0123456789abcdef"[(value>>(60-4*i))&15];
        *p++=' ';};
    literal("ft-camera-binding ");
#define FT_CAMERA_FIELD(name) field(#name,d.name)
    FT_CAMERA_FIELD(publication_pc);FT_CAMERA_FIELD(publication_accepted);FT_CAMERA_FIELD(publication_sequence);FT_CAMERA_FIELD(publication_rejection);
    FT_CAMERA_FIELD(pending);FT_CAMERA_FIELD(native_pending);FT_CAMERA_FIELD(version);FT_CAMERA_FIELD(invalidate_pc);FT_CAMERA_FIELD(invalidate_class);FT_CAMERA_FIELD(invalidations);FT_CAMERA_FIELD(invalidation_sequence);
    FT_CAMERA_FIELD(resource_ready);FT_CAMERA_FIELD(run_seen);FT_CAMERA_FIELD(run_authenticated);FT_CAMERA_FIELD(run_rejection);FT_CAMERA_FIELD(start_scope);
    FT_CAMERA_FIELD(join_attempts);FT_CAMERA_FIELD(app_match);FT_CAMERA_FIELD(worker_match);FT_CAMERA_FIELD(context_match);FT_CAMERA_FIELD(join_rejection);FT_CAMERA_FIELD(native_rejection);
    FT_CAMERA_FIELD(gate_rejection);FT_CAMERA_FIELD(epoch_published);FT_CAMERA_FIELD(barrier_retained);FT_CAMERA_FIELD(internal_bound);FT_CAMERA_FIELD(runtime_bound);FT_CAMERA_FIELD(partial);FT_CAMERA_FIELD(inflight);FT_CAMERA_FIELD(first_failure);
#undef FT_CAMERA_FIELD
    p[-1]='\n';return size_t(p-out);
}
inline void emit_camera_binding(const CameraBindingDiagnostic& d) noexcept {
    const auto error=errno;char line[1536];const auto n=format_camera_binding(d,line,sizeof line);
    (void)send(STDERR_FILENO,line,n,MSG_DONTWAIT|MSG_NOSIGNAL);errno=error;
}
}
