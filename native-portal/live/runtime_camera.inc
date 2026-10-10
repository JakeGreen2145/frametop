// SPDX-License-Identifier: MIT
// Included inside Runtime::Impl. All original method calls use loader-generated
// bypasses. No owner queue lock is held over native execution or listener waits.
    CameraOwner camera_owner;
    SourceAdmission* source=nullptr;
    std::atomic<bool> camera_bound{false};
    uint64_t published_camera=0,published_application=0;
    CameraBinding camera_binding{};
    coordination::PortalToken portal_token{};
    std::optional<RenderDebt> render_debt;
    uint64_t camera_debt=0;
    bool camera_closing=false,camera_closed=false,camera_active=false;
    bool camera_cleanup_active=false;
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
        const bool ok=ft_native_portal_call_saved(target,&s);camera_active=prior;
        return ok;
    }
    bool camera_ready() noexcept {
        CameraBinding held;
        const auto demand_state=camera_owner.snapshot();
        return camera_bound.load() && !camera_closing && !demand_state.fault && !demand_state.stock_full_display &&
            camera_owner.retained_binding(held) && held==camera_binding && source && source->ready(held);
    }
    static bool camera_source_settled(void* p,uint64_t watermark) noexcept {
        auto& self=*static_cast<Impl*>(p);CameraBinding held;
        return self.source && self.camera_owner.retained_binding(held) && held==self.camera_binding &&
            self.source->settled(held,watermark);
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
    bool camera_route(uint64_t pc,arena::Saved& s) noexcept {
        if(pc==0x8dc1c) {
            if(tid()!=bootstrap_tid || handoff_wait || camera_bound.load() ||
               !camera_owner.observe_publication(pc,s))std::abort();
            published_camera=s.x[26];published_application=s.x[19];return true;
        }
        if(cleanup_pc(pc)) {
            // Nested resource cleanup is within the already quiescent outer
            // application cleanup. Do not reacquire or self-wait in that call.
            if((tid()==owner_tid || (tid()==bootstrap_tid && joined)) && camera_cleanup_active)return true;
            backend.invalidate(Reason::Off);
            if(tid()!=owner_tid && !(tid()==bootstrap_tid && joined))std::abort();
            if(camera_bound.load() && tid()==owner_tid)camera_maintenance(true);
            struct Cleanup {Impl* p;uint64_t pc;arena::Saved* saved;bool ok=false;} c{this,pc,&s};
            auto original=[](void* v) noexcept {
                auto& c=*static_cast<Cleanup*>(v);c.p->camera_cleanup_active=true;
                c.ok=c.p->camera_original(c.pc,*c.saved);c.p->camera_cleanup_active=false;
                if(!c.ok)std::abort();
            };
            const auto result=camera_owner.cleanup(pc,s,original,&c,std::chrono::milliseconds(0));
            if(result==binding::CameraNativeBinding::Dispatch::Refused)std::abort();
            return true;
        }
        if(!writer_pc(pc))return false;
        if(camera_bound.load()) {
            const bool app_entry=pc==0x6d6e0 || pc==0x6d6f0;
            const bool camera_entry=pc==0x15f9e0 || pc==0x15fba8 || pc==0x15f938 || pc==0x15faf8 || pc==0x15fcc0;
            const bool handler=pc==0x1366b0 || pc==0x136118 || pc==0x13a100;
            if((app_entry && s.x[0]!=camera_binding.application) ||
               (camera_entry && s.x[0]!=camera_binding.camera) ||
               (handler && s.x[0]!=camera_binding.context))std::abort();
        }
        if(tid()==owner_tid && camera_active)return true; // complete original nested path, existing ticket
        // Before completed publication/binding only bootstrap initialization is
        // native stock. Listener entry before Run is unsupported, not unretained.
        if(!camera_bound.load()) {
            if(tid()!=bootstrap_tid || handoff_wait)std::abort();
            return true;
        }
        auto mutation=[&](const CameraBinding& held,coordination::DemandArbiter& arbiter) {
            coordination::NativeReceipt receipt;
            if(!(held==camera_binding))return receipt;
            // A native Release/Clear/migration may invalidate the original source
            // consumer. Serialization alone does not settle a committed command.
            // Until that exact source-consumer scope is proved, withhold the
            // destructive operation and fail-stop, never forward over live debt.
            if(camera_debt || (adapter && adapter->state().pending))std::abort();
            // Full stock/foreign methods keep their actual return and side effects.
            // Their service ownership stays unadmitted. Do not replay them later
            // or let a portal release clear uncertain independent demand.
            const bool camera_update=pc==0x15fcc0;
            const bool handler=pc==0x1366b0 || pc==0x136118;
            const auto kind=(pc==0x15d730 || pc==0x15d198 || pc==0x15d248)?
                coordination::CallerKind::Stock:coordination::CallerKind::Foreign;
            uint64_t caller_object=s.x[0];
            // Pinned 1366c8/136130 dereference x1 as the connection reference.
            // Read it only here, AFTER the FIFO's ticket lookup, never at ingress.
            if(handler && !get(s.x[1],caller_object))return receipt;
            const coordination::Caller caller{kind,caller_object,held.association_generation};
            if(!camera_update) {
                backend.invalidate(Reason::External);if(demand)demand->external_observed();owned=false;
                if(arbiter.register_caller(caller))arbiter.set_level(caller,true,true);
            }
            if(camera_original(pc,s)) {
                receipt.execution=coordination::Execution::Executed;receipt.value_valid=true;receipt.value=s.x[0];
                if((pc==0x6d6f0 && uint8_t(s.x[0])) || pc==0x15d198 || pc==0x15d248)
                    arbiter.set_level(caller,false,false);
                if(handler && caller_object) {
                    uint8_t requested=1;
                    if(get(caller_object+0x86,requested))arbiter.set_level(caller,requested!=0,requested!=0);
                }
                // A handler's return=1 and a cleared request marker are logical
                // intent only. Neither changes the native service queue journal.
            }
            return receipt;
        };
        coordination::NativeReceipt result;
        if(tid()==owner_tid)result=camera_owner.direct(mutation);
        else {
            // submit retains its gate ticket BEFORE mutation can read app.camera.
            auto submitted=camera_owner.submit(camera_owner.epoch(),camera_binding.renderer_generation,mutation);
            if(!submitted.id)std::abort();
            coordination::Result complete;
            const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(250);
            while(!camera_owner.take(submitted.id,complete)) {
                if(std::chrono::steady_clock::now()>=end)std::abort();
                std::this_thread::yield();
            }
            if(!complete.retention_released)std::abort();
            result=complete.native;
        }
        if(result.execution!=coordination::Execution::Executed || !result.value_valid)std::abort();
        s.pad=arena::SkipOwned;return true;
    }
    bool camera_owned_transition(bool acquiring) noexcept {
        transition_called=false;transition_result=0;
        bool accepted=false;
        auto result=camera_owner.direct([&](const CameraBinding& held,coordination::DemandArbiter& arbiter) {
            coordination::Caller caller{coordination::CallerKind::Portal,1,held.association_generation};
            if(!source || !source->ready(held) || !arbiter.register_caller(caller))return coordination::NativeReceipt{};
            if(acquiring)portal_token=arbiter.portal_on(caller);
            else if(portal_token.id)arbiter.portal_off(portal_token);
            auto actual=arbiter.reconcile([&](const coordination::ActionPlan& plan) {
                if((plan.action==coordination::Action::Connect)!=acquiring)return coordination::NativeReceipt{};
                arena::Saved r{};r.x[0]=held.camera;r.x[1]=2;
                const uint64_t pc=acquiring?0x15f9e0:0x15fba8;
                if(!camera_original(pc,r))return coordination::NativeReceipt{};
                transition_called=true;transition_result=r.x[0];
                auto receipt=source->transition(held,plan,r.x[0]);
                // A provider may supply service facts, never rewrite native return.
                receipt.execution=coordination::Execution::Executed;receipt.value_valid=true;receipt.value=r.x[0];
                return receipt;
            });
            const auto snapshot=arbiter.snapshot();
            accepted=!snapshot.fault && (acquiring?arbiter.frame_admissible(portal_token):snapshot.native_idle);
            return actual;
        });
        return accepted && result.execution==coordination::Execution::Executed && result.value_valid && result.value!=0;
    }
    void camera_commit_debt() noexcept {
        if(camera_debt || !adapter || !portal_token.id || adapter->state().tickets_used!=2)return;
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
            camera_owner.drain([&](const CameraBinding& held,coordination::DemandArbiter& arbiter) {
                if(camera_debt || !source || !source->ready(held))return coordination::NativeReceipt{};
                return arbiter.reconcile([&](const coordination::ActionPlan& plan) {
                    if(plan.action!=coordination::Action::Disconnect)return coordination::NativeReceipt{};
                    arena::Saved r{};r.x[0]=held.camera;r.x[1]=2;
                    if(!camera_original(0x15fba8,r))return coordination::NativeReceipt{};
                    auto receipt=source->transition(held,plan,r.x[0]);
                    receipt.execution=coordination::Execution::Executed;receipt.value_valid=true;receipt.value=r.x[0];
                    return receipt;
                });
            });
            camera_closed=camera_owner.finish_close();
        }
    }
