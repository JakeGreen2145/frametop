// SPDX-License-Identifier: MIT
#pragma once
#include "stock_native_facts.hpp"
#include <sys/uio.h>
namespace native_portal::live {
// This owns the read permission, not the backend. It uses the SAME production
// engine's pending-call coordinates rather than a second queue-observation engine.
// Trusted loader pins + same-original provider lifetime are begin's external
// prerequisites. Retained camera tickets alone never establish provider lifetime.
class NativeStockReadBoundary final {
public:
    using M=ClientSourceBinding::Module;
    using C=ClientSourceBinding::NativeReadContext;
    struct Site {M module;uint64_t pc;};
    inline static constexpr Site required_sites[]={
#define COMP(pc) {M::Compositor,pc}
#define CLIENT(pc) {M::Client,pc}
        COMP(0x15f9e0),COMP(0x15fba8),COMP(0x15fcc0),COMP(0x15fbb8),
        COMP(0x15fa78),COMP(0x15faa4),COMP(0x15fbd8),COMP(0x15fc20),COMP(0x15fdc8),
        COMP(0x15fa24),COMP(0x15fa28),COMP(0x15fa44),COMP(0x15fa48),COMP(0x15fae8),COMP(0x15faec),
        COMP(0x15fbec),COMP(0x15fbf0),COMP(0x15fc34),COMP(0x15fc38),
        COMP(0x15fee4),COMP(0x15fee8),COMP(0x160164),COMP(0x160168),COMP(0x160184),COMP(0x160188),
        COMP(0x160380),COMP(0x160384),COMP(0x1605c4),COMP(0x1605c8),
        COMP(0x15f9fc),COMP(0x15fa60),COMP(0x15fa94),COMP(0x15fbc4),COMP(0x15fd30),
        COMP(0x15fa04),COMP(0x15fa8c),COMP(0x15fabc),COMP(0x15fbc0),COMP(0x15fc4c),COMP(0x15fd48),COMP(0x15fd90),
        COMP(0x15fbd0),COMP(0x15fc08),COMP(0x16013c),
        CLIENT(0x32a270),CLIENT(0x32a398),CLIENT(0x32a39c),CLIENT(0x32a49c),CLIENT(0x32a504),CLIENT(0x32a628),
        CLIENT(0x32750c),CLIENT(0x327e54),CLIENT(0x3280a8),CLIENT(0x32813c),CLIENT(0x328140),
        CLIENT(0x327640),CLIENT(0x3276c4),CLIENT(0x3276c8),CLIENT(0x327e18),CLIENT(0x327f2c),
        CLIENT(0x327f30),CLIENT(0x327f70),CLIENT(0x327f74),CLIENT(0x3274d8),CLIENT(0x32758c),
        CLIENT(0x327590),CLIENT(0x3275a0),CLIENT(0x3275a4),CLIENT(0x3280ec),CLIENT(0x327678),
        CLIENT(0x328150),CLIENT(0x3276d8),CLIENT(0x258438),CLIENT(0x2584c4),CLIENT(0x258508)
#undef COMP
#undef CLIENT
    };
    explicit NativeStockReadBoundary(CameraAccessGate& gate,StockSourceReadBoundary backend) noexcept:
        gate_(gate),backend_(backend),process_(getpid()),caller_(std::this_thread::get_id()) {}
    bool begin(const StockSourceReadScope& scope,uint64_t client) noexcept {
        if(!identity(scope) || active_ || !client || !backend_.covered || !backend_.provider_live)return false;
        for(const auto& site:required_sites)if(!backend_.covered(backend_.context,site.module,site.pc))return false;
        binding_=scope.binding;ticket_=scope.ticket;version_=scope.version;sequence_=scope.sequence;
        runtime_=scope.runtime;method_=scope.method;entry_sp_=scope.saved.sp;entry_lr_=scope.saved.x[30];client_=client;
        active_=true;return true;
    }
    bool callback(const StockSourceReadScope& scope,const C& c) noexcept {
        if(!matches(scope) || callback_)return false;
        // Saved is borrowed only for this synchronous event. leave() always
        // revokes it, including every refusal path.
        callback_=&scope;context_=c;return true;
    }
    void leave() noexcept {callback_=nullptr;context_={};}
    void end() noexcept {leave();active_=false;manager_={};guard_={};}
    bool clean() const noexcept {return !guard_.begun && !manager_.locked;}
    bool observe(const StockSourceReadScope& scope,const C& c) noexcept {
        if(!matches(scope) || scope.module!=M::Client || scope.method!=0x15fcc0 || !c.pending || !c.entered)return false;
        const auto& s=scope.saved;const uint64_t pc=scope.pc;
        const uint64_t mf=c.releasing?0x40:0x50,inf=c.releasing?0x60:0x70;
        if(c.sp<mf+inf || c.sp!=entry_sp_-0x360)return false;
        const uint64_t inner=c.sp-mf-inf;
        if(pc==(c.releasing?0x327678:0x3280ec)) {
            if(manager_.locked || guard_.begun || s.sp!=c.sp-mf || s.x[30]!=client_+pc || uint32_t(s.x[0]))return false;
            manager_={true,c.releasing,c.queue,c.sp,c.id,c.provider};return true;
        }
        if(pc==(c.releasing?0x3276d8:0x328150)) {
            if(!manager_matches(c) || guard_.begun || s.sp!=c.sp-mf || s.x[0]!=c.provider+8)return false;
            manager_={};return true;
        }
        if(!manager_matches(c) || !c.inner_entered || !c.object)return false;
        const uint64_t address=inner+(c.releasing?0x48:0x58);
        if(pc==0x258438) {
            if(guard_.begun || s.sp!=inner || s.x[0]!=address || s.x[30]!=client_+(c.releasing?0x327504:0x327e4c))return false;
            guard_={true,false,false,c.releasing,c.queue,c.id,c.provider,c.object,inner,address};return true;
        }
        if(!guard_matches(c))return false;
        if(pc==0x2584c4) {
            if(guard_.result || s.sp!=inner-0x30 || s.x[19]!=address || s.x[30]!=client_+pc)return false;
            guard_.result=true;guard_.success=uint32_t(s.x[0])!=0;
            return true; // false bool revokes capability, never aborts native work
        }
        if(pc==0x2584f8) // optional observation: normal accessor RET does NOT unlock
            return s.sp==inner && s.x[30]==client_+(c.releasing?0x327504:0x327e4c);
        if(pc==0x258508) {
            const auto lr=s.x[30]-client_;
            const bool link=c.releasing?(lr==0x327558 || lr==0x3275cc):
                (lr==0x327e84 || lr==0x327f80 || lr==0x327fa4 || lr==0x327fd8);
            if(s.sp!=inner || s.x[0]!=address || !link)return false;
            guard_={};return true;
        }
        return false;
    }
    bool read(uint64_t address,void* out,size_t n) noexcept {
        if(!callback_ || !matches(*callback_) || !out || !address || !n || address>UINT64_MAX-n)return false;
        const auto& scope=*callback_;const auto& s=scope.saved;const auto& c=context_;const uint64_t pc=scope.pc;
        bool allowed=false;
        if(scope.module==M::Compositor) {
            uint64_t slot=0,target=0;
            switch(pc) {
            case 0x15fa24:case 0x15fa44:case 0x15fae8:slot=8;target=0x32a270;break;
            case 0x15fbec:case 0x15fc34:slot=16;target=0x328380;break;
            case 0x15fee4:case 0x160184:slot=0x30;target=0x3280a8;break;
            case 0x160164:case 0x160380:case 0x1605c4:slot=0x38;target=0x327640;break;
            default:break;
            }
            const bool method_match=slot==8?method_==0x15f9e0:slot==16?method_==0x15fba8:method_==0x15fcc0;
            if(slot && method_match && s.x[19]==binding_.camera && body_sp(s.sp) && n==8 &&
               (!c.provider || c.provider==s.x[0]) && s.x[slot==8?3:slot==16?2:slot==0x30?5:3]==client_+target &&
               backend_.provider_live(backend_.context,scope,s.x[0]))
                allowed=address==s.x[0] || address==client_+0x60f108+slot;
            if(method_==0x15fcc0 && (pc==0x15fee8 || pc==0x160188) && c.pending && !c.releasing && pc==c.return_pc &&
               s.sp==c.sp && body_sp(s.sp) && s.x[19]==binding_.camera && !uint32_t(s.x[0]) && n==8)
                allowed=address==c.sp+0x90 || address==c.sp+0x98;
        } else {
            if(pc==0x32a49c && method_==0x15f9e0 && c.connect_pending && c.connect_entered && c.new_path &&
               c.call_sp==entry_sp_-0x20 && s.sp==c.call_sp-0xc0 && s.x[20]==c.provider &&
               s.x[23]==c.slot && s.x[19]==c.alias && n==8 && address==s.x[0] &&
               backend_.provider_live(backend_.context,scope,c.provider))allowed=true;
            const uint64_t mf=c.releasing?0x40:0x50,inf=c.releasing?0x60:0x70;
            const bool inner=method_==0x15fcc0 && c.pending && c.inner_entered && c.sp==entry_sp_-0x360 &&
                c.sp>=mf+inf && s.sp==c.sp-mf-inf && s.x[c.releasing?21:20]==c.object;
            if(inner && pc==0x327f30 && !c.releasing && !uint32_t(s.x[0]) && n==8 && address==c.sp+0x90)allowed=true;
            if(inner && manager_matches(c) && guard_matches(c) && guard_.result && guard_.success) {
                if(pc==(c.releasing?0x32750c:0x327e54) && n==8 && address==s.x[0] && address<=UINT64_MAX-0x470)allowed=true;
                const uint64_t store=c.releasing?0x32758c:0x327f70;
                if((pc==store || pc==store+4) && n==4 && c.header && c.header<=UINT64_MAX-0x470 &&
                   address>=c.header+0x68 && address<c.header+0x468 && (address-c.header-0x68)%8==0) {
                    if(pc==store+4)allowed=address==c.ref;
                    else if(c.releasing)allowed=s.x[1]<=UINT64_MAX-s.x[24] && address==s.x[1]+s.x[24];
                    else allowed=s.x[19]<=UINT64_MAX-0x68 && address==s.x[19]+0x68;
                }
            }
        }
        if(!allowed || !matches(scope))return false;
        const bool ok=backend_.read?backend_.read(backend_.context,scope,address,out,n):read_self(address,out,n);
        return ok && matches(scope);
    }
private:
    bool body_sp(uint64_t sp) const noexcept {const auto frame=method_==0x15fcc0?0x360u:0x20u;return entry_sp_>=frame && sp==entry_sp_-frame;}
    bool identity(const StockSourceReadScope& s) const noexcept {
        CameraBinding b;
        return getpid()==process_ && std::this_thread::get_id()==caller_ && s.ticket && s.version && s.sequence && s.runtime &&
            gate_.lookup(s.ticket,b) && b==s.binding;
    }
    bool matches(const StockSourceReadScope& s) const noexcept {
        return active_ && identity(s) && s.binding==binding_ && s.ticket==ticket_ && s.version==version_ && s.sequence==sequence_ &&
            s.runtime==runtime_ && s.method==method_;
    }
    bool manager_matches(const C& c) const noexcept {return manager_.locked && manager_.release==c.releasing && manager_.q==c.queue &&
        manager_.sp==c.sp && manager_.id==c.id && manager_.provider==c.provider;}
    bool guard_matches(const C& c) const noexcept {return guard_.begun && guard_.release==c.releasing && guard_.q==c.queue &&
        guard_.id==c.id && guard_.provider==c.provider && guard_.object==c.object;}
    static bool read_self(uint64_t address,void* out,size_t n) noexcept {
        iovec local{out,n},remote{reinterpret_cast<void*>(address),n};
        return process_vm_readv(getpid(),&local,1,&remote,1,0)==static_cast<ssize_t>(n);
    }
    CameraAccessGate& gate_;StockSourceReadBoundary backend_;
    const int process_;const std::thread::id caller_;
    bool active_=false;CameraBinding binding_{};
    uint64_t ticket_=0,version_=0,sequence_=0,runtime_=0,method_=0,entry_sp_=0,entry_lr_=0,client_=0;
    const StockSourceReadScope* callback_=nullptr;C context_{};
    struct Manager {bool locked=false,release=false;unsigned q=0;uint64_t sp=0,id=0,provider=0;} manager_{};
    struct Guard {bool begun=false,result=false,success=false,release=false;unsigned q=0;
        uint64_t id=0,provider=0,object=0,sp=0,address=0;} guard_{};
};
}
