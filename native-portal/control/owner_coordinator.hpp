#pragma once
#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace native_portal::coordination {
// Bind to the generation gate's real destruction interlock. No pointer lookup
// precedes retain. Callbacks must not throw; false retain must not acquire debt.
struct Retention {
    void* context=nullptr;
    bool (*retain)(void*,uint64_t epoch,uint64_t& ticket)=nullptr;
    bool (*release)(void*,uint64_t ticket)=nullptr;
};
enum class Execution { NotExecuted, Executed, Failed };
enum class QueueState { Disconnected, Connected, Unknown };
enum class Ownership { None, NewlyOwned, Borrowed, Unknown, NativeConnection };
// NativeConnection owns only the exact original camera Connect/Disconnect pair,
// not the manager, producer generation or independent consumers.
enum class UseDisposition { None, AuthenticatedBorrowedUse, NativeConnection };
struct QueueOutcome {
    bool observed=false,value_valid=false;
    int64_t value=0;
    QueueState state=QueueState::Unknown;
    Ownership ownership=Ownership::Unknown;
    uint64_t id=0,header_generation=0;
};
struct NativeReceipt {
    Execution execution=Execution::NotExecuted;
    bool value_valid=false;
    int64_t value=0; // exact native return, including false/zero/negative
    std::array<QueueOutcome,2> queues{}; // data, layout; observed, not inferred
    // Reattach only: executor revalidated the intact producer, provider, name /
    // alias and exact headers BEFORE executing Connect. Never infer from pointers.
    bool producer_revalidated=false;
    UseDisposition use=UseDisposition::None;
};
enum class Rejection { None, Full, Stale, Closing, Retention, Invalid, Exhausted, Cancelled };
struct Submission { uint64_t id=0; Rejection rejection=Rejection::None; };
struct Result {
    NativeReceipt native{};
    Rejection reason=Rejection::None;
    bool retention_released=false;
};
enum class Wait { Ready, Unknown, OwnerWouldBlock };

// One instance per retained native generation; construct on the render owner.
// Submission IDs are local handles, NEVER native epoch authority. The binding
// validates native callers and provides the epoch. All gate/native callbacks run
// unlocked. Lifetime: join submitters/owner before destroying; unresolved tickets
// are fail-stop, not implicitly declared settled. No worker or native calls start
// here. An event loop must pump; synchronous owner waits are explicitly refused.
class OwnerCoordinator {
public:
    static constexpr size_t MaxSlots=64;
    using Mutation=std::function<NativeReceipt()>;
    // The exact already-retained ticket. Borrow only during this callback;
    // never release/store it or mint another ticket to perform native lookup.
    using ScopedMutation=std::function<NativeReceipt(uint64_t ticket)>;
    OwnerCoordinator(uint64_t epoch,uint64_t generation,size_t capacity,Retention);
    ~OwnerCoordinator();
    OwnerCoordinator(const OwnerCoordinator&)=delete;
    OwnerCoordinator& operator=(const OwnerCoordinator&)=delete;
    Submission submit(uint64_t epoch,uint64_t generation,Mutation);
    Submission submit_scoped(uint64_t epoch,uint64_t generation,ScopedMutation);
    size_t pump(size_t budget=MaxSlots);
    bool cancel(uint64_t id);
    void close(); // cancel unexecuted only; never wait for running work
    Wait wait(uint64_t id,Result&);
    bool take(uint64_t id,Result&); // reclaim completed slot, never a retained one
    bool retry_release(uint64_t id); // explicit retry of gate release only
    bool drained() const;
private:
    enum class Phase { Free, Retaining, Queued, Running, Releasing, Done, Quarantined };
    struct Slot {
        Phase phase=Phase::Free;
        uint64_t id=0,ticket=0;
        bool cancelled=false;
        ScopedMutation mutation;
        Result result{};
    };
    size_t find(uint64_t) const;
    size_t first_pending() const;
    bool finish(size_t);
    const uint64_t epoch_,generation_;
    const size_t capacity_;
    const Retention retention_;
    const std::thread::id owner_;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::array<Slot,MaxSlots> slots_{};
    uint64_t next_=0;
    bool closing_=false,active_=false;
};

