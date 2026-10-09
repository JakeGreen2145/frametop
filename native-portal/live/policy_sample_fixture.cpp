// SPDX-License-Identifier: MIT
#include "policy_sample.hpp"
#include <cstring>
using namespace native_portal;
using namespace native_portal::live;
static PolicySample sample;
static arena::Saved saved(const uint64_t *v) {
    arena::Saved s{};s.sp=v[0];s.x[19]=v[1];s.x[20]=v[2];
    s.x[0]=v[3];s.x[1]=v[4];s.x[2]=v[5];s.x[3]=v[6];s.x[23]=v[7];
    for(unsigned i=0;i<3;++i)std::memcpy(s.q[i],v+8+i,4);
    return s;
}
extern "C" void sample_reset() {sample={};}
extern "C" void sample_tap(uint64_t pc,const uint64_t *v) {sample.tap(pc,saved(v));}
extern "C" void sample_finish(const uint64_t *v,unsigned p0,unsigned p1,uint64_t *out) {
    const auto s=saved(v);const auto p=sample.finish(s,p0,p1);
    out[0]=sample.complete(p);out[1]=sample.conflict;out[2]=sample.seen;
    out[3]=p.enabled;out[4]=p.request;out[5]=p.construct;out[6]=p.special;out[7]=p.forced;
    const auto stock=pure_policy(p,false),owned=pure_policy(p,true);
    auto store=[&](unsigned i,const PolicyOutput &r){out[i]=r.construct;out[i+1]=r.demand;out[i+2]=r.style;uint32_t a;std::memcpy(&a,&r.alpha,4);out[i+3]=a;};
    store(8,stock);store(12,owned);
    out[16]=frame_policy(p,true,true,true,false).portal;
    uint32_t b,g;std::memcpy(&b,&p.base,4);std::memcpy(&g,&p.gain,4);out[17]=b;out[18]=g;
}
