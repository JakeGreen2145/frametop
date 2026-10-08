// SPDX-License-Identifier: MIT
#include "bridge.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
using namespace native_portal::arena;
struct Fixture {
 std::vector<unsigned char> mem=std::vector<unsigned char>(0x30000);
 static bool read(void* p,uint64_t a,void* v,size_t n) noexcept {auto& m=static_cast<Fixture*>(p)->mem; if(a>m.size() || n>m.size()-a)return false;memcpy(v,m.data()+a,n);return true;}
 static bool write(void* p,uint64_t a,const void* v,size_t n) noexcept {auto& m=static_cast<Fixture*>(p)->mem;if(a>m.size() || n>m.size()-a)return false;memcpy(m.data()+a,v,n);return true;}
 static bool coherent(void*,uint64_t r) noexcept {return r==0x8000;}
 template<class T> void put(uint64_t a,T v){assert(write(this,a,&v,sizeof v));}
 Bridge bridge{Memory{this,read,write,coherent}};
 static constexpr uint64_t R=0x1000,A=0x8000,K=0xa000,W=0xc000;
 void setup(){
  assert(bridge.arm({R,1,2,0x9000,0},true));
  Saved s{};s.x[19]=R;s.x[0]=0x9000;s.x[1]=StockBudget;s.x[8]=0x20000;
  bridge.dispatch(s,Allocate,1,2);assert(s.x[1]==Capacity);
  put<uint64_t>(0x20000,A);bridge.dispatch(s,AllocationReturn,1,2);
  put<uint64_t>(R+0x1390,A);put<uint64_t>(R+0x1410,0x10000);
  put<uint64_t>(R+0x328,64);put<uint32_t>(R+0x1418,0);
  put<uint64_t>(A+0xb0,0xfeed);put<uint32_t>(A+0xb8,Capacity);
  s={};s.x[24]=R;s.x[0]=1;bridge.dispatch(s,Initialized,1,2);assert(bridge.ready());
  put<uint64_t>(R+0x1380,0x22000);put<uint64_t>(R+0x15a0,0x23000);put<uint64_t>(R+0x15a8,0x23098);
  put<uint64_t>(W+8,0xd000);put<uint32_t>(0xd000+0xb8,592);
  put<uint64_t>(K+8,R);put<uint64_t>(K+0x2d0,0xe000);
  put<uint64_t>(0xe000+8,0xf000);put<uint32_t>(0xf000+0xb8,528);
 }
};
int main(){
 Fixture f;f.setup(); Owned o{Fixture::K,0xe000,Fixture::W,0x18000,1,{}};
 o.portal.fill(0xa5);assert(f.bridge.offer(o));
 for(size_t i=0;i<528;i++) f.mem[Fixture::K+0xc0+i]=static_cast<unsigned char>(i);
 Saved s{};s.x[27]=Fixture::K;s.x[0]=Fixture::R;s.x[1]=0xe000;s.x[2]=Fixture::K+0xc0;s.x[7]=0x2993f8;
 auto before=s;f.bridge.dispatch(s,Upload,1,3);assert(s.x[1]==before.x[1] && s.pad==Unsupported);
 // Thread mismatch conservatively poisons this bound instance; other renderers remain stock.
 s=before;s.x[0]=0x1234;f.bridge.dispatch(s,Upload,1,2);assert(s.pad==Original);
 Fixture g;g.setup();assert(g.bridge.offer(o));
 s=before;g.bridge.dispatch(s,Upload,1,2);assert(s.pad==Replacement && s.x[1]==Fixture::W && s.x[2]==o.staging);
 assert(!memcmp(g.mem.data()+o.staging,g.mem.data()+Fixture::K+0xc0,528));
 assert(!memcmp(g.mem.data()+o.staging+528,o.portal.data(),64));
 s={};s.x[19]=Fixture::R;s.x[20]=592;s.x[21]=Fixture::W+0x18;s.x[25]=o.staging;s.x[23]=0x808;s.x[24]=8;
 g.bridge.dispatch(s,Admission,1,2);assert(s.pad==Replacement && g.bridge.policy().actual()==640);
 // Native write failed after admission: no return snapshot, no refund.
 assert(!g.bridge.offer(Owned{}));assert(g.bridge.policy().actual()==640);
 Descriptor d;assert(!g.bridge.descriptor(d));
 g.put<uint32_t>(Fixture::R+0x1418,640);g.put<uint32_t>(Fixture::W+0x18,0);
 s=before;g.bridge.dispatch(s,UploadReturn,1,2);assert(g.bridge.descriptor(d));assert(d.offset==0 && d.range==592 && d.ticket==1);
 Owned wrong=o;wrong.ticket=2;wrong.wrapper=0x24000;assert(!g.bridge.offer(wrong));
 for(unsigned site=0;site<SiteCount;site++) {
  Saved foreign{};for(auto& x:foreign.x)x=0x27000;
  auto expected=foreign;auto memory=g.mem;
  g.bridge.dispatch(foreign,site,77,88);
  assert(!memcmp(&foreign,&expected,sizeof foreign));assert(memory==g.mem);
 }
 s=before;g.bridge.dispatch(s,Upload,1,2);assert(s.pad==SkipOwned); // never stock fallback
 assert(g.bridge.policy().actual()==640);
 s={};s.x[22]=Fixture::R;g.bridge.dispatch(s,Reset,1,2);assert(g.bridge.policy().actual()==0);
 std::cout << "bridge: all assertions passed\n";
}
