// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <limits>
namespace native_portal::arena {
constexpr uint64_t StockBudget=524288, ExtraBudget=128, Capacity=524416;
enum class Result { Admit, OwnedStop, Unsupported };
enum class Offset { Arena, Own, Invalid };
class Policy {
    uint64_t b_=0,d_=0; unsigned count_=0; bool ready_=false;
public:
    bool initialize(uint64_t capacity,uint64_t alignment,bool coherent) noexcept {
        if(ready_ || capacity!=Capacity || alignment!=64 || !coherent) return false;
        ready_=true; return true;
    }
    uint64_t stock() const noexcept { return b_; }
    uint64_t extra() const noexcept { return d_; }
    uint64_t actual() const noexcept { return b_+d_; }
    bool ready() const noexcept { return ready_; }
    Result check(uint64_t cursor,uint64_t size,bool owned) const noexcept {
        if(!ready_ || cursor!=actual() || cursor%64 ||
           size>std::numeric_limits<uint64_t>::max()-cursor-63) return Result::Unsupported;
        if(owned && (size!=592 || count_>=2)) return Result::OwnedStop;
        uint64_t end=(cursor+size+63)&~uint64_t(63);
        uint64_t charge=owned?576:end-cursor;
        if(charge>StockBudget-b_ || end>Capacity) return owned?Result::OwnedStop:Result::Unsupported;
        return Result::Admit;
    }
    Result append(uint64_t cursor,uint64_t size,bool owned) noexcept {
        auto result=check(cursor,size,owned); if(result!=Result::Admit) return result;
        uint64_t end=(cursor+size+63)&~uint64_t(63);
        b_+=owned?576:end-cursor;
        if(owned) {d_+=64; ++count_;}
        return result;
    }
    Offset classify(uint64_t offset,uint64_t size) const noexcept {
        if(offset==UINT32_MAX) return Offset::Own;
        if(!ready_ || offset>=Capacity || size>Capacity-offset) return Offset::Invalid;
        return Offset::Arena;
    }
    // Only the exact native cursor-reset hook may supply this proof. Queue
    // submission, cancellation and packet disposal are deliberately not inputs.
    bool reset(bool native_reset) noexcept {
        if(!ready_ || !native_reset) return false;
        b_=d_=0; count_=0; return true;
    }
};
}
