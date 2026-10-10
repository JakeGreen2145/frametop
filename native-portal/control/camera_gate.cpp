// SPDX-License-Identifier: MIT
#include "camera_gate.hpp"
#include <atomic>
#include <exception>
#include <unistd.h>

namespace native_portal {
namespace {
// Process-wide issuance prevents stale tickets from naming a new gate at the
// same address. Saturate rather than wrapping. Storage slots, not IDs, recycle.
std::atomic<uint64_t> epochs{0}, tickets{0};
uint64_t issue(std::atomic<uint64_t>& counter) noexcept {
    auto prior=counter.load(std::memory_order_relaxed);
    while(prior!=UINT64_MAX) {
        if(counter.compare_exchange_weak(prior,prior+1,std::memory_order_relaxed))return prior+1;
    }
    return 0;
}
bool valid(const CameraBinding& b) noexcept {
    return b.context && b.application && b.camera && b.renderer && b.device &&
        b.association && b.renderer_generation && b.device_generation && b.association_generation;
}
}
#ifdef FT_NATIVE_PORTAL_TEST_CALL
// Source-fixture fault injection only. Saturates, never erases/reuses an ID.
void exhaust_camera_gate_for_test(bool epoch) noexcept {
    (epoch?epochs:tickets).store(UINT64_MAX);
}
#endif
CameraAccessGate::CameraAccessGate() noexcept:process_(getpid()) {}
bool CameraAccessGate::same_process() const noexcept {return process_==getpid();}
bool CameraAccessGate::valid_epoch(uint64_t e) const noexcept {return e && e==epoch_;}
CameraAccessGate::~CameraAccessGate() {
    // Never lock an inherited mutex after fork. Such a gate is unusable there.
    if(!same_process())return;
    std::lock_guard<std::mutex> lock(mutex_);
    if(active_ || cleaning_)std::terminate();
}
bool CameraAccessGate::publish(const CameraBinding& b,std::thread::id owner,uint64_t& out,PublishFailure* failure) {
    out=0;if(failure)*failure=PublishFailure::None;
    auto reject=[&](PublishFailure f){if(failure)*failure=f;return false;};
    if(!same_process())return reject(PublishFailure::Process);
    if(!valid(b))return reject(PublishFailure::Fields);
    if(owner==std::thread::id{})return reject(PublishFailure::Owner);
    std::lock_guard<std::mutex> lock(mutex_);
    if(epoch_ || active_ || cleaning_)return reject(PublishFailure::State);
    const auto e=issue(epochs);if(!e)return reject(PublishFailure::Exhausted);
    binding_=b;owner_=owner;epoch_=e;closing_=false;out=e;return true;
}
bool CameraAccessGate::retain(uint64_t e,uint64_t& out) {
    out=0;if(!same_process())return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if(!valid_epoch(e) || closing_ || active_==capacity)return false;
    for(auto& t:tickets_)if(!t.id) {
        const auto id=issue(tickets);if(!id)return false;
        t={id,std::this_thread::get_id()};++active_;out=id;return true;
    }
    return false;
}
bool CameraAccessGate::release(uint64_t id) {
    if(!id || !same_process())return false;
    std::lock_guard<std::mutex> lock(mutex_);
    for(auto& t:tickets_)if(t.id==id) {
        t={};--active_;changed_.notify_all();return true;
    }
    return false;
}
bool CameraAccessGate::lookup(uint64_t id,CameraBinding& out) const {
    out={};if(!id || !same_process())return false;
    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto& t:tickets_)if(t.id==id) {out=binding_;return true;}
    return false;
}
bool CameraAccessGate::begin_close(uint64_t e) {
    if(!same_process())return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if(!valid_epoch(e))return false;
    closing_=true;changed_.notify_all();return true;
}
bool CameraAccessGate::quiescent(uint64_t e) const {
    if(!same_process())return false;
    std::lock_guard<std::mutex> lock(mutex_);
    return valid_epoch(e) && active_==0;
}
bool CameraAccessGate::self_wait() const noexcept {
    const auto self=std::this_thread::get_id();
    if(self==owner_ || self==cleanup_thread_)return true;
    for(const auto& t:tickets_)if(t.id && t.issuer==self)return true;
    return false;
}
CameraAccessGate::CleanupResult CameraAccessGate::close_and_cleanup(
    uint64_t e,CleanupKind kind,Cleanup callback,void* argument,std::chrono::milliseconds timeout) {
    if(!same_process())return CleanupResult::Invalid;
    std::unique_lock<std::mutex> lock(mutex_);
    if(!valid_epoch(e))return CleanupResult::Stale;
    // Even malformed cleanup requests revoke admission. They never erase debt.
    closing_=true;changed_.notify_all();
    if(!callback || timeout.count()<0 ||
       (kind!=CleanupKind::Resources && kind!=CleanupKind::Destroy))return CleanupResult::Invalid;
    if(active_ || cleaning_) {
        if(self_wait())return CleanupResult::WouldDeadlock;
        // wait_for releases the gate lock; releases/lookups remain possible.
        if(!changed_.wait_for(lock,timeout,[&]{return !valid_epoch(e) || (!active_ && !cleaning_);}))
            return CleanupResult::Timeout;
        if(!valid_epoch(e))return CleanupResult::Stale;
    }
    cleaning_=true;cleanup_thread_=std::this_thread::get_id();
    lock.unlock();
    callback(argument); // the only native/IPC/GPU/join boundary; mutex is free
    lock.lock();
    cleaning_=false;cleanup_thread_={};
    if(kind==CleanupKind::Destroy) {epoch_=0;binding_={};owner_={};}
    changed_.notify_all();return CleanupResult::Completed;
}
} // namespace native_portal
