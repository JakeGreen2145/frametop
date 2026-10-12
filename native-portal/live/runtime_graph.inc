// SPDX-License-Identifier: MIT
// One dispatch loan. Admission metadata is copied under its short lock; no lock
// spans a native call. CameraOwner's caller retains the binding, not graph rights.
    static constexpr uint64_t graph_sites[]={0x2a2498,0x2a2564,0x2a2870,0x2983c8,
        0x298750,0x29ce00,0x29b278,0x299a60,0x2993f8,0x2990c8,0x2988a8};
    CameraOwner::RetainedCaller graph_caller;
    struct DispatchLoan {
        uint64_t invocation=0,version=0,sp=0,lr=0,owner=0;
        CameraBinding binding{};
        runtime::Evidence::StockCommandFacts recording{};
        bool preparation=false,optimizer_seen=false;
        SourceIdentity source{};
    } graph_loan;
    uint64_t graph_sequence=0;
    bool graph_disposal_reader=false;
    static bool graph_authority(void* user,bool preparation) noexcept {
        const auto& p=*static_cast<Impl*>(user);
        if(getpid()!=p.pid || tid()!=p.owner_tid)return false;
        if(!p.dormant_enabled)return !p.production_source; // explicit-source embedding only
        return preparation?p.graph_live():p.graph_disposal_reader;
    }
    bool graph_provider(const Provenance& v) noexcept {
        // The delivered provider proof covers the already selected stock C++
        // IVRResources receiver and original names. NEVER call the cold generic
        // interface getter here. Generic/braced names can call log subscribers.
        uint64_t comp=0,client=0,receiver=0,vptr=0,slot0=0,slot2=0;
        if(!graph_live() || !supported_pair(v.vs,v.ps) || !ft_native_portal_source_modules ||
           !ft_native_portal_source_modules(&comp,&client) || comp!=base || !client || client!=graph_loan.source.client_module ||
           !get(base+0x5b0e48,receiver) || !receiver || !get(receiver,vptr) || vptr!=client+0x6121d8 ||
           !get(vptr,slot0) || slot0!=client+0x3ccfc0 || !get(vptr+16,slot2) || slot2!=client+0x3cd878)return false;
        for(const auto& name:graph_resource_names) {
            uint64_t pointer=base+name.string;
            if(name.pointer && (!get(base+name.pointer,pointer) || pointer!=base+name.string))return false;
            char value[128]{};const size_t n=std::strlen(name.text)+1;
            if(n>sizeof value || !runtime::read_self(pointer,value,n) || std::memcmp(value,name.text,n))return false;
        }
        // Unchanged image/data, selected receiver lifetime, native deferred
        // ownership and ordinary imported-library bindings remain conditions.
        return graph_live();
    }
    bool graph_coverage() const noexcept {
        for(auto pc:graph_sites)if(!dormant_covered(const_cast<Impl*>(this),ClientSourceBinding::Module::Compositor,pc))return false;
        return true;
    }
    bool graph_active() const noexcept {return dormant_admission.graph().invocation!=0;}
    bool graph_live() const noexcept {
        return graph_loan.invocation && graph_loan.preparation && getpid()==pid && tid()==graph_loan.owner &&
            graph_loan.version==dormant_version && graph_loan.binding==camera_binding &&
            graph_loan.recording.generation==command_owner.recording && same(command_owner.identity,binding.identity);
    }
    void graph_conflict(uint64_t pc,const arena::Saved& s) noexcept {
        const auto g=dormant_admission.graph();if(!g.invocation)return;
        bool conflict=false;
        // Exact selected renderer only. Unrelated process-global observations
        // are not a writer to this graph. W may retire normally after cutoff.
        if(pc==0x29bd18 || pc==0x29c238)conflict=s.x[0]==g.renderer;
        if(pc==0x2a1e18)conflict=s.x[0]==g.renderer;
        if(pc==0x29f1e8 && tid()==owner_tid && backend_open && s.x[0]==g.renderer)conflict=true;
        if(g.preparation && pc==0x29ce00 && s.x[0]==g.renderer && tid()==owner_tid &&
           !graph_loan.optimizer_seen && s.sp==graph_loan.sp-0xd0 && s.x[30]==base+0x2a1ecc) {
            // 2a1ec8 ALWAYS calls Collapse. Pinned 29ce1c..29ce30 reads the
            // zero flag and returns without graph mutation. Permit that exact
            // once-only ordinary no-op; recursive or now-nonzero calls stop.
            uint8_t optimize=1;
            if(graph_live() && get(g.renderer+0x15d0,optimize) && !optimize) {
                graph_loan.optimizer_seen=true;return;
            }
        }
        if(g.preparation) {
            switch(pc) {
            case 0x2983c8:case 0x298750:case 0x29ce00:case 0x29b278:
            case 0x299a60:case 0x2993f8:case 0x2990c8:case 0x2988a8:
                conflict=s.x[0]==g.renderer;break;
            default:break;
            }
        }
        if(pc==0x4b3d0 && s.x[0])conflict=s.x[0]==g.renderer || (g.preparation &&
            (s.x[0]==g.wrapper || s.x[0]==g.command || s.x[0]==g.pipelines[0] || s.x[0]==g.pipelines[1]));
        if(pc==0x2a2870 && tid()==owner_tid)conflict=true;
        if(conflict)camera_route_abort(CameraFatalReason::MutationDebt,pc);
    }
    bool graph_begin(const arena::Saved& s) noexcept {
        if(!dormant_enabled)return !production_source; // never production fallback on missing installed API
        // Present is deliberately NOT authorized by its LR alone.
        if(!dormant_engaged || stopped || camera_closing || !camera_bound || !graph_coverage() ||
           graph_loan.invocation || current_render_active || camera_active || s.sp<0x830 ||
           s.x[0]!=binding.identity.renderer || uint32_t(s.x[1]) || s.x[30]!=base+0x6caf0 ||
           graph_sequence==UINT64_MAX)return false;
        const auto facts=evidence.stock_command_facts();
        if(!facts.begun || !facts.wrapper || !facts.command || facts.generation!=command_owner.recording ||
           !same(evidence.identity(),binding.identity))return false;
        RuntimeAdmission::Graph g{};g.invocation=++graph_sequence;g.renderer=s.x[0];
        g.wrapper=facts.wrapper;g.command=facts.command;g.preparation=true;
        if(!dormant_admission.graph_begin(dormant_version,g,[&]{
            graph_caller=camera_owner.retain_caller(camera_owner.epoch(),camera_binding.renderer_generation);
            return bool(graph_caller);
        }))return false;
        CameraBinding held;
        if(!graph_caller.binding(held) || !(held==camera_binding))camera_route_abort(CameraFatalReason::WriterBinding,0x2a1e18);
        graph_loan={g.invocation,dormant_version,s.sp,s.x[30],tid(),held,facts,true};
        if(!source_identity(this,held,graph_loan.source)) {
            if(!graph_caller.release())camera_route_abort(CameraFatalReason::RetentionNotReleased,0x2a1e18);
            dormant_admission.graph_end(dormant_version,g.invocation);graph_loan={};return false;
        }
        return true;
    }
    void graph_boundary(uint64_t pc,const arena::Saved& s) noexcept {
        if(!graph_loan.invocation)return;
        if(pc==0x2a2230) {
            // Always revoke preparation before any disposal preflight or native
            // movement. A mismatched cutoff cannot release lifetime or debt.
            graph_loan.preparation=false;
            dormant_admission.graph_cutoff(graph_loan.version,graph_loan.invocation);
            if(backend_open)camera_route_abort(CameraFatalReason::MutationDebt,pc);
        }
        if(pc!=0x2a2498 && pc!=0x2a2564)return;
        if(tid()!=graph_loan.owner || s.sp!=graph_loan.sp || s.x[30]!=graph_loan.lr)return;
        // The pinned restored RET has no future graph-dependent instruction.
        // This is last-use retirement BEFORE RET, not proof RET executed, native
        // completion, successful submission, join, or settlement.
        if(backend_open)camera_route_abort(CameraFatalReason::MutationDebt,pc);
        evidence.graph_event(pc,s);
        if(!graph_caller.release())camera_route_abort(CameraFatalReason::RetentionNotReleased,pc);
        dormant_admission.graph_end(graph_loan.version,graph_loan.invocation);graph_loan={};
    }
