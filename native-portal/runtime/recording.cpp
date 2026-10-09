// SPDX-License-Identifier: MIT
#include "evidence.hpp"
namespace native_portal::runtime {
bool Evidence::recording_event(uint64_t pc,const arena::Saved &s) noexcept {
    if(pc==0x2a1e18) {
        if(s.x[0]!=identity_.renderer || uint32_t(s.x[1]))return false;
        // Real calls through this renderer's +250 slot, executed independently
        // in bootstrap_native.py. No inference from the live first-failure PC.
        bool caller=false;
        for(uint64_t lr:{0x6caf0u,0x9e6e0u,0x15a9bcu,0x15ad64u,0x294d10u,0x294e58u,0x2955a8u})
            caller=caller || s.x[30]==base_+lr;
        binding::Owner o{};
        if(!caller || turnover_.phase || s.sp<0xd0 || !command_owner(o) ||
           (!run_ && (handoff_.load() || lifecycle_ || wrapper_ || marker_ || staging_ticket_ || call_)))return fail(pc);
        uint64_t pool=0;
        if(!at(o.command+0x50,pool) || !pool || pool>UINT64_MAX-0xd0)return fail(pc);
        trace_command(o.command,o.vk_command);
        turnover_={s.sp-0xd0,o.command,command_wrapper_,o.vk_command,o.generation,pool,1};
        turnover_.caller=s.x[30]-base_;
        return true;
    }
    if((pc==0x2a2230 || pc==0x2a2320) && turnover_.phase) {
        if(s.x[19]!=identity_.renderer || s.sp!=turnover_.sp ||
           turnover_.phase!=(pc==0x2a2230?1u:2u))return fail(pc);
        ++turnover_.phase;
        // Owned recordings still require the existing graph/Lifecycle proofs.
        if(!run_)return true;
    }
    // The new-command path allocates/constructs 0x7b0 bytes before this call.
    // Pooled commands can begin BEFORE wrapper acquisition, so keep a bounded
    // pending ledger. Never infer a begin from the later wrapper publication.
    if(pc==0x301808) {
        if(creation_ || s.x[0]!=s.x[20] || !s.x[0] || s.x[0]>UINT64_MAX-0x7b0 || s.sp<0x70) return fail(pc);
        uint64_t D=0;
        if(!read_self(s.x[0]+8,&D,8) || D!=identity_.renderer+0x50) return false;
        for(const auto &c:recordings_) if(c.command==s.x[0]) return fail(pc);
        creation_=s.x[0];creation_sp_=s.sp;begin_success_=false;return true;
    }
    if(pc==0x300a08 && creation_) {
        if(begin_open_ || begin_success_ || s.x[0]!=creation_ || s.x[19]!=creation_ ||
           s.sp!=creation_sp_-0x40 || s.x[30]!=base_+0x300ae0 ||
           !read_self(creation_+0x738,&begin_vk_,8) || !begin_vk_) return fail(pc);
        begin_sp_=s.sp;begin_open_=true;return true;
    }
    if(pc==0x300a38 && creation_) {
        uint64_t lr=0,vk=0;
        if(!begin_open_ || s.sp!=begin_sp_-0x30 || s.x[19]!=creation_ ||
           !read_self(s.sp+8,&lr,8) || lr!=base_+0x300ae0 ||
           !read_self(creation_+0x738,&vk,8) || vk!=begin_vk_) return fail(pc);
        begin_open_=false;begin_success_=static_cast<uint32_t>(s.x[0])==0;
        return begin_success_;
    }
    if(pc==0x30180c) {
        if(!creation_ || begin_open_ || s.sp!=creation_sp_ || s.x[20]!=creation_) return fail(pc);
        const auto command=creation_;creation_=0;
        if(!begin_success_ || !(s.x[0]&255)) {begin_success_=false;return false;}
        begin_success_=false;
        for(auto &c:recordings_) if(!c.command) {
            c={command,begin_vk_,0,true};return true;
        }
        return fail(pc); // bounded exhaustion cannot become partial tracking
    }
    if(pc==0x2a23f4) {
        PublicationDiagnostic d{};d.phase=turnover_.phase;d.caller=turnover_.caller;
        d.return_pc=s.x[30]>=base_ && s.x[30]-base_<0x600000?s.x[30]-base_:0;
        d.recording=recording_;d.attempt=++publication_attempts_;
        d.flags=uint64_t(run_) | uint64_t(transferred_)<<1 | uint64_t(dispatch_)<<2 |
            uint64_t(bool(lifecycle_))<<3 | uint64_t(graph_owned_)<<4 |
            uint64_t(s.sp==turnover_.sp)<<5;
        for(const auto &c:recordings_)d.entries+=bool(c.command);
        // Preserve evaluation order and native read count. Unchecked bits mean
        // unknown, NOT false. Save before fail() erases the command ledger.
        auto check=[&](bool ok,unsigned id) {
            d.checked|=1ull<<id;if(ok)d.passed|=1ull<<id;
            else {d.predicate=id;if(id>=5 && !publication_.predicate)publication_=d;}
            return ok;
        };
        if(!check(ready_,1) || !check(s.x[19]==identity_.renderer,2) ||
           !check(!command_wrapper_,3) || !check(s.x[0],4))return false;
        if(!check(turnover_.phase==5,5) || !check(s.sp==turnover_.sp,6) ||
           !check(!run_ || transferred_,7))return fail(pc);
        uint64_t C=0,D=0,vk=0;
        if(!check(read_self(s.x[0]+8,&C,8),8) || !check(read_self(C+8,&D,8),9) ||
           !check(D==identity_.renderer+0x50,10) || !check(read_self(C+0x738,&vk,8),11) ||
           !check(vk,12))return fail(pc);
        trace_command(C,vk);
        Recording *candidate=nullptr;
        for(auto &c:recordings_)if(c.command==C) {candidate=&c;break;}
        if(!check(candidate,13))return fail(pc);
        auto &c=*candidate;d.generation=c.generation;d.renewal=c.renewal;
        d.flags|=uint64_t(c.begun)<<6 | uint64_t(c.vk==vk)<<7;
        if(!check(c.vk==vk,14) || !check(c.begun,15))return fail(pc);
        if(lifecycle_ && spans_[6].begin==lifecycle_owner_.command && C!=spans_[6].begin)spans_[7]=spans_[6];
        if(!check(add(2,s.x[0],0x10),16) || !check(add(6,C,0x7b0),17))return fail(pc);
        command_wrapper_=s.x[0];
        if(!c.generation)c.generation=generations_.issue();
        d.generation=c.generation;
        if(!check(c.generation,18))return fail(pc);
        c.published=true;c.stock_pool=0;recording_=c.generation;turnover_={};return true;
    }
    if(pc==0x305260 || pc==0x2953a0) {
        if(s.x[0]!=command_wrapper_) return false;
        uint64_t C=0,vk=0,published=1,D=0;bool generation=false;
        for(const auto &c:recordings_)if(c.command==turnover_.command && c.vk==turnover_.vk &&
            c.generation==turnover_.generation && c.begun)generation=true;
        if(pc==0x305260 && turnover_.phase==3 && s.sp==turnover_.sp &&
           s.x[19]==identity_.renderer && s.x[30]==base_+0x2a23b4 && generation &&
           recording_==turnover_.generation && command_wrapper_==turnover_.wrapper &&
           at(identity_.renderer+0x1380,published) && !published &&
           at(command_wrapper_+8,C) && C==turnover_.command && at(C+8,D) && D==identity_.renderer+0x50 &&
           at(C+0x738,vk) && vk==turnover_.vk &&
           (!run_ || transferred_) &&
           ((!lifecycle_ && !graph_owned_) || (run_ && lifecycle_ && lifecycle_->state().current && !lifecycle_->state().unsupported))) {
            // Native cleared R+1380 before calling the wrapper destructor. The
            // nested pool return is a separate event, not executable destruction.
            spans_[2]={};command_wrapper_=0;turnover_.phase=4;return true;
        }
        fail(pc);return true; // any other release remains unsupported
    }
    if(pc==0x311648 && turnover_.phase==4 && s.x[1]==turnover_.command) {
        uint64_t pool=0,vk=0;
        if(s.sp+0x30!=turnover_.sp || s.x[30]!=base_+0x3052f0 || s.x[19]!=turnover_.wrapper ||
           !at(turnover_.command+0x50,pool) || pool!=turnover_.pool || s.x[0]!=pool+0xd0 ||
           !at(turnover_.command+0x738,vk) || vk!=turnover_.vk)return fail(pc);
        // No new publication may borrow this submitted generation without reset
        // plus Begin. Owned settlement remains exclusively Lifecycle-issued.
        for(auto &c:recordings_)if(c.command==turnover_.command)c.begun=false;
        turnover_.phase=5;return true;
    }
    if(pc==0x311648 || pc==0x4b3d0 || pc==0x3003d0) {
        const auto command=pc==0x311648?s.x[1]:s.x[0];
        if(!command) return false;
        if(command==creation_) return fail(pc);
        for(auto &c:recordings_) if(c.command==command) {
            if(spans_[6].begin==command || spans_[7].begin==command) {fail(pc);return true;}
            // The pinned stock tail at 297ca0 loads C from its stack slot,
            // obtains C+50, and calls this pool's return method at LR297cb4.
            // Trial 11 returns an unpublished C here on bootstrap and Run.
            // Preserve identity only; no Begin, generation or settlement is
            // inferred. Current/retained commands stay on the Lifecycle path.
            if(pc==0x311648 && s.x[30]==base_+0x297cb4 && !c.published &&
               !c.generation && c.begun && !c.renewal &&
               (!lifecycle_ || lifecycle_owner_.command!=command)) {
                uint64_t D=0,pool=0,vk=0;
                if(!read_self(command+8,&D,8) || D!=identity_.renderer+0x50 ||
                   !read_self(command+0x50,&pool,8) || !pool || pool>UINT64_MAX-0xd0 ||
                   s.x[0]!=pool+0xd0 || (c.stock_pool && c.stock_pool!=pool) ||
                   !read_self(command+0x738,&vk,8) || vk!=c.vk)return fail(pc);
                c.stock_pool=pool;c.begun=false;return true;
            }
            c={};return true;
        }
        return false;
    }
    // Reset invalidates authorization at ENTRY, even on raw failure. Only the
    // bound Lifecycle may issue its successor from reset+begin success. No local
    // counter, arena cursor or changed address substitutes for that proof.
    if(run_ && (pc==0x300b20 || pc==0x300a08 || pc==0x300c54 || pc==0x300a38)) {
        const auto object=(pc==0x300b20 || pc==0x300a08)?s.x[0]:s.x[19];
        for(auto &c:recordings_)if(c.command==object) {
            uint64_t vk=0;if(!read_self(c.command+0x738,&vk,8) || vk!=c.vk)return fail(pc);
            if(c.stock_pool) {
                uint64_t D=0,pool=0;
                if(!read_self(c.command+8,&D,8) || D!=identity_.renderer+0x50 ||
                   !read_self(c.command+0x50,&pool,8) || pool!=c.stock_pool ||
                   s.x[19]!=c.command || s.x[20]!=pool ||
                   ((pc==0x300b20 || pc==0x300a08) && s.x[30]!=base_+0x2a9cdc))return fail(pc);
            }
            if(pc==0x300b20) {
                if(c.generation==recording_)recording_=0;
                c.generation=0;c.begun=false;c.reset_sp=s.sp;c.reset_lr=s.x[30];c.renewal=1;
            } else if(pc==0x300c54) {
                if(c.renewal!=1 || s.sp+0x20!=c.reset_sp || uint32_t(s.x[0]))return fail(pc);
                c.renewal=2;
            } else if(pc==0x300a08) {
                if(c.renewal!=2 || s.sp!=c.reset_sp || s.x[30]!=c.reset_lr)return fail(pc);
                c.renewal=3;
            } else {
                if(c.renewal!=3 || s.sp+0x30!=c.reset_sp || uint32_t(s.x[0]))return fail(pc);
                c.renewal=0;c.begun=true;
            }
            break;
        }
    }
    if(!run_ && (pc==0x300b20 || pc==0x300a08)) {
        for(auto &c:recordings_)if(c.command==s.x[0])return fail(pc);
    }
    if(!lifecycle_)return false;
    const auto previous=lifecycle_->state();
    if((pc==0x2a2230 || pc==0x2a2320) && previous.current)return true;
    if((pc==0x2987d8 || pc==0x298840) && previous.retained)return true;
    if((pc==0x300c54 || pc==0x300a38) && s.x[19]!=lifecycle_owner_.command)return false;
    binding::Site site;
    switch(pc) {
    case 0x300b20:site=binding::Site::ResetEnter;break;
    case 0x300c54:site=binding::Site::ResetResult;break;
    case 0x300a08:site=binding::Site::BeginEnter;break;
    case 0x300a38:site=binding::Site::BeginResult;break;
    case 0x3018b0:site=binding::Site::QueueEnter;break;
    case 0x2a2230:site=binding::Site::CurrentEnter;break;
    case 0x2a2320:site=binding::Site::CurrentPost;break;
    case 0x2987d8:site=binding::Site::RetainedEnter;break;
    case 0x298840:site=binding::Site::RetainedPost;break;
    default:return false;
    }
    binding::Capture capture{};
    for(unsigned i=0;i<31;++i) capture.x[i]=s.x[i];
    capture.sp=s.sp;
    if(!lifecycle_->capture(site,capture,static_cast<uint64_t>(tid_),lifecycle_owner_.generation,lifecycle_owner_.vk_command)) return false;
    const auto state=lifecycle_->state();
    if(site==binding::Site::BeginResult && state.next_generation && state.current && state.retained &&
       state.invalidated && !state.unsupported) {
        for(auto &c:recordings_) if(c.command==lifecycle_owner_.command) {
            c.generation=state.next_generation;c.begun=true;
            if(spans_[6].begin==c.command)recording_=c.generation;
            return true;
        }
        return fail(pc);
    }
    return true;
}
bool Evidence::command_read_callback(void *u,uint64_t p,void *out,size_t n) noexcept {
    if(!u) return false;
    const auto &e=*static_cast<Evidence*>(u);
    if(!e.owner()) return false;
    if(e.graph_read(p,out,n))return true;
    if(n!=8)return false;
    // Byte bounds on R/C are not authority to reinterpret those allocations as
    // graph nodes. No pass/packet/name bytes are borrowed by this binding yet.
    const auto R=e.identity_.renderer,C=e.lifecycle_owner_.command;
    if(p!=R+0x15a0 && p!=R+0x15a8 && p!=R+0x15b8 && p!=R+0x15c0 &&
       (!C || p!=C+0x738)) return false;
    return e.read(p,out,n);
}
bool Evidence::bind_lifecycle(binding::Command &command,const binding::Owner &o) noexcept {
    render::Arena a{};
    if(!initialized() || !identity_.device_generation || !same_identity(o.identity) || !arena(a) ||
       !recording_ || o.recording!=recording_ || o.generation!=recording_ ||
       o.command!=a.command || o.arena!=a.resource || o.buffer!=a.buffer || o.map!=a.map ||
       o.capacity!=a.capacity || o.alignment!=a.alignment) return false;
    uint64_t vk=0;if(!at(a.command+0x738,vk) || o.vk_command!=vk) return false;
    if(lifecycle_) {
        const auto state=lifecycle_->state();
        if(!state.current || !state.retained || !state.invalidated || state.unsupported ||
           state.next_generation!=recording_) return false;
    }
    lifecycle_.emplace(command,binding::Memory{this,command_read_callback},generations_);
    if(!lifecycle_->bind(o,static_cast<uint64_t>(tid_))) {lifecycle_.reset();return false;}
    lifecycle_owner_=o;return true;
}
bool Evidence::release_lifecycle() noexcept {
    if(!owner())return false;
    if(!lifecycle_)return true;
    const auto state=lifecycle_->state();
    if(!state.current || !state.retained || !state.invalidated || state.unsupported || !state.next_generation)return false;
    lifecycle_.reset();lifecycle_owner_={};spans_[7]={};return true;
}
} // namespace native_portal::runtime
