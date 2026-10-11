// SPDX-License-Identifier: MIT
#pragma once
#include "source_admission.hpp"
#include <cstring>
#include <cmath>
#include <memory>
#include <optional>
#include <tuple>
#include <unistd.h>
namespace native_portal::live {
struct SourceIdentity {
    uint64_t runtime=0,version=0,client_module=0;
    bool operator==(const SourceIdentity& o) const noexcept {
        return runtime==o.runtime && version==o.version && client_module==o.client_module;
    }
};
using AuthenticateIdentity=bool(*)(void*,const CameraBinding&,SourceIdentity&) noexcept;
// Parent authenticates retained binding/module/owner and infrastructure, without
// mutable camera reads. Collector coverage is a separate required callback.
struct CurrentSourceContracts {AuthenticateIdentity authenticate=nullptr;};
struct BootstrapEligibility {bool available=false,needs_connect=false;};
enum class CurrentRenderPath {Unknown,ConnectedGate,RetainedTimeContinuation};
class NativeSourceAdmission;
class CurrentRenderCollector;
class CurrentRenderSelection final {
public:
    CurrentRenderSelection(const CurrentRenderSelection&)=default;
    CurrentRenderSelection& operator=(const CurrentRenderSelection&)=default;
private:
    friend class CurrentRenderCollector;
    friend class NativeSourceAdmission;
    struct Window {
        const int process=getpid();const std::thread::id owner=std::this_thread::get_id();
        bool active=false,consumed=false;uint64_t serial=0;
    };
    CurrentRenderSelection()=default;
    bool consume(const CameraBinding& b,const SourceIdentity& identity,const render::NativeBorrow& n) const noexcept {
        auto window=window_.lock();
        if(!window || window->process!=getpid() || window->owner!=std::this_thread::get_id() ||
           !window->active || window->consumed || window->serial!=serial_)return false;
        window->consumed=true; // even a refused packet cannot replay this window
        return b==binding_ && identity==identity_ && same_borrow(borrow_,n);
    }
    static bool same_borrow(const render::NativeBorrow& a,const render::NativeBorrow& b) noexcept {
        const auto& p=a.provenance;const auto& q=b.provenance;
        if(std::tie(a.watermark,a.command,a.invocation_sp,a.camera,a.resource_index,a.slot,a.selected_texture)!=
           std::tie(b.watermark,b.command,b.invocation_sp,b.camera,b.resource_index,b.slot,b.selected_texture) ||
           a.original!=b.original || a.replacement!=b.replacement ||
           std::tie(p.identity.process_instance,p.identity.renderer,p.identity.renderer_generation,p.identity.device,p.identity.device_generation,
                    p.ticket,p.frame,p.lease,p.origin,p.geometry,p.deadline,p.revocation,p.recording_generation,p.eye,p.wrapper,p.marker,p.vs,p.ps)!=
           std::tie(q.identity.process_instance,q.identity.renderer,q.identity.renderer_generation,q.identity.device,q.identity.device_generation,
                    q.ticket,q.frame,q.lease,q.origin,q.geometry,q.deadline,q.revocation,q.recording_generation,q.eye,q.wrapper,q.marker,q.vs,q.ps))return false;
        for(size_t i=0;i<p.descriptors.size();++i)
            if(std::tie(p.descriptors[i].buffer,p.descriptors[i].offset,p.descriptors[i].range)!=
               std::tie(q.descriptors[i].buffer,q.descriptors[i].offset,q.descriptors[i].range))return false;
        return true;
    }
    std::weak_ptr<Window> window_;
    uint64_t serial_=0,ticket_=0,invocation_=0,entry_sp_=0,caller_lr_=0;
    CurrentRenderPath path_=CurrentRenderPath::Unknown;float native_opacity_=0;
    CameraBinding binding_{};SourceIdentity identity_{};render::NativeBorrow borrow_{};
};
// Observes the one native caller. Has NO native reader or native call callback.
// Caller keeps the original gate ticket until finish/abort. Every Saved is copied
// into scalar facts at its actual pre-instruction observer, never retained.
class CurrentRenderCollector final {
public:
    using M=ClientSourceBinding::Module;
    using Covered=bool(*)(void*,M,uint64_t) noexcept;
    using Borrow=bool(*)(void*,const CurrentRenderSelection&,render::NativeBorrow) noexcept;
    inline static constexpr uint64_t required_sites[]={0x160be8,0x160ca8,0x161bc0,0x161be8,
        0x161c74,0x161c78,0x161d64,0x161d68,0x161e40,0x161ed8,0x160dd4,0x161f30,
        0x160ca0,0x160db0,0x160e34,0x1621c4,0x160e60};
    CurrentRenderCollector(CameraAccessGate& gate,void* context,AuthenticateIdentity authenticate,Covered covered):
        gate_(gate),context_(context),authenticate_(authenticate),covered_(covered),window_(std::make_shared<CurrentRenderSelection::Window>()) {}
    CurrentRenderCollector(const CurrentRenderCollector&)=delete;
    bool begin(uint64_t ticket,const CameraBinding& b,uint64_t invocation,uint64_t compositor,const arena::Saved& s) noexcept {
        if(!owner() || failed_ || active_ || !ticket || !invocation || invocation<=last_invocation_ ||
           s.x[0]!=b.camera || s.sp<0x460 || !s.x[30] || !covered_ || !authenticate_)return reject();
        for(const auto pc:required_sites)if(!covered_(context_,M::Compositor,pc))return reject();
        SourceIdentity identity;
        if(!authenticate_(context_,b,identity) || !identity.runtime || !identity.version || !identity.client_module)return reject();
        binding_=b;identity_=identity;ticket_=ticket;invocation_=last_invocation_=invocation;
        entry_sp_=s.sp;caller_lr_=s.x[30];compositor_=compositor;stage_=0;upload_=texture_=0;
        path_=CurrentRenderPath::Unknown;ramp_=opacity_=0;
        active_=true;return held()?true:reject();
    }
    bool capture(uint64_t pc,const arena::Saved& s) noexcept {
        if(!owner())return false;
        // Native epilogues have restored K and the entry SP/LR. They revoke the
        // scope only; unlike Connect they publish no own-completion receipt.
        if(pc==0x160dd4 || pc==0x161f30)return finish(invocation_,s);
        if(!owner() || !active_ || failed_ || window_->active || !held() ||
           s.sp!=entry_sp_-0x460 || s.x[27]!=binding_.camera)return reject();
        switch(pc) {
        case 0x160ca0:
            if(stage_!=0)return reject();
            stage_=uint32_t(s.x[7])?12:11;break;
        case 0x160ca8:
            if(stage_!=12)return reject();
            path_=uint32_t(s.x[0])?CurrentRenderPath::ConnectedGate:CurrentRenderPath::Unknown;
            stage_=11;break;
        case 0x160db0: {
            if(stage_!=11)return reject();
            double time=0;std::memcpy(&time,&s.q[0],8);
            // Native zero/nonfinite paths forward without granting optional use.
            if(!std::isfinite(time) || time==0 || (s.nzcv&0x40000000)) {stage_=15;break;}
            stage_=13;break;
        }
        case 0x160e34:
            if(stage_!=13)return reject();
            std::memcpy(&ramp_,&s.q[2],4); // Saved v2 low32 after the native store
            stage_=std::isfinite(ramp_) && ramp_>0 && !(s.nzcv&0x80000000)?14:15;break;
        case 0x1621c4: {
            float clamp=0;std::memcpy(&clamp,&s.q[0],4);
            if(stage_!=14 || ramp_<=1 || clamp!=1)return reject();
            ramp_=clamp;stage_=16;break;
        }
        case 0x160e60:
            if((stage_!=14 && stage_!=16) || !std::isfinite(ramp_) || ramp_<=0 || ramp_>1)return reject();
            if(path_==CurrentRenderPath::Unknown)path_=CurrentRenderPath::RetainedTimeContinuation;
            stage_=1;break;
        case 0x161bc0:
            if(stage_!=1)return reject();
            stage_=2;break;
        case 0x161be8:
            if(stage_!=2 || uint32_t(s.x[0])>=2)return reject();
            index_=uint32_t(s.x[0]);stage_=3;break;
        case 0x161c74:
            if(stage_!=3 || s.x[0]!=binding_.camera+4*index_)return reject();
            stage_=4;break; // pre-load x24 is NOT the slot
        case 0x161c78:
            if(stage_!=4 || s.x[0]!=binding_.camera+4*index_ || uint32_t(s.x[24])>=16)return reject();
            slot_=uint32_t(s.x[24]);stage_=5;break;
        case 0x161d64:
            if(stage_!=5 || s.x[0]!=binding_.renderer || !s.x[1] || s.x[2]!=binding_.camera+0xc0 ||
               s.x[7]!=compositor_+0x2993f8)return reject();
            std::memcpy(&opacity_,&s.q[8],4); // actual native s8 product, never recomputed
            if(path_==CurrentRenderPath::RetainedTimeContinuation && (!std::isfinite(opacity_) || opacity_<=0))return reject();
            stage_=6;break;
        case 0x161d68:
            if(stage_!=6 || !upload_)return reject();
            stage_=7;break;
        case 0x161e40:
            if(stage_!=7 || !s.x[0])return reject();
            texture_=s.x[0];stage_=8;break;
        case 0x161ed8:
            if(stage_!=8 || s.x[28]!=index_ || s.x[21]!=3*slot_ || s.x[24]!=s.x[21] ||
               s.x[0]!=binding_.renderer || s.x[1]!=s.sp+0x300 || s.x[22]!=s.x[1] ||
               s.x[2]!=compositor_+0x450650 || s.x[4]!=compositor_+0x29b278)return reject();
            stage_=9;break;
        default:return reject();
        }
        return true;
    }
    // Existing Adapter/Evidence reserve result, called after the pre-call capture
    // and before the original upload. This is NOT a second upload/consumer.
    bool upload_reserved(uint64_t ticket) noexcept {
        if(!owner() || !active_ || failed_ || !held() || stage_!=6 || upload_ || !ticket || ticket<=last_upload_)return reject();
        upload_=last_upload_=ticket;return true;
    }
    // Scalar transport for the existing staging/NativeBorrow fields. This is not
    // an admission certificate, native read token or deferred sample lifetime.
    struct Sample {
        uint32_t resource_index=0,slot=0;
        uint64_t selected_texture=0,upload_ticket=0,invocation_sp=0,camera=0;
    };
    std::optional<Sample> sample() const noexcept {
        if(!owner() || !active_ || failed_ || stage_!=9 || window_->active || !held())return {};
        return Sample{index_,slot_,texture_,upload_,entry_sp_-0x460,binding_.camera};
    }
    // Invoke from the existing NativeBorrow callback inside Adapter::enqueue.
    // The packet is already copied; this one-shot certificate dies on return.
    bool with_selection(render::NativeBorrow n,Borrow callback,void* context) noexcept {
        if(!owner() || !active_ || failed_ || !held())return reject();
        // held() calls external authentication, which may revoke this scope.
        // Revalidate after it returns before publishing any borrow authority.
        if(!active_ || failed_ || window_->active || stage_!=9 || !callback ||
           n.camera!=binding_.camera || n.invocation_sp!=entry_sp_-0x460 || n.resource_index!=index_ ||
           n.slot!=slot_ || n.selected_texture!=texture_ || n.provenance.ticket!=upload_)return reject();
        uint64_t texture=0;std::memcpy(&texture,n.original.data()+0x80,8);
        if(texture!=texture_)return reject();
        CurrentRenderSelection selection;selection.window_=window_;selection.serial_=++window_->serial;
        selection.ticket_=ticket_;selection.invocation_=invocation_;selection.entry_sp_=entry_sp_;selection.caller_lr_=caller_lr_;
        selection.binding_=binding_;selection.identity_=identity_;selection.borrow_=n;
        selection.path_=path_;selection.native_opacity_=opacity_;
        stage_=10;window_->active=true;window_->consumed=false;
        const bool result=callback(context,selection,n);
        const bool consumed=window_->consumed;window_->active=false;
        return result && consumed && held() && !failed_;
    }
    bool finish(uint64_t invocation,const arena::Saved& returned) noexcept {
        if(!owner() || !active_ || window_->active || invocation!=invocation_ || returned.sp!=entry_sp_ ||
           returned.x[30]!=caller_lr_ || !held())return reject();
        active_=false;stage_=0;return !failed_;
    }
    void abort() noexcept{reject();}
private:
    bool owner() const noexcept{return window_->process==getpid() && window_->owner==std::this_thread::get_id();}
    bool held() const noexcept {
        CameraBinding held;SourceIdentity identity;
        return owner() && gate_.lookup(ticket_,held) && held==binding_ && authenticate_ &&
            authenticate_(context_,binding_,identity) && identity==identity_;
    }
    bool reject() noexcept{if(owner()){failed_=true;window_->active=false;}return false;}
    CameraAccessGate& gate_;void* context_;AuthenticateIdentity authenticate_;Covered covered_;
    std::shared_ptr<CurrentRenderSelection::Window> window_;
    CameraBinding binding_{};SourceIdentity identity_{};
    uint64_t ticket_=0,invocation_=0,last_invocation_=0,entry_sp_=0,caller_lr_=0,compositor_=0,upload_=0,last_upload_=0,texture_=0;
    CurrentRenderPath path_=CurrentRenderPath::Unknown;float ramp_=0,opacity_=0;
    uint32_t index_=0,slot_=0;unsigned stage_=0;bool active_=false,failed_=false;
};
}
