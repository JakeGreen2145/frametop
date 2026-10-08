// SPDX-License-Identifier: MIT
#include "bridge.hpp"
#include <cstring>
namespace native_portal::arena {
bool Bridge::read(uint64_t a,void* out,size_t n) noexcept {
    return a && n<=UINT64_MAX-a && mem_.read && mem_.read(mem_.user,a,out,n);
}
bool Bridge::arm(Scope s,bool pinned) noexcept {
    if(armed_ || !pinned || !s.renderer || s.renderer>UINT64_MAX-0x2000 ||
       !s.lifetime || !s.thread || !s.allocator || s.base>UINT64_MAX-0x600000 ||
       !mem_.read || !mem_.write || !mem_.coherent) return false;
    scope_=s;armed_=true;return true;
}
bool Bridge::context(uint64_t r) noexcept {
    uint64_t a=0,m=0,b=0,align=0;uint32_t cap=0;
    return r==scope_.renderer && get(r+0x1390,a) && a==resource_ &&
        get(r+0x1410,m) && m==map_ && get(r+0x328,align) && align==64 &&
        get(a+0xb0,b) && b==buffer_ && get(a+0xb8,cap) && cap==Capacity &&
        mem_.coherent(mem_.user,a);
}
bool Bridge::offer(const Owned& o) noexcept {
    if(!ready() || !o.camera || o.camera>UINT64_MAX-0x1000 || !o.stock_wrapper ||
       !o.wrapper || o.wrapper>UINT64_MAX-0x20 || !o.staging || o.staging>UINT64_MAX-592 ||
       !o.ticket || (owned_wrapper_ && o.wrapper!=owned_wrapper_) || (owned_.ticket && (!used_ || !descriptor_.ticket)) ||
       (owned_.ticket && o.ticket<=owned_.ticket)) return false;
    // Staging must not alias either native constants or the stock wrapper.
    auto overlap=[](uint64_t a,uint64_t n,uint64_t b,uint64_t m){return a<b+m && b<a+n;};
    if(o.stock_wrapper>UINT64_MAX-0x20 ||
       overlap(o.staging,592,o.camera,0x1000) || overlap(o.staging,592,o.wrapper,0x20) ||
       overlap(o.staging,592,o.stock_wrapper,0x20)) return false;
    owned_=o;owned_wrapper_=o.wrapper;used_=charged_=false;descriptor_={};return true;
}
void Bridge::dispatch(Saved& s,unsigned site,uint64_t lifetime,uint64_t thread) noexcept {
    s.pad=Original;if(!armed_ || site>=SiteCount) return;
    uint64_t r=0;
    switch(site) {
    case Upload:case UboClassify:r=s.x[0];break;
    case Allocate:case Admission:case AllocationReturn:r=s.x[19];break;
    case GeometryClassify:r=s.x[21];break;
    case Initialized:r=s.x[24];break;
    case Reset:r=s.x[22];break;
    case UploadReturn:if(s.x[27]!=owned_.camera || !owned_.ticket) return;r=scope_.renderer;break;
    }
    if(r!=scope_.renderer) return; // never mutate unrelated native objects
    auto fatal=[&] {failed_=true;s.pad=Unsupported;descriptor_={};};
    if(lifetime!=scope_.lifetime || thread!=scope_.thread) {
        if(requested_) fatal(); else failed_=true;
        return;
    }
    if(site==Allocate) {
        if(failed_ || requested_ || s.x[0]!=scope_.allocator || s.x[1]!=StockBudget || !s.x[8]) {
            if(requested_) fatal();else failed_=true;
            return;
        }
        requested_=true;allocation_slot_=s.x[8];s.x[1]=Capacity;return;
    }
    if(site==AllocationReturn) {
        if(!requested_ || failed_ || allocated_ || !get(allocation_slot_,allocated_) || !allocated_) failed_=true;
        return;
    }
    if(site==Initialized) {
        if(!requested_ || !allocated_ || failed_ || !(s.x[0]&255)) {failed_=true;return;}
        uint64_t align=0;uint32_t cap=0,cursor=0;
        if(!get(r+0x1390,resource_) || resource_!=allocated_ || resource_>UINT64_MAX-0xc0 ||
           !get(r+0x1410,map_) || !map_ || map_>UINT64_MAX-Capacity ||
           !get(resource_+0xb0,buffer_) || !buffer_ || !get(resource_+0xb8,cap) ||
           !get(r+0x328,align) || !get(r+0x1418,cursor) || cursor ||
           !policy_.initialize(cap,align,mem_.coherent(mem_.user,resource_))) failed_=true;
        return;
    }
    if(!policy_.ready()) {
        // Unsupported late attach never changes stock. Recognized owned calls
        // are not offered until initialization has established the reservation.
        return;
    }
    if(failed_ || !context(r)) {fatal();return;}
    if(site==Reset) {
        policy_.reset(true);owned_={};descriptor_={};used_=charged_=false;return;
    }
    if(site==Upload) {
        if(!owned_.ticket || s.x[27]!=owned_.camera) return;
        auto drop=[&]{s.pad=SkipOwned;s.x[30]=scope_.base+0x161d68;used_=true;charged_=false;descriptor_={};};
        if(used_) {drop();return;}
        uint64_t kr=0,kw=0,wr=0,stock_resource=0,begin=0,end=0,cmd=0;uint32_t size=0,cursor=0;
        if(s.x[1]!=owned_.stock_wrapper || s.x[2]!=owned_.camera+0xc0 ||
           s.x[7]!=scope_.base+0x2993f8 || !get(owned_.camera+8,kr) || kr!=r ||
           !get(owned_.camera+0x2d0,kw) || kw!=owned_.stock_wrapper ||
           !get(kw+8,stock_resource) || !stock_resource || stock_resource>UINT64_MAX-0xc0 ||
           !get(stock_resource+0xb8,size) || size!=528 ||
           !get(owned_.wrapper+8,wr) || !wr || wr>UINT64_MAX-0xc0 ||
           !get(wr+0xb8,size) || size!=592 || !get(r+0x1380,cmd) || !cmd ||
           !get(r+0x15a0,begin) || !get(r+0x15a8,end) || !begin || end<=begin ||
           (end-begin)%0x98 || !get(r+0x1418,cursor) ||
           policy_.check(cursor,592,true)!=Result::Admit) {drop();return;}
        std::array<unsigned char,592> staging{};
        if(!read(s.x[2],staging.data(),528)) {drop();return;}
        std::memcpy(staging.data()+528,owned_.portal.data(),64);
        if(!mem_.write(mem_.user,owned_.staging,staging.data(),staging.size())) {drop();return;}
        used_=true;offset_=cursor;s.x[1]=owned_.wrapper;s.x[2]=owned_.staging;
        s.pad=Replacement;return; // the original BLR x7 executes exactly once
    }
    if(site==Admission) {
        uint32_t cursor=0;uint64_t cmd=0;
        bool own=owned_.ticket && used_ && !charged_ && s.x[25]==owned_.staging &&
            s.x[21]==owned_.wrapper+0x18 && s.x[20]==592 && s.x[23]==0x808 && s.x[24]==8;
        if(!get(r+0x1418,cursor) || s.x[0]!=cursor || !s.x[21] ||
           !get(r+0x1380,cmd) || !cmd ||
           policy_.append(cursor,s.x[20],own)!=Result::Admit) {fatal();return;}
        if(own) charged_=true;
        s.pad=Replacement;return; // CMP XZR,XZR prevents the native log-and-copy path
    }
    if(site==UploadReturn) {
        if(!used_)return;
        uint32_t offset=0,cursor=0;
        if(!charged_ || !get(owned_.wrapper+0x18,offset) || offset!=offset_ ||
           !get(r+0x1418,cursor) || cursor!=policy_.actual()) {descriptor_={};return;}
        descriptor_={buffer_,offset,592,owned_.ticket};return;
    }
    uint64_t offset=site==UboClassify?s.x[4]:s.x[3],size=0;
    if(offset==UINT32_MAX) return; // unchanged own-buffer sentinel path
    if(site==UboClassify) {
        uint64_t wr=0;uint32_t n=0;
        if(!s.x[2] || s.x[2]>UINT64_MAX-0x20 || !get(s.x[2]+8,wr) ||
           !wr || wr>UINT64_MAX-0xc0 || !get(wr+0xb8,n)) {fatal();return;} size=n;
    } else {
        uint32_t n=0,cursor=0;
        if(!s.x[24] || s.x[24]>UINT64_MAX-0x200 || !get(s.x[24]+0x1c0,n) ||
           !get(r+0x1418,cursor) || offset>cursor || n>cursor-offset) {fatal();return;} size=n;
    }
    if(policy_.classify(offset,size)!=Offset::Arena) {fatal();return;}
    s.x[site==UboClassify?3:0]=Capacity-1;s.pad=Replacement;
}
}
