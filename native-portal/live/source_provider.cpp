// SPDX-License-Identifier: MIT
#include "source_provider.hpp"
#include <cstring>
#include <limits>
#include <sys/uio.h>
#include <unistd.h>

namespace native_portal::live {
namespace {
bool self_read(void*,uint64_t address,void* out,size_t bytes) {
    if(!address || !out || !bytes || address>UINT64_MAX-bytes)return false;
    iovec local{out,bytes},remote{reinterpret_cast<void*>(address),bytes};
    return process_vm_readv(getpid(),&local,1,&remote,1,0)==static_cast<ssize_t>(bytes);
}
}
ClientSourceBinding::ClientSourceBinding(CameraAccessGate& gate,Read reader,void* context) noexcept:
    gate_(gate),reader_(reader?reader:self_read),reader_context_(context),
    process_(getpid()),owner_(std::this_thread::get_id()) {}
bool ClientSourceBinding::owner() const noexcept {
    return getpid()==process_ && std::this_thread::get_id()==owner_;
}
bool ClientSourceBinding::held() const noexcept {
    CameraBinding b;
    return owner() && !failed_.load() && ticket_ && gate_.lookup(ticket_,b) && b==binding_;
}
bool ClientSourceBinding::refuse() noexcept {failed_.store(true);return false;}
bool ClientSourceBinding::read(uint64_t address,void* out,size_t size) noexcept {
    return held() && address && address<=UINT64_MAX-size &&
        reader_(reader_context_,address,out,size) && held();
}
bool ClientSourceBinding::begin(uint64_t ticket,const CameraBinding& b,coordination::Action action,
    uint64_t comp,uint64_t client,const char* comp_digest,const char* client_digest,bool native_inputs) noexcept {
    if(!owner())return refuse();
    if(active_ || failed_.load() || b.camera>UINT64_MAX-0x500 || !comp_digest || !client_digest ||
       std::strcmp(comp_digest,compositor_sha) || std::strcmp(client_digest,client_sha) ||
       !client || comp>UINT64_MAX-0x700000 || client>UINT64_MAX-0x700000 ||
       (action!=coordination::Action::None && action!=coordination::Action::Connect && action!=coordination::Action::Disconnect))return refuse();
    if(bound_ && (!(b==binding_) || comp!=compositor_base_ || client!=client_base_))return refuse();
    ticket_=ticket;binding_=b;
    if(!held())return refuse();
    bound_=true;compositor_base_=comp;client_base_=client;observation_={};call_={};metadata_={};
    native_inputs_=native_inputs;observation_.action=action;active_=true;return true;
}
bool ClientSourceBinding::event(Module module,uint64_t pc,const arena::Saved& s,const NativeSelectedId* selected) noexcept {
    if(!owner())return refuse();
    if(!active_ || !held() || selected_)return refuse();
    selected_=selected;
    const bool ok=observation_.action==coordination::Action::None?metadata_event(module,pc,s):
        module==Module::Compositor?compositor(pc,s):client(pc,s);
    selected_=nullptr;return ok;
}
bool ClientSourceBinding::native_connect_output(const arena::Saved& s) noexcept {
    if(!owner() || !held() || !active_ || !native_inputs_ || observation_.action!=coordination::Action::Connect ||
       !call_.pending || !call_.entered || call_.output || call_.sp<0xc0 || s.sp!=call_.sp-0xc0 ||
       s.x[20]!=provider_ || s.x[23]!=call_.slot || !s.x[19] || s.x[19]!=call_.alias ||
       !(call_.existing || (call_.new_path && call_.inserted && call_.header)))return refuse();
    call_.id=s.x[19];call_.output=true;return true;
}
ClientSourceBinding::NativeReadContext ClientSourceBinding::native_read_context() const noexcept {
    if(!owner() || !active_ || !held())return {};
    return {call_.pending,call_.entered,call_.new_path,call_.sp,call_.slot,call_.alias,
        metadata_.pending,metadata_.releasing,metadata_.entered,metadata_.inner_entered,metadata_.queue,
        metadata_.sp,metadata_.return_pc,metadata_.id,provider_,metadata_.object,metadata_.header,metadata_.ref_address};
}
bool ClientSourceBinding::compositor(uint64_t pc,const arena::Saved& s) noexcept {
    unsigned q=0;uint64_t ret=0;bool entry=false;
    switch(pc) {
    case 0x15fa24:ret=0x15fa28;entry=true;break;
    case 0x15fa44:q=1;ret=0x15fa48;entry=true;break;
    case 0x15fae8:q=1;ret=0x15faec;entry=true;break;
    case 0x15fbec:ret=0x15fbf0;entry=true;break;
    case 0x15fc34:q=1;ret=0x15fc38;entry=true;break;
    case 0x15fa28:case 0x15fa48:case 0x15faec:case 0x15fbf0:case 0x15fc38:break;
    default:return refuse();
    }
    const bool connecting=observation_.action==coordination::Action::Connect;
    if(connecting!=(pc<0x15fb00) || s.x[19]!=binding_.camera || !s.sp)return refuse();
    if(entry) {
        if(call_.pending || observation_.queues[q].observed ||
           (q && !observation_.queues[0].observed))return refuse();
        const uint64_t slot=binding_.camera+0x350+q*8;
        const uint64_t path=compositor_base_+(q?0x450358:0x4502e8);
        const uint64_t target=client_base_+(connecting?0x32a270:0x328380);
        if(binding_.camera>UINT64_MAX-0x358 || s.x[connecting?3:2]!=target ||
           (connecting && (s.x[1]!=slot || s.x[2]!=path)) ||
           (provider_ && s.x[0]!=provider_))return refuse();
        uint64_t vptr=0,method=0;
        if(!read(s.x[0],&vptr,8) || vptr!=client_base_+0x60f108 ||
           !read(vptr+(connecting?8:16),&method,8) || method!=target)return refuse();
        uint64_t id=0;
        if(!connecting) {
            if(native_inputs_) {
                if(!selected_ || selected_->queue!=q || selected_->value!=s.x[1])return refuse();
                id=selected_->value;if(!id)return refuse();
            } else if(!read(slot,&id,8) || !id || id!=s.x[1])return refuse();
        }
        provider_=s.x[0];observation_.provider=provider_;call_={};call_.pending=true;call_.queue=q;
        call_.sp=s.sp;call_.slot=slot;call_.path=path;call_.return_pc=ret;call_.id=id;
        return true;
    }
    if(!call_.pending || pc!=call_.return_pc || s.sp!=call_.sp)return refuse();
    auto& out=observation_.queues[call_.queue];
    out.observed=true;out.result=static_cast<int32_t>(s.x[0]);out.id=call_.id;
    if(connecting && out.result==0) {
        if(native_inputs_) {if(!call_.output || !out.id)return refuse();}
        else if(!read(call_.slot,&out.id,8) || !out.id)return refuse();
        out.alias=call_.alias;out.header=call_.header;
        if(out.alias && out.alias!=out.id)return refuse();
        if(call_.inserted && call_.header && call_.alias==out.id)out.ownership=Entry::NewEntry;
        else if(call_.existing && call_.alias==out.id)out.ownership=Entry::ExistingEntry;
    }
    call_={};return true;
}
bool ClientSourceBinding::client(uint64_t pc,const arena::Saved& s) noexcept {
    if(!call_.pending || observation_.action!=coordination::Action::Connect)return refuse();
    if(pc==0x32a270) {
        if(call_.entered || s.sp!=call_.sp || s.x[0]!=observation_.provider ||
           s.x[1]!=call_.slot || s.x[2]!=call_.path ||
           s.x[30]!=compositor_base_+call_.return_pc)return refuse();
        call_.entered=true;return true;
    }
    if(!call_.entered || call_.sp<0xc0 || s.sp!=call_.sp-0xc0 ||
       s.x[20]!=observation_.provider || s.x[23]!=call_.slot)return refuse();
    if(pc==0x32a398) {
        // This is BEFORE b.ls; only exact equality proves the dedup branch.
        if(s.x[0]==s.x[19] && s.x[19]) {
            if(call_.new_path)return refuse();
            call_.existing=true;call_.alias=s.x[19];
        }
        return true;
    }
    if(pc==0x32a39c) {
        if(call_.new_path || call_.existing || !s.x[19])return refuse();
        call_.new_path=true;call_.alias=s.x[19];return true;
    }
    if(pc==0x32a49c) {
        if(!call_.new_path || call_.header || s.x[19]!=call_.alias)return refuse();
        uint64_t id=0;
        // Actual native header accessor return; do not borrow server queue offsets
        // or scan arbitrary readable mappings. Only the native's next u64 load.
        if(!read(s.x[0],&id,8) || id!=call_.alias)return refuse();
        call_.header=s.x[0];return true;
    }
    if(pc==0x32a504) {
        if(!call_.new_path || call_.inserted || s.x[19]!=call_.alias)return refuse();
        // A missing header observation leaves ownership Unknown, not guessed.
        call_.inserted=true;return true;
    }
    return refuse();
}
bool ClientSourceBinding::finish(uint64_t actual) noexcept {
    if(!owner())return refuse();
    bool valid=active_ && held() && !call_.pending && !metadata_.pending;
    for(const auto& r:observation_.reads)if(r.acquire_observed && r.acquire_result==0 && !r.returned)valid=false;
    // The verified q1 failure path always continues through the actual q0 call.
    // Missing peer observations cannot certify a completed empty Update.
    if(observation_.reads[1].disposition==AcquireDisposition::RecognizedNoGrant &&
       !observation_.reads[0].acquire_observed)valid=false;
    if(!valid)refuse();
    observation_.actual=actual;observation_.valid=valid;active_=false;ticket_=0;return valid;
}
#include "source_metadata.inc"
ClientSourceBinding::Observation ClientSourceBinding::snapshot() const noexcept {
    if(!owner())return {};
    auto out=observation_;out.valid=out.valid && !failed_.load();return out;
}
}
