// SPDX-License-Identifier: MIT
// Real self-process storage; native call effects below are explicitly MODELED.
struct StagingFixture : AdapterFixture {
    std::array<std::byte,0x1000> camera{};
    std::array<std::byte,0x20> stock_wrapper{};
    std::array<std::byte,0x128> stock_resource{};
    std::array<std::byte,0x460> caller{};
    std::array<std::byte,1280> uploaded{};
    unsigned descriptors=0,enqueues=0,fault=0,writes=0;
    uint64_t last_destination=0;
    render::Adapter *adapter=nullptr;
    inline static uint64_t now=10;
    static uint64_t clock() {return now;}
    static bool read(void *p,uint64_t a,void *out,size_t n) noexcept {
        auto m=static_cast<StagingFixture*>(p)->evidence.bridge_memory();return m.read(m.user,a,out,n);
    }
    static bool coherent(void *p,uint64_t a) noexcept {
        return static_cast<StagingFixture*>(p)->evidence.coherent(a);
    }
    static bool write(void *p,uint64_t a,const void *in,size_t n) noexcept {
        auto &f=*static_cast<StagingFixture*>(p);auto m=f.evidence.bridge_memory();f.last_destination=a;
        if(f.fault==2)++a;
        if(f.fault==3)--n;
        if(f.fault==4)++n;
        if(f.fault==5)a=UINT64_MAX-16;
        if(f.fault==6)in=nullptr;
        if(f.fault==9) {
            bool accepted=true;std::thread other([&]{accepted=m.write(m.user,a,in,n);});other.join();
            assert(!accepted);return m.write(m.user,a,in,n);
        }
        if(f.fault==10) {
            const auto child=fork();assert(child>=0);
            if(!child)_exit(m.write(m.user,a,in,n)?1:0);
            int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
        }
        const bool ok=m.write(m.user,a,in,n);if(ok)++f.writes;
        if(f.fault==7)f.adapter->off();
        if(f.fault==8)now=100;
        // Replay after the single attempt, even before Bridge returns, refuses.
        assert(!m.write(m.user,a,in,n));return ok;
    }
    std::array<render::Recorded,2> records{};
    static bool stock(void *,const render::Binding &,uint64_t a) noexcept {return a==0xabc;}
    static bool call(void *u,render::Registers &r) noexcept {
        auto &f=*static_cast<StagingFixture*>(u);
        if(r.target==base+0x2988a8) {
            uint32_t offset=0;std::memcpy(&offset,f.wrapper.data()+0x18,4);
            render::Descriptor d{0x999,offset,592};
            std::memcpy(reinterpret_cast<void*>(r.x[1]),&d,sizeof d);++f.descriptors;return true;
        }
        if(r.target==base+0x29b278) {
            auto &out=f.records[f.enqueues++];
            std::memcpy(out.prefix.data(),reinterpret_cast<void*>(r.x[1]),352);
            std::memcpy(out.label.data(),reinterpret_cast<void*>(r.x[2]),16);out.label_size=16;return true;
        }
        return AdapterFixture::call(u,r);
    }
    render::EyeInput input(unsigned eye) {
        render::EyeInput in{};in.eye=eye;in.observed_origin=4;
        in.raw_to_standing.v={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        const std::array<float,16> projection{1,0,0,0,0,1,0,0,0,0,-1,-.1f,0,0,-1,0};
        std::memcpy(camera.data()+0x100,identity.data(),64);
        std::memcpy(camera.data()+0x140,projection.data(),64);
        put(camera,8,addr(r));put(camera,0x2d0,addr(stock_wrapper));
        put(stock_wrapper,8,addr(stock_resource));put(stock_resource,8,addr(r)+0x50);
        put(stock_resource,0xb8,uint32_t(528));
        put(caller,0x308,uint64_t(0xabc));put(caller,0x320,uint32_t(57));put(caller,0x330,uint32_t(125));
        return in;
    }
};
static void staging_self_process(unsigned fault=0) {
    if(!expected_device_generation)return;
    StagingFixture f;f.fault=fault;StagingFixture::now=10;
    arena::Bridge bridge({&f,StagingFixture::read,StagingFixture::write,StagingFixture::coherent});assert(f.bootstrap(true,&bridge));
    Backend backend;FrameGate gate(backend);BackendStatus status;
    status.capable=status.serialized=status.external_clear=true;status.origin=4;assert(backend.publish(status));
    Lease lease;lease.epoch=8;lease.origin=4;lease.generation=2;lease.deadline_ns=100;lease.flags=1;
    lease.geometry.center[1]=-2.5f;assert(backend.publish_lease(lease));
    render::Binding b{};b.identity=f.evidence.identity();b.upstream=&bridge;
    b.targets={Fixture::base+0x296c00,Fixture::base+0x2993f8,Fixture::base+0x2988a8,
        Fixture::base+0x29b278,Fixture::base+0x2966a0,Fixture::base+0x294358};
    render::Callbacks cb{};cb.user=&f;cb.validate=AdapterFixture::validate;cb.call=StagingFixture::call;
    cb.observe=AdapterFixture::observe;cb.stock_blend=StagingFixture::stock;cb.pipeline=AdapterFixture::pipeline;cb.clock=StagingFixture::clock;
    std::optional<render::Adapter> holder;auto &adapter=holder.emplace(backend,gate,b,cb,2);
    f.adapter=&adapter;assert(adapter.initialize());
    PolicyInput policy;policy.enabled=true;assert(adapter.latch(gate.begin(1,policy,true,true,false,10)));
    auto mem=f.evidence.bridge_memory();std::array<std::byte,592> arbitrary{};
    assert(!mem.write(mem.user,addr(arbitrary),arbitrary.data(),592));
    uint64_t previous=0;
    for(unsigned index=0;index<2;++index) {
        const unsigned eye=fault==29?1-index:index;
        auto input=f.input(eye);arena::Saved s{};s.sp=addr(f.caller);s.x[27]=addr(f.camera);
        s.x[0]=addr(f.r);s.x[1]=addr(f.stock_wrapper);s.x[2]=addr(f.camera)+0xc0;s.x[7]=Fixture::base+0x2993f8;
        if(fault==23)++s.x[7];
        if(fault==24)++s.x[2];
        if(fault==25)s.x[27]=UINT64_MAX-0x100;
        if(fault==26)s.x[1]=UINT64_MAX-4;
        if(fault==27)s.sp=0;
        if(fault==28)put(f.stock_resource,0xb8,uint32_t(592));
        const auto ticket=f.evidence.stage(adapter,0x161d64,s,input);
        if((fault>=2 && fault<=9) || (fault>=23 && fault<=28)) {
            assert(!ticket && s.pad==arena::SkipOwned && s.x[30]==Fixture::base+0x161d68);
            assert(adapter.state().frame_invalid && !f.descriptors && !f.enqueues);
            assert(adapter.state().pending==(fault<=9));assert(!bridge.policy().actual());
            assert(!mem.write(mem.user,f.last_destination,arbitrary.data(),592));
            for(auto value:arbitrary)assert(value==std::byte{});
            return;
        }
        assert(ticket);assert(s.pad==arena::Replacement);
        if(fault==20 || fault==21) {
            if(fault==20)assert(!f.evidence.stage(adapter,0x161d64,s,input));
            else {
                std::thread other([&]{assert(!f.evidence.stage(adapter,0x161d68,s,input,ticket));});other.join();
                assert(!f.evidence.stage(adapter,0x161d68,s,input,ticket));
            }
            assert(adapter.state().pending && adapter.state().frame_invalid);return;
        }
        if(fault==22) {
            const auto child=fork();assert(child>=0);
            if(!child)_exit(f.evidence.stage(adapter,0x161d68,s,input,ticket)?1:0);
            int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
        }
        const auto source=s.x[2];assert(source!=addr(arbitrary));
        assert(!mem.write(mem.user,source,arbitrary.data(),592)); // callback window closed
        uint32_t cursor=index*640;
        arena::Saved admission{};admission.x[19]=addr(f.r);admission.x[0]=cursor;
        admission.x[20]=592;admission.x[21]=addr(f.wrapper)+0x18;admission.x[25]=source;
        admission.x[23]=0x808;admission.x[24]=8;
        adapter.upstream(admission,arena::Admission,b.identity.renderer_generation,static_cast<uint64_t>(syscall(SYS_gettid)));
        assert(admission.pad==arena::Replacement);
        // Model the native memcpy, cursor and wrapper writes. No guest mappings.
        assert(read_self(source,f.uploaded.data()+cursor,592));
        assert(!std::memcmp(f.uploaded.data()+cursor,f.camera.data()+0xc0,528));
        ft::portal::Draw draw;draw.raw_to_standing=input.raw_to_standing;
        draw.box.center={lease.geometry.center[0],lease.geometry.center[1],lease.geometry.center[2]};
        draw.box.size={lease.geometry.size[0],lease.geometry.size[1],lease.geometry.size[2]};
        for(unsigned i=0;i<4;++i)draw.box.quaternion[i]=lease.geometry.quaternion[i];
        draw.opacity=lease.geometry.opacity;draw.epoch=lease.epoch;draw.tracking_valid=true;
        for(unsigned i=0;i<16;++i) {
            float view=0,projection=0;std::memcpy(&view,f.camera.data()+0x100+4*i,4);
            std::memcpy(&projection,f.camera.data()+0x140+4*i,4);
            draw.raw_to_eye.v[i]=view;draw.projection.v[i]=projection;
        }
        ft::portal::NativeConstants native{};std::memcpy(native.data(),f.camera.data()+0xc0,528);
        const auto expected_constants=ft::portal::snapshot(native,ft::portal::compute(draw));
        assert(!std::memcmp(f.uploaded.data()+cursor,expected_constants.data(),592));
        put(f.wrapper,0x18,cursor);put(f.r,0x1418,uint32_t(cursor+640));
        if(fault==34) {
            assert(!f.evidence.stage(adapter,0x161f00,s,input,ticket));
            assert(s.pad==arena::Original && adapter.state().pending && adapter.state().frame_invalid);
            assert(!mem.write(mem.user,source,arbitrary.data(),592));
            assert(bridge.policy().actual()==640);return;
        }
        if(fault==33) {
            holder.reset();assert(!bridge.release(ticket));assert(backend.render_pending());
            assert(!mem.write(mem.user,source,arbitrary.data(),592));
            assert(bridge.policy().actual()==640);return;
        }
        if(fault==32) {
            uint64_t object=addr(f.stock_wrapper);arena::Saved destroy{};
            destroy.x[0]=addr(f.r);destroy.x[1]=reinterpret_cast<uint64_t>(&object);
            assert(f.evidence.event(0x2966a0,destroy));
            assert(!f.evidence.stage(adapter,0x161d68,s,input,ticket));
            assert(adapter.state().pending && bridge.policy().actual()==640 && !f.descriptors);return;
        }
        if(fault==30 && index) {
            assert(previous && previous!=ticket);
            assert(!f.evidence.stage(adapter,0x161d68,s,input,previous));
            assert(adapter.state().pending && bridge.policy().actual()==1280 && f.descriptors==2 && f.enqueues==1);return;
        }
        if(fault==1 || (fault>=11 && fault<=15)) {
            // Reused K or a late identity must not borrow the current invocation.
            if(fault==1)put(f.camera,0x2d0,addr(f.wrapper));
            if(fault==12)++s.sp;
            if(fault==13)++s.x[27];
            if(fault==14)put(f.stock_wrapper,8,addr(f.own));
            if(fault==15)++input.eye;
            assert(!f.evidence.stage(adapter,0x161d68,s,input,ticket+(fault==11)));
            assert(adapter.state().pending && adapter.state().frame_invalid);
            assert(bridge.policy().actual()==640 && !f.descriptors && !f.enqueues);return;
        }
        assert(f.evidence.stage(adapter,0x161d68,s,input,ticket));assert(f.descriptors==2*(index+1));
        if(fault==16) {
            assert(!f.evidence.stage(adapter,0x161d68,s,input,ticket));
            assert(adapter.state().pending && adapter.state().frame_invalid && bridge.policy().actual()==640);return;
        }
        s.x[0]=addr(f.r);s.x[1]=s.x[22]=s.sp+0x300;s.x[4]=Fixture::base+0x29b278;
        s.x[2]=Fixture::base+0x450650;
        if(fault==31) {
            ++s.x[2];assert(!f.evidence.stage(adapter,0x161ed8,s,input,ticket));
            assert(adapter.state().pending && !f.enqueues && bridge.policy().actual()==640);return;
        }
        if(fault>=17 && fault<=19) {
            if(fault==17)++s.x[1];
            if(fault==18)adapter.off();
            if(fault==19)StagingFixture::now=100;
            assert(!f.evidence.stage(adapter,0x161ed8,s,input,ticket));
            assert(adapter.state().pending && adapter.state().frame_invalid && bridge.policy().actual()==640);
            assert(!f.enqueues);return;
        }
        assert(f.evidence.stage(adapter,0x161ed8,s,input,ticket));assert(f.enqueues==index+1);
        assert(s.pad==arena::SkipOwned && s.x[30]==Fixture::base+0x161edc);
        auto expected=render::Prefix{};std::memcpy(expected.data(),f.caller.data()+0x300,352);
        put(expected,8,addr(f.marker));put(expected,0x38,addr(f.wrapper));put(expected,0x58,addr(f.wrapper));
        assert(expected==f.records[index].prefix);previous=ticket;
        assert(!mem.read(mem.user,addr(f.camera)+0xc0,arbitrary.data(),528));
        assert(!mem.write(mem.user,source,arbitrary.data(),592));
    }
    assert(adapter.outer_return());assert(adapter.state().pending && !adapter.state().committed);
    assert(bridge.policy().actual()==1280);
    render::Observation observation{};assert(f.evidence.snapshot(addr(f.wrapper),addr(f.marker),observation));
    auto reader=f.evidence.pass_reader();
    binding::PassRequest request{{b.identity,observation.arena.recording_generation},addr(f.marker),Fixture::base,0};
    binding::PassGate pass(adapter,reader,request);binding::PassRegisters regs{};regs.x[0]=addr(f.r);
    pass.callback(regs,0); // complete uploads still do not grant retained graph reads
    assert(!adapter.state().committed && !f.pipelines && backend.render_pending());
}
