// SPDX-License-Identifier: MIT
#pragma once
#include "../control/camera_gate.hpp"
#include "../control/owner_coordinator.hpp"
#include "../arena/bridge.hpp"
#include <atomic>

namespace native_portal::live {
// Observation part of the production source provider, NOT SourceAdmission.
// No ready()/settled() facade: successful connection is not producer publication
// or original source-consumer retirement. See source-provider.md for the exact
// remaining hook/contract boundary. Does not call any private native interface.
class ClientSourceBinding final {
public:
    inline static constexpr char compositor_sha[]="9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265";
    inline static constexpr char client_sha[]="fe967dcc0d97dab984bd9acc1dc522fcecbcf9beb794fdd7ec156334ec31e313";
    enum class Module { Compositor, Client };
    enum class Entry { Unknown, NewEntry, ExistingEntry };
    struct QueueCall {
        bool observed=false;
        int32_t result=0;
        uint64_t id=0,alias=0,header=0;
        Entry ownership=Entry::Unknown; // manager entry only, NOT release authority
    };
    struct Observation {
        bool valid=false;
        uint64_t actual=0,provider=0;
        coordination::Action action=coordination::Action::None;
        std::array<QueueCall,2> queues{};
    };
    // read_self by default; injection is for CPU instruction fixtures only.
    using Read=bool (*)(void*,uint64_t,void*,size_t);
    explicit ClientSourceBinding(CameraAccessGate&,Read=nullptr,void* context=nullptr) noexcept;
    // Caller owns and keeps this original gate ticket for the complete original
    // native invocation. Pins are a trusted loader/preparer contract, as in
    // runtime::Evidence, NOT proof supplied by an arbitrary digest string.
    // begin belongs BEFORE original camera Acquire/Release, not its return.
    bool begin(uint64_t retained_ticket,const CameraBinding&,coordination::Action,
               uint64_t compositor_base,uint64_t client_base,
               const char* compositor_digest,const char* client_digest) noexcept;
    // Saved registers BEFORE the named instruction. Calls are observed only;
    // every original instruction/result remains untouched. Missing events never
    // become inferred ownership. Foreign-thread events revoke without reading.
    bool event(Module,uint64_t relative_pc,const arena::Saved&) noexcept;
    bool finish(uint64_t actual_camera_return) noexcept;
    Observation snapshot() const noexcept;
private:
    struct Call {
        bool pending=false,entered=false,new_path=false,inserted=false,existing=false;
        unsigned queue=0;
        uint64_t sp=0,slot=0,path=0,return_pc=0,alias=0,header=0,id=0;
    } call_{};
    bool owner() const noexcept;
    bool held() const noexcept;
    bool refuse() noexcept;
    bool read(uint64_t,void*,size_t) noexcept;
    bool compositor(uint64_t,const arena::Saved&) noexcept;
    bool client(uint64_t,const arena::Saved&) noexcept;
    CameraAccessGate& gate_;
    Read reader_;void* reader_context_;
    const int process_;
    const std::thread::id owner_;
    std::atomic<bool> failed_{false};
    bool active_=false;
    uint64_t ticket_=0,compositor_base_=0,client_base_=0,provider_=0;
    bool bound_=false;
    CameraBinding binding_{};
    Observation observation_{};
};
}
