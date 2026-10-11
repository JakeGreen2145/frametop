// SPDX-License-Identifier: MIT
// CPU-only production CameraOwner/gate/native-cleanup composition.
// ALL native callbacks and identities here are modeled, not native ABI proof.
#include "camera_owner.hpp"
#include <cassert>
#include <cstdio>
#include <future>
#include <type_traits>
#include <vector>
#include <sys/wait.h>

using namespace native_portal;
using Owner = live::CameraOwner;
using Dispatch = binding::CameraNativeBinding::Dispatch;
using namespace std::chrono_literals;
static const CameraBinding ids{1,0x200000,3,4,5,6,11,12,13};
static constexpr uint64_t generation=7;

// Link-time boundary injection, no production hook or substitute gate.
// All non-injected operations call the actual production gate.
static std::atomic<bool> fail_release{false},fail_retain{false};
static thread_local unsigned pause_retain=0;
static std::promise<void>* retain_entered=nullptr;
static std::shared_future<void> retain_continue;
extern "C" bool real_release(CameraAccessGate*,uint64_t)
    asm("__real__ZN13native_portal16CameraAccessGate7releaseEm");
extern "C" bool wrap_release(CameraAccessGate* gate,uint64_t ticket)
    asm("__wrap__ZN13native_portal16CameraAccessGate7releaseEm");
extern "C" bool wrap_release(CameraAccessGate* gate,uint64_t ticket) {
    return !fail_release.load() && real_release(gate,ticket);
}
extern "C" bool real_retain(CameraAccessGate*,uint64_t,uint64_t&)
    asm("__real__ZN13native_portal16CameraAccessGate6retainEmRm");
extern "C" bool wrap_retain(CameraAccessGate*,uint64_t,uint64_t&)
    asm("__wrap__ZN13native_portal16CameraAccessGate6retainEmRm");
extern "C" bool wrap_retain(CameraAccessGate* gate,uint64_t epoch,uint64_t& ticket) {
    if(fail_retain.load()){ticket=0;return false;}
    if(pause_retain==1){retain_entered->set_value();retain_continue.wait();}
    const bool result=real_retain(gate,epoch,ticket);
    if(pause_retain==2){assert(result);retain_entered->set_value();retain_continue.wait();}
    return result;
}
static arena::Saved publication() {
    arena::Saved s{};s.sp=0x80000;s.x[19]=ids.application;
    s.x[20]=ids.application+0x20000;s.x[26]=ids.camera;return s;
}
static void bind(Owner& owner) {
    assert(owner.observe_publication(0x8dc1c,publication()));
    assert(owner.bind(ids,generation));
}
static void modeled_cleanup(void* p) noexcept {++*static_cast<unsigned*>(p);}
static Dispatch cleanup(Owner& owner,unsigned& calls) {
    arena::Saved s{};s.x[0]=ids.camera;
    auto result=owner.cleanup(0x15df70,s,modeled_cleanup,&calls,0ms);
    assert(s.pad==(result==Dispatch::Refused?arena::Unsupported:arena::SkipOwned));
    return result;
}
static void finish(Owner& owner) {
    assert(owner.begin_close());assert(owner.finish_close());
    assert(owner.bind_stage()==Owner::BindStage::Closed);
    assert(!owner.snapshot().native_idle); // integration close is not native idle
    unsigned calls=0;assert(cleanup(owner,calls)==Dispatch::Completed && calls==1);
}

static void blocked_owner_original_lifetime() {
    Owner owner;bind(owner);const auto epoch=owner.epoch();
    std::promise<void> entered;
    std::mutex original_mutex;std::condition_variable original_returned;bool returned=false;
    const auto owner_thread=std::this_thread::get_id();
    // Main represents Renderer blocked away from pump. The incoming listener
    // retains and resolves its scalar binding without executing ANY owner work.
    auto listener=std::async(std::launch::async,[&] {
        auto call=owner.retain_caller(epoch,generation);assert(call);
        CameraBinding b;assert(call.binding(b) && b==ids);
        assert(std::this_thread::get_id()!=owner_thread);
        assert(!owner.retained_binding(b)); // old owner-only API remains restricted
        unsigned scopes=0;uint64_t seen_ticket=0;
        assert(!call.observe_scope({}));
        assert(call.observe_scope([&](CameraAccessGate& gate,uint64_t ticket,const CameraBinding& held) {
            ++scopes;seen_ticket=ticket;
            assert(held==ids && ticket && gate.lookup(ticket,b) && b==ids);
            assert(std::this_thread::get_id()!=owner_thread);
            entered.set_value();
            { // Hold this existing ticket through the complete modeled original.
                std::unique_lock<std::mutex> lock(original_mutex);
                original_returned.wait(lock,[&]{return returned;});
            }
            assert(gate.lookup(ticket,b) && b==ids); // close cannot retire it
        }));
        assert(scopes==1 && seen_ticket && call.binding(b) && b==ids);
        assert(call.release());assert(!call && !call.release());
        assert(!call.observe_scope([&](CameraAccessGate&,uint64_t,const CameraBinding&){++scopes;}));
        assert(scopes==1);
    });
    assert(entered.get_future().wait_for(2s)==std::future_status::ready);
    assert(owner.begin_close());assert(!owner.retain_caller(epoch,generation));
    assert(!owner.finish_close());assert(owner.bind_stage()!=Owner::BindStage::Closed);
    unsigned calls=0;assert(cleanup(owner,calls)==Dispatch::Refused && calls==0);
    {std::lock_guard<std::mutex> lock(original_mutex);returned=true;}
    original_returned.notify_one();listener.get();
    assert(owner.finish_close() && calls==0); // no native cleanup in finish_close
    assert(!owner.retain_caller(epoch,generation));
    assert(cleanup(owner,calls)==Dispatch::Completed && calls==1);
}

