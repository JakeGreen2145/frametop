// SPDX-License-Identifier: MIT
// Baseline regression probe: native Begin entry precedes its Vk handle load.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main original_evidence_main
#include "evidence_test.cpp"
#undef main
#pragma GCC diagnostic pop
static uint64_t known_command,known_device;
static bool native_available(void *,uint64_t pc,uint64_t address,uint64_t &out) noexcept {
    if(pc==0x301808 && address==known_command+8) {out=known_device;return true;}
    return false;
}
int main() {
    Fixture f;known_command=addr(f.executable);known_device=addr(f.r)+0x50;
    assert(f.evidence.begin_dormant_stock({nullptr,native_available}));
    assert(f.evidence.pin(Fixture::base,elf_sha256));assert(f.evidence.event(0x7fda0,f.s));
    f.s.x[0]=f.s.x[20]=known_command;f.s.sp=addr(f.stack)+0x100;
    assert(f.evidence.event(0x301808,f.s));
    f.s.sp-=0x40;f.s.x[19]=known_command;f.s.x[30]=Fixture::base+0x300ae0;
    assert(f.evidence.event(0x300a08,f.s) && "Begin entry must not demand a future native load");
}
