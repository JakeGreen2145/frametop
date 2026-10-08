// SPDX-License-Identifier: MIT
// Offline-only producer. No mapping, installer, process attach or activation.
#include <stdio.h>
#include <stdint.h>
struct site { uint64_t offset; uint32_t word; unsigned kind; };
#define SITE_COUNT 9
static struct site sites[SITE_COUNT];
#include "../hooks/emit.h"
static const uint64_t offsets[SITE_COUNT]={0x2a2230,0x2a2320,0x2987d8,0x298840,0x300b20,0x300c54,0x300a08,0x300a38,0x3018b0};
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"r");if(!f)return 2;
    for(unsigned i=0;i<SITE_COUNT;++i) {
        unsigned long long offset;unsigned word,kind;
        if(fscanf(f,"%llx %x %u",&offset,&word,&kind)!=3 || offset!=offsets[i])return 2;
        sites[i]=(struct site){offset,word,kind};
    }
    fclose(f);puts("[");
    for(unsigned i=0;i<SITE_COUNT;++i) {
        uintptr_t at=0x600000+i*ISLAND_STRIDE;uint32_t code[ISLAND_STRIDE/4],branch;
        size_t n=emit_island(code,at,0,i,0x680000);
        // The shared emitter relocates control flow, but not ADRP. Replace only
        // the displaced ADRP tail after its complete original restore sequence.
        if(i==1) {
            uint32_t word=sites[i].word;
            if(!n || (word&0x9f000000u)!=0x90000000u) return 3;
            int64_t delta=((word>>5)&0x7ffffu)*4+((word>>29)&3u);
            if(delta&(INT64_C(1)<<20))delta-=INT64_C(1)<<21;
            uint64_t target=(sites[i].offset&~UINT64_C(4095))+delta*4096;
            size_t words=n/4-2;
            imm64(code,&words,word&31u,target);
            if(!branch_word(at+words*4,sites[i].offset+4,&branch))return 3;
            code[words++]=branch;n=words*4;
        }
        if(!n || n>ISLAND_STRIDE || !branch_word(sites[i].offset,at,&branch))return 3;
        printf("%s{\"id\":%u,\"offset\":%llu,\"island\":%llu,\"branch\":%u,\"code\":\"",i?",":"",i,(unsigned long long)sites[i].offset,(unsigned long long)at,branch);
        for(size_t k=0;k<n;++k)printf("%02x",((unsigned char *)code)[k]);
        puts("\"}");
    }
    puts("]");return 0;
}
