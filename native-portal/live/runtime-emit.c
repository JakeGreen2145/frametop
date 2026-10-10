/* SPDX-License-Identifier: MIT
 * Pinned call/return/ADRP islands, including the merged camera boundary group.
 * Not a generic
 * skip emitter: BL replacement resumes PC+4, method replacement returns native
 * x30. The loader must validate runtime-sites.json's complete original guard.
 */
#include <stdint.h>
#include <stddef.h>
static void imm(uint32_t *o,size_t *n,unsigned r,uint64_t v) {
    for(unsigned h=0;h<4;++h)o[(*n)++]=(h?0xf2800000u:0xd2800000u)|(h<<21)|(((v>>(h*16))&65535)<<5)|r;
}
static int branch(uint32_t *o,size_t *n,uint64_t at,uint64_t to) {
    int64_t delta=(int64_t)to-(int64_t)(at+*n*4);
    if((to|at)&3 || delta<-(INT64_C(1)<<27) || delta>=(INT64_C(1)<<27))return 0;
    o[(*n)++]=0x14000000u|((uint32_t)(delta/4)&0x3ffffffu);return 1;
}
static void restore(uint32_t *o,size_t *n) {
    o[(*n)++]=0xf94003e9|(96<<10);o[(*n)++]=0xd51b4209;
    o[(*n)++]=0xf94003e9|(97<<10);o[(*n)++]=0xd51b4409;
    o[(*n)++]=0xf94003e9|(98<<10);o[(*n)++]=0xd51b4429;
    for(unsigned r=0;r<32;r+=2)o[(*n)++]=0xad400000u|((16+r)<<15)|((r+1)<<10)|(31<<5)|r;
    for(unsigned r=0;r<30;r+=2)o[(*n)++]=0xa9400000u|(r<<15)|((r+1)<<10)|(31<<5)|r;
    o[(*n)++]=0xf94003fe|(30<<10);o[(*n)++]=0x910003ff|(800<<10);
}
size_t ft_emit_runtime_island(uint32_t *out,uint64_t at,uint64_t base,uint64_t pc,uint64_t callback) {
    uint32_t word=0;int method=0,adrp=0,plain=0;
    switch(pc) {
    /* The exact instruction words are validated against the pinned ELF in tests. */
    case 0x9eeb0:word=0x9403033e;break;
    case 0xa050c:word=0x9402fd35;break;
    case 0x161ed8:word=0xd63f0080;break;
    case 0x29f200:word=0x97fffa30;break;
    case 0x15f9e0:word=0xa9be7bfd;method=1;break;
    case 0x15fba8:word=0xa9be7bfd;method=1;break;
    case 0x4b3d0:word=0x90002b30;adrp=1;break;
    case 0x142708:word=0xf0001821;adrp=1;break;
    case 0x8dc1c:word=0x910cc000;plain=1;break;
    case 0x15e1c8:word=0xa9be7bfd;method=1;break;
    case 0x15df70:word=0xa9be7bfd;method=1;break;
    case 0x8b020:word=0xa9ba7bfd;method=1;break;
    case 0x162360:word=0xa9b87bfd;method=1;break;
    case 0x6d6e0:word=0x91408000;method=1;break;
    case 0x6d6f0:word=0x91408000;method=1;break;
    case 0x1366b0:word=0xa9be7bfd;method=1;break;
    case 0x136118:word=0xa9be7bfd;method=1;break;
    case 0x13a100:word=0xa9b27bfd;method=1;break;
    case 0x15d730:word=0xa9b97bfd;method=1;break;
    case 0x15d198:word=0xf9400401;method=1;break;
    case 0x15d248:word=0xa9be7bfd;method=1;break;
    case 0x15f938:word=0xa9bc7bfd;method=1;break;
    case 0x15faf8:word=0xa9bc7bfd;method=1;break;
    case 0x15fcc0:word=0xd10d83ff;method=1;break;
    default:return 0;
    }
    if(!out || !callback || (at&3) || (base&3))return 0;
    size_t n=0;
    out[n++]=0xd10003ff|(800<<10);
    for(unsigned r=0;r<30;r+=2)out[n++]=0xa9000000u|(r<<15)|((r+1)<<10)|(31<<5)|r;
    out[n++]=0xf90003fe|(30<<10);out[n++]=0x910003e9|(800<<10);out[n++]=0xf90003e9|(31<<10);
    for(unsigned r=0;r<32;r+=2)out[n++]=0xad000000u|((16+r)<<15)|((r+1)<<10)|(31<<5)|r;
    out[n++]=0xd53b4209;out[n++]=0xf90003e9|(96<<10);
    out[n++]=0xd53b4409;out[n++]=0xf90003e9|(97<<10);
    out[n++]=0xd53b4429;out[n++]=0xf90003e9|(98<<10);
    out[n++]=0xf90003ff|(99<<10); /* selector defaults to original, always */
    imm(out,&n,0,pc);out[n++]=0x910003e1;imm(out,&n,16,callback);out[n++]=0xd63f0200;
    out[n++]=0xf94003e9|(99<<10);const size_t original=n++; /* cbz x9 */
    out[n++]=0xf100093f;const size_t invalid=n++; /* cmp x9,2 ; b.ne */
    restore(out,&n);
    if(adrp || plain)out[n++]=0xd4200000;else if(method)out[n++]=0xd65f03c0;else if(!branch(out,&n,at,base+pc+4))return 0;
    const size_t stock=n;restore(out,&n);
    if(adrp) {
        int64_t page=((word>>29)&3)|(((word>>5)&0x7ffff)<<2);
        if(page&(INT64_C(1)<<20))page-=INT64_C(1)<<21;
        imm(out,&n,word&31,((base+pc)&~UINT64_C(4095))+page*4096);
        if(!branch(out,&n,at,base+pc+4))return 0;
    } else if(method || plain) {out[n++]=word;if(!branch(out,&n,at,base+pc+4))return 0;}
    else {
        imm(out,&n,30,base+pc+4);
        if(pc==0x161ed8)out[n++]=0xd61f0080; /* displaced BLR x4 -> BR x4 */
        else {int64_t d=word&0x3ffffffu;if(d&(INT64_C(1)<<25))d-=INT64_C(1)<<26;
            if(!branch(out,&n,at,base+pc+d*4))return 0;}
    }
    const size_t fatal=n;out[n++]=0xd4200000; /* invalid selector cannot fall through */
    out[original]=0xb4000009u|((uint32_t)(stock-original)<<5);
    out[invalid]=0x54000001u|((uint32_t)(fatal-invalid)<<5);
    return n*4;
}

