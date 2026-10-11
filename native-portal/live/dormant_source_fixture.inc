// SPDX-License-Identifier: MIT
// Inside DormantFixture. Native schedule copied from the delivered source-model
// Connect fixture, routed through actual Runtime and real bounded self reads.
    static constexpr uint64_t client_base=0x10000000;
    uint64_t provider=client_base+0x60f108;
    std::array<uint64_t,2> source_headers{11,12};
    void* source_vtable=nullptr;void* empty_page=nullptr;
    unsigned modeled_acquires=0,modeled_returns=0,modeled_empty=0;
    static void* reserve_source_vtable() {
        struct Reservation {
            void* page=mmap(reinterpret_cast<void*>(client_base+0x60f000),4096,
                PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
            Reservation(){assert(page==reinterpret_cast<void*>(client_base+0x60f000));}
            ~Reservation(){assert(!munmap(page,4096));}
        };
        // Keep the low address occupied between Runtime instances, otherwise
        // the process heap can grow into it before the next source_map call.
        static Reservation reservation;
        return reservation.page;
    }
    void source_map() {
        source_vtable=reserve_source_vtable();
        std::memset(source_vtable,0,4096);
        // Reserve the fixed modeled vtable before an anonymous PROT_NONE page.
        // Its process-lifetime reservation also excludes heap growth between runs.
        empty_page=mmap(nullptr,4096,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(empty_page!=MAP_FAILED);
        auto store=[&](size_t offset,uint64_t value){std::memcpy(static_cast<unsigned char*>(source_vtable)+offset,&value,8);};
        store(0x110,client_base+0x32a270);store(0x118,client_base+0x328380);
        store(0x138,client_base+0x3280a8);store(0x140,client_base+0x327640);
    }
    void source_unmap(){source_vtable=nullptr;if(empty_page)assert(!munmap(empty_page,4096));empty_page=nullptr;}
    std::array<std::array<std::byte,0x200>,2> metadata_objects{},metadata_headers{},metadata_values{};
    void source_metadata(uint64_t entry_sp,unsigned q,bool release,int raw=0,bool bypass=false) {
        const uint64_t local_sp=entry_sp-0x360,manager=reinterpret_cast<uint64_t>(&provider);
        if(release)++modeled_returns;else ++modeled_acquires;
        if(raw==3)++modeled_empty;
        const auto object=addr(metadata_objects[q]),header=raw==3?reinterpret_cast<uint64_t>(empty_page):addr(metadata_headers[q]),meta=addr(metadata_values[q]),data=meta+0x100;
        if(raw==3) {
            // Deliberately stale nonzero slots and an inaccessible header.
            std::memcpy(reinterpret_cast<void*>(local_sp+0x90),&header,8);
            std::memcpy(reinterpret_cast<void*>(local_sp+0x98),&header,8);
        }
        const uint64_t start=release?(q?0x160164:0x160380):(q?0x15fee4:0x160184);
        const uint64_t manager_entry=release?0x327640:0x3280a8,call=release?0x3276c4:0x32813c;
        const uint64_t inner_entry=release?0x3274d8:0x327e18,mframe=release?0x40:0x50,iframe=release?0x60:0x70;
        auto client_event=[&](uint64_t pc,const arena::Saved& saved){auto copy=saved;router.event(FT_STOCK_OBSERVER_CLIENT_TAG|pc,copy);};
        arena::Saved s{};s.sp=local_sp;s.x[19]=addr(camera);s.x[0]=manager;s.x[1]=11+q;
        s.x[2]=release?meta:local_sp+0x90;s.x[3]=release?client_base+manager_entry:local_sp+0x98;s.x[5]=client_base+manager_entry;
        router.event(start,s);
#ifdef FT_DORMANT_CURRENT_TEST
        const auto before_empty_reads=source_test_reads();
#endif
        s.x[30]=base+start+4;client_event(manager_entry,s);
        s.sp=local_sp-mframe;s.x[0]=bypass?0:1;s.x[20]=manager;s.x[22]=manager+8;s.x[19]=11+q;
#ifdef FT_DORMANT_CURRENT_TEST
        const char* manager_vector=release?(bypass?"manager-False-got-0":"manager-False-got-4302794752"):
            (bypass?"manager-True-got-0":"manager-True-got-4302794752");
        s.x[0]=admission_scalar(manager_vector,release?0x32766c:0x3280e0,0);
#endif
        client_event(release?0x32766c:0x3280e0,s);
        s.x[0]=0;s.x[30]=client_base+(release?0x327678:0x3280ec);
        if(!bypass)client_event(release?0x327678:0x3280ec,s);
        if(raw!=4) {
        s.x[19]=11+q;s.x[20]=manager+0x70;s.x[0]=object;
        s.x[1]=release?meta:local_sp+0x90;s.x[2]=release?0:local_sp+0x98;s.x[3]=0;
        client_event(call,s);s.x[30]=client_base+call+4;client_event(inner_entry,s);
        s.sp-=iframe;s.x[release?21:20]=object;
        const auto inner=s.sp,address=inner+(release?0x48:0x58);
        auto g=s;g.x[0]=address;g.x[30]=client_base+(release?0x327504:0x327e4c);client_event(0x258438,g);
        g.sp=inner-0x30;g.x[19]=address;g.x[0]=1;g.x[30]=client_base+0x2584c4;client_event(0x2584c4,g);
        g.sp=inner;g.x[30]=client_base+(release?0x327504:0x327e4c);client_event(0x2584f8,g);
        s.x[0]=header;put(metadata_headers[q],0,uint64_t(11+q));
        client_event(release?0x32750c:0x327e54,s);
        if(raw==3) {
            // Native no-grant leaves stale output untouched. Never emulate a read.
            s.x[19]=0x80;s.nzcv=0x60000000;
#ifdef FT_DORMANT_CURRENT_TEST
            s.x[19]=admission_scalar("mode0-latest-128",0x327e60,19);
            s.nzcv=admission_scalar("mode0-latest-128",0x327e60,96);
#endif
            client_event(0x327e60,s);
        } else if(raw!=5) {
        s.x[1]=release?meta:local_sp+0x90;client_event(release?0x3275a0:0x327f2c,s);
        std::memcpy(reinterpret_cast<void*>(local_sp+0x90),&meta,8); // modeled provider output, also on provider7
        if(raw!=7)std::memcpy(reinterpret_cast<void*>(local_sp+0x98),&data,8);
        s.x[0]=raw==7?2:0;
        client_event(release?0x3275a4:0x327f30,s);
        if(raw!=7) {
        put(metadata_headers[q],0x68,uint32_t(release?2:1));s.x[19]=header;s.x[1]=header;s.x[24]=0x68;s.x[0]=release?1:2;
        client_event(release?0x32758c:0x327f70,s);put(metadata_headers[q],0x68,uint32_t(s.x[0]));
        client_event(release?0x327590:0x327f74,s);
        }
        }
        g.sp=inner;g.x[0]=address;g.x[30]=client_base+(raw==7?0x327fa4:raw==5?0x327e84:raw?0x327fd8:release?0x3275cc:0x327f80);client_event(0x258508,g);
        s.sp=local_sp-mframe;s.x[0]=raw;client_event(call+4,s);
        }
        s.sp=local_sp-mframe;s.x[21]=bypass?0:1;s.x[22]=manager+8;
#ifdef FT_DORMANT_CURRENT_TEST
        s.x[21]=admission_scalar(manager_vector,release?0x3276d0:0x328148,21);
#endif
        client_event(release?0x3276d0:0x328148,s);
        s.x[0]=manager+8;if(!bypass)client_event(release?0x3276d8:0x328150,s);
        s.sp=local_sp;s.x[19]=addr(camera);s.x[0]=raw;router.event(start+4,s);
#ifdef FT_DORMANT_CURRENT_TEST
        if(raw==3)assert(source_test_reads()==before_empty_reads);
#endif
    }
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
