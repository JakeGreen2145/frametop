// SPDX-License-Identifier: MIT
#include "evidence.hpp"
#include <atomic>
#include <cstring>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>

// Same C ABI as pipeline/ft_pipeline.h, without a Vulkan SDK build dependency.
// VkDevice is a dispatchable pointer. There is no fallback generation issuer.
extern "C" uint64_t ft_pipeline_generation(void *) __attribute__((weak));
namespace native_portal::runtime {
namespace {
static_assert(sizeof(void*)==8, "Pinned native ABI is 64-bit");
std::atomic<uint64_t> epochs{0};
static_assert(decltype(epochs)::is_always_lock_free);
uint64_t issue() noexcept {
    auto old=epochs.load(std::memory_order_relaxed);
    while(old!=UINT64_MAX) {
        if(epochs.compare_exchange_weak(old,old+1,std::memory_order_relaxed)) return old+1;
    }
    return 0;
}
bool bounded(uint64_t p,size_t n) noexcept {return p && n && n<=UINT64_MAX-p;}
constexpr std::array<uint32_t,8> source_over{1,1,7,0,1,7,0,15};
}
bool read_self(uint64_t p,void *out,size_t n) noexcept {
    if(!out || !bounded(p,n) || n>4096) return false;
    iovec local{out,n},remote{reinterpret_cast<void*>(p),n};
    // No /proc reads, signal handler, heap, logging or file descriptor. EFAULT,
    // EPERM, ENOSYS, EINTR and short copies all refuse without retrying.
    return syscall(SYS_process_vm_readv,getpid(),&local,1,&remote,1,0)==static_cast<long>(n);
}
Evidence::Evidence() noexcept : pid_(getpid()),tid_(static_cast<pid_t>(syscall(SYS_gettid))) {}
bool Evidence::owner() const noexcept {return getpid()==pid_ && syscall(SYS_gettid)==tid_;}
bool Evidence::pin(uint64_t base,const char *sha) noexcept {
    if(!owner() || pinned_ || !sha || std::strcmp(sha,elf_sha256) || !base || base>UINT64_MAX-0x600000) return false;
    base_=base;pinned_=true;return true;
}
bool Evidence::add(unsigned i,uint64_t p,size_t n) noexcept {
    if(i>=spans_.size() || !epoch_ || !bounded(p,n)) return false;
    spans_[i]={p,n,epoch_};return true;
}
bool Evidence::read(uint64_t p,void *out,size_t n) const noexcept {
    if(!owner() || failed_ || !constructed_ || !bounded(p,n)) return false;
    if(borrowed_slot_ && p==borrowed_slot_ && n==8) return read_self(p,out,n);
    if(staging_read(p,out,n)) return true;
    if(graph_read(p,out,n)) return true;
    for(const auto &s:spans_) {
        if(s.epoch==epoch_ && p>=s.begin && p-s.begin<=s.size && n<=s.size-(p-s.begin))
            return read_self(p,out,n);
    }
    return false;
}
bool Evidence::fail(uint64_t pc) noexcept {
    if(!failed_)first_failure_pc_.store(pc);
    clear_staging();
    failed_=true;ready_=false;allocation_open_=false;call_=0;spans_={};recordings_={};recording_=0;return false;
}
bool Evidence::initialized() const noexcept {return owner() && ready_ && !failed_;}
render::Identity Evidence::identity() const noexcept {return initialized()?identity_:render::Identity{};}
bool Evidence::event(uint64_t pc,const arena::Saved &s) noexcept {
    if(!owner() || !pinned_ || failed_) return false;
    if(pc==0x7fda0) { // successful 0x16c0 constructor return; R is x24, not x0
        if(constructed_ || !bounded(s.x[24],0x16c0)) return fail(pc);
        epoch_=issue();if(!epoch_) return fail(pc);
        constructed_=true;identity_.renderer=s.x[24];identity_.renderer_generation=epoch_;
        // PID is a process-local discriminator only. Fork is separately rejected;
        // no evidence may be persisted or compared across process instances.
        identity_.process_instance=static_cast<uint64_t>(pid_);
        if(!add(0,s.x[24],0x16c0)) return fail(pc);
        uint64_t vtable=0;
        if(!at(s.x[24],vtable) || vtable!=base_+0x599c38) return fail(pc);
        return true;
    }
    if(!constructed_) return false;
    const auto R=identity_.renderer;
    if(pc==0x301808 || pc==0x30180c || pc==0x300a08 || pc==0x300a38 ||
       pc==0x300b20 || pc==0x300c54 || pc==0x3003d0 || pc==0x311648 || pc==0x4b3d0 ||
       pc==0x305260 || pc==0x2953a0 || pc==0x3018b0 || pc==0x2a1e18 || pc==0x2a2230 ||
       pc==0x2a2320 || pc==0x2987d8 || pc==0x298840 || pc==0x2a23f4) return recording_event(pc,s);
    if(pc==0x29bd18 || pc==0x29c238) {
        if(s.x[0]!=R) return false;
        fail(pc);return true; // revoke before native teardown, never retire native objects
    }
    if(pc==0x2966a0 || pc==0x294678) {
        if(s.x[0]!=R) return false;
        uint64_t object=0;
        if(!read_self(s.x[1],&object,8)) return fail(pc);
        if((pc==0x2966a0 && (object==wrapper_ || (staging_ticket_ && object==stock_))) ||
           (pc==0x294678 && object==marker_)) {
            fail(pc);return true; // revoke before native destruction, never read freed storage
        }
        return false;
    }
    if(pc==0x2fe090) {
        if(s.x[0]!=resource_) return false;
        fail(pc);return true; // this consumer does not authorize a later remap
    }
    if(pc==0x2987d4) {
        if(s.x[22]!=R || !ready_) return false;
        // Arena cursor reset is NOT a command recording generation.
        return true;
    }
    if(pc==0x29a14c) {
        if(ready_ || resource_ || allocation_open_ || s.x[19]!=R || s.x[0]!=R+0xec0 ||
           (s.x[1]!=0x80000 && s.x[1]!=0x80080) || !bounded(s.sp,0x148) ||
           s.x[8]!=s.sp+0x140) return fail(pc);
        requested_size_=static_cast<uint32_t>(s.x[1]);allocation_sp_=s.sp;
        allocation_slot_=s.x[8];allocation_open_=true;return true;
    }
    if(pc==0x29a150) {
        // The output is live only in this paired suspended native frame. x0 is
        // not the resource return value; native consumes/clears the slot next.
        if(!allocation_open_ || s.sp!=allocation_sp_ || s.x[19]!=R ||
           !read_self(allocation_slot_,&resource_,8) || !add(1,resource_,0x128)) return fail(pc);
        allocation_open_=false;allocation_slot_=allocation_sp_=0;return true;
    }
    if(pc==0x29a528) { // before STR x1,[R+1380]; newly returned 16-byte wrapper
        if(ready_ || s.x[19]!=R || command_wrapper_ || !add(2,s.x[1],0x10)) return fail(pc);
        command_wrapper_=s.x[1];
        uint64_t command=0;
        if(!at(command_wrapper_+8,command)) return fail(pc);
        for(auto &c:recordings_) if(c.command==command && c.begun) {
            if(!add(6,command,0x7b0)) return fail(pc);
            uint64_t vk=0,D=0;
            if(!at(command+0x738,vk) || vk!=c.vk || !at(command+8,D) || D!=R+0x50) return fail(pc);
            // Issue only for the published owner, not the last-created pool member.
            c.generation=generations_.issue();if(!c.generation) return fail(pc);
            recording_=c.generation;break;
        }
        return true;
    }
    if(pc==0x7fdbc) {
        if(ready_ || s.x[24]!=R || !(s.x[0]&255) || allocation_open_ || !resource_ || !command_wrapper_) return fail(pc);
        uint64_t physical=0,context=0,vtable=0;render::Arena a{};
        if(!at(R,vtable) || vtable!=base_+0x599c38 || !at(R+0xa78,identity_.device) || !identity_.device ||
           !at(R+0xb0,physical) || !physical || !at(R+0x78,context) || !context || !arena(a) || a.cursor) return fail(pc);
        // Attach only the layer's actual device creation generation. Null weak
        // export leaves this fact unsupported; never substitute our renderer epoch.
        if(!at(resource_+0x50,backing_) || !at(resource_+0x58,type_index_) ||
           !at(resource_+0x60,allocation_bytes_) || !at(R+0xf00,type_count_) ||
           !at(R+0xf04+8*type_index_,type_flags_)) return fail(pc);
        map_=a.map;buffer_=a.buffer;
        if(ft_pipeline_generation) identity_.device_generation=ft_pipeline_generation(reinterpret_cast<void*>(identity_.device));
        ready_=true;return true;
    }
    return false;
}
bool Evidence::route(arena::Bridge &bridge,uint64_t pc,arena::Saved &s) noexcept {
    if(!owner() || failed_ || !pinned_ || borrowed_slot_) return false;
    if(pc==0x7fda0) {
        if(bridge_ || !event(pc,s)) return false;
        bridge_=&bridge;
        if(!bridge.arm({identity_.renderer,epoch_,static_cast<uint64_t>(tid_),identity_.renderer+0xec0,base_},true)) return fail(pc);
        return true;
    }
    if(bridge_!=&bridge) return false;
    if(pc==0x29a14c) {
        bridge.dispatch(s,arena::Allocate,epoch_,static_cast<uint64_t>(tid_));
        if(s.x[1]!=arena::Capacity) return fail(pc);
        return event(pc,s);
    }
    if(pc==0x29a150) {
        const auto slot=allocation_slot_;
        if(!event(pc,s)) return false;
        borrowed_slot_=slot;
        bridge.dispatch(s,arena::AllocationReturn,epoch_,static_cast<uint64_t>(tid_));
        borrowed_slot_=0;
        return true;
    }
    if(pc==0x7fdbc) {
        if(!event(pc,s)) return false;
        bridge.dispatch(s,arena::Initialized,epoch_,static_cast<uint64_t>(tid_));
        return bridge.ready();
    }
    return event(pc,s);
}
bool Evidence::arena(render::Arena &a) const noexcept {
    const auto R=identity_.renderer;uint64_t D=0,allocator_D=0,backing=0,map_owner=0,mapped=0,allocation_size=0,device=0;
    uint32_t capacity=0,cursor=0,index=0,count=0,flags=0;int32_t refs=0;
    if(!at(R+0xa78,device) || device!=identity_.device || !at(R+0x1390,a.resource) || a.resource!=resource_ ||
       !at(resource_+8,D) || D!=R+0x50 || !at(R+0xec0,allocator_D) || allocator_D!=D ||
       !at(resource_+0x50,backing) || !backing || !at(resource_+0x58,index) ||
       !at(R+0xf00,count) || !count || count>32 || index>=count ||
       !at(R+0xf04+8*index,flags) || (flags&0xe)!=0xe ||
       !at(resource_+0x60,allocation_size) || !at(resource_+0xb8,capacity) || capacity!=requested_size_ ||
       allocation_size<capacity || !at(resource_+0xb0,a.buffer) || !a.buffer ||
       !at(resource_+0x98,refs) || refs<=0 || !at(resource_+0xa0,mapped) ||
       !at(R+0x1408,map_owner) || map_owner!=resource_ || !at(R+0x1410,a.map) ||
       a.map!=mapped || !bounded(a.map,capacity) || !at(R+0x328,a.alignment) || a.alignment!=64 ||
       !at(R+0x1418,cursor) || cursor>capacity || !at(R+0x1380,a.command_wrapper) ||
       a.command_wrapper!=command_wrapper_ || !at(command_wrapper_+8,a.command) || !a.command ||
       !at(R+0x15a0,a.pass_begin) || !at(R+0x15a8,a.pass_end) ||
       a.pass_end<a.pass_begin || (a.pass_begin&7) || (!a.pass_begin && a.pass_end) ||
       (a.pass_end-a.pass_begin)%0x98 || (a.pass_end-a.pass_begin)/0x98>binding::pass_limit) return false;
    if(ready_ && (backing!=backing_ || index!=type_index_ || flags!=type_flags_ || count!=type_count_ ||
                  a.map!=map_ || a.buffer!=buffer_ || allocation_size!=allocation_bytes_)) return false;
    a.capacity=capacity;a.cursor=cursor;a.recording_generation=recording_;
    if(recording_) {
        uint64_t vk=0,D=0;bool found=false;
        if(!at(a.command+0x738,vk) || !at(a.command+8,D) || D!=R+0x50) return false;
        for(const auto &c:recordings_) if(c.command==a.command && c.vk==vk && c.generation==recording_) found=true;
        if(!found) return false;
    }
    return true;
}
bool Evidence::coherent(uint64_t resource) const noexcept {
    if(!initialized() || resource!=resource_) return false;
    render::Arena a{};return arena(a);
}
bool Evidence::call_enter(const render::Registers &r) noexcept {
    if(!initialized()) return false;
    if(call_) return fail(call_-base_); // the already authenticated native method
    if(r.x[0]!=identity_.renderer) return false;
    if(r.target==base_+0x296c00) {
        if(wrapper_ || r.x[1]!=592) return false;
        for(unsigned i=2;i<=6;++i) if(r.x[i]) return false;
    } else if(r.target==base_+0x294358) {
        std::array<uint32_t,6> input{};
        // Borrow only for the duration of the direct call-enter callback.
        if(marker_ || !read_self(r.x[1],input.data(),sizeof input) || input!=std::array<uint32_t,6>{1,1,3,1,3,0}) return false;
    } else return false;
    call_=r.target;return true;
}
bool Evidence::call_return(const render::Registers &r) noexcept {
    const auto pc=r.target-base_;
    if(!initialized() || !call_ || r.target!=call_) return false;
    const auto target=call_;call_=0;
    if(target==base_+0x296c00) {
        if(!add(3,r.x[0],0x20)) return fail(pc);
        wrapper_=r.x[0];uint32_t size=0,offset=0;uint64_t D=0;
        if(!at(wrapper_+8,wrapper_resource_) || !add(4,wrapper_resource_,0x128) || wrapper_resource_==resource_ ||
           !at(wrapper_resource_+8,D) || D!=identity_.renderer+0x50 ||
           !at(wrapper_resource_+0xb8,size) || size!=592 || !at(wrapper_+0x18,offset) || offset!=UINT32_MAX) return fail(pc);
    } else {
        if(!add(5,r.x[0],0x28)) return fail(pc);
        marker_=r.x[0];std::array<uint32_t,8> attachment{};
        if(!read(marker_+8,attachment.data(),sizeof attachment) || attachment!=source_over) return fail(pc);
    }
    return true;
}
bool Evidence::snapshot(uint64_t wrapper,uint64_t marker,render::Observation &out) const noexcept {
    out={};if(!initialized() || !wrapper_ || !marker_ || wrapper!=wrapper_ || marker!=marker_) return false;
    // During authenticated Run an old wrapper offset can outlive an arena cursor
    // reset. Reading that bounded metadata is not descriptor authorization:
    // Adapter/Bridge demand a fresh charged copy and exact post-upload offset.
    render::Observation o{};uint32_t size=0,offset=0;uint64_t D=0;
    if(!arena(o.arena) || !at(wrapper+8,o.wrapper_resource) || o.wrapper_resource!=wrapper_resource_ ||
       !at(wrapper_resource_+8,D) || D!=identity_.renderer+0x50 || !at(wrapper_resource_+0xb8,size) || size!=592 ||
       !at(wrapper+0x18,offset) || (offset!=UINT32_MAX && (offset>o.arena.capacity || size>o.arena.capacity-offset ||
            (!run_ && (offset>o.arena.cursor || size>o.arena.cursor-offset)))) ||
       !read(marker+8,o.attachment.data(),sizeof o.attachment) || o.attachment!=source_over) return false;
    o.wrapper_size=size;o.wrapper_offset=offset;out=o;return true;
}
bool Evidence::read_callback(void *p,uint64_t a,void *out,size_t n) noexcept {return p && static_cast<Evidence*>(p)->read(a,out,n);}
bool Evidence::write_callback(void *p,uint64_t destination,const void *source,size_t size) noexcept {
    if(!p) return false;
    auto &e=*static_cast<Evidence*>(p);
    if(!e.owner()) {e.staging_poisoned_.store(true);return false;}
    if(!e.initialized() || e.staging_poisoned_.load() || !e.staging_writer_) return false;
    auto *writer=e.staging_writer_;e.staging_writer_=nullptr; // consume BEFORE copy
    const bool ok=writer->staging_write(e.staging_ticket_,destination,source,size);
    if(!ok)e.staging_poisoned_.store(true);
    return ok;
}
bool Evidence::coherent_callback(void *p,uint64_t a) noexcept {return p && static_cast<Evidence*>(p)->coherent(a);}
bool Evidence::live_callback(void *u,const render::Context &c) noexcept {
    if(!u)return false;
    const auto &e=*static_cast<Evidence*>(u);
    return e.graph_capable() && e.dispatch_ && e.graph_scope_ &&
        e.same_identity(c.identity) && c.recording_generation && c.recording_generation==e.recording_;
}
bool Evidence::same_identity(const render::Identity &i) const noexcept {
    return i.process_instance==identity_.process_instance && i.renderer==identity_.renderer &&
        i.renderer_generation==identity_.renderer_generation && i.device==identity_.device &&
        i.device_generation==identity_.device_generation;
}
bool Evidence::observe(void *p,const render::Binding &b,uint64_t w,uint64_t m,render::Observation &out) noexcept {
    out={};if(!p) return false;
    const auto &e=*static_cast<Evidence*>(p);render::Observation o{};
    if(!e.initialized() || !e.identity_.device_generation || !e.same_identity(b.identity) ||
       !e.snapshot(w,m,o) || !o.arena.recording_generation) return false;
    out=o;return true;
}
bool Evidence::arena_event(arena::Bridge &bridge,unsigned site,arena::Saved &s) noexcept {
    if(!initialized() || bridge_!=&bridge ||
       (site!=arena::UboClassify && site!=arena::GeometryClassify && site!=arena::Admission && site!=arena::Reset))return false;
    const auto R=identity_.renderer;
    const auto actual=site==arena::UboClassify?s.x[0]:site==arena::GeometryClassify?s.x[21]:
        site==arena::Reset?s.x[22]:s.x[19];
    if(actual!=R)return false;
    auto refuse=[&] {bridge.failed_=true;bridge.descriptor_={};s.pad=arena::Unsupported;return false;};
    // A pinned suspended instruction grants these scalar reads, not a general
    // readable-memory registry. Neither a lease nor an owned upload is required
    // for stock descriptors, including bootstrap calls before worker publication.
    if(!bridge.bound(R,epoch_) || bridge.scope_.thread!=static_cast<uint64_t>(tid_) || classifier_[0].size)return refuse();
    if(site==arena::UboClassify && s.x[4]!=UINT32_MAX) {
        uint64_t resource=0,device_context=0;uint32_t offset=0,size=0;
        if(!bounded(s.x[2],0x20) || !read_self(s.x[2]+0x18,&offset,4) || offset!=s.x[4] ||
           !read_self(s.x[2]+8,&resource,8) || !bounded(resource,0x128) ||
           !read_self(resource+8,&device_context,8) || device_context!=R+0x50 ||
           !read_self(resource+0xb8,&size,4))return refuse();
        classifier_[0]={s.x[2]+8,resource,8};classifier_[1]={resource+0xb8,size,4};
    } else if(site==arena::GeometryClassify && s.x[3]!=UINT32_MAX) {
        uint32_t offset=0,size=0;
        if(!bounded(s.x[24],0x2b0) || !read_self(s.x[24]+0x1bc,&offset,4) || offset!=s.x[3] ||
           !read_self(s.x[24]+0x1c0,&size,4))return refuse();
        classifier_[0]={s.x[24]+0x1c0,size,4};
    }
    bridge.dispatch(s,site,epoch_,static_cast<uint64_t>(tid_));
    classifier_={}; // no callback return, thread handoff or graph read inherits it
    return s.pad!=arena::Unsupported;
}
bool Evidence::bridge_read_callback(void *p,uint64_t a,void *out,size_t n) noexcept {
    if(!p || !out)return false;
    auto &e=*static_cast<Evidence*>(p);
    if(!e.owner() || e.failed_)return false;
    for(const auto &field:e.classifier_)if(field.size && a==field.address && n==field.size) {
        std::memcpy(out,&field.value,n);return true;
    }
    return e.read(a,out,n);
}
arena::Memory Evidence::bridge_memory() noexcept {return {this,bridge_read_callback,write_callback,coherent_callback};}
binding::PassReader Evidence::pass_reader() noexcept {return {this,read_callback,live_callback};}
#include "graph.inc"
} // namespace native_portal::runtime
