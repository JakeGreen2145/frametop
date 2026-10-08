#include "control.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>
namespace native_portal {
uint64_t boottime() { timespec ts{}; if(clock_gettime(CLOCK_BOOTTIME,&ts))return 0; return uint64_t(ts.tv_sec)*1000000000+uint64_t(ts.tv_nsec); }
static uint32_t u32(const uint8_t* p) { uint32_t v=0; for(int i=3;i>=0;--i)v=(v<<8)|p[i]; return v; }
static uint64_t u64(const uint8_t* p) { return uint64_t(u32(p))|(uint64_t(u32(p+4))<<32); }
static void put(uint8_t* p,uint64_t v,int bytes) { for(int i=0;i<bytes;++i) { p[i]=v&255; v>>=8; } }
static float f32(const uint8_t* p) { auto v=u32(p); float f; std::memcpy(&f,&v,4); return f; }
static void putf(uint8_t* p,float f) { uint32_t v; std::memcpy(&v,&f,4); put(p,v,4); }
static Wire header(const char* magic) { Wire w{}; std::memcpy(w.data(),magic,8); put(w.data()+8,1,4); put(w.data()+12,128,4); return w; }
static bool header_ok(const void* p,size_t n,const char* magic) { auto b=static_cast<const uint8_t*>(p); return n==128&&!std::memcmp(b,magic,8)&&u32(b+8)==1&&u32(b+12)==128; }
bool valid(const Geometry& g) {
    for(float c:g.center)if(!std::isfinite(c)||std::abs(c)>100)return false;
    for(float s:g.size)if(!std::isfinite(s)||s<.05f||s>5)return false;
    double norm=0; for(float q:g.quaternion) { if(!std::isfinite(q))return false; norm+=double(q)*q; }
    return std::abs(norm-1)<=.0001 && std::isfinite(g.opacity)&&g.opacity>=0&&g.opacity<=1;
}
uint64_t deadline(uint64_t sent,uint64_t receipt,uint32_t ttl) {
    uint64_t span=uint64_t(ttl)*1000000;
    if(!sent||!receipt||!ttl||ttl>250||sent>receipt||sent>UINT64_MAX-span||receipt>UINT64_MAX-span)return 0;
    uint64_t d=std::min(sent+span,receipt+span); return d>receipt?d:0;
}
Wire encode(const Request& q) {
    Wire w=header("FTNPORT1"); auto p=w.data();
    put(p+16,uint32_t(q.op),4); put(p+20,q.flags,4); put(p+24,q.epoch,8); put(p+32,q.sequence,8); put(p+40,q.generation,8);
    put(p+48,q.ttl_ms,4); put(p+56,q.origin,8);
    for(int i=0;i<3;++i) { putf(p+64+4*i,q.geometry.center[i]); putf(p+76+4*i,q.geometry.size[i]); }
    for(int i=0;i<4;++i)putf(p+88+4*i,q.geometry.quaternion[i]);
    putf(p+104,q.geometry.opacity); put(p+112,q.sent_ns,8); return w;
}
bool decode(const void* data,size_t n,Request& q) {
    if(!header_ok(data,n,"FTNPORT1"))return false;
    auto p=static_cast<const uint8_t*>(data); auto op=u32(p+16);
    if(op<1||op>4||u32(p+20)>7||u32(p+52)||u32(p+108)||u64(p+120))return false;
    q.op=Operation(op); q.flags=u32(p+20); q.epoch=u64(p+24); q.sequence=u64(p+32); q.generation=u64(p+40);
    q.ttl_ms=u32(p+48); q.origin=u64(p+56); q.sent_ns=u64(p+112);
    for(int i=0;i<3;++i) { q.geometry.center[i]=f32(p+64+4*i); q.geometry.size[i]=f32(p+76+4*i); }
    for(int i=0;i<4;++i)q.geometry.quaternion[i]=f32(p+88+4*i);
    q.geometry.opacity=f32(p+104);
    return q.sequence && q.ttl_ms>=1&&q.ttl_ms<=250&&valid(q.geometry);
}
Wire encode(const Reply& r) {
    Wire w=header("FTNSTAT1"); auto p=w.data(); put(p+16,uint32_t(r.state),4); put(p+20,uint32_t(r.reason),4);
    put(p+24,r.epoch,8); put(p+32,r.sequence,8); put(p+40,r.generation,8); put(p+48,r.origin,8);
    put(p+56,r.submitted_frame,8); put(p+64,r.instance,8); put(p+72,r.capability,4); return w;
}
bool decode(const void* data,size_t n,Reply& r) {
    if(!header_ok(data,n,"FTNSTAT1"))return false;
    auto p=static_cast<const uint8_t*>(data);
    if(u32(p+16)>uint32_t(State::Fault)||u32(p+20)>uint32_t(Reason::Off)||u32(p+72)>1)return false;
    for(int i=76;i<128;++i)if(p[i])return false;
    r.state=State(u32(p+16)); r.reason=Reason(u32(p+20)); r.epoch=u64(p+24); r.sequence=u64(p+32); r.generation=u64(p+40);
    r.origin=u64(p+48); r.submitted_frame=u64(p+56); r.instance=u64(p+64); r.capability=u32(p+72); return true;
}
const char* name(State s) { static const char* names[]={"unavailable","off","warming","active-submitted","suspended-external","suppressed-menu","draining","fault"}; auto i=uint32_t(s); return i<8?names[i]:"fault"; }
const char* name(Reason r) { static const char* names[]={"none","missing-bridge","unsupported-backend","busy","invalid-packet","replay","epoch","expired","origin-changed","owner-gone","external-consumer","tracking-invalid","menu","native-fault","callback-concurrency","off"}; auto i=uint32_t(r); return i<16?names[i]:"invalid-packet"; }
bool Backend::lease(Lease& out,uint64_t now) const {
    BackendStatus b; Lease l;
    if(!now||!status(b)||!b.capable||!b.serialized||!b.external_clear||b.fault||!b.origin||!lease_.read(l)||!l.epoch||!l.generation||!(l.flags&1)||(l.flags&2)||now>=l.deadline_ns||l.origin!=b.origin||l.revocation!=revocation())return false;
    out=l; return true;
}
}
