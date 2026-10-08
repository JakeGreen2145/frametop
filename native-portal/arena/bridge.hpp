// SPDX-License-Identifier: MIT
#pragma once
#include "policy.hpp"
#include <array>
#include <cstddef>
namespace native_portal::arena {
// Exact observer bridge ABI. pad is an arena-specific continuation selector.
struct Saved { uint64_t x[31],sp; unsigned char q[32][16]; uint64_t nzcv,fpcr,fpsr,pad; };
static_assert(sizeof(Saved)==800);
enum Site : unsigned { Upload, Allocate, Admission, UboClassify, GeometryClassify,
                       Initialized, Reset, UploadReturn, AllocationReturn, SiteCount };
enum Route : uint64_t { Original, Replacement, SkipOwned, Unsupported };
struct Memory {
    void* user=nullptr;
    bool (*read)(void*,uint64_t,void*,size_t) noexcept=nullptr;
    bool (*write)(void*,uint64_t,const void*,size_t) noexcept=nullptr;
    // Binding-issued evidence from the native memory selection / mapping route.
    // A mapped pointer alone is NOT coherence proof. Missing callback refuses.
    bool (*coherent)(void*,uint64_t resource) noexcept=nullptr;
};
struct Scope { uint64_t renderer=0,lifetime=0,thread=0,allocator=0,base=0; };
struct Owned {
    uint64_t camera=0,stock_wrapper=0,wrapper=0,staging=0,ticket=0;
    std::array<unsigned char,64> portal{};
};
struct Descriptor { uint64_t buffer=0,offset=0,range=0,ticket=0; };
// No global instance, constructor install, executable mapping, or runtime calls.
// One owner thread serializes arm/offer/dispatch. The caller authenticates the
// frame/ticket before offer and retains its immutable ownership through enqueue.
class Bridge {
    Memory mem_; Scope scope_{}; Policy policy_; Owned owned_{}; Descriptor descriptor_{};
    bool armed_=false,requested_=false,used_=false,charged_=false,failed_=false;
    uint64_t resource_=0,map_=0,buffer_=0,offset_=0,allocation_slot_=0,allocated_=0,owned_wrapper_=0;
    bool read(uint64_t,void*,size_t) noexcept;
    template<class T> bool get(uint64_t a,T& v) noexcept {return read(a,&v,sizeof v);}
    bool context(uint64_t renderer) noexcept;
public:
    explicit Bridge(Memory memory) noexcept : mem_(memory) {}
    bool arm(Scope,bool pinned) noexcept;
    bool offer(const Owned&) noexcept;
    bool geometry_matches(uint64_t camera,const void* matrices) noexcept;
    void skip(Saved& s) const noexcept {s.pad=SkipOwned;s.x[30]=scope_.base+0x161d68;}
    void dispatch(Saved&,unsigned site,uint64_t lifetime,uint64_t thread) noexcept;
    bool ready() const noexcept {return policy_.ready() && !failed_;}
    bool bound(uint64_t renderer,uint64_t lifetime) const noexcept {
        return ready() && scope_.renderer==renderer && scope_.lifetime==lifetime;
    }
    // End one authenticated invocation, not its reservation/reset interval.
    bool release(uint64_t ticket) noexcept {
        if(!ticket || owned_.ticket!=ticket) return false;
        owned_={};descriptor_={};used_=charged_=false;return true;
    }
    bool descriptor(Descriptor& d) const noexcept {if(!descriptor_.ticket || failed_) return false; d=descriptor_; return true;}
    const Policy& policy() const noexcept {return policy_;}
};
}
