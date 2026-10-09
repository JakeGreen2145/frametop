// SPDX-License-Identifier: MIT
#include "lifecycle.hpp"
namespace native_portal::binding {
uint64_t Generations::issue() noexcept {
    if(getpid()!=pid_ || std::this_thread::get_id()!=thread_ || last_==UINT64_MAX) return 0;
    return ++last_;
}
Lifecycle::Lifecycle(Command &c,Memory m,Generations &g) noexcept : command_(c),memory_(m),ids_(g) {}
bool Lifecycle::bind(const Owner &o,uint64_t tid) noexcept {
    if(!command_.thread() || getpid()!=ids_.pid_ || std::this_thread::get_id()!=ids_.thread_) return false;
    if(state_.bound || !memory_.read || !tid || !o.generation ||
       o.generation>ids_.last_ || o.generation<=ids_.claimed_) return false;
    // A successor may be published before a DIFFERENT pool command completes
    // reset+begin and issues a later ID. Require issued, unclaimed, monotonically
    // claimed authority, not "last globally issued"; Evidence verifies current C.
    // Reuse Command's full owner comparison without weakening it to pointer equality.
    Scope s{o,1,tid,1,1,o.command};
    if(!command_.scope(s)) return false;
    ids_.claimed_=o.generation;
    owner_=o;tid_=tid;state_.bound=true;return true;
}
bool Lifecycle::fail() noexcept {state_.unsupported=true;return false;}
bool Lifecycle::read(uint64_t p,void *out,size_t n) const noexcept {
    return p && p<=UINT64_MAX-n && memory_.read && memory_.read(memory_.user,p,out,n);
}
bool Lifecycle::word(uint64_t p,uint64_t &out) const noexcept {return read(p,&out,sizeof(out));}
bool Lifecycle::scan(uint64_t offset,unsigned &mask) const noexcept {
    // Whole native vectors, not sampled tickets. Hard work cap refuses large scenes.
    // No scratch vector, heap, syscall, logging, wait, or native mutation.
    uint64_t lo=0,hi=0;mask=0;
    if(owner_.identity.renderer>UINT64_MAX-offset-16 ||
       !word(owner_.identity.renderer+offset,lo) || !word(owner_.identity.renderer+offset+8,hi) ||
       hi<lo || (hi-lo)%0x98 || (hi-lo)/0x98>64 || (lo&7)) return false;
    unsigned total=0;
    for(uint64_t pass=lo;pass<hi;pass+=0x98) {
        uint64_t a=0,z=0;
        if(!word(pass+0x50,a) || !word(pass+0x58,z) || z<a || (z-a)%0x2b0 || (a&7) ||
           (z-a)/0x2b0>128-total) return false;
        total+=static_cast<unsigned>((z-a)/0x2b0);
        for(uint64_t packet=a;packet<z;packet+=0x2b0) {
            uint64_t marker=0;
            if(!word(packet+8,marker)) return false;
            if(marker!=command_.tickets_[0].marker) continue;
            uint64_t ptr=0,len=0,ticket=0;
            std::array<char,16> label{};
            if(!word(packet+0x160,ptr) || !word(packet+0x168,len) || len!=15 ||
               !read(ptr,label.data(),label.size()) || label[15] ||
               !render::decode_ticket(label,len,ticket)) return false;
            unsigned bit=0;
            for(unsigned i=0;i<2;++i) if(ticket==command_.tickets_[i].ticket) bit=1u<<i;
            if(!bit || (mask&bit)) return false;
            mask|=bit;
        }
    }
    return true;
}
bool Lifecycle::empty(uint64_t offset) const noexcept {
    uint64_t lo=0,hi=0;
    return word(owner_.identity.renderer+offset,lo) && word(owner_.identity.renderer+offset+8,hi) && lo==hi;
}
bool Lifecycle::complete() const noexcept {
    render::Binding b{};b.identity=owner_.identity;
    return command_.completed(b,command_.token_);
}
bool Lifecycle::capture(Site site,const Capture &r,uint64_t tid,uint64_t generation,uint64_t vk) noexcept {
    if(!command_.thread()) return false; // no mutable reads on foreign threads/processes
    if(!state_.bound || !command_.lifetime_) return false;
    // Unrelated common-queue/reset/begin objects stay stock. Same-C unknown generation
    // cannot become stock just because its backend tickets were already consumed.
    uint64_t object=owner_.command;
    if(site==Site::QueueEnter) object=r.x[1];
    if(site==Site::ResetEnter || site==Site::BeginEnter) object=r.x[0];
    if(object!=owner_.command) return true;
    if(tid!=tid_ || generation!=owner_.generation || vk!=owner_.vk_command ||
       !r.sp || state_.unsupported) return fail();
    unsigned current=0,retained=0;
    switch(site) {
    case Site::QueueEnter: {
        uint64_t actual=0;
        if(queued_ || state_.invalidated || reset_open_ || !command_.pair_recorded() ||
           !word(object+0x738,actual) || actual!=owner_.vk_command) return fail();
        queued_=true;return true;
    }
    case Site::CurrentEnter:
        if(current_open_ || state_.current || r.x[19]!=owner_.identity.renderer ||
           !command_.pair_recorded() || !scan(0x15a0,current) || current!=3 ||
           !scan(0x15b8,retained) || retained) return fail();
        current_sp_=r.sp;current_open_=true;return true;
    case Site::CurrentPost:
        if(!current_open_ || r.sp!=current_sp_ || r.x[19]!=owner_.identity.renderer ||
           !empty(0x15a0) || !scan(0x15b8,retained) || retained!=3) return fail();
        current_open_=false;state_.current=true;return true;
    case Site::RetainedEnter:
        if(!state_.current || retained_open_ || state_.retained || r.x[22]!=owner_.identity.renderer ||
           !scan(0x15a0,current) || current || !scan(0x15b8,retained) || retained!=3) return fail();
        if(!word(owner_.identity.renderer+0x15b8,retained_base_)) return fail();
        retained_sp_=r.sp;retained_open_=true;return true;
    case Site::RetainedPost: {
        uint64_t base=0;
        if(!retained_open_ || r.sp!=retained_sp_ || r.x[22]!=owner_.identity.renderer ||
           !scan(0x15a0,current) || current || !empty(0x15b8) ||
           !word(owner_.identity.renderer+0x15b8,base) || base!=retained_base_) return fail();
        retained_open_=false;state_.retained=true;return true;
    }
    case Site::ResetEnter: {
        uint64_t actual=0;
        // The native reset routine itself has NO exact GPU-completion guard.
        // Seeing an early reset poisons settlement; never pretend observation stopped it.
        if(reset_open_ || begin_open_ || state_.invalidated || !queued_ || !complete() ||
           !r.x[30] || !word(object+0x738,actual) || actual!=owner_.vk_command) return fail();
        reset_sp_=r.sp;reset_lr_=r.x[30];reset_open_=true;return true;
    }
    case Site::ResetResult:
        if(!reset_open_ || r.sp>UINT64_MAX-0x20 || r.sp+0x20!=reset_sp_ ||
           r.x[19]!=owner_.command) return fail();
        reset_open_=false;
        if(static_cast<uint32_t>(r.x[0])) return false; // failed reset retains old executable
        state_.invalidated=true;begin_expected_=true;return true;
    case Site::BeginEnter:
        if(!begin_expected_ || begin_open_ || r.sp!=reset_sp_ || r.x[30]!=reset_lr_) return fail();
        begin_expected_=false;begin_open_=true;begin_sp_=r.sp;return true;
    case Site::BeginResult:
        if(!begin_open_ || r.sp>UINT64_MAX-0x30 || r.sp+0x30!=begin_sp_) return fail();
        begin_open_=false;
        if(static_cast<uint32_t>(r.x[0])) return false; // old reset success remains valid
        state_.next_generation=ids_.issue();
        if(!state_.next_generation) return fail();
        return true;
    }
    return fail();
}
LifecycleState Lifecycle::state() const noexcept {return command_.thread()?state_:LifecycleState{};}
bool Lifecycle::settled(const render::Binding &b,const render::Provenance &a,
                        const render::Provenance &c,const render::Completion &token) const noexcept {
    if(!command_.thread() || !state_.bound || state_.unsupported || !queued_ ||
       !state_.current || !state_.retained || !state_.invalidated || command_.bad_) return false;
    render::Completion saved{};
    // Exact tickets AND exact immutable token, including Command's resubmission debt.
    return command_.submission(a,c,saved) && command_.completed(b,token);
}
bool Lifecycle::settled_callback(void *u,const render::Binding &b,const render::Provenance &a,
                                 const render::Provenance &c,const render::Completion &t) noexcept {
    return u && static_cast<Lifecycle *>(u)->settled(b,a,c,t);
}
}
