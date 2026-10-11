// SPDX-License-Identifier: MIT
// Included by evidence.cpp to keep every existing Evidence consumer linked.
bool Evidence::owner_event(uint64_t pc,const arena::Saved &s,uint16_t *rejection) noexcept {
    if(rejection)*rejection=0;
    // IDs are local to the relative PC. Record at the first failed predicate,
    // with the original short-circuit order and no extra native reads.
    auto check=[&](bool ok,uint16_t id) {if(!ok && rejection)*rejection=id;return ok;};
    if(!check(getpid()==pid_,255) || !check(pinned_,254))return false;
    if(pc==0x14099c) {
        if(!check(owner(),1) || !check(!handoff_.load(),2) || !check(!worker_,3) ||
           !check(s.x[19],4) || !check(s.x[21],5))return false;
        uint64_t vtable=0,context=0;
        if(!check(read_self(s.x[19],&vtable,8),6) || !check(vtable==base_+0x5a1428,7) ||
           !check(read_self(s.x[19]+0x48,&context,8),8) || !check(context==s.x[21],9))return false;
        worker_=s.x[19];context_=context;return true;
    }
    if(pc==0x140dbc) {
        if(!check(owner(),1) || !check(worker_,2) || !check(s.x[0]==worker_,3) || !check(s.x[21]==context_,4))return false;
        uint64_t worker=0,R=0;
        if(!check(initialized(),5) || !check(read_self(context_+0x98,&worker,8),6) || !check(worker==worker_,7) ||
           !check(read_self(context_+0x1c0,&scheduler_,8),8) || !check(scheduler_,9) ||
           !check(read_self(scheduler_+8,&application_,8),10) || !check(application_,11) ||
           !check(read_self(application_+0x256a0,&R,8),12) || !check(R==identity_.renderer,13) ||
           !check(!staging_ticket_,14) || !check(!call_,15) || !check(!lifecycle_,16) ||
           !check(!turnover_.phase,17))return false;
        handoff_.store(1,std::memory_order_release);return true;
    }
    if(pc!=0x14e5a8)return false;
    if(!check(handoff_.load(std::memory_order_acquire)==1,1) || !check(s.x[0]==worker_,2) ||
       !check(s.x[30]==base_+0x182aa8,3))return false;
    uint64_t context=0,worker=0,scheduler=0,app=0,R=0,device=0;uint32_t native_tid=0,status=0;
    if(!check(read_self(worker_+0x48,&context,8),4) || !check(context==context_,5) ||
       !check(read_self(context+0x98,&worker,8),6) || !check(worker==worker_,7) ||
       !check(read_self(worker_+0x10,&native_tid,4),8) || !check(native_tid==uint32_t(syscall(SYS_gettid)),9) ||
       !check(read_self(worker_+0x14,&status,4),10) || !check(status==0x80000000,11) ||
       !check(read_self(context+0x1c0,&scheduler,8),12) || !check(scheduler==scheduler_,13) ||
       !check(read_self(scheduler+8,&app,8),14) || !check(app==application_,15) ||
       !check(read_self(app+0x256a0,&R,8),16) || !check(R==identity_.renderer,17) ||
       !check(read_self(R+0xa78,&device,8),18) || !check(device==identity_.device,19))return false;
    unsigned expected=1;if(!check(handoff_.compare_exchange_strong(expected,2),20))return false;
    tid_=pid_t(native_tid);generations_.thread_=std::this_thread::get_id();
    if(bridge_)bridge_->scope_.thread=native_tid;
    run_=true;return true;
}
Evidence::ApplicationOwner Evidence::application_owner() const noexcept {
    if(!initialized() || !run_ || handoff_.load()!=2)return {};
    return {worker_,context_,scheduler_,application_};
}
bool Evidence::graph_capable() const noexcept {
    return initialized() && !dormant_stock() && run_ && handoff_.load()==2 && recording_;
}
bool Evidence::graph_read(uint64_t p,void *out,size_t n) const noexcept {
    if(!owner() || dormant_stock() || failed_ || !graph_scope_ || !p || !n || n>UINT64_MAX-p)return false;
    for(size_t i=0;i<graph_count_;++i) {
        const auto &s=graph_spans_[i];
        if(p>=s.begin && p-s.begin<=s.size && n<=s.size-(p-s.begin))return read_self(p,out,n);
    }
    return false;
}
bool Evidence::graph_capture(bool attachments) noexcept {
    graph_count_=0;graph_scope_=false;graph_owned_=false;
    if(!initialized() || dormant_stock() || !run_)return false;
    auto add_graph=[&](uint64_t begin,uint64_t end,uint64_t stride,size_t cap)->bool {
        if(end<begin || (!begin && end) || (begin&7) || (end-begin)%stride || (end-begin)/stride>cap)return false;
        if(end==begin)return true;
        // A role cannot alias any registered native allocation or another role.
        for(const auto &span:spans_)if(span.epoch==epoch_ && begin<span.begin+span.size && span.begin<end)return false;
        for(size_t i=0;i<graph_count_;++i)if(begin<graph_spans_[i].begin+graph_spans_[i].size && graph_spans_[i].begin<end) {
            return begin==graph_spans_[i].begin && end-begin==graph_spans_[i].size;
        }
        if(graph_count_==graph_spans_.size())return false;
        graph_spans_[graph_count_++]={begin,end-begin};return true;
    };
    const auto R=identity_.renderer;
    size_t packets=0;
    for(uint64_t off:{uint64_t(0x15a0),uint64_t(0x15b8)}) {
        uint64_t lo=0,hi=0;if(!at(R+off,lo) || !at(R+off+8,hi) || !add_graph(lo,hi,0x98,64))return false;
        for(uint64_t p=lo;p<hi;p+=0x98) {
            uint64_t a=0,z=0;
            if(!read_self(p+0x50,&a,8) || !read_self(p+0x58,&z,8) || !add_graph(a,z,0x2b0,1024-packets))return false;
            packets+=(z-a)/0x2b0;
            for(uint64_t packet=a;packet<z;packet+=0x2b0) {
                uint64_t marker=0;if(!read_self(packet+8,&marker,8))return false;
                graph_owned_=graph_owned_ || (marker_ && marker==marker_);
            }
            if(attachments && off==0x15a0 && a!=z)for(uint64_t field:{uint64_t(0),uint64_t(8),uint64_t(0x10)}) {
                uint64_t target=0;if(!read_self(p+field,&target,8))return false;
                if(target && (target>UINT64_MAX-0x150 || !add_graph(target,target+0x150,1,0x150)))return false;
            }
        }
    }
    if(attachments) {
        uint64_t lo=0,hi=0;
        if(!at(R+0x1478,lo) || !at(R+0x1480,hi) || !add_graph(lo,hi,0x40,128))return false;
    }
    graph_scope_=true;return true;
}
bool Evidence::graph_event(uint64_t pc,const arena::Saved &s) noexcept {
    if(!owner() || !run_ || dormant_stock())return false;
    if(pc==0x2a1e18) {
        if(s.x[0]!=identity_.renderer || uint32_t(s.x[1]) || s.x[30]!=base_+0x6caf0)return false;
        if(dispatch_)return fail(pc);
        if(!graph_capable() || !graph_capture(true))return false;
        dispatch_=true;dispatch_sp_=s.sp;transferred_=false;return true;
    }
    if(pc==0x2a2230) {
        // End preparation/pipeline borrowing BEFORE native move/free. The new
        // call-local disposal scope separately follows current and retained roles.
        dispatch_=false;graph_scope_=false;graph_count_=0;
        return s.x[19]==identity_.renderer && graph_capture(false);
    }
    if(pc==0x2a2320 || pc==0x2987d8 || pc==0x298840) {
        graph_scope_=false;graph_count_=0;
        const auto R=pc==0x2a2320?s.x[19]:s.x[22];
        const bool ok=R==identity_.renderer && graph_capture(false);
        if(pc==0x2a2320)transferred_=ok;
        return ok;
    }
    if(pc==0) {graph_scope_=false;graph_count_=0;return true;}
    return false;
}
bool Evidence::command_owner(binding::Owner &out) const noexcept {
    out={};render::Arena a{};
    if(!initialized() || dormant_stock() || !recording_ || !arena(a))return false;
    out.identity=identity_;out.recording=out.generation=recording_;
    out.command=a.command;out.arena=a.resource;out.buffer=a.buffer;out.map=a.map;
    out.capacity=a.capacity;out.alignment=a.alignment;
    return at(a.command+0x738,out.vk_command) && out.vk_command;
}
bool Evidence::settled(const render::Binding &b,const render::Provenance &a,const render::Provenance &c,const render::Completion &t) const noexcept {
    return owner() && initialized() && lifecycle_ && lifecycle_->settled(b,a,c,t);
}
