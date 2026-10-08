// SPDX-License-Identifier: MIT
#pragma once
#include "../binding/pass.hpp"
#include <sys/types.h>

namespace native_portal::runtime {
inline constexpr char elf_sha256[] = "9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265";
// Fault-contained copy, NOT ownership evidence. Only the current process is read.
// Exact-size success required; a failed partial read may modify the destination.
bool read_self(uint64_t, void *, size_t) noexcept;

// Inactive, serialized native-event consumer. No installer or arbitrary memory
// registration. The trusted trampoline must route ALL paired events and revoke
// before native teardown. Event arguments are exact saved native registers, not
// IPC inputs. A digest supplied here is a preparer contract, not live-image proof.
// Native objects remain owned by the compositor; this object owns only evidence.
// Missing recording and pass-graph lifetime proof deliberately prevents observe
// and PassReader::live from authorizing Adapter work. snapshot is diagnostic.
class Evidence {
 public:
    Evidence() noexcept;
    Evidence(const Evidence &)=delete;
    Evidence &operator=(const Evidence &)=delete;
    bool pin(uint64_t image_base, const char *sha256) noexcept;
    bool event(uint64_t relative_pc, const arena::Saved &) noexcept;
    // Paired direct Adapter-owned native calls; no user-authored wrapper pointer.
    bool call_enter(const render::Registers &) noexcept;
    bool call_return(const render::Registers &) noexcept;
    bool initialized() const noexcept;
    render::Identity identity() const noexcept;
    bool coherent(uint64_t resource) const noexcept;
    bool snapshot(uint64_t wrapper,uint64_t marker,render::Observation &) const noexcept;
    arena::Memory bridge_memory() noexcept;
    binding::PassReader pass_reader() noexcept;
    static bool observe(void *, const render::Binding &,uint64_t,uint64_t,render::Observation &) noexcept;
 private:
    struct Span { uint64_t begin=0,size=0,epoch=0; };
    const pid_t pid_,tid_;
    uint64_t base_=0,epoch_=0,allocation_sp_=0,allocation_slot_=0;
    uint64_t resource_=0,command_wrapper_=0,wrapper_=0,wrapper_resource_=0,marker_=0;
    uint64_t call_=0,backing_=0,map_=0,buffer_=0,allocation_bytes_=0;
    uint32_t requested_size_=0,type_index_=0,type_flags_=0,type_count_=0;
    render::Identity identity_{};
    std::array<Span,6> spans_{};
    bool pinned_=false,constructed_=false,ready_=false,failed_=false,allocation_open_=false;
    bool owner() const noexcept;
    bool add(unsigned,uint64_t,size_t) noexcept;
    bool read(uint64_t,void *,size_t) const noexcept;
    template<class T> bool at(uint64_t p,T &v) const noexcept {return read(p,&v,sizeof v);}
    bool arena(render::Arena &) const noexcept;
    bool fail() noexcept;
    static bool read_callback(void *,uint64_t,void *,size_t) noexcept;
    static bool write_callback(void *,uint64_t,const void *,size_t) noexcept;
    static bool coherent_callback(void *,uint64_t) noexcept;
    static bool live_callback(void *,const render::Context &) noexcept;
};
} // namespace native_portal::runtime
