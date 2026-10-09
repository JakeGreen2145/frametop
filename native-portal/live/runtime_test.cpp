// SPDX-License-Identifier: MIT
// Local CPU tests. Native event delivery and Vulkan payloads are modeled.
#include "runtime.hpp"
#include "diagnostics.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <thread>
#include <sys/mman.h>
using namespace native_portal;
using namespace native_portal::live;
int main() {
    DiagnosticLog log;LiveDiagnostics live{};char line[DiagnosticLog::line_capacity];
    assert(!log.format(live,0,line,1)); // truncated output does not spend the budget
    assert(log.format(live,0,line,sizeof line));
    live.pc=0x1480b8;live.events=10000;live.frames=1000;
    assert(!log.format(live,0,line,sizeof line));
    for(unsigned i=1;i<DiagnosticLog::limit;++i) {
        live.capable=!live.capable;assert(log.format(live,0,line,sizeof line));
    }
    assert(std::strstr(line,"camera_read="));
    assert(std::strstr(line,"camera_cache="));
    assert(std::strstr(line,"camera_flags="));
    assert(std::strstr(line,"camera_handles="));
    assert(std::strstr(line,"idle_first="));
    assert(std::strstr(line,"ready_first_ns="));
    assert(std::strstr(line,"lease_first_ns="));
    assert(std::strstr(line,"remaining=0\n"));
    for(unsigned i=0;i<1000;++i) {
        live.capable=!live.capable;assert(!log.format(live,63,line,sizeof line));
    }
    DiagnosticLog facts;LiveDiagnostics facts_only{};
    assert(facts.format(facts_only,0,line,sizeof line));
    facts_only.first_owner_rejection=(uint64_t(0x140dbc)<<16)|13;
    assert(facts.format(facts_only,0,line,sizeof line));
    assert(std::strstr(line,"first_owner_pc=0x140dbc first_owner_pred=13"));
    facts_only.first_evidence_pc=0x305260;
    assert(facts.format(facts_only,0,line,sizeof line));
    facts_only.first_stop_pc=0x29bd18;
    assert(facts.format(facts_only,0,line,sizeof line));
    assert(std::strstr(line,"first_evidence_pc=0x305260 first_stop_pc=0x29bd18"));
    assert(!facts.format(facts_only,0,line,sizeof line));
    DiagnosticLog association;LiveDiagnostics sample{};
    assert(association.format(sample,0,line,sizeof line));
    for(auto field:{&LiveDiagnostics::assoc_checked,&LiveDiagnostics::assoc_passed,
                    &LiveDiagnostics::assoc_failed,&LiveDiagnostics::assoc_lr}) {
        sample.*field=1;assert(association.format(sample,0,line,sizeof line));
        assert(!association.format(sample,0,line,sizeof line));
    }
    sample.assoc_owner=true;assert(association.format(sample,0,line,sizeof line));
    sample.assoc_frame=100;assert(!association.format(sample,0,line,sizeof line));
    assert(std::strstr(line,"assoc_checked=0x1 assoc_passed=0x1 assoc_failed=1 assoc_owner=1 assoc_lr=0x1"));
    DiagnosticLog policy_log;LiveDiagnostics policy_sample{};
    assert(policy_log.format(policy_sample,0,line,sizeof line));
    for(auto field:{&PolicyDiagnostic::checked,&PolicyDiagnostic::passed,&PolicyDiagnostic::failed,
                    &PolicyDiagnostic::seen,&PolicyDiagnostic::actual,&PolicyDiagnostic::expected,
                    &PolicyDiagnostic::style,&PolicyDiagnostic::expected_style,&PolicyDiagnostic::alpha,&PolicyDiagnostic::expected_alpha}) {
        policy_sample.policy_detail.*field=1;assert(policy_log.format(policy_sample,0,line,sizeof line));
        assert(!policy_log.format(policy_sample,0,line,sizeof line));
    }
    policy_sample.policy_detail.sampled=true;assert(policy_log.format(policy_sample,0,line,sizeof line));
    policy_sample.frames=100;assert(!policy_log.format(policy_sample,0,line,sizeof line));
    policy_sample.assoc_fallback=true;assert(policy_log.format(policy_sample,0,line,sizeof line));
    DiagnosticLog camera_log;LiveDiagnostics camera_sample{};
    assert(camera_log.format(camera_sample,0,line,sizeof line));
    for(auto field:{&LiveDiagnostics::camera_cache,&LiveDiagnostics::camera_flags,&LiveDiagnostics::camera_h0,
                    &LiveDiagnostics::camera_h1,&LiveDiagnostics::idle_first,&LiveDiagnostics::ready_first_ns,
                    &LiveDiagnostics::lease_first_ns}) {
        camera_sample.*field=1;assert(camera_log.format(camera_sample,0,line,sizeof line));
        assert(!camera_log.format(camera_sample,0,line,sizeof line));
    }
    camera_sample.camera_read=true;assert(camera_log.format(camera_sample,0,line,sizeof line));
    camera_sample.ready_last_ns=100;assert(!camera_log.format(camera_sample,0,line,sizeof line));
    static_assert(sizeof(Diagnostics)==96); // exported mailbox ABI stays unchanged
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
