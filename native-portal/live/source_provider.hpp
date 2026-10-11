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
    enum class AcquireDisposition { Unknown, Granted, RecognizedNoGrant };
    struct ReadCall {
        AcquireDisposition disposition=AcquireDisposition::Unknown;
        bool acquire_observed=false,release_observed=false,returned=false,increment=false,decrement=false;
        bool metadata_return_observed=false;
        int32_t acquire_result=0,release_result=0;
        uint64_t metadata=0,data=0,queue=0,id=0,header=0,ref_address=0;
        uint64_t metadata_return=0; // raw register only; never producer/settlement authority
    };
    struct Observation {
        bool valid=false;
        uint64_t actual=0,provider=0;
        coordination::Action action=coordination::Action::None;
        std::array<QueueCall,2> queues{};
        std::array<ReadCall,2> reads{};
    };
    // read_self by default; injection is for CPU instruction fixtures only.
    using Read=bool (*)(void*,uint64_t,void*,size_t);
    explicit ClientSourceBinding(CameraAccessGate&,Read=nullptr,void* context=nullptr) noexcept;
    // Caller owns and keeps this original gate ticket for the complete original
    // native invocation. Pins are a trusted loader/preparer contract, as in
    // runtime::Evidence, NOT proof supplied by an arbitrary digest string.
    // begin belongs BEFORE original camera Acquire/Release/Update, not its return.
    // Action::None denotes Update observation only, never a demand transition.
    bool begin(uint64_t retained_ticket,const CameraBinding&,coordination::Action,
               uint64_t compositor_base,uint64_t client_base,
               const char* compositor_digest,const char* client_digest,bool native_inputs=false) noexcept;
    // Saved registers BEFORE the named instruction. Calls are observed only;
    // every original instruction/result remains untouched. Missing events never
    // become inferred ownership. Foreign-thread events revoke without reading.
    // Explicit native-register selection, not a memory-reader substitution.
    struct NativeSelectedId { unsigned queue=0;uint64_t value=0; };
    bool event(Module,uint64_t relative_pc,const arena::Saved&,const NativeSelectedId* =nullptr) noexcept;
    bool native_connect_output(const arena::Saved&) noexcept;
    struct NativeReadContext {
        bool connect_pending=false,connect_entered=false,new_path=false;
        uint64_t call_sp=0,slot=0,alias=0;
        bool pending=false,releasing=false,entered=false,inner_entered=false;
        unsigned queue=0;
        uint64_t sp=0,return_pc=0,id=0,provider=0,object=0,header=0,ref=0;
    };
    NativeReadContext native_read_context() const noexcept;
    bool finish(uint64_t actual_camera_return) noexcept;
    void cancel() noexcept {refuse();}
    bool failed() const noexcept {return !owner() || failed_.load();}
    Observation snapshot() const noexcept;
private:
    struct Call {
        bool pending=false,entered=false,new_path=false,inserted=false,existing=false,output=false;
        unsigned queue=0;
        uint64_t sp=0,slot=0,path=0,return_pc=0,alias=0,header=0,id=0;
    } call_{};
    struct MetadataCall {
        bool pending=false,releasing=false,entered=false,inner=false,inner_entered=false,inner_returned=false;
        bool property=false,property_returned=false,ref_before=false,ref_after=false;
        bool latest_observed=false,empty_path=false;
        unsigned queue=0;
        int32_t inner_result=0;
        uint64_t sp=0,return_pc=0,id=0,metadata_slot=0,data_slot=0,metadata=0;
        uint64_t object=0,header=0,ref_address=0,property_return=0;
        uint32_t ref_value=0;
    } metadata_{};
    bool metadata_event(Module,uint64_t,const arena::Saved&) noexcept;
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
    bool active_=false,native_inputs_=false;
    const NativeSelectedId* selected_=nullptr; // call-local input, never retained
    uint64_t ticket_=0,compositor_base_=0,client_base_=0,provider_=0;
    bool bound_=false;
    CameraBinding binding_{};
    Observation observation_{};
};
}