static void admission_states_and_identity() {
    Owner owner;CameraBinding b=ids;
    auto empty=owner.retain_caller(1,generation);assert(!empty);
    assert(!empty.binding(b) && b==CameraBinding{} && !empty.release());
    assert(owner.observe_publication(0x8dc1c,publication()));
    assert(!owner.retain_caller(1,generation)); // publication is not binding
    assert(!owner.bind(ids,0));assert(!owner.retain_caller(1,generation));
    assert(owner.bind(ids,generation));const auto epoch=owner.epoch();
    assert(!owner.retain_caller(0,generation));assert(!owner.retain_caller(epoch+1,generation));
    assert(!owner.retain_caller(epoch,0));assert(!owner.retain_caller(epoch,generation+1));
    auto actual=owner.retain_caller(epoch,generation);assert(actual);
    b={};assert(actual.binding(b) && b==ids);assert(actual.release());finish(owner);
    Owner successor;bind(successor);assert(successor.epoch()!=epoch);
    assert(!successor.retain_caller(epoch,generation));finish(successor);
}

static void nested_capacity_moves_and_cross_thread_release() {
    using Handle=decltype(std::declval<Owner&>().retain_caller(1,1));
    static_assert(!std::is_copy_constructible_v<Handle> && !std::is_copy_assignable_v<Handle>);
    static_assert(std::is_nothrow_move_constructible_v<Handle> && std::is_nothrow_move_assignable_v<Handle>);
    Owner owner;bind(owner);const auto epoch=owner.epoch();
    std::vector<Handle> held;
    for(size_t i=1;i<CameraAccessGate::capacity;++i) {
        held.push_back(owner.retain_caller(epoch,generation));assert(held.back());
    }
    assert(!owner.retain_caller(epoch,generation)); // bind barrier occupies one slot
    auto moved=std::move(held.front());assert(moved && !held.front());
    assert(!held.front().release());
    CameraBinding b=ids;assert(!held.front().binding(b) && b==CameraBinding{});
    assert(moved.release() && !moved.release());
    for(unsigned i=0;i<256;++i) {
        auto reused=owner.retain_caller(epoch,generation);assert(reused);
        assert(!moved.release()); // cannot release the new occupant
    } // safe RAII fallback releases each exact ticket
    auto last=owner.retain_caller(epoch,generation);assert(last);
    held[1]=std::move(last);assert(!last && held[1]); // release destination first
    auto nested=owner.retain_caller(epoch,generation);assert(nested);
    assert(!owner.retain_caller(epoch,generation));
    assert(owner.begin_close());assert(!owner.finish_close());
    std::thread other([handle=std::move(nested)]() mutable {
        CameraBinding b;assert(handle.binding(b) && b==ids);assert(handle.release());
    });other.join(); // release is NOT issuer-thread-only, like the production gate
    held.clear();assert(owner.finish_close());
    unsigned calls=0;assert(cleanup(owner,calls)==Dispatch::Completed && calls==1);
}

static void close_retain_interleavings() {
    for(unsigned pause:{1u,2u}) {
        Owner owner;bind(owner);const auto epoch=owner.epoch();
        std::promise<void> entered,resume;retain_entered=&entered;
        retain_continue=resume.get_future().share();
        auto listener=std::async(std::launch::async,[&] {
            pause_retain=pause;return owner.retain_caller(epoch,generation);
        });
        assert(entered.get_future().wait_for(2s)==std::future_status::ready);
        // Gate determines the winner: before its retain loses, after wins.
        assert(owner.begin_close());resume.set_value();auto handle=listener.get();
        assert(bool(handle)==(pause==2));
        if(handle) {
            assert(!owner.finish_close());CameraBinding b;
            assert(handle.binding(b) && b==ids);assert(handle.release());
        }
        assert(owner.finish_close());unsigned calls=0;
        assert(cleanup(owner,calls)==Dispatch::Completed && calls==1);
    }
}

