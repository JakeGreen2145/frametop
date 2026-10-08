// SPDX-License-Identifier: MIT
// Test-only C ABI to compare compiled production extraction with Unicorn memory.
#include "pass.hpp"
#include <cstring>
using namespace native_portal::render;
using namespace native_portal::binding;
namespace {
struct Memory { const unsigned char *bytes; size_t size; Address base; };
bool read(void *u,Address a,void *p,size_t n) noexcept {
    auto &m=*static_cast<Memory*>(u);
    if(a<m.base || a-m.base>m.size || n>m.size-(a-m.base)) return false;
    std::memcpy(p,m.bytes+(a-m.base),n); return true;
}
bool live(void *,const Context &) noexcept { return true; }
}
extern "C" int pass_extract_test(const unsigned char *bytes,size_t size,Address base,
                                 Address renderer,Address marker,uint32_t mode,
                                 uint64_t *result) {
    Memory m{bytes,size,base};
    PassRequest r{{{1,renderer,1,2,1},7},marker,0,mode};
    const auto s=extract_passes({&m,read,live},r);
    result[0]=s.pair.complete_scan; result[1]=s.owned;
    for(unsigned i=0;i<2;++i) {
        result[2+i*5]=s.pair.eyes[i].pass;
        for(unsigned j=0;j<4;++j) result[3+i*5+j]=s.pair.eyes[i].lookup.x[j];
    }
    return int(s.reason);
}
