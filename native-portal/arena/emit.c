// SPDX-License-Identifier: MIT
// Offline preparation only. Pin words arrive from the hash-verified private ELF.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
struct site { uint64_t offset; uint32_t word; unsigned kind; };
#define SITE_COUNT 9
static struct site sites[SITE_COUNT];
#include "../hooks/emit.h"
static const uint64_t offsets[SITE_COUNT]={0x161d64,0x29a14c,0x299110,0x2988b0,0x29f358,0x7fdbc,0x2987d4,0x161d68,0x29a150};
// The existing audited save/restore generator is used unchanged. Four restored
// continuations select original, replacement, owned skip, or an unsupported trap.
static size_t arena_emit(uint32_t* out,uintptr_t at,uintptr_t base,unsigned id,uintptr_t callback) {
    uint32_t tmp[512], b;size_t n=0,k=0;
    struct site original=sites[id];
    size_t bytes=emit_island(tmp,at,base,id,callback);if(!bytes)return 0;
    while(k<bytes/4 && tmp[k]!=0xd63f0200)++k;
    if(k==bytes/4)return 0;
    // Insert initialized selector immediately before BLR x16.
    for(size_t i=0;i<k;i++)out[n++]=tmp[i];
    out[n++]=0xf9018fff; // str xzr,[sp,#792]
    out[n++]=tmp[k++];
    out[n++]=0xf9418fe9; // ldr x9,[sp,#792]
    size_t checks[3];
    for(unsigned route=0;route<3;route++) {
        out[n++]=0xf100013f|(route<<10); // cmp x9,#route
        checks[route]=n;out[n++]=0; // b.eq restored continuation
    }
    size_t fatal=n;out[n++]=0;
    for(unsigned route=0;route<4;route++) {
        size_t start=n;
        if(route<3)out[checks[route]]=0x54000000|(((uint32_t)(start-checks[route])&0x7ffff)<<5);
        else {if(!branch_word(at+fatal*4,at+start*4,&b))return 0;out[fatal]=b;}
        sites[id]=original;
        if(route==1 && id==2) {sites[id].kind=0;sites[id].word=0xeb1f03ff;}
        if((route==1 && (id==3 || id==4)) || route==2) {sites[id].kind=0;sites[id].word=0xd503201f;}
        if(route==3) {sites[id].kind=5;sites[id].word=0xd4200000;}
        bytes=emit_island(tmp,at+(start-k)*4,base,id,callback);
        if(!bytes){sites[id]=original;return 0;}
        for(size_t i=k;i<bytes/4;i++)out[n++]=tmp[i];
    }
    sites[id]=original;return n*4;
}
int main(int argc,char** argv) {
    if(argc!=2 && argc!=5)return 2;
    // Optional explicit image base, island block and callback addresses allow
    // preparation for a binding's reserved mapping. This program never maps it.
    uint64_t requested_base=argc==5?strtoull(argv[2],NULL,0):0;
    uint64_t requested_island=argc==5?strtoull(argv[3],NULL,0):0;
    uint64_t requested_callback=argc==5?strtoull(argv[4],NULL,0):0;
    if(requested_base>INT64_MAX-0x700000 || requested_island>INT64_MAX-SITE_COUNT*2048 ||
       requested_callback>INT64_MAX || (argc==5 && (!requested_island || !requested_callback)))return 2;
    FILE* f=fopen(argv[1],"r");if(!f)return 2;
    for(unsigned i=0;i<SITE_COUNT;i++) {
        unsigned long long offset;unsigned word,kind;
        if(fscanf(f,"%llx %x %u",&offset,&word,&kind)!=3 || offset!=offsets[i])return 2;
        sites[i]=(struct site){offset,word,kind};
    }
    fclose(f);puts("[");
    for(unsigned variant=0;variant<(argc==5?1u:3u);variant++)for(unsigned id=0;id<SITE_COUNT;id++) {
        uint64_t base=argc==5?requested_base:variant==0?0:variant==1?0x100000000ull:0x7000000000ull;
        uint64_t at=(argc==5?requested_island:base+0x600000)+id*2048;
        uint64_t callback=argc==5?requested_callback:base+0x680000;uint32_t code[512],branch;
        if(base>UINT64_MAX-0x700000 || at>UINT64_MAX-2048 || (at|base|callback)&3)return 3;
        size_t n=arena_emit(code,at,base,id,callback);
        if(!n || n>2048 || !branch_word(base+sites[id].offset,at,&branch))return 3;
        printf("%s{\"id\":%u,\"base\":%llu,\"island\":%llu,\"callback\":%llu,\"offset\":%llu,\"word\":%u,\"kind\":%u,\"branch\":%u,\"code\":\"",variant||id?",\n":"",id,(unsigned long long)base,(unsigned long long)at,(unsigned long long)callback,(unsigned long long)sites[id].offset,sites[id].word,sites[id].kind,branch);
        for(size_t j=0;j<n;j++)printf("%02x",((unsigned char*)code)[j]);puts("\"}");
    }
    puts("]");return 0;
}
