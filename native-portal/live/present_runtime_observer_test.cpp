// SPDX-License-Identifier: MIT
// Test-only TU, compiled with -fno-access-control instead of changing production
// visibility. Includes the unchanged Runtime implementation, replacing its object
// ONLY in the focused CPU fixture. No state writes or authority shortcuts.
#include "runtime.cpp"
#include <cstdio>
#include <cassert>
using namespace native_portal;
extern "C" unsigned graph_test_reads();
extern "C" void graph_test_probe(live::Runtime& runtime,const char* phase) {
    auto& p=*runtime.p_;const char* raw=std::getenv("FT_GRAPH_ACTION");
    if(!raw || !std::strcmp(phase,"joined"))return;
    const std::string action=raw;
    static std::atomic<unsigned> pumped{0};static uint64_t submitted=0;
    const auto g=p.dormant_admission.graph();
    if(action=="early" && !g.invocation && std::strcmp(phase,"recorded"))return;
    const bool lookup=!std::strncmp(phase,"lookup-",7);
    if(lookup || !std::strcmp(phase,"dispatch") || !std::strcmp(phase,"packets")) {
        assert(g.invocation && g.preparation && p.graph_live() && p.camera_owner.callers_==1);
        assert(!p.current_render_active);
    }
    if(!std::strcmp(phase,"lookup-second"))assert(g.pipelines[0]);
    if(!std::strcmp(phase,"dispatch") && std::getenv("FT_GRAPH_DISTINCT"))assert(g.pipelines[0] && g.pipelines[1] && g.pipelines[0]!=g.pipelines[1]);
    if(!std::strcmp(phase,"current-open")) {
        assert(p.graph_active() && !g.preparation && !p.graph_live() && p.camera_owner.callers_==1);
        // This checks only the adapter guard. The complete native old-W sized
        // free/factory replay belongs to the parent's reviewed retirement merge.
        arena::Saved s{};s.x[0]=g.wrapper;const auto copy=s;p.graph_conflict(0x4b3d0,s);
        assert(!std::memcmp(&copy,&s,sizeof s));
    }
    if(action=="disposal" && !std::strcmp(phase,"current-open")) {
        arena::Saved s{};s.sp=p.graph_loan.sp-0xd0;s.x[19]=p.binding.identity.renderer;
        auto before=graph_test_reads();assert(!p.evidence.graph_event(0x2a2230,s) && graph_test_reads()==before);
        s.sp+=16;before=graph_test_reads();assert(!p.evidence.graph_event(0x2a2320,s) && graph_test_reads()==before);
        std::fputs("GRAPH current duplicate-pre/wrong-post zero-reads=1\n",stderr);
    }
    if((action=="retained-wrong-post" || action=="retained-duplicate-pre") && !std::strcmp(phase,"retained-open")) {
        arena::Saved s{};s.sp=p.evidence.retained_anchor_sp_;s.x[22]=p.binding.identity.renderer;
        if(action=="retained-wrong-post")s.sp+=16;
        const auto before=graph_test_reads();
        assert(!p.evidence.graph_event(action=="retained-wrong-post"?0x298840:0x2987d8,s) && graph_test_reads()==before);
        std::fputs("GRAPH retained rejected zero-reads=1 debt-retained=1\n",stderr);
    }
    if(action=="pump") {
        if(!std::strcmp(phase,"lookup-first")) {
            pumped=0;
            std::thread foreign([&]{
                auto s=p.camera_owner.submit(p.camera_owner.epoch(),p.camera_binding.renderer_generation,
                    [&](const auto&,auto&){++pumped;return coordination::NativeReceipt{};});
                assert(s.id);submitted=s.id;
            });foreign.join();
        }
        if(lookup || !std::strcmp(phase,"dispatch") || !std::strcmp(phase,"packets")) {
            p.camera_maintenance(false);assert(!pumped);
        }
        if(!std::strcmp(phase,"disposal")) {
            assert(pumped==1);coordination::Result result;assert(p.camera_owner.take(submitted,result));
            assert(result.retention_released);std::fputs("GRAPH pump deferred-across-both-eyes=1 drained-after-return=1\n",stderr);
        }
    }
    if(action=="close" && !std::strcmp(phase,"dispatch")) {
        runtime.stop();p.camera_maintenance(true);
        assert(p.dormant_admission.phase()==live::RuntimeAdmission::Phase::Closing);
        assert(!p.dormant_admission.close_ready(p.dormant_version) && p.graph_active());
        std::fputs("GRAPH close retained=1 debt-unchanged=1\n",stderr);
    }
    if(action=="unmatched" && !std::strcmp(phase,"dispatch")) {
        arena::Saved s{};s.sp=p.graph_loan.sp+16;s.x[30]=p.graph_loan.lr;
        for(auto pc:{0x2a2498u,0x2a2564u}){runtime.event(pc,s);assert(p.graph_active());}
        s.sp=p.graph_loan.sp;s.x[30]+=4;runtime.event(0x2a2498,s);assert(p.graph_active());
        std::fputs("GRAPH unmatched preserved-outer=1\n",stderr);
    }
    if(action=="early" && !std::strcmp(phase,"dispatch")) {
        arena::Saved s{};s.sp=p.graph_loan.sp;s.x[30]=p.graph_loan.lr;runtime.event(0x2a2564,s);
        assert(!p.graph_active() && !p.camera_owner.callers_ && p.adapter->state().pending);
        assert(!p.evidence.dispatch_ && !p.evidence.graph_scope_);
        std::fputs("GRAPH early last-use=1 debt-retained=1 no-settlement=1\n",stderr);
    }
    if(action=="disposal" && !std::strcmp(phase,"wait")) {
        arena::Saved s{};s.sp=p.graph_loan.sp-0xd0;s.x[19]=s.x[22]=p.binding.identity.renderer;
        auto reject=[&](uint64_t pc,arena::Saved bad){const auto before=graph_test_reads();
            assert(!p.evidence.graph_event(pc,bad) && graph_test_reads()==before);};
        reject(0x2a2320,s);reject(0x2987d8,s);reject(0x298840,s);
        auto bad=s;bad.sp+=16;reject(0x2a2230,bad);
        std::fputs("GRAPH disposal wrong-sp/missing-pre zero-reads=1\n",stderr);
    }
    if(action=="disposal" && !std::strcmp(phase,"disposal")) {
        arena::Saved s{};s.sp=0x12300;s.x[19]=s.x[22]=p.binding.identity.renderer;
        for(auto pc:{0x2a2230u,0x2a2320u,0x2987d8u,0x298840u}) {
            const auto before=graph_test_reads();assert(!p.evidence.graph_event(pc,s) && graph_test_reads()==before);
        }
        std::fputs("GRAPH disposal duplicate zero-reads=1\n",stderr);
    }
    const bool second=action=="second-free" && !std::strcmp(phase,"lookup-second");
    if(second || (!std::strcmp(phase,"dispatch") && (action.rfind("guard-",0)==0 || action.rfind("free-",0)==0))) {
        arena::Saved s{};s.sp=p.graph_loan.sp;s.x[0]=g.renderer;
        uint64_t pc=0x4b3d0;
        if(action.rfind("guard-",0)==0)pc=std::stoull(action.substr(6),nullptr,16);
        if(action=="free-w")s.x[0]=g.wrapper;
        if(action=="free-c")s.x[0]=g.command;
        if(action=="free-p0" || second)s.x[0]=g.pipelines[0];
        if(action=="free-p1")s.x[0]=g.pipelines[1];
        if(pc==0x15fcc0 || pc==0x15fba8)s.x[0]=p.camera_binding.camera;
        if(pc==0x8b020)s.x[0]=p.camera_binding.application;
        std::fprintf(stderr,"GRAPH conflict before-native pc=0x%llx\n",(unsigned long long)pc);
        runtime.event(pc,s);std::fputs("GRAPH forbidden original continuation\n",stderr);_Exit(72);
    }
    if(action=="unrelated" && !std::strcmp(phase,"dispatch")) {
        arena::Saved s{};s.x[0]=0x12300;s.x[1]=0x12340;const auto saved=s;
        for(auto pc:{0x2983c8u,0x298750u,0x29ce00u,0x29b278u,0x4b3d0u}) {
            runtime.event(pc,s);assert(!std::memcmp(&s,&saved,sizeof s));assert(p.graph_active());
        }
        std::fputs("GRAPH unrelated originals-unchanged=1\n",stderr);
    }
}
extern "C" void present_snapshot(live::Runtime& runtime,const char* mode,const char* phase,unsigned skips,unsigned lookups) {
    auto& p=*runtime.p_;
    if(std::getenv("FT_GRAPH_OPTIMIZER") && !std::strcmp(phase,"dispatch")) {
        assert(!p.graph_live() && !lookups && "unsupported optimizer must revoke preparation before native mutation");
        arena::Saved s{};s.x[0]=p.binding.identity.renderer;s.sp=p.graph_loan.sp-0xd0;s.x[30]=p.base+0x2a1ecc;
        const auto saved=s;runtime.event(0x29ce00,s);assert(!std::memcmp(&s,&saved,sizeof s));
        std::fputs("GRAPH ordinary optimizer preserved no-prepare=1\n",stderr);
    }
    if(std::getenv("FT_GRAPH_PROVIDER") && !std::strcmp(phase,"dispatch")) {
        assert(!lookups && "unqualified native resource provider must not receive added lookup");
    }
    if(std::getenv("FT_GRAPH_DISPOSAL") && !std::strcmp(phase,"recorded")) {
        arena::Saved s{};s.sp=0x12300;s.x[19]=s.x[22]=p.binding.identity.renderer;
        for(auto pc:{0x2a2230u,0x2a2320u,0x2987d8u,0x298840u}) {
            const auto before=graph_test_reads();
            const bool accepted=p.evidence.graph_event(pc,s);
            assert(!accepted && graph_test_reads()==before && "unsolicited disposal must not capture graph");
        }
    }
    if(std::getenv("FT_GRAPH_PROBE") && !std::strcmp(mode,"consumer-flush") && !std::strcmp(phase,"dispatch")) {
        assert(p.camera_owner.callers_==1 && "dispatch must retain its own caller after camera Render returned");
    }
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
