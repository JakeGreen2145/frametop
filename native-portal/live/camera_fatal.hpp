// SPDX-License-Identifier: MIT
#pragma once
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <sys/socket.h>
#include <unistd.h>

namespace native_portal::live {
// Stable, distinct IDs for the abort sites inside camera_route only.
enum class CameraFatalReason : uint8_t {
    DecisionScope=1,
    Publication=2,
    CleanupThread=3,
    CleanupOriginal=4,
    CleanupRefused=5,
    WriterBinding=6,
    NestedMethod=7,
    WriterBeforeHandoff=8,
    MutationDebt=9,
    SubmitRefused=10,
    CompletionTimeout=11,
    RetentionNotReleased=12,
    NativeResult=13,
};

// Scalar coordinates only. No saved-register pointers, object addresses, or
// owner-only state. The caller samples atomics independently, not a transaction.
struct CameraFatalRecord {
    CameraFatalReason reason;
    uint64_t event,current_tid,owner_tid,bootstrap_tid;
    bool bound,handoff_wait,stopped,joined;
};

// Best effort for systemd journal's stderr socket. One nonblocking send, no
// retries, locks, allocation, stdio, fd flag changes, or pipe/file fallback.
// Full/closed/missing/non-socket stderr may drop the record; partial sends may
// truncate it. Logging never authorizes continuation. Preserve errno for direct
// nonfatal checking. The formatting loops have fixed, input-independent bounds.
inline void emit_camera_failure(const CameraFatalRecord& r) noexcept {
    const int saved_errno=errno;
    constexpr size_t line_size=sizeof("ft-camera-fatal reason= event= current_tid= owner_tid= bootstrap_tid= bound= handoff_wait= stopped= joined=\n")-1+5*18+4;
    static_assert(line_size<=256);
    char line[line_size];char* out=line;
    const auto literal=[&](const auto& text) {
        for(size_t i=0;i<sizeof(text)-1;++i)*out++=text[i];
    };
    const auto hex=[&](uint64_t value) {
        *out++='0';*out++='x';
        for(unsigned i=0;i<16;++i)*out++="0123456789abcdef"[(value>>(60-4*i))&15];
    };
    literal("ft-camera-fatal reason=");hex(static_cast<uint8_t>(r.reason));
    literal(" event=");hex(r.event);
    literal(" current_tid=");hex(r.current_tid);
    literal(" owner_tid=");hex(r.owner_tid);
    literal(" bootstrap_tid=");hex(r.bootstrap_tid);
    literal(" bound=");*out++=r.bound?'1':'0';
    literal(" handoff_wait=");*out++=r.handoff_wait?'1':'0';
    literal(" stopped=");*out++=r.stopped?'1':'0';
    literal(" joined=");*out++=r.joined?'1':'0';
    *out++='\n';
    (void)send(STDERR_FILENO,line,static_cast<size_t>(out-line),MSG_DONTWAIT|MSG_NOSIGNAL);
    errno=saved_errno;
}

[[noreturn]] inline void abort_camera_failure(const CameraFatalRecord& r) noexcept {
    emit_camera_failure(r);
    std::abort();
}
} // namespace native_portal::live
