// SPDX-License-Identifier: MIT
#pragma once
#include "source_admission.hpp"
#include "stock_source_receipts.hpp"
#include "current_source_completion.hpp"
#include "current_source_selection.hpp"
#include <cstring>
namespace native_portal::live {
// Only Runtime supplies this retained observation boundary. It checks the gate,
// loader module handles, owner, application/camera/renderer and device association
// before reading scalars. No API here authorizes independent source reads.
class NativeSourceAdmission final:public SourceAdmission {
public:
    using State=StockSourceState;
    using Authenticate=bool(*)(void*,const CameraBinding&,State&) noexcept;
    // Identity-only predicate for a provider constructed before render handoff.
    // It must not read native fields. Without it, construction pins the owner.
    using StockOwner=bool(*)(void*) noexcept;
    NativeSourceAdmission(void* context,Authenticate auth,StockOwner stock_owner=nullptr):
        context_(context),auth_(auth),stock_owner_check_(stock_owner) {}
    NativeSourceAdmission(void* context,CurrentSourceContracts contracts,StockOwner owner):
        context_(context),auth_(nullptr),stock_owner_check_(owner),identity_auth_(contracts.authenticate),current_mode_(true) {}
    BootstrapEligibility bootstrap_eligible(const CameraBinding& b) noexcept {
        SourceIdentity identity;
        if(!current_mode_ || !identity_lifetime(b,identity) || uncertain_ || current_connect_ || current_update_ || watermark_)return {};
        if(journal_ && !connected_)return {};
        return {true,!journal_};
    }
    bool begin_current_connect(const CameraBinding& b,uint64_t invocation) noexcept {
        if(!invocation || invocation<=last_current_invocation_ || !bootstrap_eligible(b).available)return false;
        current_connect_=invocation;last_current_invocation_=invocation;before_local_=false;return true;
    }
    bool finish_current_connect(const CameraBinding& b,const CurrentConnectResult& r) noexcept {
        SourceIdentity identity;
        if(!current_mode_ || !identity_lifetime(b,identity) || !current_connect_ ||
           !current_result(b,r) || r.sequence_!=current_connect_ || r.stream_!=current_connect_) {
            uncertain_=true;return false;
        }
        current_connect_=0;last_=r.observation_;
        const auto& f=r.facts_;const auto& o=r.observation_;
        const bool queues=o.queues[0].observed || o.queues[1].observed;
        if(uncertain_ || !o.valid || o.action!=coordination::Action::Connect || o.actual!=1 ||
           !f.has(NativeFieldFacts::ACTIVE|NativeFieldFacts::CONNECTED) || !f.values.active || !f.values.connected ||
           (queues?!f.has(NativeFieldFacts::ID0|NativeFieldFacts::ID1):(!journal_ || !connected_))) {
            uncertain_=true;return false;
        }
        // An empty real already-connected return is neutral. No fabricated IDs.
        if(!queues){last_use_=coordination::UseDisposition::AuthenticatedBorrowedUse;return true;}
        if(journal_ && f.values.ids!=ids_){uncertain_=true;return false;}
        before_local_=false;apply_observation(f.values,o,true);return !uncertain_;
    }
    bool begin_current_update(const CameraBinding& b,uint64_t invocation) noexcept {
        SourceIdentity identity;
        if(!current_mode_ || !identity_lifetime(b,identity) || uncertain_ || current_connect_ || current_update_ ||
           !invocation || invocation<=last_current_invocation_)return false;
        current_update_=last_current_invocation_=invocation;return true;
    }
    bool finish_current_update(const CameraBinding& b,const CurrentUpdateResult& r) noexcept {
        SourceIdentity identity;
        if(!current_mode_ || !identity_lifetime(b,identity) || !current_update_ || !current_result(b,r) ||
           r.sequence_!=current_update_ || r.stream_!=current_update_) {uncertain_=true;return false;}
        current_update_=0;
        const auto& o=r.observation_;const auto& f=r.facts_;
        if(uncertain_ || !o.valid || o.action!=coordination::Action::None ||
           (provider_ && o.provider && provider_!=o.provider)) {uncertain_=true;return false;}
        bool any=false,complete=true;
        for(unsigned q=0;q<2;++q) {
            const auto& read=o.reads[q];const auto bit=q?NativeFieldFacts::ID1:NativeFieldFacts::ID0;
            if(journal_ && f.has(bit) && f.values.ids[q]!=ids_[q]) {uncertain_=true;return false;}
            const bool attempted=read.acquire_observed || read.release_observed;
            const bool paired=read.acquire_observed && !read.acquire_result && read.returned &&
                read.release_observed && !read.release_result && read.increment && read.decrement &&
                read.metadata_return_observed && read.metadata && read.data && read.ref_address &&
                read.queue && read.header && f.has(bit) && read.id==f.values.ids[q];
            if(attempted && !paired){uncertain_=true;return false;}
            any|=attempted;complete&=paired;
        }
        if(any && (!f.has(NativeFieldFacts::ACTIVE|NativeFieldFacts::CONNECTED) || !f.values.active || !f.values.connected || !o.provider)) {
            uncertain_=true;return false;
        }
        if(!journal_) {
            if(!complete || !f.values.ids[0] || !f.values.ids[1] || f.values.ids[0]==f.values.ids[1]) {uncertain_=true;return false;}
            apply_observation(f.values,o,true);
        }
        // With a good journal, q1 absent is a native conditional path, not failure.
        return !uncertain_;
    }
    // Owner folds only immutable history drained after stock-original exclusion.
    // No auth callback and no native pointer reads occur here. Current eligibility
    // still goes through lifetime()/Authenticate under the engagement read token.
    bool fold_stock_receipt(const CameraBinding& b,uint64_t version,const StockSourceReceipt& r) noexcept {
        if(!stock_owner() || r.process_!=stock_process_ || std::this_thread::get_id()!=r.owner_) {
            stock_invalid_.store(true);return false;
        }
        const auto& f=r.facts_;const auto& state=f.values;const auto& o=r.observation_;
        if(stock_invalid_.load() || uncertain_ || before_ok_ || watermark_ || local_connection_ ||
           !(b==r.binding_) || !r.runtime_ || !version || version!=r.version_ || !o.valid ||
           r.sequence_!=stock_sequence_+1 || (stock_sequence_ && (stock_stream_!=r.stream_ || stock_runtime_!=r.runtime_ || stock_version_!=version ||
               stock_owner_!=r.owner_)) || !f.has(NativeFieldFacts::CLIENT) || !state.client ||
           (bound_ && (!(binding_==b) || client_!=state.client))) {uncertain_=true;return false;}
        last_=o; // literal partial failures survive refusal, never authorizing use
        if(o.provider && provider_ && o.provider!=provider_){uncertain_=true;return false;}
        for(unsigned q=0;q<2;++q)if(journal_ && f.has(q?NativeFieldFacts::ID1:NativeFieldFacts::ID0) && state.ids[q]!=ids_[q]) {
            uncertain_=true;return false;
        }
        const bool flags=f.has(NativeFieldFacts::ACTIVE|NativeFieldFacts::CONNECTED);
        const bool ids=f.has(NativeFieldFacts::ID0|NativeFieldFacts::ID1);
        const bool attempted=o.reads[0].acquire_observed || o.reads[1].acquire_observed ||
            o.reads[0].release_observed || o.reads[1].release_observed;
        const bool queues=o.queues[0].observed || o.queues[1].observed;
        if(o.action==coordination::Action::Connect) {
            if(!flags || !state.active || !state.connected || o.actual!=1 ||
               (queues?!ids:(!journal_ || !connected_))) {uncertain_=true;return false;}
        } else if(o.action==coordination::Action::Disconnect) {
            if(r.held_disconnect_) {
                if(queues || !journal_ || !connected_ || o.actual!=1) {uncertain_=true;return false;}
            } else if(!ids || !flags || state.active || state.connected ||
                      (f.committed&(NativeFieldFacts::ACTIVE|NativeFieldFacts::CONNECTED))!=
                        (NativeFieldFacts::ACTIVE|NativeFieldFacts::CONNECTED)) {uncertain_=true;return false;}
        } else if(attempted && (!ids || !flags || !state.active || !state.connected)) {uncertain_=true;return false;}
        bound_=true;binding_=b;client_=state.client;
        stock_stream_=r.stream_;stock_version_=version;stock_owner_=r.owner_;stock_sequence_=r.sequence_;stock_runtime_=r.runtime_;
        before_local_=false;
        // No-effect paths are neutral, not evidence of cold zeros or warm reads.
        // No missing selected ID is filled in from historical state.
        if((o.action==coordination::Action::None && !attempted) || r.held_disconnect_ ||
           (o.action==coordination::Action::Connect && !queues))return true;
        apply_observation(state,o,true);
        return !uncertain_;
    }
    // Preferred batch API: failure to seal, including missing RET or overflow,
    // poisons this provider as well as refusing engagement. Never resets history.
    bool fold_stock_history(StockSourceTransport& transport) noexcept {
        if(!stock_owner() || !transport.owner()) {stock_invalid_.store(true);return false;}
        if(!transport.seal()){uncertain_=true;return false;}
        while(auto r=transport.take())if(!fold_stock_receipt(transport.binding_,transport.version_,*r))return false;
        if(transport.failed()){uncertain_=true;return false;}
        return !uncertain_ && !stock_invalid_.load();
    }
    bool connect_eligible(const CameraBinding& b) noexcept override {
        if(current_mode_)return bootstrap_eligible(b).available;
        State s;if(!lifetime(b,s) || uncertain_ || s.external)return false;
        if(!s.active && !s.connected && !s.ids[0] && !s.ids[1])return true;
        return journal_ && s.ids==ids_ && ((connected_ && s.active && s.connected) ||
            (detached_ && !s.active && !s.connected));
    }
    bool transition_eligible(const CameraBinding& b,coordination::Action action) noexcept override {
        if(current_mode_) {
            SourceIdentity identity;
            if(!identity_lifetime(b,identity) || uncertain_)return false;
            return action==coordination::Action::None ||
                (action==coordination::Action::Connect && bootstrap_eligible(b).available);
        }
        State s;if(!lifetime(b,s) || uncertain_)return false;
        if(action==coordination::Action::Connect)return connect_eligible(b);
        // Off and source settlement need no fresh image. A Disconnect still
        // requires the original locally owned connection, never Borrowed USE.
        return action==coordination::Action::None ||
            (local_connection_ && connected_ && s.ids==ids_);
    }
    void before(const CameraBinding& b,coordination::Action action) noexcept override {
        if(current_mode_)return; // current results are collector-issued only
        State s;before_ok_=lifetime(b,s) && !uncertain_;before_action_=action;
        held_disconnect_=false;
        before_local_=before_ok_ && !s.active && !s.connected &&
            ((!s.ids[0] && !s.ids[1]) || (detached_ && s.ids==ids_));
    }
    void held_disconnect(const CameraBinding& b) noexcept override {
        State s;
        if(!before_ok_ || before_action_!=coordination::Action::Disconnect ||
           !lifetime(b,s) || !journal_ || !connected_ || s.ids!=ids_) {uncertain_=true;return;}
        held_disconnect_=true;
    }
    void after(const CameraBinding& b,const ClientSourceBinding::Observation& o) noexcept override {
        if(current_mode_)return;
        last_=o;State s;
        if(!before_ok_ || !lifetime(b,s) || !o.valid || o.action!=before_action_) {uncertain_=true;return;}
        before_ok_=false;
        apply_observation(s,o,false);
    }
    coordination::NativeReceipt transition(const CameraBinding& b,const coordination::ActionPlan& p,uint64_t actual) noexcept override {
        coordination::NativeReceipt r;State s;
        if(!lifetime(b,s) || !last_.valid || last_.actual!=actual || last_.action!=p.action)return r;
        // Preserve literal per-queue failures and successful partial peers even
        // when no use/detach capability can be issued. Unknown is not unobserved.
        for(unsigned i=0;i<2;++i) {
            const auto& q=last_.queues[i];if(!q.observed)continue;
            r.queues[i]={true,true,q.result,coordination::QueueState::Unknown,
                coordination::Ownership::Unknown,q.id,0};
        }
        if(uncertain_)return r;
        if(p.action==coordination::Action::Connect)r.use=last_use_;
        else if(!local_connection_ || !detached_)return r;
        else r.use=coordination::UseDisposition::NativeConnection;
        for(unsigned i=0;i<2;++i) {
            const auto& q=last_.queues[i];if(!q.observed)continue;
            r.queues[i]={true,true,q.result,
                p.action==coordination::Action::Connect?coordination::QueueState::Connected:coordination::QueueState::Disconnected,
                r.use==coordination::UseDisposition::NativeConnection?coordination::Ownership::NativeConnection:coordination::Ownership::Borrowed,
                q.id,0}; // No service/header generation invented from IDs.
        }
        return r;
    }
    bool borrow_ready(const CameraBinding& b,const CurrentRenderSelection& selection,render::NativeBorrow n) noexcept {
        SourceIdentity identity;
        if(!current_mode_ || !identity_lifetime(b,identity) || !selection.consume(b,identity,n) ||
           uncertain_ || !journal_ || !connected_ || current_connect_ || current_update_ ||
           !n.provenance.frame || !n.provenance.origin || n.provenance.ticket<=settled_ticket_)return false;
        return accept_borrow(b,n);
    }
    bool borrow_ready(const CameraBinding& b,render::NativeBorrow n) noexcept override {
        if(current_mode_)return false; // opt-in requires a live collector-issued selection
        State s;
        if(!lifetime(b,s) || uncertain_ || !journal_ || !connected_ || s.ids!=ids_ || !s.active || !s.connected)return false;
        return accept_borrow(b,n);
    }
private:
    bool accept_borrow(const CameraBinding& b,render::NativeBorrow n) noexcept {
        const auto& p=n.provenance;
        if(n.camera!=b.camera || !n.invocation_sp || !n.command ||
           !p.ticket || !p.recording_generation || p.eye>1 || !n.watermark || n.watermark<=settled_watermark_ ||
           p.identity.renderer!=b.renderer || p.identity.device!=b.device ||
           p.identity.renderer_generation!=b.renderer_generation || p.identity.device_generation!=b.device_generation ||
           n.resource_index>1 || n.slot>=16 || !n.selected_texture)return false;
        uint64_t sampled=0;std::memcpy(&sampled,n.original.data()+0x80,8);
        if(sampled!=n.selected_texture)return false;
        for(size_t i=0;i<n.original.size();++i) {
            if((i>=8 && i<16) || (i>=0x38 && i<0x40) || (i>=0x58 && i<0x60))continue;
            if(n.original[i]!=n.replacement[i])return false;
        }
        if(watermark_ && n.watermark!=watermark_)return false;
        if(borrows_[p.eye].provenance.ticket)return false;
        const auto& other=borrows_[1-p.eye];
        if(other.provenance.ticket && (other.command!=n.command || other.provenance.recording_generation!=p.recording_generation ||
            other.provenance.frame!=p.frame || other.provenance.ticket==p.ticket))return false;
        if(current_mode_ && other.provenance.ticket &&
           (other.provenance.identity.process_instance!=p.identity.process_instance || other.provenance.origin!=p.origin ||
            other.provenance.lease!=p.lease || other.provenance.geometry!=p.geometry ||
            other.provenance.deadline!=p.deadline || other.provenance.revocation!=p.revocation))return false;
        if(p.ticket>highest_ticket_)highest_ticket_=p.ticket;
        watermark_=n.watermark;borrows_[p.eye]=n;return true;
    }
public:
    bool backend(const CameraBinding& b,uint64_t ticket,const render::Recorded& record,uint64_t command,uint64_t recording) noexcept override {
        State s;if(!lifetime(b,s) || uncertain_)return false;
        for(const auto& n:borrows_)if(n.provenance.ticket==ticket && ticket)
            return n.command==command && n.provenance.recording_generation==recording && n.replacement==record.prefix;
        return false;
    }
    bool settled(const CameraBinding& b,uint64_t watermark) noexcept override {
        State s;if(!lifetime(b,s) || uncertain_ || !watermark)return false;
        if(watermark==settled_watermark_)return true;
        if(watermark!=watermark_ || !borrows_[0].provenance.ticket || !borrows_[1].provenance.ticket)return false;
        // Called only AFTER Runtime's frozen sampled-use/reverse-release coverage
        // and existing Command/Lifecycle exact completion. No extra GPU registry.
        settled_ticket_=highest_ticket_;settled_watermark_=watermark;borrows_={};watermark_=0;return true;
    }
private:
    bool stock_owner() const noexcept {
        return getpid()==stock_process_ && (stock_owner_check_?stock_owner_check_(context_):
            std::this_thread::get_id()==stock_constructor_owner_);
    }
    void apply_observation(const State& s,const ClientSourceBinding::Observation& o,bool stock) noexcept {
        if(o.provider && provider_ && o.provider!=provider_){uncertain_=true;return;}
        if(o.provider)provider_=o.provider;
        if(o.action==coordination::Action::Connect) {
            const bool empty=!o.queues[0].observed && !o.queues[1].observed;
            if(empty) {
                if(o.actual!=1 || !journal_ || !connected_ || s.ids!=ids_)uncertain_=true;
                last_use_=coordination::UseDisposition::AuthenticatedBorrowedUse;return;
            }
            bool fresh=true;
            for(unsigned i=0;i<2;++i) {
                const auto& q=o.queues[i];
                if(!q.observed || q.result || !q.id || q.id!=s.ids[i] || q.alias!=q.id ||
                   q.ownership==ClientSourceBinding::Entry::Unknown ||
                   (q.ownership==ClientSourceBinding::Entry::NewEntry && !q.header)) {uncertain_=true;return;}
                fresh&=q.ownership==ClientSourceBinding::Entry::NewEntry;
            }
            if(o.actual!=1 || !provider_ || s.ids[0]==s.ids[1]){uncertain_=true;return;}
            ids_=s.ids;journal_=connected_=true;detached_=false;
            local_connection_=!stock && before_local_ && fresh;
            last_use_=local_connection_?coordination::UseDisposition::NativeConnection:
                coordination::UseDisposition::AuthenticatedBorrowedUse;
        } else if(o.action==coordination::Action::Disconnect) {
            // Only the actual aggregate decision authenticates a held no-op.
            // Empty Release outside that scope remains uncertain, never detach.
            if(held_disconnect_ && o.actual==1 && !o.queues[0].observed && !o.queues[1].observed &&
               connected_ && s.ids==ids_)return;
            // Native Release=1 without two Disconnect results is NOT detach.
            for(unsigned i=0;i<2;++i)if(!o.queues[i].observed || o.queues[i].result || o.queues[i].id!=ids_[i]) {
                uncertain_=true;return;
            }
            if(o.actual!=1 || !journal_){uncertain_=true;return;}
            connected_=false;detached_=true;
        } else {
            // An authentic existing Update may establish narrow warm use. Its
            // exact manager/header/ref observations do not imply a new owner or
            // a producer generation. Never recover an uncertain partial call.
            bool complete=true,attempted=false;
            for(unsigned i=0;i<2;++i) {
                const auto& r=o.reads[i];
                attempted|=r.acquire_observed || r.release_observed;
                complete&=r.acquire_observed && !r.acquire_result && r.returned &&
                    r.release_observed && !r.release_result && r.increment && r.decrement &&
                    r.queue && r.header && r.id==s.ids[i];
                if(stock)complete&=r.metadata_return_observed && r.metadata && r.data && r.ref_address;
            }
            if(stock && complete && (!s.ids[0] || !s.ids[1] || s.ids[0]==s.ids[1])) {uncertain_=true;return;}
            if(stock && !complete && (attempted || (!journal_ && (s.active || s.connected || s.ids[0] || s.ids[1])))) {
                uncertain_=true;return;
            }
            if(journal_ && s.ids!=ids_){uncertain_=true;return;}
            if(complete && provider_ && s.active && s.connected) {
                if(!journal_){ids_=s.ids;journal_=connected_=true;local_connection_=false;}
            }
        }
    }
    template<uint64_t Method> bool current_result(const CameraBinding& b,const NativeSourceResult<Method>& r) const noexcept {
        return r.process_==stock_process_ && r.caller_==std::this_thread::get_id() && r.owner_==r.caller_ &&
            r.binding_==b && r.runtime_==identity_.runtime && r.version_==identity_.version && r.method_==Method &&
            r.facts_.has(NativeFieldFacts::CLIENT) && r.facts_.values.client==identity_.client_module;
    }
    bool identity_lifetime(const CameraBinding& b,SourceIdentity& out) noexcept {
        if(!current_mode_ || !stock_owner_check_ || !stock_owner() || stock_invalid_.load() ||
           !identity_auth_ || !identity_auth_(context_,b,out) || !out.runtime || !out.version || !out.client_module ||
           (bound_ && !(binding_==b)) || (client_ && client_!=out.client_module) ||
           (identity_.runtime && !(identity_==out)) ||
           (stock_runtime_ && (stock_runtime_!=out.runtime || stock_version_!=out.version)))return false;
        identity_=out;bound_=true;binding_=b;client_=out.client_module;return true;
    }
    bool lifetime(const CameraBinding& b,State& s) noexcept {
        if(current_mode_) {SourceIdentity identity;return identity_lifetime(b,identity); }
        if(stock_invalid_.load()){uncertain_=true;return false;}
        if(!auth_ || !auth_(context_,b,s) || (bound_ && !(binding_==b)))return false;
        if(bound_ && client_!=s.client)return false;
        bound_=true;binding_=b;client_=s.client;return true;
    }
    const int stock_process_=getpid();
    std::atomic<bool> stock_invalid_{false};
    uint64_t stock_stream_=0,stock_version_=0,stock_sequence_=0,stock_runtime_=0;
    std::thread::id stock_owner_{};
    const std::thread::id stock_constructor_owner_=std::this_thread::get_id();
    void* context_;Authenticate auth_;StockOwner stock_owner_check_;
    AuthenticateIdentity identity_auth_=nullptr;bool current_mode_=false;SourceIdentity identity_{};
    uint64_t current_connect_=0,current_update_=0,last_current_invocation_=0,settled_ticket_=0,highest_ticket_=0;
    CameraBinding binding_{};uint64_t client_=0,provider_=0,watermark_=0,settled_watermark_=0;
    bool bound_=false,uncertain_=false,journal_=false,connected_=false,detached_=false,local_connection_=false;
    bool before_ok_=false,before_local_=false,held_disconnect_=false;
    coordination::Action before_action_=coordination::Action::None;
    coordination::UseDisposition last_use_=coordination::UseDisposition::None;
    std::array<uint64_t,2> ids_{};
    ClientSourceBinding::Observation last_{};
    std::array<render::NativeBorrow,2> borrows_{};
};
}
