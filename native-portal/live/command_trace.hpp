// SPDX-License-Identifier: MIT
#pragma once
#include "../runtime/evidence.hpp"
#include <atomic>
#include <cstdio>
namespace native_portal::live {
// Startup-only append log. Writers reserve once, never wait, allocate, format,
// read native memory or perform IO. Published slots are immutable. Full means
// lost evidence, NEVER admission or overwrite. The existing collector drains it.
class CommandTrace {
public:
    static constexpr size_t capacity=8192,identity_capacity=128;
    enum Route : uint64_t { Owner=1, HandoffWait=2, Foreign=3, OwnerEvent=4 };
    struct Row {
        uint64_t pc=0,lr=0,sp=0,tid=0,owner_tid=0,route=0,ledger_sampled=0;
        uint64_t x0=0,x1=0,x19=0,x20=0;
        runtime::CommandDiagnostic ledger{};
    };
private:
    struct Slot {std::atomic<bool> ready{false};Row row{};};
    std::array<Slot,capacity> slots_{};
    std::array<std::atomic<uint64_t>,identity_capacity> identities_{};
    std::atomic<uint64_t> count_{0},identity_overflow_{0};
    bool known(uint64_t command) const noexcept {
        if(!command)return false;
        for(const auto &v:identities_)if(v.load(std::memory_order_relaxed)==command)return true;
        return false;
    }
    void remember(uint64_t command) noexcept {
        if(!command)return;
        for(auto &v:identities_) {
            auto old=v.load(std::memory_order_relaxed);
            if(old==command)return;
            if(!old && v.compare_exchange_strong(old,command,std::memory_order_relaxed))return;
        }
        identity_overflow_.fetch_add(1,std::memory_order_relaxed);
    }
public:
    static bool selected(uint64_t pc) noexcept {
        return runtime::command_trace_pc(pc) || pc==0x14099c || pc==0x140dbc || pc==0x14e5a8;
    }
    void push(uint64_t pc,const arena::Saved &s,uint64_t base,uint64_t thread,uint64_t owner,
              Route route,const runtime::CommandDiagnostic *d=nullptr) noexcept {
        if(!selected(pc))return;
        uint64_t command=d?d->command:0;
        if(!d) switch(pc) {
        case 0x301808:case 0x300a08:case 0x300b20:case 0x3003d0:case 0x4b3d0:command=s.x[0];break;
        case 0x30180c:command=s.x[20];break;
        case 0x300a38:case 0x300c54:command=s.x[19];break;
        case 0x311648:command=s.x[1];break;
        default:break;
        }
        // Generic pool returns and sized deletes include many unrelated native
        // resources. Filter only by observed command identity, never by reading it.
        if((pc==0x311648 || pc==0x4b3d0) &&
           !(d && (d->before.member || d->after.member)) && !known(command))return;
        if(pc==0x301808 || pc==0x300a08 || pc==0x300b20 || pc==0x29a528 || pc==0x2a23f4)remember(command);
        const auto index=count_.fetch_add(1,std::memory_order_relaxed);
        if(index>=capacity)return;
        auto &slot=slots_[index];auto &r=slot.row;
        r.pc=pc;r.lr=s.x[30]>=base && s.x[30]-base<0x600000?s.x[30]-base:0;
        r.sp=s.sp;r.tid=thread;r.owner_tid=owner;r.route=route;
        r.x0=s.x[0];r.x1=s.x[1];r.x19=s.x[19];r.x20=s.x[20];
        r.ledger_sampled=d!=nullptr;
        if(d)r.ledger=*d;else r.ledger.command=command;
        slot.ready.store(true,std::memory_order_release);
    }
    bool read(size_t index,Row &r) const noexcept {
        if(index>=capacity || !slots_[index].ready.load(std::memory_order_acquire))return false;
        r=slots_[index].row;return true;
    }
    uint64_t count() const noexcept {return count_.load(std::memory_order_relaxed);}
    uint64_t identity_overflow() const noexcept {return identity_overflow_.load(std::memory_order_relaxed);}
    static size_t format(size_t index,const Row &r,char *out,size_t size) noexcept {
        const auto &d=r.ledger;const auto &a=d.before;const auto &b=d.after;
        const char *route=r.route==Owner?"owner":r.route==HandoffWait?"handoff-wait":r.route==Foreign?"foreign":"owner-event";
        const int n=std::snprintf(out,size,
            "ft-native-command seq=%zu pc=0x%llx lr=0x%llx tid=%llu owner_tid=%llu route=%s ledger_sampled=%llu "
            "command=0x%llx native_vk=0x%llx sp=0x%llx x0=0x%llx x1=0x%llx x19=0x%llx x20=0x%llx "
            "before_member=%llu after_member=%llu before_vk=0x%llx after_vk=0x%llx "
            "generation=%llu,%llu renewal=%llu,%llu begun=%llu,%llu entries=%llu,%llu "
            "recording=%llu,%llu creation=0x%llx,0x%llx phase=%llu,%llu flags=0x%llx,0x%llx accepted=%llu failure=0x%llx\n",
            index+1,(unsigned long long)r.pc,(unsigned long long)r.lr,(unsigned long long)r.tid,
            (unsigned long long)r.owner_tid,route,(unsigned long long)r.ledger_sampled,
            (unsigned long long)d.command,(unsigned long long)d.native_vk,(unsigned long long)r.sp,
            (unsigned long long)r.x0,(unsigned long long)r.x1,(unsigned long long)r.x19,(unsigned long long)r.x20,
            (unsigned long long)a.member,(unsigned long long)b.member,(unsigned long long)a.vk,(unsigned long long)b.vk,
            (unsigned long long)a.generation,(unsigned long long)b.generation,(unsigned long long)a.renewal,(unsigned long long)b.renewal,
            (unsigned long long)a.begun,(unsigned long long)b.begun,(unsigned long long)a.entries,(unsigned long long)b.entries,
            (unsigned long long)a.recording,(unsigned long long)b.recording,(unsigned long long)a.creation,(unsigned long long)b.creation,
            (unsigned long long)a.phase,(unsigned long long)b.phase,(unsigned long long)a.flags,(unsigned long long)b.flags,
            (unsigned long long)d.accepted,(unsigned long long)d.failure);
        return n>0 && size_t(n)<size?size_t(n):0;
    }
};
static_assert(std::atomic<uint64_t>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);
}
