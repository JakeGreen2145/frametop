// SPDX-License-Identifier: MIT
#pragma once
#include "evidence.hpp"

namespace native_portal::runtime {
// Register-only receipt collector. Installation coverage is supplied by the
// authenticated loader, NOT inferred from events or the canonical source file.
// Use capture -> Evidence::event -> finish synchronously on the native owner.
// A false result denies integration only; never suppress native continuation.
class NativeStockFactCollector {
 public:
    NativeStockFactCollector() noexcept;
    NativeStockFactCollector(const NativeStockFactCollector &)=delete;
    NativeStockFactCollector &operator=(const NativeStockFactCollector &)=delete;
    static const uint64_t *required_sites(size_t &count) noexcept;
    bool install_coverage(const uint64_t *sites,size_t count) noexcept;
    Evidence::StockFactReader reader() noexcept;
    bool capture(uint64_t pc,const arena::Saved &) noexcept;
    // Only after successful capture, on this owner before finish. No scalar
    // answer or Evidence obligation belongs to this classified global event.
    bool unrelated() const noexcept {return unrelated_;}
    bool observing() const noexcept {return observation_.kind!=0;}
    void finish() noexcept;
    // Only after actual Evidence Run handoff and parent exclusion. No graph loan.
    bool handoff(const Evidence &) noexcept;
    bool complete() const noexcept;
    // Single immutable atomic receipt. PC[23:0], predicate[31:24], factory
    // phase[35:32], flags[40:36], module-relative caller[63:41]. No pointers.
    // 302790 predicates: phase, SP, LR, output slot, name, pool range (1..6).
    // Flags: known pool, current slot, factory slot, SP checked, SP matched.
    uint64_t failure_record() const noexcept {return failure_.load();}
    static uint64_t refusal_pc(uint64_t record) noexcept {
        const auto pc=record&0xffffff;return pc==0xffffff?UINT64_MAX:pc;
    }
    uint64_t failure_pc() const noexcept {return refusal_pc(failure_record());}
 private:
    struct Point {uint64_t pc=0,sp=0,lr=0,thread=0;};
    struct Member {
        uint64_t c=0,d=0,p=0,vk=0,name=0;bool made=false,begun=false,returned=false;
        unsigned reset=0;Point constructor{},device{},pool{},constructed{},created{},begin{},loaded{},result{},reset_entry{},reset_result{};
    };
    struct Constructor {uint64_t c=0,d=0,p=0;Point entry{};unsigned phase=0;} ctor_;
    struct Factory {
        uint64_t owner=0,slot=0,w=0,c=0,p=0,name=0;Point entry{},constructor{},acquire{},pop{},positive_store{},loaded{},return_intent{},stored{},return_slot{};
        unsigned phase=0;
    } factory_,publication_;
    struct Turn {uint64_t w=0,c=0;Point entry{};unsigned phase=0;} turn_;
    struct Dispose {uint64_t w=0,c=0,p=0;Point entry{};unsigned phase=0;} dispose_;
    struct Begin {uint64_t c=0;Point entry{};bool loaded=false;} begin_;
    // One bounded disjoint invocation. Copies only, never a native pointer loan,
    // publication, generation or settlement authority. Nested ambiguity refuses.
    struct Observation {
        unsigned kind=0,phase=0; // direct acquisition, factory, wrapper cleanup
        uint64_t owner=0,slot=0,w=0,c=0,p=0,name=0,renderer=0;
        Point entry{},acquire{},pop{},positive{};
    } observation_;
    int observe_shared(uint64_t,const arena::Saved &) noexcept;
    struct Field {uint64_t address=0,value=0;};
    std::array<Member,64> members_{};
    std::array<Field,6> fields_{};size_t fields_count_=0;
    const pid_t pid_;std::atomic<pid_t> tid_;
    std::atomic<uint64_t> failure_{0};
    bool coverage_=false,bootstrapped_=false,unrelated_=false;
    uint64_t base_=0,active_pc_=0,current_w_=0,current_c_=0;
    render::Identity identity_{};
    Point active_{};
    bool owner() const noexcept;
    bool deny(uint64_t,uint64_t detail=0) noexcept;
    bool field(uint64_t,uint64_t) noexcept;
    bool command_fields(const Member &) noexcept;
    Member *member(uint64_t) noexcept;
    bool bootstrap(uint64_t,const render::Identity &,uint64_t,uint64_t,uint64_t) noexcept;
    bool scalar(uint64_t,uint64_t,uint64_t &) noexcept;
    static bool scalar_cb(void *,uint64_t,uint64_t,uint64_t &) noexcept;
    static bool bootstrap_cb(void *,uint64_t,const render::Identity &,uint64_t,uint64_t,uint64_t) noexcept;
    static bool complete_cb(void *) noexcept;
    static bool cleanup_return_cb(void *,uint64_t,uint64_t) noexcept;
};
} // namespace native_portal::runtime
