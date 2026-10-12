// SPDX-License-Identifier: MIT
// Modeled native memory/driver boundary for the complete Runtime fixture.
// source_command_native.py separately executes pinned call/return instructions.
    std::array<std::byte,0x80> source_texture{};
    std::array<std::byte,0x180> source_image{};
    // Shared dispatch/backend/acquire coordinates must retain native SP alignment
    // even when first-consumer-only fields change Chain's member layout.
    alignas(16) std::array<std::byte,0x400> source_stack{};
    std::array<uint64_t,4> source_vector{};
    std::string source_fault;
    void source_init() {
        put(source_texture,0x28,uint64_t(1));put(source_texture,0x48,addr(source_image));
        put(source_image,8,addr(r)+0x50);put(source_image,0xb0,uint64_t(0xcafe));
        put(r,0x768,uint32_t(0));put(r,0xb20,uint32_t(-3));put(r,0x13f8,uint64_t(42));
        put(r,0x1550,uint64_t(source_vector.data()));put(r,0x1558,uint64_t(source_vector.data()));
        put(r,0x1560,uint64_t(source_vector.data()+source_vector.size()));
    }
    void source_barrier(bool reverse) {
        const uint64_t sp=addr(source_stack)+0x200;
        arena::Saved call{};call.sp=sp;call.x[19]=addr(r);call.x[20]=uint64_t(source_vector.data());
        call.x[0]=addr(source_image);call.x[1]=addr(executable);call.x[2]=reverse?5:1;call.x[3]=reverse?1:5;
        call.x[4]=1;call.x[6]=8;call.x[7]=0x20;
        put(source_stack,0x200,uint32_t(0));put(source_stack,0x208,uint32_t(1));
        put(source_stack,0x210,reverse?uint32_t(0):uint32_t(-3));put(source_stack,0x218,reverse?uint32_t(-3):uint32_t(0));
        put(source_image,0xd8,reverse?UINT64_MAX:uint64_t(42));
        if(source_fault=="resource" && reverse)call.x[0]+=8;
        if(source_fault=="command" && reverse)call.x[1]+=8;
        if(source_fault=="family" && reverse)put(source_stack,0x218,uint32_t(-1));
        if(source_fault=="image" && reverse)put(source_image,0xb0,uint64_t(0xcaff));
        if(source_fault=="frame" && reverse)put(r,0x13f8,uint64_t(43));
        if(source_fault=="generation" && reverse)put(executable,0x738,uint64_t(0xc002));
        router.event(reverse?0x2a255c:0x29ac80,call);
        if(source_fault=="generation" && reverse)put(executable,0x738,uint64_t(0xc001));
        arena::Saved inner{};inner.sp=sp-0xc0;inner.x[19]=addr(source_image);inner.x[20]=addr(executable);
        inner.x[0]=0xc001;inner.x[1]=1;inner.x[2]=8;
        put(source_stack,0x140,uint32_t(1));put(source_stack,0x148,inner.sp+0x70);
        const uint32_t b[]={45,0,0,0,0,0x20,reverse?5u:1u,reverse?1u:5u,
            reverse?0u:uint32_t(-3),reverse?uint32_t(-3):0u,0xcafe,0,1,0,1,0,1,0};
        std::memcpy(source_stack.data()+0x1b0,b,sizeof b);
        if(source_fault=="range" && reverse)put(source_stack,0x1b0+0x38,uint32_t(2));
        if(source_fault!="barrier-api")router.event(0x2fe72c,inner);
        router.event(0x2fe730,inner);
        inner.x[0]=addr(executable);inner.x[1]=addr(source_image);router.event(0x2fe738,inner);
        if(source_fault!="registration")router.event(0x2fe73c,inner);
        router.event(reverse?0x2a2560:0x29ac84,call);
    }
    void source_replay(unsigned phase) {
        const char* path=std::getenv("FT_SOURCE_NATIVE");assert(path);
        FILE* file=std::fopen(path,"rb");assert(file);uint64_t count=0;assert(std::fread(&count,8,1,file)==1);
        auto relocate=[&](uint64_t v) {
            const uint64_t starts[]={addr(r),addr(executable),addr(command),addr(source_texture),addr(source_image),
                uint64_t(source_vector.data()),addr(packets),addr(source_stack)};
            for(unsigned i=0;i<8;++i)if(v>=0x1000000+i*0x10000 && v<0x1000000+i*0x10000+0x4000)
                return starts[i]+v-(0x1000000+i*0x10000);
            return v;
        };
        auto move=[&](auto& bytes) {for(size_t i=0;i+8<=bytes.size();i+=8) {
            uint64_t v=0;std::memcpy(&v,bytes.data()+i,8);v=relocate(v);std::memcpy(bytes.data()+i,&v,8);
        }};
        for(uint64_t n=0;n<count;++n) {
            uint64_t selected=0,pc=0,end=0;arena::Saved event{};
            decltype(source_stack) stack_bytes{};decltype(source_image) image_bytes{};std::array<std::byte,32> vector_bytes{};
            assert(std::fread(&selected,8,1,file)==1 && std::fread(&pc,8,1,file)==1 &&
                std::fread(&event,sizeof event,1,file)==1 && std::fread(stack_bytes.data(),stack_bytes.size(),1,file)==1 &&
                std::fread(image_bytes.data(),image_bytes.size(),1,file)==1 && std::fread(vector_bytes.data(),vector_bytes.size(),1,file)==1 &&
                std::fread(&end,8,1,file)==1);
            if(selected!=phase)continue;
            for(auto& x:event.x)x=relocate(x);
            event.sp=relocate(event.sp);
            move(stack_bytes);move(image_bytes);move(vector_bytes);
            source_stack=stack_bytes;source_image=image_bytes;std::memcpy(source_vector.data(),vector_bytes.data(),32);
            put(r,0x1558,relocate(end));
            const auto before=event;router.event(pc,event);assert(!std::memcmp(&before,&event,sizeof event));
        }
        assert(!std::fclose(file));
    }
    void source_sample(unsigned eye) {
        if(std::getenv("FT_SOURCE_NATIVE")){source_replay(eye);return;}
        const auto saved=s;
        if(source_fault=="missing" || (source_fault=="partial-eye" && eye==1))return;
        // Backend's native SP includes its 0x90-byte frame, then acquire -0x50.
        arena::Saved sample{};sample.sp=addr(source_stack)+0x250;
        sample.x[0]=sample.x[21]=addr(r);sample.x[1]=addr(source_texture);sample.x[2]=8;sample.x[3]=0x20;
        sample.x[24]=addr(packets)+eye*0x2b0;sample.x[20]=sample.x[24]+0x80;
        if(source_fault=="record")sample.x[24]+=8;
        router.event(0x29f294,sample);
        uint64_t vector_end=0;std::memcpy(&vector_end,r.data()+0x1558,8);
        if(vector_end==uint64_t(source_vector.data())) {
            source_barrier(false);source_vector[0]=addr(source_texture);
            put(r,0x1558,uint64_t(source_vector.data()+1));
        }
        router.event(0x29f298,sample);s=saved;
    }
    void source_reverse() {
        if(std::getenv("FT_SOURCE_STOCK_RELEASE")) {
            // An unrelated native texture in the same renderer vector. Its
            // original work is outside the owned attribution, not an error.
            arena::Saved stock{};stock.x[19]=addr(r);stock.x[0]=addr(other);stock.x[1]=addr(executable);
            stock.sp=addr(source_stack)+0x200;const auto before=stock;
            router.event(0x2a255c,stock);router.event(0x2a2560,stock);
            assert(!std::memcmp(&stock,&before,sizeof stock));
        }
        if(std::getenv("FT_SOURCE_NATIVE")){source_replay(2);return;}
        if(source_fault=="missing" || source_fault=="reverse")return;
        source_barrier(true);
        put(r,0x1558,uint64_t(source_vector.data())); // original clear, modeled
    }
