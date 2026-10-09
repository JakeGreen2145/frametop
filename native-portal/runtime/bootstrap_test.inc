// SPDX-License-Identifier: MIT
// CPU event fixture; the independent instruction runner derives LR/SP values.
static void bootstrap_recording_turnover() {
    for(unsigned fault=0;fault<=16;++fault) {
        Fixture f;assert(f.bootstrap(true,nullptr,1));
        std::array<std::byte,0x10> next{};put(next,8,addr(f.other));
        put(f.executable,0x50,uint64_t(0xa000));
        binding::Owner before{};assert(f.evidence.command_owner(before));
        if(fault==1)assert(f.objects()); // native-owned packet resources forbid bootstrap exemption
        f.s={};f.s.x[0]=addr(f.r);f.s.sp=0x900000;f.s.x[30]=Fixture::base+0x6caf0;
        if(fault==2)f.s.x[30]+=4;
        if(fault==3)f.s.x[0]+=8;
        f.evidence.event(0x2a1e18,f.s);
        f.s.sp-=0xd0;f.s.x[19]=addr(f.r);
        if(fault!=4)f.evidence.event(0x2a2230,f.s);
        if(fault!=5)f.evidence.event(0x2a2320,f.s);
        if(fault!=6)put(f.r,0x1380,uint64_t(0));
        if(fault==7)put(f.executable,0x738,uint64_t(0xc003));
        if(fault==8)put(f.command,8,addr(f.other));
        if(fault==9)++f.s.sp;
        f.s.x[0]=addr(f.command);f.s.x[30]=Fixture::base+(fault==10?0x2a2338:0x2a23b4);
        f.evidence.event(0x305260,f.s);
        if(!fault) {
            assert(f.evidence.initialized());
            uint64_t value=0;auto reader=f.evidence.bridge_memory();
            assert(!reader.read(reader.user,addr(f.command)+8,&value,8));
            assert(reader.read(reader.user,addr(f.executable)+0x738,&value,8));
        }
        f.s.sp-=0x30;f.s.x[0]=0xa0d0;f.s.x[1]=addr(f.executable);f.s.x[19]=addr(f.command);
        f.s.x[30]=Fixture::base+0x3052f0;
        if(fault==11)f.s.x[30]+=4;
        if(fault==12) {put(f.executable,0x50,uint64_t(0xb000));f.s.x[0]=0xb0d0;}
        if(fault!=13)f.evidence.event(0x311648,f.s);
        if(fault==14)f.evidence.event(0x311648,f.s); // duplicate pool return is not a second exemption
        if(fault==15)put(next,8,addr(f.executable)); // old submitted generation cannot be republished
        f.s.sp=0x8fff30;f.s.x[19]=addr(f.r);f.s.x[0]=addr(next);
        if(fault==16)++f.s.sp;
        f.evidence.event(0x2a23f4,f.s);put(f.r,0x1380,addr(next));
        if(fault)assert(!f.evidence.initialized());
        else {
            binding::Owner after{};assert(f.evidence.initialized() && f.evidence.command_owner(after));
            assert(after.command==addr(f.other) && after.generation>before.generation);
            assert(after.identity.renderer_generation==before.identity.renderer_generation);
            // No new graph authority, queue success, disposal certificate or reset.
            assert(!f.evidence.graph_capable());
        }
    }
}
