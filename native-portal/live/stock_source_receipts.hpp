// SPDX-License-Identifier: MIT
#pragma once
#include "stock_native_boundary.hpp"
#include <optional>
#include <unistd.h>

namespace native_portal::live {
template<class Sink> class NativeSourceMethodCollector;
class StockSourceTransport;
using StockSourceCollector=NativeSourceMethodCollector<StockSourceTransport>;
class NativeSourceAdmission;
// Only a matching pre-RET candidate PLUS completed own bypass publishes this record. Queue/header,
// stack and metadata addresses are historical opaque identities, never loans.
template<uint64_t Method> class CurrentSourceCompletion;
template<uint64_t Kind> class NativeSourceResult final {
public:
    const ClientSourceBinding::Observation& observation() const noexcept{return observation_;}
    uint64_t sequence() const noexcept{return sequence_;}
    uint64_t runtime() const noexcept{return runtime_;}
    const NativeFieldFacts& native_fields() const noexcept{return facts_;}
private:
    template<class Sink> friend class NativeSourceMethodCollector;
    friend class StockSourceTransport;
    friend class NativeSourceAdmission;
    template<uint64_t Method> friend class CurrentSourceCompletion;
    NativeSourceResult()=default;
    CameraBinding binding_{};
    NativeFieldFacts facts_{};
    ClientSourceBinding::Observation observation_{};
    uint64_t version_=0,sequence_=0,stream_=0,runtime_=0,method_=0,return_pc_=0,ticket_=0,sp_=0,lr_=0;
    int process_=0;
    std::thread::id caller_{},owner_{};
    bool held_disconnect_=false;
};
using StockSourceReceipt=NativeSourceResult<0>;
// One bounded, one-way history stream per binding/version. Construct with the
// authenticated render owner's ID and nonzero Runtime identity. The transport
// must outlive every caller collector. seal() is owner-only and called only
// under the parent's all-stock-originals exclusion; this class counts camera
// invocations, not complete outer handlers. No mutex spans a read/native call.
class StockSourceTransport final {
public:
    using Record=StockSourceReceipt;
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
    template<class Sink> friend class NativeSourceMethodCollector;
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
// ClientSourceBinding's exact native event engine, behind the concrete native allowlist.
// Caller owns the real gate ticket throughout begin/events/RET and publication.
template<class Sink> class NativeSourceMethodCollector final {
public:
    using M=ClientSourceBinding::Module;
    using Transport=Sink;
    using Record=typename Sink::Record;
    NativeSourceMethodCollector(CameraAccessGate& gate,Sink& transport,StockSourceReadBoundary backend={}) noexcept:
        gate_(gate),transport_(transport),boundary_(gate,backend),binding_engine_(gate,read,this),
        process_(getpid()),caller_(std::this_thread::get_id()) {}
    ~NativeSourceMethodCollector(){if(active_)transport_.abandon();}
    NativeSourceMethodCollector(const NativeSourceMethodCollector&)=delete;
    bool begin(uint64_t ticket,const CameraBinding& b,uint64_t version,uint64_t method,const arena::Saved& s,
               uint64_t comp,uint64_t client,const char* comp_digest,const char* client_digest) noexcept {
        if(!owner() || active_ || binding_engine_.failed())return reject();
        if(s.x[0]!=b.camera || s.sp<0x500 || !s.x[30] ||
           (method!=0x15f9e0 && method!=0x15fba8 && method!=0x15fcc0))return reject();
        CameraBinding held;
        if(!ticket || !gate_.lookup(ticket,held) || !(held==b))return reject();
        const auto sequence=transport_.begin(b,version);if(!sequence)return reject();
        active_=true;candidate_=false;receipt_=Record{};
        receipt_.binding_=b;receipt_.ticket_=ticket;receipt_.version_=version;receipt_.sequence_=sequence;
        receipt_.method_=method;receipt_.sp_=s.sp;receipt_.lr_=s.x[30];receipt_.stream_=transport_.stream_;
        receipt_.process_=process_;receipt_.caller_=caller_;receipt_.owner_=transport_.owner_;receipt_.runtime_=transport_.runtime_;
        const auto action=method==0x15f9e0?coordination::Action::Connect:
            method==0x15fba8?coordination::Action::Disconnect:coordination::Action::None;
        if(!binding_engine_.begin(ticket,b,action,comp,client,comp_digest,client_digest,true))return reject();
        const auto scope=make_scope(M::Compositor,method,s);
        if(!boundary_.begin(scope,client) || !held_now())return reject();
        receipt_.facts_.put(scope,NativeCameraField::Client,client,0,NativeFactPath::ModuleIdentity);
        return true;
    }
    // Only old engine PCs enter event. New field and guard PCs have their own
    // routes, so an unknown producer cannot poison/impersonate an old event.
    bool event(M module,uint64_t pc,const arena::Saved& s) noexcept {
        if(!owner() || !active_ || candidate_ || !held_now() || in_callback_)return reject();
        if(module==M::Compositor && (s.x[19]!=receipt_.binding_.camera || !body_sp(s.sp)))return reject();
        ClientSourceBinding::NativeSelectedId selected{};const ClientSourceBinding::NativeSelectedId* input=nullptr;
        if(module==M::Compositor) {
            switch(pc) {
            case 0x15fbec:case 0x160184:case 0x160380:case 0x1605c4:input=&selected;break;
            case 0x15fc34:case 0x15fee4:case 0x160164:selected.queue=1;input=&selected;break;
            default:break;
            }
            if(input) {
                selected.value=s.x[1];
                const auto scope=make_scope(module,pc,s);
                const auto bit=selected.queue?NativeFieldFacts::ID1:NativeFieldFacts::ID0;
                if(receipt_.facts_.has(bit) && receipt_.facts_.values.ids[selected.queue]!=s.x[1])return reject();
                receipt_.facts_.put(scope,selected.queue?NativeCameraField::Id1:NativeCameraField::Id0,
                    selected.value,8,NativeFactPath::SelectedRequest);
            }
        }
        const auto scope=make_scope(module,pc,s);
        if(!boundary_.callback(scope,binding_engine_.native_read_context()))return reject();
        in_callback_=true;
        const bool ok=binding_engine_.event(module,pc,s,input);
        boundary_.leave();in_callback_=false;
        return ok && held_now()?true:reject();
    }
    bool camera_fact(M module,uint64_t pc,const arena::Saved& s) noexcept {
        if(!owner() || !active_ || candidate_ || !held_now() || in_callback_)return reject();
        const auto scope=make_scope(module,pc,s);
        if(module==M::Client) {
            if(pc!=0x32a628 || receipt_.method_!=0x15f9e0 || !binding_engine_.native_connect_output(s))return reject();
            const auto c=binding_engine_.native_read_context();
            if(c.call_sp!=receipt_.sp_-0x20 || (s.x[23]!=receipt_.binding_.camera+0x350 && s.x[23]!=receipt_.binding_.camera+0x358))return reject();
            const auto field=s.x[23]==receipt_.binding_.camera+0x350?NativeCameraField::Id0:NativeCameraField::Id1;
            receipt_.facts_.put(scope,field,s.x[19],8,NativeFactPath::CommittedStore);return true;
        }
        if(s.x[19]!=receipt_.binding_.camera || !body_sp(s.sp))return reject();
        NativeCameraField field;NativeFactPath path=NativeFactPath::NativeLoad;uint64_t method=0,value=0;
        switch(pc) {
        case 0x15f9fc:method=0x15f9e0;field=NativeCameraField::Active;value=uint8_t(s.x[0]);path=NativeFactPath::CommittedStore;break;
        case 0x15fa60:case 0x15fa94:method=0x15f9e0;field=NativeCameraField::Active;value=uint8_t(s.x[0]);break;
        case 0x15fa04:case 0x15fa8c:method=0x15f9e0;field=NativeCameraField::Connected;value=uint8_t(s.x[0]);break;
        case 0x15fabc:method=0x15f9e0;field=NativeCameraField::Connected;value=1;path=NativeFactPath::CommittedStore;break;
        case 0x15fbc0:method=0x15fba8;field=NativeCameraField::Connected;value=uint8_t(s.x[0]);break;
        case 0x15fbc4:method=0x15fba8;field=NativeCameraField::Active;path=NativeFactPath::CommittedStore;break;
        case 0x15fc4c:method=0x15fba8;field=NativeCameraField::Connected;path=NativeFactPath::CommittedStore;break;
        case 0x15fbd0:case 0x15fc08:method=0x15fba8;field=NativeCameraField::External;path=NativeFactPath::CommittedStore;break;
        case 0x15fd30:method=0x15fcc0;field=NativeCameraField::Active;value=uint8_t(s.x[20]);break;
        case 0x15fd48:method=0x15fcc0;field=NativeCameraField::Connected;value=uint8_t(s.x[1]);break;
        case 0x15fd90:method=0x15fcc0;field=NativeCameraField::Connected;value=uint8_t(s.x[0]);break;
        case 0x16013c:method=0x15fcc0;field=NativeCameraField::External;value=uint8_t(s.x[0]);path=NativeFactPath::CommittedStore;break;
        default:return reject();
        }
        if(method!=receipt_.method_)return reject();
        receipt_.facts_.put(scope,field,value,1,path);return true;
    }
    bool boundary_event(M module,uint64_t pc,const arena::Saved& s) noexcept {
        if(!owner() || !active_ || candidate_ || !held_now() || in_callback_)return reject();
        return boundary_.observe(make_scope(module,pc,s),binding_engine_.native_read_context())?true:reject();
    }
    // Only actual installed aggregate decision selection, not Release=1.
    bool held_disconnect(uint64_t pc,const arena::Saved& s) noexcept {
        if(!owner() || !active_ || candidate_ || !held_now() || receipt_.method_!=0x15fba8 ||
           pc!=0x15fbb8 || s.x[19]!=receipt_.binding_.camera || !body_sp(s.sp) || receipt_.held_disconnect_)return reject();
        receipt_.held_disconnect_=true;return true;
    }
    // PRE-instruction observer. Stores only a candidate. No completion, no reads.
    bool camera_return(uint64_t pc,const arena::Saved& s) noexcept {
        if(!owner())return reject();
        const uint64_t method=pc==0x15fdc8?0x15fcc0:
            (pc==0x15fa78 || pc==0x15faa4)?0x15f9e0:(pc==0x15fbd8 || pc==0x15fc20)?0x15fba8:0;
        if(!active_ || candidate_ || !method || method!=receipt_.method_ || s.sp!=receipt_.sp_ ||
           s.x[30]!=receipt_.lr_ || !held_ticket())return reject();
        candidate_=true;receipt_.return_pc_=pc;candidate_result_=s.x[0];
        if(!boundary_.clean())reject();
        boundary_.end();return held_now();
    }
    // Parent calls ONLY after ft_native_portal_call_saved completed THIS native
    // method bypass. Outer-handler return is not completion. Nested routing and
    // original exclusion remain parent-owned. The exact scalar result is retained.
    struct Invocation {uint64_t runtime=0,version=0,stream=0,sequence=0,ticket=0,method=0,sp=0,lr=0;};
    Invocation invocation() const noexcept {
        if(!owner() || !active_)return {};
        return {receipt_.runtime_,receipt_.version_,receipt_.stream_,receipt_.sequence_,receipt_.ticket_,
            receipt_.method_,receipt_.sp_,receipt_.lr_};
    }
    bool complete_own_bypass(const Invocation& call,uint64_t actual) noexcept {
        if(!owner() || !active_ || !candidate_ || call.method!=receipt_.method_ || call.sp!=receipt_.sp_ ||
           call.lr!=receipt_.lr_ || call.runtime!=receipt_.runtime_ || call.version!=receipt_.version_ ||
           call.stream!=receipt_.stream_ || call.sequence!=receipt_.sequence_ || call.ticket!=receipt_.ticket_ ||
           actual!=candidate_result_ || !held_ticket())return reject();
        const bool valid=binding_engine_.finish(actual);receipt_.observation_=binding_engine_.snapshot();
        active_=false;candidate_=false;boundary_.end();
        if(!valid)transport_.invalidate();
        return transport_.complete(receipt_) && valid;
    }
    void cancel() noexcept {reject();}
private:
    bool body_sp(uint64_t sp) const noexcept {return sp==receipt_.sp_-(receipt_.method_==0x15fcc0?0x360:0x20);}
    bool owner() const noexcept{return getpid()==process_ && std::this_thread::get_id()==caller_;}
    bool held_ticket() const noexcept {
        CameraBinding held;
        return owner() && gate_.lookup(receipt_.ticket_,held) && held==receipt_.binding_;
    }
    bool held_now() const noexcept {return !transport_.failed() && held_ticket();}
    bool reject() noexcept {
        binding_engine_.cancel();
        if(owner())boundary_.end();
        return transport_.invalidate();
    }
    StockSourceReadScope make_scope(M module,uint64_t pc,const arena::Saved& s) const noexcept {
        return {receipt_.binding_,receipt_.ticket_,receipt_.version_,receipt_.sequence_,receipt_.method_,pc,receipt_.runtime_,module,s};
    }
    static bool read(void* p,uint64_t a,void* out,size_t size) {
        auto& self=*static_cast<NativeSourceMethodCollector*>(p);
        return self.in_callback_ && self.held_now() && self.boundary_.read(a,out,size) && self.held_now();
    }
    CameraAccessGate& gate_;Sink& transport_;NativeStockReadBoundary boundary_;
    ClientSourceBinding binding_engine_;const int process_;const std::thread::id caller_;
    bool active_=false,candidate_=false,in_callback_=false;uint64_t candidate_result_=0;
    Record receipt_{};
};
}
