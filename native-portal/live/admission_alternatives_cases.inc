// SPDX-License-Identifier: MIT
// Source models only. Production collectors/admission, not native A64 execution.
// Included after CurrentFixture in current_source_admission_test.cpp.
struct AlternativeFixture:CurrentFixture<SourceFixture<CurrentUpdateCollector>> {
    enum Fault {None, MissingReason, WrongLatest, WrongMode, WrongProvider, WrongId,
        WrongSp, WrongLr, MissingExit, ChangedMode, MissingGuard, FalseGuard,
        MissingGuardEnd, PropertyOnEmpty, MissingInnerReturn};
    void empty(unsigned q,int raw=3,bool bypass=false,Fault fault=None) {
        const uint64_t local=sp-0x360,body=local-0x50-0x70;
        const uint64_t object=0x90200000+q*0x1000,header=0x90300000+q*0x1000;
        const uint64_t start=q?0x15fee4:0x160184;
        arena::Saved s{};s.sp=local;s.x[19]=b.camera;s.x[0]=manager;s.x[1]=11+q;
        s.x[2]=local+0x90;s.x[3]=local+0x98;s.x[5]=client+0x3280a8;
        if(fault==WrongMode)s.x[4]=1;
        if(fault==WrongProvider)++s.x[0];
        event(M::Compositor,start,s);
        s.x[30]=start+4;if(fault==WrongLr)++s.x[30];
        event(M::Client,0x3280a8,s);
        s.sp=local-0x50;s.x[0]=bypass?0:1;s.x[19]=11+q;s.x[20]=manager;s.x[22]=manager+8;
        if(fault==WrongId)++s.x[19];
        guard(0x3280e0,s);
        if(!bypass){s.x[0]=0;s.x[30]=client+0x3280ec;guard(0x3280ec,s);}
        if(raw!=4) {
            s.x[19]=11+q;s.x[20]=manager+0x70;s.x[0]=object;s.x[1]=local+0x90;s.x[2]=local+0x98;s.x[3]=0;
            event(M::Client,0x32813c,s);s.x[30]=client+0x328140;event(M::Client,0x327e18,s);
            s.sp=body;s.x[20]=object;
            auto g=s;g.x[0]=body+0x58;g.x[30]=client+0x327e4c;guard(0x258438,g);
            g.sp=body-0x30;g.x[19]=body+0x58;g.x[0]=fault==FalseGuard?0:1;g.x[30]=client+0x2584c4;
            if(fault!=MissingGuard)guard(0x2584c4,g);
            // No header bytes or output slots exist in the fake backend.
            s.x[0]=header;event(M::Client,0x327e54,s);
            if(raw==3) {
                s.x[19]=fault==WrongLatest?0x7f:0x80;s.nzcv=fault==WrongLatest?0x80000000:0x60000000;
                if(fault==WrongSp)++s.sp;
                if(fault!=MissingReason)event(M::Client,0x327e60,s);
            }
            if(raw==7 || fault==PropertyOnEmpty) {
                s.x[1]=local+0x90;event(M::Client,0x327f2c,s);
                // Provider7 may mutate the output. No clean-no-effect inference.
                put(local+0x90,0xdeadbeef);s.x[0]=2;event(M::Client,0x327f30,s);
            }
            g.sp=body;g.x[0]=body+0x58;g.x[30]=client+(raw==7?0x327fa4:raw==5?0x327e84:0x327fd8);
            if(fault!=MissingGuardEnd)guard(0x258508,g);
            s.sp=local-0x50;s.x[0]=raw;
            if(fault!=MissingInnerReturn)event(M::Client,0x328140,s);
        }
        s.sp=local-0x50;s.x[21]=fault==ChangedMode?(bypass?1:0):(bypass?0:1);s.x[22]=manager+8;
        if(fault!=MissingExit)guard(0x328148,s);
        if(!bypass){s.x[0]=manager+8;guard(0x328150,s);}
        s.sp=local;s.x[19]=b.camera;s.x[0]=raw;event(M::Compositor,start+4,s);
    }
    template<class Body> bool update(Body body,bool retired=true,uint64_t actual=2) {
        CurrentUpdateCompletion completion(b,1,std::this_thread::get_id(),1);
        CurrentUpdateCollector c(gate,completion,{SourceFixture<CurrentUpdateCollector>::read,this,SourceFixture<CurrentUpdateCollector>::covered,SourceFixture<CurrentUpdateCollector>::provider_live});
        current=&c;
        assert(admission.begin_current_update(b,completion.invocation_id()));
        assert(begin(0x15fcc0));body();
        auto s=entry();s.x[0]=actual;
        assert(c.camera_return(0x15fdc8,s));assert(!completion.take());
        bool result=false;
        if(retired) {
            if(!c.complete_own_bypass(c.invocation(),actual)){current=&collector;return false;}
            auto r=completion.take();assert(r);
            assert(r->observation().actual==actual);
            result=admission.finish_current_update(b,*r);
        }
        current=&collector;return result;
    }
    void history() {Fixture h;h.connect();assert(admission.fold_stock_history(h.transport));warm();}
};
void legacy_no_grant_does_not_read_header() {
    Fixture f;f.warm();unsigned reads=0;
    struct Memory {Fixture* f;unsigned* reads;} memory{&f,&reads};
    ClientSourceBinding engine(f.gate,[](void* p,uint64_t a,void* out,size_t n) {
        auto& m=*static_cast<Memory*>(p);++*m.reads;
        for(size_t i=0;i<n;++i){auto it=m.f->memory.find(a+i);if(it==m.f->memory.end())return false;
            static_cast<unsigned char*>(out)[i]=it->second;}return true;
    },&memory);
    assert(engine.begin(f.ticket,f.b,A::None,0,Fixture::client,ClientSourceBinding::compositor_sha,ClientSourceBinding::client_sha));
    arena::Saved s{};s.sp=Fixture::sp-0x360;s.x[19]=f.b.camera;s.x[0]=Fixture::manager;s.x[1]=11;
    s.x[2]=s.sp+0x90;s.x[3]=s.sp+0x98;s.x[5]=Fixture::client+0x3280a8;
    assert(engine.event(M::Compositor,0x160184,s));s.x[30]=0x160188;assert(engine.event(M::Client,0x3280a8,s));
    s.sp-=0x50;s.x[19]=11;s.x[20]=Fixture::manager+0x70;s.x[0]=0x90200000;s.x[1]=s.x[2];s.x[2]=s.x[3];s.x[3]=0;
    assert(engine.event(M::Client,0x32813c,s));s.x[30]=Fixture::client+0x328140;assert(engine.event(M::Client,0x327e18,s));
    s.sp-=0x70;s.x[20]=0x90200000;s.x[0]=0x90300000;const auto before=reads;
    assert(engine.event(M::Client,0x327e54,s));assert(reads==before);
    s.sp+=0x70;s.x[0]=3;assert(engine.event(M::Client,0x328140,s));
    s.sp+=0x50;s.x[19]=f.b.camera;assert(engine.event(M::Compositor,0x160188,s));assert(reads==before);
    assert(engine.finish(2));assert(engine.snapshot().reads[0].disposition==ClientSourceBinding::AcquireDisposition::Unknown);
}
void admission_alternative_cases() {
    legacy_no_grant_does_not_read_header();
    for(bool bypass:{false,true}) {
        AlternativeFixture f;f.warm();const auto before=f.reads;
        assert(f.update([&]{f.empty(1,3,bypass);f.empty(0,3,bypass);}));
        assert(f.reads==before+4); // only two vtable identity reads per original call
        assert(f.admission.bootstrap_eligible(f.b).needs_connect);
        assert(f.update([&]{f.empty(1,3,bypass);f.metadata(0);f.metadata(0,true);}));
        assert(f.admission.bootstrap_eligible(f.b).needs_connect); // partial is not warm history
        assert(f.update([&]{for(unsigned q:{1u,0u}){f.metadata(q);f.metadata(q,true);}}));
        assert(!f.admission.bootstrap_eligible(f.b).needs_connect);
        f.retained=true;assert(f.select(f.borrow(),1));assert(f.select(f.borrow(1),2));
        assert(f.admission.settled(f.b,2));
    }
    {AlternativeFixture f;f.history();assert(!f.update([&]{f.empty(1);}));} // q1 failure must continue through q0
    {AlternativeFixture f;f.warm();assert(!f.update([&]{f.metadata(0);f.metadata(0,true);}));} // unproved first q0-only
    for(int raw:{4,5,7}) {
        AlternativeFixture f;f.history();assert(!f.update([&]{f.empty(1,raw);}));
        assert(!f.admission.bootstrap_eligible(f.b).available);
        assert(!f.admission.begin_current_update(f.b,UINT64_MAX));
        assert(!f.select(f.borrow(),1));
    }
    for(int i=AlternativeFixture::MissingReason;i<=AlternativeFixture::MissingInnerReturn;++i) {
        AlternativeFixture f;f.history();bool refused=false;
        try {refused=!f.update([&]{f.empty(1,3,true,static_cast<AlternativeFixture::Fault>(i));});}
        catch(const SourceFixture<CurrentUpdateCollector>::ScopeRejected&) {refused=true;}
        assert(refused);assert(!f.admission.bootstrap_eligible(f.b).available);
    }
    {AlternativeFixture f;f.history();
     assert(!f.update([&]{f.empty(1);f.empty(0);},false));
     assert(!f.admission.bootstrap_eligible(f.b).available);assert(!f.select(f.borrow(),1));}
    {AlternativeFixture f;f.history();
     assert(!f.update([&]{f.empty(1);f.metadata(0);})); // real grant still needs Return
     assert(!f.admission.bootstrap_eligible(f.b).available);}
    {AlternativeFixture f;f.history();assert(f.select(f.borrow(),1));
     assert(f.update([&]{f.empty(1);f.empty(0);}));
     assert(!f.admission.settled(f.b,2));assert(!f.admission.bootstrap_eligible(f.b).available);
     f.retained=true;assert(f.select(f.borrow(1),2));assert(f.admission.settled(f.b,2));}
    for(unsigned fault=1;fault<=17;++fault) {
        CurrentFixture<> f;f.connect();assert(f.admission.fold_stock_history(f.transport));
        f.retained=true;f.borrow_fault=fault;assert(!f.select(f.borrow(),1));
    }
    {CurrentFixture<> f;f.connect();assert(f.admission.fold_stock_history(f.transport));
     f.retained=true;f.revoke_at_selection_auth=true;assert(!f.select(f.borrow(),1));assert(!f.selection_callbacks);}
    for(float value:{-.1f,0.f,NAN}) {
        CurrentFixture<> f;f.connect();assert(f.admission.fold_stock_history(f.transport));
        auto entry=f.entry(),s=entry;assert(f.render.begin(f.ticket,f.b,1,0,s));
        s.sp-=0x460;s.x[27]=f.b.camera;s.x[7]=0;assert(f.render.capture(0x160ca0,s));
        double time=10;std::memcpy(&s.q[0],&time,8);s.nzcv=0x20000000;assert(f.render.capture(0x160db0,s));
        std::memcpy(&s.q[2],&value,4);assert(f.render.capture(0x160e34,s));
        assert(!f.render.sample());assert(f.render.capture(0x160dd4,entry));
        f.retained=true;assert(f.select(f.borrow(),2)); // actual no-use RET is neutral
    }
    for(unsigned bad=0;bad<9;++bad) {
        CurrentFixture<> f;f.connect();assert(f.admission.fold_stock_history(f.transport));f.retained=true;
        switch(bad) {
        case 0:f.ramp=0;break;case 1:f.ramp=-.1f;break;case 2:f.ramp=INFINITY;break;
        case 3:f.ramp=NAN;break;case 4:f.retained_time=0;break;case 5:f.retained_time=NAN;break;
        case 6:f.skip_join=true;break;case 7:f.opacity=0;break;case 8:f.opacity=NAN;break;
        }
        assert(!f.select(f.borrow(),1));
    }
    {CurrentFixture<> f;f.retained=true;assert(!f.select(f.borrow(),1));} // no history
    {CurrentFixture<> f;f.connect();assert(f.admission.fold_stock_history(f.transport));
     f.retained=true;f.clamped=true;f.ramp=2;assert(f.select(f.borrow(),1));}
    {CurrentFixture<> f;f.connect();assert(f.admission.fold_stock_history(f.transport));
     f.retained=true;f.ramp=2;assert(!f.select(f.borrow(),1));} // missing clamp
    std::puts("PASS admission alternatives: neutral completion, first-history partial, bypass, raw4/5/7, exact return, debt, ramp/opacity");
}
