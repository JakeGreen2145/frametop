// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <unistd.h>
namespace native_portal::live {
// Metadata only. Counts admission, never CPU/GPU/source completion. The exact
// Adapter/Command/SourceCommand remain the only work-settlement authorities.
class RuntimeAdmission final {
public:
    enum class Phase {Prebind,Dormant,Engaged,Closing,Closed,Invalid};
    RuntimeAdmission() noexcept=default;
    RuntimeAdmission(const RuntimeAdmission&)=delete;
    Phase phase() const noexcept {
        if(!process())return Phase::Invalid;
        std::lock_guard<std::mutex> lock(mutex_);return phase_;
    }
    bool bind(uint64_t version) noexcept {
        if(!process())return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(phase_!=Phase::Prebind || !version)return false;
        version_=version;phase_=Phase::Dormant;return true;
    }
    // retain must only acquire the real caller handle, with no native reads or
    // callbacks. This joins close and successful lifetime retain atomically.
    template<class Retain> bool original(uint64_t version,Retain retain) noexcept {
        if(!process())return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(!matches(version) || phase_!=Phase::Dormant || originals_==64 || !retain())return false;
        ++originals_;return true;
    }
    void original_return(uint64_t version) noexcept {
        if(!process())std::abort();
        std::lock_guard<std::mutex> lock(mutex_);
        if(!matches(version) || !originals_)std::abort();
        --originals_;
    }
    // prepare is owner-only immutable-history preflight/fold. No reader/native
    // call, wait, or reentry into this class may run under the short lock. False
    // must not publish partial admission; irreversible failure calls invalidate
    // after returning. ENGAGED is entered before the first optional read/attempt.
    template<class Prepare> bool engage(uint64_t version,Prepare prepare) noexcept {
        if(!process())return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(!matches(version))return false;
        if(phase_==Phase::Engaged)return true;
        if(phase_!=Phase::Dormant || originals_ || readers_ || !prepare())return false;
        phase_=Phase::Engaged;engaged_=true;return true;
    }
    // draining is only for authenticated owner callbacks settling already
    // admitted ENGAGED work. It never admits a new original or fresh attempt.
    bool reader(uint64_t version,bool draining=false) noexcept {
        if(!process())return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(!matches(version) || (phase_!=Phase::Engaged && !(draining && engaged_ && phase_==Phase::Closing)) || readers_==64)return false;
        ++readers_;return true;
    }
    void reader_return(uint64_t version) noexcept {
        if(!process())std::abort();
        std::lock_guard<std::mutex> lock(mutex_);
        if(!matches(version) || !readers_)std::abort();
        --readers_;
    }
    bool begin_close(uint64_t version) noexcept {
        if(!process())return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(!matches(version) || (phase_!=Phase::Dormant && phase_!=Phase::Engaged && phase_!=Phase::Closing))return false;
        phase_=Phase::Closing;return true;
    }
    bool close_ready(uint64_t version) const noexcept {
        if(!process())return false;
        std::lock_guard<std::mutex> lock(mutex_);
        return matches(version) && phase_==Phase::Closing && !originals_ && !readers_;
    }
    bool finish_close(uint64_t version) noexcept {
        if(!process())return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(!matches(version) || phase_!=Phase::Closing || originals_ || readers_)return false;
        phase_=Phase::Closed;return true;
    }
    void invalidate() noexcept {
        if(!process())return;
        std::lock_guard<std::mutex> lock(mutex_);phase_=Phase::Invalid;
    }
private:
    bool process() const noexcept {return getpid()==process_;}
    bool matches(uint64_t version) const noexcept {return version && version==version_;}
    const int process_=getpid();
    mutable std::mutex mutex_;
    Phase phase_=Phase::Prebind;
    bool engaged_=false;
    uint64_t version_=0;
    unsigned originals_=0,readers_=0;
};
}
