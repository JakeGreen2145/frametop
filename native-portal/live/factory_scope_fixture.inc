// SPDX-License-Identifier: MIT
// Source-only schedules through the actual default Runtime. Desktop's partial
// pinned-text contract supplies direct LR297b70, P=R+11e0, caller-SP+80 output,
// and same-output return LR297cb4. Native bodies/order remain modeled here.
static int factory_scope_probe(const std::string& mode) {
    auto f=std::make_unique<Chain>();active=f.get();f->bootstrap_router();
    const auto pool=addr(f->r)+0x11e0;
    put(f->other,0x50,pool);put(f->other,0x738,uint64_t(0xc001));
    f->begin_command(addr(f->other),0,pool);
    auto* guard=mmap(nullptr,4096,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(guard!=MAP_FAILED);
    const auto before=native_reads.load();
    auto emit=[&](uint64_t pc,arena::Saved s) {
        // Preserve every Saved lane, not only the argument registers.
        s.pad=0x55;s.nzcv=0xa0000000;
        const auto original=s;f->router.event(pc,s);
        assert(!std::memcmp(&original,&s,sizeof s));
    };
    arena::Saved s{};s.sp=addr(f->outer_stack)+0x1000;
    s.x[0]=pool;s.x[1]=uint64_t(guard);s.x[8]=s.sp+0x80;
    s.x[30]=Chain::base+0x297b70;
    if(mode.rfind("factory-diag-",0)==0) {
        // Existing qualified selected-wrapper turnover, stopped before the
        // acquire callback. A malformed owned descendant must stay fatal.
        const auto sp=s.sp;s={};s.sp=sp+0xd0;s.x[0]=addr(f->r);s.x[30]=Chain::base+FT_BOOT_DISPATCH_LR;
        emit(0x2a1e18,s);s.sp=sp;s.x[19]=addr(f->r);s.x[1]=addr(f->command);emit(0x2a1e3c,s);
        emit(0x2a2230,s);emit(0x2a2320,s);
        s.x[0]=addr(f->command);s.x[30]=Chain::base+FT_BOOT_RELEASE_LR;emit(0x305260,s);
        s.sp-=0x30;s.x[19]=addr(f->command);s.x[1]=addr(f->executable);emit(0x3052e0,s);
        s.x[0]=addr(f->bootstrap_pool)+0xd0;emit(0x3052ec,s);
        s.x[30]=Chain::base+FT_BOOT_POOL_LR;emit(0x311648,s);
        s.x[0]=1;emit(0x3052f0,s);
        if(mode=="factory-diag-phase") {
            s={};s.sp=sp;s.x[0]=pool;s.x[1]=uint64_t(guard);s.x[8]=sp+0x80;s.x[30]=Chain::base+0x297b70;
        } else {
            s={};s.sp=sp;s.x[0]=addr(f->r);s.x[1]=uint64_t(guard);s.x[8]=sp+0x98;
            s.x[30]=Chain::base+0x2a23e8;emit(0x3053f8,s);
            s.sp-=0x30;s.x[0]=addr(f->bootstrap_wrapper);s.x[1]=addr(f->r);s.x[2]=uint64_t(guard);
            s.x[30]=Chain::base+0x30548c;emit(0x305308,s);
            s.sp-=0x20;s.x[0]=pool;s.x[1]=uint64_t(guard);s.x[8]=addr(f->bootstrap_wrapper)+8;
            s.x[30]=Chain::base+0x305330;
        }
        if(mode=="factory-diag-sp")++s.sp;
        if(mode=="factory-diag-caller")s.x[30]=Chain::base+0x297b70;
        if(mode=="factory-diag-slot")++s.x[8];
        if(mode=="factory-diag-name")++s.x[1];
        if(mode=="factory-diag-pool")s.x[0]=0;
    }
    if(mode!="factory-control") {
        uint64_t pc=0x302790;
        if(mode.rfind("factory-site-",0)==0)pc=std::stoull(mode.substr(13),nullptr,16);
        emit(pc,s);
        if(mode.rfind("factory-diag-",0)==0) {
            auto later=s;later.x[0]=0;later.x[8]=0;later.x[30]=Chain::base+0xfeed;
            emit(0x302790,later); // first receipt cannot be replaced by later facts
        }
        if(mode=="factory-direct-chain") {
            // Model the already-established hidden output register protocol.
            // Never read the name or output slot or infer wrapper publication.
            s.sp-=0x30;s.x[30]=Chain::base+0x3027b0;emit(0x304e60,s);
            s.sp-=0x60;s.x[19]=pool;s.x[21]=s.x[8];s.x[22]=addr(f->other);
            emit(0x304ec8,s);emit(0x304ecc,s);
        }
    }
    s={};s.sp=addr(f->outer_stack)+0x1000;s.x[0]=pool+0xd0;
    s.x[1]=addr(f->other);s.x[30]=Chain::base+FT_STOCK_POOL_LR;
    emit(0x311648,s); // existing proved caller, not a new LR allowance
    assert(native_reads.load()==before);
    assert(!f->router.diagnostics().frames && !f->router.diagnostics().pending && !f->enqueues);
    assert(!munmap(guard,4096));
    f.reset();active=nullptr; // flush real bounded diagnostic logger
    std::fprintf(stderr,"FACTORY mode=%s reads=0 saved-unchanged=1 no-work=1 returned=1\n",mode.c_str());
    return 0;
}
