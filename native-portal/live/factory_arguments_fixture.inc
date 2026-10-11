// SPDX-License-Identifier: MIT
// Relocated Desktop-executed arguments; surrounding native bodies are models.
#include "factory-arguments.private.hpp"
static const FactorySaved *argument_vector=nullptr;
static int ancestry_case=-1;
static arena::Saved relocate_arguments(const FactorySaved &v,Chain &f,uint64_t sp,
                                       uint64_t pool,uint64_t wrapper,uint64_t owner) {
    arena::Saved s{};s.sp=sp;
    auto move=[&](uint64_t x) -> uint64_t {
        if(x==v.p)return pool;
        if(x>=v.r && x<v.r+0x1700)return addr(f.r)+(x-v.r);
        if(x>=v.w && x<v.w+0x200)return wrapper+(x-v.w);
        if(x>=v.a && x<v.a+0x40)return owner+(x-v.a);
        if(x>=v.sp-0x1000 && x<v.sp+0x1000)return sp+(x-v.sp);
        if(x>=v.base && x<v.base+0x600000)return Chain::base+(x-v.base);
        assert(false && "unclassified proven argument relocation");return 0;
    };
    for(unsigned i=0;i<31;++i)s.x[i]=(v.known&(1u<<i))?move(v.x[i]):v.x[i];
    // These ABI lanes were not captured by Desktop. Seed them separately and
    // require byte-for-byte preservation, never treat them as native evidence.
    s.pad=0x55;s.nzcv=0xa0000000;
    return s;
}
