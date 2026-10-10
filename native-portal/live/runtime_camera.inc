// SPDX-License-Identifier: MIT
// Included inside Runtime::Impl. All original method calls use loader-generated
// bypasses. No owner queue lock is held over native execution or listener waits.
    CameraOwner camera_owner;
    NativeSourceAdmission native_source{this,source_authenticate};
    SourceAdmission* source=nullptr;
    std::atomic<bool> camera_bound{false};
    uint64_t published_camera=0,published_application=0;
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
        const bool prior=camera_active;camera_active=true;
        const bool source_call=source_begin(pc);
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
           !same(now.identity,borrow.provenance.identity) || !self.source->borrow_ready(held,borrow))return false;
        c.tickets[eye]=borrow.provenance.ticket;c.watermark=borrow.watermark;c.owner=now;
        return true;
    }
    bool camera_ready() noexcept {
        CameraBinding held;
        const auto demand_state=camera_owner.snapshot();
        const uint64_t installed=ft_native_portal_camera_coverage?ft_native_portal_camera_coverage():0;
        return (installed&0xfffff)==0xfffff && camera_bound.load() && !camera_closing && !demand_state.fault && !demand_state.stock_full_display &&
            camera_owner.retained_binding(held) && held==camera_binding && source && source_integrity() && source->connect_eligible(held);
    }
    static bool camera_source_settled(void* p,uint64_t watermark) noexcept {
        auto& self=*static_cast<Impl*>(p);CameraBinding held;
        return self.source && self.source_integrity() && self.camera_owner.retained_binding(held) && held==self.camera_binding &&
            self.source_command_settled(watermark) && self.source->settled(held,watermark);
    }
    void camera_handoff() noexcept {
        if(!published_camera || !published_application || camera_bound.load())return;
        const auto app=evidence.application_owner();const auto id=evidence.identity();
        if(app.application!=published_application || app.worker!=worker || app.context!=context)return;
        CameraBinding b{context,published_application,published_camera,id.renderer,id.device,
            app.scheduler,id.renderer_generation,id.device_generation,id.renderer_generation};
        if(!camera_owner.bind_handoff(b,id.renderer_generation))return;
        camera_binding=b;camera=published_camera;camera_bound.store(true);
    }
    [[noreturn]] void camera_route_abort(CameraFatalReason reason,uint64_t pc) noexcept {
        // Foreign callers must not sample owner-only state. These atomics are
        // lock-free on supported builds; bootstrap_tid is immutable. Separate
        // loads are failure coordinates, not an authoritative state snapshot.
        static_assert(std::atomic<uint64_t>::is_always_lock_free);
        static_assert(std::atomic<bool>::is_always_lock_free);
        abort_camera_failure({reason,pc,tid(),owner_tid.load(std::memory_order_relaxed),bootstrap_tid,
            camera_bound.load(std::memory_order_relaxed),handoff_wait.load(std::memory_order_relaxed),
            stopped.load(std::memory_order_relaxed),joined.load(std::memory_order_relaxed)});
    }
    bool camera_route(uint64_t pc,arena::Saved& s) noexcept {
        if(pc==0x15fbb8 || pc==0x15fb20) {
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
            if(tid()!=bootstrap_tid || handoff_wait || camera_bound.load() ||
               !camera_owner.observe_publication(pc,s))camera_route_abort(CameraFatalReason::Publication,pc);
            published_camera=s.x[26];published_application=s.x[19];return true;
        }
        if(cleanup_pc(pc)) {
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
            if(result==binding::CameraNativeBinding::Dispatch::Refused)camera_route_abort(CameraFatalReason::CleanupRefused,pc);
            return true;
        }
        if(!writer_pc(pc))return false;
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
            source_begin(pc);
            if(camera_aggregate && !camera_method_active && (pc==0x15f9e0 || pc==0x15fba8)) {
                if(!camera_method(pc,s))camera_route_abort(CameraFatalReason::NestedMethod,pc);
                s.pad=arena::SkipOwned;
            }
            return true; // other nested operations stay in the original method
        }
        // Before completed publication/binding only bootstrap initialization is
        // native stock. Listener entry before Run is unsupported, not unretained.
        if(!camera_bound.load()) {
            if(tid()!=bootstrap_tid || handoff_wait)camera_route_abort(CameraFatalReason::WriterBeforeHandoff,pc);
            return true;
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
            if(adapter && adapter->state().pending)return;
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
            camera_closed=camera_owner.finish_close();
        }
    }
