// SPDX-License-Identifier: MIT
// Desktop contract 8a63fbdc. All native execution and memory changes modeled.
static void bootstrap_factory_observe(Chain &f) {
    const auto F=addr(f.stack)+0x100,W=addr(f.command),P=addr(f.bootstrap_pool),C=addr(f.executable);
    const auto before=native_reads.load();arena::Saved s{};s.sp=F;s.x[0]=addr(f.r)+0x5000;
    s.x[1]=Chain::base+0x478270;s.x[8]=F+0x98;s.x[19]=addr(f.r);s.x[30]=Chain::base+0x29a51c;
    if(ancestry_case>=0)s=relocate_arguments(factory_ancestry_vectors[ancestry_case][0],f,F,P,W,addr(f.r)+0x5000);
    const auto root=s;
    auto emit=[&](uint64_t pc) {const auto copy=s;f.router.event(pc,s);assert(!std::memcmp(&copy,&s,sizeof s));};
    emit(0x3053f8);s.sp-=0x30;s.x[0]=W;s.x[1]=root.x[0];s.x[2]=root.x[1];s.x[30]=Chain::base+0x30548c;
    if(ancestry_case>=0)s=relocate_arguments(factory_ancestry_vectors[ancestry_case][1],f,F-0x30,P,W,root.x[0]);
    emit(0x305308);
    s.sp-=0x20;s.x[0]=P;s.x[1]=root.x[1];s.x[8]=W+8;s.x[30]=Chain::base+0x305330;
    if(ancestry_case>=0)s=relocate_arguments(factory_ancestry_vectors[ancestry_case][2],f,F-0x50,P,W,root.x[0]);
    emit(0x302790);
    s.sp-=0x30;s.x[30]=Chain::base+0x3027b0;emit(0x304e60);
    s.sp-=0x60;s.x[19]=P;s.x[21]=W+8;s.x[22]=C;emit(0x304ec8);emit(0x304ecc);
    s.sp=F-0x30;s.x[30]=Chain::base+0x30548c;emit(0x3053a8);
    s.x[20]=F+0x98;s.x[22]=W;emit(0x305490);
    s=root;s.x[0]=F+0x98;emit(0x305470);
    assert(native_reads.load()==before);
    if(ancestry_case>=0) {
        s=relocate_arguments(factory_ancestry_vectors[ancestry_case][3],f,F,P,W,root.x[0]);
        // Desktop stopped before STR 29a528. Its caller has cleared the result
        // slot; x0 and the renderer field still name the old wrapper.
        put(f.stack,0x198,uint64_t(0));put(f.r,0x1380,s.x[0]);
        assert(s.x[0]!=W && s.x[1]==W && s.x[19]==addr(f.r));
    }
    f.s=s; // actual initializer publication follows
}
static void shared_factory_sequence(Chain &f,const std::string &mode) {
    const uint64_t P=addr(f.r)+0x11e0,C=addr(f.other),W=addr(f.bootstrap_wrapper);
    const uint64_t F=addr(f.outer_stack)+0x1000,A=addr(f.r)+0x5000;
    put(f.other,8,addr(f.r)+0x50);put(f.other,0x50,P);put(f.other,0x738,uint64_t(0xc001));
    f.begin_command(C,0,P);
    const auto before=native_reads.load();
    auto emit=[&](uint64_t pc,arena::Saved s) {
        if(argument_vector && pc==0x302790)s=relocate_arguments(*argument_vector,f,s.sp,P,W,A);
        if((mode=="shared-bad-pre-ret" && pc==0x3053a8) ||
           (mode=="shared-bad-interrupted" && pc==0x304ecc) ||
           (mode=="shared-bad-cleanup-call" && pc==0x311648))return;
        if(mode=="shared-bad-sp" && pc==0x304e60)++s.sp;
        if(mode=="shared-bad-lr" && pc==0x3053a8)++s.x[30];
        if(mode=="shared-bad-slot" && pc==0x302790)++s.x[8];
        if(mode=="shared-bad-name" && pc==0x302790)++s.x[1];
        if(mode=="shared-bad-pool" && pc==0x304ecc)++s.x[19];
        if(mode=="shared-bad-subject" && pc==0x305308)s.x[0]=addr(f.command);
        if(mode=="shared-bad-current-pop" && pc==0x304ec8)s.x[22]=addr(f.executable);
        if(mode=="shared-bad-cleanup-current" && pc==0x3052e0)s.x[1]=addr(f.executable);
        if(mode=="shared-bad-unwind" && pc==0x311648)s.x[30]=Chain::base+0x302808;
        if(mode=="shared-bad-return-pool" && pc==0x311648)++s.x[0];
        if(mode=="shared-bad-current-return" && pc==0x311648)s.x[1]=addr(f.executable);
        if(mode=="shared-bad-cleanup-result" && pc==0x3052f0)s.x[0]=0;
        s.pad=0x55;s.nzcv=0xa0000000;const auto original=s;f.router.event(pc,s);
        assert(!std::memcmp(&original,&s,sizeof s));
    };
    arena::Saved s{};s.sp=F;
    const bool cleanup=mode.find("cleanup")!=std::string::npos;
    const bool null=mode.find("null")!=std::string::npos;
    const bool known=cleanup && !null && mode!="shared-cleanup-unknown";
    if(mode.rfind("shared-bad-free-",0)==0) {
        s.x[0]=mode=="shared-bad-free-r"?addr(f.r):mode=="shared-bad-free-w"?addr(f.command):addr(f.executable);
        emit(0x4b3d0,s);s={};s.sp=F;
    }
    if(cleanup) {
        s.x[0]=W;s.x[30]=Chain::base+0x2953b4;emit(0x305260,s);
        s.sp-=0x30;s.x[19]=W;s.x[1]=null?0:(known?C:W+0x10000);emit(0x3052e0,s);
        if(!null) {
            s.x[0]=P+0xd0;emit(0x3052ec,s);
            s.x[30]=Chain::base+0x3052f0;emit(0x311648,s);
        }
        s.x[0]=null?0xdead:1;emit(0x3052f0,s);
        s.sp=F;s.x[0]=W;s.x[30]=Chain::base+0x2953c8;emit(0x4b3d0,s);
    } else {
        s.x[0]=A;s.x[1]=Chain::base+0x478270;s.x[8]=F+0x98;
        s.x[19]=mode=="shared-unselected"?addr(f.r)+0x8000:addr(f.r);
        s.x[30]=Chain::base+(mode=="shared-unselected"?0x2a23e8:0x29a51c);
        const auto root=s;emit(0x3053f8,s);
        if(mode!="shared-factory-null") {
            s.sp-=0x30;s.x[0]=W;s.x[1]=A;s.x[2]=root.x[1];s.x[30]=Chain::base+0x30548c;emit(0x305308,s);
            s.sp-=0x20;s.x[0]=P;s.x[1]=root.x[1];s.x[8]=W+8;s.x[30]=Chain::base+0x305330;emit(0x302790,s);
            s.sp-=0x30;s.x[30]=Chain::base+0x3027b0;emit(0x304e60,s);
            s.sp-=0x60;s.x[19]=P;s.x[21]=W+8;s.x[22]=C;
            if(!null)emit(0x304ec8,s);
            emit(0x304ecc,s);
            s.sp=F-0x30;s.x[30]=Chain::base+0x30548c;emit(0x3053a8,s);
            s.x[20]=F+0x98;s.x[22]=W;emit(0x305490,s);
        }
        s=root;s.x[0]=F+0x98;emit(0x305470,s);
        if(mode=="shared-unselected") {
            s.x[0]=W;emit(0x2a23f8,s); // different renderer, never selected publication
        }
        if(mode=="shared-bad-publication") {
            s.x[0]=W;s.x[19]=addr(f.r);emit(0x2a23f8,s);
        }
        // A later caller event, not pre-RET intent, retires the copied receipt.
        s={};s.sp=F;s.x[0]=P+0xd0;s.x[1]=C;s.x[30]=Chain::base+FT_STOCK_POOL_LR;
        emit(0x311648,s);
        if(mode=="shared-bad-duplicate")emit(0x311648,s);
    }
    if(cleanup && !known) {
        s={};s.sp=F;s.x[0]=P+0xd0;s.x[1]=C;s.x[30]=Chain::base+FT_STOCK_POOL_LR;emit(0x311648,s);
    }
    if(mode=="shared-cleanup-known") {
        const auto next=addr(f.bootstrap_command);
        put(f.bootstrap_command,8,addr(f.r)+0x50);put(f.bootstrap_command,0x50,P);
        put(f.bootstrap_command,0x738,uint64_t(0xc001));f.begin_command(next,0,P);
        s={};s.sp=F;s.x[0]=P+0xd0;s.x[1]=next;s.x[30]=Chain::base+FT_STOCK_POOL_LR;emit(0x311648,s);
        s={};s.x[0]=next;emit(0x4b3d0,s);
    }
    assert(native_reads.load()==before);
    // A returned member remains tracked until this real lifetime boundary.
    s={};s.x[0]=C;emit(0x4b3d0,s);
    std::fprintf(stderr,"SHARED mode=%s reads=0 saved-unchanged=1 returned=1\n",mode.c_str());
}
static int shared_factory_probe(const std::string &mode) {
    auto f=std::make_unique<Chain>();active=f.get();f->bootstrap_router();
    shared_factory_sequence(*f,mode);assert(!f->enqueues && !f->router.diagnostics().pending);
    f.reset();active=nullptr;return 0;
}
