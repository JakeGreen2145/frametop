// SPDX-License-Identifier: MIT
#include "policy.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
using namespace native_portal::arena;
int main() {
    Policy p;
    assert(!p.initialize(0x80000, 64, true)); // enlarged allocation is mandatory
    assert(!p.initialize(Capacity, 32, true));
    assert(!p.initialize(Capacity, 64, false));
    assert(p.initialize(Capacity, 64, true));
    assert(p.append(0,592,true)==Result::Admit);
    assert(p.append(640,592,true)==Result::Admit);
    assert(p.append(1280,592,true)==Result::OwnedStop);
    assert(p.actual()==1280 && p.stock()==1152 && p.extra()==128);
    assert(p.append(1280,StockBudget-1152,false)==Result::Admit);
    assert(p.actual()==Capacity && p.stock()==StockBudget);
    assert(p.append(Capacity,64,false)==Result::Unsupported);
    assert(p.actual()==Capacity); // failures never refund partial/cancelled writes
    assert(p.classify(UINT32_MAX,UINT64_MAX)==Offset::Own);
    assert(p.classify(0x80000,64)==Offset::Arena);
    assert(p.classify(Capacity,1)==Offset::Invalid);
    assert(p.classify(UINT32_MAX-1,64)==Offset::Invalid);
    assert(p.classify(0x80000,UINT64_MAX)==Offset::Invalid);
    assert(!p.reset(false));
    assert(p.actual()==Capacity);
    assert(p.reset(true));
    assert(p.actual()==0 && p.extra()==0);
    assert(p.append(0,UINT64_MAX,false)==Result::Unsupported);
    assert(p.append(UINT64_MAX,64,false)==Result::Unsupported);
    assert(p.append(1,64,false)==Result::Unsupported);
    assert(p.append(0,0,false)==Result::Admit); // preserve zero-byte native stock append
    assert(p.append(0,1,false)==Result::Admit);
    assert(p.actual()==64 && p.stock()==64);
    assert(p.append(64,528,false)==Result::Admit);
    assert(p.actual()==640);
    // Each baseline-valid stock stream retains its complete budget.
    for (unsigned lead=0;lead<1024;lead+=64) {
        Policy q; assert(q.initialize(Capacity,64,true));
        if(lead) assert(q.append(0,lead,false)==Result::Admit);
        assert(q.append(q.actual(),592,true)==Result::Admit);
        assert(q.append(q.actual(),592,true)==Result::Admit);
        assert(q.append(q.actual(),StockBudget-q.stock(),false)==Result::Admit);
        assert(q.actual()==Capacity);
    }
    std::cout << "policy: all assertions passed\n";
}
