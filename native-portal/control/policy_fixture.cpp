#include "policy.hpp"
#include <cstdio>
#include <initializer_list>
using namespace native_portal;
int main() {
    for(unsigned b=0;b<256;++b)for(unsigned style=0;style<5;++style)for(bool owned:{false,true}) {
        PolicyInput p{bool(b&1),bool(b&2),bool(b&4),bool(b&8),bool(b&16),bool(b&32),bool(b&64),bool(b&128),style,.37f,.63f};
        auto r=pure_policy(p,owned);
        printf("%u %u %u %.9g\n",unsigned(r.demand),unsigned(r.construct),r.style,double(r.alpha));
    }
}
