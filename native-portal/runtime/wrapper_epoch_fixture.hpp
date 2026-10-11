// SPDX-License-Identifier: MIT
#pragma once
// Complete selected factory observer sequence from retained native contract.
// This helper models native execution and allocation; it is not a native replay.
template<class Emit>
static void wrapper_factory_model(uint64_t base,uint64_t R,uint64_t W,uint64_t C,
                                  uint64_t P,uint64_t B,Emit emit) {
    const uint64_t A=R+0x1300,name=base+0x478dc0;
    native_portal::arena::Saved s{};s.sp=B;s.x[19]=R;s.x[0]=s.x[20]=A;
    s.x[1]=name;s.x[8]=B+0x98;s.x[30]=base+0x2a23e8;emit(0x3053f8,s);
    s.sp-=0x30;s.x[0]=W;s.x[1]=A;s.x[2]=name;s.x[30]=base+0x30548c;emit(0x305308,s);
    s.sp-=0x20;s.x[0]=P;s.x[1]=name;s.x[3]=A;s.x[19]=W;s.x[8]=W+8;s.x[30]=base+0x305330;emit(0x302790,s);
    s.sp-=0x30;s.x[30]=base+0x3027b0;emit(0x304e60,s);
    s.sp-=0x60;s.x[19]=P;s.x[21]=W+8;s.x[22]=C;emit(0x304ec8,s);emit(0x304ecc,s);
    s.sp=B-0x30;s.x[30]=base+0x30548c;emit(0x3053a8,s);
    s.x[20]=B+0x98;s.x[22]=W;emit(0x305490,s);
    s.sp=B;s.x[19]=R;s.x[20]=A;s.x[0]=B+0x98;s.x[30]=base+0x2a23e8;emit(0x305470,s);
    s.x[20]=0;s.x[0]=W;emit(0x2a23f4,s);emit(0x2a23f8,s);
}
