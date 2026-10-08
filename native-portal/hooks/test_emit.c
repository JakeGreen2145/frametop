#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "pins.h"
#include "emit.h"
int main(void) {
    uint32_t b,code[ISLAND_STRIDE/4];
    assert(!branch_word(4,3,&b)); assert(!branch_word(0,1ul<<27,&b));
    assert(branch_word(1ul<<27,0,&b));
    puts("[");
    for(unsigned base_i=0;base_i<3;base_i++) {
        uintptr_t base=(uintptr_t[]){0x10000000,0x4000000000,0x7f12000000}[base_i];
        for(unsigned i=0;i<SITE_COUNT;i++) {
            uintptr_t at=base+0x400000+i*ISLAND_STRIDE, cb=base+0x700000;
            size_t n=emit_island(code,at,base,i,cb); assert(n && n<ISLAND_STRIDE);
            assert(branch_word(base+sites[i].offset,at,&b));
            printf("{\"name\":\"%s\",\"id\":%u,\"base\":%lu,\"offset\":%u,\"word\":%u,\"kind\":%u,\"island\":%lu,\"callback\":%lu,\"branch\":%u,\"code\":\"",sites[i].name,i,base,sites[i].offset,sites[i].word,sites[i].kind,at,cb,b);
            for(size_t j=0;j<n;j++) printf("%02x",((unsigned char*)code)[j]);
            printf("\"}%s\n",base_i==2 && i==SITE_COUNT-1?"":",");
        }
    }
    puts("]"); return 0;
}
