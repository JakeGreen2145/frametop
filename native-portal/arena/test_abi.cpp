// SPDX-License-Identifier: MIT
// Test-only C ABI adapter. Executes the real production Bridge in Unicorn tests.
#include "bridge.hpp"
using namespace native_portal::arena;
extern "C" {
Bridge* arena_new(Memory* m) {return new Bridge(*m);}
void arena_delete(Bridge* b) {delete b;}
bool arena_arm(Bridge* b,Scope* s,bool pin) {return b->arm(*s,pin);}
bool arena_offer(Bridge* b,Owned* o) {return b->offer(*o);}
void arena_dispatch(Bridge* b,Saved* s,unsigned site,uint64_t life,uint64_t thread) {b->dispatch(*s,site,life,thread);}
bool arena_descriptor(Bridge* b,Descriptor* d) {return b->descriptor(*d);}
uint64_t arena_actual(Bridge* b) {return b->policy().actual();}
bool arena_ready(Bridge* b) {return b->ready();}
}
