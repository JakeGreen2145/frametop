// SPDX-License-Identifier: MIT
// Test-only TU, compiled with -fno-access-control instead of changing production
// visibility. Includes the unchanged Runtime implementation, replacing its object
// ONLY in the focused CPU fixture. No state writes or authority shortcuts.
#include "runtime.cpp"
#include <cstdio>
using namespace native_portal;
extern "C" void present_snapshot(live::Runtime& runtime,const char* mode,const char* phase,unsigned skips,unsigned lookups) {
    auto& p=*runtime.p_;
    if(!std::strcmp(phase,"joined")) {
        // The worker has exited. Do not call owner-thread-only readers here.
        const auto d=runtime.diagnostics();
        std::fprintf(stderr,"CONSUMER mode=%s phase=joined pending=%u submitted_frame=%llu\n",
            mode,d.pending,(unsigned long long)d.submitted_frame);
        return;
    }
    const auto a=p.adapter?p.adapter->state():render::State{};
    const auto c=p.command?p.command->state():binding::State{};
    const auto reader=p.evidence.pass_reader();
    const binding::Context context{p.binding.identity,p.command_owner.recording};
    const bool pass_live=reader.live && reader.live(reader.user,context);
    // Re-run only the read-only production scanner at the suspended dispatch.
    // An unavailable reader must return Lifetime without reading native graphs.
    int reason=-1;size_t owned=0;
    if(!std::strcmp(phase,"dispatch")) {
        const auto scan=binding::extract_passes(reader,{context,p.marker,p.base,0});
        reason=int(scan.reason);owned=scan.owned;
    }
    std::fprintf(stderr,"CONSUMER mode=%s phase=%s graph=%u recording=%llu pass_live=%u pass_reason=%d scan_owned=%zu first_evidence=0x%llx first_stock=0x%llx pair=%llu committed=%u rejected=%u skips=%u lookups=%u tickets=%zu adapter_pending=%u camera_debt=%llu source_sealed=%u source_bad=%u source_count=%zu command=%u recording_debt=%u success_debt=%u certificate=%u gpu_complete=%u cpu_done=%u cpu_settled=%u queue_observed=%u stereo_submitted=%u submitted_frame=%llu uploads=%llu,%llu encoded=%llu,%llu queue=%llu refusal=%u pending=%u\n",
        mode,phase,p.evidence.dispatch_,(unsigned long long)p.evidence.recording_,pass_live,reason,owned,
        (unsigned long long)p.evidence.first_failure_pc(),(unsigned long long)p.stock_facts.failure_pc(),(unsigned long long)a.pair_generation,
        a.committed,a.rejected,skips,lookups,a.tickets_used,a.pending,(unsigned long long)p.camera_debt,
        p.source_command.sealed,p.source_command.bad,size_t(p.source_command.count),bool(p.command),c.recording_debt,c.success_debt,c.certificate,
        a.gpu_complete,a.cpu_done,a.cpu_settled,a.queue_observed,a.stereo_submitted,(unsigned long long)p.diag.submitted_frame,
        (unsigned long long)p.diag.uploads[0],(unsigned long long)p.diag.uploads[1],
        (unsigned long long)p.diag.encoded[0],(unsigned long long)p.diag.encoded[1],
        (unsigned long long)p.diag.queue_success,unsigned(p.diag.refusal),p.diag.pending);
}
