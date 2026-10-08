// SPDX-License-Identifier: MIT
// Offline only. Never installs a branch or maps executable memory.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
struct site { uint64_t offset; uint32_t word; unsigned kind; };
#define SITE_COUNT 2
static struct site sites[SITE_COUNT];
#include "../hooks/emit.h"
// id0 dispatch: callback receives saved x0=R/w1=mode, then original CBNZ.
// id1 packet: callback receives saved x19=packet/x21=R/x22=pass. Set saved.pad
// nonzero ONLY for exact immutable owned marker with no committed pair. Default
// zero preserves stock. Both routes restore all registers/flags/SIMD/SP/LR.
static size_t pass_emit(uint32_t *out,uintptr_t at,uintptr_t base,unsigned id,uintptr_t callback) {
    if(id==0) return emit_island(out,at,base,id,callback);
    uint32_t tmp[256],branch;size_t n=0,k=0;
    size_t bytes=emit_island(tmp,at,base,id,callback);
    if(!bytes) return 0;
    while(k<bytes/4 && tmp[k]!=0xd63f0200) ++k;
    if(k==bytes/4) return 0;
    for(size_t i=0;i<k;++i)out[n++]=tmp[i];
    out[n++]=0xf9018fff; // str xzr,[sp,#792], never use an uninitialized selector
    out[n++]=tmp[k++];
    out[n++]=0xf9418fe9; // ldr x9,[sp,#792]
    size_t choose=n++; // CBNZ x9, fully restored skip route
    for(size_t i=k;i<bytes/4-2;++i)out[n++]=tmp[i];
    out[n++]=sites[id].word;
    if(!branch_word(at+n*4,base+0x2a1c1c,&branch))return 0;
    out[n++]=branch;
    size_t skip=n;
    out[choose]=0xb5000009|((uint32_t)(skip-choose)<<5);
    for(size_t i=k;i<bytes/4-2;++i)out[n++]=tmp[i];
    if(!branch_word(at+n*4,base+0x2a1c0c,&branch))return 0;
    out[n++]=branch;
    return n*4;
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"r");if(!f)return 2;
    const uint64_t offsets[SITE_COUNT]={0x2a1e18,0x2a1c18};
    const unsigned kinds[SITE_COUNT]={3,0};
    for(unsigned i=0;i<SITE_COUNT;++i) {
        unsigned long long offset;unsigned word,kind;
        if(fscanf(f,"%llx %x %u",&offset,&word,&kind)!=3 || offset!=offsets[i] || kind!=kinds[i])return 2;
        sites[i]=(struct site){offset,word,kind};
    }
    fclose(f);puts("[");
    for(unsigned v=0;v<3;++v)for(unsigned id=0;id<SITE_COUNT;++id) {
        uint64_t base=v==0?0:v==1?0x100000000ull:0x7000000000ull;
        uint64_t at=base+0x600000+id*1024,callback=base+0x680000;
        uint32_t code[256],branch;
        size_t bytes=pass_emit(code,at,base,id,callback);
        if(!bytes || bytes>sizeof code || !branch_word(base+sites[id].offset,at,&branch))return 3;
        printf("%s{\"id\":%u,\"base\":%llu,\"island\":%llu,\"callback\":%llu,\"offset\":%llu,\"branch\":%u,\"code\":\"",v||id?",\n":"",id,(unsigned long long)base,(unsigned long long)at,(unsigned long long)callback,(unsigned long long)sites[id].offset,branch);
        for(size_t j=0;j<bytes;++j)printf("%02x",((unsigned char*)code)[j]);
        puts("\"}");
    }
    puts("]");return 0;
}
