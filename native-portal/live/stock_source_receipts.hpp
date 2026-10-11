// SPDX-License-Identifier: MIT
#pragma once
#include "source_provider.hpp"
#include <optional>
#include <unistd.h>

namespace native_portal::live {
struct StockSourceState {
    uint64_t client=0;std::array<uint64_t,2> ids{};
    uint8_t active=0,connected=0,external=0;
    bool operator==(const StockSourceState& s) const noexcept {
        return client==s.client && ids==s.ids && active==s.active && connected==s.connected && external==s.external;
    }
};
// Synchronous boundary only. A ticket retains the camera association, NOT the
// client queue, mapping, native mutex or acquired metadata slot. The integrator
// must validate each PC/address/size against its native suspended read contract.
// Header/ref rereads require an actual successful native lock result paired to
// this accessor invocation, including revocation at native unlock. In particular
// 2584c4 follows 2584c0 without a native bool check; successful metadata API
// results, readable mappings and a retained ticket are NOT that witness. Until
// that event is installed/proved, the supplied boundary must refuse these reads.
// State snapshots need their own exact current-method read proof as well.
// Neither callback may retain scope, Saved, out, or native borrowed pointers.
struct StockSourceReadScope {
    const CameraBinding& binding;
    uint64_t ticket,version,sequence,method,pc,runtime;
    ClientSourceBinding::Module module;
    const arena::Saved& saved;
};
struct StockSourceReadBoundary {
    using Read=bool(*)(void*,const StockSourceReadScope&,uint64_t,void*,size_t) noexcept;
    using State=bool(*)(void*,const StockSourceReadScope&,StockSourceState&) noexcept;
    Read read=nullptr;State state=nullptr;void* context=nullptr;
};
class StockSourceCollector;
class NativeSourceAdmission;
// Only a completed matching RET can create this by-value record. Queue/header,
// stack and metadata addresses are historical opaque identities, never loans.
class StockSourceReceipt final {
public:
    const ClientSourceBinding::Observation& observation() const noexcept{return observation_;}
    uint64_t sequence() const noexcept{return sequence_;}
    uint64_t runtime() const noexcept{return runtime_;}
private:
    friend class StockSourceCollector;
    friend class StockSourceTransport;
    friend class NativeSourceAdmission;
    StockSourceReceipt()=default;
    CameraBinding binding_{};
    StockSourceState before_{},after_{};
    ClientSourceBinding::Observation observation_{};
    uint64_t version_=0,sequence_=0,stream_=0,runtime_=0,method_=0,return_pc_=0,ticket_=0,sp_=0,lr_=0;
    int process_=0;
    std::thread::id caller_{},owner_{};
    bool held_disconnect_=false;
};
// One bounded, one-way history stream per binding/version. Construct with the
// authenticated render owner's ID and nonzero Runtime identity. The transport
// must outlive every caller collector. seal() is owner-only and called only
// under the parent's all-stock-originals exclusion; this class counts camera
// invocations, not complete outer handlers. No mutex spans a read/native call.
class StockSourceTransport final {
public:
    static constexpr size_t capacity=32;
    StockSourceTransport(const CameraBinding& b,uint64_t version,std::thread::id owner,uint64_t runtime) noexcept:
        binding_(b),version_(version),owner_(owner),process_(getpid()),stream_(next_stream_.fetch_add(1)),runtime_(runtime) {}
    StockSourceTransport(const StockSourceTransport&)=delete;
    bool seal() noexcept {
        if(!owner())return invalidate();
        std::lock_guard<std::mutex> lock(mutex_);
        if(active_ || failed_.load() || !version_ || !stream_ || !runtime_)return invalidate();
        sealed_=true;return true;
    }
    std::optional<StockSourceReceipt> take() noexcept {
        if(!owner()){invalidate();return {};}
        std::lock_guard<std::mutex> lock(mutex_);
        if(!sealed_ || failed_.load() || consumed_==issued_)return {};
        auto out=slots_[consumed_];
        if(!out){invalidate();return {};}
        ++consumed_;return out;
    }
    struct Status {size_t pending=0,completed=0,abandoned=0;bool failed=true,sealed=false;};
    Status status() noexcept {
        if(!owner()){invalidate();return {};}
        std::lock_guard<std::mutex> lock(mutex_);
        return {active_,completed_,abandoned_,failed_.load(),sealed_};
    }
    bool failed() const noexcept{return getpid()!=process_ || failed_.load();}
    bool invalidate() noexcept{failed_.store(true);return false;}
private:
    friend class StockSourceCollector;
    friend class NativeSourceAdmission;
    bool owner() const noexcept{return getpid()==process_ && std::this_thread::get_id()==owner_;}
    uint64_t begin(const CameraBinding& b,uint64_t version) noexcept {
        if(getpid()!=process_)return invalidate(),0;
        std::lock_guard<std::mutex> lock(mutex_);
        // Overlap has no serial certificate, even if completion order looks good.
        if(failed_.load() || sealed_ || active_ || issued_==capacity || !(binding_==b) || version!=version_ || !version || !runtime_)
            return invalidate(),0;
        ++active_;return ++issued_;
    }
    bool complete(const StockSourceReceipt& r) noexcept {
        if(getpid()!=process_)return invalidate();
        std::lock_guard<std::mutex> lock(mutex_);
        if(!active_ || !r.sequence_ || r.sequence_!=issued_ || slots_[r.sequence_-1])return invalidate();
        slots_[r.sequence_-1]=r;--active_;++completed_;return !failed_.load();
    }
    // Abandonment is never completion. Keep its reserved slot missing forever.
    void abandon() noexcept {
        invalidate();if(getpid()!=process_)return;
        std::lock_guard<std::mutex> lock(mutex_);
        if(active_){--active_;++abandoned_;}
    }
    const CameraBinding binding_;const uint64_t version_;
    const std::thread::id owner_;const int process_;const uint64_t stream_,runtime_;
    inline static std::atomic<uint64_t> next_stream_{1};
    mutable std::mutex mutex_;
    std::atomic<bool> failed_{false};
    bool sealed_=false;size_t issued_=0,consumed_=0,active_=0,completed_=0,abandoned_=0;
    std::array<std::optional<StockSourceReceipt>,capacity> slots_{};
};
// One persistent instance per owning caller, e.g. bounded Runtime TLS. Reuses
// ClientSourceBinding's exact native event engine, with no default raw reader.
// Caller owns the real gate ticket throughout begin/events/RET and publication.
class StockSourceCollector final {
public:
    StockSourceCollector(CameraAccessGate& gate,StockSourceTransport& transport,StockSourceReadBoundary boundary) noexcept:
        gate_(gate),transport_(transport),boundary_(boundary),binding_engine_(gate,read,this),
        process_(getpid()),caller_(std::this_thread::get_id()) {}
    ~StockSourceCollector(){if(active_)transport_.abandon();}
    StockSourceCollector(const StockSourceCollector&)=delete;
    bool begin(uint64_t ticket,const CameraBinding& b,uint64_t version,uint64_t method,const arena::Saved& s,
               uint64_t comp,uint64_t client,const char* comp_digest,const char* client_digest) noexcept {
        if(!owner() || active_ || binding_engine_.failed())return reject();
        if(!boundary_.read || !boundary_.state || s.x[0]!=b.camera || !s.sp || !s.x[30] ||
           (method!=0x15f9e0 && method!=0x15fba8 && method!=0x15fcc0))return reject();
        CameraBinding held;
        if(!ticket || !gate_.lookup(ticket,held) || !(held==b))return reject();
        const auto sequence=transport_.begin(b,version);if(!sequence)return reject();
        active_=true;receipt_=StockSourceReceipt{};
        receipt_.binding_=b;receipt_.ticket_=ticket;receipt_.version_=version;receipt_.sequence_=sequence;
        receipt_.method_=method;receipt_.sp_=s.sp;receipt_.lr_=s.x[30];receipt_.stream_=transport_.stream_;
        receipt_.process_=process_;receipt_.caller_=caller_;receipt_.owner_=transport_.owner_;receipt_.runtime_=transport_.runtime_;
        const auto action=method==0x15f9e0?coordination::Action::Connect:
            method==0x15fba8?coordination::Action::Disconnect:coordination::Action::None;
        if(!binding_engine_.begin(ticket,b,action,comp,client,comp_digest,client_digest))return reject();
        const auto scope=make_scope(ClientSourceBinding::Module::Compositor,method,s);
        if(!held_now() || !boundary_.state(boundary_.context,scope,receipt_.before_) ||
           receipt_.before_.client!=client || !held_now())return reject();
        return true;
    }
    bool event(ClientSourceBinding::Module module,uint64_t pc,const arena::Saved& s) noexcept {
        if(!owner())return reject();
        if(!active_ || !held_now() || scope_)return reject();
        const auto scope=make_scope(module,pc,s);scope_=&scope;
        const bool ok=binding_engine_.event(module,pc,s);scope_=nullptr;
        return ok && held_now()?true:reject();
    }
    // Call only when the actual installed aggregate decision selected its no-op.
    // This is not inferred from Release=1 or an empty queue observation.
    bool held_disconnect(uint64_t pc,const arena::Saved& s) noexcept {
        if(!owner() || !active_ || !held_now() || receipt_.method_!=0x15fba8 ||
           pc!=0x15fbb8 || s.x[19]!=receipt_.binding_.camera || !s.sp || receipt_.held_disconnect_)return reject();
        receipt_.held_disconnect_=true;return true;
    }
    bool camera_return(uint64_t pc,const arena::Saved& s) noexcept {
        if(!owner())return reject();
        const uint64_t method=pc==0x15fdc8?0x15fcc0:
            (pc==0x15fa78 || pc==0x15faa4)?0x15f9e0:(pc==0x15fbd8 || pc==0x15fc20)?0x15fba8:0;
        if(!active_ || !method || method!=receipt_.method_ || s.sp!=receipt_.sp_ || s.x[30]!=receipt_.lr_ || !held_ticket())return reject();
        receipt_.return_pc_=pc;
        // A rejected observation never authorizes another native read, but a
        // matching actual RET still records completion separately from a gap.
        const auto scope=make_scope(ClientSourceBinding::Module::Compositor,pc,s);
        if(!held_now() || !boundary_.state(boundary_.context,scope,receipt_.after_) || !held_now())reject();
        const bool valid=binding_engine_.finish(s.x[0]);receipt_.observation_=binding_engine_.snapshot();
        active_=false;
        if(!valid)transport_.invalidate();
        return transport_.complete(receipt_) && valid;
    }
    void cancel() noexcept {reject();}
private:
    bool owner() const noexcept{return getpid()==process_ && std::this_thread::get_id()==caller_;}
    bool held_ticket() const noexcept {
        CameraBinding held;
        return owner() && gate_.lookup(receipt_.ticket_,held) && held==receipt_.binding_;
    }
    bool held_now() const noexcept {return !transport_.failed() && held_ticket();}
    bool reject() noexcept {binding_engine_.cancel();return transport_.invalidate();}
    StockSourceReadScope make_scope(ClientSourceBinding::Module module,uint64_t pc,const arena::Saved& s) const noexcept {
        return {receipt_.binding_,receipt_.ticket_,receipt_.version_,receipt_.sequence_,receipt_.method_,pc,receipt_.runtime_,module,s};
    }
    static bool read(void* p,uint64_t a,void* out,size_t size) {
        auto& self=*static_cast<StockSourceCollector*>(p);
        return self.scope_ && self.held_now() && self.boundary_.read(self.boundary_.context,*self.scope_,a,out,size) && self.held_now();
    }
    CameraAccessGate& gate_;StockSourceTransport& transport_;const StockSourceReadBoundary boundary_;
    ClientSourceBinding binding_engine_;const int process_;const std::thread::id caller_;
    bool active_=false;StockSourceReceipt receipt_{};
    const StockSourceReadScope* scope_=nullptr; // synchronous event only; cleared before return
};
}
