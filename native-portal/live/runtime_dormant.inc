// SPDX-License-Identifier: MIT
// Included in Runtime::Impl. Metadata exclusion and synchronous original scopes.
    RuntimeAdmission dormant_admission;
    bool dormant_enabled=false,dormant_engaged=false; // owner writes; enable immutable after pin
    uint64_t dormant_version=0;
    std::array<uint64_t,256> dormant_sites{};size_t dormant_site_count=0;
    std::optional<StockSourceTransport> dormant_history;
    std::atomic<uint64_t> dormant_invocations{0};
    struct DormantScope {
        Impl* runtime;const pid_t process;const uint64_t version,invocation;
        CameraAccessGate& gate;const uint64_t ticket;const CameraBinding binding;
        DormantScope* previous;
        StockSourceCollector* method=nullptr;
        uint64_t method_pc=0;
        unsigned native_depth=0;
        inline static thread_local DormantScope* top=nullptr;
        DormantScope(Impl& r,uint64_t id,CameraAccessGate& g,uint64_t t,const CameraBinding& b):
            runtime(&r),process(getpid()),version(r.dormant_version),invocation(id),gate(g),ticket(t),binding(b),previous(top) {top=this;}
        ~DormantScope(){if(top!=this)std::abort();top=previous;}
    };
    DormantScope* dormant_scope() noexcept {
        auto* scope=DormantScope::top;
        return scope && scope->runtime==this && scope->process==pid && getpid()==pid &&
            scope->version==dormant_version?scope:nullptr;
    }
    bool dormant_configure(const uint64_t* sites,size_t count) noexcept {
        if(pinned || !sites || !count || count>dormant_sites.size())return false;
        if(!stock_facts.install_coverage(sites,count))return false;
        std::copy(sites,sites+count,dormant_sites.begin());dormant_site_count=count;
        for(const auto& site:NativeStockReadBoundary::required_sites)
            if(!dormant_covered(this,site.module,site.pc))return false;
        if(!evidence.begin_dormant_stock(stock_facts.reader()))return false;
        native_source.emplace(this,CurrentSourceContracts{source_identity},source_stock_owner);
        source=&*native_source;dormant_enabled=true;return true;
    }
    static bool dormant_covered(void* p,ClientSourceBinding::Module module,uint64_t pc) noexcept {
        const auto& self=*static_cast<Impl*>(p);
        const uint64_t key=pc|(module==ClientSourceBinding::Module::Client?(UINT64_C(1)<<63):0);
        for(size_t i=0;i<self.dormant_site_count;++i)if(self.dormant_sites[i]==key)return true;
        return false;
    }
    static bool dormant_provider_live(void* p,const StockSourceReadScope& observation,uint64_t provider) noexcept {
        auto& self=*static_cast<Impl*>(p);auto* scope=self.dormant_scope();
        if(!scope || !scope->method || !provider || observation.runtime!=uint64_t(&self) ||
           observation.version!=scope->version || observation.ticket!=scope->ticket ||
           observation.method!=scope->method_pc || !(observation.binding==scope->binding))return false;
        const auto invocation=scope->method->invocation();
        if(observation.sequence!=invocation.sequence || observation.ticket!=invocation.ticket ||
           observation.method!=invocation.method || observation.runtime!=invocation.runtime ||
           observation.version!=invocation.version)return false;
        CameraBinding held;uint64_t comp=0,client=0;
        if(!scope->gate.lookup(scope->ticket,held) || !(held==scope->binding) ||
           !ft_native_portal_source_modules || !ft_native_portal_source_modules(&comp,&client) || comp!=self.base || !client)return false;
        // Conditional platform lifetime comes from this suspended native method's
        // actual provider call, not the camera ticket. The concrete boundary and
        // binding engine separately check exact PC, frame, target and pinned vtable.
        return observation.module==ClientSourceBinding::Module::Compositor?
            observation.saved.x[0]==provider:
            observation.pc==0x32a49c && observation.saved.x[20]==provider;
    }
    bool dormant_bound(const CameraBinding& b,uint64_t version) noexcept {
        if(!dormant_enabled)return true;
        dormant_version=version;
        dormant_history.emplace(b,version,std::this_thread::get_id(),uint64_t(this));
        return dormant_admission.bind(version);
    }
    static bool dormant_writer_binding(uint64_t pc,const arena::Saved& s,const CameraBinding& b) noexcept {
        switch(pc) {
        case 0x6d6e0:case 0x6d6f0:return s.x[0]==b.application;
        case 0x1366b0:case 0x136118:case 0x13a100:return s.x[0]==b.context;
        case 0x15f9e0:case 0x15fba8:case 0x15f938:case 0x15faf8:case 0x15fcc0:return s.x[0]==b.camera;
        default:return true; // stock mode method receiver is authenticated by its native nested camera entry
        }
    }
    bool dormant_original_call(uint64_t pc,arena::Saved& s,DormantScope& scope) noexcept {
        if(!dormant_writer_binding(pc,s,scope.binding))camera_route_abort(CameraFatalReason::WriterBinding,pc);
        const auto target=ft_native_portal_original?ft_native_portal_original(pc):0;
        if(!target)camera_route_abort(CameraFatalReason::NativeResult,pc);
        if(scope.native_depth==8) {
            dormant_history->invalidate(); // bound collector storage, not native recursion
            if(!ft_native_portal_call_saved(target,&s))camera_route_abort(CameraFatalReason::NativeResult,pc);
            s.pad=arena::SkipOwned;return true;
        }
        struct Depth {unsigned& value;Depth(unsigned& v):value(v){++value;}~Depth(){--value;}} depth(scope.native_depth);
        const bool method=pc==0x15f9e0 || pc==0x15fba8 || pc==0x15fcc0;
        bool returned=false;
        if(method) {
            // One stack-bounded collector per actual method. Overlap is history
            // uncertainty, never permission to suppress or reschedule native work.
            StockSourceCollector collector(scope.gate,*dormant_history,{nullptr,this,dormant_covered,dormant_provider_live});
            uint64_t comp=0,client=0;
            const bool began=ft_native_portal_source_modules && ft_native_portal_source_modules(&comp,&client) && comp==base &&
                collector.begin(scope.ticket,scope.binding,scope.version,pc,s,comp,client,
                    ClientSourceBinding::compositor_sha,ClientSourceBinding::client_sha);
            const auto invocation=collector.invocation();
            auto* prior=scope.method;const auto prior_pc=scope.method_pc;
            scope.method=&collector;scope.method_pc=pc;
            returned=ft_native_portal_call_saved(target,&s);
            if(began && returned)collector.complete_own_bypass(invocation,s.x[0]);else collector.cancel();
            scope.method=prior;scope.method_pc=prior_pc;
        } else returned=ft_native_portal_call_saved(target,&s);
        if(!returned)camera_route_abort(CameraFatalReason::NativeResult,pc);
        s.pad=arena::SkipOwned;return true;
    }
    bool dormant_original(uint64_t pc,arena::Saved& s) noexcept {
        if(!dormant_enabled)return false;
        if(auto* scope=dormant_scope())return dormant_original_call(pc,s,*scope);
        CameraOwner::RetainedCaller caller;
        if(!dormant_admission.original(dormant_version,[&]{
            caller=camera_owner.retain_caller(camera_owner.epoch(),camera_binding.renderer_generation);return bool(caller);
        })) {
            if(dormant_admission.phase()==RuntimeAdmission::Phase::Engaged)return false;
            camera_route_abort(CameraFatalReason::SubmitRefused,pc);
        }
        uint64_t invocation=dormant_invocations.fetch_add(1)+1;
        if(!invocation)camera_route_abort(CameraFatalReason::SubmitRefused,pc);
        bool called=false;
        if(!caller.observe_scope([&](CameraAccessGate& gate,uint64_t ticket,const CameraBinding& held){
            if(!(held==camera_binding))camera_route_abort(CameraFatalReason::WriterBinding,pc);
            DormantScope scope(*this,invocation,gate,ticket,held);
            called=dormant_original_call(pc,s,scope);
        }) || !called)camera_route_abort(CameraFatalReason::NativeResult,pc);
        if(!caller.release())camera_route_abort(CameraFatalReason::RetentionNotReleased,pc);
        dormant_admission.original_return(dormant_version);return true;
    }
    bool dormant_engage() noexcept {
        if(!dormant_enabled || dormant_engaged)return true;
        if(tid()!=owner_tid || !camera_bound.load() || stopped.load())return false;
        const bool engaged=dormant_admission.engage(dormant_version,[&]{
            if(source_foreign.load() || !stock_facts.complete() || !dormant_history || dormant_history->failed())return false;
            const auto current=evidence.stock_command_facts();if(!current.generation)return false;
            if(!native_source->fold_stock_history(*dormant_history))return false;
            return evidence.engage_stock(evidence.identity(),current.generation);
        });
        if(engaged)dormant_engaged=true;
        return engaged;
    }
    bool dormant_fact_site(uint64_t pc) const noexcept {
        if(!dormant_enabled || dormant_engaged)return false;
        if(stock_facts.observing())return true; // existing event boundary, copied receipts only
        if(pc==0x29a14c || pc==0x29a150 || pc==0x2a23f4 || pc==0x2a1f64)return true;
        size_t count=0;const auto* sites=runtime::NativeStockFactCollector::required_sites(count);
        return std::find(sites,sites+count,pc)!=sites+count;
    }
