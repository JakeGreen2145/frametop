// SPDX-License-Identifier: MIT
// CPU fixture boundary only. The test drives the production observer with
// registers from pinned native instructions and supplies emulator memory reads.
#include "source_provider.hpp"
#include <memory>
using namespace native_portal;
using namespace native_portal::live;
struct Fixture {
    CameraAccessGate gate;
    CameraBinding binding{1,2,0x90000000,4,5,6,7,8,9};
    uint64_t epoch=0,ticket=0;
    ClientSourceBinding observer;
    Fixture(ClientSourceBinding::Read read,void* context):observer(gate,read,context) {
        if(!gate.publish(binding,std::this_thread::get_id(),epoch) || !gate.retain(epoch,ticket))std::abort();
    }
    ~Fixture(){if(ticket)gate.release(ticket);}
};
extern "C" {
void* source_fixture(ClientSourceBinding::Read read,void* context) {return new Fixture(read,context);}
void source_delete(void* p){delete static_cast<Fixture*>(p);}
int source_begin(void* p,int action) {
    auto& f=*static_cast<Fixture*>(p);
    return f.observer.begin(f.ticket,f.binding,action==1?coordination::Action::Connect:coordination::Action::Disconnect,
        0,0x10000000,ClientSourceBinding::compositor_sha,ClientSourceBinding::client_sha);
}
int source_event(void* p,int module,uint64_t pc,const arena::Saved* s) {
    auto& f=*static_cast<Fixture*>(p);
    return f.observer.event(module==0?ClientSourceBinding::Module::Compositor:ClientSourceBinding::Module::Client,pc,*s);
}
int source_finish(void* p,uint64_t result) {return static_cast<Fixture*>(p)->observer.finish(result);}
uint64_t source_value(void* p,unsigned q,unsigned field) {
    const auto s=static_cast<Fixture*>(p)->observer.snapshot();
    if(q>=2)return 0;
    const auto& a=s.queues[q];
    switch(field) {
    case 0:return a.observed;case 1:return uint32_t(a.result);case 2:return a.id;
    case 3:return unsigned(a.ownership);case 4:return a.header;case 5:return a.alias;
    case 6:return s.valid;case 7:return s.actual;default:return 0;
    }
}
void source_drop_retention(void* p) {auto& f=*static_cast<Fixture*>(p);if(f.ticket)f.gate.release(f.ticket);f.ticket=0;}
int source_foreign(void* p) {
    bool result=true;std::thread t([&]{arena::Saved s{};result=static_cast<Fixture*>(p)->observer.event(ClientSourceBinding::Module::Compositor,0x15fa24,s);});t.join();return result;
}
}
