// SPDX-License-Identifier: MIT
#include "evidence.hpp"
namespace native_portal::runtime {
bool Evidence::recording_event(uint64_t pc,const arena::Saved &s) noexcept {
    // The new-command path allocates/constructs 0x7b0 bytes before this call.
    // Pooled commands can begin BEFORE wrapper acquisition, so keep a bounded
    // pending ledger. Never infer a begin from the later wrapper publication.
    if(pc==0x301808) {
        if(creation_ || s.x[0]!=s.x[20] || !s.x[0] || s.x[0]>UINT64_MAX-0x7b0 || s.sp<0x70) return fail();
        uint64_t D=0;
        if(!read_self(s.x[0]+8,&D,8) || D!=identity_.renderer+0x50) return false;
        for(const auto &c:recordings_) if(c.command==s.x[0]) return fail();
        creation_=s.x[0];creation_sp_=s.sp;begin_success_=false;return true;
    }
    if(pc==0x300a08 && creation_) {
        if(begin_open_ || begin_success_ || s.x[0]!=creation_ || s.x[19]!=creation_ ||
           s.sp!=creation_sp_-0x40 || s.x[30]!=base_+0x300ae0 ||
           !read_self(creation_+0x738,&begin_vk_,8) || !begin_vk_) return fail();
        begin_sp_=s.sp;begin_open_=true;return true;
    }
    if(pc==0x300a38 && creation_) {
        uint64_t lr=0,vk=0;
        if(!begin_open_ || s.sp!=begin_sp_-0x30 || s.x[19]!=creation_ ||
           !read_self(s.sp+8,&lr,8) || lr!=base_+0x300ae0 ||
           !read_self(creation_+0x738,&vk,8) || vk!=begin_vk_) return fail();
        begin_open_=false;begin_success_=static_cast<uint32_t>(s.x[0])==0;
        return begin_success_;
    }
    if(pc==0x30180c) {
        if(!creation_ || begin_open_ || s.sp!=creation_sp_ || s.x[20]!=creation_) return fail();
        const auto command=creation_;creation_=0;
        if(!begin_success_ || !(s.x[0]&255)) {begin_success_=false;return false;}
        begin_success_=false;
        for(auto &c:recordings_) if(!c.command) {
            c={command,begin_vk_,0,true};return true;
        }
        return fail(); // bounded exhaustion cannot become partial tracking
    }
    if(pc==0x305260 || pc==0x2953a0) {
        if(s.x[0]!=command_wrapper_) return false;
        fail();return true; // wrapper relinquishes its native reference before reuse
    }
    if(pc==0x311648 || pc==0x4b3d0 || pc==0x3003d0) {
        const auto command=pc==0x311648?s.x[1]:s.x[0];
        if(!command) return false;
        if(command==creation_) return fail();
        for(auto &c:recordings_) if(c.command==command) {
            if(spans_[6].begin==command) {fail();return true;}
            c={};return true;
        }
        return false;
    }
    // Reset invalidates authorization at ENTRY, even on raw failure. Only the
    // bound Lifecycle may issue its successor from reset+begin success. No local
    // counter, arena cursor or changed address substitutes for that proof.
    if(pc==0x300b20 || pc==0x300a08) {
        for(auto &c:recordings_) if(c.command==s.x[0]) {
            if(c.generation==recording_) recording_=0;
            c.generation=0;c.begun=false;break;
        }
    }
    if(!lifecycle_) return false;
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
            c.generation=state.next_generation;recording_=c.generation;return true;
        }
        return fail();
    }
    return true;
}
bool Evidence::command_read_callback(void *u,uint64_t p,void *out,size_t n) noexcept {
    if(!u || n!=8) return false;
    const auto &e=*static_cast<Evidence*>(u);
    if(!e.owner()) return false;
    // Byte bounds on R/C are not authority to reinterpret those allocations as
    // graph nodes. No pass/packet/name bytes are borrowed by this binding yet.
    const auto R=e.identity_.renderer,C=e.spans_[6].begin;
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
} // namespace native_portal::runtime
