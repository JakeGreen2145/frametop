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
        bool ending=false;
        if(!client)switch(pc) {
        case 0x15fee4:case 0x15fee8:case 0x160164:case 0x160168:case 0x160184:case 0x160188:
        case 0x160380:case 0x160384:case 0x1605c4:case 0x1605c8:
        case 0x15fa24:case 0x15fa28:case 0x15fa44:case 0x15fa48:case 0x15fae8:case 0x15faec:
        case 0x15fbec:case 0x15fbf0:case 0x15fc34:case 0x15fc38:observation=true;break;
        case 0x15fdc8:case 0x15fa78:case 0x15faa4:case 0x15fbd8:case 0x15fc20:observation=true;ending=true;break;
        default:break;
        }
        if(!observation)return false;
        // Global hooks also see unrelated native calls. Never borrow a new ticket
        // on bootstrap/foreign ingress and never read owner optionals off-thread.
        if(tid()!=owner_tid) {if(camera_bound.load())source_foreign.store(true);return true;}
        if(!source_observing)return true;
        if(ending) {
            const uint64_t expected=pc==0x15fdc8?0x15fcc0:(pc==0x15fa78 || pc==0x15faa4)?0x15f9e0:0x15fba8;
            source_finish(s.x[0],source_method==expected);return true;
        }
        const auto module=client?ClientSourceBinding::Module::Client:ClientSourceBinding::Module::Compositor;
        if(source_binding->event(module,offset,s) && source)source->observe(camera_binding,module,offset,s);
        return true;
    }