static void failed_release_retry_and_fork() {
    Owner owner;bind(owner);auto handle=owner.retain_caller(owner.epoch(),generation);assert(handle);
    assert(owner.begin_close());fail_release=true;
    assert(!handle.release() && handle);CameraBinding b;assert(handle.binding(b) && b==ids);
    fail_release=false;assert(!owner.finish_close()); // handle accounting was not dropped
    unsigned calls=0;assert(cleanup(owner,calls)==Dispatch::Refused && calls==0);
    const auto child=fork();assert(child>=0);
    if(child==0) {
        alarm(3);
        assert(!owner.retain_caller(owner.epoch(),generation));
        assert(!handle.binding(b) && b==CameraBinding{});
        assert(!handle.release() && handle); // PID refusal keeps inherited handle intact
        _exit(0);
    }
    int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    assert(handle.release());assert(owner.finish_close());
    assert(cleanup(owner,calls)==Dispatch::Completed && calls==1);
}

static void partial_bind_refuses() {
    // Existing bind-to-barrier failure cannot be repaired or granted admission.
    // No retained ticket exists here; the closed gate can genuinely clean up.
    Owner owner;assert(owner.observe_publication(0x8dc1c,publication()));
    fail_retain=true;assert(!owner.bind(ids,generation));fail_retain=false;
    assert(owner.bind_stage()==Owner::BindStage::Published);
    assert(!owner.retain_caller(owner.epoch(),generation));
    unsigned calls=0;assert(cleanup(owner,calls)==Dispatch::Completed && calls==1);
}
static void publication_visibility_and_locked_fork() {
    Owner owner;
    assert(!owner.bind(ids,generation));
    assert(owner.bind_stage()==Owner::BindStage::CleanRefused);
    assert(!owner.retain_caller(1,generation));
    assert(owner.observe_publication(0x8dc1c,publication()));
    std::promise<void> entered,resume,bound,close;
    retain_entered=&entered;retain_continue=resume.get_future().share();
    auto end=close.get_future();
    auto renderer=std::async(std::launch::async,[&] {
        pause_retain=1;assert(owner.bind_handoff(ids,generation));pause_retain=0;
        bound.set_value();end.wait();finish(owner);
    });
    assert(entered.get_future().wait_for(2s)==std::future_status::ready);
    assert(owner.bind_stage()==Owner::BindStage::Published);
    // Stage's publication edge permits reading epoch, but caller admission must
    // refuse the partial epoch even though the real gate already published it.
    const auto epoch=owner.epoch();assert(epoch);
    assert(!owner.retain_caller(epoch,generation));
    resume.set_value();bound.get_future().wait();
    auto call=owner.retain_caller(epoch,generation);assert(call);
    CameraBinding b;assert(call.binding(b) && b==ids);assert(call.release());
    close.set_value();renderer.get();

    Owner locked;bind(locked);auto held=locked.retain_caller(locked.epoch(),generation);assert(held);
    std::promise<void> locked_entered,unlock;
    retain_entered=&locked_entered;retain_continue=unlock.get_future().share();
    auto listener=std::async(std::launch::async,[&] {
        pause_retain=1;return locked.retain_caller(locked.epoch(),generation);
    });
    assert(locked_entered.get_future().wait_for(2s)==std::future_status::ready);
    // The listener is inside retain_caller with its mutex locked at fork.
    auto child=fork();assert(child>=0);
    if(child==0) {
        alarm(3);
        assert(!locked.retain_caller(locked.epoch(),generation));
        assert(!held.release() && held);
        assert(!held.binding(b) && b==CameraBinding{});
        _exit(0);
    }
    int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    unlock.set_value();auto other=listener.get();assert(other && other.release());
    assert(held.release());finish(locked);
}

static void destructor_failstop() {
    // A deliberate terminate handler marks ONLY the exact destructor/assignment
    // boundary. No assert(false) or callback abort can supply the expected exit.
    for(unsigned mode:{0u,1u,2u}) {
        auto child=fork();assert(child>=0);
        if(child==0) {
            Owner owner;bind(owner);
            auto a=owner.retain_caller(owner.epoch(),generation);assert(a);
            auto b=owner.retain_caller(owner.epoch(),generation);assert(b);
            std::set_terminate([]{_exit(86);});
            if(mode==0)owner.~CameraOwner();
            else {
                fail_release=true;
                if(mode==1)a.~RetainedCaller();
                else a=std::move(b);
            }
            _exit(90); // a forbidden return, NOT the expected terminate result
        }
        int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==86);
    }
}

int main() {
    blocked_owner_original_lifetime();admission_states_and_identity();
    nested_capacity_moves_and_cross_thread_release();close_retain_interleavings();
    failed_release_retry_and_fork();partial_bind_refuses();
    publication_visibility_and_locked_fork();destructor_failstop();
    std::puts("camera-caller: production lifetime/cleanup, blocked owner, identity, capacity, moves, close races, release retry, fork PASS; native callbacks MODELED");
}
