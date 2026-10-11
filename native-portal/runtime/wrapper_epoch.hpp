// SPDX-License-Identifier: MIT
#pragma once
#include "../arena/bridge.hpp"
namespace native_portal::runtime {
// Scalar-only selected-wrapper storage lifetime. This is deliberately separate
// from Command/Lifecycle generations and never certifies GPU/CPU/source debt.
// Owner/process/renderer lifetime and accepted turnover are checked by callers.
class WrapperEpoch {
 public:
    bool seed(uint64_t w) noexcept {
        if(epoch_ || !w)return false;
        current_=w;epoch_=1;return true;
    }
    bool begin(uint64_t base,uint64_t r,uint64_t w,uint64_t c,uint64_t p,uint64_t sp) noexcept {
        if(phase_ || !epoch_ || current_!=w || !c || !p || sp<0xe0 || (sp&15) || sp>UINT64_MAX-0xa0)return false;
        base_=base;r_=r;w_=w;c_=c;p_=p;sp_=sp;phase_=1;return true;
    }
    bool active() const noexcept {return phase_!=0;}
    uint64_t current() const noexcept {return current_;}
    uint64_t factory() const noexcept {return phase_>=7?next_:0;}
    bool dead(uint64_t p) const noexcept {return p && p==dead_;}
    // Only these callbacks belong to the already accepted selected cleanup and
    // successor. Other global callbacks retain their existing classification.
    static bool site(uint64_t pc) noexcept {
        switch(pc) {
        case 0x3052e0:case 0x3052ec:case 0x311648:case 0x3052f0:
        case 0x3053f8:case 0x305308:case 0x302790:case 0x304e60:
        case 0x304ec8:case 0x304ecc:case 0x3053a8:case 0x305490:
        case 0x305470:case 0x2a23f8:return true;
        default:return false;
        }
    }
    bool matches(uint64_t pc,const arena::Saved& s) const noexcept {
        // Unrelated pool returns must still run their ordinary ledger reducer.
        if(pc==0x311648)return s.x[1]==c_ || s.sp==sp_-0x30;
        return true;
    }
    bool retire(const arena::Saved& s) noexcept {
        if(phase_!=5 || s.sp!=sp_ || s.x[30]!=base_+0x2a23c0 ||
           s.x[19]!=r_ || s.x[20]!=w_ || s.x[0]!=w_ || s.x[1]!=0x10 || current_!=w_)return false;
        dead_=w_;dead_epoch_=epoch_;current_=0;phase_=6;return true;
    }
    bool event(uint64_t pc,const arena::Saved& s) noexcept {
        const auto lr=s.x[30];
        switch(pc) {
        case 0x3052e0:
            if(phase_!=1 || s.sp!=sp_-0x30 || s.x[19]!=w_ || s.x[1]!=c_ ||
               (lr!=base_+0x2a23b4 && lr!=base_+0x30528c && lr!=base_+0x3052c4 && lr!=base_+0x3052dc))return false;
            load_lr_=lr;phase_=2;return true;
        case 0x3052ec:
            if(phase_!=2 || s.sp!=sp_-0x30 || s.x[19]!=w_ || s.x[1]!=c_ || s.x[0]!=p_+0xd0 || lr!=load_lr_)return false;
            phase_=3;return true;
        case 0x311648:
            if(phase_!=3 || s.sp!=sp_-0x30 || s.x[19]!=w_ || s.x[1]!=c_ || s.x[0]!=p_+0xd0 || lr!=base_+0x3052f0)return false;
            phase_=4;return true;
        case 0x3052f0:
            if(phase_!=4 || s.sp!=sp_-0x30 || s.x[19]!=w_ || lr!=base_+0x3052f0 || (s.x[0]&255)!=1)return false;
            phase_=5;return true;
        case 0x3053f8:
            if(phase_!=6 || s.sp!=sp_ || lr!=base_+0x2a23e8 || s.x[19]!=r_ ||
               !s.x[0] || s.x[20]!=s.x[0] || s.x[8]!=sp_+0x98)return false;
            a_=s.x[0];name_=s.x[1];phase_=7;next_=0;return true;
        case 0x305308:
            if(phase_!=7 || s.sp!=sp_-0x30 || lr!=base_+0x30548c || s.x[1]!=a_ ||
               s.x[2]!=name_ || !s.x[0] || s.x[0]>UINT64_MAX-0x10 || epoch_==UINT64_MAX)return false;
            next_=s.x[0];++epoch_;phase_=8;return true;
        case 0x302790:
            if(phase_!=8 || s.sp!=sp_-0x50 || lr!=base_+0x305330 || s.x[8]!=next_+8 ||
               s.x[19]!=next_ || s.x[3]!=a_ || s.x[1]!=name_ || !s.x[0] || s.x[0]>UINT64_MAX-0xd0)return false;
            next_pool_=s.x[0];phase_=9;return true;
        case 0x304e60:
            if(phase_!=9 || s.sp!=sp_-0x80 || lr!=base_+0x3027b0 || s.x[0]!=next_pool_ || s.x[8]!=next_+8)return false;
            phase_=10;return true;
        case 0x304ec8:
            if(phase_!=10 || s.sp!=sp_-0xe0 || s.x[19]!=next_pool_ || s.x[21]!=next_+8 || !s.x[22])return false;
            next_c_=s.x[22];store_lr_=lr;phase_=11;return true;
        case 0x304ecc:
            if(phase_!=11 || s.sp!=sp_-0xe0 || s.x[19]!=next_pool_ || s.x[21]!=next_+8 || s.x[22]!=next_c_ || lr!=store_lr_)return false;
            phase_=12;return true;
        case 0x3053a8:
            if(phase_!=12 || s.sp!=sp_-0x30 || lr!=base_+0x30548c)return false;
            phase_=13;return true;
        case 0x305490:
            if(phase_!=13 || s.sp!=sp_-0x30 || lr!=base_+0x30548c || s.x[20]!=sp_+0x98 || s.x[22]!=next_)return false;
            phase_=14;return true;
        case 0x305470:
            if(phase_!=14 || s.sp!=sp_ || lr!=base_+0x2a23e8 || s.x[0]!=sp_+0x98)return false;
            phase_=15;return true;
        case 0x2a23f8:
            if(phase_!=15 || s.sp!=sp_ || lr!=base_+0x2a23e8 || s.x[19]!=r_ || s.x[20] ||
               s.x[0]!=next_ || epoch_<=dead_epoch_)return false;
            current_=next_;phase_=0;return true;
        default:return false;
        }
    }
    uint64_t command() const noexcept {return next_c_;}
    uint64_t pool() const noexcept {return next_pool_;}
 private:
    unsigned phase_=0;
    uint64_t current_=0,epoch_=0,dead_=0,dead_epoch_=0;
    uint64_t base_=0,r_=0,w_=0,c_=0,p_=0,sp_=0,load_lr_=0;
    uint64_t next_=0,a_=0,name_=0,next_pool_=0,next_c_=0,store_lr_=0;
};
}
