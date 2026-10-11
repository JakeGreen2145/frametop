// SPDX-License-Identifier: MIT
// Included inside Runtime::Impl. Current source scopes never sample camera tuples.
    static bool source_identity(void* p,const CameraBinding& b,SourceIdentity& out) noexcept {
        auto& self=*static_cast<Impl*>(p);CameraBinding held;uint64_t comp=0,client=0;
        if(getpid()!=self.pid || tid()!=self.owner_tid || !self.dormant_enabled || !self.camera_bound.load() ||
           !self.camera_owner.retained_binding(held) || !(held==b) || !(b==self.camera_binding) ||
           !self.source_integrity() || !ft_native_portal_source_modules(&comp,&client) || comp!=self.base || !client)return false;
        out={uint64_t(&self),self.dormant_version,client};return true;
    }
    enum class SourceRoute {Event,Field,Boundary,Return};
    struct CurrentMethodScope {
        void* collector;
        bool (*route)(void*,SourceRoute,ClientSourceBinding::Module,uint64_t,const arena::Saved&) noexcept;
        CameraAccessGate& gate;const CameraBinding binding;
        uint64_t ticket,sequence,method;
    };
    CurrentMethodScope* current_method=nullptr; // owner-only, synchronous native bypass
    bool current_method_ready=false,current_source_failed=false;
    template<class Collector> static bool current_method_route(void* p,SourceRoute route,
        ClientSourceBinding::Module module,uint64_t pc,const arena::Saved& s) noexcept {
        auto& collector=*static_cast<Collector*>(p);
        switch(route) {
        case SourceRoute::Field:return collector.camera_fact(module,pc,s);
        case SourceRoute::Boundary:return collector.boundary_event(module,pc,s);
        case SourceRoute::Return:return collector.camera_return(pc,s);
        default:return collector.event(module,pc,s);
        }
    }
    static bool current_provider_live(void* p,const StockSourceReadScope& scope,uint64_t provider) noexcept {
        auto& self=*static_cast<Impl*>(p);
        if(getpid()!=self.pid || tid()!=self.owner_tid || !self.current_method || !provider)return false;
        const auto& method=*self.current_method;SourceIdentity identity;CameraBinding held;
        if(scope.runtime!=uint64_t(&self) || scope.version!=self.dormant_version || scope.ticket!=method.ticket ||
           scope.sequence!=method.sequence || scope.method!=method.method || !(scope.binding==method.binding) ||
           !method.gate.lookup(method.ticket,held) || !(held==method.binding) || !source_identity(&self,held,identity))return false;
        return scope.module==ClientSourceBinding::Module::Compositor?scope.saved.x[0]==provider:
            scope.pc==0x32a49c && scope.saved.x[20]==provider;
    }
    template<class Completion,class Collector> bool current_original_method(uint64_t pc,arena::Saved& s) noexcept {
        current_method_ready=false;bool called=false,returned=false,observed=false;
        const auto target=ft_native_portal_original?ft_native_portal_original(pc):0;
        if(!target)return false;
        const bool prior=camera_active;camera_active=true;
        camera_owner.observe_scope([&](CameraAccessGate& gate,uint64_t ticket,const CameraBinding& held){
            if(!(held==camera_binding) || s.x[0]!=held.camera)return;
            Completion completion(held,dormant_version,std::this_thread::get_id(),uint64_t(this));
            Collector collector(gate,completion,{nullptr,this,dormant_covered,current_provider_live});
            const bool reserved=pc==0x15f9e0?native_source->begin_current_connect(held,completion.invocation_id()):
                native_source->begin_current_update(held,completion.invocation_id());
            uint64_t comp=0,client=0;
            const bool began=reserved && ft_native_portal_source_modules && ft_native_portal_source_modules(&comp,&client) && comp==base &&
                collector.begin(ticket,held,dormant_version,pc,s,comp,client,ClientSourceBinding::compositor_sha,ClientSourceBinding::client_sha);
            const auto invocation=collector.invocation();
            CurrentMethodScope scope{&collector,current_method_route<Collector>,gate,held,ticket,invocation.sequence,pc};
            auto* previous=current_method;current_method=&scope;
            called=true;returned=ft_native_portal_call_saved(target,&s);
            const bool completed=began && returned && collector.complete_own_bypass(invocation,s.x[0]);
            auto result=completion.take();
            if constexpr(std::is_same_v<Completion,CurrentConnectCompletion>)
                observed=completed && result && native_source->finish_current_connect(held,*result);
            else observed=completed && result && native_source->finish_current_update(held,*result);
            current_method=previous;
        });
        if(!called) {
            // Retention refusal is not an invented native result. Callers retain
            // their existing complete-original semantics but optional use closes.
            source_foreign.store(true);returned=ft_native_portal_call_saved(target,&s);
        }
        // Partial/missing native results may have connected a peer even though
        // no portal token was committed. Retain the existing backend source hold
        // and closing barrier; never call that an unused integration owner.
        if(!observed) {current_source_failed=true;source_foreign.store(true);backend.camera_pending(true);}
        camera_active=prior;current_method_ready=observed;return returned;
    }
    bool current_render_coverage() const noexcept {
        for(const auto pc:CurrentRenderCollector::required_sites)
            if(!dormant_covered(const_cast<Impl*>(this),ClientSourceBinding::Module::Compositor,pc))return false;
        return true;
    }
    CameraOwner::RetainedCaller current_render_caller;
    std::optional<CurrentRenderCollector> current_render;
    uint64_t current_render_invocation=0,current_render_sp=0,current_render_lr=0;
    bool current_render_active=false,current_render_failed=false;
    void current_render_event(uint64_t pc,const arena::Saved& s) noexcept {
        if(!dormant_enabled || !dormant_engaged || tid()!=owner_tid)return;
        if(pc==0x160be8) {
            // Unclaimed stock rendering has no integration upload reservation.
            // Do not turn its ordinary continuation into a failed owned scope.
            if(!claimed_layer)return;
            if(current_render_active || current_render_failed || !dormant_admission.reader(dormant_version)) {current_render_failed=true;return;}
            current_render_caller=camera_owner.retain_caller(camera_owner.epoch(),camera_binding.renderer_generation);
            if(!current_render_caller){dormant_admission.reader_return(dormant_version);current_render_failed=true;return;}
            bool began=false;const uint64_t invocation=++current_render_invocation;
            current_render_caller.observe_scope([&](CameraAccessGate& gate,uint64_t ticket,const CameraBinding& held){
                if(!current_render)current_render.emplace(gate,this,source_identity,dormant_covered);
                began=invocation && current_render->begin(ticket,held,invocation,base,s);
            });
            if(!began) {
                current_render_failed=true;
                if(!current_render_caller.release())std::abort();
                dormant_admission.reader_return(dormant_version);return;
            }
            current_render_active=true;current_render_sp=s.sp;current_render_lr=s.x[30];return;
        }
        bool selected=false;
        for(const auto site:CurrentRenderCollector::required_sites)selected|=site==pc;
        if(!selected || !current_render_active)return;
        if(!current_render->capture(pc,s))current_render_failed=true;
        if((pc==0x160dd4 || pc==0x161f30) && s.sp==current_render_sp && s.x[30]==current_render_lr) {
            current_render_active=false;
            if(!current_render_caller.release())std::abort();
            dormant_admission.reader_return(dormant_version);
        }
    }
    static bool current_borrow(void* p,const CurrentRenderSelection& selection,render::NativeBorrow borrow) noexcept {
        auto& self=*static_cast<Impl*>(p);
        return self.native_source && self.native_source->borrow_ready(self.camera_binding,selection,borrow);
    }
