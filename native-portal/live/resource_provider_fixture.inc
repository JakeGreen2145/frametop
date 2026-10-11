// SPDX-License-Identifier: MIT
// Included inside Chain. Pinned scalar data, synthetic mapped storage/services.
    #include "resource-names.private.hpp"
    uint64_t resource_receiver=client_base+0x6121d8;
    void resource_map() {
        // Keep fixed fixture pages reserved through sequential Runtime instances.
        // No brk/anonymous allocator may claim a retired low-address hole.
        static std::vector<uint64_t> pages;
        auto map=[&](uint64_t address) {
            const auto page=address&~uint64_t(4095);
            if(std::find(pages.begin(),pages.end(),page)!=pages.end())return;
            const auto result=mmap(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE,
                MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
            assert(result==reinterpret_cast<void*>(page));pages.push_back(page);
        };
        auto word=[](uint64_t address,uint64_t value){std::memcpy(reinterpret_cast<void*>(address),&value,8);};
        map(base+0x5b0e48);map(client_base+0x6121d8);
        word(base+0x5b0e48,uint64_t(&resource_receiver));
        word(client_base+0x6121d8,client_base+0x3ccfc0);word(client_base+0x6121e8,client_base+0x3cd878);
        for(const auto& name:resource_fixture_names) {
            if(name.pointer){map(base+name.pointer);word(base+name.pointer,base+name.string);}
            map(base+name.string);map(base+name.string+std::strlen(name.text));
            std::memcpy(reinterpret_cast<void*>(base+name.string),name.text,std::strlen(name.text)+1);
        }
        const char* fault=std::getenv("FT_GRAPH_PROVIDER");if(!fault)return;
        if(!std::strcmp(fault,"missing"))word(base+0x5b0e48,0);
        if(!std::strcmp(fault,"vptr"))resource_receiver+=8;
        if(!std::strcmp(fault,"slot0"))word(client_base+0x6121d8,client_base+0x3ccfc4);
        if(!std::strcmp(fault,"slot2"))word(client_base+0x6121e8,client_base+0x3cd87c);
        if(!std::strcmp(fault,"name"))*reinterpret_cast<char*>(base+0x47c690)='{';
        if(!std::strcmp(fault,"pointer"))word(base+0x59b210,base+0x47c6d0);
    }
