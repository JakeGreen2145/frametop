// SPDX-License-Identifier: MIT
#include "pass.hpp"
#include <cstring>
#include <limits>

namespace native_portal::binding {
namespace {
template<class T, size_t N> T field(const std::array<std::byte,N> &b,size_t o) noexcept {
    T v{}; std::memcpy(&v,b.data()+o,sizeof v); return v;
}
struct Scan {
    PassReader reader;
    const PassRequest &request;
    PassScan out;
    bool read(Address a, void *p, size_t n) noexcept {
        if (!a || a>UINT64_MAX-n || !reader.read(reader.user,a,p,n)) {
            out.reason=PassReason::ReadFailure; return false;
        }
        return true;
    }
    template<class T> bool at(Address a,T &v) noexcept { return read(a,&v,sizeof v); }
    bool span(Address b,Address e,size_t stride,size_t cap,size_t &count) noexcept {
        if (e<b || (b==0 && e!=0) || (b%8) || (e-b)%stride) {
            out.reason=PassReason::InvalidSpan; return false;
        }
        count=(e-b)/stride;
        if(count>cap) { out.reason=PassReason::Limit; return false; }
        return true;
    }
    bool run() noexcept {
        if (request.dispatch_mode) { out.reason=PassReason::InactiveMode; return false; }
        const Address R=request.context.identity.renderer;
        if (!reader.read || !reader.live || !request.marker || !R ||
            R>UINT64_MAX-0x15d1 || !request.context.recording_generation ||
            request.image_base>UINT64_MAX-0x29dac0 ||
            !reader.live(reader.user,request.context)) return false;
        Address begin{},end{},wrapper{},cache_begin{},cache_end{};
        if (!at(R+0x15a0,begin) || !at(R+0x15a8,end) || !at(R+0x1380,wrapper) ||
            !wrapper || wrapper>UINT64_MAX-16 || !at(wrapper+8,out.pair.command) ||
            !out.pair.command || !at(R+0x1478,cache_begin) || !at(R+0x1480,cache_end)) return false;
        size_t passes{},caches{};
        if (!span(begin,end,0x98,pass_limit,passes) ||
            !span(cache_begin,cache_end,0x40,cache_limit,caches)) return false;
        // The optimizer may rewrite pass/packet vectors. Do not predict its output
        // or move it across the native pre-dispatch/query/barrier sequence.
        uint8_t optimize{};
        Address pre_begin{},pre_end{};
        if (!at(R+0x15d0,optimize)) return false;
        if (optimize) { out.reason=PassReason::PreDispatchMutation; return false; }
        for (Address offset : {Address{0x1570},Address{0x1588}}) {
            if(!at(R+offset,pre_begin) || !at(R+offset+8,pre_end)) return false;
            if(pre_begin!=pre_end) { out.reason=PassReason::PreDispatchMutation; return false; }
        }
        std::array<std::array<std::byte,0x40>,cache_limit> cache{};
        for (size_t i=0;i<caches;++i)
            if(!read(cache_begin+i*0x40,cache[i].data(),0x40)) return false;
        size_t total=0;
        std::array<uint64_t,2> tickets{};
        bool cold=false;
        for(size_t i=0;i<passes;++i) {
            const Address pass=begin+i*0x98;
            std::array<std::byte,0x98> p{};
            if(!read(pass,p.data(),p.size())) return false;
            const Address pb=field<Address>(p,0x50),pe=field<Address>(p,0x58);
            size_t count{};
            if(!span(pb,pe,0x2b0,packet_limit-total,count)) return false;
            total+=count;
            if(!count) continue; // native dispatcher does not prepare empty passes
            Address value=0,framebuffer=0;
            // EXACT first-match key from 0x2a0668..0x2a06f0. +0x18 and +0x6b
            // deliberately are NOT key fields. Do not invent a stricter key.
            for(size_t j=0;j<caches;++j) {
                const auto &c=cache[j];
                if(field<Address>(c,0x10)!=field<Address>(p,0) ||
                   field<Address>(c,0x18)!=field<Address>(p,8) ||
                   field<Address>(c,0x20)!=field<Address>(p,0x10) ||
                   field<Address>(c,0x30)!=field<Address>(p,0x20) ||
                   ((field<uint32_t>(c,0x38)^field<uint32_t>(p,0x68))&0xffffff)) continue;
                value=field<Address>(c,0); framebuffer=field<Address>(c,8); break;
            }
            // All nonempty passes must hit, including stock. This subset cannot
            // enter helper materialization before an owned pass.
            cold|=!value || !framebuffer;
            for(size_t j=0;j<count;++j) {
                const Address packet=pb+j*0x2b0;
                Address marker{};
                if(!at(packet+8,marker)) return false;
                if(marker!=request.marker) continue; // ticket-looking stock untouched
                if(out.owned==2) { out.reason=PassReason::PartialOrDuplicate; return false; }
                auto &eye=out.pair.eyes[out.owned];
                std::array<std::byte,0x2b0> bytes{};
                if(!read(packet,bytes.data(),bytes.size())) return false;
                std::memcpy(eye.record.prefix.data(),bytes.data(),352);
                eye.record.label_size=field<uint64_t>(bytes,0x168);
                // Owned tickets are short strings. The native mover repairs this
                // exact inline pointer. Never chase arbitrary label pointers.
                if(field<Address>(bytes,0x160)!=packet+0x170) {
                    out.reason=PassReason::BadLabel; return false;
                }
                std::memcpy(eye.record.label.data(),bytes.data()+0x170,16);
                uint64_t ticket{};
                if(!decode_ticket(eye.record.label,eye.record.label_size,ticket)) {
                    out.reason=PassReason::BadLabel; return false;
                }
                if(out.owned && tickets[0]==ticket) { out.reason=PassReason::PartialOrDuplicate; return false; }
                tickets[out.owned]=ticket;
                std::memcpy(eye.record.descriptors.data(),bytes.data()+0x1c8,216);
                const auto vs=field<uint32_t>(bytes,0x20),ps=field<uint32_t>(bytes,0x30);
                const bool alternate=field<uint8_t>(bytes,0x15e)!=0;
                if(!supported_pair(vs,ps) || field<uint8_t>(bytes,0x2a8) ||
                   field<Address>(bytes,0x180) || !field<uint8_t>(p,0x68) ||
                   (alternate && !field<uint8_t>(p,0x69))) {
                    out.reason=PassReason::UnsupportedPacket; return false;
                }
                Address target=field<Address>(p,0);
                const Address alternative=field<Address>(p,0x10);
                if(alternate && alternative) target=alternative;
                uint32_t format{};
                if(!target || target>UINT64_MAX-0x150 || !at(target+0x14c,format)) return false;
                eye.context=request.context; eye.pass=pass;
                eye.lookup.target=request.image_base+0x29dac0;
                // Only x0..x3 are lookup arguments. Unused caller-clobbered lanes
                // are canonical zero at prepare AND the future backend island.
                eye.lookup.x={R,packet,value,format,0,0,0,0};
                ++out.owned;
            }
        }
        if(out.owned!=2) { out.reason=out.owned?PassReason::PartialOrDuplicate:PassReason::NoOwned; return false; }
        if(!reader.live(reader.user,request.context)) { out.reason=PassReason::Lifetime; return false; }
        out.pair.suppression_scan=true;
        if(cold) { out.reason=PassReason::ColdPass; return false; }
        out.pair.complete_scan=true;
        for(auto &eye:out.pair.eyes) eye.ready=true;
        out.reason=PassReason::Ready;
        return true;
    }
};
} // namespace
PassScan extract_passes(PassReader reader,const PassRequest &request) noexcept {
    Scan scan{reader,request,{}};
    if(!scan.run()) {
        scan.out.pair.complete_scan=false;
        for(auto &eye:scan.out.pair.eyes) eye.ready=false;
    }
    return scan.out;
}
uint64_t prepare_passes(Adapter &adapter,PassReader reader,const PassRequest &request,
                        PairPreparation *rejected_snapshot) noexcept {
    const auto scan=extract_passes(reader,request);
    if(rejected_snapshot) *rejected_snapshot=scan.pair;
    if(scan.reason==PassReason::InactiveMode) return 0;
    // Feed failures too, so a partial/duplicate/cold owned dispatch latches the
    // Adapter's one-shot refusal before any owned backend can admit a packet.
    const auto generation=adapter.prepare(scan.pair);
    if(!generation) return 0;
    // Native pipeline callbacks may invoke foreign code. Confirm the exact
    // borrowed packet/pass values still agree before offering commit authority.
    const auto after=extract_passes(reader,request);
    bool same=after.reason==PassReason::Ready && after.pair.command==scan.pair.command;
    for(unsigned i=0;i<2 && same;++i) {
        const auto &a=scan.pair.eyes[i], &b=after.pair.eyes[i];
        same=a.pass==b.pass && a.lookup.target==b.lookup.target && a.lookup.x==b.lookup.x &&
             a.record.prefix==b.record.prefix && a.record.label==b.record.label &&
             a.record.label_size==b.record.label_size;
        for(unsigned j=0;j<9 && same;++j)
            same=a.record.descriptors[j].buffer==b.record.descriptors[j].buffer &&
                 a.record.descriptors[j].offset==b.record.descriptors[j].offset &&
                 a.record.descriptors[j].range==b.record.descriptors[j].range;
    }
    if(!same) {
        // An unclassified post-prepare scan cannot become rejection proof just
        // because a later packet scan looks valid again.
        if(rejected_snapshot) rejected_snapshot->suppression_scan=false;
        // Reviewed one-shot prepare rejects a second attempt and latches invalid.
        adapter.prepare({});
        return 0;
    }
    return generation;
}
} // namespace native_portal::binding
