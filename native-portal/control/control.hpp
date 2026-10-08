// Private, same-host control plane. No OpenVR or native camera access here.
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <type_traits>
namespace native_portal {
using Clock = uint64_t (*)();
uint64_t boottime();
enum class Operation : uint32_t { Acquire=1, Update=2, Release=3, Status=4 };
enum class State : uint32_t { Unavailable, Off, Warming, ActiveSubmitted, SuspendedExternal, SuppressedMenu, Draining, Fault };
enum class Reason : uint32_t { None, MissingBridge, Unsupported, Busy, Invalid, Replay, Epoch, Expired, Origin, OwnerGone, External, Tracking, Menu, NativeFault, Concurrency, Off };
const char* name(State); const char* name(Reason);
struct Geometry { float center[3]{0,.75f,-.65f}; float size[3]{1,.35f,.65f}; float quaternion[4]{0,0,0,1}; float opacity=1; };
struct Request { Operation op=Operation::Acquire; uint32_t flags=1; uint64_t epoch=0, sequence=0, generation=0; uint32_t ttl_ms=250; uint64_t origin=0; Geometry geometry; uint64_t sent_ns=0; };
struct Reply { State state=State::Unavailable; Reason reason=Reason::Unsupported; uint64_t epoch=0, sequence=0, generation=0, origin=0, submitted_frame=0, instance=0; uint32_t capability=0; };
using Wire = std::array<uint8_t,128>;
Wire encode(const Request&); bool decode(const void*,size_t,Request&);
Wire encode(const Reply&); bool decode(const void*,size_t,Reply&);
uint64_t deadline(uint64_t sent,uint64_t receipt,uint32_t ttl);
bool valid(const Geometry&);
// All payload words are atomic: no C++ data race from speculative seqlock reads.
// SC order makes the bounded coherence test portable, including ARM. One writer
// is admitted nonblockingly; callers must fail closed if publication loses.
template<class T> class Mailbox {
    static_assert(std::is_trivially_copyable<T>::value,"POD snapshot required");
    static_assert(std::atomic<uint64_t>::is_always_lock_free,"64-bit atomics required");
    static constexpr size_t N=(sizeof(T)+7)/8;
    std::array<std::atomic<uint64_t>,N> words_{};
    std::atomic<uint64_t> seq_{0}; std::atomic_flag writer_=ATOMIC_FLAG_INIT;
public:
    bool publish(const T& value) noexcept {
        if(writer_.test_and_set())return false;
        std::array<uint64_t,N> raw{}; std::memcpy(raw.data(),&value,sizeof value);
        seq_.fetch_add(1); for(size_t i=0;i<N;++i)words_[i].store(raw[i]); seq_.fetch_add(1);
        writer_.clear(); return true;
    }
    bool read(T& out) const noexcept {
        for(int attempt=0;attempt<3;++attempt) {
            auto before=seq_.load(); if(!before || (before&1))continue;
            std::array<uint64_t,N> raw{}; for(size_t i=0;i<N;++i)raw[i]=words_[i].load();
            if(seq_.load()==before) { std::memcpy(static_cast<void*>(&out),raw.data(),sizeof out); return true; }
        } return false;
    }
};
struct Lease { uint64_t epoch=0, origin=0, generation=0, deadline_ns=0, revocation=0; uint32_t flags=0; Geometry geometry; };
struct BackendStatus {
    bool capable=false, serialized=false, external_clear=false, fault=false, pending=false;
    uint64_t origin=0, submitted_epoch=0, submitted_frame=0;
};
// Only a proven adapter may publish capable/serialized/external_clear. In
// production the default stays unsupported. Submission is an observer fact.
class Backend {
    Mailbox<BackendStatus> status_; Mailbox<Lease> lease_; std::atomic<bool> collision_{false};
    std::atomic<bool> render_pending_{false};
    std::atomic<uint64_t> revoked_{0}; std::atomic<Reason> blocked_{Reason::None}; std::atomic<bool> new_ready_{true};
public:
    Backend() { status_.publish(BackendStatus{}); lease_.publish(Lease{}); }
    // Single render owner; independent of lease revocation and demand settlement.
    void render_pending(bool value) noexcept { render_pending_=value; }
    bool render_pending() const noexcept { return render_pending_.load(); }
    void refuse() noexcept { collision_=true; }
    bool publish(const BackendStatus& s) { if(status_.publish(s))return true; collision_=true; return false; }
    bool status(BackendStatus& s) const {
        if(collision_||!status_.read(s))return false;
        s.pending=render_pending_.load();
        auto reason=blocked_.load();
        if(reason!=Reason::None)s.external_clear=false;
        if(reason==Reason::NativeFault)s.fault=true;
        return true;
    }
    uint64_t revocation() const noexcept { return revoked_.load(); }
    void invalidate(Reason r) noexcept { blocked_=r; if(revoked_.fetch_add(1)==UINT64_MAX)refuse(); }
    void settled() noexcept { blocked_=Reason::None; } // native owner, verified idle only
    void new_lease_ready(bool value) noexcept { new_ready_=value; }
    bool new_lease_ready() const noexcept { return new_ready_.load()&&!render_pending_.load(); }
    bool lease(Lease& out,uint64_t now) const;
    bool publish_lease(const Lease& l) { if(lease_.publish(l))return true; collision_=true; return false; }
};
class Client {
    int fd_=-1; int pid_=0; uint64_t start_=0;
public:
    Client()=default; ~Client(){close();} Client(const Client&)=delete; Client& operator=(const Client&)=delete;
    bool connect(const std::string& path); bool exchange(const Request&,Reply&); void close();
};
class Server {
    struct Impl; Impl* impl_;
public:
    Server(const std::string& runtime,Backend&,Clock=boottime); ~Server();
    Server(const Server&)=delete; Server& operator=(const Server&)=delete;
    bool start(); void stop(); const std::string& path() const;
};
}
