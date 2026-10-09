// SPDX-License-Identifier: MIT
#include "command_trace.hpp"
#include <cassert>
#include <cstring>
#include <thread>
#include <memory>
using namespace native_portal;
using native_portal::live::CommandTrace;
int main() {
    assert(CommandTrace::selected(0x15f9e0) && CommandTrace::selected(0x15fba8));
    auto storage=std::make_unique<CommandTrace>();auto &trace=*storage;
    assert(CommandTrace::capacity==8192);
    arena::Saved s{};s.x[0]=1;s.x[20]=1;s.x[30]=0x70001234;s.sp=2;
    const auto saved=s;
    trace.push(0x4b3d0,s,0x70000000,10,11,CommandTrace::Foreign);
    assert(trace.count()==0); // unrelated deletes never consume the command budget
    trace.push(0x301808,s,0x70000000,10,11,CommandTrace::HandoffWait);
    trace.push(0x4b3d0,s,0x70000000,10,11,CommandTrace::Foreign);
    assert(trace.count()==2 && !std::memcmp(&saved,&s,sizeof s));
    CommandTrace::Row first{},row{};assert(trace.read(0,first));
    assert(first.ledger.command==1 && !first.ledger_sampled && first.lr==0x1234);
    char text[1536];assert(!CommandTrace::format(0,first,text,1));
    assert(CommandTrace::format(0,first,text,sizeof text));
    assert(std::strstr(text,"route=handoff-wait") && std::strstr(text,"ledger_sampled=0"));
    // Real concurrent writers, immutable published slots and a concurrent reader.
    std::atomic<bool> done{false};
    std::thread reader([&] {
        size_t next=0;
        while(!done.load() || next<trace.count()) {
            if(next>=CommandTrace::capacity)break;
            if(trace.read(next,row)) {
                assert(row.pc==0x301808 || row.pc==0x4b3d0 || row.pc==0x300a38);
                ++next;
            } else std::this_thread::yield();
        }
    });
    std::array<std::thread,4> writers;
    for(unsigned t=0;t<writers.size();++t)writers[t]=std::thread([&,t] {
        auto input=s;input.x[19]=100+t;
        for(size_t i=0;i<CommandTrace::capacity/4+1;++i)trace.push(0x300a38,input,0x70000000,100+t,11,CommandTrace::Foreign);
    });
    for(auto &writer:writers)writer.join();
    done=true;reader.join();
    assert(trace.count()==2+4*(CommandTrace::capacity/4+1));
    for(size_t i=0;i<CommandTrace::capacity;++i)assert(trace.read(i,row));
    assert(!trace.read(CommandTrace::capacity,row));
    assert(trace.read(0,row) && !std::memcmp(&first,&row,sizeof row));
    auto identity_storage=std::make_unique<CommandTrace>();auto &identities=*identity_storage;
    for(size_t i=0;i<=CommandTrace::identity_capacity;++i) {
        s.x[0]=i+1;identities.push(0x301808,s,0,10,10,CommandTrace::Owner);
    }
    assert(identities.identity_overflow()==1);
    auto camera_storage=std::make_unique<CommandTrace>();auto &camera=*camera_storage;
    camera.push(0x15f9e0,s,0x70000000,10,11,CommandTrace::Foreign);
    camera.push(0x15fba8,s,0x70000000,11,11,CommandTrace::Owner,nullptr,true,1);
    assert(camera.read(0,row) && !row.result_valid && row.tid==10 && row.x0==s.x[0]);
    assert(camera.read(1,row) && row.result_valid && row.result==1 && row.tid==11);
    assert(CommandTrace::format(1,row,text,sizeof text) && std::strstr(text,"result_valid=1 result=0x1"));
    std::puts("command trace: immutable first records, concurrent writers/reader, exact capacity/overflow, identity filter and format bounds PASS");
}
