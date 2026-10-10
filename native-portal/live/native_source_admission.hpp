// SPDX-License-Identifier: MIT
#pragma once
#include "source_admission.hpp"
#include <cstring>
namespace native_portal::live {
// Only Runtime supplies this retained observation boundary. It checks the gate,
// loader module handles, owner, application/camera/renderer and device association
// before reading scalars. No API here authorizes independent source reads.
class NativeSourceAdmission final:public SourceAdmission {
public:
    struct State {uint64_t client=0;std::array<uint64_t,2> ids{};uint8_t active=0,connected=0,external=0;};
    using Authenticate=bool(*)(void*,const CameraBinding&,State&) noexcept;
    NativeSourceAdmission(void* context,Authenticate auth):context_(context),auth_(auth) {}
    bool connect_eligible(const CameraBinding& b) noexcept override {
        State s;if(!lifetime(b,s) || uncertain_ || s.external)return false;
        if(!s.active && !s.connected && !s.ids[0] && !s.ids[1])return true;
        return journal_ && s.ids==ids_ && ((connected_ && s.active && s.connected) ||
            (detached_ && !s.active && !s.connected));
    }
    bool transition_eligible(const CameraBinding& b,coordination::Action action) noexcept override {
        State s;if(!lifetime(b,s) || uncertain_)return false;
        if(action==coordination::Action::Connect)return connect_eligible(b);
        // Off and source settlement need no fresh image. A Disconnect still
        // requires the original locally owned connection, never Borrowed USE.
        return action==coordination::Action::None ||
            (local_connection_ && connected_ && s.ids==ids_);
    }
    void before(const CameraBinding& b,coordination::Action action) noexcept override {
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
        last_=o;State s;
        if(!before_ok_ || !lifetime(b,s) || !o.valid || o.action!=before_action_) {uncertain_=true;return;}
        before_ok_=false;
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
            local_connection_=before_local_ && fresh;
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
            bool complete=true;
            for(unsigned i=0;i<2;++i) {
                const auto& r=o.reads[i];
                complete&=r.acquire_observed && !r.acquire_result && r.returned &&
                    r.release_observed && !r.release_result && r.increment && r.decrement &&
                    r.queue && r.header && r.id==s.ids[i];
            }
            if(journal_ && s.ids!=ids_){uncertain_=true;return;}
            if(complete && provider_ && s.active && s.connected) {
                if(!journal_){ids_=s.ids;journal_=connected_=true;local_connection_=false;}
            }
        }
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
    bool borrow_ready(const CameraBinding& b,render::NativeBorrow n) noexcept override {
        State s;const auto& p=n.provenance;
        if(!lifetime(b,s) || uncertain_ || !journal_ || !connected_ || s.ids!=ids_ ||
           !s.active || !s.connected || n.camera!=b.camera || !n.invocation_sp || !n.command ||
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
        watermark_=n.watermark;borrows_[p.eye]=n;return true;
    }
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
        settled_watermark_=watermark;borrows_={};watermark_=0;return true;
    }
private:
    bool lifetime(const CameraBinding& b,State& s) noexcept {
        if(!auth_ || !auth_(context_,b,s) || (bound_ && !(binding_==b)))return false;
        if(bound_ && client_!=s.client)return false;
        bound_=true;binding_=b;client_=s.client;return true;
    }
    void* context_;Authenticate auth_;
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
