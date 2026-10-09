// SPDX-License-Identifier: MIT
#pragma once
#include "../control/policy.hpp"
#include "../arena/bridge.hpp"
#include <array>
#include <cstring>
namespace native_portal::live {
// One pinned 112a40 invocation, sampled before its first shared feedback write.
// Missing values are usable only when that invocation's native branch bypassed
// the load. They are not observations of the underlying settings/mailboxes.
struct PolicySample {
    std::array<uint64_t,14> values{};
    uint64_t seen=0,sp=0,frame=0,owner=0;
    bool conflict=false,finished=false,forced_seen=false;
    void tap(uint64_t pc,const arena::Saved &s) noexcept {
        auto add=[&](unsigned i,uint64_t v) {
            if(!seen) {sp=s.sp;frame=s.x[19];owner=s.x[20];}
            if(finished || sp!=s.sp || frame!=s.x[19] || owner!=s.x[20] || (seen&(1ull<<i)))conflict=true;
            seen|=1ull<<i;values[i]=v;
        };
        uint32_t scalar=0;std::memcpy(&scalar,s.q[0],4);
        switch(pc) {
        case 0x112ab4:add(0,uint32_t(s.x[3]));break;
        case 0x112aec:add(1,uint32_t(s.x[1]));add(2,s.x[2]);break;
        case 0x112af4:add(3,scalar);break;
        case 0x112afc:add(4,0);break;case 0x112c70:add(4,1);break;
        case 0x112c10:add(5,uint32_t(s.x[1]));break;case 0x112c78:add(6,uint32_t(s.x[0]));break;
        case 0x112ac4:add(7,uint32_t(s.x[0]));break;case 0x112b10:add(8,uint32_t(s.x[0]));break;
        case 0x112c50:add(9,uint32_t(s.x[1]));break;case 0x112d14:add(10,uint32_t(s.x[23]));break;
        case 0x112cfc:add(11,uint8_t(s.x[0]));break;case 0x112d2c:add(12,scalar);break;
        case 0x112d74:case 0x112d90:{uint32_t v=0;std::memcpy(&v,s.q[pc==0x112d74?2:1],4);add(13,v);break;}
        default:break;
        }
    }
    PolicyInput finish(const arena::Saved &s,bool p0,bool p1) noexcept {
        if(finished || (seen && (sp!=s.sp || frame!=s.x[19] || owner!=s.x[20])))conflict=true;
        finished=true;
        PolicyInput p{};p.style=uint32_t(values[0]);p.enabled=values[1]==1;
        p.special=values[4]!=0;p.request=values[5]!=0;p.construct=values[6]!=0;
        uint64_t forced=0;
        for(unsigned k=7;k<=10;++k)if(seen&(1ull<<k)) {
            if(forced_seen && forced!=values[k])conflict=true;
            forced_seen=true;forced=values[k];
        }
        p.forced=forced!=0;p.suppression=values[11]!=0;p.p0=p0;p.p1=p1;
        const uint32_t base=uint32_t(values[12]),gain=uint32_t(values[13]);
        std::memcpy(&p.base,&base,4);std::memcpy(&p.gain,&gain,4);return p;
    }
    bool complete(const PolicyInput &p) const noexcept {
        // Initial settings and actual selected branch are unconditional.
        uint64_t required=(1ull<<0)|(1ull<<1)|(1ull<<2)|(1ull<<4)|(1ull<<7);
        if(!values[2])required|=1ull<<3; // null special object consumes scalar
        const bool native_enabled=values[1]!=0; // native CBNZ, not a default
        if(native_enabled)required|=1ull<<(p.special?6:5);
        // 112afc with enable==0 branches past 112c10. Request is then irrelevant
        // to BOTH pure_policy outputs; frame_policy still refuses disabled input.
        const bool construct=native_enabled&&(p.special?p.construct:p.request);
        const bool demand=construct||(p.p1&&p.p0);
        if(!construct)required|=1ull<<8;
        if(!p.p1)required|=1ull<<9;
        if(!demand) {
            required|=1ull<<11;
            if(!p.suppression)required|=1ull<<10;
        }
        if(!p.p1&&!p.forced&&!construct) {
            required|=1ull<<12;
            const auto style=p.style?p.style:1;
            if(style>=1&&style<=3)required|=1ull<<13;
        }
        return (seen&required)==required;
    }
};
}