enum class CallerKind { Stock, Foreign, Portal };
struct Caller { CallerKind kind=CallerKind::Stock;uint64_t id=0,generation=0; };
struct PortalToken { uint64_t epoch=0,generation=0,id=0; };
struct Settlement {
    void* context=nullptr;
    // Binding verifies ALL original committed CPU, GPU, and source obligations.
    // A queue return, empty list, or GPU completion alone is not settlement.
    bool (*settled)(void*,uint64_t debt)=nullptr;
};
struct QueueJournal {
    QueueState state=QueueState::Unknown; // unknown until an actual executor receipt
    Ownership ownership=Ownership::None;
    uint64_t id=0,header_generation=0;
    QueueOutcome connect{},disconnect{};
};
enum class Action { None, Connect, Disconnect };
struct ActionPlan {
    Action action=Action::None;
    uint64_t epoch=0,generation=0,serial=0;
    std::array<bool,2> selected{};
    std::array<QueueJournal,2> queues{};
    bool reattach=false; // prior complete detach; executor must revalidate producer
};
struct DemandSnapshot {
    bool producer_demand=false,legacy_stock_full=false,foreign_full=false;
    bool stock_full_display=false,portal_ready=false,native_idle=false;
    bool closing=false,fault=false;
    // Integration-native work was attempted and has not fully detached. This
    // differs from global native idle: a never-used Unknown queue has no debt.
    bool integration_obligations=false;
    UseDisposition use=UseDisposition::None;
    size_t committed_debts=0;
};
// Owner-only logical arbitration. Existing renderer/CameraControl interfaces
// remain unchanged until the binding explicitly replaces its legacy adapter.
// No native handle is a lease and no implicit native retry is performed.
class DemandArbiter {
public:
    static constexpr size_t MaxCallers=32,MaxTokens=32,MaxDebts=64;
    using Executor=std::function<NativeReceipt(const ActionPlan&)>;
    DemandArbiter(uint64_t epoch,uint64_t generation,Retention);
    ~DemandArbiter();
    DemandArbiter(const DemandArbiter&)=delete;
    DemandArbiter& operator=(const DemandArbiter&)=delete;
    bool register_caller(Caller); // caller authenticity is an incoming binding duty
    bool set_level(Caller,bool producer,bool full_display);
    bool connection_lost(Caller); // exact kind/id/generation only
    PortalToken portal_on(Caller); // idempotent while active; nonreused identities
    bool portal_off(PortalToken);
    // Demand/connection eligibility only. The renderer must independently prove
    // source readiness/reuse, native frame lifetime, and its existing frame gates.
    bool frame_admissible(PortalToken) const;
    uint64_t commit(PortalToken,Settlement); // retained debt survives Off/close
    bool poll_settlement(uint64_t debt);
    bool close(); // false on wrong thread/reentrant mutation; caller must retry
    DemandSnapshot snapshot() const;
    std::array<QueueJournal,2> queues() const;
    NativeReceipt reconcile(const Executor&); // plan + actual receipt, never bool ack
private:
    struct CallerSlot { bool used=false,revoked=false,producer=false,full=false;Caller key{}; };
    struct TokenSlot { PortalToken token{};size_t caller=MaxCallers;bool active=false;size_t debts=0; };
    struct DebtSlot { uint64_t id=0,ticket=0;size_t token=MaxTokens;Settlement settlement{};bool proven=false; };
    bool owner() const;
    bool mutable_now() const;
    size_t caller_index(Caller) const;
    size_t token_index(PortalToken) const;
    void retire_token(size_t);
    ActionPlan plan();
    void apply(const ActionPlan&,const NativeReceipt&);
    const uint64_t epoch_,generation_;
    const Retention retention_;
    const std::thread::id owner_;
    std::array<CallerSlot,MaxCallers> callers_{};
    std::array<TokenSlot,MaxTokens> tokens_{};
    std::array<DebtSlot,MaxDebts> debts_{};
    std::array<QueueJournal,2> queues_{};
    uint64_t issued_tokens_=0,issued_debts_=0,issued_plans_=0;
    bool closing_=false,fault_=false,busy_=false,connect_attempted_=false,reattach_=false;
    UseDisposition use_=UseDisposition::None;
};
}
