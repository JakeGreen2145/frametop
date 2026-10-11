// SPDX-License-Identifier: MIT
// Included inside Runtime::Impl. All original method calls use loader-generated
// bypasses. No owner queue lock is held over native execution or listener waits.
    CameraOwner camera_owner;
    static bool source_stock_owner(void* p) noexcept {
        const auto& self=*static_cast<Impl*>(p);
        return getpid()==self.pid && tid()==self.owner_tid && self.camera_bound.load();
    }
    NativeSourceAdmission legacy_source{this,source_authenticate,source_stock_owner};
    std::optional<NativeSourceAdmission> native_source; // selected once, before pin/native work
    SourceAdmission* source=nullptr;
    std::atomic<bool> camera_bound{false};
    // Serializes only owned decisions. Never held across original/native calls.
    std::mutex camera_join_mutex;
    uint64_t unbound_invocations=0,run_invalidations=0,join_version=0;
    // Start/Run lifetime is independent of whether P has arrived. Saved scalar
    // identities only; the slots span complete originals and never grant reads.
    uint64_t start_application=0,destructive_applications[64]{};
    bool start_revoked=false;
    bool run_authenticated=false;
    CameraBindingDiagnostic camera_diagnostic{};
    Mailbox<CameraBindingDiagnostic> camera_published;
    enum class JoinRefusal : uint8_t { None, NoRun, NoPublication, Inflight, Invalidated,
        Resource, App, Worker, Context, Dirty, Exhausted, NativeBind, Barrier, Stopped };
    void camera_snapshot_locked() noexcept {
        const auto c=camera_owner.candidate();
        auto& d=camera_diagnostic;
        d.publication_pc=c.seen_pc;d.publication_accepted=c.accepted;
        d.publication_sequence=c.sequence;d.version=c.version;
        d.pending=d.native_pending=c.camera!=0;d.invalidate_pc=c.last_invalidate_pc;
        d.invalidate_class=uint64_t(c.classification);d.invalidations=c.invalidations;d.invalidation_sequence=c.invalidation_sequence;
        d.resource_ready=c.resource_ready;d.native_rejection=uint64_t(c.rejection);d.gate_rejection=uint64_t(c.gate_rejection);
        const auto stage=camera_owner.bind_stage();
        d.epoch_published=c.epoch_published;d.barrier_retained=stage==CameraOwner::BindStage::Retained;
        d.internal_bound=stage>=CameraOwner::BindStage::Published;
        d.partial=stage==CameraOwner::BindStage::Published;
        d.runtime_bound=camera_bound.load();d.inflight=unbound_invocations;
        camera_published.publish(d);
    }
    void camera_join_refused(JoinRefusal why) noexcept {
        camera_diagnostic.join_rejection=uint64_t(why);
        if(!camera_diagnostic.first_failure && why!=JoinRefusal::NoRun && why!=JoinRefusal::NoPublication &&
           why!=JoinRefusal::Inflight && why!=JoinRefusal::Resource)
            camera_diagnostic.first_failure=uint64_t(why);
        camera_snapshot_locked();
    }
    CameraBinding camera_binding{};
    coordination::PortalToken portal_token{};
    std::optional<RenderDebt> render_debt;
    uint64_t camera_debt=0;
    bool camera_closing=false,camera_closed=false,camera_active=false;
    bool camera_cleanup_active=false;
    // One active retained mutation owns this decision scope. Nested app/handler
    // calls inherit its caller; they are not additional independent demand.
    coordination::DemandArbiter* camera_aggregate=nullptr;
    coordination::Caller camera_caller{};
    bool camera_method_active=false;
    struct CameraAggregateScope {
        Impl& self;
        CameraAggregateScope(Impl& p,coordination::DemandArbiter& arbiter,coordination::Caller caller)
            :self(p) {if(p.camera_aggregate)std::abort();p.camera_aggregate=&arbiter;p.camera_caller=caller;}
        ~CameraAggregateScope(){self.camera_aggregate=nullptr;self.camera_caller={};}
    };
    bool camera_method(uint64_t pc,arena::Saved& s) noexcept {
        auto& arbiter=*camera_aggregate;
        const bool acquiring=pc==0x15f9e0;
        if(camera_caller.kind!=coordination::CallerKind::Portal &&
           (!arbiter.register_caller(camera_caller) || !arbiter.set_level(camera_caller,acquiring,acquiring)))return false;
        if(arbiter.snapshot().fault)return false;
        bool called=false,ok=false;
        auto original=[&] {
            if(called)std::abort();
            called=true;
            camera_method_active=true;ok=camera_original(pc,s);camera_method_active=false;
            coordination::NativeReceipt r;
            if(ok){r.execution=coordination::Execution::Executed;r.value_valid=true;r.value=s.x[0];}
            return r;
        };
        // Stock-only native calls do not manufacture portal queue ownership.
        // Once integration owns a connection, its final native disconnect must
        // update that same per-queue journal, even if the last caller is stock.
        if(source && source_integrity() && source->transition_eligible(camera_binding,
           acquiring?coordination::Action::Connect:coordination::Action::Disconnect) &&
           (camera_caller.kind==coordination::CallerKind::Portal || arbiter.snapshot().integration_obligations)) {
            arbiter.reconcile([&](const coordination::ActionPlan& plan) {
                if((plan.action==coordination::Action::Connect)!=acquiring)return coordination::NativeReceipt{};
                auto actual=original();if(!ok)return actual;
                auto r=source_receipt(camera_binding,plan,s.x[0]);
                r.execution=actual.execution;r.value_valid=actual.value_valid;r.value=actual.value;return r;
            });
        }
        if(!called)original();
        return ok;
    }
    static bool writer_pc(uint64_t pc) noexcept {
        switch(pc) {
        case 0x6d6e0:case 0x6d6f0:case 0x1366b0:case 0x136118:case 0x13a100:
        case 0x15d730:case 0x15d198:case 0x15d248:case 0x15f9e0:case 0x15fba8:
        case 0x15f938:case 0x15faf8:case 0x15fcc0:return true;
        default:return false;
        }
    }
    static bool cleanup_pc(uint64_t pc) noexcept {
        return pc==0x8b020 || pc==0x15e1c8 || pc==0x15df70 || pc==0x162360;
    }
    bool camera_original(uint64_t pc,arena::Saved& s) noexcept {
        const auto target=ft_native_portal_original?ft_native_portal_original(pc):0;
        if(!target || getpid()!=pid || (tid()!=owner_tid && !(tid()==bootstrap_tid && joined)))return false;
        if(dormant_enabled && dormant_engaged && tid()==owner_tid) {
            if(pc==0x15f9e0)return current_original_method<CurrentConnectCompletion,CurrentConnectCollector>(pc,s);
            if(pc==0x15fcc0)return current_original_method<CurrentUpdateCompletion,CurrentUpdateCollector>(pc,s);
        }
        const bool prior=camera_active;camera_active=true;
        const bool source_call=!dormant_enabled && source_begin(pc);
        const bool ok=ft_native_portal_call_saved(target,&s);
        // Direct bypass return is authoritative; nested calls finish at their
        // actual native RET hooks. Missing nested return cannot borrow outer x0.
        if(source_observing)source_finish(s.x[0],ok && source_call);
        camera_active=prior;
        return ok;
    }
    static bool source_authenticate(void* p,const CameraBinding& b,NativeSourceAdmission::State& out) noexcept {
        auto& self=*static_cast<Impl*>(p);CameraBinding held;uint64_t comp=0,client=0;
        if(getpid()!=self.pid || tid()!=self.owner_tid || !self.camera_bound.load() ||
           !self.camera_owner.retained_binding(held) || !(held==b) || !(b==self.camera_binding) ||
           !self.source_integrity() || !ft_native_portal_source_modules(&comp,&client) || comp!=self.base)return false;
        uint64_t camera=0,renderer=0,device=0,application=0;
        if(!get(b.application+0x25320,camera) || camera!=b.camera ||
           !get(b.camera,application) || application!=b.application ||
           !get(b.camera+8,renderer) || renderer!=b.renderer ||
           !get(b.application+0x256a0,renderer) || renderer!=b.renderer ||
           !get(b.renderer+0xa78,device) || device!=b.device ||
           !get(b.camera+0x349,out.external) || !get(b.camera+0x34a,out.active) ||
           !get(b.camera+0x360,out.connected) || !get(b.camera+0x350,out.ids[0]) ||
           !get(b.camera+0x358,out.ids[1]))return false;
        out.client=client;return true;
    }
    static bool camera_borrow(void* p,render::NativeBorrow borrow) noexcept {
        auto& self=*static_cast<Impl*>(p);CameraBinding held;binding::Owner now{};
        auto& c=self.source_command;const unsigned eye=borrow.provenance.eye;
        if(eye>1 || c.tickets[eye] || (c.watermark && c.watermark!=borrow.watermark) ||
           !self.source || !self.source_integrity() || !self.camera_owner.retained_binding(held) ||
           !(held==self.camera_binding) || !self.evidence.command_owner(now) ||
           now.command!=borrow.command || now.recording!=borrow.provenance.recording_generation ||
           !same(now.identity,borrow.provenance.identity))return false;
        const bool ready=self.dormant_enabled?
            self.current_render_active && !self.current_render_failed && self.current_render &&
                self.current_render->with_selection(borrow,current_borrow,&self):self.source->borrow_ready(held,borrow);
        if(!ready)return false;
        c.tickets[eye]=borrow.provenance.ticket;c.watermark=borrow.watermark;c.owner=now;
        return true;
    }
    bool camera_ready() noexcept {
        CameraBinding held;
        const auto demand_state=camera_owner.snapshot();
        const uint64_t installed=ft_native_portal_camera_coverage?ft_native_portal_camera_coverage():0;
        return (!dormant_enabled || (!current_render_failed && current_render_coverage())) &&
            (installed&0xfffff)==0xfffff && camera_bound.load() && !camera_closing && !demand_state.fault && !demand_state.stock_full_display &&
            camera_owner.retained_binding(held) && held==camera_binding && source && source_integrity() && source->connect_eligible(held);
    }
    static bool camera_source_settled(void* p,uint64_t watermark) noexcept {
        auto& self=*static_cast<Impl*>(p);CameraBinding held;
        return self.source && self.source_integrity() && self.camera_owner.retained_binding(held) && held==self.camera_binding &&
            self.source_command_settled(watermark) && self.source->settled(held,watermark);
    }
    void camera_handoff() noexcept {
        if(tid()!=owner_tid || camera_bound.load())return;
        std::lock_guard<std::mutex> lock(camera_join_mutex);
        auto& d=camera_diagnostic;
        d.app_match=d.worker_match=d.context_match=0;
        if(!run_authenticated){camera_join_refused(JoinRefusal::NoRun);return;}
        if(stopped.load()){camera_join_refused(JoinRefusal::Stopped);return;}
        if(camera_owner.bind_stage()>=CameraOwner::BindStage::Published){camera_join_refused(JoinRefusal::Dirty);return;}
        const auto c=camera_owner.candidate();
        if(unbound_invocations || c.inflight){camera_join_refused(JoinRefusal::Inflight);return;}
        if(start_revoked || c.invalidations!=run_invalidations){camera_join_refused(JoinRefusal::Invalidated);return;}
        if(!c.camera){camera_join_refused(JoinRefusal::NoPublication);return;}
        if(!c.resource_ready){camera_join_refused(JoinRefusal::Resource);return;}
        const auto app=evidence.application_owner();const auto id=evidence.identity();
        d.app_match=1+(app.application==c.application);
        if(d.app_match!=2){camera_join_refused(JoinRefusal::App);return;}
        d.worker_match=1+(app.worker==worker);
        if(d.worker_match!=2){camera_join_refused(JoinRefusal::Worker);return;}
        d.context_match=1+(app.context==context);
        if(d.context_match!=2){camera_join_refused(JoinRefusal::Context);return;}
        // At most one attempt per immutable version and four per Runtime. A
        // published/closing epoch is never reset or retried, even without a barrier.
        if(join_version==c.version || d.join_attempts==4){camera_join_refused(JoinRefusal::Exhausted);return;}
        join_version=c.version;++d.join_attempts;
        CameraBinding b{context,c.application,c.camera,id.renderer,id.device,
            app.scheduler,id.renderer_generation,id.device_generation,id.renderer_generation};
        if(!camera_owner.bind_handoff(b,id.renderer_generation)) {
            camera_join_refused(camera_owner.bind_stage()==CameraOwner::BindStage::Published?JoinRefusal::Barrier:JoinRefusal::NativeBind);return;
        }
        camera_binding=b;camera=c.camera;
        if(!dormant_bound(b,c.version))camera_route_abort(CameraFatalReason::Publication,0x14e5a8);
        camera_bound.store(true);
        d.join_rejection=0;camera_snapshot_locked();
    }
    [[noreturn]] void camera_route_abort(CameraFatalReason reason,uint64_t pc) noexcept {
        // Foreign callers must not sample owner-only state. These atomics are
        // lock-free on supported builds; bootstrap_tid is immutable. Separate
        // loads are failure coordinates, not an authoritative state snapshot.
        static_assert(std::atomic<uint64_t>::is_always_lock_free);
        static_assert(std::atomic<bool>::is_always_lock_free);
        emit_camera_failure({reason,pc,tid(),owner_tid.load(std::memory_order_relaxed),bootstrap_tid,
            camera_bound.load(std::memory_order_relaxed),handoff_wait.load(std::memory_order_relaxed),
            stopped.load(std::memory_order_relaxed),joined.load(std::memory_order_relaxed)});
        // Keep the small primary reason ahead of the optional discriminator on
        // a constrained journal socket. Both are best-effort, never authority.
        CameraBindingDiagnostic snapshot{};
        if(camera_published.read(snapshot))emit_camera_binding(snapshot);
        std::abort();
    }
    bool camera_unbound_original(uint64_t pc,arena::Saved& s) noexcept {
        unsigned destructive_slot=64;
        {
            std::lock_guard<std::mutex> lock(camera_join_mutex);
            if(camera_bound.load())return false;
            // Runtime camera_bound is monotonic. Before its first true value,
            // only a clean pre-bind state has zero integration obligations.
            // A partially published epoch MUST NOT take this path.
            if(camera_owner.bind_stage()>=CameraOwner::BindStage::Published || unbound_invocations==64)
                camera_route_abort(CameraFatalReason::WriterBeforeHandoff,pc);
            // First Init remains governed by NativeBinding's narrow exception.
            // App cleanup also revokes R with no P. Camera/resource destruction
            // uses only the pending publication's saved camera/app identities.
            const auto c=camera_owner.candidate();
            const uint64_t application=pc==0x8b020?s.x[0]:
                ((pc==0x15df70 || pc==0x15e1c8) && c.camera && c.camera==s.x[0]?c.application:0);
            if(application) {
                for(unsigned i=0;i<64;++i)if(!destructive_applications[i]){destructive_slot=i;break;}
                if(destructive_slot==64)camera_route_abort(CameraFatalReason::WriterBeforeHandoff,pc);
                destructive_applications[destructive_slot]=application;
                if(application==start_application)start_revoked=true;
            }
            ++unbound_invocations;camera_snapshot_locked();
        }
        struct Original {Impl* self;uint64_t pc;arena::Saved* saved;};
        Original call{this,pc,&s};
        auto original=[](void* p) noexcept {
            auto& c=*static_cast<Original*>(p);
            {std::lock_guard<std::mutex> lock(c.self->camera_join_mutex);c.self->camera_snapshot_locked();}
            const auto target=ft_native_portal_original?ft_native_portal_original(c.pc):0;
            // No source observer, arbiter, replay, owner-only fields, fabricated
            // native success, or lock spans this complete original invocation.
            if(!target || !ft_native_portal_call_saved(target,c.saved))
                c.self->camera_route_abort(CameraFatalReason::CleanupOriginal,c.pc);
        };
        if(cleanup_pc(pc)) {
            const bool initial=pc==0x162360 && tid()==bootstrap_tid && s.sp && s.x[30]==base+0x92fdc;
            const auto result=camera_owner.cleanup(pc,s,original,&call,std::chrono::milliseconds(0),initial);
            if(result==binding::CameraNativeBinding::Dispatch::Refused)
                camera_route_abort(CameraFatalReason::CleanupRefused,pc);
        } else {original(&call);s.pad=arena::SkipOwned;}
        {
            std::lock_guard<std::mutex> lock(camera_join_mutex);
            if(destructive_slot!=64)destructive_applications[destructive_slot]=0;
            --unbound_invocations;camera_snapshot_locked();
        }
        // The suspended original has returned before eligibility is reconsidered.
        camera_handoff();return true;
    }
    bool camera_route(uint64_t pc,arena::Saved& s) noexcept {
        if(pc==0x15fbb8 || pc==0x15fb20) {
            if(auto* scope=dormant_scope()) {
                if(s.x[19]!=scope->binding.camera)camera_route_abort(CameraFatalReason::DecisionScope,pc);
                return true; // unchanged native decision, no held-success selection
            }
            // Actual native decision boundaries. The emitter selects 15fbc8
            // (Release's native no-op result) or 15fb74 (Clear's lock unwind).
            // Never replace x0 with logical acceptance or a cached result.
            if(!camera_bound.load())return true;
            if((tid()==owner_tid || (tid()==bootstrap_tid && joined)) && camera_cleanup_active)return true;
            if(tid()!=owner_tid || !camera_active || s.x[19]!=camera_binding.camera)camera_route_abort(CameraFatalReason::DecisionScope,pc);
            if(camera_aggregate && camera_aggregate->snapshot().producer_demand) {
                if(pc==0x15fbb8 && source && source_observing && source_method==0x15fba8)
                    source->held_disconnect(camera_binding);
                s.pad=arena::SkipOwned;
            }
            return true;
        }
        if(pc==0x8dc1c) {
            {
                std::lock_guard<std::mutex> lock(camera_join_mutex);
                if(camera_bound.load() || camera_owner.bind_stage()>=CameraOwner::BindStage::Published) {
                    camera_snapshot_locked();
                    camera_diagnostic.publication_pc=pc;camera_diagnostic.publication_accepted=0;
                    camera_diagnostic.publication_rejection=uint64_t(binding::CameraNativeBinding::Rejection::Epoch);
                    if(camera_diagnostic.publication_sequence!=UINT64_MAX)++camera_diagnostic.publication_sequence;
                    camera_published.publish(camera_diagnostic);
                    camera_route_abort(CameraFatalReason::Publication,pc);
                }
                const bool publisher=tid()==bootstrap_tid;
                if(!publisher)camera_owner.invalidate_publication(pc);
                const bool accepted=publisher && camera_owner.observe_publication(pc,s);
                camera_diagnostic.publication_rejection=publisher?uint64_t(camera_owner.candidate().rejection):253;
                camera_snapshot_locked();
                camera_diagnostic.publication_pc=pc;camera_diagnostic.publication_accepted=accepted;
                camera_published.publish(camera_diagnostic);
            }
            // Publication may be observed after R in a modeled schedule. Only
            // the actual authenticated render owner can consume this candidate.
            camera_handoff();return true;
        }
        if((cleanup_pc(pc) || writer_pc(pc)) && camera_unbound_original(pc,s))return true;
        if(cleanup_pc(pc)) {
            if(dormant_enabled && dormant_scope()) {
                dormant_admission.begin_close(dormant_version);
                camera_route_abort(CameraFatalReason::CleanupRefused,pc); // exact retained caller cannot wait for itself
            }
            // Nested resource cleanup is within the already quiescent outer
            // application cleanup. Do not reacquire or self-wait in that call.
            if((tid()==owner_tid || (tid()==bootstrap_tid && joined)) && camera_cleanup_active)return true;
            backend.invalidate(Reason::Off);
            if(tid()!=owner_tid && !(tid()==bootstrap_tid && joined))camera_route_abort(CameraFatalReason::CleanupThread,pc);
            if(camera_bound.load() && tid()==owner_tid)camera_maintenance(true);
            struct Cleanup {Impl* p;uint64_t pc;arena::Saved* saved;bool ok=false;} c{this,pc,&s};
            auto original=[](void* v) noexcept {
                auto& c=*static_cast<Cleanup*>(v);c.p->camera_cleanup_active=true;
                c.ok=c.p->camera_original(c.pc,*c.saved);c.p->camera_cleanup_active=false;
                if(!c.ok)c.p->camera_route_abort(CameraFatalReason::CleanupOriginal,c.pc);
            };
            const auto result=camera_owner.cleanup(pc,s,original,&c,std::chrono::milliseconds(0));
            {std::lock_guard<std::mutex> lock(camera_join_mutex);camera_snapshot_locked();}
            if(result==binding::CameraNativeBinding::Dispatch::Refused)camera_route_abort(CameraFatalReason::CleanupRefused,pc);
            return true;
        }
        if(!writer_pc(pc))return false;
        if(camera_bound.load() && dormant_original(pc,s))return true;
        if(camera_bound.load()) {
            const bool app_entry=pc==0x6d6e0 || pc==0x6d6f0;
            const bool camera_entry=pc==0x15f9e0 || pc==0x15fba8 || pc==0x15f938 || pc==0x15faf8 || pc==0x15fcc0;
            const bool handler=pc==0x1366b0 || pc==0x136118 || pc==0x13a100;
            if((app_entry && s.x[0]!=camera_binding.application) ||
               (camera_entry && s.x[0]!=camera_binding.camera) ||
               (handler && s.x[0]!=camera_binding.context))camera_route_abort(CameraFatalReason::WriterBinding,pc);
        }
        if(tid()==owner_tid && camera_active) {
            // Start at the nested entry, before any arbitrated bypass. Only its
            // own native RET can finish this scope, never the enclosing return.
            if(!dormant_enabled)source_begin(pc);
            if(dormant_enabled && pc==0x15fcc0) {
                if(!camera_original(pc,s))camera_route_abort(CameraFatalReason::NestedMethod,pc);
                s.pad=arena::SkipOwned;return true;
            }
            if(camera_aggregate && !camera_method_active && (pc==0x15f9e0 || pc==0x15fba8)) {
                if(!camera_method(pc,s))camera_route_abort(CameraFatalReason::NestedMethod,pc);
                s.pad=arena::SkipOwned;
            }
            return true; // other nested operations stay in the original method
        }
        auto mutation=[&](const CameraBinding& held,coordination::DemandArbiter& arbiter) {
            coordination::NativeReceipt receipt;
            if(!(held==camera_binding))return receipt;
            // A native Release/Clear/migration may invalidate the original source
            // consumer. Serialization alone does not settle a committed command.
            // Until that exact source-consumer scope is proved, withhold the
            // destructive operation and fail-stop, never forward over live debt.
            if(camera_debt || (adapter && adapter->state().pending))camera_route_abort(CameraFatalReason::MutationDebt,pc);
            const bool camera_update=pc==0x15fcc0;
            const bool handler=pc==0x1366b0 || pc==0x136118 || pc==0x13a100;
            const auto kind=(pc==0x15d730 || pc==0x15d198 || pc==0x15d248)?
                coordination::CallerKind::Stock:coordination::CallerKind::Foreign;
            uint64_t caller_object=s.x[0];
            // Pinned handlers and connection loss dereference the connection
            // reference only under this FIFO ticket, never at listener ingress.
            if(handler && !get(s.x[1],caller_object))return receipt;
            const coordination::Caller caller{kind,caller_object,held.association_generation};
            CameraAggregateScope scope(*this,arbiter,caller);
            if(!camera_update) {
                // This is an arbitrated operation with an actual returned result,
                // not the old unknown foreign-thread observation path.
                backend.invalidate(Reason::External);owned=false;
            }
            const bool method=pc==0x15f9e0 || pc==0x15fba8;
            if(!method && (pc==0x15f938 || pc==0x15faf8)) {
                const bool on=pc==0x15f938;
                if(!arbiter.register_caller(caller) || !arbiter.set_level(caller,on,on))return receipt;
            }
            if(method?camera_method(pc,s):camera_original(pc,s)) {
                receipt.execution=coordination::Execution::Executed;receipt.value_valid=true;receipt.value=s.x[0];
                // Actual nested camera entries own the level changes. Outer
                // handlers still write their native response/request markers.
            }
            return receipt;
        };
        coordination::NativeReceipt result;
        if(tid()==owner_tid)result=camera_owner.direct(mutation);
        else {
            // submit retains its gate ticket BEFORE mutation can read app.camera.
            auto submitted=camera_owner.submit(camera_owner.epoch(),camera_binding.renderer_generation,mutation);
            if(!submitted.id)camera_route_abort(CameraFatalReason::SubmitRefused,pc);
            coordination::Result complete;
            const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(250);
            while(!camera_owner.take(submitted.id,complete)) {
                if(std::chrono::steady_clock::now()>=end)camera_route_abort(CameraFatalReason::CompletionTimeout,pc);
                std::this_thread::yield();
            }
            if(!complete.retention_released)camera_route_abort(CameraFatalReason::RetentionNotReleased,pc);
            result=complete.native;
        }
        if(result.execution!=coordination::Execution::Executed || !result.value_valid)camera_route_abort(CameraFatalReason::NativeResult,pc);
        s.pad=arena::SkipOwned;return true;
    }
    bool camera_owned_transition(bool acquiring) noexcept {
        transition_called=false;transition_result=0;
        bool accepted=false,borrowed_retirement=false;
        auto result=camera_owner.direct([&](const CameraBinding& held,coordination::DemandArbiter& arbiter) {
            coordination::Caller caller{coordination::CallerKind::Portal,1,held.association_generation};
            if(!source || !source_integrity() || !source->transition_eligible(held,
               acquiring?coordination::Action::Connect:coordination::Action::None) || !arbiter.register_caller(caller))return coordination::NativeReceipt{};
            if(dormant_enabled && acquiring) {
                // Identity/bootstrap reservation and the ONE existing native
                // Connect precede portal_on. No image or fake cold tuple is used.
                CameraAggregateScope scope(*this,arbiter,caller);
                arena::Saved r{};r.x[0]=held.camera;r.x[1]=2;
                // Enter the real hooked method, not its bypass: only its actual
                // entry supplies native SP/LR. The nested hook invokes the sole
                // complete bypass and finalizes the typed current result.
                const bool prior=camera_active;camera_active=true;current_method_ready=false;
                const bool called=ft_native_portal_call_saved(base+0x15f9e0,&r);camera_active=prior;
                if(!called)return coordination::NativeReceipt{};
                transition_called=true;transition_result=r.x[0];
                coordination::NativeReceipt actual;actual.execution=coordination::Execution::Executed;
                actual.value_valid=true;actual.value=r.x[0];
                if(!current_method_ready)return actual;
                portal_token=arbiter.portal_on(caller);if(!portal_token.id)return actual;
                auto receipt=arbiter.reconcile([&](const coordination::ActionPlan& plan){
                    if(plan.action!=coordination::Action::Connect)return coordination::NativeReceipt{};
                    auto r=source_receipt(held,plan,actual.value);
                    r.execution=actual.execution;r.value_valid=actual.value_valid;r.value=actual.value;return r;
                });
                accepted=arbiter.frame_admissible(portal_token);return receipt;
            }
            const bool borrowed_use=arbiter.snapshot().use==coordination::UseDisposition::AuthenticatedBorrowedUse;
            if(acquiring) {
                portal_token=arbiter.portal_on(caller);
                if(!portal_token.id)return coordination::NativeReceipt{};
            } else if(!arbiter.portal_off(portal_token))return coordination::NativeReceipt{};
            if(!acquiring && borrowed_use) {
                const auto snapshot=arbiter.snapshot();
                accepted=!snapshot.fault && !snapshot.committed_debts;
                borrowed_retirement=true;
                return coordination::NativeReceipt{}; // no native call or invented native return
            }
            CameraAggregateScope scope(*this,arbiter,caller);
            arena::Saved r{};r.x[0]=held.camera;r.x[1]=2;
            const uint64_t pc=acquiring?0x15f9e0:0x15fba8;
            coordination::NativeReceipt actual;
            if(!camera_method(pc,r))return actual;
            transition_called=true;transition_result=r.x[0];
            actual.execution=coordination::Execution::Executed;actual.value_valid=true;actual.value=r.x[0];

            const auto snapshot=arbiter.snapshot();
            // Portal Off retires that token, not another caller's producer need.
            accepted=!snapshot.fault && (acquiring?arbiter.frame_admissible(portal_token):!snapshot.committed_debts);
            return actual;
        });
        return accepted && (borrowed_retirement ||
            (result.execution==coordination::Execution::Executed && result.value_valid && result.value!=0));
    }
    void camera_commit_debt() noexcept {
        if(camera_debt || !adapter || !portal_token.id || adapter->state().tickets_used!=2)return;
        source_command.watermark=adapter->state().tickets_issued;
        source_command.owner=command_owner;
        render_debt.emplace(*adapter,adapter->state().tickets_issued,SourceSettlement{this,camera_source_settled});
        camera_owner.direct([&](const auto&,coordination::DemandArbiter& arbiter) {
            camera_debt=arbiter.commit(portal_token,render_debt->callback());return coordination::NativeReceipt{};
        });
        if(!camera_debt)std::abort();
        backend.camera_pending(true);
    }
    void camera_maintenance(bool close=false) noexcept {
        if(!camera_bound.load() || tid()!=owner_tid || camera_active || camera_closed)return;
        if(dormant_enabled && close && !camera_closing) {
            if(!dormant_admission.begin_close(dormant_version))return;
            // Retained callers keep their observation permission until the whole
            // original returns. Do not close their native gate underneath them.
            if(!dormant_admission.close_ready(dormant_version))return;
        }
        if(dormant_enabled && !dormant_engaged) {
            if(close && !camera_closing)camera_closing=camera_owner.begin_close();
            if(camera_closing && dormant_admission.close_ready(dormant_version)) {
                camera_closed=camera_owner.finish_close();
                if(camera_closed && !dormant_admission.finish_close(dormant_version))std::abort();
                std::lock_guard<std::mutex> lock(camera_join_mutex);camera_snapshot_locked();
            }
            return; // no owner arbiter/source reader was ever admitted
        }
        if(close && !camera_closing)camera_closing=camera_owner.begin_close();
        if(!camera_closing)camera_owner.pump(8);
        auto poll=[&](const CameraBinding&,coordination::DemandArbiter& arbiter) {
            if(camera_debt && arbiter.poll_settlement(camera_debt)) {camera_debt=0;render_debt.reset();backend.camera_pending(false);}
            return coordination::NativeReceipt{};
        };
        if(camera_closing)camera_owner.drain(poll);else camera_owner.direct(poll);
        if(camera_closing) {
            // A real upload can precede a refused first enqueue, before the
            // dispatch-time pair debt exists. Keep its closing barrier and
            // forbid Disconnect/cleanup until that native work is settled.
            if((dormant_enabled && current_source_failed) || (adapter && adapter->state().pending))return;
            camera_owner.drain([&](const CameraBinding& held,coordination::DemandArbiter& arbiter) {
                if(camera_debt || !source || !source_integrity() ||
                   !source->transition_eligible(held,coordination::Action::None))return coordination::NativeReceipt{};
                return arbiter.reconcile([&](const coordination::ActionPlan& plan) {
                    if(plan.action!=coordination::Action::Disconnect ||
                       !source->transition_eligible(held,plan.action))return coordination::NativeReceipt{};
                    arena::Saved r{};r.x[0]=held.camera;r.x[1]=2;
                    if(!camera_original(0x15fba8,r))return coordination::NativeReceipt{};
                    auto receipt=source_receipt(held,plan,r.x[0]);
                    receipt.execution=coordination::Execution::Executed;receipt.value_valid=true;receipt.value=r.x[0];
                    return receipt;
                });
            });
            if(dormant_enabled && !dormant_admission.close_ready(dormant_version))return;
            camera_closed=camera_owner.finish_close();
            if(dormant_enabled && camera_closed && !dormant_admission.finish_close(dormant_version))std::abort();
            {std::lock_guard<std::mutex> lock(camera_join_mutex);camera_snapshot_locked();}
        }
    }
