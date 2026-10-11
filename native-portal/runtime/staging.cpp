// SPDX-License-Identifier: MIT
#include "evidence.hpp"
namespace native_portal::runtime {
namespace {
bool bounded(uint64_t p,uint64_t n) noexcept {return p && n && n<=UINT64_MAX-p;}
}
void Evidence::clear_staging() noexcept {
    staging_writer_=nullptr;staging_owner_=nullptr;staging_reads_=false;staging_returned_=false;
    camera_=stock_=stock_resource_=staging_sp_=staging_ticket_=0;
    staging_input_={};
}
bool Evidence::staging_read(uint64_t p,void *out,size_t n) const noexcept {
    if(!staging_reads_ || staging_poisoned_.load() || !bounded(p,n))return false;
    // Only the fields used by Bridge, not a blanket camera/resource allocation.
    const bool field=(p==camera_+8 && n==8) || (p==camera_+0x2d0 && n==8) ||
        (p==stock_+8 && n==8) || (p==stock_resource_+0xb8 && n==4);
    const bool constants=p>=camera_+0xc0 && p<=camera_+0x2d0 && n<=camera_+0x2d0-p;
    return (field || constants) && read_self(p,out,n);
}
uint64_t Evidence::stage(render::Adapter &adapter,uint64_t pc,arena::Saved &s,
                         const render::EyeInput &input,uint64_t invocation) noexcept {
    // Foreign callers cannot race non-atomic scoped state. Poison is observed by
    // the owner before publication; fork changes only the child's private copy.
    if(!owner()) {staging_poisoned_.store(true);s.pad=arena::Unsupported;return 0;}
    auto refuse=[&]() -> uint64_t {
        staging_poisoned_.store(true);clear_staging();
        // Exact Adapter rejection releases its offer, never charged arena debt.
        adapter.enqueue(0,{});
        s.pad=arena::Unsupported;
        if(pinned_ && (pc==0x161d64 || pc==0x161ed8)) {
            s.pad=arena::SkipOwned;s.x[30]=base_+(pc==0x161d64?0x161d68:0x161edc);
        }
        return 0;
    };
    if(dormant_stock())return 0; // no Adapter or native memory access before engagement
    if(!initialized() || staging_poisoned_.load() || !bridge_ ||
       !adapter.staging_owner(bridge_,identity_,base_))return refuse();
    if(pc==0x161d64) {
        // The trusted trampoline supplies the actual suspended BLR x7 registers.
        // This synchronous native caller owns K, its stock wrapper, and constants.
        // It does NOT establish frame demand, origin authorization or graph life.
        if(invocation || staging_ticket_ || staging_writer_ || !bounded(s.sp,0x460) ||
           !bounded(s.x[27],0x1000) || !bounded(s.x[1],0x20) ||
           s.x[0]!=identity_.renderer || s.x[2]!=s.x[27]+0xc0 || s.x[7]!=base_+0x2993f8)
            return refuse();
        camera_=s.x[27];stock_=s.x[1];staging_sp_=s.sp;
        uint64_t renderer=0,wrapper=0,context=0;uint32_t size=0;
        if(!read_self(camera_+8,&renderer,8) || renderer!=identity_.renderer ||
           !read_self(camera_+0x2d0,&wrapper,8) || wrapper!=stock_ ||
           !read_self(stock_+8,&stock_resource_,8) || !bounded(stock_resource_,0x128) ||
           !read_self(stock_resource_+8,&context,8) || context!=identity_.renderer+0x50 ||
           !read_self(stock_resource_+0xb8,&size,4) || size!=528)return refuse();
        staging_input_=input;staging_input_.prefix={};
        if(!read_self(camera_+0xc0,staging_input_.constants.data(),528))return refuse();
        staging_ticket_=adapter.reserve_upload(staging_input_,camera_,stock_);
        if(!staging_ticket_)return refuse();
        staging_owner_=&adapter;staging_reads_=true;staging_writer_=&adapter;
        adapter.upstream(s,arena::Upload,epoch_,static_cast<uint64_t>(tid_));
        const bool written=!staging_writer_;
        staging_writer_=nullptr;staging_reads_=false;
        // Adapter already checks Off/expiry AFTER write and BEFORE original BLR.
        if(!written || staging_poisoned_.load() || s.pad!=arena::Replacement || adapter.state().frame_invalid)
            return refuse();
        return staging_ticket_;
    }
    if(!invocation || invocation!=staging_ticket_ || staging_owner_!=&adapter ||
       s.x[27]!=camera_ || s.sp!=staging_sp_ || input.eye!=staging_input_.eye ||
       input.observed_origin!=staging_input_.observed_origin ||
       input.raw_to_standing.v!=staging_input_.raw_to_standing.v)return refuse();
    if(pc==0x161f00) {
        // Native caller epilogue without an accepted enqueue: settle ONLY this
        // CPU borrow/offer, not its already charged native command references.
        clear_staging();staging_poisoned_.store(true);adapter.enqueue(0,{});
        s.pad=arena::Original;return 0;
    }
    uint64_t renderer=0,wrapper=0,resource=0,context=0;uint32_t size=0;
    if(!read_self(camera_+8,&renderer,8) || renderer!=identity_.renderer ||
       !read_self(camera_+0x2d0,&wrapper,8) || wrapper!=stock_ ||
       !read_self(stock_+8,&resource,8) || resource!=stock_resource_ ||
       !read_self(resource+8,&context,8) || context!=identity_.renderer+0x50 ||
       !read_self(resource+0xb8,&size,4) || size!=528)return refuse();
    if(pc==0x161d68) {
        if(staging_returned_)return refuse();
        adapter.upstream(s,arena::UploadReturn,epoch_,static_cast<uint64_t>(tid_));
        if(staging_poisoned_.load() || adapter.state().frame_invalid)return refuse();
        staging_returned_=true;return invocation;
    }
    if(pc==0x161ed8) {
        if(!staging_returned_ || s.x[0]!=identity_.renderer || s.x[1]!=s.sp+0x300 ||
           s.x[22]!=s.x[1] || s.x[2]!=base_+0x450650 || s.x[4]!=base_+0x29b278 ||
           !read_self(s.x[1],staging_input_.prefix.data(),352))return refuse();
        // Pinned 161bc0/161c74 selects resource camera and ring slot, not HMD
        // eye. At 161ed8 x21 and x24 both contain 3*slot. Read only this
        // invocation's current scalar and selected wrapper; no retained reads.
        staging_input_.invocation_sp=s.sp;staging_input_.camera=camera_;
        if(s.x[28]<2 && s.x[21]==s.x[24] && s.x[24]<48 && s.x[24]%3==0) {
            uint32_t selected=UINT32_MAX;uint64_t texture=0;
            if(read_self(camera_+0x820+4*s.x[28],&selected,4) && selected==s.x[24]/3 &&
               read_self(camera_+0x520+384*s.x[28]+24*selected,&texture,8)) {
                staging_input_.resource_index=uint32_t(s.x[28]);staging_input_.slot=selected;
                staging_input_.selected_texture=texture;
            }
        }
        const auto final=staging_input_;
        clear_staging(); // no camera/native stack read authority survives enqueue
        if(!adapter.enqueue(invocation,final))return refuse();
        s.pad=arena::SkipOwned;s.x[30]=base_+0x161edc; // Adapter issued the sole replacement enqueue
        return invocation;
    }
    return refuse(); // includes explicit abort; never infer native completion
}
} // namespace native_portal::runtime
