// SPDX-License-Identifier: MIT
// Called only AFTER the real CPU dual-image installer succeeds. The client RX
// mapping and file identity are real; this token models the retained dlopen
// reference since an ARM DSO cannot be dlopened on the x86 host.
static void test_stock_coverage(uintptr_t comp,uintptr_t client,uintptr_t ci,int fd) {
    struct ft_stock_observer_coverage_v1 s,zero={0};
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));
    assert(!memcmp(&s,&zero,sizeof s));
    self_pid=getpid();image_base=comp;client_base=client;
    image_islands=comp+0x600000;client_islands=ci;
    client_handle=&s;client_fd=fd;assert(!fstat(fd,&client_stat));
    atomic_store(&enabled,1);
#ifndef FT_TEST_STOCK_MASK
#define FT_TEST_STOCK_MASK FT_STOCK_OBSERVER_REQUIRED
#endif
#ifdef FT_STOCK_OBSERVER_SELECTED_MASK
    assert(ft_native_portal_stock_observer_coverage(1,&s,sizeof s));
    assert(s.version==1 && s.size==sizeof s && s.installed_mask==FT_TEST_STOCK_MASK);
    assert(s.required_mask==FT_STOCK_OBSERVER_REQUIRED && s.compositor_base==comp && s.client_base==client);
    assert(ft_stock_observers_complete(&s)==(FT_TEST_STOCK_MASK==FT_STOCK_OBSERVER_REQUIRED));
#else
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));
    assert(!ft_stock_observers_complete(&s));
#endif
    assert(ft_native_portal_camera_coverage()==UINT64_C(0xfffff));
    memset(&s,0xa5,sizeof s);struct ft_stock_observer_coverage_v1 prior=s;
    assert(!ft_native_portal_stock_observer_coverage(2,&s,sizeof s));
    assert(!memcmp(&s,&prior,sizeof s));
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s-1));
    assert(!memcmp(&s,&prior,sizeof s));
    assert(!ft_native_portal_stock_observer_coverage(1,NULL,sizeof s));
    pid_t child=fork();assert(child>=0);
    if(!child) {
        assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));
        assert(!memcmp(&s,&zero,sizeof s));_exit(0);
    }
    int status;assert(waitpid(child,&status,0)==child && status==0);
    atomic_store(&enabled,0);
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));
    assert(!memcmp(&s,&zero,sizeof s));atomic_store(&enabled,1);
    client_handle=NULL;
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));client_handle=&s;
    client_fd=-1;
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));client_fd=fd;
    ++client_stat.st_ino;
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));--client_stat.st_ino;
    client_islands=0;
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));client_islands=ci;
    image_islands=0;
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));
    client_fd=-1;client_handle=NULL;client_islands=client_base=image_base=0;atomic_store(&enabled,0);
    printf("PASS stock observer installed mask=%#llx; legacy camera mask unchanged; version/fork/lifetime gates\n",
           (unsigned long long)FT_TEST_STOCK_MASK);
}
