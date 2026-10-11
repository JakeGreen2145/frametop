// SPDX-License-Identifier: MIT
// Complete selected turnover contract model. Coordinates/order are from Desktop
// contract-90ecc77b3f8a REPORT.md, not a newly executed native observer stream.
// Allocation, pool/Vulkan outcomes and native memory stores remain modeled.
static void retired_turnover(Chain& f,bool same,const std::string& fault="",uint64_t load_lr=0x2a23b4,uint64_t pool_result=1) {
    const uint64_t R=addr(f.r),C=addr(f.executable);
    uint64_t W=0;std::memcpy(&W,f.r.data()+0x1380,8);
    const uint64_t P=addr(f.bootstrap_pool),B=addr(f.outer_stack)+0x1000;
    const uint64_t N=same?W:W==addr(f.command)?addr(f.bootstrap_wrapper):addr(f.command);
    const uint64_t A=R+0x1300,name=Chain::base+0x478dc0;
    // Reuse C only after its actual successful reset/Begin. The wrapper epoch
    // still changes, including when the allocator returns the identical address.
    put(f.r,0x15a8,addr(f.passes));
    auto emit=[&](uint64_t pc,arena::Saved s) {
        s.pad=0x55;s.nzcv=0xa0000000;
        for(unsigned i=0;i<sizeof(s.q);++i)reinterpret_cast<unsigned char*>(s.q)[i]=static_cast<unsigned char>(i);
        const auto saved=s;f.router.event(pc,s);assert(!std::memcmp(&s,&saved,sizeof s));
    };
    arena::Saved s{};s.sp=B+0xd0;s.x[0]=R;s.x[30]=Chain::base+0x15a9bc;emit(0x2a1e18,s);
    s.sp=B;s.x[19]=R;s.x[1]=W;emit(0x2a1e3c,s);
    emit(0x2a2230,s);emit(0x2a2320,s);put(f.r,0x1380,uint64_t(0));
    s.x[0]=s.x[20]=W;s.x[30]=Chain::base+0x2a23b4;emit(0x305260,s);
    auto free=s;free.x[1]=0x10;free.x[30]=Chain::base+0x2a23c0;
    if(fault=="early")emit(0x4b3d0,free);
    s.sp-=0x30;s.x[19]=W;s.x[20]=0;s.x[1]=C;s.x[30]=Chain::base+load_lr;emit(0x3052e0,s);
    s.x[0]=P+0xd0;emit(0x3052ec,s);s.x[30]=Chain::base+0x3052f0;
    if(fault!="missing-entry")emit(0x311648,s);
    s.x[0]=fault=="failed-result"?0:pool_result;
    if(fault!="missing-result")emit(0x3052f0,s);
    if(fault=="size")free.x[1]=0x100000010ull;
    if(fault=="sp")free.sp+=16;
    if(fault=="lr")free.x[30]+=4;
    if(fault=="r")free.x[19]+=16;
    if(fault=="w")free.x[20]+=16;
    if(fault=="command")free.x[0]=C;
    if(fault=="free-subject")free.x[0]=1;
    if(fault=="renderer")free.x[0]=R;
    const auto before=native_reads.load();
    retired_read_end=W+0x10;retired_read_begin=W;
    if(fault!="missing-free")emit(fault=="pc"?0x3003d0:0x4b3d0,free);
    assert(native_reads.load()==before);
    if(fault=="duplicate")emit(0x4b3d0,free);
    std::fprintf(stderr,"RETIRED sized-delete modeled=1 same=%u fault=%s saved-unchanged=1 free-reads=0\n",same,fault.c_str());
    // Pool recycling does not destroy C; it issues a new executable generation.
    s={};s.sp=B-0x200;s.x[0]=s.x[19]=C;s.x[20]=P;s.x[30]=Chain::base+0x2a9cdc;emit(0x300b20,s);
    s.sp-=0x20;s.x[0]=0;s.x[30]=Chain::base+0x300c54;emit(0x300c54,s);
    s.sp+=0x20;s.x[0]=C;s.x[30]=Chain::base+0x2a9cdc;emit(0x300a08,s);
    s.sp-=0x30;s.x[0]=0xc001;emit(0x300a28,s);s.x[0]=0;s.x[30]=Chain::base+0x300a38;emit(0x300a38,s);
    s={};s.sp=B;s.x[0]=s.x[20]=A;s.x[19]=R;s.x[1]=name;s.x[8]=B+0x98;s.x[30]=Chain::base+0x2a23e8;
    emit(0x3053f8,s);
    s.sp-=0x30;s.x[0]=fault=="factory-c"?C:fault=="factory-r"?R:N;
    s.x[1]=A;s.x[2]=name;s.x[30]=Chain::base+0x30548c;emit(0x305308,s);
    if(N==W)retired_read_begin=0; // new epoch, not restored old read authority
    if(fault=="factory-free")emit(0x4b3d0,free);
    s.sp-=0x20;s.x[0]=P;s.x[1]=name;s.x[3]=A;s.x[19]=N;s.x[8]=N+8;s.x[30]=Chain::base+0x305330;emit(0x302790,s);
    s.sp-=0x30;s.x[30]=Chain::base+0x3027b0;emit(0x304e60,s);
    s.sp-=0x60;s.x[19]=P;s.x[21]=N+8;s.x[22]=C;emit(0x304ec8,s);
    put(N==addr(f.command)?f.command:f.bootstrap_wrapper,8,C);emit(0x304ecc,s);
    s.sp=B-0x30;s.x[30]=Chain::base+0x30548c;emit(0x3053a8,s);
    s.x[20]=B+0x98;s.x[22]=N;emit(0x305490,s);
    s.sp=B;s.x[19]=R;s.x[20]=A;s.x[0]=B+0x98;s.x[30]=Chain::base+0x2a23e8;emit(0x305470,s);
    s.x[20]=0;s.x[0]=N;emit(0x2a23f4,s);put(f.r,0x1380,N);emit(0x2a23f8,s);
    if(fault=="current-free")emit(0x4b3d0,free);
    retired_read_begin=0;retired_read_end=0;
    // The chain fixture's fixed executable/W pair must remain current for its
    // later stereo. Different-address tests switch storage for the next model.

    put(f.r,0x15a8,addr(f.passes)+f.passes.size());
}
