// SPDX-License-Identifier: MIT
#pragma once
#include "../runtime/evidence.hpp"
#include "../binding/pass_gate.hpp"
#include <memory>
namespace native_portal::live {
// Exact 64-bit Vulkan submit layouts for bounded self-reads. The production
// source also includes the real pipeline ABI header (and its Vulkan SDK types).
// Only the separately loaded pipeline DSO makes Vulkan calls.
struct SubmitInfo {
    uint32_t type=0,pad=0; uint64_t next=0;
    uint32_t wait_count=0,pad2=0;uint64_t waits=0,stages=0;
    uint32_t command_count=0,pad3=0;uint64_t commands=0;
    uint32_t signal_count=0,pad4=0;uint64_t signals=0;
};
struct TimelineSubmit {
    uint32_t type=0,pad=0;uint64_t next=0;
    uint32_t wait_count=0,pad2=0;uint64_t waits=0;
    uint32_t signal_count=0,pad3=0;uint64_t signals=0;
};
static_assert(sizeof(SubmitInfo)==72 && sizeof(TimelineSubmit)==48);
bool read_submit(const arena::Saved &,uint64_t device,binding::SubmitSnapshot &) noexcept;
// Stable diagnostic reasons, no logs, heap strings or camera pixels in callbacks.
enum class Refusal : uint32_t {
    None, NativeInitialization, ThreadHandoff, GraphOwnership, NativeRead,
    PipelineAbi, Origin, Policy, Tracking, Adapter, Command, Stopped, ForeignThread
};
struct Diagnostics {
    uint64_t events=0,frames=0,uploads[2]{},encoded[2]{},queue_success=0;
    uint64_t submitted_frame=0,origin=0,renderer=0,recording=0;
    Refusal refusal=Refusal::NativeInitialization;
    bool capable=false,pending=false,initialized=false,stopped=false;
};
class Runtime {
    struct Impl;std::unique_ptr<Impl> p_;
public:
    Runtime();~Runtime();
    Runtime(const Runtime&)=delete;Runtime& operator=(const Runtime&)=delete;
    bool pin(uint64_t image_base,const char *sha256) noexcept;
    // Called by launch installer before native callbacks, never from a draw.
    bool start_server(const char *runtime_directory);
    void event(uint64_t relative_pc,arena::Saved &) noexcept;
    Diagnostics diagnostics() const noexcept;
    void stop() noexcept; // stop new commits, not native lifetime settlement
};
}
extern "C" {
__attribute__((visibility("default"))) bool ft_native_portal_start(uint64_t image_base,const char *sha256,const char *runtime_directory) noexcept;
__attribute__((visibility("default"))) void ft_native_portal_event(uint64_t relative_pc,native_portal::arena::Saved *saved) noexcept;
__attribute__((visibility("default"))) bool ft_native_portal_diagnostics(native_portal::live::Diagnostics *out,size_t size) noexcept;
__attribute__((visibility("default"))) void ft_native_portal_stop() noexcept;
// AAPCS64 transport. x8 is an actual indirect result pointer, not an x1 argument.
bool ft_native_portal_call(uint64_t target,uint64_t *x,uint64_t indirect_result) noexcept;
}
