// SPDX-License-Identifier: MIT
// Included inside Runtime::Impl after runtime_camera.inc. The observer is born
// on the authenticated owner thread and outlives native calls, never per-call.
    std::optional<ClientSourceBinding> source_binding;
    bool source_observing=false;
    uint64_t source_method=0;
    std::atomic<bool> source_foreign{false};
    bool source_integrity() const noexcept {
        uint64_t comp=0,client=0;
        return !source_foreign.load() && (!source_binding || !source_binding->failed()) &&
            ft_native_portal_source_modules && ft_native_portal_source_modules(&comp,&client) && comp==base && client;
    }
    coordination::NativeReceipt source_receipt(const CameraBinding& held,const coordination::ActionPlan& plan,uint64_t actual) noexcept {
        if(dormant_enabled) {
            if(!source || !source_integrity() || !current_method_ready)return {};
            return source->transition(held,plan,actual);
        }
        if(!source || !source_binding || !source_integrity())return {};
        const auto observed=source_binding->snapshot();
        if(!observed.valid || observed.actual!=actual || observed.action!=plan.action)return {};
        return source->transition(held,plan,actual);
    }
    bool source_begin(uint64_t pc) noexcept {
        if(pc!=0x15f9e0 && pc!=0x15fba8 && pc!=0x15fcc0)return false;
        if(source_observing)return false;
        uint64_t comp=0,client=0;
        if(!ft_native_portal_source_modules || !ft_native_portal_source_modules(&comp,&client) || comp!=base)return false;
        bool began=false;
        camera_owner.observe_scope([&](CameraAccessGate& gate,uint64_t ticket,const CameraBinding& held) {
            if(!(held==camera_binding))return;
            if(!source_binding)source_binding.emplace(gate);
            if(source_foreign.load())return;
            const auto action=pc==0x15fcc0?coordination::Action::None:
                pc==0x15f9e0?coordination::Action::Connect:coordination::Action::Disconnect;
            began=source_binding->begin(ticket,held,action,comp,client,
                ClientSourceBinding::compositor_sha,ClientSourceBinding::client_sha);
            if(began && source)source->before(held,action);
        });
        source_observing=began;if(began)source_method=pc;return began;
    }
    void source_finish(uint64_t actual,bool returned=true) noexcept {
        if(!source_observing)return;
        if(!returned || source_foreign.load())source_binding->cancel();
        source_binding->finish(actual);
        if(source)source->after(camera_binding,source_binding->snapshot());
        source_observing=false;source_method=0;
    }
    bool source_route(uint64_t pc,const arena::Saved& s) noexcept {
        constexpr uint64_t client_tag=UINT64_C(1)<<63;
        const bool client=(pc&client_tag)!=0;const uint64_t offset=pc&~client_tag;
        bool observation=client;
        bool ending=false,field=false,boundary=false;
        if(client) {
            field=offset==0x32a628;
            switch(offset) {
            case 0x3280e0:case 0x32766c:case 0x328148:case 0x3276d0:
            case 0x3280ec:case 0x327678:case 0x328150:case 0x3276d8:
            case 0x258438:case 0x2584c4:case 0x2584f8:case 0x258508:boundary=true;break;
            default:break;
            }
        }
        if(!client)switch(pc) {
        case 0x15f9fc:case 0x15fa60:case 0x15fa94:case 0x15fbc4:case 0x15fd30:
        case 0x15fa04:case 0x15fa8c:case 0x15fabc:case 0x15fbc0:case 0x15fc4c:
        case 0x15fd48:case 0x15fd90:case 0x15fbd0:case 0x15fc08:case 0x16013c:
            observation=field=true;break;
        case 0x15fee4:case 0x15fee8:case 0x160164:case 0x160168:case 0x160184:case 0x160188:
        case 0x160380:case 0x160384:case 0x1605c4:case 0x1605c8:
        case 0x15fa24:case 0x15fa28:case 0x15fa44:case 0x15fa48:case 0x15fae8:case 0x15faec:
        case 0x15fbec:case 0x15fbf0:case 0x15fc34:case 0x15fc38:observation=true;break;
        case 0x15fdc8:case 0x15fa78:case 0x15faa4:case 0x15fbd8:case 0x15fc20:observation=true;ending=true;break;
        default:break;
        }
        if(!observation)return false;
        const auto module=client?ClientSourceBinding::Module::Client:ClientSourceBinding::Module::Compositor;
        if(auto* scope=dormant_scope()) {
            if(!scope->method)dormant_history->invalidate();
            else if(ending)scope->method->camera_return(offset,s);
            else if(field)scope->method->camera_fact(module,offset,s);
            else if(boundary)scope->method->boundary_event(module,offset,s);
            else scope->method->event(module,offset,s);
            return true;
        }
        // Global hooks also see unrelated native calls. Never borrow a new ticket
        // on bootstrap/foreign ingress and never read owner optionals off-thread.
        if(tid()!=owner_tid) {if(camera_bound.load())source_foreign.store(true);return true;}
        if(dormant_enabled && dormant_engaged) {
            if(current_method)current_method->route(current_method->collector,
                ending?SourceRoute::Return:field?SourceRoute::Field:boundary?SourceRoute::Boundary:SourceRoute::Event,module,offset,s);
            else if(!camera_cleanup_active)source_foreign.store(true);
            return true;
        }
        if(field || boundary)return true; // legacy engine has no such PC handler
        if(!source_observing)return true;
        if(ending) {
            const uint64_t expected=pc==0x15fdc8?0x15fcc0:(pc==0x15fa78 || pc==0x15faa4)?0x15f9e0:0x15fba8;
            source_finish(s.x[0],source_method==expected);return true;
        }
        if(source_binding->event(module,offset,s) && source)source->observe(camera_binding,module,offset,s);
        return true;
    }
