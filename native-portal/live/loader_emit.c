// SPDX-License-Identifier: MIT
// Compile four times with FT_EMITTER=0..3. Reuse the actual tested generators.
#include <stdint.h>
#include <stddef.h>
#define JOIN_(a,b) a##b
#define JOIN(a,b) JOIN_(a,b)
#define main JOIN(ft_offline_emitter_main_,FT_EMITTER)
#if FT_EMITTER == 1
#include "../arena/emit.c"
#elif FT_EMITTER == 2
#include "../binding/pass_emit.c"
#elif FT_EMITTER == 3
#include "../binding/lifecycle_emit.c"
#else
struct site { uint64_t offset; uint32_t word; unsigned kind; };
#define SITE_COUNT 1
static struct site sites[SITE_COUNT];
#include "../hooks/emit.h"
#endif
#undef main
#define JOIN_(a,b) a##b
#define JOIN(a,b) JOIN_(a,b)
size_t JOIN(ft_loader_emit_,FT_EMITTER)(uint32_t *out,uintptr_t at,uintptr_t base,
                                      uint64_t pc,uint32_t word,unsigned kind,unsigned index,uintptr_t callback) {
    if(index>=SITE_COUNT)return 0;
    sites[index]=(struct site){pc,word,kind==6?0:kind};
#if FT_EMITTER == 1
    return arena_emit(out,at,base,index,callback);
#elif FT_EMITTER == 2
    return pass_emit(out,at,base,index,callback);
#else
    size_t n=emit_island(out,at,base,index,callback);
    // Same lifecycle ADRP-tail correction, generalized to the actual load bias.
    // ADR and ADRP have only one destination; no restored scratch is consumed.
    if(kind==6 && n>=8) {
        int64_t delta=((word>>5)&0x7ffffu)*4+((word>>29)&3u);
        if(delta&(INT64_C(1)<<20))delta-=INT64_C(1)<<21;
        uintptr_t target=(word&0x80000000u)?((base+pc)&~(uintptr_t)4095)+delta*4096:base+pc+delta;
        size_t words=n/4-2;uint32_t branch;
        imm64(out,&words,word&31u,target);
        if(!branch_word(at+words*4,base+pc+4,&branch))return 0;
        out[words++]=branch;n=words*4;
    }
    return n;
#endif
}
