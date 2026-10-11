// SPDX-License-Identifier: MIT
#pragma once
#include "stock_source_receipts.hpp"
namespace native_portal::live {
// Distinct types. Neither is a stock receipt and neither enters stock transport.
using CurrentConnectResult=NativeSourceResult<0x15f9e0>;
using CurrentUpdateResult=NativeSourceResult<0x15fcc0>;
inline std::atomic<uint64_t> current_source_invocations{1};
// One actual own-method bypass. This is an owner-local result slot, not history.
// Its generated invocation is available before reserve/begin, and never reused.
template<uint64_t Method> class CurrentSourceCompletion final {
public:
    using Record=NativeSourceResult<Method>;
    static constexpr bool accepts_method(uint64_t method) noexcept{return method==Method;}
    CurrentSourceCompletion(const CameraBinding& b,uint64_t version,std::thread::id owner,uint64_t runtime) noexcept:
        binding_(b),version_(version),owner_(owner),runtime_(runtime),stream_(current_source_invocations.fetch_add(1)) {}
    CurrentSourceCompletion(const CurrentSourceCompletion&)=delete;
    uint64_t invocation_id() const noexcept{return stream_;}
    std::optional<Record> take() noexcept {
        if(!owner() || failed() || active_ || taken_ || !result_)return {};
        taken_=true;return result_;
    }
    bool failed() const noexcept{return getpid()!=process_ || failed_.load();}
    bool invalidate() noexcept{failed_.store(true);return false;}
private:
    template<class Sink> friend class NativeSourceMethodCollector;
    bool owner() const noexcept{return getpid()==process_ && std::this_thread::get_id()==owner_;}
    uint64_t begin(const CameraBinding& b,uint64_t version) noexcept {
        if(!owner() || failed() || used_ || !(binding_==b) || version!=version_ || !version || !runtime_ || !stream_)
            return invalidate(),0;
        used_=active_=true;return stream_;
    }
    bool complete(const Record& r) noexcept {
        if(!owner() || !active_ || r.method_!=Method || r.stream_!=stream_ || r.sequence_!=stream_)return invalidate();
        active_=false;result_=r;return !failed();
    }
    void abandon() noexcept{invalidate();active_=false;}
    const CameraBinding binding_;const uint64_t version_;const std::thread::id owner_;
    const uint64_t runtime_,stream_;const int process_=getpid();
    std::atomic<bool> failed_{false};bool used_=false,active_=false,taken_=false;
    std::optional<Record> result_{};
};
using CurrentConnectCompletion=CurrentSourceCompletion<0x15f9e0>;
using CurrentUpdateCompletion=CurrentSourceCompletion<0x15fcc0>;
using CurrentConnectCollector=NativeSourceMethodCollector<CurrentConnectCompletion>;
using CurrentUpdateCollector=NativeSourceMethodCollector<CurrentUpdateCompletion>;
}
