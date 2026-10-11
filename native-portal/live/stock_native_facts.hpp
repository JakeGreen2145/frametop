// SPDX-License-Identifier: MIT
#pragma once
#include "source_provider.hpp"
#include <unistd.h>
namespace native_portal::live {
// Current Authenticate continues to supply this separately authorized state.
// Historical facts do not provide a current camera tuple or any native loan.
struct StockSourceState {
    uint64_t client=0;std::array<uint64_t,2> ids{};
    uint8_t active=0,connected=0,external=0;
};
struct StockSourceReadScope {
    const CameraBinding& binding;
    uint64_t ticket,version,sequence,method,pc,runtime;
    ClientSourceBinding::Module module;
    const arena::Saved& saved;
};
// Backend only, called AFTER the concrete native allowlist and gate checks.
// Null selects bounded process_vm_readv. No state-snapshot callback exists.
struct StockSourceReadBoundary {
    using Read=bool(*)(void*,const StockSourceReadScope&,uint64_t,void*,size_t) noexcept;
    using Covered=bool(*)(void*,ClientSourceBinding::Module,uint64_t) noexcept;
    using ProviderLive=bool(*)(void*,const StockSourceReadScope&,uint64_t) noexcept;
    Read read=nullptr;void* context=nullptr;
    Covered covered=nullptr;ProviderLive provider_live=nullptr;
};
enum class NativeCameraField:uint8_t { Client=1,Id0=2,Id1=4,Active=8,Connected=16,External=32 };
enum class NativeFactPath:uint8_t { ModuleIdentity,NativeLoad,CommittedStore,SelectedRequest };
struct NativeFieldFact {
    NativeCameraField field=NativeCameraField::Client;
    NativeFactPath path=NativeFactPath::ModuleIdentity;
    uint8_t width=0;
    uint64_t value=0,pc=0,method=0,sequence=0,version=0,runtime=0,ticket=0,sp=0,lr=0;
    ClientSourceBinding::Module module=ClientSourceBinding::Module::Compositor;
    int process=0;std::thread::id caller{};
    // Binding is immutable for this sequence. Retained in each fact so an
    // exported fact cannot be rebound to another same-address camera epoch.
    CameraBinding binding{};
};
struct NativeFieldFacts {
    static constexpr uint8_t CLIENT=1,ID0=2,ID1=4,ACTIVE=8,CONNECTED=16,EXTERNAL=32;
    StockSourceState values{};
    uint8_t known=0,committed=0;
    std::array<NativeFieldFact,6> last{};
    bool has(uint8_t mask) const noexcept {return (known&mask)==mask;}
    void put(const StockSourceReadScope& scope,NativeCameraField field,uint64_t value,uint8_t width,NativeFactPath path) noexcept {
        const auto bit=static_cast<uint8_t>(field);unsigned index=0;
        while((1u<<index)!=bit)++index;
        known|=bit;
        if(path==NativeFactPath::CommittedStore)committed|=bit;
        last[index]={field,path,width,value,scope.pc,scope.method,scope.sequence,scope.version,scope.runtime,
            scope.ticket,scope.saved.sp,scope.saved.x[30],scope.module,getpid(),std::this_thread::get_id(),scope.binding};
        switch(field) {
        case NativeCameraField::Client:values.client=value;break;
        case NativeCameraField::Id0:values.ids[0]=value;break;
        case NativeCameraField::Id1:values.ids[1]=value;break;
        case NativeCameraField::Active:values.active=uint8_t(value);break;
        case NativeCameraField::Connected:values.connected=uint8_t(value);break;
        case NativeCameraField::External:values.external=uint8_t(value);break;
        }
    }
};
}
