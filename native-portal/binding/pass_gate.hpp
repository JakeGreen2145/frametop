// SPDX-License-Identifier: MIT
#pragma once
#include "pass.hpp"
#include <cstddef>

namespace native_portal::binding {
// Byte-compatible with hooks/emit.h. Not a native function signature.
struct PassRegisters {
    uint64_t x[31], sp;
    unsigned char q[32][16];
    uint64_t nzcv, fpcr, fpsr, selector;
};
static_assert(sizeof(PassRegisters)==800 && offsetof(PassRegisters,selector)==792);
// One owner-thread instance per fixed native recording/lifetime context. The
// lifecycle owner creates/retires this beside Adapter, never inside a hook. No
// installer/global registry is provided. Reader ownership MUST cover the full
// dispatch including stock interleaving and both committed eyes.
class PassGate {
    Adapter &adapter_;
    PassReader reader_;
    PassRequest request_;
    bool committed_=false;
  public:
    PassGate(Adapter &adapter,PassReader reader,PassRequest request) noexcept
        : adapter_(adapter),reader_(reader),request_(request) {}
    void callback(PassRegisters &r,unsigned site) noexcept {
        if(site==0) {
            if(uint32_t(r.x[1])) return; // original CBNZ remains the only effect
            committed_=false;
            if(r.x[0]!=request_.context.identity.renderer) return;
            auto q=request_;q.dispatch_mode=0;
            PairPreparation rejected_snapshot;
            const auto generation=prepare_passes(adapter_,reader_,q,&rejected_snapshot);
            committed_=generation && adapter_.commit(generation);
            if(!committed_) {
                adapter_.reject_recorded(rejected_snapshot);
            }
            return;
        }
        if(site!=1) return;
        r.selector=0;
        if(r.x[21]!=request_.context.identity.renderer || !reader_.read ||
           !request_.marker || !r.x[19] || r.x[19]>UINT64_MAX-16) return;
        Address marker{};
        if(!reader_.read(reader_.user,r.x[19]+8,&marker,sizeof marker) ||
           marker!=request_.marker) return; // no label/shader-based stock suppression
        const bool live=reader_.live && reader_.live(reader_.user,request_.context);
        // Off/expiry is NOT a lifetime failure. Finish the already committed pair.
        r.selector=(!committed_ || !live)?1:0;
        if(!committed_) {
            // Re-read the bounded dispatch at the actual packet skip. Never
            // certify a planned skip or a marker-only/partially scanned packet.
            const auto scan=extract_passes(reader_,request_);
            if(live && scan.pair.suppression_scan) {
                for(const auto &eye:scan.pair.eyes)
                    if(eye.lookup.x[1]==r.x[19] && eye.pass==r.x[22]) {
                        adapter_.suppressed(eye,scan.pair.command);
                        return;
                    }
            }
            adapter_.suppressed({},0);
        }
    }
};
} // namespace native_portal::binding