/* Invoke the complete original cleanup synchronously without reentering its
 * patched entry. The caller uses the ordinary native call transport with this
 * address as target. The native epilogue returns to that transport's LR.
 * Installed only with the complete validated runtime-sites.json group.
 */
size_t ft_emit_camera_cleanup_bypass(uint32_t *out,uint64_t at,uint64_t base,uint64_t pc) {
    if(!out || (at&3) || (base&3))return 0;
    uint32_t word=0;
    switch(pc) {
    case 0x8b020:word=0xa9ba7bfd;break;
    case 0x15e1c8:word=0xa9be7bfd;break;
    case 0x15df70:word=0xa9be7bfd;break;
    case 0x162360:word=0xa9b87bfd;break;
    case 0x6d6e0:word=0x91408000;break;
    case 0x6d6f0:word=0x91408000;break;
    case 0x1366b0:word=0xa9be7bfd;break;
    case 0x136118:word=0xa9be7bfd;break;
    case 0x13a100:word=0xa9b27bfd;break;
    case 0x15d730:word=0xa9b97bfd;break;
    case 0x15d198:word=0xf9400401;break;
    case 0x15d248:word=0xa9be7bfd;break;
    case 0x15f938:word=0xa9bc7bfd;break;
    case 0x15faf8:word=0xa9bc7bfd;break;
    case 0x15fcc0:word=0xd10d83ff;break;
    case 0x15f9e0:word=0xa9be7bfd;break;
    case 0x15fba8:word=0xa9be7bfd;break;
    default:return 0;
    }
    size_t n=0;out[n++]=word;
    if(!branch(out,&n,at,base+pc+4))return 0;
    return n*4;
}
