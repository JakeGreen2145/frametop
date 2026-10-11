// SPDX-License-Identifier: MIT
#pragma once
#include "command.hpp"
namespace native_portal::binding {
// IDs match lifecycle_emit.c. Only the offline preparer installs these in Unicorn.
enum class Site : unsigned { CurrentEnter, CurrentPost, RetainedEnter, RetainedPost,
                            ResetEnter, ResetResult, BeginEnter, BeginResult, QueueEnter };
struct Capture {
    uint64_t x[31]{}, sp=0;
    unsigned char q[32][16]{};
    uint64_t nzcv=0,fpcr=0,fpsr=0,pad=0;
};
static_assert(sizeof(Capture)==800);
struct Memory {
    void *user=nullptr;
    bool (*read)(void *,uint64_t,void *,size_t) noexcept=nullptr;
};
// One issuer per process/renderer lifetime, kept across command object reuse.
// Exhaustion refuses; no reset, reseed, copy or assignment operation.
class Generations {
 public:
    Generations() noexcept = default;
    Generations(const Generations &)=delete;
    Generations &operator=(const Generations &)=delete;
    uint64_t issue() noexcept;
 private:
    friend class native_portal::runtime::Evidence;
    friend class Lifecycle;
    const pid_t pid_=getpid();
    std::thread::id thread_=std::this_thread::get_id();
    uint64_t last_=0,claimed_=0;
};
struct LifecycleState {
    bool bound=false,current=false,retained=false,invalidated=false,unsupported=false;
    uint64_t next_generation=0;
};
// One retained old command generation. Never reconstruct this over unsettled debt.
// The checked reader and exact recording attachment remain integration contracts.
class Lifecycle {
 public:
    Lifecycle(Command &,Memory,Generations &) noexcept;
    Lifecycle(const Lifecycle &)=delete;
    Lifecycle &operator=(const Lifecycle &)=delete;
    bool bind(const Owner &,uint64_t native_tid) noexcept;
    // Observation only: false is NOT permission to suppress a native instruction.
    // generation is the immutable old owner, never a lookup through current R.
    bool capture(Site,const Capture &,uint64_t tid,uint64_t generation,uint64_t vk_command) noexcept;
    // Pure scalar preflight. No graph/command memory read and no transition.
    bool disposal_preflight(Site,uint64_t renderer,uint64_t sp) const noexcept;
    LifecycleState state() const noexcept;
    bool settled(const render::Binding &,const render::Provenance &,const render::Provenance &,
                 const render::Completion &) const noexcept;
    static bool settled_callback(void *,const render::Binding &,const render::Provenance &,
                                 const render::Provenance &,const render::Completion &) noexcept;
 private:
    bool fail() noexcept;
    bool read(uint64_t,void *,size_t) const noexcept;
    bool word(uint64_t,uint64_t &) const noexcept;
    bool scan(uint64_t vector_offset,unsigned &mask) const noexcept;
    bool complete() const noexcept;
    Command &command_;
    const Memory memory_;
    Generations &ids_;
    Owner owner_{};
    LifecycleState state_{};
    uint64_t tid_=0,current_sp_=0,retained_sp_=0,reset_sp_=0,reset_lr_=0;
    uint64_t begin_sp_=0,retained_base_=0;
    bool empty(uint64_t) const noexcept;
    bool queued_=false,current_open_=false,retained_open_=false,reset_open_=false;
    bool begin_expected_=false,begin_open_=false;
};
}
