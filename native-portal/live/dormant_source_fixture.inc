// SPDX-License-Identifier: MIT
// Inside DormantFixture. Native schedule copied from the delivered source-model
// Connect fixture, routed through actual Runtime and real bounded self reads.
    static constexpr uint64_t client_base=0x10000000;
    uint64_t provider=client_base+0x60f108;
    std::array<uint64_t,2> source_headers{11,12};
    void* source_vtable=nullptr;
    void source_map() {
        source_vtable=mmap(reinterpret_cast<void*>(client_base+0x60f000),4096,
            PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        assert(source_vtable==reinterpret_cast<void*>(client_base+0x60f000));
        auto store=[&](size_t offset,uint64_t value){std::memcpy(static_cast<unsigned char*>(source_vtable)+offset,&value,8);};
        store(0x110,client_base+0x32a270);store(0x118,client_base+0x328380);
        store(0x138,client_base+0x3280a8);store(0x140,client_base+0x327640);
    }
    void source_unmap(){if(source_vtable)assert(!munmap(source_vtable,4096));}
    void source_fact(uint64_t pc,uint64_t body_sp,uint64_t value=0) {
        arena::Saved fact{};fact.sp=body_sp;fact.x[19]=addr(camera);fact.x[0]=fact.x[1]=fact.x[20]=value;
        router.event(pc,fact);
    }
    void source_connect(uint64_t entry_sp,uint64_t lr,bool existing=false,int failure=0) {
        const auto provider_address=reinterpret_cast<uint64_t>(&provider);
        source_fact(0x15f9fc,entry_sp-0x20,1);source_fact(0x15fa04,entry_sp-0x20);
        for(unsigned q=0;q<2;++q) {
            arena::Saved call{};call.sp=entry_sp-0x20;call.x[19]=addr(camera);call.x[0]=provider_address;
            call.x[1]=addr(camera)+0x350+8*q;call.x[2]=base+(q?0x450358:0x4502e8);call.x[3]=client_base+0x32a270;
            const uint64_t begin=q?0x15fa44:0x15fa24;
            router.event(begin,call);call.x[30]=base+begin+4;
            router.event(FT_STOCK_OBSERVER_CLIENT_TAG|0x32a270,call);
            call.sp-=0xc0;call.x[20]=provider_address;call.x[23]=addr(camera)+0x350+8*q;call.x[19]=11+q;
            const int raw=q==0?failure:0;
            if(!raw) {
                if(existing){call.x[0]=11+q;router.event(FT_STOCK_OBSERVER_CLIENT_TAG|0x32a398,call);}
                else {
                    router.event(FT_STOCK_OBSERVER_CLIENT_TAG|0x32a39c,call);
                    call.x[0]=reinterpret_cast<uint64_t>(&source_headers[q]);
                    router.event(FT_STOCK_OBSERVER_CLIENT_TAG|0x32a49c,call);
                    router.event(FT_STOCK_OBSERVER_CLIENT_TAG|0x32a504,call);
                }
                put(camera,0x350+8*q,uint64_t(11+q));
                router.event(FT_STOCK_OBSERVER_CLIENT_TAG|0x32a628,call);
            }
            call.sp=entry_sp-0x20;call.x[19]=addr(camera);call.x[0]=uint32_t(raw);router.event(begin+4,call);
        }
        source_fact(0x15fabc,entry_sp-0x20);source_fact(0x15fa94,entry_sp-0x20,1);
        put(camera,0x34a,uint8_t(1));put(camera,0x360,uint8_t(failure?0:1));
        arena::Saved ret{};ret.sp=entry_sp;ret.x[30]=lr;ret.x[0]=failure?0:1;
        router.event(0x15faa4,ret); // actual completion remains outside this observer
    }
