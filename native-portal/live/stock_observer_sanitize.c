// SPDX-License-Identifier: MIT
// CPU-only sanitizer executable. No constructor, Runtime or native module load.
#define main ft_loader_original_test_main
#include "loader_test.c"
#undef main
int main(int argc,char **argv) {
    assert(argc==3);
    assert(ft_loader_original_test_main(2,argv)==0);
    assert(ft_loader_test_dual(argv[2])==1);
    return 0;
}
