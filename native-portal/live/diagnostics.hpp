// SPDX-License-Identifier: MIT
#pragma once
#include "runtime.hpp"
#include <cstdio>
namespace native_portal::live {
// Private mailbox, deliberately separate from the existing exported Diagnostics ABI.
// Only scalar facts, relative image PCs and counters. No native pointers or pixels.
struct PolicyDiagnostic {
    uint64_t checked=0,passed=0,failed=0,seen=0,actual=0,expected=0;
    uint64_t style=0,expected_style=0,alpha=0,expected_alpha=0;
    bool sampled=false;
};
struct LiveDiagnostics {
    uint64_t pc=0,first_pc=0,phases=0,seals=0,events=0,frames=0;
    // Collector overlays independent atomic facts, including foreign Run refusal.
    uint64_t first_owner_rejection=0,first_evidence_pc=0,first_stop_pc=0,first_stock_pc=0;
    runtime::PublicationDiagnostic publication{};
    uint64_t stop_flags=0,stop_lr=0;
    uint64_t assoc_checked=0,assoc_passed=0,assoc_failed=0,assoc_lr=0,assoc_frame=0;
    bool assoc_owner=false,assoc_fallback=false;
    PolicyDiagnostic policy_detail{};
    uint64_t camera_cache=0,camera_flags=0,camera_h0=0,camera_h1=0;
    uint64_t idle_first=0,ready_first_ns=0,ready_last_ns=0,lease_first_ns=0;
    bool camera_read=false;

