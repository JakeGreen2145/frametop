/* Bounded A64 generator for these exact pins, not a general relocator. */
#include <stdint.h>
#include <stddef.h>
#define ISLAND_STRIDE 1024
#define CONTEXT_SIZE 800
struct saved { uint64_t x[31], sp; unsigned char q[32][16]; uint64_t nzcv, fpcr, fpsr, pad; };
_Static_assert(sizeof(struct saved)==CONTEXT_SIZE,"bridge ABI");
static int branch_word(uintptr_t from, uintptr_t to, uint32_t *word) {
    int64_t d=(int64_t)to-(int64_t)from;
    if ((from|to)&3 || d<-(INT64_C(1)<<27) || d>=(INT64_C(1)<<27)) return 0;
    *word=0x14000000u|((uint32_t)(d/4)&0x3ffffffu); return 1;
}
static void imm64(uint32_t *out,size_t *n,unsigned r,uint64_t v) {
    for (unsigned h=0;h<4;h++) out[(*n)++]=(h?0xf2800000:0xd2800000)|(h<<21)|(((v>>(h*16))&65535)<<5)|r;
}
static uintptr_t native_target(uintptr_t pc,uint32_t word,unsigned bits) {
    int64_t imm=(bits==26?word&0x3ffffff:(word>>5)&0x7ffff);
    if (imm&(INT64_C(1)<<(bits-1))) imm-=INT64_C(1)<<bits;
    return pc+imm*4;
}
static size_t emit_island(uint32_t *out,uintptr_t at,uintptr_t base,unsigned id,uintptr_t callback) {
    if (id>=SITE_COUNT) return 0;
    const struct site *s=sites+id; size_t n=0; uint32_t b;
    uintptr_t pc=base+s->offset;
    out[n++]=0xd10003ff|(CONTEXT_SIZE<<10); /* sub sp,sp,#800 */
    for (unsigned r=0;r<30;r+=2) out[n++]=0xa9000000|(r<<15)|((r+1)<<10)|(31<<5)|r;
    out[n++]=0xf90003fe|(30<<10); /* str x30,[sp,#240] */
    out[n++]=0x910003e9|(CONTEXT_SIZE<<10);
    out[n++]=0xf90003e9|(31<<10); /* original SP */
    for (unsigned r=0;r<32;r+=2) out[n++]=0xad000000|((16+r)<<15)|((r+1)<<10)|(31<<5)|r;
    out[n++]=0xd53b4209; out[n++]=0xf90003e9|(96<<10); /* NZCV */
    out[n++]=0xd53b4409; out[n++]=0xf90003e9|(97<<10); /* FPCR */
    out[n++]=0xd53b4429; out[n++]=0xf90003e9|(98<<10); /* FPSR */
    out[n++]=0x910003e0; /* mov x0,sp */
    out[n++]=0xd2800001|(id<<5);
    imm64(out,&n,16,callback); out[n++]=0xd63f0200;
    out[n++]=0xf94003e9|(96<<10); out[n++]=0xd51b4209;
    out[n++]=0xf94003e9|(97<<10); out[n++]=0xd51b4409;
    out[n++]=0xf94003e9|(98<<10); out[n++]=0xd51b4429;
    for (unsigned r=0;r<32;r+=2) out[n++]=0xad400000|((16+r)<<15)|((r+1)<<10)|(31<<5)|r;
    for (unsigned r=0;r<30;r+=2) out[n++]=0xa9400000|(r<<15)|((r+1)<<10)|(31<<5)|r;
    out[n++]=0xf94003fe|(30<<10);
    out[n++]=0x910003ff|(CONTEXT_SIZE<<10);
    if (s->kind==1 || s->kind==2) {
        /* Native LR, not an island return address. No live scratch register. */
        imm64(out,&n,30,pc+4);
        if(s->kind==2) {out[n++]=0xd61f0000|(s->word&0x3e0); return n*4;}
        if(!branch_word(at+n*4,native_target(pc,s->word,26),&b))return 0;
        out[n++]=b; return n*4;
    }
    if(s->kind==3) {
        out[n++]=(s->word&~0x00ffffe0u)|(2<<5); /* Pinned CBZ/CBNZ/B.cond to second B; preserve register/condition bits. */
        if(!branch_word(at+n*4,pc+4,&b))return 0;
        out[n++]=b;
        if(!branch_word(at+n*4,native_target(pc,s->word,19),&b))return 0;
        out[n++]=b;return n*4;
    }
    if(s->kind==4) {
        if(!branch_word(at+n*4,native_target(pc,s->word,26),&b))return 0;
        out[n++]=b;return n*4;
    }
    out[n++]=s->word;
    if(s->kind==5)return n*4;
    if(!branch_word(at+n*4,pc+4,&b))return 0;
    out[n++]=b;return n*4;
}
