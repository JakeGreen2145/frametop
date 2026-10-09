// SPDX-License-Identifier: MIT
#pragma once
#include "../binding/pass.hpp"
#include "../binding/lifecycle.hpp"
#include <optional>
#include <atomic>
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
// Recording attachment is event-issued; dynamic graph retention is not proved.
// PassReader::live remains unsupported. Staging is a separate call-local borrow.
struct PublicationDiagnostic {
    uint64_t predicate=0,phase=0,flags=0,checked=0,passed=0;
    uint64_t caller=0,return_pc=0,recording=0,generation=0,renewal=0,entries=0,attempt=0;
};
// Private command-ledger observations. Scalars only; these never authorize reads.
struct RecordingDiagnostic {
    uint64_t member=0,vk=0,generation=0,renewal=0,begun=0,entries=0;
    uint64_t recording=0,creation=0,phase=0,flags=0;
};
struct CommandDiagnostic {
    uint64_t pc=0,command=0,native_vk=0,accepted=0,failure=0;
    RecordingDiagnostic before{},after{};
};
inline bool command_trace_pc(uint64_t pc) noexcept {
    switch(pc) {
    case 0x301808:case 0x30180c:case 0x300a08:case 0x300a38:
    case 0x300b20:case 0x300c54:case 0x3003d0:case 0x311648:case 0x4b3d0:
    case 0x305260:case 0x2953a0:case 0x29a528:case 0x2a23f4:
    case 0x2a1e18:case 0x2a2230:case 0x2a2320: return true;
    default:return false;
    }
}
class Evidence {
 public:
    Evidence() noexcept;
    Evidence(const Evidence &)=delete;
    Evidence &operator=(const Evidence &)=delete;
    bool pin(uint64_t image_base, const char *sha256) noexcept;
    bool event(uint64_t relative_pc, const arena::Saved &) noexcept;
    // Compose only bootstrap Bridge sites; borrow allocation output for the
    // duration of this call, then revoke before native resumes/clears the slot.
    bool route(arena::Bridge &,uint64_t relative_pc,arena::Saved &) noexcept;
    // Synchronous classifier fields only, never a graph or persistent allocation borrow.
    bool arena_event(arena::Bridge &,unsigned site,arena::Saved &) noexcept;
    // Paired direct Adapter-owned native calls; no user-authored wrapper pointer.
    bool call_enter(const render::Registers &) noexcept;
    bool call_return(const render::Registers &) noexcept;
    // Exact pinned camera call/return/enqueue scopes. Eye/origin/standing remain
    // independent authenticated frame-owner inputs, never native readiness.
    // 161d64 returns a nonreused invocation ticket. Pass that exact ticket to
    // 161d68 and 161ed8; 161f00 aborts an unfinished invocation before epilogue.
    // Upload success means original BLR once; enqueue success means SkipOwned
    // with LR=161edc because Adapter already issued the replacement call.
    // False never means stock fallback. No pointer is registered by this API.
    uint64_t stage(render::Adapter &,uint64_t pc,arena::Saved &,const render::EyeInput &,
                   uint64_t invocation=0) noexcept;
    bool initialized() const noexcept;
    // Optional predicate result is call-local, never authority or a second reader.
    bool owner_event(uint64_t,const arena::Saved &,uint16_t *rejection=nullptr) noexcept;
    uint64_t first_failure_pc() const noexcept {return first_failure_pc_.load();}
    // Owner-only first invalidating publication, copied through the router mailbox.
    PublicationDiagnostic publication_diagnostic() const noexcept {return owner()?publication_:PublicationDiagnostic{};}
    CommandDiagnostic command_diagnostic() const noexcept {return owner()?command_diagnostic_:CommandDiagnostic{};}
    bool graph_event(uint64_t,const arena::Saved &) noexcept;
    bool graph_capable() const noexcept;
    bool settled(const render::Binding &,const render::Provenance &,const render::Provenance &,const render::Completion &) const noexcept;
    bool command_owner(binding::Owner &) const noexcept;
    bool release_lifecycle() noexcept;
    // Command must outlive Evidence. No caller generation or reader is accepted.
    // Owner's queue/signal identities still belong to the separate Command binding.
    bool bind_lifecycle(binding::Command &,const binding::Owner &) noexcept;
    render::Identity identity() const noexcept;
    bool coherent(uint64_t resource) const noexcept;
    bool snapshot(uint64_t wrapper,uint64_t marker,render::Observation &) const noexcept;
    arena::Memory bridge_memory() noexcept;
    binding::PassReader pass_reader() noexcept;
    static bool observe(void *, const render::Binding &,uint64_t,uint64_t,render::Observation &) noexcept;
 private:
    struct Span { uint64_t begin=0,size=0,epoch=0; };
    const pid_t pid_;pid_t tid_;
    uint64_t worker_=0,context_=0,scheduler_=0,application_=0;
    std::atomic<unsigned> handoff_{0};
    bool run_=false,dispatch_=false,graph_scope_=false,graph_owned_=false,transferred_=false;
    uint64_t dispatch_sp_=0;
    struct GraphSpan {uint64_t begin=0,size=0;};
    std::array<GraphSpan,196> graph_spans_{};
    size_t graph_count_=0;
    bool graph_capture(bool attachments) noexcept;
    bool graph_read(uint64_t,void *,size_t) const noexcept;
    uint64_t base_=0,epoch_=0,allocation_sp_=0,allocation_slot_=0,borrowed_slot_=0;
    arena::Bridge *bridge_=nullptr;
    struct ClassifierField {uint64_t address=0,value=0;size_t size=0;};
    std::array<ClassifierField,2> classifier_{};
    static bool bridge_read_callback(void *,uint64_t,void *,size_t) noexcept;
    // Native call-local borrow; never exported as a general read/write registry.
    render::Adapter *staging_writer_=nullptr;
    const render::Adapter *staging_owner_=nullptr; // compared only, never dereferenced
    uint64_t camera_=0,stock_=0,stock_resource_=0,staging_sp_=0,staging_ticket_=0;
    render::EyeInput staging_input_{};
    bool staging_reads_=false,staging_returned_=false;
    std::atomic<bool> staging_poisoned_{false};
    bool staging_read(uint64_t,void *,size_t) const noexcept;
    void clear_staging() noexcept;
    uint64_t resource_=0,command_wrapper_=0,wrapper_=0,wrapper_resource_=0,marker_=0;
    uint64_t call_=0,backing_=0,map_=0,buffer_=0,allocation_bytes_=0;
    uint32_t requested_size_=0,type_index_=0,type_flags_=0,type_count_=0;
    render::Identity identity_{};
    std::array<Span,8> spans_{};
    struct Recording { uint64_t command=0,vk=0,generation=0;bool begun=false;
        uint64_t reset_sp=0,reset_lr=0;unsigned renewal=0;
        uint64_t stock_pool=0;bool published=false; };
    std::array<Recording,64> recordings_{};
    binding::Generations generations_;
    std::optional<binding::Lifecycle> lifecycle_;
    binding::Owner lifecycle_owner_{};
    uint64_t creation_=0,creation_sp_=0,begin_sp_=0,begin_vk_=0,recording_=0;
    bool begin_open_=false,begin_success_=false;
    struct Turnover {
        uint64_t sp=0,command=0,wrapper=0,vk=0,generation=0,pool=0;
        unsigned phase=0;
        uint64_t caller=0;
    } turnover_;
    PublicationDiagnostic publication_{};
    uint64_t publication_attempts_=0;
    CommandDiagnostic command_diagnostic_{};
    RecordingDiagnostic recording_diagnostic(uint64_t) const noexcept;
    void trace_command(uint64_t command,uint64_t vk=0) noexcept;
    bool event_impl(uint64_t,const arena::Saved &) noexcept;
    bool recording_event(uint64_t,const arena::Saved &) noexcept;
    bool same_identity(const render::Identity &) const noexcept;
    bool pinned_=false,constructed_=false,ready_=false,failed_=false,allocation_open_=false;
    bool owner() const noexcept;
    bool add(unsigned,uint64_t,size_t) noexcept;
    bool read(uint64_t,void *,size_t) const noexcept;
    template<class T> bool at(uint64_t p,T &v) const noexcept {return read(p,&v,sizeof v);}
    bool arena(render::Arena &) const noexcept;
    std::atomic<uint64_t> first_failure_pc_{0};
    bool fail(uint64_t pc) noexcept;
    static bool read_callback(void *,uint64_t,void *,size_t) noexcept;
    static bool command_read_callback(void *,uint64_t,void *,size_t) noexcept;
    static bool write_callback(void *,uint64_t,const void *,size_t) noexcept;
    static bool coherent_callback(void *,uint64_t) noexcept;
    static bool live_callback(void *,const render::Context &) noexcept;
};
} // namespace native_portal::runtime