    uint64_t uploads[2]{},encoded[2]{},queue_success=0,submitted_frame=0;
    Refusal first_refusal=Refusal::None,refusal=Refusal::NativeInitialization;
    bool seal=false,graph=false,initialized=false,origin_ok=false,associated=false,idle=false;
    bool demand=false,can_acquire=false,owned_debt=false,pipeline=false,adapter=false;
    bool tracking=false,policy=false,capable=false,pending=false,stopped=false;
};
inline const char *refusal_name(Refusal r) noexcept {
    constexpr const char *names[]={"none","native-initialization","thread-handoff","graph-ownership","native-read",
        "pipeline-abi","origin","policy","tracking","adapter","command","stopped","foreign-thread"};
    const auto i=unsigned(r);return i<sizeof(names)/sizeof(*names)?names[i]:"unknown";
}
inline const char *seal_block(const LiveDiagnostics &d) noexcept {
    if(!d.seal)return "not-sampled";
    if(d.capable)return "none";
    if(d.stopped)return "stopped";
    if(!d.graph)return "graph-ownership";
    if(!d.pipeline)return "pipeline-abi";
    if(!d.origin_ok)return "origin";
    if(!d.associated)return "association";
    if(!d.demand)return "demand-missing";
    if(!d.can_acquire && !d.owned_debt)return "demand-unavailable";
    if(!d.adapter)return "adapter";
    return "none";
}
// Collector-thread only. Counter/PC progress alone never triggers repeated output.
class DiagnosticLog {
    LiveDiagnostics last_{};uint64_t owners_=0;unsigned lines_=0;
public:
    static constexpr unsigned limit=64;
    static constexpr size_t line_capacity=3072;
    size_t format(const LiveDiagnostics &d,uint64_t owners,char *out,size_t size) noexcept {
        const auto &a=last_;const auto &p=d.policy_detail;const auto &q=a.policy_detail;
        if(lines_>=limit)return 0;
        if(lines_ && owners==owners_ && d.first_owner_rejection==a.first_owner_rejection &&
           d.first_evidence_pc==a.first_evidence_pc && d.first_stop_pc==a.first_stop_pc && d.first_stock_pc==a.first_stock_pc &&
           d.publication.predicate==a.publication.predicate && d.stop_flags==a.stop_flags && d.stop_lr==a.stop_lr &&
           d.assoc_checked==a.assoc_checked && d.assoc_passed==a.assoc_passed && d.assoc_failed==a.assoc_failed &&
           d.assoc_owner==a.assoc_owner && d.assoc_lr==a.assoc_lr && d.assoc_fallback==a.assoc_fallback &&
           p.checked==q.checked && p.passed==q.passed && p.failed==q.failed && p.seen==q.seen &&
           p.actual==q.actual && p.expected==q.expected && p.style==q.style && p.expected_style==q.expected_style &&
           p.alpha==q.alpha && p.expected_alpha==q.expected_alpha && p.sampled==q.sampled &&
           d.camera_cache==a.camera_cache && d.camera_read==a.camera_read && d.camera_flags==a.camera_flags &&
           d.camera_h0==a.camera_h0 && d.camera_h1==a.camera_h1 && d.idle_first==a.idle_first &&
           d.ready_first_ns==a.ready_first_ns && d.lease_first_ns==a.lease_first_ns &&
           d.phases==a.phases && d.first_refusal==a.first_refusal &&
           d.refusal==a.refusal && d.seal==a.seal && d.graph==a.graph && d.initialized==a.initialized &&
           d.origin_ok==a.origin_ok && d.associated==a.associated && d.idle==a.idle && d.demand==a.demand &&
           d.can_acquire==a.can_acquire && d.owned_debt==a.owned_debt && d.pipeline==a.pipeline &&
           d.adapter==a.adapter && d.tracking==a.tracking && d.policy==a.policy && d.capable==a.capable &&
           d.pending==a.pending && d.stopped==a.stopped)return 0;
        const int n=std::snprintf(out,size,
            "ft-native-live line=%u pc=0x%llx phases=0x%llx owner_events=0x%llx handoff=%u "
            "first_owner_pc=0x%llx first_owner_pred=%u first_evidence_pc=0x%llx first_stop_pc=0x%llx "
            "first_stock_pc=0x%llx "
            "stop_flags=0x%llx stop_lr=0x%llx pub_pred=%llu pub_phase=%llu pub_flags=0x%llx "
            "pub_checked=0x%llx pub_passed=0x%llx pub_caller=0x%llx pub_return=0x%llx "
            "pub_recording=%llu pub_generation=%llu pub_renewal=%llu pub_entries=%llu pub_attempt=%llu "
            "first_refusal=%s first_pc=0x%llx refusal=%s seal=%u seal_pc=0x%llx seal_block=%s "
            "graph=%u initialized=%u origin_ok=%u associated=%u idle=%u demand=%u can_acquire=%u owned_debt=%u "
            "assoc_checked=0x%llx assoc_passed=0x%llx assoc_failed=%llu assoc_owner=%u assoc_lr=0x%llx assoc_frame=%llu assoc_fallback=%u "
            "pipeline=%u adapter=%u tracking=%u policy=%u capable=%u pending=%u stopped=%u "
            "policy_sampled=%u policy_failed=%llu policy_checked=0x%llx policy_passed=0x%llx policy_seen=0x%llx "
            "policy_actual=0x%llx policy_expected=0x%llx policy_style=%llu,%llu policy_alpha=0x%llx,0x%llx "
            "camera_read=%u camera_cache=%llu camera_flags=0x%llx camera_handles=0x%llx,0x%llx "
            "idle_first=%llu ready_first_ns=%llu ready_last_ns=%llu lease_first_ns=%llu "
            "events=%llu frames=%llu seals=%llu uploads=%llu,%llu encoded=%llu,%llu queue_success=%llu submitted_frame=%llu remaining=%u\n",
            lines_+1,(unsigned long long)d.pc,(unsigned long long)d.phases,(unsigned long long)owners,unsigned(bool(owners&(1ull<<5))),
            (unsigned long long)(d.first_owner_rejection>>16),unsigned(d.first_owner_rejection&0xffff),
            (unsigned long long)d.first_evidence_pc,(unsigned long long)d.first_stop_pc,
            (unsigned long long)d.first_stock_pc,
            (unsigned long long)d.stop_flags,(unsigned long long)d.stop_lr,
            (unsigned long long)d.publication.predicate,(unsigned long long)d.publication.phase,
            (unsigned long long)d.publication.flags,(unsigned long long)d.publication.checked,
            (unsigned long long)d.publication.passed,(unsigned long long)d.publication.caller,
            (unsigned long long)d.publication.return_pc,(unsigned long long)d.publication.recording,
            (unsigned long long)d.publication.generation,(unsigned long long)d.publication.renewal,
            (unsigned long long)d.publication.entries,(unsigned long long)d.publication.attempt,
            refusal_name(d.first_refusal),(unsigned long long)d.first_pc,refusal_name(d.refusal),d.seal,
            (unsigned long long)(d.seal?0x1480b8:0),seal_block(d),d.graph,d.initialized,d.origin_ok,d.associated,d.idle,
            d.demand,d.can_acquire,d.owned_debt,
            (unsigned long long)d.assoc_checked,(unsigned long long)d.assoc_passed,(unsigned long long)d.assoc_failed,
            d.assoc_owner,(unsigned long long)d.assoc_lr,(unsigned long long)d.assoc_frame,d.assoc_fallback,
            d.pipeline,d.adapter,d.tracking,d.policy,d.capable,d.pending,d.stopped,
            p.sampled,(unsigned long long)p.failed,(unsigned long long)p.checked,(unsigned long long)p.passed,
            (unsigned long long)p.seen,(unsigned long long)p.actual,(unsigned long long)p.expected,
            (unsigned long long)p.style,(unsigned long long)p.expected_style,
            (unsigned long long)p.alpha,(unsigned long long)p.expected_alpha,
            d.camera_read,(unsigned long long)d.camera_cache,(unsigned long long)d.camera_flags,
            (unsigned long long)d.camera_h0,(unsigned long long)d.camera_h1,
            (unsigned long long)d.idle_first,(unsigned long long)d.ready_first_ns,
            (unsigned long long)d.ready_last_ns,(unsigned long long)d.lease_first_ns,
            (unsigned long long)d.events,(unsigned long long)d.frames,(unsigned long long)d.seals,
            (unsigned long long)d.uploads[0],(unsigned long long)d.uploads[1],(unsigned long long)d.encoded[0],
            (unsigned long long)d.encoded[1],(unsigned long long)d.queue_success,(unsigned long long)d.submitted_frame,limit-lines_-1);
        if(n<=0 || size_t(n)>=size)return 0;
        last_=d;owners_=owners;++lines_;return size_t(n);
    }
};
}
