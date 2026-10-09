// SPDX-License-Identifier: MIT
// Local CPU tests. Native event delivery and Vulkan payloads are modeled.
#include "runtime.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <thread>
#include <sys/mman.h>
using namespace native_portal;
using namespace native_portal::live;
int main() {
    static_assert(sizeof(arena::Saved)==800);
    Runtime runtime;
    arena::Saved saved{}; saved.x[0]=42;saved.pad=19;
    auto original=saved;
    runtime.event(0x1234,saved);
    assert(!std::memcmp(&original,&saved,sizeof saved));
    assert(!runtime.pin(0x70000000,"wrong"));
    assert(runtime.pin(0x70000000,native_portal::runtime::elf_sha256));
    assert(!runtime.pin(0x70000000,native_portal::runtime::elf_sha256));
    runtime.event(0x2a1e18,saved);
    auto d=runtime.diagnostics();
    assert(!d.capable && !d.uploads[0] && !d.uploads[1] && !d.queue_success && !d.submitted_frame);
    assert(d.refusal==Refusal::NativeInitialization);
    // Unsupported reads and events must not turn opaque native pointers into authority.
    saved.x[24]=1;runtime.event(0x7fda0,saved);
    assert(!runtime.diagnostics().capable);
    uint64_t out=0;assert(!native_portal::runtime::read_self(1,&out,8));
    // Exact bounded native VkSubmitInfo and timeline pNext parsing, no Vulkan call.
    uint64_t cb=0xc001,semaphore=0xc002,value=0x123400;
    TimelineSubmit timeline{};timeline.type=1000207003;timeline.signal_count=1;timeline.signals=(uint64_t)&value;
    SubmitInfo submit{};submit.type=4;submit.next=(uint64_t)&timeline;submit.command_count=1;submit.commands=(uint64_t)&cb;
    submit.signal_count=1;submit.signals=(uint64_t)&semaphore;
    saved={};saved.x[0]=0xc003;saved.x[1]=1;saved.x[2]=(uint64_t)&submit;
    binding::SubmitSnapshot snapshot{};
    assert(read_submit(saved,0xc004,snapshot));
    assert(snapshot.command==cb && snapshot.semaphore==semaphore && snapshot.value==value && snapshot.queue==0xc003);
    timeline.next=1;assert(!read_submit(saved,0xc004,snapshot));timeline.next=0;
    submit.command_count=2;assert(!read_submit(saved,0xc004,snapshot));submit.command_count=1;
    saved.x[3]=1;assert(!read_submit(saved,0xc004,snapshot));saved.x[3]=0;
    timeline.signals=1;assert(!read_submit(saved,0xc004,snapshot));
    std::puts("runtime: default stock continuation, exact pin, unsupported ownership, bounded real-memory queue payload parsing PASS");
}
