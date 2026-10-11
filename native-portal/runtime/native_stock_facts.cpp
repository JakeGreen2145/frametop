// SPDX-License-Identifier: MIT
#include "native_stock_facts.hpp"
#include <sys/syscall.h>
#include <unistd.h>
namespace native_portal::runtime {
namespace {
constexpr uint64_t required[]={
    0x7fda0,0x29a528,0x7fdbc,0x301800,0x303d10,0x303d70,0x301804,0x301808,0x30180c,
    0x300a08,0x300a28,0x300a38,0x300b20,0x300c54,0x3003d0,0x4b3d0,0x311648,
    0x2a1e18,0x2a1e3c,0x2a2230,0x2a2320,0x305260,0x3052e0,0x3052ec,0x3052f0,
    0x3053f8,0x305308,0x302790,0x304e60,0x304ec8,0x304ecc,0x3053a8,0x305490,0x305470,0x2a23f8,
    0x2987d8,0x298840,0x29bd18,0x29c238,0x2953a0};
bool ptr(uint64_t p,uint64_t n) noexcept {return p && p<=UINT64_MAX-n;}
bool turnover_caller(uint64_t lr) noexcept {
    for(auto x:{0x6caf0u,0x9e6e0u,0x15a9bcu,0x15ad64u,0x294d10u,0x294e58u,0x2955a8u})if(lr==x)return true;
    return false;
}
bool return_caller(uint64_t lr) noexcept {
    for(auto x:{0x29798cu,0x297cb4u,0x297d40u,0x29a8b8u,0x29b080u,0x29cbe0u,0x29ccbcu})if(lr==x)return true;
    return false;
}
}
NativeStockFactCollector::NativeStockFactCollector() noexcept:pid_(getpid()),tid_(pid_t(syscall(SYS_gettid))) {}
const uint64_t *NativeStockFactCollector::required_sites(size_t &n) noexcept {n=sizeof(required)/sizeof(*required);return required;}
bool NativeStockFactCollector::owner() const noexcept {return getpid()==pid_ && syscall(SYS_gettid)==tid_;}
bool NativeStockFactCollector::deny(uint64_t pc,uint64_t detail) noexcept {
    const auto word=(pc?pc:0xffffff)|(detail&~uint64_t(0xffffff));
    uint64_t zero=0;failure_.compare_exchange_strong(zero,word);return false;
}
bool NativeStockFactCollector::install_coverage(const uint64_t *pcs,size_t n) noexcept {
    if(!owner() || coverage_ || base_ || !pcs || n>4096)return deny(0);
    for(auto pc:required) {bool found=false;for(size_t i=0;i<n;++i)found=found || pcs[i]==pc;if(!found)return deny(pc);}
    coverage_=true;return true;
}
Evidence::StockFactReader NativeStockFactCollector::reader() noexcept {return {this,scalar_cb,bootstrap_cb,complete_cb,cleanup_return_cb};}
bool NativeStockFactCollector::cleanup_return_cb(void *u,uint64_t pc,uint64_t c) noexcept {
    if(!u)return false;
    const auto &self=*static_cast<NativeStockFactCollector*>(u);
    return self.owner() && !self.failure_.load() && self.active_pc_==pc && pc==0x311648 &&
        self.observation_.kind==3 && self.observation_.phase==4 && self.observation_.c==c;
}
bool NativeStockFactCollector::scalar_cb(void *u,uint64_t pc,uint64_t a,uint64_t &v) noexcept {return u && static_cast<NativeStockFactCollector*>(u)->scalar(pc,a,v);}
bool NativeStockFactCollector::bootstrap_cb(void *u,uint64_t b,const render::Identity &i,uint64_t w,uint64_t c,uint64_t v) noexcept {return u && static_cast<NativeStockFactCollector*>(u)->bootstrap(b,i,w,c,v);}
bool NativeStockFactCollector::complete_cb(void *u) noexcept {return u && static_cast<NativeStockFactCollector*>(u)->complete();}
NativeStockFactCollector::Member *NativeStockFactCollector::member(uint64_t c) noexcept {
    if(c)for(auto &m:members_)if(m.c==c)return &m;
    return nullptr;
}
bool NativeStockFactCollector::field(uint64_t a,uint64_t v) noexcept {
    if(!ptr(a,8) || fields_count_==fields_.size())return deny(active_pc_);
    fields_[fields_count_++]={a,v};return true;
}
bool NativeStockFactCollector::command_fields(const Member &m) noexcept {
    return field(m.c+8,m.d) && field(m.c+0x50,m.p) && field(m.c+0x738,m.vk);
}
bool NativeStockFactCollector::scalar(uint64_t pc,uint64_t a,uint64_t &out) noexcept {
    if(!owner())return deny(pc);
    if(failure_.load() || !coverage_ || !active_pc_ || pc!=active_pc_)return deny(pc);
    for(size_t i=0;i<fields_count_;++i)if(fields_[i].address==a) {out=fields_[i].value;return true;}
    return deny(pc); // never read_self, Saved/stack loans, or late object reads
}
void NativeStockFactCollector::finish() noexcept {
    if(!owner()) {deny(0);return;}
    fields_={};fields_count_=0;active_pc_=0;active_={};unrelated_=false;
}
bool NativeStockFactCollector::bootstrap(uint64_t b,const render::Identity &i,uint64_t w,uint64_t c,uint64_t vk) noexcept {
    if(!owner() || !coverage_ || failure_.load() || i.process_instance!=uint64_t(pid_) || !ptr(i.renderer,0x16c0) || !i.renderer_generation || !ptr(b,0x600000))return deny(active_pc_);
    if(!base_) {
        if(active_pc_!=0x7fda0 || w || c || vk || i.device || i.device_generation)return deny(active_pc_);
        base_=b;identity_=i;return true;
    }
    if(b!=base_ || i.renderer!=identity_.renderer || i.renderer_generation!=identity_.renderer_generation || bootstrapped_)return deny(active_pc_);
    if(!i.device) {
        auto *m=member(c);
        if(active_pc_!=0x29a528 || current_w_ || !m || !m->made || !m->begun || m->vk!=vk || !ptr(w,0x10))return deny(active_pc_);
        // Existing qualified bootstrap W/C/Device/Vk borrow, not a new walk.
        if(!wrapper_epoch_.seed(w))return deny(active_pc_);
        current_w_=w;current_c_=c;return field(c+0x50,m->p);
    }
    if(active_pc_!=0x7fdbc || !i.device_generation || current_w_!=w || current_c_!=c || vk)return deny(active_pc_);
    identity_=i;bootstrapped_=true;return true;
}
bool NativeStockFactCollector::handoff(const Evidence &e) noexcept {
    if(getpid()!=pid_ || failure_.load() || !coverage_ || !bootstrapped_ || active_pc_ || observation_.kind || ctor_.phase || begin_.c || turn_.phase || factory_.phase || dispose_.phase)return deny(0);
    const auto i=e.identity();const auto f=e.stock_command_facts();
    if(i.process_instance!=identity_.process_instance || i.renderer!=identity_.renderer || i.renderer_generation!=identity_.renderer_generation || i.device!=identity_.device || i.device_generation!=identity_.device_generation || f.command!=current_c_ || f.wrapper!=current_w_ || !e.application_owner().worker)return deny(0);
    tid_=pid_t(syscall(SYS_gettid));return true;
}
bool NativeStockFactCollector::complete() const noexcept {
    if(!owner() || failure_.load() || !coverage_ || !bootstrapped_ || active_pc_ || observation_.kind || ctor_.phase || begin_.c || turn_.phase || factory_.phase || dispose_.phase || !current_c_ || !current_w_)return false;
    for(const auto &m:members_)if(m.reset)return false;
    return true;
}
int NativeStockFactCollector::observe_shared(uint64_t pc,const arena::Saved &s) noexcept {
    auto &o=observation_;
    const auto lr=s.x[30]-base_;
    auto owned_w=[&](uint64_t w) {return w && (w==current_w_ || w==turn_.w || w==factory_.w);};
    auto owned_slot=[&](uint64_t slot) {
        return slot && ((current_w_ && slot==current_w_+8) || (turn_.w && slot==turn_.w+8) ||
            (factory_.w && slot==factory_.w+8) || (factory_.phase && slot==factory_.slot));
    };
    // Any address in the selected native frame wins over disjoint-root matching.
    // This also catches malformed descendants whose subject or phase is wrong.
    const auto F=turn_.phase?turn_.entry.sp-0xd0:factory_.entry.sp;
    const bool owned_frame=(turn_.phase || factory_.phase || dispose_.phase) && F>=0xe0 && s.sp>=F-0xe0 && s.sp<=F;
    const bool shared=pc==0x3053f8 || pc==0x305308 || pc==0x302790 || pc==0x304e60 ||
        pc==0x304ec8 || pc==0x304ecc || pc==0x3053a8 || pc==0x305490 || pc==0x305470 ||
        pc==0x305260 || pc==0x3052e0 || pc==0x3052ec || pc==0x3052f0;
    // Dropping an observation receipt at a later caller/ancestor boundary grants
    // nothing. Until that boundary pre-RET is NOT a completed factory. Incomplete
    // scopes cannot be overwritten by a later invocation or permit engagement.
    const bool terminal=(o.kind==1 && o.phase==5) || (o.kind==2 && o.phase==8) || (o.kind==3 && o.phase==5);
    const bool root=pc==0x3053f8 || pc==0x305260 || pc==0x302790;
    if(o.kind==2 && (pc==0x2a23f8 || pc==0x29a528) && o.renderer!=identity_.renderer) {
        if(o.phase!=8 || s.sp!=o.entry.sp || s.x[30]!=o.entry.lr || s.x[19]!=o.renderer ||
           (pc==0x2a23f8?s.x[0]:s.x[1])!=o.w ||
           o.entry.lr!=base_+(pc==0x2a23f8?0x2a23e8:0x29a51c))return -1;
        o={};return 1; // paired other-renderer continuation, not a scalar answer
    }
    if(o.kind && s.sp>=o.entry.sp && (!shared || root)) {
        if(!terminal)return -1;
        o={};
    }
    if(shared && (owned_frame || (pc==0x305260 && owned_w(s.x[0])) ||
       ((pc==0x302790 || pc==0x304e60 || pc==0x3053f8) && owned_slot(s.x[8])) ||
       ((pc==0x304ec8 || pc==0x304ecc) && owned_slot(s.x[21])) ||
       (pc==0x305308 && owned_w(s.x[0])) ||
       ((pc==0x3052e0 || pc==0x3052ec || pc==0x3052f0) && owned_w(s.x[19]))))return 0;
    if(pc==0x302790 && !o.kind) {
        struct Root {uint64_t lr,slot,name;unsigned renderer;};
        constexpr Root roots[]={{0x297878,0x50,0x477ae8,20},{0x297b70,0x80,0x477c50,19},
            {0x29a464,0x88,0x4780c0,19},{0x29af28,0x80,0x478388,23},{0x29ca68,0xa8,0x4784a8,24}};
        // At LR 29a464 the name-page ADRP has already clobbered x24.
        // x0, not x24, carries the pool derived from the retained renderer.
        for(const auto &r:roots)if(lr==r.lr && ptr(s.x[r.renderer],0x12b0) &&
            s.x[0]==s.x[r.renderer]+0x11e0 && s.x[1]==base_+r.name &&
            s.sp>=0x90 && ptr(s.sp,r.slot+8) && s.x[8]==s.sp+r.slot) {
            o={};o.kind=1;o.phase=3;o.entry=active_;o.acquire=active_;
            o.p=s.x[0];o.slot=s.x[8];o.name=s.x[1];return 1;
        }
        return 0; // preserve the exact six selected diagnostic predicates
    }
    if(pc==0x3053f8 && !o.kind) {
        if(!ptr(s.x[0],0x40) || !ptr(s.x[19],0x16c0) || s.sp<0xe0 ||
           !ptr(s.sp,0xa0) || s.x[8]!=s.sp+0x98 ||
           (lr!=0x29a51c && !(lr==0x2a23e8 && s.x[19]!=identity_.renderer)))return 0;
        o={};o.kind=2;o.phase=1;o.entry=active_;o.owner=s.x[0];o.slot=s.x[8];o.name=s.x[1];o.renderer=s.x[19];return 1;
    }
    if(pc==0x305260 && !o.kind) {
        bool caller=false;for(auto x:{0x2953b4u,0x29c104u,0x29c54cu,0x2a23b4u,0x2a2404u,0x2a2420u})caller=caller || lr==x;
        if(!caller || !ptr(s.x[0],0x10) || s.sp<0x30)return 0;
        o={};o.kind=3;o.phase=1;o.entry=active_;o.w=s.x[0];return 1;
    }
    if(!o.kind)return 0;
    if(o.kind==3) {
        if(pc==0x3052e0) {
            if(o.phase!=1 || s.sp!=o.entry.sp-0x30 || s.x[19]!=o.w ||
               (s.x[1] && (s.x[1]==current_c_ || s.x[1]==turn_.c || s.x[1]==factory_.c)))return -1;
            o.c=s.x[1];o.phase=2;return 1;
        }
        if(pc==0x3052ec) {
            if(o.phase!=2 || !o.c || s.sp!=o.entry.sp-0x30 || s.x[19]!=o.w || s.x[1]!=o.c || s.x[0]<=0xd0)return -1;
            o.p=s.x[0]-0xd0;o.phase=3;return 1;
        }
        if(pc==0x311648) {
            if(o.phase!=3 || s.sp!=o.entry.sp-0x30 || s.x[19]!=o.w || s.x[1]!=o.c || lr!=0x3052f0 || s.x[0]!=o.p+0xd0)return -1;
            // Known members MUST enter the ordinary ledger reducer below.
            if(member(o.c))return 0;
            o.phase=4;return 1;
        }
        if(pc==0x3052f0) {
            if(s.sp!=o.entry.sp-0x30 || s.x[19]!=o.w)return -1;
            if(o.c) {
                if(o.phase!=4 || lr!=0x3052f0 || (s.x[0]&255)!=1)return -1;
                if(auto *m=member(o.c))m->returned=true;
            } else if(o.phase!=2)return -1; // native null bypass has no return value
            o.phase=5;return 1;
        }
        if(shared)return -1;
        return 0;
    }
    switch(pc) {
    case 0x305308:
        if(o.kind!=2 || o.phase!=1 || s.sp!=o.entry.sp-0x30 || lr!=0x30548c ||
           s.x[1]!=o.owner || s.x[2]!=o.name || !ptr(s.x[0],0x10))return -1;
        o.w=s.x[0];o.phase=2;return 1;
    case 0x302790:
        if(o.kind!=2 || o.phase!=2 || s.sp!=o.entry.sp-0x50 || lr!=0x305330 ||
           s.x[8]!=o.w+8 || s.x[1]!=o.name || !ptr(s.x[0],0xd0))return -1;
        o.p=s.x[0];o.acquire=active_;o.phase=3;return 1;
    case 0x304e60:
        if(o.phase!=3 || s.sp!=o.acquire.sp-0x30 || lr!=0x3027b0 ||
           s.x[8]!=(o.kind==1?o.slot:o.w+8) || s.x[0]!=o.p)return -1;
        o.pop=active_;o.phase=4;return 1;
    case 0x304ec8: {
        if(o.phase!=4 || s.sp!=o.pop.sp-0x60 || s.x[21]!=(o.kind==1?o.slot:o.w+8) ||
           s.x[19]!=o.p || !s.x[22] || s.x[22]==current_c_ || s.x[22]==turn_.c || s.x[22]==factory_.c)return -1;
        auto *m=member(s.x[22]);
        if(m && (!m->made || !m->begun || m->reset || m->p!=o.p))return -1;
        o.c=s.x[22];o.positive=active_;o.phase=9;return 1;
    }
    case 0x304ecc:
        if((o.phase!=4 && o.phase!=9) || s.sp!=o.pop.sp-0x60 ||
           s.x[21]!=(o.kind==1?o.slot:o.w+8) || s.x[19]!=o.p)return -1;
        if(o.phase==9 && (s.x[30]!=o.positive.lr || s.x[22]!=o.c))return -1;
        // Without the positive predecessor x22 is stale. No positive fact is
        // created, even when those bits happen to name a tracked command.
        o.phase=5;return 1;
    case 0x3053a8:
        if(o.kind!=2 || o.phase!=5 || s.sp!=o.entry.sp-0x30 || lr!=0x30548c)return -1;
        o.phase=6;return 1;
    case 0x305490:
        if(o.kind!=2 || o.phase!=6 || s.sp!=o.entry.sp-0x30 || lr!=0x30548c || s.x[20]!=o.slot || s.x[22]!=o.w)return -1;
        o.phase=7;return 1;
    case 0x305470:
        if(o.kind!=2 || (o.phase!=7 && o.phase!=1) || s.sp!=o.entry.sp ||
           s.x[30]!=o.entry.lr || s.x[0]!=o.slot)return -1;
        o.phase=8;return 1;
    default:return shared?-1:0;
    }
}
bool NativeStockFactCollector::capture(uint64_t pc,const arena::Saved &s) noexcept {
    if(!owner())return deny(pc);
    if(failure_.load() || !coverage_ || active_pc_)return deny(pc);
    active_pc_=pc;active_={pc,s.sp,s.x[30],uint64_t(tid_.load())};fields_count_=0;fields_={};unrelated_=false;
    // Before Evidence qualifies the renderer at 7fda0 there can be no known
    // member or constructor/Begin/turnover scope: all are created below this
    // gate. Global callbacks for unrelated lifetimes carry no facts for us.
    // Keep the synchronous PC scope for the qualified bootstrap, but grant no
    // fields, identity or readiness. Prior coverage/owner/scope failures above
    // remain sticky; once base_ exists every original guard below still applies.
    if(!base_)return true;
    const auto R=identity_.renderer;
    auto bad=[&] {return deny(pc);};
    bool obligation=ctor_.phase || begin_.c || turn_.phase || factory_.phase || dispose_.phase;
    for(const auto& m:members_)obligation=obligation || m.reset;
    auto unrelated=[&] {unrelated_=true;return true;};
    const auto observed=observe_shared(pc,s);
    if(observed<0) {
        if(pc==0x311648)if(auto *m=member(s.x[1]))m->begun=false;
        return bad();
    }
    if(observed>0)return unrelated();
    if(pc==0x305308 && (s.x[0]==R || member(s.x[0]) || s.x[0]==current_w_))return bad();
    if(pc!=0x302790 && wrapper_epoch_.active() && WrapperEpoch::site(pc) && wrapper_epoch_.matches(pc,s) &&
       !wrapper_epoch_.event(pc,s))return bad();
    // These hooks are process-global. Entry qualifies a new command by device;
    // descendants can only describe an existing owned invocation/member. Never
    // ask Evidence to read a missing scalar for an unrelated native invocation.
    if(!obligation) {
        switch(pc) {
        case 0x301800:
            if(!member(s.x[0]) && s.x[1]!=R+0x50)return unrelated();
            break;
        case 0x303d10:case 0x303d70:case 0x300a28:case 0x300a38:case 0x300c54:
            if(!member(s.x[19]))return unrelated();
            break;
        case 0x301804:case 0x30180c:
            if(!member(s.x[20]))return unrelated();
            break;
        case 0x301808:case 0x300a08:case 0x300b20:
            if(!member(s.x[0]))return unrelated();
            break;
        case 0x311648:
            if(!member(s.x[1]))return unrelated();
            break;
        default:break;
        }
    }
    switch(pc) {
    case 0x301800: {
        if(ctor_.phase || begin_.c || !ptr(s.x[0],0x7b0) || s.x[20]!=s.x[0] || s.x[1]!=R+0x50 || s.x[2]!=s.x[19] || !ptr(s.x[2],0xd0) || s.sp<0xa0 || member(s.x[0]))return bad();
        Member *free=nullptr;for(auto &m:members_)if(!m.c) {free=&m;break;}if(!free)return bad();
        free->c=s.x[0];free->name=s.x[3];free->constructor=active_;
        ctor_={s.x[0],s.x[1],s.x[2],active_,1};return true;
    }
    case 0x303d10:case 0x303d70: {
        if(ctor_.phase!=(pc==0x303d10?1u:2u) || s.sp!=ctor_.entry.sp-0xa0 || s.x[19]!=ctor_.c)return bad();
        auto *m=member(ctor_.c);if(!m)return bad();
        if(pc==0x303d10) {if(s.x[1]!=ctor_.d || s.x[30]!=base_+0x301804)return bad();m->d=s.x[1];m->device=active_;}
        else {if(s.x[22]!=ctor_.p)return bad();m->p=s.x[22];m->pool=active_;}
        ++ctor_.phase;return true;
    }
    case 0x301804: {
        if(ctor_.phase!=3 || s.sp!=ctor_.entry.sp || s.x[20]!=ctor_.c || s.x[30]!=base_+0x301804)return bad();
        member(ctor_.c)->constructed=active_;ctor_.phase=4;return true;
    }
    case 0x301808: {
        if(ctor_.phase!=4 || s.sp!=ctor_.entry.sp || s.x[0]!=ctor_.c || s.x[20]!=ctor_.c)return bad();
        ctor_.phase=5;return field(ctor_.c+8,ctor_.d);
    }
    case 0x300a08: {
        if(begin_.c || s.sp<0x30 || s.x[0]!=s.x[19])return bad();
        auto *m=member(s.x[0]);if(!m)return bad();
        if(ctor_.phase) {
            if(ctor_.phase!=5 || ctor_.c!=m->c || s.sp!=ctor_.entry.sp-0x40 || s.x[30]!=base_+0x300ae0)return bad();
        } else {
            if(!m->made || m->reset!=2 || s.sp!=m->reset_entry.sp || s.x[30]!=m->reset_entry.lr || s.x[20]!=m->p)return bad();
            m->reset=3;if(!command_fields(*m))return false;
        }
        m->begun=false;m->begin=active_;begin_={m->c,active_,false};return true;
    }
    case 0x300a28: {
        auto *m=member(begin_.c);
        if(!m || begin_.loaded || s.sp!=begin_.entry.sp-0x30 || s.x[19]!=m->c || s.x[30]!=begin_.entry.lr || !s.x[0] || (m->made && m->vk!=s.x[0]))return bad();
        m->vk=s.x[0];m->loaded=active_;begin_.loaded=true;return field(m->c+0x738,m->vk);
    }
    case 0x300a38: {
        auto *m=member(begin_.c);
        if(!m || !begin_.loaded || s.sp!=begin_.entry.sp-0x30 || s.x[19]!=m->c || s.x[30]!=base_+0x300a38 || uint32_t(s.x[0]))return bad();
        m->result=active_;m->begun=true;m->reset=0;m->returned=false;begin_={};return command_fields(*m);
    }
    case 0x30180c: {
        auto *m=member(ctor_.c);
        if(ctor_.phase!=5 || !m || !m->begun || begin_.c || s.sp!=ctor_.entry.sp || s.x[20]!=m->c || s.x[30]!=base_+0x30180c || !(s.x[0]&255))return bad();
        m->made=true;m->created=active_;ctor_={};return true;
    }
    case 0x300b20:case 0x300c54: {
        auto *m=member(pc==0x300b20?s.x[0]:s.x[19]);
        if(!m || !m->made || !m->returned || m->c==current_c_ || s.x[19]!=m->c || s.x[20]!=m->p || begin_.c || dispose_.phase)return bad();
        m->begun=false;
        if(pc==0x300b20) {
            if(s.x[30]!=base_+0x2a9cdc || m->reset || s.sp<0x30)return bad();
            m->reset=1;m->reset_entry=active_;
        } else {
            if(m->reset!=1 || s.sp!=m->reset_entry.sp-0x20 || s.x[30]!=base_+0x300c54 || uint32_t(s.x[0]))return bad();
            m->reset=2;m->reset_result=active_;
        }
        return command_fields(*m);
    }
    case 0x2a1e18:
        if(turn_.phase || dispose_.phase || factory_.phase || s.x[0]!=R || uint32_t(s.x[1]) || !turnover_caller(s.x[30]-base_) || s.sp<0xd0 || !current_w_ || !current_c_)return bad();
        turn_={current_w_,current_c_,active_,1};return true;
    case 0x2a1e3c: {
        auto *m=member(turn_.c);
        if(turn_.phase!=1 || s.sp!=turn_.entry.sp-0xd0 || s.x[0]!=R || s.x[19]!=R || s.x[1]!=turn_.w || s.x[30]!=turn_.entry.lr || !m || !m->made || !m->begun)return bad();
        turn_.phase=2;return field(R+0x1380,turn_.w) && field(turn_.w+8,m->c) && command_fields(*m);
    }
    case 0x2a1f64:
        if(turn_.phase!=2 || s.sp!=turn_.entry.sp-0xd0 || s.x[19]!=R || s.x[20]!=turn_.c || s.x[2]!=turn_.w)return bad();
        return true; // optional actual branch. Absence never invents x20.
    case 0x2a2230:case 0x2a2320:
        if(turn_.phase!=(pc==0x2a2230?2u:3u) || s.sp!=turn_.entry.sp-0xd0 || s.x[19]!=R)return bad();
        ++turn_.phase;return true;
    case 0x305260:
        if(s.x[0]!=current_w_)return bad();
        current_w_=0;current_c_=0;publication_={}; // revoke before any future C/P load
        if(turn_.phase!=4 || dispose_.phase || s.x[0]!=turn_.w || s.x[19]!=R || s.sp!=turn_.entry.sp-0xd0 || s.x[30]!=base_+0x2a23b4)return bad();
        if(s.x[20]!=turn_.w || !member(turn_.c) ||
           !wrapper_epoch_.begin(base_,R,turn_.w,turn_.c,member(turn_.c)->p,s.sp))return bad();
        dispose_={turn_.w,0,0,active_,1};return true;
    case 0x3052e0:
        if(dispose_.phase!=1 || s.sp!=dispose_.entry.sp-0x30 || s.x[19]!=dispose_.w || s.x[1]!=turn_.c)return bad();
        dispose_.c=s.x[1];dispose_.phase=2;return true;
    case 0x3052ec: {
        auto *m=member(dispose_.c);
        if(dispose_.phase!=2 || !m || s.sp!=dispose_.entry.sp-0x30 || s.x[19]!=dispose_.w || s.x[1]!=m->c || s.x[0]<0xd0 || s.x[0]-0xd0!=m->p)return bad();
        dispose_.p=s.x[0]-0xd0;dispose_.phase=3;return true;
    }
    case 0x311648: {
        auto *m=member(s.x[1]);if(!m || !m->made)return bad();
        const bool begun=m->begun;m->begun=false; // entry revokes even a refused return
        if(!begun || m->reset || s.x[0]!=m->p+0xd0 || m->c==current_c_)return bad();
        const bool observed_return=observation_.kind==3 && observation_.phase==3 && observation_.c==m->c;
        const bool selected_return=dispose_.phase && (m->c==dispose_.c || s.sp==dispose_.entry.sp-0x30);
        if(observed_return) {
            // observe_shared already paired W/load/pre-BL/entry, including P.
            observation_.phase=4;
        } else if(selected_return) {
            if(dispose_.phase!=3 || s.sp!=dispose_.entry.sp-0x30 || s.x[30]!=base_+0x3052f0 || s.x[19]!=dispose_.w || m->c!=dispose_.c)return bad();
            dispose_.phase=4;
        } else if(!return_caller(s.x[30]-base_))return bad();
        m->returned=!selected_return && !observed_return;
        return command_fields(*m);
    }
    case 0x3052f0:
        if(dispose_.phase!=4 || s.sp!=dispose_.entry.sp-0x30 || s.x[19]!=dispose_.w || s.x[30]!=base_+0x3052f0 || (s.x[0]&255)!=1)return bad();
        member(dispose_.c)->returned=true;
        dispose_={};turn_.phase=5;return true;
    case 0x3053f8:
        // Bootstrap wrapper identity uses the existing qualified initializer
        // borrow. This collector's factory path is the authenticated turnover.
        if(factory_.phase || turn_.phase!=6 || s.sp<0xe0 || !ptr(s.sp,0xa0) || s.sp!=turn_.entry.sp-0xd0 || s.x[30]!=base_+0x2a23e8 || s.x[8]!=s.sp+0x98 || !s.x[0])return bad();
        factory_={};factory_.owner=s.x[0];factory_.slot=s.x[8];factory_.name=s.x[1];factory_.entry=active_;factory_.phase=1;return true;
    case 0x305308:
        if(factory_.phase!=1 || s.sp!=factory_.entry.sp-0x30 || s.x[30]!=base_+0x30548c || s.x[1]!=factory_.owner || s.x[2]!=factory_.name || !ptr(s.x[0],0x10))return bad();
        factory_.w=s.x[0];factory_.constructor=active_;factory_.phase=2;return true;
    case 0x302790: {
        auto reject=[&](unsigned predicate) {
            bool known_pool=false;for(const auto &m:members_)known_pool=known_pool || (m.c && m.p==s.x[0]);
            const bool sp_checked=factory_.phase>=2 && factory_.constructor.sp>=0x20;
            const uint64_t flags=uint64_t(known_pool) | uint64_t(current_w_ && s.x[8]==current_w_+8)<<1 |
                uint64_t(factory_.w && s.x[8]==factory_.w+8)<<2 | uint64_t(sp_checked)<<3 |
                uint64_t(sp_checked && s.sp==factory_.constructor.sp-0x20)<<4;
            const uint64_t caller=s.x[30]>=base_ && s.x[30]-base_<0x600000?s.x[30]-base_:0;
            return deny(pc,uint64_t(predicate)<<24 | uint64_t(factory_.phase&15)<<32 | flags<<36 | caller<<41);
        };
        // Same short-circuit predicates and order. Diagnostics read only owned
        // local facts, never the name, output slot, native pool or Saved memory.
        if(factory_.phase!=2)return reject(1);
        if(s.sp!=factory_.constructor.sp-0x20)return reject(2);
        if(s.x[30]!=base_+0x305330)return reject(3);
        if(s.x[8]!=factory_.w+8)return reject(4);
        if(s.x[1]!=factory_.name)return reject(5);
        if(!ptr(s.x[0],0xd0))return reject(6);
        if(!wrapper_epoch_.event(pc,s))return bad();
        factory_.p=s.x[0];factory_.acquire=active_;factory_.phase=3;return true;
    }
    case 0x304e60:
        if(factory_.phase!=3 || s.sp!=factory_.acquire.sp-0x30 || s.x[30]!=base_+0x3027b0 || s.x[8]!=factory_.w+8 || s.x[0]!=factory_.p)return bad();
        factory_.pop=active_;factory_.phase=4;return true;
    case 0x304ec8: {
        auto *m=member(s.x[22]);
        if(factory_.phase!=4 || s.sp!=factory_.pop.sp-0x60 || s.x[21]!=factory_.w+8 || s.x[19]!=factory_.p || !m || !m->made || !m->begun || m->p!=factory_.p)return bad();
        factory_.c=m->c;factory_.positive_store=active_;factory_.phase=9;return true;
    }
    case 0x304ecc: {
        // The failed-creation branch 304f9c/304fa0 also arrives here. Only
        // adjacent actual STR+post observations establish the positive result.
        if(factory_.phase!=9 || s.sp!=factory_.positive_store.sp || s.x[30]!=factory_.positive_store.lr || s.x[22]!=factory_.c || s.x[21]!=factory_.w+8 || s.x[19]!=factory_.p)return bad();
        factory_.loaded=active_;factory_.phase=5;return true;
    }
    case 0x3053a8:
        if(factory_.phase!=5 || s.sp!=factory_.constructor.sp || s.x[30]!=base_+0x30548c)return bad();
        factory_.return_intent=active_;factory_.phase=6;return true;
    case 0x305490:
        if(factory_.phase!=6 || s.sp!=factory_.entry.sp-0x30 || s.x[20]!=factory_.slot || s.x[22]!=factory_.w || s.x[30]!=base_+0x30548c)return bad();
        factory_.stored=active_;factory_.phase=7;return true;
    case 0x305470:
        if(factory_.phase!=7 || s.sp!=factory_.entry.sp || s.x[30]!=factory_.entry.lr || s.x[0]!=factory_.slot)return bad();
        factory_.return_slot=active_;factory_.phase=8;return true;
    case 0x2a23f4:return true; // pre-store never publishes or supplies fields
    case 0x2a23f8: {
        auto *m=member(factory_.c);
        if(factory_.phase!=8 || turn_.phase!=6 || dispose_.phase || s.sp!=factory_.entry.sp || s.x[19]!=R || s.x[0]!=factory_.w || s.x[30]!=factory_.entry.lr || !m || !m->made || !m->begun || m->reset)return bad();
        current_w_=s.x[0];current_c_=m->c;
        const bool ok=field(current_w_+8,m->c) && command_fields(*m);
        publication_=factory_;factory_={};turn_={};return ok;
    }
    case 0x3003d0:case 0x4b3d0: {
        if(pc==0x4b3d0 && turn_.phase==5 && s.x[0]==turn_.w &&
           s.x[0]!=R && !current_w_ && !factory_.phase && !dispose_.phase &&
           !member(s.x[0]) && wrapper_epoch_.retire(s)) {
            turn_.phase=6;return true; // W storage only, never erase C or debt
        }
        if(pc==0x4b3d0 && turn_.phase && s.sp==turn_.entry.sp-0xd0 && s.x[30]==base_+0x2a23c0)return bad();
        if(wrapper_epoch_.dead(s.x[0]))return bad();
        if(s.x[0] && (s.x[0]==R || s.x[0]==current_w_ || s.x[0]==turn_.w || s.x[0]==factory_.w))return bad();
        auto *m=member(s.x[0]);if(!m)return true;
        const bool live=m->c==current_c_ || m->c==ctor_.c || m->c==begin_.c || m->c==turn_.c || m->c==factory_.c;
        *m={};return !live || bad(); // delete revokes before same-address reuse
    }
    case 0x29bd18:case 0x29c238:if(s.x[0]==R)return bad();return true;
    case 0x2953a0:if(s.x[0]==current_w_ || s.x[0]==factory_.w)return bad();return true;
    default:return true; // no scalar answer at unrecognized PCs
    }
}
} // namespace native_portal::runtime
