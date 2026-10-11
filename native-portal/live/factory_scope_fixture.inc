// SPDX-License-Identifier: MIT
// Source-only schedules through the actual default Runtime. Desktop's complete
// contract supplies the root registers, pool, name and caller-local output slot.
// Native bodies/order and queue results remain modeled, not instruction execution.
static int factory_scope_probe(const std::string& mode,Chain *existing=nullptr) {
    auto owned=existing?std::unique_ptr<Chain>{}:std::make_unique<Chain>();
    auto *f=existing?existing:owned.get();active=f;if(!existing)f->bootstrap_router();
    const auto pool=addr(f->r)+0x11e0;
    put(f->other,0x50,pool);put(f->other,0x738,uint64_t(0xc001));
    f->begin_command(addr(f->other),0,pool);
    const bool selected=mode.rfind("factory-selected",0)==0;
    if(selected || mode=="factory-member-reset") {
        put(f->bootstrap_command,8,addr(f->r)+0x50);put(f->bootstrap_command,0x50,pool);
        put(f->bootstrap_command,0x738,uint64_t(0xc001));f->begin_command(addr(f->bootstrap_command),0,pool);
    }
    auto* guard=mmap(nullptr,4096,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(guard!=MAP_FAILED);
    const auto before=native_reads.load();
    auto emit=[&](uint64_t pc,arena::Saved s) {
        // Preserve every Saved lane, not only the argument registers.
        s.pad=0x55;s.nzcv=0xa0000000;
        const auto original=s;f->router.event(pc,s);
        assert(!std::memcmp(&original,&s,sizeof s));
    };
    arena::Saved s{};s.sp=addr(f->outer_stack)+0x1000;
    struct Root {uint64_t lr,slot,name,queue;unsigned renderer;};
    constexpr Root roots[]={{0x297878,0x50,0x477ae8,0x297928,20},
        {0x297b70,0x80,0x477c50,0x297c70,19},
        {0x29a464,0x88,0x4780c0,0x29a4f4,19},
        {0x29af28,0x80,0x478388,0x29b044,23},
        {0x29ca68,0xa8,0x4784a8,0x29cba0,24}};
    unsigned index=1;
    if(mode.rfind("factory-root-",0)==0)index=unsigned(mode.at(13)-'0');
    assert(index<5);const auto root=roots[index];
    s.x[0]=pool;s.x[1]=Chain::base+root.name;s.x[8]=s.sp+root.slot;
    s.x[root.renderer]=addr(f->r);if(index==2)s.x[24]=pool;
    s.x[30]=Chain::base+root.lr;
    const auto acquire=s;
    const bool member_ctor=mode=="factory-member-ctor",member_reset=mode=="factory-member-reset";
    const auto C=addr(f->bootstrap_command),M=s.sp+0x300;
    if(member_ctor || member_reset) {
        arena::Saved q{};q.sp=M;q.x[0]=q.x[20]=C;q.x[1]=addr(f->r)+0x50;q.x[2]=q.x[19]=pool;
        q.x[3]=Chain::base+0x478270;q.x[30]=Chain::base+0x3017f0;
        if(member_ctor)emit(0x301800,q);
        else {
            q.x[0]=pool+0xd0;q.x[1]=C;q.x[30]=Chain::base+FT_STOCK_POOL_LR;emit(0x311648,q);
            q.x[0]=q.x[19]=C;q.x[20]=pool;q.x[30]=Chain::base+0x2a9cdc;emit(0x300b20,q);
        }
    }
    if(mode.rfind("factory-diag-",0)==0 || selected) {
        // Existing qualified selected-wrapper turnover, stopped before the
        // acquire callback. A malformed owned descendant must stay fatal.
        const auto sp=s.sp;s={};s.sp=sp+0xd0;s.x[0]=addr(f->r);s.x[30]=Chain::base+FT_BOOT_DISPATCH_LR;
        emit(0x2a1e18,s);s.sp=sp;s.x[19]=addr(f->r);s.x[1]=addr(f->command);emit(0x2a1e3c,s);
        emit(0x2a2230,s);emit(0x2a2320,s);
        s.x[0]=addr(f->command);s.x[30]=Chain::base+FT_BOOT_RELEASE_LR;emit(0x305260,s);
        if(mode=="factory-selected-sibling") {
            // A disjoint receipt cannot erase the open selected cleanup.
            auto q=acquire;q.sp=sp-0x400;q.x[8]=q.sp+0x80;q.x[19]=addr(f->r);
            emit(0x302790,q);q.sp-=0x30;q.x[30]=Chain::base+0x3027b0;emit(0x304e60,q);
            q.sp-=0x60;q.x[19]=pool;q.x[21]=q.x[8];q.x[22]=addr(f->other);emit(0x304ec8,q);emit(0x304ecc,q);
            q.sp=sp-0x400;q.x[0]=pool+0xd0;q.x[1]=addr(f->other);q.x[30]=Chain::base+FT_STOCK_POOL_LR;emit(0x311648,q);
        }
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
    if(mode=="factory-alias-current")s.x[8]=addr(f->command)+8;
    if(mode!="factory-control") {
        uint64_t pc=0x302790;
        if(mode.rfind("factory-site-",0)==0)pc=std::stoull(mode.substr(13),nullptr,16);
        emit(pc,s);
        if(mode.rfind("factory-diag-",0)==0) {
            auto later=s;later.x[0]=0;later.x[8]=0;later.x[30]=Chain::base+0xfeed;
            emit(0x302790,later); // first receipt cannot be replaced by later facts
        }
        if(selected) {
            const auto F=s.sp+0x50;
            if(mode=="factory-selected-reuse") {auto dead=s;dead.x[0]=addr(f->bootstrap_command);emit(0x4b3d0,dead);}
            s.sp-=0x30;s.x[30]=Chain::base+0x3027b0;emit(0x304e60,s);
            s.sp-=0x60;s.x[19]=pool;s.x[21]=addr(f->bootstrap_wrapper)+8;s.x[22]=addr(f->bootstrap_command);
            if(mode=="factory-selected-wrong-c")s.x[22]=addr(f->command);
            if(mode!="factory-selected-no-positive")emit(0x304ec8,s);
            emit(0x304ecc,s);
            s.sp=F-0x30;s.x[30]=Chain::base+0x30548c;
            if(mode!="factory-selected-no-pre-ret")emit(0x3053a8,s);
            s.x[20]=F+0x98;s.x[22]=addr(f->bootstrap_wrapper);
            if(mode!="factory-selected-null-output")emit(0x305490,s);
            s.sp=F;s.x[0]=F+0x98;s.x[30]=Chain::base+0x2a23e8;emit(0x305470,s);
            s.x[19]=addr(f->r);s.x[0]=addr(f->bootstrap_wrapper);
            put(f->bootstrap_wrapper,8,addr(f->bootstrap_command));put(f->r,0x1380,addr(f->bootstrap_wrapper));
            emit(0x2a23f8,s);
        }
        if(mode=="factory-direct-chain" || member_ctor || member_reset || mode.rfind("factory-root-",0)==0) {
            const bool null=mode.find("-null")!=std::string::npos;
            // Null expansion reaches 304ecc with stale x22, bypassing 304ec8.
            // Do not read either historical output or name storage.
            s.sp-=0x30;s.x[30]=Chain::base+0x3027b0;emit(0x304e60,s);
            s.sp-=0x60;s.x[19]=pool;s.x[21]=s.x[8];s.x[22]=addr(f->other);
            if(!null)emit(0x304ec8,s);
            emit(0x304ecc,s);
            if(mode.rfind("factory-root-",0)==0) {
                // Existing queue entry is a later actual caller continuation.
                // It is not a success result and supplies no settlement fact.
                s=acquire;s.x[0]=addr(f->r)+0xa80;s.x[1]=null?0:addr(f->other);
                s.x[30]=Chain::base+root.queue;emit(0x3018b0,s);
            }
        }
    }
    s={};s.sp=addr(f->outer_stack)+0x1000;s.x[0]=pool+0xd0;
    s.x[1]=addr(f->other);s.x[30]=Chain::base+FT_STOCK_POOL_LR;
    if(mode!="factory-selected-sibling")emit(0x311648,s); // existing proved caller, not a new LR allowance
    if(member_ctor || member_reset) {
        arena::Saved q{};q.sp=M;q.x[19]=C;q.x[20]=C;q.x[1]=addr(f->r)+0x50;
        if(member_ctor) {
            q.sp-=0xa0;q.x[30]=Chain::base+0x301804;emit(0x303d10,q);
            q.x[22]=pool;emit(0x303d70,q);q.sp=M;emit(0x301804,q);
            q.x[0]=C;emit(0x301808,q);q.sp-=0x40;q.x[30]=Chain::base+0x300ae0;
        } else {
            q.x[20]=pool;q.sp-=0x20;q.x[0]=0;q.x[30]=Chain::base+0x300c54;emit(0x300c54,q);
            q.sp=M;q.x[30]=Chain::base+0x2a9cdc;
        }
        q.x[0]=C;emit(0x300a08,q);q.sp-=0x30;q.x[0]=0xc001;emit(0x300a28,q);
        q.x[0]=0;q.x[30]=Chain::base+0x300a38;emit(0x300a38,q);
        if(member_ctor) {q.sp=M;q.x[20]=C;q.x[0]=1;q.x[30]=Chain::base+0x30180c;emit(0x30180c,q);}
        q={};q.x[0]=C;emit(0x4b3d0,q);
    }
    assert(native_reads.load()==before);
    assert(!f->router.diagnostics().frames && !f->router.diagnostics().pending && !f->enqueues);
    assert(!munmap(guard,4096));
    if(existing) {s={};s.x[0]=addr(f->other);emit(0x4b3d0,s);}
    else {owned.reset();active=nullptr;} // flush real bounded diagnostic logger
    std::fprintf(stderr,"FACTORY mode=%s reads=0 saved-unchanged=1 no-work=1 returned=1\n",mode.c_str());
    return 0;
}
